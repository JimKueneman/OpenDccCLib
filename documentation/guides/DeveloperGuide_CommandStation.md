---
title: Developer Guide — Command Station
subtitle: From first project to a fully featured DCC command station
tagline: Covers the library on any platform; the TI MSPM0G3507 LaunchPad is the worked example
---
## 1. Introduction

OpenDccCLib is a C library that implements the NMRA DCC (Digital Command Control) protocol for microcontrollers. It is portable across any hardware platform and toolchain, and it allocates no dynamic memory: every buffer is statically sized at compile time, so it fits processors with very little RAM.

This guide walks through building a complete DCC command station, then explains every module so you understand exactly what the code does and how to extend it. Everything stated here is taken from the library source at the time of generation; where a number matters, the header that defines it is named so you can check it.

### 1.1 What the Library Does (and Does Not Do)

The library handles the protocol: bit encoding with NMRA timing (58 µs one-bit halves, 100 µs zero-bit halves), packet construction with the XOR error byte, a multi-slot scheduler with priorities, duplicate combining and auto-refresh, all four NMRA service-mode methods (direct, paged, register, address) with ACK detection, the RailCom cutout and the decoding of RailCom replies, and spec-correct repeat counts for every one-shot packet.

It contains no hardware-specific code. You supply short driver functions that toggle a GPIO, start and stop timers, and read current sense, and the library calls them through function pointers in one `dcc_config_t` struct. It also does not own the H-bridge, the RailCom detector's analog front end, or any track electronics; those are yours.

### 1.2 Platform Support

Because the library never touches hardware directly, it ports to any processor and toolchain with a C compiler, one periodic timer, and a few GPIOs.

| Processor / Board | Example IDE / Toolchain |
|---|---|
| TI MSPM0G3507 LaunchPad | Code Composer Studio / Theia (the shipped example) |
| RP2040 / RP2350 | Arduino IDE, PlatformIO, Pico SDK |
| STM32 (F4 and others) | STM32CubeIDE |
| ESP32 | Arduino IDE, PlatformIO |
| Any ARM Cortex-M | GCC + Makefile |

> This guide uses the TI MSPM0G3507 as the worked example. All concepts apply equally to any other target; only the two driver files change.

## 2. DCC Protocol Concepts

### 2.1 Signal Encoding

The DCC signal is a square wave whose bit value is encoded in the half-period duration. A one-bit has a nominal half-period of 58 µs (NMRA allows 55–61 µs). A zero-bit has a minimum half-period of 100 µs. The command station alternates the track polarity at these intervals. The constants live in `dcc_defines.h`.

| Bit | Half-period (library) | NMRA range | Define |
|---|---|---|---|
| One | 58 µs | 55–61 µs | `DCC_ONE_BIT_HALF_PERIOD_US` |
| Zero | 100 µs | ≥ 95 µs, total ≤ 12 000 µs | `DCC_ZERO_BIT_HALF_PERIOD_US`, `DCC_ZERO_BIT_MAX_TOTAL_DURATION_US` |

### 2.2 Packet Format

Every packet is: preamble (one-bits) | start bit 0 | address byte | start bit 0 | instruction byte(s) | start bit 0 | XOR byte | end bit 1. The XOR byte is the exclusive-or of all preceding data bytes. The operations-mode preamble length is yours to set with `USER_DEFINED_DCC_PREAMBLE_BITS_OPS`; the library requires at least 14, or 16 when RailCom is compiled in, because the cutout can blank the first preamble bits and a decoder still needs 10 driven ones. Service-mode packets use `DCC_PREAMBLE_BITS_SERVICE` = 20.

### 2.3 Address Types

| Address type | Range | Bytes | Enum |
|---|---|---|---|
| Broadcast | 0 | 1 | `DCC_ADDRESS_BROADCAST` |
| Short (7-bit) | 1–127 | 1 | `DCC_ADDRESS_SHORT` |
| Long (14-bit) | 128–10239 | 2, first byte 0xC0–0xE7 | `DCC_ADDRESS_LONG` |
| Idle | 255 | 1 | `DCC_ADDRESS_IDLE` |
| Basic accessory | 1–511 (board) | 2 | `DCC_ADDRESS_ACCESSORY` |
| Extended accessory | 1–2047 | 2 | `DCC_ADDRESS_ACCESSORY_EXTENDED` |

> Accessory builders take the 9-bit **board** address plus an output pair, not a flat 11-bit output number. The library encodes the wire form; do not pre-shift addresses yourself.

### 2.4 Instruction Types

