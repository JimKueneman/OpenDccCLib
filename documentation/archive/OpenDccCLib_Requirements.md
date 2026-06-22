# OpenDccCLib — Library Requirements & Architecture

> **ARCHIVED / HISTORICAL — frozen original design intent.** Module and API names
> here predate the role-first naming refactor and no longer match the source.
> For the current as-built design see [../ARCHITECTURE.md](../ARCHITECTURE.md);
> for current status see [../STATUS.md](../STATUS.md).

## Context

Design a C library implementing NMRA DCC (Digital Command Control) specifications S-9.1 through S-9.3.2. The library provides a simple user-facing API on the front end and generates spec-compliant DCC signals on the back end. It follows the same architectural patterns as OpenLcbCLib: single config struct, DI via function pointers, compile-time feature flags, no dynamic memory, portable to any MCU.

**User priorities:**
- Buffering system for outbound messages
- Duplicate message combining (coalesce repeated commands to same address)
- Auto-refresh of key control messages (speed/function packets per NMRA requirements)
- Simple user-facing public API
- Easily adaptable backend driver system (any MCU, precision hardware timers)
- Extensive DI for high test coverage and easy MCU swapping

---

## 1. Feature Flags (`DCC_COMPILE_*`)

Defined by user in `dcc_user_config.h`. Validated in `dcc_config.h` with `#error` directives.

Unlike OpenLcbCLib (which needed fine-grained flags to minimize code on tiny bootloader MCUs), this library targets high-end processors with plenty of code space. Only **2 role-level flags** exist — everything within a role (including RailCom and service mode) compiles in unconditionally.

| Flag | Includes |
|------|----------|
| `DCC_COMPILE_COMMAND_STATION` | Packet encoding (all speed modes, functions F0-F68, accessories, CV ops-mode, consisting), bit framing, scheduler (refresh, priority, duplicate combining), RailCom cutout & receive |
| `DCC_COMPILE_DECODER` | Packet decoding (all packet types), address matching, CV storage, decoder lock, factory reset, fail-safe, RailCom response encoding |
| `DCC_COMPILE_SERVICE_MODE_DIRECT` | Direct byte write/verify + direct bit manipulation -- the modern preferred mode |
| `DCC_COMPILE_SERVICE_MODE_PAGED` | Paged mode -- uses register 6 as page pointer, older but still common |
| `DCC_COMPILE_SERVICE_MODE_REGISTER` | Physical register mode -- legacy, addresses registers 1-8 only |
| `DCC_COMPILE_SERVICE_MODE_ADDRESS` | Address-only mode -- minimal, writes only CV 1 (primary address) |

Service mode flags all require `DCC_COMPILE_COMMAND_STATION`. Each service mode is a separate module with its own state machine, but they share common ACK detection and reset packet sequencing via a shared internal module (`dcc_service_mode_common`).

**Optional hardware is handled at runtime, not compile time.** RailCom code is always compiled in with its parent role. If the hardware isn't present, the user sets the corresponding callback to NULL:
- `railcom_cutout_begin = NULL` -> bit encoder skips cutout window
- `railcom_uart_read = NULL` -> no RailCom receive processing
- `current_sense_read = NULL` -> all service mode modules return error (even if compiled in)

**Validation:**
```c
#if !defined(DCC_COMPILE_COMMAND_STATION) && !defined(DCC_COMPILE_DECODER)
#warning "No role enabled. Define DCC_COMPILE_COMMAND_STATION and/or DCC_COMPILE_DECODER."
#endif

#if defined(DCC_COMPILE_SERVICE_MODE_DIRECT) && !defined(DCC_COMPILE_COMMAND_STATION)
#error "DCC_COMPILE_SERVICE_MODE_DIRECT requires DCC_COMPILE_COMMAND_STATION"
#endif
/* same for PAGED, REGISTER, ADDRESS */
```

A device can define both CS and DECODER flags for booster/repeater applications.

---

## 2. Types (`dcc_types.h`)

Includes `dcc_user_config.h`. Validates all `USER_DEFINED_DCC_*` constants. Defines:

### Address
```c
typedef uint16_t dcc_address_t;  // 0-10239

typedef enum {
    DCC_ADDRESS_SHORT,              // 7-bit, 1-127
    DCC_ADDRESS_LONG,               // 14-bit, 128-10239
    DCC_ADDRESS_BROADCAST,          // 0
    DCC_ADDRESS_IDLE,               // 255
    DCC_ADDRESS_ACCESSORY,          // 9-bit basic accessory
    DCC_ADDRESS_ACCESSORY_EXTENDED  // 11-bit signal
} dcc_address_type_enum;
```

### Speed
```c
typedef enum {
    DCC_SPEED_MODE_14,
    DCC_SPEED_MODE_28,
    DCC_SPEED_MODE_128
} dcc_speed_mode_enum;
```

