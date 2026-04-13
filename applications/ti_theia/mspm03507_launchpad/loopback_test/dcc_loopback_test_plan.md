# DCC Loopback Test Plan — PC-Orchestrated Two-Board Validation

## Goal

Validate every DCC command station API by physically transmitting DCC packets from
one MSPM0G3507 LaunchPad (command station) to a second LaunchPad (decoder), with a
Python script on the PC orchestrating both boards via their UART connections and
performing pass/fail comparison.

## Hardware Setup

```
  PC (USB Hub)
  |           |
  USB         USB
  |           |
  LaunchPad A                             LaunchPad B
  (Command Station)                       (Decoder)
  UART0 @ 230400                          UART0 @ 230400
  (backchannel)                           (backchannel)
  PB12 (DCC out) ──── DCC wire ────────>  PB1 (DCC input, edge interrupt)
  PB17 (debug toggle) ── trigger wire ──> PA28 (trigger input)
  PB15 (ACK sense in) < ACK wire ───────  PA31 (ACK pulse out)
  GND ──────────────── ground wire ─────  GND
```

- **Board A**: Existing `command_station` project (sends DCC)
- **Board B**: New `decoder` project (receives and decodes DCC)
- **PC**: Python test script talks to both boards over serial
- 4 wires between boards: DCC signal + debug trigger + ACK pulse + common ground

## Existing Project Locations

```
applications/ti_theia/mspm03507_launchpad/
├── command_station/    # Board A — already implemented
├── decoder/            # Board B — to be implemented (empty placeholder)
└── loopback_test/      # Python test script location (empty placeholder)
```

---

## Board A — Command Station (changes for ACK sense)

UART command interface:

| Command | Syntax | Response |
|---------|--------|----------|
| POWER | `POWER ON\|OFF` | `OK: track power ON\|OFF` |
| SPEED | `SPEED <addr> <0-127> <FWD\|REV> [14\|28\|128]` | `OK: SPEED addr=N speed=N dir=FWD\|REV mode=N` |
| ESTOP | `ESTOP [addr]` | `OK: ESTOP addr=N` or `OK: ESTOP broadcast` |
| FUNC | `FUNC <addr> <0-68> <ON\|OFF>` | `OK: FUNC addr=N FN=ON\|OFF` |
| ACC | `ACC <board> <pair> <ON\|OFF>` | `OK: ACC board=N pair=N ON\|OFF` |
| ACCE | `ACCE <addr> <aspect>` | `OK: ACCE addr=N aspect=N` |
| CV | `CV WRITE\|VERIFY <addr> <cv> <value>` | `OK: CV command scheduled` |
| CV BIT | `CV BIT <addr> <cv> <bit_pos> <0\|1>` | `OK: CV command scheduled` |
| CONSIST | `CONSIST <addr> SET <ca> [NORMAL\|REVERSE]` | `OK: CONSIST addr=N SET` |
| CONSIST | `CONSIST <addr> CLEAR` | `OK: CONSIST addr=N CLEAR` |
| BSS | `BSS <addr> <1-127> <ON\|OFF>` | `OK: BSS addr=N state=N ON\|OFF` |
| BSL | `BSL <addr> <1-32767> <ON\|OFF>` | `OK: BSL addr=N state=N ON\|OFF` |
| ANALOG | `ANALOG <addr> <output> <value>` | `OK: ANALOG addr=N output=N value=N` |
| RESTRICT | `RESTRICT <addr> ON <limit>\|OFF` | `OK: RESTRICT addr=N ON\|OFF` |
| REFRESH | `REFRESH ON\|OFF` | `OK: auto-refresh ON\|OFF` |
| DEBUG | `DEBUG` | `OK: toggled PB17` |
| SVC ENTER | `SVC ENTER` | `OK: service mode entered` |
| SVC EXIT | `SVC EXIT` | `OK: service mode exited` |
| SVC DIRECT | `SVC DIRECT WRITE\|VERIFY <cv> <value>` | `OK: service mode operation started` |
| SVC DIRECT BIT | `SVC DIRECT BITW\|BITV <cv> <bit> <0\|1>` | `OK: service mode operation started` |
| SVC PAGED | `SVC PAGED WRITE\|VERIFY <cv> <value>` | `OK: service mode operation started` |
| SVC REG | `SVC REG WRITE\|VERIFY <reg> <value>` | `OK: service mode operation started` |
| SVC ADDR | `SVC ADDR WRITE <addr>` | `OK: service mode operation started` |
| STATUS | `STATUS` | `STATUS: svc_mode=... locos=.../...` |

Service mode completion is reported asynchronously via `SVC RESULT: SUCCESS` or `SVC RESULT: NO_ACK`.

Address parsing: `40` = short (<=127) or long (>=128), `40S` = force short, `40L` = force long.

PB17 toggles on every packet sent (via `on_packet_sent` callback).
PB15 reads ACK pulse from decoder (GPIO input for `current_sense_read`).

All one-shot packets have `repeat_count = 3`.

---

## Board B — Decoder Project

### Location

`applications/ti_theia/mspm03507_launchpad/decoder/`

### Compile Flags (dcc_user_config.h)

```c
#define DCC_COMPILE_DECODER
/* DCC_COMPILE_COMMAND_STATION — NOT defined */
```

### Hardware Configuration (SysConfig)

Copy SysConfig base from command_station, then modify:

| Peripheral | Pin(s) | Purpose | Notes |
|---|---|---|---|
| HFXT | PA5/PA6 | 40MHz crystal | Same as command_station |
| TIMA0 | (internal) | Free-running µs timestamp | Same config as command_station |
| UART0 | PA10 (TX), PA11 (RX) | PC communication @ 230400 | Same as command_station |
| GPIO input | PB1 | DCC signal input (edge interrupt) | New — rising+falling edge interrupt |
| GPIO input | PA28 | Trigger input (from Board A PB17) | New — optional, for sync verification |
| GPIO output | PB22 | Heartbeat LED | Same as command_station |
| GPIO output | PA31 | ACK pulse output | New — feeds CS PB15 for service mode ACK |
| ACK_PULSE_TIMER | TIMG12 | One-shot ACK pulse timer | 1 MHz (prescale=40), default 6ms, ISR clears pin |
| SYSTICK | (internal) | 100ms heartbeat tick | Same as command_station |