| Instruction | Mask (`dcc_defines.h`) | Description |
|---|---|---|
| Speed 14/28-step | `0x40` reverse / `0x60` forward | Baseline speed and direction |
| Speed 128-step | `0x20` advanced ops + `0x3F` | 126 steps plus stop and e-stop |
| Function group 1 | `0x80` | FL, F1–F4 |
| Function group 2a / 2b | `0xB0` / `0xA0` | F5–F8 / F9–F12 |
| Feature expansion | `0xC0` + sub-instruction | F13–F68, binary state, analog, time and date, system time |
| CV access, long form | `0xE0` (`0xEC` write, `0xE4` verify, `0xE8` bit) | Operations-mode CV programming (POM) |
| Consist control | `0x12` / `0x13` set, `0x10` clear | Advanced consisting (CV19) |

## 3. Project File Structure

The files in `dcc_lib/` are library code and should not be edited. The files at the top level and in `application_callbacks/` and `application_drivers/` are yours.

```
command_station/                         <- your project folder
  command_station.c                      <- main entry: config struct, ISRs, main loop
  dcc_user_config.h                      <- REQUIRED: feature flags and sizes
  uart_command_parser.c/h                <- demo UART command-line interface
  application_callbacks/
    callbacks_dcc.c/h                    <- application event hooks
  application_drivers/
    ti_driverlib_dcc_driver.c/h          <- timers, GPIO, current sense (hardware)
    ti_driverlib_uart_driver.c/h         <- UART I/O
  dcc_lib/ -> ../../../../src/dcc        <- library core (symlink; do not edit)
    dcc_config.h/c                       - config struct, lifecycle, wiring of every module
    dcc_types.h, dcc_defines.h           - types and NMRA constants
    dcc_bit_encoder.h/c                  - ISR-level bit serialization
    dcc_scheduler.h/c                    - slots, priority, duplicate combining, auto-refresh
    dcc_application_command_station_packet.h/c      - packet builders
    dcc_application_command_station_main_track.h/c  - power, send, auto-refresh
    dcc_application_command_station_service_track.h/c - service mode API
    dcc_service_mode_*.h/c               - per-mode primitives and task orchestrators
    dcc_railcom_cutout.h/c               - cutout timer state machine
    dcc_railcom_command_station.h/c      - RailCom receive, datagram assembly
    dcc_railcom_utilities.h/c            - 4/8 code words (shared with the decoder role)
```

## 4. dcc_user_config.h in Depth

This file decides which role compiles and how much RAM the library reserves. Every constant is validated in `dcc_types.h` with a `#error`, so a missing one fails the build rather than the run.

### 4.1 Role and Feature Flags

```
#define DCC_COMPILE_COMMAND_STATION
#define DCC_COMPILE_RAILCOM                  // cutout + receive; comment out to strip

#define DCC_COMPILE_SERVICE_MODE_DIRECT      // primitives
#define DCC_COMPILE_SERVICE_MODE_PAGED
#define DCC_COMPILE_SERVICE_MODE_REGISTER
#define DCC_COMPILE_SERVICE_MODE_ADDRESS

#define DCC_COMPILE_SERVICE_MODE_TASK_DIRECT // orchestrators (read/write/verify sequences)
#define DCC_COMPILE_SERVICE_MODE_TASK_PAGED
#define DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
#define DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS
#define DCC_COMPILE_SERVICE_MODE_TASK_DETECT
```

Each `DCC_COMPILE_SERVICE_MODE_*` flag requires `DCC_COMPILE_COMMAND_STATION`; `dcc_config.h` emits a `#error` otherwise. RailCom and service mode are compiled in with their flags but enabled at **runtime** by what you wire: leave the corresponding hardware pointers NULL and the feature is inert.

### 4.2 Sizes and Tuning Constants