### Packet Buffer
```c
#define DCC_PACKET_MAX_BYTES 6  // 2 addr + 3 instruction + 1 XOR

typedef struct {
    uint8_t data[DCC_PACKET_MAX_BYTES];
    uint8_t byte_count;
    uint8_t preamble_bits;   // 14 ops, 20 service mode
    uint8_t repeat_count;    // times to send before dropping
} dcc_packet_t;
```

### Scheduler Slot (with duplicate combining & auto-refresh)
```c
typedef enum {
    DCC_PRIORITY_ESTOP,
    DCC_PRIORITY_SPEED,
    DCC_PRIORITY_FUNCTION,
    DCC_PRIORITY_ACCESSORY,
    DCC_PRIORITY_CV,
    DCC_PRIORITY_IDLE
} dcc_priority_enum;

typedef struct {
    dcc_packet_t packet;
    dcc_priority_enum priority;
    dcc_address_t address;
    uint8_t tag;              // sub-key for combining (e.g., function group #)
    bool auto_refresh;        // true = keep in refresh cycle indefinitely
    bool active;
} dcc_scheduler_slot_t;
```

When a new command arrives for an (address, tag) pair that already has an active slot, the scheduler **overwrites** the existing slot's packet data rather than allocating a new slot. This is duplicate combining.

### User-Configurable Constants (defined in `dcc_user_config.h`, validated in `dcc_types.h`)

```c
// Role selection
#define DCC_COMPILE_COMMAND_STATION
// #define DCC_COMPILE_DECODER

// Service mode selection (requires CS)
#define DCC_COMPILE_SERVICE_MODE_DIRECT
#define DCC_COMPILE_SERVICE_MODE_PAGED
// #define DCC_COMPILE_SERVICE_MODE_REGISTER
// #define DCC_COMPILE_SERVICE_MODE_ADDRESS

// CS buffer & pool sizes
#define USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT    16  // max concurrent active packets
#define USER_DEFINED_DCC_MAX_LOCOS                8  // max locos with auto-refresh
#define USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH     4  // RailCom receive ring buffer

// Service mode tuning
#define USER_DEFINED_DCC_SERVICE_MODE_RETRIES     3
#define USER_DEFINED_DCC_ACK_THRESHOLD_MA        60
#define USER_DEFINED_DCC_ACK_MIN_DURATION_US   5000
#define USER_DEFINED_DCC_ACK_MAX_DURATION_US   7000

// Decoder config
#define USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS   29  // F0-F28
```

No `dcc_user_config.c` is needed -- all configuration is compile-time constants and feature flags. Factory reset defaults are the user's responsibility via their `cv_write` callback or a `factory_reset` callback in `dcc_config_t`.

### Additional types (gated by feature flags)
- `dcc_service_mode_result_t` -- success/no-ack/verify-fail/busy/error
- `dcc_program_mode_enum` -- direct byte/bit, paged, address-only
- `dcc_railcom_datagram_t` -- datagram_id, data[6], count, valid
- `dcc_railcom_channel_enum` -- CH1, CH2
- `dcc_bit_state_enum` -- idle/preamble/start/data/xor/end/railcom_cutout
- `dcc_decoded_packet_t` -- address, type, instruction bytes, valid flag

---

## 3. The `dcc_config_t` Struct

Single user-facing config struct. User populates it, passes to `DccConfig_initialize()`.

### REQUIRED: Common (always present)
- `void (*lock_shared_resources)(void)` -- disable IRQ / acquire mutex. REQUIRED.
- `void (*unlock_shared_resources)(void)` -- enable IRQ / release mutex. REQUIRED.
- `uint32_t (*get_timestamp_usec)(void)` -- microsecond timestamp. REQUIRED.

### Command Station (`DCC_COMPILE_COMMAND_STATION`)

**REQUIRED hardware drivers:**
- `void (*timer_start)(uint16_t half_bit_period_usec)` -- start DCC bit timer. User configures their timer in output compare toggle mode so the track signal pin toggles automatically on compare match with zero jitter. Timer must call `DccConfig_timer_half_bit_isr()` on compare match interrupt.
- `void (*timer_set_period)(uint16_t half_bit_period_usec)` -- change timer compare value for next half-bit period (ISR context, single register write)
- `void (*timer_stop)(void)` -- stop timer
- `void (*track_power_set)(bool enabled)` -- enable/disable track power

**Debug options:**
- `bool suppress_idle_packets` -- when true, bit encoder stops transmitting when scheduler is empty instead of sending idle packets. Only real API-generated packets appear on the wire.

**NULL-optional hardware drivers (feature disabled at runtime if NULL):**
- `void (*railcom_cutout_begin)(void)` -- tristate H-bridge for RailCom cutout. NULL = no cutout.
- `void (*railcom_cutout_end)(void)` -- resume H-bridge after cutout. NULL = no cutout.
- `bool (*railcom_uart_read)(uint8_t *byte)` -- read RailCom UART. NULL = no RailCom receive.
- `uint16_t (*current_sense_read)(void)` -- ADC current for service mode ACK. NULL = service mode API returns error.