**Remove from command_station SysConfig**: DCC_BIT_TIMER (TIMA1), DCC signal output (PB12), debug output (PB17), LED2 (PB26), LED3 (PB27).

### Critical Design: ISR-Safe Callback Buffering

**All decoder callbacks fire from ISR context** (edge interrupt → bit decoder → packet decoder → callbacks). UART TX from ISR will crash (same issue as command station echo).

Solution: callbacks write to a ring buffer of formatted RECV strings; main loop drains the buffer over UART.

```c
#define RECV_RING_SIZE 16
#define RECV_LINE_MAX  80

static char _recv_ring[RECV_RING_SIZE][RECV_LINE_MAX];
static volatile uint16_t _recv_head = 0;
static volatile uint16_t _recv_tail = 0;

/* Called from ISR context — just snprintf into ring buffer */
static void _recv_enqueue(const char *fmt, ...) {
    uint16_t next = (_recv_head + 1) % RECV_RING_SIZE;
    if (next == _recv_tail) return;  /* overflow — drop */
    va_list args;
    va_start(args, fmt);
    vsnprintf(_recv_ring[_recv_head], RECV_LINE_MAX, fmt, args);
    va_end(args);
    _recv_head = next;
}

/* Called from main loop — drain to UART */
void DecoderCallbacks_drain(void) {
    while (_recv_tail != _recv_head) {
        TI_UartDriver_write_string(_recv_ring[_recv_tail]);
        TI_UartDriver_write_string("\r\n");
        _recv_tail = (_recv_tail + 1) % RECV_RING_SIZE;
    }
}
```

### DCC Library Wiring (dcc_config_t)

```c
static const dcc_config_t dcc_config = {
    /* Common: REQUIRED */
    .lock_shared_resources   = &TI_DccDriver_lock_shared_resources,
    .unlock_shared_resources = &TI_DccDriver_unlock_shared_resources,
    .get_timestamp_usec      = &TI_DccDriver_get_timestamp_usec,

    /* Decoder: REQUIRED */
    .cv_read                 = &DecoderCallbacks_cv_read,
    .cv_write                = &DecoderCallbacks_cv_write,

    /* Decoder: NULL-optional */
    .railcom_uart_write      = NULL,

    /* Decoder: OPTIONAL application callbacks */
    .on_speed_command              = &DecoderCallbacks_on_speed_command,
    .on_emergency_stop             = &DecoderCallbacks_on_emergency_stop,
    .on_function_command           = &DecoderCallbacks_on_function_command,
    .on_accessory_basic_command    = &DecoderCallbacks_on_accessory_basic_command,
    .on_accessory_extended_command = &DecoderCallbacks_on_accessory_extended_command,
    .on_cv_write                   = &DecoderCallbacks_on_cv_write,
    .on_cv_verify                  = &DecoderCallbacks_on_cv_verify,
    .on_cv_bit                     = &DecoderCallbacks_on_cv_bit,
    .on_consist_command            = &DecoderCallbacks_on_consist_command,
    .on_binary_state_short         = &DecoderCallbacks_on_binary_state_short,
    .on_binary_state_long          = &DecoderCallbacks_on_binary_state_long,
    .on_analog_function            = &DecoderCallbacks_on_analog_function,
    .on_speed_restriction          = &DecoderCallbacks_on_speed_restriction,
    .on_failsafe_entered           = &DecoderCallbacks_on_failsafe_entered,
    .on_failsafe_exited            = &DecoderCallbacks_on_failsafe_exited,
};
```

### CV Stub Implementation

**Critical**: The decoder has a CV lock mechanism (CV15 must equal CV16 for writes to succeed). The stub must return matching values.

```c
static uint8_t _cv_store[1024];

void DecoderCallbacks_initialize(void) {
    memset(_cv_store, 0, sizeof(_cv_store));
    _cv_store[0]  = 3;  /* CV1 = 3 (default short address) */
    _cv_store[14] = 0;  /* CV15 = 0 (lock key) */
    _cv_store[15] = 0;  /* CV16 = 0 (lock value) — matches CV15 = unlocked */
    _cv_store[28] = 6;  /* CV29 = 0x06 (28-step mode default) */
    _recv_head = 0;
    _recv_tail = 0;
}

bool DecoderCallbacks_cv_read(uint16_t cv_number, uint8_t *value) {
    if (cv_number < 1 || cv_number > 1024) return false;
    *value = _cv_store[cv_number - 1];
    return true;
}

bool DecoderCallbacks_cv_write(uint16_t cv_number, uint8_t value) {
    if (cv_number < 1 || cv_number > 1024) return false;
    _cv_store[cv_number - 1] = value;
    return true;
}
```

### Decoder Callback Output Format

Each callback enqueues a single RECV line:

```
RECV SPEED addr=3 speed=64 dir=FWD mode=128
RECV ESTOP addr=3
RECV FUNC addr=3 func=1 state=ON
RECV FUNC addr=3 func=0 state=ON
RECV ACC board=5 pair=2 activate=ON
RECV ACCE addr=100 aspect=5
RECV CV_WRITE cv=29 value=6
RECV CV_VERIFY cv=29 value=6
RECV CV_BIT cv=29 bit=3 value=1
RECV CONSIST addr=3 consist=10 dir=NORMAL
RECV BSS addr=3 state=1 active=ON
RECV BSL addr=3 state=1 active=ON
RECV ANALOG addr=3 output=1 value=128
RECV RESTRICT addr=3 enabled=ON limit=50
```

Callback implementations:

```c
void DecoderCallbacks_on_speed_command(uint16_t address, uint8_t speed,
                                       bool direction, dcc_speed_mode_enum mode) {
    _recv_enqueue("RECV SPEED addr=%u speed=%u dir=%s mode=%u",
                  address, speed, direction ? "FWD" : "REV",
                  mode == DCC_SPEED_MODE_128 ? 128 :
                  mode == DCC_SPEED_MODE_28 ? 28 : 14);
}

void DecoderCallbacks_on_emergency_stop(uint16_t address) {
    _recv_enqueue("RECV ESTOP addr=%u", address);
}

void DecoderCallbacks_on_function_command(uint16_t address, uint8_t function_number,
                                           bool state) {
    _recv_enqueue("RECV FUNC addr=%u func=%u state=%s",
                  address, function_number, state ? "ON" : "OFF");
}

void DecoderCallbacks_on_accessory_basic_command(uint16_t board_address,
                                                  uint8_t output_pair,
                                                  bool activate) {
    _recv_enqueue("RECV ACC board=%u pair=%u activate=%s",
                  board_address, output_pair, activate ? "ON" : "OFF");
}

void DecoderCallbacks_on_accessory_extended_command(uint16_t address, uint8_t aspect) {
    _recv_enqueue("RECV ACCE addr=%u aspect=%u", address, aspect);
}

void DecoderCallbacks_on_cv_write(uint16_t cv_number, uint8_t value) {
    _recv_enqueue("RECV CV_WRITE cv=%u value=%u", cv_number, value);
    AckPulseDriver_fire();  /* Write succeeded — always ACK */
}

void DecoderCallbacks_on_cv_verify(uint16_t cv_number, uint8_t value) {
    _recv_enqueue("RECV CV_VERIFY cv=%u value=%u", cv_number, value);
    /* ACK only if stored value matches per NMRA S-9.2.3 */
    uint8_t stored;
    if (DecoderCallbacks_cv_read(cv_number, &stored) && stored == value) {
        AckPulseDriver_fire();
    }
}

void DecoderCallbacks_on_cv_bit(uint16_t cv_number, uint8_t bit_position,
                                 bool bit_value) {
    _recv_enqueue("RECV CV_BIT cv=%u bit=%u value=%u",
                  cv_number, bit_position, bit_value ? 1 : 0);
    /* For bit-verify: ACK if stored bit matches.
     * For bit-write: on_cv_write already fired ACK; fire() is no-op if active. */
    uint8_t stored;
    if (DecoderCallbacks_cv_read(cv_number, &stored)) {
        bool stored_bit = (stored >> bit_position) & 0x01;
        if (stored_bit == bit_value) {
            AckPulseDriver_fire();  /* no-op if already active from on_cv_write */
        }
    }
}

void DecoderCallbacks_on_consist_command(uint16_t address, uint8_t consist_address,
                                          bool direction_normal) {
    _recv_enqueue("RECV CONSIST addr=%u consist=%u dir=%s",
                  address, consist_address,
                  direction_normal ? "NORMAL" : "REVERSE");
}

void DecoderCallbacks_on_binary_state_short(uint16_t address, uint8_t state_number,
                                             bool active) {
    _recv_enqueue("RECV BSS addr=%u state=%u active=%s",
                  address, state_number, active ? "ON" : "OFF");
}

void DecoderCallbacks_on_binary_state_long(uint16_t address, uint16_t state_number,
                                            bool active) {
    _recv_enqueue("RECV BSL addr=%u state=%u active=%s",
                  address, state_number, active ? "ON" : "OFF");
}

void DecoderCallbacks_on_analog_function(uint16_t address, uint8_t output_number,
                                          uint8_t value) {
    _recv_enqueue("RECV ANALOG addr=%u output=%u value=%u",
                  address, output_number, value);
}

void DecoderCallbacks_on_speed_restriction(uint16_t address, bool enabled,
                                            uint8_t speed_limit) {
    if (enabled)
        _recv_enqueue("RECV RESTRICT addr=%u enabled=ON limit=%u",
                      address, speed_limit);
    else
        _recv_enqueue("RECV RESTRICT addr=%u enabled=OFF", address);
}

void DecoderCallbacks_on_failsafe_entered(void) {
    _recv_enqueue("RECV FAILSAFE_ENTER");
}

void DecoderCallbacks_on_failsafe_exited(void) {
    _recv_enqueue("RECV FAILSAFE_EXIT");
}
```

### UART Commands (Board B)

| Command | Syntax | Purpose |
|---------|--------|---------|
| ADDR | `ADDR <addr> <SHORT\|LONG\|ACC\|ACCE>` | Set decoder listen address/type |
| CLEAR | `CLEAR` | Flush RECV ring buffer |
| ACK | `ACK` | Report current ACK pulse width |
| ACK | `ACK <width_us>` | Set ACK pulse width (1000-20000 us) |
| ACK ON | `ACK ON` | Enable ACK pulse generation |
| ACK OFF | `ACK OFF` | Disable ACK pulse generation (forces NO_ACK on CS) |
| STATUS | `STATUS` | Print current address and type |
| HELP | `HELP` | List commands |

The `ADDR` command writes the appropriate CVs and re-initializes the library:
- `ADDR 3 SHORT` → CV1=3, CV29 bit 5 cleared, CV541 bit 7 cleared
- `ADDR 200 LONG` → CV17/CV18 encoded, CV29 bit 5 set, CV541 bit 7 cleared
- `ADDR 0 ACC` → CV513/CV521 encoded, CV541 bit 7 set, bit 5 cleared
- `ADDR 0 ACCE` → CV513/CV521 encoded, CV541 bit 7 set, bit 5 set

### Edge Interrupt ISR

```c
/* GPIO interrupt handler for DCC input on PB1 */
void GROUP1_IRQHandler(void) {
    uint32_t ts = TI_DccDriver_get_timestamp_usec();
    DccConfig_decoder_edge(ts);
    DL_GPIO_clearInterruptStatus(GPIO_DCC_INPUT_PORT, GPIO_DCC_INPUT_DCC_IN_PIN);
}
```

Note: PB1 is on GPIOB which uses GROUP1 interrupt on MSPM0G3507.

### Main Loop

```c
int main(void) {
    SYSCFG_DL_init();

    NVIC_EnableIRQ(GPIOB_INT_IRQn);
    NVIC_EnableIRQ(TIMESTAMP_TIMER_INST_INT_IRQN);

    TI_DccDriver_initialize();
    TI_UartDriver_initialize();
    DecoderCallbacks_initialize();
    DccConfig_initialize(&dcc_config);  /* reads CV1=3, CV29=0x06 defaults */
    DecoderCommandParser_initialize();

    TI_UartDriver_write_string("\r\n");
    TI_UartDriver_write_string("DCC Decoder - MSPM0G3507 LaunchPad\r\n");
    TI_UartDriver_write_string("Type HELP for available commands.\r\n");
    TI_UartDriver_write_string("> ");

    while (1) {
        DccConfig_run();
        DecoderCallbacks_drain();
        TI_UartDriver_echo_process();
        DecoderCommandParser_process();
    }
}
```

### Application Drivers