| Define | Example value | Meaning |
|---|---|---|
| `USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT` | 24 | Scheduler slots. One (address, tag) pair per slot, so a loco with speed plus two function groups uses three |
| `DCC_REFRESH_PROMPT_SENDS` | 3 (library default) | Full-rate sends of a refresh slot after each insert, before it goes cold |
| `DCC_REFRESH_COLD_CYCLES` | 60 (library default) | Keep-alive interval of a cold refresh slot, in packet cycles (about 0.4 s); 0 disables the tier and restores the flat ring |
| `DCC_REFRESH_COLD_MAX_CYCLES` | 120 (library default) | Longest any refresh slot may go unsent, in packet cycles (about 0.8 s); see 9.5 |
| `DCC_REFRESH_CV11_FLOOR` | 20 (library default) | Smallest decoder CV 11 (0.1 s units, so 2.0 s) the ceiling is guaranteed to stay under; lower it with the ceiling |
| `USER_DEFINED_DCC_MAX_LOCOS` | 10 | Locomotives tracked by the demo application's loco table |
| `USER_DEFINED_DCC_PREAMBLE_BITS_OPS` | 18 | Operations-mode preamble; ≥ 14, ≥ 16 with RailCom |
| `USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH` | 4 | Ring of decoded RailCom datagrams |
| `USER_DEFINED_DCC_SERVICE_MODE_RETRIES` | 3 | Retries before a service-mode operation reports failure |
| `USER_DEFINED_DCC_ACK_THRESHOLD_MA` | 60 | Current-sense value that counts as an ACK |
| `USER_DEFINED_DCC_ACK_MIN_DURATION_US` | 5000 | Shortest pulse accepted as an ACK |
| `USER_DEFINED_DCC_ACK_MAX_DURATION_US` | 7000 | Longest pulse accepted; longer is treated as over-current, not an ACK |
| `USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US` | 116 | Gap inside a pulse the ACK counter tolerates |

The example values are those of the shipped command-station project; `templates/typical/dcc_user_config.h` holds a smaller default set. The three `DCC_REFRESH_*` values are library defaults from `dcc_defines.h`; define any of them in `dcc_user_config.h` to override.

## 5. Initialization — command_station.c

The main file wires the hardware drivers, the library, and your callbacks. The pattern is always the same: define the config struct, initialize hardware, initialize the library, enter the main loop.

### 5.1 The dcc_config_t Struct

All fields are function pointers or timing values. The library calls the pointers when it needs to toggle a pin, start a timer, or notify you.

```
static const dcc_config_t dcc_config = {
    /* REQUIRED: common platform drivers */
    .lock_shared_resources   = &TI_DccDriver_lock_shared_resources,
    .unlock_shared_resources = &TI_DccDriver_unlock_shared_resources,
    .get_timestamp_usec      = &TI_DccDriver_get_timestamp_usec,

    /* shared 58 us timer: clocks both track encoders */
    .shared_timer_start      = &TI_DccDriver_shared_timer_start,
    .shared_timer_stop       = &TI_DccDriver_shared_timer_stop,

    /* RailCom cutout one-shot timer + per-state periods (0 = spec default) */
    .railcom_timer_start     = &TI_DccDriver_railcom_timer_start,
    .railcom_timer_stop      = &TI_DccDriver_railcom_timer_stop,
    .railcom_cutout_start_delay_us = 26,   /* DELAY    -> T_CS  = 26 us  */
    .railcom_uart_rx_delay_us      = 54,   /* SETTLING -> T_TS1 = 80 us  */
    .railcom_ch1_window_us         = 97,   /* CH1      -> T_TC1 = 177 us */
    .railcom_ch1_ch2_gap_us        = 16,   /* GAP      -> T_TS2 = 193 us */
    .railcom_ch2_window_us         = 261,  /* CH2      -> T_CE  = 454 us */

    .main_track    = { .pin_toggle = &TI_DccDriver_main_pin_toggle,
                       .track_power_set = &TI_DccDriver_track_power_set,
                       .railcom = NULL },              /* detector not fitted on the demo */
    .service_track = { .pin_toggle = &TI_DccDriver_svc_pin_toggle,
                       .track_power_set = &TI_DccDriver_svc_track_power_set,
                       .current_sense_read = &TI_DccDriver_current_sense_read },

    /* OPTIONAL application callbacks (NULL = no notification) */
    .on_packet_sent = &CallbacksDcc_on_packet_sent,
};
```

### 5.2 Per-Channel Hardware (dcc_output_hw_t)

Each DCC output channel, main track and service track, has its own set of pointers. With the shared-timer architecture the per-channel `timer_start` and `timer_stop` are left NULL; `pin_toggle` does the work.

| Field | Required? | Description |
|---|---|---|
| `pin_toggle` | REQUIRED | Toggle this channel's DCC output pin. ISR context; keep it to one register write |
| `track_power_set` | REQUIRED | Enable or disable the H-bridge for this channel |
| `timer_start` / `timer_stop` | NULL with the shared timer | Per-channel timer; unused in the shipped design |
| `current_sense_read` | Service track | Return milliamps (ADC) or 0 / non-zero (comparator). Used for ACK detection |
| `railcom` | NULL if no detector | Pointer to a `dcc_railcom_hw_t` (main track only in practice) |