**OPTIONAL application callbacks (NULL = no notification):**
- `void (*on_idle)(void)` -- scheduler has nothing to send
- `void (*on_packet_sent)(const dcc_packet_t *packet)` -- packet fully transmitted
- `void (*on_service_mode_complete)(dcc_service_mode_result_t result)` -- programming op finished
- `void (*on_railcom_datagram)(uint16_t address, uint8_t channel, const dcc_railcom_datagram_t *datagram)` -- fired from `DccConfig_run()`, NOT ISR

### Decoder (`DCC_COMPILE_DECODER`)

**REQUIRED hardware drivers:**
- `bool (*cv_read)(uint16_t cv_number, uint8_t *value)` -- read CV from persistent storage
- `bool (*cv_write)(uint16_t cv_number, uint8_t value)` -- write CV to persistent storage

**NULL-optional hardware drivers:**
- `void (*railcom_uart_write)(uint8_t byte)` -- transmit 4/8 encoded RailCom response. NULL = no RailCom responses.

**OPTIONAL application callbacks (NULL = no notification):**
- `void (*on_speed_command)(uint16_t address, uint8_t speed, bool direction, dcc_speed_mode_enum mode)`
- `void (*on_emergency_stop)(uint16_t address)`
- `void (*on_function_command)(uint16_t address, uint8_t function_number, bool state)`
- `void (*on_accessory_basic_command)(uint16_t board_address, uint8_t output_pair, bool activate)`
- `void (*on_accessory_extended_command)(uint16_t address, uint8_t aspect)`
- `void (*on_cv_write)(uint16_t cv_number, uint8_t value)`
- `void (*on_failsafe_entered)(void)` / `void (*on_failsafe_exited)(void)`

---

## 4. User-Facing API

Split across `dcc_config.h` (lifecycle/ISR only) and `dcc_application_*.h` modules (user commands). Following the OpenLcbCLib `application_` naming convention for all user-facing modules.

### `dcc_config.h` -- Lifecycle & ISR Entry Points Only
```c
void DccConfig_initialize(const dcc_config_t *config);
void DccConfig_run(void);                     // call in main loop
void DccConfig_timer_half_bit_isr(void);      // call from timer ISR
void DccConfig_100ms_timer_tick(void);        // call from 100ms timer
void DccConfig_track_power_on(void);
void DccConfig_track_power_off(void);
void DccConfig_decoder_edge(uint32_t timestamp_usec);  // call from input-capture ISR
```

### `dcc_application_loco.h/.c` -- Locomotive Control
```c
bool DccApplicationLoco_set_speed(dcc_address_t addr, dcc_address_type_enum type,
                                   uint8_t speed, bool direction, dcc_speed_mode_enum mode);
bool DccApplicationLoco_set_function(dcc_address_t addr, dcc_address_type_enum type,
                                      uint8_t function_number, bool state);
bool DccApplicationLoco_emergency_stop(dcc_address_t addr, dcc_address_type_enum type);
void DccApplicationLoco_emergency_stop_all(void);
```

### `dcc_application_accessory.h/.c` -- Accessory Control
```c
bool DccApplicationAccessory_set_basic(uint16_t board_addr, uint8_t output_pair, bool activate);
bool DccApplicationAccessory_set_extended(uint16_t address, uint8_t aspect);
```