Reuse from command_station with modifications:
- `ti_driverlib_dcc_driver.c/.h` — keep lock/unlock, get_timestamp_usec, timestamp timer init. **Remove**: timer_start/set_period/stop, track_power_set (not needed for decoder)
- `ti_driverlib_uart_driver.c/.h` — copy verbatim (same UART, same ring buffer, same echo)

### Files to Create

| File | Source | Notes |
|---|---|---|
| `decoder.syscfg` | Adapt from `command_station.syscfg` | Remove TIMA1, add PB1 edge input, add PA28 input, remove PB12/PB17/PB26/PB27 |
| `dcc_user_config.h` | Adapt from command_station | `DCC_COMPILE_DECODER` only |
| `decoder.c` | New | Main, ISR, config wiring |
| `decoder_callbacks.h/.c` | New | RECV ring buffer + callback implementations |
| `decoder_command_parser.h/.c` | New | ADDR, CLEAR, ACK, STATUS, HELP |
| `application_drivers/ack_pulse_driver.h/.c` | New | One-shot GPIO ACK pulse driver (PA31 + TIMG12) |
| `application_drivers/ti_driverlib_dcc_driver.h/.c` | Adapt from command_station | Remove timer/power functions |
| `application_drivers/ti_driverlib_uart_driver.h/.c` | Copy from command_station | Verbatim |
| `dcc_lib` | Symlink | Same as command_station → `../../../../../../src/dcc` |
| `ti/` | Copy from command_station | Same driverlib |
| Project files | Copy from command_station | `.project`, `.cproject`, `.ccsproject`, etc. — rename |

---

## PC Test Script — Python

### Location

`applications/ti_theia/mspm03507_launchpad/loopback_test/`

### Files

- `dcc_loopback_test.py` — pytest test suite
- `dcc_test_fixture.py` — serial communication helper class
- `conftest.py` — pytest fixtures
- `requirements.txt` — dependencies

### Dependencies

```
pyserial>=3.5
pytest>=7.0
```

### Configuration

Serial ports passed via environment variables or command line:

```bash
pytest dcc_loopback_test.py --cmd-port /dev/tty.usbmodemXXXX --dec-port /dev/tty.usbmodemYYYY
```

### Test Flow (per test case)

1. Set decoder address via `ADDR` command to Board B
2. Send `CLEAR` to Board B (flush any prior RECV lines)
3. Flush Board B serial input buffer (discard stale data)
4. Send command to Board A (e.g., `SPEED 3 64 FWD`)
5. Wait for `OK` response from Board A (timeout 1s)
6. Wait for `RECV` line(s) from Board B (timeout 500ms)
7. Parse Board B response fields and compare against expected values
8. Report pass/fail

### Address Switching Strategy

The decoder can only listen on one address at a time. Tests are grouped by address type:

1. **Short address tests** — `ADDR 3 SHORT` (covers SPEED, ESTOP addressed, FUNC, CV)
2. **Long address tests** — `ADDR 200 LONG` (covers SPEED, FUNC with long addr)
3. **Broadcast tests** — `ADDR 0 SHORT` with address 0 = broadcast (covers ESTOP broadcast)
4. **Accessory tests** — `ADDR 0 ACC` (covers ACC commands)
5. **Extended accessory tests** — `ADDR 0 ACCE` (covers ACCE commands)

### Test Matrix

#### Speed Commands (decoder addr=3 SHORT)

| # | Command (Board A) | Expected RECV (Board B) | Notes |
|---|---|---|---|
| 1 | `SPEED 3 0 FWD` | `RECV SPEED addr=3 speed=0 dir=FWD mode=128` | Stop |
| 2 | `SPEED 3 64 FWD` | `RECV SPEED addr=3 speed=64 dir=FWD mode=128` | Mid speed |
| 3 | `SPEED 3 127 FWD` | `RECV SPEED addr=3 speed=127 dir=FWD mode=128` | Max speed |
| 4 | `SPEED 3 64 REV` | `RECV SPEED addr=3 speed=64 dir=REV mode=128` | Reverse |
| 5 | `SPEED 3 1 FWD` | `RECV ESTOP addr=3` | Speed 1 = e-stop in 128-step |

#### Speed Commands — Long Address (decoder addr=200 LONG)

| # | Command (Board A) | Expected RECV (Board B) |
|---|---|---|
| 6 | `SPEED 200 64 FWD` | `RECV SPEED addr=200 speed=64 dir=FWD mode=128` |
| 7 | `SPEED 200L 64 FWD` | `RECV SPEED addr=200 speed=64 dir=FWD mode=128` |

#### Speed Commands — Forced Short (decoder addr=40 SHORT)

| # | Command (Board A) | Expected RECV (Board B) |
|---|---|---|
| 8 | `SPEED 40S 64 FWD` | `RECV SPEED addr=40 speed=64 dir=FWD mode=128` |
| 9 | `SPEED 40 64 FWD` | `RECV SPEED addr=40 speed=64 dir=FWD mode=128` |

#### E-Stop Commands

| # | Decoder Addr | Command (Board A) | Expected RECV (Board B) |
|---|---|---|---|
| 10 | 3 SHORT | `ESTOP 3` | `RECV ESTOP addr=3` |
| 11 | 3 SHORT | `ESTOP` | `RECV ESTOP addr=0` (broadcast e-stop, addr=0) |

Note: Broadcast e-stop uses address 0. The decoder with address 3 will receive it because address 0 is broadcast.

#### Function Commands (decoder addr=3 SHORT)

Each test sends one FUNC command. The decoder fires `on_function_command` once per function in the group. Since the command station tracks bitmask state and sends the full group, the decoder will fire callbacks for ALL functions in that group. The test must account for this.

**Strategy**: Before each function test, ensure the loco state is clean (Board A has no prior function state for address 3). Use `REFRESH OFF` to get deterministic single-shot packets.