### 5.3 RailCom Hardware (dcc_railcom_hw_t)

| Field | Called by the library at | Your job |
|---|---|---|
| `begin_railcom_cutout` | T_CS | Tri-state the H-bridge |
| `end_railcom_cutout` | T_CE | Restore normal drive |
| `uart_rx_enable` | T_TS1 and T_TS2 | Open the receiver for the Channel 1, then the Channel 2, window |
| `uart_rx_disable` | T_TC1 and T_CE | Close it |
| `uart_read` | after the cutout, from `DccConfig_run()` | Pop one received byte; return false when empty |
| `on_railcom_datagram_result` | after decoding, main loop | Receive `(address, channel, datagram)` |

> **Receive contract.** The library reads whatever `uart_read` returns after a cutout and cannot tell a window byte from a stray one. Accept receiver bytes only between an enable and the following disable, discard anything outside (the DCC drive waveform seen by the detector between cutouts, or a stale byte from an earlier cutout), and flush at `begin_railcom_cutout`. Gate in the RX interrupt or toggle the peripheral's receiver; the bench firmware in `test/compliance` does it in the RX interrupt and is verified on the wire.

### 5.4 Setup and Main Loop

```
int main(void) {
    SYSCFG_DL_init();                    /* MCU clocks, GPIO, timers, UART */
    TI_DccDriver_initialize();
    TI_UartDriver_initialize();
    DccConfig_initialize(&dcc_config);   /* library ready; track power still OFF */
    UartCommandParser_initialize();

    while (1) {
        DccConfig_run();                 /* scheduling, receive drain, callbacks */
        TI_UartDriver_echo_process();
        UartCommandParser_process();
    }
}
```

> `DccConfig_run()` must be called continuously. Every application callback fires from inside it, never from an interrupt, so callbacks may use UART or other slow I/O. Do not block in the loop.

## 6. ISR Architecture

### 6.1 The 58 µs Shared Timer

One hardware timer fires every `DCC_ONE_BIT_HALF_PERIOD_US` = 58 µs. Its ISR calls `DccConfig_58us_timer_isr()`, which ticks the main-track and service-track bit encoders. A one-bit half is one tick, a zero-bit half is two (116 µs, inside the NMRA range). The same ISR also samples current sense on the service track for ACK detection.

```
void DCC_BIT_TIMER_INST_IRQHandler(void) {
    switch (DL_TimerA_getPendingInterrupt(DCC_BIT_TIMER_INST)) {
        case DL_TIMER_IIDX_ZERO:
            TI_DccDriver_timestamp_tick();
            DccConfig_58us_timer_isr();
            break;
        default: break;
    }
}
```

### 6.2 RailCom One-Shot Timer

A second timer runs in one-shot mode and drives the five-state cutout machine. The library restarts it for each phase, and its ISR calls `DccConfig_railcom_oneshot_timer_isr()`. See section 11 for the phase table.

The cutout is armed at the packet end bit's **last edge**. The bit encoder's state machine runs one tick ahead of the wire, so the arm is deferred one tick to land on that edge; this is what keeps T_CS inside 26–32 µs and leaves the end bit intact. It was measured on the hardware-in-the-loop bench at 27.8–28.0 µs from the decoded end bit.

### 6.3 100 ms Periodic Tick

A 100 ms timer, SysTick in the example, calls `DccConfig_100ms_timer_tick()` for time-outs and housekeeping. It is also a convenient place for a heartbeat LED.

## 7. Implementing the Drivers

The driver files are the only hardware-specific code in a project. This section states the contract each function must meet when porting.

| Function | Context | Contract |
|---|---|---|
| `lock_shared_resources` / `unlock_shared_resources` | any | Disable and re-enable interrupts, or take and release a mutex. Keep the region short |
| `get_timestamp_usec` | any | Free-running microsecond counter; monotonic, wrap at 2^32 is fine |
| `shared_timer_start(period)` / `shared_timer_stop` | main loop | Start or stop the 58 µs periodic timer whose ISR calls `DccConfig_58us_timer_isr()` |
| `railcom_timer_start(period)` / `railcom_timer_stop` | ISR | One-shot timer whose ISR calls `DccConfig_railcom_oneshot_timer_isr()` |
| `pin_toggle` | ISR | One register write that toggles the channel's DCC pin |
| `track_power_set(bool)` | main loop | Enable or disable the H-bridge |
| `current_sense_read` | ISR (58 µs) | Return the service-track current. The demo returns 100 or 0 from a digital pin |