### `dcc_application_cv.h/.c` -- CV Programming & Helpers
```c
// Ops-mode CV access (programming on the main)
bool DccApplicationCv_write_ops_mode(dcc_address_t addr, dcc_address_type_enum type,
                                      uint16_t cv_number, uint8_t value);
bool DccApplicationCv_bit_ops_mode(dcc_address_t addr, dcc_address_type_enum type,
                                    uint16_t cv_number, uint8_t bit_pos, bool bit_val, bool write);

// Service mode common (requires current_sense_read != NULL)
bool DccApplicationCv_service_mode_enter(void);
bool DccApplicationCv_service_mode_exit(void);

// Direct mode [DCC_COMPILE_SERVICE_MODE_DIRECT]
bool DccApplicationCv_direct_write_byte(uint16_t cv_number, uint8_t value);
bool DccApplicationCv_direct_verify_byte(uint16_t cv_number, uint8_t value);
bool DccApplicationCv_direct_write_bit(uint16_t cv_number, uint8_t bit_pos, bool bit_val);
bool DccApplicationCv_direct_verify_bit(uint16_t cv_number, uint8_t bit_pos, bool bit_val);

// Paged mode [DCC_COMPILE_SERVICE_MODE_PAGED]
bool DccApplicationCv_paged_write(uint16_t cv_number, uint8_t value);
bool DccApplicationCv_paged_verify(uint16_t cv_number, uint8_t value);

// Register mode [DCC_COMPILE_SERVICE_MODE_REGISTER]
bool DccApplicationCv_register_write(uint8_t register_number, uint8_t value);
bool DccApplicationCv_register_verify(uint8_t register_number, uint8_t value);

// Address-only mode [DCC_COMPILE_SERVICE_MODE_ADDRESS]
bool DccApplicationCv_address_write(uint8_t address);

// CV 29 helpers
bool DccApplicationCv_cv29_is_direction_reversed(uint8_t cv29);
bool DccApplicationCv_cv29_is_28_128_speed_steps(uint8_t cv29);
bool DccApplicationCv_cv29_is_analog_enabled(uint8_t cv29);
bool DccApplicationCv_cv29_is_railcom_enabled(uint8_t cv29);
bool DccApplicationCv_cv29_is_speed_table_enabled(uint8_t cv29);
bool DccApplicationCv_cv29_is_extended_address(uint8_t cv29);
uint8_t DccApplicationCv_cv29_build(bool dir_reversed, bool speed_28_128, bool analog,
                                     bool railcom, bool speed_table, bool extended_addr);

// CV 28 helpers
bool DccApplicationCv_cv28_is_ch1_enabled(uint8_t cv28);
bool DccApplicationCv_cv28_is_ch2_enabled(uint8_t cv28);
bool DccApplicationCv_cv28_is_unsolicited_enabled(uint8_t cv28);
uint8_t DccApplicationCv_cv28_build(bool ch1, bool ch2, bool unsolicited);
```

### `dcc_application_consist.h/.c` -- Consisting
```c
bool DccApplicationConsist_set_address(dcc_address_t addr, dcc_address_type_enum type,
                                        uint8_t consist_addr, bool direction_normal);
bool DccApplicationConsist_clear_address(dcc_address_t addr, dcc_address_type_enum type);
```

### `dcc_application_railcom.h/.c` -- RailCom Read
```c
bool DccApplicationRailcom_read(dcc_railcom_datagram_t *out);
uint8_t DccApplicationRailcom_available(void);
```
User can choose either:
- **Callback** (`on_railcom_datagram`) -- fired automatically from `DccConfig_run()`
- **Polling** (`DccApplicationRailcom_read`) -- user pulls when ready
- **Both** -- callback fires AND datagram stays readable until polled

---

## 5. Module Breakdown (all in `src/dcc/`)

Each module has a `.h` with its interface struct typedef + extern function declarations, and a `.c` with implementation. **Only `dcc_config.c` includes all module headers** -- it is the wiring module.

| Module | Compiled With | Responsibility |
|--------|--------------|----------------|
| `dcc_config.h/.c` | Always | Config struct, user API, feature validation, wiring of all interface structs, `_initialize()` / `_run()` |
| `dcc_types.h` | Always | All typedefs, user config validation, user-configurable constants |
| `dcc_defines.h` | Always | Protocol constants: timing (58us, 100us), instruction masks, CV numbers, RailCom IDs |
| `dcc_packet_encoder.h/.c` | CS | High-level: "set speed for addr 3" -> `dcc_packet_t` byte array with XOR. All speed modes, functions, accessories, CV ops, consisting. |
| `dcc_bit_encoder.h/.c` | CS | ISR-level: `dcc_packet_t` -> preamble + byte framing + end bit on the wire. Drives timer/signal. Double-buffered with scheduler. |
| `dcc_scheduler.h/.c` | CS | **Refresh cycle, priority queue, duplicate combining, auto-refresh.** Round-robin through active slots. Overwrites existing (address, tag) slots on duplicate. E-stop > speed > function > accessory > CV > idle. |
| `dcc_service_mode_common.h/.c` | CS (shared) | ACK detection, reset packet sequencing, retry logic -- shared by all service mode modules. Disabled at runtime if `current_sense_read = NULL`. |
| `dcc_service_mode_direct.h/.c` | `SERVICE_MODE_DIRECT` | Direct byte write/verify + direct bit manipulation |
| `dcc_service_mode_paged.h/.c` | `SERVICE_MODE_PAGED` | Paged mode via register 6 page pointer |
| `dcc_service_mode_register.h/.c` | `SERVICE_MODE_REGISTER` | Physical register mode (legacy, registers 1-8) |
| `dcc_service_mode_address.h/.c` | `SERVICE_MODE_ADDRESS` | Address-only mode (writes CV 1 only) |
| `dcc_railcom_decoder.h/.c` | CS | 4/8 decoding, cutout timing state machine, receive buffer. Disabled at runtime if `railcom_cutout_begin = NULL`. |
| `dcc_bit_decoder.h/.c` | DECODER | Edge timestamp -> bit classification (one/zero) using spec timing thresholds. Handles asymmetric zeros. Accumulates bits into bytes, detects preamble, reconstructs raw packets. |
| `dcc_packet_decoder.h/.c` | DECODER | Parse reconstructed byte array -> structured data. XOR validation. Address matching. Dispatch to callbacks. All packet types. |
| `dcc_cv_storage.h/.c` | DECODER | CV read/write via user callbacks. Decoder lock (CV 15/16). Factory reset (CV 8). Consist address (CV 19). Fail-safe (CV 11/13/14). |
| `dcc_railcom_encoder.h/.c` | DECODER | 4/8 bit encoding, Ch1/Ch2 datagram construction. Disabled at runtime if `railcom_uart_write = NULL`. |