| # | Command (Board A) | Expected RECV containing | Group |
|---|---|---|---|
| 12 | `FUNC 3 0 ON` | `RECV FUNC addr=3 func=0 state=ON` | Group 1 (FL) |
| 13 | `FUNC 3 1 ON` | `RECV FUNC addr=3 func=1 state=ON` | Group 1 |
| 14 | `FUNC 3 4 ON` | `RECV FUNC addr=3 func=4 state=ON` | Group 1 |
| 15 | `FUNC 3 5 ON` | `RECV FUNC addr=3 func=5 state=ON` | Group 2a |
| 16 | `FUNC 3 8 ON` | `RECV FUNC addr=3 func=8 state=ON` | Group 2a |
| 17 | `FUNC 3 9 ON` | `RECV FUNC addr=3 func=9 state=ON` | Group 2b |
| 18 | `FUNC 3 12 ON` | `RECV FUNC addr=3 func=12 state=ON` | Group 2b |
| 19 | `FUNC 3 13 ON` | `RECV FUNC addr=3 func=13 state=ON` | F13-F20 |
| 20 | `FUNC 3 20 ON` | `RECV FUNC addr=3 func=20 state=ON` | F13-F20 |
| 21 | `FUNC 3 21 ON` | `RECV FUNC addr=3 func=21 state=ON` | F21-F28 |
| 22 | `FUNC 3 28 ON` | `RECV FUNC addr=3 func=28 state=ON` | F21-F28 |
| 23 | `FUNC 3 29 ON` | `RECV FUNC addr=3 func=29 state=ON` | F29-F36 |
| 24 | `FUNC 3 36 ON` | `RECV FUNC addr=3 func=36 state=ON` | F29-F36 |
| 25 | `FUNC 3 37 ON` | `RECV FUNC addr=3 func=37 state=ON` | F37-F44 |
| 26 | `FUNC 3 44 ON` | `RECV FUNC addr=3 func=44 state=ON` | F37-F44 |
| 27 | `FUNC 3 45 ON` | `RECV FUNC addr=3 func=45 state=ON` | F45-F52 |
| 28 | `FUNC 3 52 ON` | `RECV FUNC addr=3 func=52 state=ON` | F45-F52 |
| 29 | `FUNC 3 53 ON` | `RECV FUNC addr=3 func=53 state=ON` | F53-F60 |
| 30 | `FUNC 3 60 ON` | `RECV FUNC addr=3 func=60 state=ON` | F53-F60 |
| 31 | `FUNC 3 61 ON` | `RECV FUNC addr=3 func=61 state=ON` | F61-F68 |
| 32 | `FUNC 3 68 ON` | `RECV FUNC addr=3 func=68 state=ON` | F61-F68 |
| 33 | `FUNC 3 1 OFF` | `RECV FUNC addr=3 func=1 state=OFF` | Group 1 |
| 34 | `FUNC 3 29 OFF` | `RECV FUNC addr=3 func=29 state=OFF` | F29-F36 |

Note: Because function commands are cumulative bitmasks, `FUNC 3 1 ON` after `FUNC 3 0 ON` will produce RECV lines for BOTH func=0 and func=1. The test script must track cumulative state or reset between tests.

#### Accessory Commands (decoder addr=0 ACC)

| # | Command (Board A) | Expected RECV (Board B) |
|---|---|---|
| 24 | `ACC 1 0 ON` | `RECV ACC board=1 pair=0 activate=ON` |
| 25 | `ACC 1 0 OFF` | `RECV ACC board=1 pair=0 activate=OFF` |
| 26 | `ACC 511 3 ON` | `RECV ACC board=511 pair=3 activate=ON` |

#### Extended Accessory Commands (decoder addr=0 ACCE)

| # | Command (Board A) | Expected RECV (Board B) |
|---|---|---|
| 27 | `ACCE 1 0` | `RECV ACCE addr=1 aspect=0` |
| 28 | `ACCE 1 31` | `RECV ACCE addr=1 aspect=31` |

#### CV Commands — Ops Mode (decoder addr=3 SHORT)

| # | Command (Board A) | Expected RECV (Board B) |
|---|---|---|
| 29 | `CV WRITE 3 29 6` | `RECV CV_WRITE cv=29 value=6` |
| 30 | `CV WRITE 3 1 3` | `RECV CV_WRITE cv=1 value=3` |
| 31 | `CV VERIFY 3 29 6` | `RECV CV_VERIFY cv=29 value=6` |
| 32 | `CV BIT 3 29 3 1` | `RECV CV_BIT cv=29 bit=3 value=1` |
| 33 | `CV BIT 3 29 0 0` | `RECV CV_BIT cv=29 bit=0 value=0` |

Note: CV writes require the decoder lock to be open (CV15 == CV16). The stub initializes both to 0.

#### Consist Control (decoder addr=3 SHORT)

| # | Command (Board A) | Expected RECV (Board B) |
|---|---|---|
| 34 | `CONSIST 3 SET 10 NORMAL` | `RECV CONSIST addr=3 consist=10 dir=NORMAL` |
| 35 | `CONSIST 3 SET 10 REVERSE` | `RECV CONSIST addr=3 consist=10 dir=REVERSE` |
| 36 | `CONSIST 3 CLEAR` | `RECV CONSIST addr=3 consist=0` |

#### Binary State Control — Short (decoder addr=3 SHORT)

| # | Command (Board A) | Expected RECV (Board B) |
|---|---|---|
| 37 | `BSS 3 1 ON` | `RECV BSS addr=3 state=1 active=ON` |
| 38 | `BSS 3 1 OFF` | `RECV BSS addr=3 state=1 active=OFF` |
| 39 | `BSS 3 127 ON` | `RECV BSS addr=3 state=127 active=ON` |

#### Binary State Control — Long (decoder addr=3 SHORT)

| # | Command (Board A) | Expected RECV (Board B) |
|---|---|---|
| 40 | `BSL 3 1 ON` | `RECV BSL addr=3 state=1 active=ON` |
| 41 | `BSL 3 1 OFF` | `RECV BSL addr=3 state=1 active=OFF` |
| 42 | `BSL 3 32767 ON` | `RECV BSL addr=3 state=32767 active=ON` |

#### Analog Function (decoder addr=3 SHORT)

| # | Command (Board A) | Expected RECV (Board B) |
|---|---|---|
| 43 | `ANALOG 3 1 128` | `RECV ANALOG addr=3 output=1 value=128` |
| 44 | `ANALOG 3 1 0` | `RECV ANALOG addr=3 output=1 value=0` |
| 45 | `ANALOG 3 1 255` | `RECV ANALOG addr=3 output=1 value=255` |

#### Speed Restriction (decoder addr=3 SHORT)