## 8. The Bit Encoder

The bit encoder serializes packets one half-bit per tick from ISR context. It is **single-buffered**: it holds one active packet and a `packet_loaded` flag. When the end bit goes out it signals completion; the scheduler loads the next packet from the main loop during the following idle or preamble time. There is no front/back buffer swap.

The encoder walks the packet in order: preamble ones, then for each byte a start bit and eight data bits, then the end bit. At the end bit it also arms the RailCom cutout, as described in 6.2.

## 9. The Scheduler

### 9.1 Slots

The scheduler owns a static array of `USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT` slots. Each holds one packet (up to 6 bytes), its address, tag, priority, repeat count, auto-refresh flag and two pacing counters: full-rate sends still owed and packet cycles since the slot was last sent. `DccApplicationCommandStationMainTrack_send_packet()` inserts a one-shot; `_add_to_auto_refresh()` inserts a refreshed slot. Both reuse an existing slot with the same (address, tag).

### 9.2 Priority

One-shots are selected before refresh, highest priority first.

| Priority (`dcc_priority_enum`) | Use |
|---|---|
| `DCC_PRIORITY_ESTOP` | Emergency stop, highest |
| `DCC_PRIORITY_SPEED` | Speed and direction |
| `DCC_PRIORITY_FUNCTION` | Functions, consist, binary state, analog, time |
| `DCC_PRIORITY_ACCESSORY` | Turnouts and signals |
| `DCC_PRIORITY_CV` | Operations-mode CV programming |
| `DCC_PRIORITY_IDLE` | Idle, lowest |

### 9.3 Duplicate Combining

A new command for the same (address, tag) overwrites the packet in the existing slot instead of taking a second one. Tags (`dcc_tag_enum`) separate command kinds: speed, each function group, accessory, CV, consist, binary state, analog. Rapid throttle changes therefore never fill the scheduler.

### 9.4 Repeat Counts

A one-shot slot is sent `repeat_count` times; the scheduler decrements after each send and drops the slot at zero. A packet handed over with a count of 0 is therefore **never sent**. Every packet builder sets a spec-correct default from the `DCC_REPEAT_*` table in `dcc_defines.h`, and an application may overwrite the field after the builder returns.

| Builders | Default | Basis |
|---|---|---|
| CV write, CV bit write (loco POM and accessory) | 2 | S-9.2.1 2.3.7.3: two identical packets before a decoder modifies a CV; 2.4.3 applies the same to accessory decoders |
| CV verify, CV bit verify | 1 | Acts on the first packet |
| Model date | 3 | S-9.2.1 2.3.6.2: "at least three times" |
| Model time, system time | 1 | Once per update |
| Accessory NOP, accessory stop | 1 | Maintainer policy |
| Everything else: speed, functions, e-stop, accessory on/off, consist, binary state, analog, reset, idle | 2 | S-9.2 section C gives no count; one repeat survives a single lost packet |

### 9.5 Auto-Refresh

Speed and function commands should be repeated so a decoder keeps hearing them. A slot marked for auto-refresh is never dropped until removed, and it is paced in two tiers so a large pool does not slow down the few locomotives being driven:

| Value (`dcc_defines.h`, overridable) | Default | Effect |
|---|---|---|
| `DCC_REFRESH_PROMPT_SENDS` | 3 | Every insert (a new or changed command) is sent this many times at full rate, one per packet cycle, so a single lost packet does not lose the change |
| `DCC_REFRESH_COLD_CYCLES` | 60 (about 0.4 s) | After the burst the slot is "cold" and is re-sent once per this many packet cycles as a keep-alive. 0 disables the tier: every refresh slot is sent in turn, as a flat ring |
| `DCC_REFRESH_COLD_MAX_CYCLES` | 120 (about 0.8 s) | Ceiling: a slot unsent for this long is "overdue" and goes ahead of everything, so a stream of throttle changes cannot hold a locomotive off the track |

Each packet cycle the scheduler ages every refresh slot, then picks, round-robin within each group: an overdue slot; else a slot still in its burst; else a merely-due cold slot. Right after an overdue send a waiting burst goes first, so under overload changes and overdue keep-alives alternate. A changed command therefore reaches the wire within one packet cycle when nothing else is in its burst, and shares the burst pass fairly with other simultaneous changes. A cycle with nothing due sends an idle packet.