### Removed modules
- `src/drivers/canbus/` -- removed, DCC has no CAN requirement
- `src/utilities/` -- removed, no generic helpers needed
- `src/utilities_pc/` -- kept for PC-specific test helpers (threading)

---

## 6. Interface Structs (DI for Cross-Module Communication)

Every module receives its dependencies through a `interface_dcc_<module>_t` struct of function pointers, populated by `dcc_config.c`. No module directly `#include`s another module's header. This enables:
- Complete mockability in tests (swap any function pointer for a mock)
- Easy MCU swapping (change hardware callbacks, nothing else)
- High test coverage without hardware

### Key Interface Structs

**`interface_dcc_bit_encoder_t`** -- hardware timer/signal + callback to scheduler on packet complete
```
timer_start, timer_set_period, timer_stop, track_signal_set, on_packet_complete,
railcom_cutout_begin, railcom_cutout_end  (NULL-checked at runtime)
```

**`interface_dcc_scheduler_t`** -- bit encoder entry point + user callbacks + timestamp
```
encode_packet, on_packet_sent, on_idle, get_timestamp_usec
```

**`interface_dcc_packet_decoder_t`** -- CV read + all decoder application callbacks
```
cv_read, on_speed_command, on_emergency_stop, on_function_command,
on_accessory_basic_command, on_accessory_extended_command, on_cv_write,
on_failsafe_entered, on_failsafe_exited
```

**`interface_dcc_service_mode_t`** -- packet output + current sense + completion callback
```
encode_packet, current_sense_read, get_timestamp_usec, on_complete
```

**`interface_dcc_cv_storage_t`** -- user-provided CV read/write + fail-safe
```
cv_read, cv_write
```

**`interface_dcc_railcom_decoder_t`** -- UART read + datagram callback
```
railcom_uart_read, on_railcom_datagram
```

**`interface_dcc_railcom_encoder_t`** -- UART write for decoder responses
```
railcom_uart_write
```

---

## 7. Scheduler Design (Buffering, Combining, Auto-Refresh)

The scheduler is the heart of the command station. Key behaviors:

### Slot Array
- Static array of `dcc_scheduler_slot_t` sized by `USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT`
- Each slot holds one `dcc_packet_t` plus metadata (address, tag, priority, auto_refresh)

### Duplicate Combining
- When user calls `DccApplicationLoco_set_speed(addr=3, speed=50)` and there's already an active slot for (address=3, tag=SPEED), the scheduler **overwrites** that slot's packet data with the new speed value
- No new slot allocated -- the latest command always wins
- Tag values: SPEED, FUNC_GROUP_1, FUNC_GROUP_2A, FUNC_GROUP_2B, FUNC_EXPANSION_xx, ACCESSORY, CV

### Auto-Refresh
- Per NMRA specs, command stations must continuously repeat speed and function packets
- When `DccApplicationLoco_set_speed()` is called, the slot is marked `auto_refresh = true`
- The scheduler round-robins through all auto-refresh slots, interleaving with one-shot packets
- Speed packets refreshed most frequently; function packets less often
- Idle packets fill gaps when no other packets are pending

### Priority
- E-stop packets jump to front of queue immediately
- One-shot packets (CV writes, accessory commands) are sent once with higher urgency, then removed
- Refresh packets cycle continuously

### Double Buffering with Bit Encoder
- Scheduler prepares next packet in a "ready" buffer while bit encoder transmits from "active" buffer
- On `on_packet_complete` callback from bit encoder, buffers swap -- zero dead time between packets
- `lock_shared_resources` / `unlock_shared_resources` protects the swap point

### Minimum Inter-Packet Timing
- Scheduler ensures >= 5ms between end of one packet and start of next (via timestamp checking)

---

## 8. RailCom Receive Buffer Design

The command station must receive bi-directional data from decoders during the RailCom cutout window (S-9.3.2).

### Buffer Structure
- Static circular buffer of `dcc_railcom_datagram_t` entries
- Sized by `USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH` (e.g., 4)
- ISR writes, main loop reads -> classic producer/consumer with `lock/unlock` at the handoff