| # | Command (Board A) | Expected RECV (Board B) |
|---|---|---|
| 46 | `RESTRICT 3 ON 50` | `RECV RESTRICT addr=3 enabled=ON limit=50` |
| 47 | `RESTRICT 3 OFF` | `RECV RESTRICT addr=3 enabled=OFF` |

#### Edge Cases

| # | Test | Setup | Expected |
|---|---|---|---|
| 31 | Power off stops DCC | `POWER OFF`, send `SPEED 3 64 FWD` | Board A returns OK but no RECV on Board B |
| 32 | Power on resumes | `POWER ON` after off | Idle packets resume, decoder exits failsafe |
| 33 | Wrong address filtered | Decoder addr=3, send `SPEED 5 64 FWD` | No RECV (filtered by decoder) |
| 34 | Address type mismatch | Decoder addr=3 SHORT, send `SPEED 3L 64 FWD` | No RECV (long addr doesn't match short) |

---

## Implementation Order

### Phase 1: Decoder Project (Board B)
1. Copy project infrastructure from command_station (project files, driverlib, dcc_lib symlink)
2. Create `decoder.syscfg` — adapt from command_station (TIMA0 timestamp, UART0, PB1 edge input, PA28 input, PB22 LED, SysTick)
3. Create `dcc_user_config.h` with `DCC_COMPILE_DECODER` only
4. Create `application_drivers/` — adapt from command_station (keep timestamp + UART, remove timer/power)
5. Create `decoder_callbacks.h/.c` — RECV ring buffer + all 15 callback implementations
6. Create `decoder_command_parser.h/.c` — ADDR, CLEAR, STATUS, HELP
7. Create `decoder.c` — main, edge ISR (GROUP1_IRQHandler), SysTick, config wiring
8. Build, flash, manual test with command_station

### Phase 2: Python Test Script
9. Create `loopback_test/dcc_test_fixture.py` — serial helper class
10. Create `loopback_test/conftest.py` — pytest fixtures
11. Create `loopback_test/dcc_loopback_test.py` — full test matrix
12. Create `loopback_test/requirements.txt`
13. Run tests, iterate

---

## Notes

- **REFRESH OFF recommended** for testing — sends each packet exactly 3 times, giving deterministic behavior. With REFRESH ON, speed/function packets repeat indefinitely, flooding Board B with duplicate RECV lines.
- **Function test ordering matters** — function commands are cumulative bitmasks. Either reset Board A between tests (reboot or clear loco table) or track cumulative state in the test script.
- **Decoder callbacks fire from ISR** — all RECV output must be buffered and drained from main loop. Ring buffer overflow drops messages silently (16 slots should be enough for 3 repeats of any single command).
- **CV lock** — DecoderCallbacks_initialize() sets CV15=CV16=0 (unlocked). If a test writes CV15 or CV16 to different values, subsequent CV writes will fail. Test script should be aware of this ordering.
- **GROUP1_IRQHandler** — PB1 is on GPIOB, which maps to GROUP1 interrupt on MSPM0G3507. This must not conflict with other GPIOB interrupts.
- **PA28 trigger input** — available for the test script to verify timing correlation between sent and received packets. Not required for basic functional tests but useful for timing analysis.

---

## Bugs Found and Fixed During Loopback Testing

| Bug | File | Fix |
|-----|------|-----|
| Broadcast address 0 rejected by short-address decoder | `dcc_packet_decoder.c` | Removed `data[0] >= 0x01` lower bound check — address 0 now enters the short address block |
| Basic accessory output_pair only read 2 of 3 DDD bits | `dcc_packet_decoder.c` | Changed `(data[1] >> 1) & 0x03` → `data[1] & 0x07` to read all 3 bits per NMRA S-9.2.1 `1AAACDDD` |
| Loco table lookup matched address only, ignoring address_type | `uart_command_parser.c` | Added `&& _loco_table[i].address_type == addr_type` to `_find_or_create_loco` — prevents `SPEED 3L` from reusing a short-address-3 entry |

---

## NMRA Spec Coverage Analysis

Cross-referenced against S-9.1, S-9.2, S-9.2.1, S-9.2.1.1, S-9.2.2, S-9.2.3, S-9.2.4, S-9.3.2.

### Covered by Loopback Tests (85 tests — all passing)

| Spec | Requirement | Test(s) |
|---|---|---|
| S-9.2.1 | 128-step speed encoding: stop (0), e-stop (1), min (2), mid (64), near-max (126), max (127) | TestSpeedShort, TestSpeedBoundaries |
| S-9.2 | 28-step speed encoding: stop (0), e-stop (1), min (2), mid (15), max (29), reverse | TestSpeed28Step |
| S-9.2 | 14-step speed encoding: stop (0), e-stop (1), min (2), max (15), reverse | TestSpeed14Step |
| S-9.2.1 | 128-step direction: FWD/REV | test_speed_reverse |
| S-9.2 | Short address range: 1-127 | TestAddressBoundaries (1, 40, 127) |
| S-9.2.1 | Long address range: 128-10239 | TestSpeedLong, TestAddressBoundaries (128, 9999) |
| S-9.2 | Broadcast address (0) for e-stop | test_estop_broadcast |
| S-9.2 | Address filtering: wrong address rejected | test_wrong_address_filtered |
| S-9.2.1 | Address type filtering: short/long mismatch rejected | test_address_type_mismatch |
| S-9.2 | Short/long boundary: 127 short != 128 long | test_short_long_boundary_127_vs_128 |
| S-9.2 | Broadcast e-stop (all decoders) | test_estop_broadcast |
| S-9.2 | Addressed e-stop | test_estop_addressed |
| S-9.2 | Speed 1 = e-stop in 128-step mode | test_speed_1_is_estop |
| S-9.2 | Function Group 1: FL, F1-F4 | test_func_fl, test_func_f1, test_func_f4 |
| S-9.2 | Function Group 2a: F5-F8 | test_func_f5, test_func_f8 |
| S-9.2 | Function Group 2b: F9-F12 | test_func_f9, test_func_f12 |
| S-9.2.1 | F13-F20 | test_func_f13, test_func_f20 |
| S-9.2.1 | F21-F28 | test_func_f21, test_func_f28 |
| S-9.2.1 | F29-F36 | test_func_f29, test_func_f36 |
| S-9.2.1 | F37-F44 | test_func_f37, test_func_f44 |
| S-9.2.1 | F45-F52 | test_func_f45, test_func_f52 |
| S-9.2.1 | F53-F60 | test_func_f53, test_func_f60 |
| S-9.2.1 | F61-F68 | test_func_f61, test_func_f68 |
| S-9.2 | Function OFF | test_func_f1_off, test_func_f29_off |
| S-9.2.1 | Basic accessory: on/off, address range 1-511, output pairs 0-3 | TestAccessoryBasic |
| S-9.2.1 | Extended accessory: aspects 0-31 | TestAccessoryExtended |
| S-9.2.1 | Ops-mode CV write (long form) | TestCvOpsMode |
| S-9.2.1 | Ops-mode CV verify | test_cv_verify |
| S-9.2.1 | Ops-mode CV bit manipulation (write/clear) | test_cv_bit_write, test_cv_bit_clear |
| S-9.2.1 | CV write requires >= 2 identical packets (repeat_count=3) | TestRepeatCounts |
| S-9.2.2 | CV decoder lock: CV15 != CV16 blocks writes | TestCvDecoderLock |
| S-9.2.2 | CV decoder lock: CV15/CV16 always writable | test_cv_write_blocked_when_locked |
| S-9.2.2 | CV decoder unlock: CV15 == CV16 permits writes | test_cv_write_after_unlock |
| S-9.2.1 | Consist control: set normal/reverse, clear | TestConsist |
| S-9.2.1 | Binary state control (short form): on/off, max (127) | TestBinaryStateShort |
| S-9.2.1 | Binary state control (long form): on/off, max (32767) | TestBinaryStateLong |
| S-9.2.1 | Analog function output: 0, 128, 255 | TestAnalogFunction |
| S-9.2.1 | Speed restriction: on with limit, off | TestSpeedRestriction |
| S-9.2 | Idle packet does not trigger decoder callbacks | TestIdlePackets |
| S-9.2 | Power off stops DCC signal | test_power_off_stops_dcc |
| S-9.2 | Power on resumes DCC signal | test_power_off_stops_dcc (restores power) |
| S-9.2.3 | Direct mode byte write with ACK | TestServiceModeDirect |
| S-9.2.3 | Direct mode byte verify (match + mismatch) | TestServiceModeDirect |
| S-9.2.3 | Direct mode bit write/verify | TestServiceModeDirect |
| S-9.2.3 | Paged mode write/verify | TestServiceModePaged |
| S-9.2.3 | Register mode write/verify | TestServiceModeRegister |
| S-9.2.3 | Address-only mode write | TestServiceModeAddress |
| S-9.2.3 | ACK pulse >= 5ms detected as valid | TestServiceModeCompliance |
| S-9.2.3 | ACK pulse < 5ms rejected | TestServiceModeCompliance |
| S-9.2.3 | ACK disabled → NO_ACK result | TestServiceModeAckDisabled |

### NOT Covered — Requires Hardware Beyond Loopback

| Spec | Requirement | Reason |
|---|---|---|
| S-9.1 | One-bit half-period: 55-61 us | Requires oscilloscope or logic analyzer |
| S-9.1 | Zero-bit half-period: >= 95 us | Requires oscilloscope or logic analyzer |
| S-9.1 | One-bit asymmetry <= 3 us | Requires oscilloscope or logic analyzer |
| S-9.2 | Preamble >= 14 one-bits (ops mode) | Requires bit-level capture |
| S-9.2.3 | Preamble >= 20 one-bits (service mode) | Requires bit-level capture |
| S-9.3.2 | RailCom cutout timing (26-488 us) | RailCom not wired (Phase 3) |
| S-9.3.2 | RailCom 4/8 encoding, channel 1/2 data | RailCom not wired (Phase 3) |
| S-9.2.4 | Failsafe timeout (CV11) | Callbacks wired (RECV FAILSAFE_ENTER/EXIT), but timeout testing requires controlled power interruption |
| S-9.2.4 | 500 ms power loss → decoder stops | Requires controlled power interruption |

### NOT Covered — Not Implemented in Library

| Spec | Requirement | Notes |
|---|---|---|
| S-9.2.1.1 | Logon enable/assign (partition 253/254) | Not implemented in library |
| S-9.2.1.1 | CRC-8 for packets > 6 bytes | Not implemented in library |

### Future Test Additions (when hardware/features are available)

| Priority | Test | Blocked By |
|---|---|---|
| High | RailCom address broadcast | RailCom wiring (Phase 3) |
| Medium | Failsafe timeout (CV11) | Callbacks wired; needs controlled power interruption test |
| Low | Logon sequence | Implement S-9.2.1.1 |

---

## Service Mode ACK Testing (Phase 2)

### Hardware

ACK is simulated via GPIO: decoder Board B pulses PA31 HIGH for a configurable
duration when a service mode CV command is received. CS Board A reads PB15 as a
digital current sense substitute (`current_sense_read` returns 100 when HIGH,
0 when LOW — above the 60 mA threshold used by the existing ACK detection logic
in `dcc_service_mode_common.c`).

```
Board B PA31 (ACK out) ────> Board A PB15 (ACK in)
```

### ACK Pulse Driver (Board B)

New files: `application_drivers/ack_pulse_driver.h` and `.c`

```c
void AckPulseDriver_initialize(void);
void AckPulseDriver_set_width_us(uint32_t width_us);  // default 6000 (6ms)
uint32_t AckPulseDriver_get_width_us(void);
void AckPulseDriver_set_enabled(bool enabled);         // default true
bool AckPulseDriver_is_enabled(void);
void AckPulseDriver_fire(void);  // ISR-safe, no-op if disabled or already active
```

- `fire()` sets PA31 HIGH, starts TIMG12 one-shot timer
- TIMG12 ISR clears PA31 LOW when timer expires
- `fire()` is no-op when disabled or when timer is already running (prevents
  double-fire for bit-write where both `on_cv_write` and `on_cv_bit` fire)
- `ACK ON/OFF` UART command controls `set_enabled()` for test isolation

### CS current_sense_read (Board A)

New function in `ti_driverlib_dcc_driver.c`:

```c
uint16_t TI_DccDriver_current_sense_read(void) {
    uint32_t pins = DL_GPIO_readPins(GPIO_ACK_SENSE_PORT, GPIO_ACK_SENSE_ACK_IN_PIN);
    return (pins & GPIO_ACK_SENSE_ACK_IN_PIN) ? 100 : 0;
}
```

Wired into `dcc_config_t` as `.current_sense_read = &TI_DccDriver_current_sense_read`
(replaces current `NULL`).

### Service Mode Result Reporting (Board A)

The `on_service_mode_complete` callback outputs an async result line:

```
SVC RESULT: SUCCESS
SVC RESULT: NO_ACK
```

### ACK Rules per NMRA S-9.2.3

- **Write (byte or bit)**: Always ACK on successful write
- **Verify byte**: ACK only if stored CV value matches the verify value
- **Verify bit**: ACK only if the specified bit in the stored CV matches

Per S-9.2.3 Section D: "at least 60 mA for 6 ms +/- 1 ms" — the spec defines a
**minimum** pulse duration (5ms) but **no maximum**. Any pulse >= 5ms at >= 60 mA
is a valid ACK.

### Service Mode Test Matrix

#### Direct Mode (TestServiceModeDirect)

| # | Setup | Command (Board A) | Expected Result | Notes |
|---|---|---|---|---|
| 1 | ACK ON, width=6000 | `SVC DIRECT WRITE 5 42` | `SVC RESULT: SUCCESS` | Byte write, standard ACK |
| 2 | After test 1 | `SVC DIRECT VERIFY 5 42` | `SVC RESULT: SUCCESS` | Verify match |
| 3 | After test 1 | `SVC DIRECT VERIFY 5 99` | `SVC RESULT: NO_ACK` | Verify mismatch — no ACK |
| 4 | ACK ON, width=6000 | `SVC DIRECT BITW 5 3 1` | `SVC RESULT: SUCCESS` | Bit write |
| 5 | Write CV5=8 first | `SVC DIRECT BITV 5 3 1` | `SVC RESULT: SUCCESS` | Bit verify match (bit 3 of 8 = 1) |
| 6 | Write CV5=0 first | `SVC DIRECT BITV 5 3 1` | `SVC RESULT: NO_ACK` | Bit verify mismatch |

#### Paged Mode (TestServiceModePaged)

| # | Setup | Command (Board A) | Expected Result |
|---|---|---|---|
| 7 | ACK ON, width=6000 | `SVC PAGED WRITE 5 42` | `SVC RESULT: SUCCESS` |
| 8 | After test 7 | `SVC PAGED VERIFY 5 42` | `SVC RESULT: SUCCESS` |
| 9 | After test 7 | `SVC PAGED VERIFY 5 99` | `SVC RESULT: NO_ACK` |

#### Register Mode (TestServiceModeRegister)

| # | Setup | Command (Board A) | Expected Result |
|---|---|---|---|
| 10 | ACK ON, width=6000 | `SVC REG WRITE 1 42` | `SVC RESULT: SUCCESS` |
| 11 | After test 10 | `SVC REG VERIFY 1 42` | `SVC RESULT: SUCCESS` |
| 12 | After test 10 | `SVC REG VERIFY 1 99` | `SVC RESULT: NO_ACK` |

#### Address-Only Mode (TestServiceModeAddress)

| # | Setup | Command (Board A) | Expected Result |
|---|---|---|---|
| 13 | ACK ON, width=6000 | `SVC ADDR WRITE 42` | `SVC RESULT: SUCCESS` |

#### ACK Enable/Disable (TestServiceModeAckDisabled)

| # | Setup | Command (Board A) | Expected Result | Notes |
|---|---|---|---|---|
| 14 | ACK OFF | `SVC DIRECT WRITE 5 42` | `SVC RESULT: NO_ACK` | Decoder suppresses ACK |
| 15 | ACK ON (re-enable) | `SVC DIRECT WRITE 5 42` | `SVC RESULT: SUCCESS` | ACK restored |

#### ACK Pulse Width Compliance (TestServiceModeCompliance)

Per NMRA S-9.2.3: "at least 60 mA for 6 ms +/- 1 ms" — minimum 5ms, no defined maximum.

| # | Decoder ACK Width | Command (Board A) | Expected Result | Notes |
|---|---|---|---|---|
| 16 | 6000 us (6ms) | `SVC DIRECT WRITE 5 42` | `SVC RESULT: SUCCESS` | Nominal |
| 17 | 5000 us (5ms) | `SVC DIRECT WRITE 5 42` | `SVC RESULT: SUCCESS` | Right at NMRA minimum |
| 18 | 8000 us (8ms) | `SVC DIRECT WRITE 5 42` | `SVC RESULT: SUCCESS` | Long valid — no max per spec |
| 19 | 3000 us (3ms) | `SVC DIRECT WRITE 5 42` | `SVC RESULT: NO_ACK` | Below 5ms minimum |

### Test Flow (per service mode test)

1. Send `ACK ON` or `ACK OFF` to Board B as needed
2. Send `ACK <width>` to Board B if testing pulse width compliance
3. Send `SVC ENTER` to Board A → wait for `OK`
4. Send service mode command to Board A → wait for `OK`
5. Wait for `SVC RESULT:` line from Board A (timeout 5s, accounts for retries)
6. Assert result matches expected
7. Send `SVC EXIT` to Board A

### Implementation Order (Phase 2)

1. Add GPIO output (PA31) and ACK_PULSE_TIMER (TIMG12) to decoder SysConfig
2. Create `ack_pulse_driver.h/.c` — one-shot GPIO pulse driver
3. Modify decoder `callbacks_dcc.c` — fire ACK on CV write/verify/bit callbacks
4. Modify decoder `decoder_command_parser.c` — add ACK/ACK ON/ACK OFF commands
5. Modify decoder `decoder.c` — init ACK driver, enable TIMG12 NVIC
6. Add GPIO input (PB15) to CS SysConfig
7. Add `TI_DccDriver_current_sense_read()` to CS driver
8. Wire `.current_sense_read` into CS `dcc_config_t` (replace NULL)
9. Verify `on_service_mode_complete` callback outputs `SVC RESULT:` line
10. Add Python test fixture helpers (`svc_enter`, `svc_exit`, `wait_svc_result`, `set_ack_width`, `set_ack_enabled`)
11. Add Python test classes: TestServiceModeDirect, TestServiceModePaged, TestServiceModeRegister, TestServiceModeAddress, TestServiceModeAckDisabled, TestServiceModeCompliance