The ceiling exists because of the decoder packet time-out (S-9.2.4 section 4, CV 11): a decoder stops when no packet addressed to it arrives in time, and idle packets do not count. With the worst-case packet (6 bytes, all zero bits, a RailCom cutout, about 15 ms) 120 cycles is 1.8 s, so the documented floor for CV 11 on a layout driven by this library is `DCC_REFRESH_CV11_FLOOR` = 20 (2.0 s in the decoder's 0.1 s units) or 0 (off); a compile-time check pins the ceiling under that floor. Keeping the keep-alive under a second also keeps idle locomotives visible to RailCom occupancy detectors, which learn addresses from replies to addressed packets.

Two rules from S-9.2 are enforced in the scheduler regardless of pacing: a same-address packet for short addresses 112–127 is never sent within 5 ms of the previous one (an idle spacer is inserted), and an idle packet goes out whenever nothing else is due.

## 10. Service Mode Programming

Service mode runs on the dedicated programming track. The command station sends specific packet sequences and the decoder answers with a 6 ms current pulse (ACK). The library splits the work into **primitives** (one mode's packet sequences) and **tasks** (read, write and verify orchestration on top of them). Sequence constants are in `dcc_defines.h`: 3 reset packets, 5 command packets, 6 reset packets after, 6 recovery packets.

| Mode | API prefix `DccApplicationCommandStationServiceTrack_` | Notes |
|---|---|---|
| Direct | `direct_read_cv`, `direct_write_cv`, `direct_read_bit`, `direct_write_bit` | Read is eight bit-verifies; write is write then verify |
| Paged | `paged_read_cv`, `paged_write_cv`, `paged_read_bit`, `paged_write_bit` | Page preset then register access; reads scan |
| Register | `register_read_cv`, `register_write_cv`, `register_read_bit`, `register_write_bit`, `register_verify_value`, `register_factory_reset` | Takes a `dcc_decoder_type_enum` (mobile or accessory) per call |
| Address | `address_read`, `address_write`, `address_verify`, `address_read_bit`, `address_write_bit` | CV1 only, short addresses |
| Detect | `detect_mode(on_detect)` | Probes every compiled mode; reports a bitmask of `DCC_SERVICE_MODE_SUPPORTED_*` |

Every task takes an `on_complete(result, value)` callback, most take an `on_progress(phase, step, estimated_steps)` callback, and each returns `bool`: false means it could not start, for example because another operation is running. Results arrive later, from `DccConfig_run()`. Call `enter_service_mode()` first and `exit_service_mode()` when done.

| Result (`dcc_service_mode_result_t`) | Meaning |
|---|---|
| `DCC_SERVICE_MODE_SUCCESS` | Completed; `value` holds the byte or bit read |
| `DCC_SERVICE_MODE_NO_ACK` | No qualifying current pulse; no decoder, or wrong mode |
| `DCC_SERVICE_MODE_VERIFY_FAIL` | Verify after write did not match |
| `DCC_SERVICE_MODE_BUSY` | Another operation is running, or a primitive could not start |
| `DCC_SERVICE_MODE_ERROR` | Internal error, e.g. no current sense wired |
| `DCC_SERVICE_MODE_NOT_IN_SERVICE_MODE` | Call `enter_service_mode()` first |

### 10.1 ACK Detection

The 58 µs ISR samples `current_sense_read()` on the service track during the ACK scan window, which opens after the second command packet per S-9.2.3. A valid ACK exceeds `USER_DEFINED_DCC_ACK_THRESHOLD_MA` for a duration between `USER_DEFINED_DCC_ACK_MIN_DURATION_US` and `USER_DEFINED_DCC_ACK_MAX_DURATION_US`; a longer pulse is rejected as over-current. A gap inside the pulse longer than `USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US` resets the counter. All of this is exercised on the bench through a mock-ACK loopback, including the boundary widths.

## 11. RailCom

After each main-track packet the command station opens a cutout by tri-stating the H-bridge, and a decoder answers with a current signal that a detector converts to 250 kbaud UART data. Channel 1 carries two bytes (address broadcast); Channel 2 up to six (CV read-back, status). The library runs the timing, tells your hardware when to tri-state and when to listen, decodes the 4/8 code words and assembles datagrams; you receive finished datagrams tagged with the address of the packet whose cutout carried them.

| State | Duration (default) | Boundary | Hook called |
|---|---|---|---|
| DELAY | 26 µs | T_CS = 26 µs after the end bit | `begin_railcom_cutout` |
| SETTLING | 54 µs | T_TS1 = 80 µs | `uart_rx_enable` |
| CH1 | 97 µs | T_TC1 = 177 µs | `uart_rx_disable` |
| GAP | 16 µs | T_TS2 = 193 µs | `uart_rx_enable` |
| CH2 | 261 µs | T_CE = 454 µs | `uart_rx_disable`, `end_railcom_cutout` |

The five durations are configurable in `dcc_config_t` (0 selects the default from `dcc_defines.h`) and at runtime with `DccConfig_set_railcom_cutout_timing()`. `DccConfig_cancel_railcom_cutout()` aborts an in-progress cutout and restores the bridge; `DccConfig_railcom_cutout_is_active()` reports state.

Decoded datagrams carry a 4-bit id (`DCC_RAILCOM_ID_*`: POM 0, ADR1 1, ADR2 2, EXT 3, DYN 7, XPOM 8–11, CV auto 12, time 14) and up to six data bytes. ACK (`0x0F` or `0xF0`) and NACK (`0x3C`) are code words, not datagrams; a datagram followed by ACK padding is kept. The 4/8 table follows the S-9.3.2 draft's Table 2 (the released 2012 table is identical for the 64 data words).

> **Known limitation.** Received bytes are split into channels by count: the first two are treated as Channel 1, the rest as Channel 2. A reply that contains only Channel 2 data (a decoder with the Channel 1 broadcast disabled through CV 28) is reported as a Channel 1 datagram. Tracked as an open issue; the bench suite keeps a deliberately failing case for it.

## 12. Callbacks — Where Your Application Lives

Command-station callbacks live in `dcc_config_t` and fire from `DccConfig_run()`.

| Callback | Fires when |
|---|---|
| `on_packet_sent(const dcc_packet_t *)` | The scheduler has dispatched a packet to the encoder (transmit start). The bench firmware uses it to pulse a scope trigger |
| `on_accessory_srq(address, is_extended)` | A RailCom accessory decoder raised a service request; answer with a stop packet to collect its update |
| `on_railcom_datagram_result(address, channel, datagram)` | In `dcc_railcom_hw_t`; a datagram was decoded |
| `on_complete` / `on_progress` / `on_detect` | Per service-mode call; see section 10 |

## 13. Application API Reference

All functions are in the three `dcc_application_command_station_*.h` headers. Builders fill a `dcc_packet_t` and return false on a bad argument; they do not send. Sending is a main-track call.

### 13.1 Main Track (`DccApplicationCommandStationMainTrack_`)

| Function | Description |
|---|---|
| `power_on()` / `power_off()` | Track power and DCC generation on the main track |
| `send_packet(packet, address, tag, priority)` | One-shot; sent `repeat_count` times |
| `add_to_auto_refresh(packet, address, tag, priority)` | Refreshed slot; replaces an existing (address, tag) |
| `remove_from_auto_refresh(address)` | Drop every refresh slot for an address |
| `remove_all_auto_refresh()` | Idle-only stream |

### 13.2 Packet Builders (`DccApplicationCommandStationPacket_load_`)

| Builder | Description |
|---|---|
| `idle`, `reset`, `estop_all(isPanic)` | Idle, broadcast reset, broadcast stop (panic = e-stop, else controlled stop) |
| `speed_128(addr, type, speed, dir)`, `speed_28(...)`, `speed_14(..., headlight)` | Speed and direction |
| `func_group_1`, `func_group_2a`, `func_group_2b` | FL/F1–F4, F5–F8, F9–F12 |
| `func_f13_f20` … `func_f61_f68` | Function expansion groups |
| `accessory_basic(board, pair, activate)`, `accessory_extended(addr, aspect)`, `accessory_nop(addr, is_extended)` | Turnouts, signals, RailCom polling |
| `accessory_basic_stop`, `accessory_extended_stop` | Stop (collect a RailCom service request) |
| `accessory_basic_cv_write/verify/bit`, `accessory_extended_cv_write/verify/bit` | Accessory operations-mode CV access |
| `cv_write_pom`, `cv_verify_pom`, `cv_bit_pom` | Loco operations-mode CV access |
| `consist_set(addr, type, consist, normal)`, `consist_clear` | Advanced consist |
| `binary_state_short`, `binary_state_long`, `analog_function` | Feature expansion |
| `system_time(ms)`, `model_time(...)`, `model_date(day, month, year)` | Broadcast time and date |

### 13.3 Service Track (`DccApplicationCommandStationServiceTrack_`)

`power_on/off`, `enter_service_mode`, `exit_service_mode`, `is_service_mode_active`, and the per-mode tasks listed in section 10.

## 14. Porting to a New MCU

1. Copy the `command_station` example project.
2. Rewrite `ti_driverlib_dcc_driver.c/h` for your timers, GPIO and current sense, meeting the contracts in section 7.
3. Rewrite or drop `ti_driverlib_uart_driver.c/h` (the CLI is optional).
4. Set the sizes in `dcc_user_config.h` for your RAM.
5. Wire your ISRs to `DccConfig_58us_timer_isr()`, `DccConfig_railcom_oneshot_timer_isr()` and `DccConfig_100ms_timer_tick()`.
6. Build, then verify on a logic analyzer: 58 µs halves, ≥ 100 µs zero halves, and a cutout that starts 26–32 µs after the end bit.

Minimum hardware: one periodic timer at 58 µs, one GPIO per track channel, a microsecond timestamp. Optional: a one-shot timer for RailCom, an ADC or comparator for ACK sensing, a 250 kbaud UART fed by a RailCom detector.

## 15. Unit Testing

Tests are GoogleTest C++ files beside the sources, `src/dcc/*_Test.cxx`, one binary per file. They run against mocked drivers, so a green host suite says the protocol logic is right, not that a board is wired right; the hardware-in-the-loop suite below covers the wire.

```
cd test
make            # configures CMake, builds, runs every binary serially, writes test/coverage.html
```

At generation time: 29 test binaries, 1144 tests, 0 failures, 0 warnings; line coverage 95.4 %, function coverage 97.7 %, branch coverage 87.8 % (gcovr). The build also compiles six single-role configurations so a missing `DCC_COMPILE_*` guard fails as a compile or link error.

| Test file | What it tests |
|---|---|
| `dcc_bit_encoder_Test` | Bit timing, preamble, framing, cutout armed at the end bit's last edge |
| `dcc_scheduler_Test` | Priority, duplicate combining, refresh pacing (burst, keep-alive, ceiling, fairness, flat-ring mode, CV 11 floor), 5 ms spacing, repeat counts untouched and overridden |
| `dcc_application_command_station_packet_Test` | Byte-exact vectors for every builder plus its repeat default |
| `dcc_application_command_station_main_track_Test`, `..._service_track_Test` | Application API |
| `dcc_service_mode_{direct,paged,register,address,common}_Test` | Per-mode primitives, ACK detection, failed-start handling |
| `dcc_service_mode_task_{direct,paged,register,address,detect}_Test` | Read/write/verify orchestration and detect |
| `dcc_railcom_cutout_Test`, `dcc_railcom_command_station_Test`, `dcc_railcom_utilities_Test` | Cutout state machine, receive and datagram assembly, 4/8 code words |
| `dcc_config_Test`, `dcc_failsafe_Test`, decoder-side tests | Wiring, lifecycle, ISR dispatch; the decoder role |

### 15.1 Hardware-in-the-Loop

`test/compliance/` holds a Saleae logic-analyzer bench that drives the shipped firmware over UART and checks the wire against the NMRA standards: S-9.1 timing, S-9.2 baseline packets, S-9.2.1 packet bytes and repeat counts, S-9.2.3 service mode with a mock-ACK loopback, S-9.3.2 cutout timing and sub-windows plus the receive path through a mock-decoder loopback, and the scheduler. A preflight script proves every probe and jumper before a run. See `test/compliance/command_station/HIL_SETUP.md`.

## 16. Troubleshooting

| Symptom | Likely cause and fix |
|---|---|
| No DCC on the scope | Track power is off; type `POWER ON`. Check `pin_toggle` wiring |
| Bit timing wrong | Shared timer period is not 58 µs; check the MCU clock tree |
| Decoder does not respond | H-bridge wiring, or the address in `SPEED` does not match the decoder |
| A one-shot command never appears | Its `repeat_count` is 0; the builders set defaults, an override to 0 means never send |
| `SVC RESULT: NO ACK` | No decoder on the programming track, `current_sense_read` not wired, or threshold too high |
| `SVC RESULT: BUSY` | Another service-mode operation is running; wait for its result |
| `ERR: scheduler full` | Raise `USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT` |
| `#error` about a `USER_DEFINED_*` | The constant is missing from `dcc_user_config.h`, or the file is not on the include path |
| RailCom datagrams missing or mislabeled | Receiver not gated to the windows (see 5.3), stale bytes not flushed, or the detector's UART is not 250 kbaud 8N1 |

Library source: github.com/JimKueneman/OpenDccCLib. NMRA DCC standards: www.nmra.org/dcc.