### Data Flow
1. **ISR context** (during cutout window): `dcc_railcom_decoder` reads raw bytes from UART via `railcom_uart_read()`, decodes 4/8 encoding, tags each datagram with the DCC address of the packet that preceded the cutout
2. **ISR -> Buffer**: Complete decoded `dcc_railcom_datagram_t` pushed into circular buffer (protected by `lock/unlock`)
3. **Main loop** (`DccConfig_run()`): Drains buffer, fires `on_railcom_datagram` callback for each entry
4. **Polling alternative**: User can call `DccApplicationRailcom_read()` / `DccApplicationRailcom_available()` instead of or in addition to the callback

### Address Correlation
- The bit encoder tracks which DCC address was in the packet it just finished transmitting
- When the cutout begins, that address is passed to the RailCom decoder so received datagrams can be tagged with the correct source address
- This is how the command station knows which decoder responded

---

## 9. Execution Contexts & Boundaries

### ISR Context (timer compare match -- fires every 58-100 us)
- `DccBitEncoder_half_bit_isr()` -- toggle track signal pin, advance bit state machine
- RailCom `cutout_begin` / `cutout_end` -- called from bit encoder at precise timing after packet end
- **Must complete in < 30 us** -- only pin writes, pointer reads, counter decrements
- Reads from the "active" packet buffer (read-only in ISR)

### Main Loop Context (`DccConfig_run()`)
- **Scheduler**: selects next packet, duplicate combining, refresh cycle management
- **Packet Encoder**: builds `dcc_packet_t` byte arrays from user commands
- **Service Mode**: state machine, polls `current_sense_read()` for ACK, reset sequencing
- **RailCom Decoder**: drains UART buffer via `railcom_uart_read()`, decodes 4/8, fires `on_railcom_datagram`
- **Decoder side**: `dcc_packet_decoder` processes accumulated bits, dispatches callbacks
- **Fail-safe**: checks packet timeout timer
- All application callbacks fire from this context (never from ISR)

### User API Context (any thread)
- `DccApplicationLoco_set_speed()`, `DccApplicationLoco_set_function()`, etc. write into scheduler slots
- If called from a different thread than `DccConfig_run()`, must be protected by `lock/unlock`
- On bare-metal single-threaded systems, these are called from main loop (no lock needed)

### Boundary Handoffs (3 critical points)

```
 USER API             MAIN LOOP              ISR (timer)
 --------             ---------              -----------
     |                    |                      |
     |  set_speed()       |                      |
     |------------------>|                      |
     |  (lock/unlock      |                      |
     |   if threaded)     |                      |
     |                    |  scheduler writes     |
     |                    |  next packet into     |
     |                    |  "ready" buffer       |
     |                    |                      |
     |                    |      BOUNDARY 1:      |
     |                    | <--- packet_complete  |
     |                    |      (ISR sets flag,  |
     |                    |       main loop       |
     |                    |       detects it,     |
     |                    |       swaps buffers   |
     |                    |       under lock)     |
     |                    |                      |
     |                    |      BOUNDARY 2:      |
     |                    |      railcom_uart_    |
     |                    | <--- read() polled    |
     |                    |      from main loop   |
     |                    |      (reads HW FIFO)  |
     |                    |                      |
     |  on_railcom_       |      BOUNDARY 3:      |
     |  datagram()  <-----|      all callbacks    |
     |  on_speed()  <-----|      fire from main   |
     |  etc.              |      loop context      |
```

**Boundary 1 -- Buffer Swap** is the only point requiring `lock/unlock`:
- ISR sets a `packet_complete` flag (atomic write)
- Main loop detects flag -> calls `lock()` -> swaps active/ready buffer pointers -> calls `unlock()`
- ISR never writes to the buffer; it only reads from "active" and sets a flag when done

**Boundary 2 -- RailCom UART** is polled, not interrupt-driven from the library's perspective:
- User's UART driver buffers incoming bytes (hardware FIFO or ring buffer)
- Main loop calls `railcom_uart_read()` to drain them -- no lock needed (user's driver handles its own ISR)

**Boundary 3 -- Callbacks** are always main-loop context:
- User code in callbacks can safely access shared state, call other APIs, etc.

---

## 10. Bit Encoder Design (ISR-Level)

Runs entirely in timer ISR context. State machine:

1. **PREAMBLE** -- output one-bits (58us halves), count down preamble_bits
2. **START_BIT** -- output one zero-bit (100us halves)
3. **DATA_BIT** -- output 8 bits of current byte, MSB first (58us for 1, 100us for 0)
4. **Separator** -- zero-bit between bytes, or...
5. **END_BIT** -- one-bit after XOR byte -> packet complete
6. **RAILCOM_CUTOUT** -- (if enabled) tristate H-bridge for cutout window, then resume

The timer compare value toggles between 58us and 100us depending on bit value. Each timer match toggles the track signal polarity AND advances the state machine.

---

## 11. File Tree

```
src/dcc/
  dcc_config.h              -- dcc_config_t, user API, feature flag validation
  dcc_config.c              -- wiring: build interface structs, initialize all modules
  dcc_types.h               -- all typedefs, user constant validation
  dcc_defines.h             -- protocol constants, timing, masks
  dcc_packet_encoder.h/.c   -- [CS] address encoding, instruction building, XOR (all packet types)
  dcc_bit_encoder.h/.c      -- [CS] ISR state machine, bit timing, double buffer
  dcc_scheduler.h/.c        -- [CS] refresh, priority, combining, auto-refresh
  dcc_service_mode_common.h/.c  -- [CS] shared ACK detection, reset sequences, retry logic
  dcc_service_mode_direct.h/.c  -- [SERVICE_MODE_DIRECT] direct byte/bit write/verify
  dcc_service_mode_paged.h/.c   -- [SERVICE_MODE_PAGED] paged mode via register 6
  dcc_service_mode_register.h/.c -- [SERVICE_MODE_REGISTER] physical register mode (legacy)
  dcc_service_mode_address.h/.c -- [SERVICE_MODE_ADDRESS] address-only mode
  dcc_railcom_decoder.h/.c  -- [CS] 4/8 decode, cutout timing, receive buffer (runtime-optional)
  dcc_bit_decoder.h/.c      -- [DECODER] edge->bit classification, preamble detection, byte assembly
  dcc_packet_decoder.h/.c   -- [DECODER] parse bytes->structured data, XOR, address match, dispatch
  dcc_cv_storage.h/.c       -- [DECODER] CV abstraction, decoder lock, factory reset, fail-safe CVs
  dcc_railcom_encoder.h/.c       -- [DECODER] 4/8 encode, Ch1/Ch2 datagrams (runtime-optional)
  dcc_application_loco.h/.c      -- [USER] locomotive speed, function, e-stop
  dcc_application_accessory.h/.c -- [USER] basic and extended accessory control
  dcc_application_cv.h/.c        -- [USER] CV programming (ops + service mode) + CV 29/28 helpers
  dcc_application_consist.h/.c   -- [USER] consist address management
  dcc_application_railcom.h/.c   -- [USER] RailCom polling read API
  CMakeLists.txt
```

---

## 12. Implementation Phases

### Phase 1 -- Foundation
1. `dcc_types.h` -- all types, user config validation
2. `dcc_defines.h` -- protocol constants
3. `dcc_config.h` -- 2 feature flags, `dcc_config_t` struct
4. `dcc_config.c` -- skeleton wiring

### Phase 2 -- Command Station Core
5. `dcc_packet_encoder` -- idle, reset, 128-step speed, XOR
6. `dcc_bit_encoder` -- preamble, byte framing, ISR state machine
7. `dcc_scheduler` -- round-robin, double buffer, duplicate combining, auto-refresh

### Phase 3 -- Expand Command Station Packet Encoder
8. 14/28-step speed modes
9. Function groups (Group 1, Group 2, feature expansion F13-F68)
10. Accessory packets (basic + extended)
11. Ops-mode CV access (long form write/verify/bit)
12. Consisting (set/clear consist address)
13. Binary state, analog function, speed restriction, time/date, system time

### Phase 4 -- Service Mode & RailCom (CS side)
14. `dcc_service_mode_common` -- ACK detection, reset sequences, retry logic
15. `dcc_service_mode_direct` -- direct byte/bit (the modern preferred mode, implement first)
16. `dcc_service_mode_paged` -- paged mode
17. `dcc_service_mode_register` -- physical register mode (legacy)
18. `dcc_service_mode_address` -- address-only mode (minimal)
19. `dcc_railcom_decoder` -- 4/8 decoding, cutout state machine, receive buffer

### Phase 5 -- Decoder Side
20. `dcc_bit_decoder` -- edge->bit classification, preamble detection, byte assembly
21. `dcc_packet_decoder` -- parse all packet types, validate, address match, dispatch
22. `dcc_cv_storage` -- CV read/write, decoder lock, factory reset, fail-safe, consist CVs
23. `dcc_railcom_encoder` -- 4/8 encoding, Ch1/Ch2 datagram construction

---

## 13. Key Architectural Decisions

1. **2 compile flags, runtime NULL for optional hardware**: `DCC_COMPILE_COMMAND_STATION` and `DCC_COMPILE_DECODER` only. RailCom, service mode, etc. are always compiled but disabled at runtime if hardware callbacks are NULL.
2. **ISR safety**: Bit encoder is ISR-only. Double-buffered handoff with scheduler. `lock/unlock` protects swap.
3. **No dynamic memory**: All arrays statically sized by `USER_DEFINED_DCC_*` constants.
4. **Extensive DI**: Every module receives dependencies through interface structs. Only `dcc_config.c` knows about all modules. Everything is mockable for tests. MCU swap = change the config struct callbacks, nothing else.
5. **Dual-role**: A device can be both CS and DECODER (booster/repeater) by defining both flags. Modules are independent.
6. **Timer abstraction**: Library never touches hardware directly. User provides `timer_start`/`set_period`/`stop` and calls `DccConfig_timer_half_bit_isr()` from their ISR. Works on any MCU with a precision timer.
7. **CV numbering**: 1-based (matching NMRA). 0-based wire encoding handled only inside `dcc_packet_encoder.c`.
8. **Duplicate combining**: Scheduler uses (address, tag) as a composite key. New command for same key overwrites in-place.
9. **Auto-refresh**: Speed and function slots are persistent. Library handles the NMRA-required repetition automatically. User just calls `set_speed()` once and the library keeps refreshing.
10. **Auto-idle**: When the scheduler has no packets queued, the library automatically sends idle packets (`0xFF 0x00 0xFF`) to keep a valid DCC signal on the track. The `on_idle` callback fires as a notification but the library handles it regardless.
11. **Decoder bit classification**: The library classifies one/zero bits internally from raw edge timestamps (`DccConfig_decoder_edge(timestamp_usec)`). User ISR is trivial: read timer on pin change, call library. Spec timing thresholds (including asymmetric zero) implemented once correctly in the library.

---

## 14. Spec Compliance Notes

Items validated against NMRA S-9.x specs that must be correctly implemented:

### Bit Timing (`dcc_defines.h`)
- One-bit half period: 58 us nominal (55-61 us acceptable)
- Zero-bit half period: >= 100 us (command station); decoder accepts >= 90 us
- **Zero-bit max total duration: 12000 us** -- bit encoder must enforce
- **Asymmetric zero**: first half may be stretched; decoder must accept this
- Preamble: CS sends >= 14 one-bits (>= 20 for service mode); **decoder accepts >= 10**

### Accessory Address Encoding (`dcc_packet_encoder`)
- Basic accessory: 9-bit address split across 2 bytes
- **High 3 address bits in byte 2 are INVERTED** (`1AAACDDD` where AAA = ~high bits)
- Extended accessory: 11-bit address, different encoding (`0AAA0AA1`)
- Broadcast accessory: `10111111` for both basic and extended

### CV Handling (`dcc_cv_storage`)
- CV numbers 1-based (wire encoding is 0-based, handled in packet encoder only)
- **Indexed CVs**: CV 31 (high) + CV 32 (low) form page pointer for CVs 257-512
- **Accessory decoder CVs**: CV 513 (address LSB), 519 (config), 521 (mfr version), 540 (bi-directional), 541 (address MSB)
- CV 33-46: function mapping outputs
- CV 67-94: speed table (28 entries)
- CV 105-106: user ID bytes

### Packet Types (`dcc_packet_encoder`)
All packet types to implement (S-9.2 + S-9.2.1 + S-9.2.1.1):
- Idle, reset, broadcast stop/e-stop
- Speed: 14-step, 28-step, 128-step
- Functions: Group 1 (FL,F1-F4), Group 2a (F5-F8), Group 2b (F9-F12), expansion (F13-F68)
- Accessory: basic (9-bit + output pair), extended (11-bit + aspect), broadcast
- CV ops-mode: long form (write/verify/bit), short form
- Consisting: set/clear consist address
- Binary state control: short form (127 states), long form (32767 states)
- Analog function control
- Speed restriction (advanced operations sub-instruction)
- **Time/Date and System Time** -- broadcast packets for clock display decoders

### RailCom Details (`dcc_railcom_decoder` / `dcc_railcom_encoder`)
- **Channel 1**: 2 bytes, 12-bit encoding (start + 6 data + stop per byte), window ~464 us, starts 88 us after packet end
- **Channel 2**: up to 6 bytes, same encoding, total window ~1544 us from cutout start
- **4/8 encoding**: each 4-bit nibble -> 8-bit byte via lookup table (defined in S-9.3.2)
- **Datagram IDs**: 0-2 = mobility/address feedback, 7-8 = CV readback, 15 = DCC ack
- **CV 28 bits**: bit 0 = Ch1 enable, bit 1 = Ch2 enable, bit 2 = unsolicited datagram expansion
- **CV 29 bit 3** must also be set to globally enable RailCom

---

## 15. Verification

After each phase:
```bash
cd /Users/jimkueneman/Documents/OpenDccCLib/test
make    # must build clean, all tests pass, coverage report generated
```

Test strategy per module:
- Mock all interface struct function pointers
- Verify packet byte arrays against hand-calculated expected values from the spec
- Verify scheduler combining by feeding duplicate commands and checking slot state
- Verify bit encoder state machine transitions
- Verify decoder parsing round-trips (encode -> decode -> compare)
