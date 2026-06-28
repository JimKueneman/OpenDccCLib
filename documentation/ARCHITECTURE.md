# OpenDccCLib — Architecture (As-Built)

> **Verified against commit `c4dc61e` (2026-04-21).**
> This document describes the library **as it is implemented today**. For the
> original design intent (which predates the role-first naming refactor), see
> [archive/OpenDccCLib_Requirements.md](archive/OpenDccCLib_Requirements.md).
> For what is done vs. pending, see [ComplianceOverview.md](compliance/ComplianceOverview.md).

## 1. Overview

OpenDccCLib is a portable C library implementing NMRA DCC (S-9.1 through S-9.3.2)
for both command-station and decoder roles. Design constraints:

- **No dynamic memory.** All buffers and state are statically sized by
  `USER_DEFINED_DCC_*` constants.
- **No OS / RTOS.** Runs from a main loop plus a small number of ISRs.
- **Dependency injection everywhere.** Cross-module calls go through
  `interface_dcc_*_t` function-pointer structs, never direct includes. Only
  `dcc_config.c` (the wiring module) includes every module header.
- **Portable hardware layer.** The library never touches hardware directly; the
  user supplies driver callbacks in one `dcc_config_t` struct.

The user includes a single header — `dcc_config.h` — populates one struct, and
calls `DccConfig_initialize()`.

## 2. Feature flags

Defined by the user in `dcc_user_config.h`, validated with `#error`/`#warning`
in `dcc_config.h`.

| Flag | Enables |
|---|---|
| `DCC_COMPILE_COMMAND_STATION` | Packet encoding, bit framing, scheduler, service mode, RailCom cutout + receive |
| `DCC_COMPILE_DECODER` | Packet decoding, CV storage, RailCom response encoding |
| `DCC_COMPILE_ACCESSORY_DECODER` | Accessory-decoder RailCom (SRQ, status, time, error) |
| `DCC_COMPILE_SERVICE_MODE_DIRECT` | Direct byte/bit programming (requires CS) |
| `DCC_COMPILE_SERVICE_MODE_PAGED` | Paged programming (requires CS) |
| `DCC_COMPILE_SERVICE_MODE_REGISTER` | Physical register programming (requires CS) |
| `DCC_COMPILE_SERVICE_MODE_ADDRESS` | Address-only programming (requires CS) |

**Optional hardware is gated at runtime, not compile time.** RailCom and service
mode compile in with their parent role; if the hardware isn't present, the user
leaves the corresponding callback NULL and the feature self-disables. The four
service-mode flags each require `DCC_COMPILE_COMMAND_STATION`. A device may define
both CS and DECODER for booster/repeater use.

## 3. User-configurable constants

Set in `dcc_user_config.h`:

- `USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT` — max concurrent active scheduler slots
- `USER_DEFINED_DCC_MAX_LOCOS` — max autorefresh locomotives
- `USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH` — RailCom receive ring-buffer depth
- `USER_DEFINED_DCC_SERVICE_MODE_RETRIES`
- `USER_DEFINED_DCC_ACK_THRESHOLD_MA`, `..._ACK_MIN_DURATION_US`, `..._ACK_MAX_DURATION_US`
- `USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS`

## 4. The `dcc_config_t` struct

One user-facing struct (`dcc_config.h`). Required fields must be non-NULL;
NULL-optional fields disable the associated feature at runtime.

**Common (always required):** `lock_shared_resources`, `unlock_shared_resources`,
`get_timestamp_usec`.

**Command Station:** a shared fixed-period DCC timer (`shared_timer_start/stop`,
clocked at `DCC_ONE_BIT_HALF_PERIOD_US` = 58 µs) plus a RailCom one-shot timer
(`railcom_timer_start/stop`). Per-channel hardware is described by `dcc_output_hw_t`
for `main_track` and `service_track` (each with `timer_start/stop`, `pin_toggle`,
`track_power_set`, optional `current_sense_read`, and an optional nested
`dcc_railcom_hw_t`). Optional callbacks: `on_packet_sent`, `on_service_mode_result`,
`on_accessory_srq`.

**Decoder:** required `cv_read`/`cv_write`; NULL-optional `railcom_tx_pin_set`,
`decoder_edge_irq_enable`, `start_ack_pulse`/`stop_ack_pulse`; and a set of
`on_*_command` notification callbacks. Note the CV callbacks
(`on_cv_write_command`, `on_cv_verify_command`, `on_cv_bit_command`) carry a
`bool service_mode` argument so the application can distinguish service-mode
programming from POM.

## 5. Public API surface

### Lifecycle & ISR entry points (`dcc_config.h`)

- `DccConfig_initialize(const dcc_config_t *)`
- `DccConfig_run()` — main loop; **all application callbacks fire from here**
- `DccConfig_58us_timer_isr()` — [CS] shared bit timer
- `DccConfig_railcom_oneshot_timer_isr()` — [CS] RailCom cutout state machine
- `DccConfig_100ms_timer_tick()` — [CS] timeouts / housekeeping
- `DccConfig_decoder_edge_isr(uint32_t timestamp_usec)` — [DECODER] input-capture edge

### Application modules (role-first naming)

| Module | Role | Public prefix |
|---|---|---|
| `dcc_application_command_station_main_track` | CS | `DccApplicationCommandStationMainTrack_` — `power_on/off`, `send_packet`, `add_to_auto_refresh`, `remove_from_auto_refresh`, `remove_all_auto_refresh` |
| `dcc_application_command_station_service_track` | CS | `DccApplicationCommandStationServiceTrack_` — `power_on/off`, `enter/exit_service_mode`, `is_service_mode_active`, direct/paged/register/address `write`/`verify` |
| `dcc_application_command_station_packet` | CS | `DccApplicationCommandStationPacket_load_*` — packet builders |
| `dcc_application_decoder_cv` | DECODER | `DccApplicationDecoderCv_` — `read`, `write`, `is_locked` |
| `dcc_application_decoder_railcom` | DECODER | `DccApplicationDecoderRailcom_send_*` — address feedback, POM response, dynamic data, ack/nack, track search, cv auto transfer, raw |
| `dcc_application_accessory_decoder_railcom` | ACCESSORY | `DccApplicationAccessoryDecoderRailcom_` — SRQ, status (1/4/extended), time/error report, cutout/stop hooks |

> **Migration note:** the pre-refactor modules `dcc_application_main_track`,
> `dcc_application_service_track`, and `dcc_packet_encoder` still exist and are
> still compiled/tested alongside the role-first modules above. Retiring them is
> tracked in [ComplianceOverview.md](compliance/ComplianceOverview.md).

## 6. Internal modules

| Module | Role | Responsibility |
|---|---|---|
| `dcc_config` | always | Wiring: builds every interface struct, owns all module contexts, `initialize()`/`run()`/ISR dispatch |
| `dcc_types` | always | Typedefs, user-constant validation |
| `dcc_defines` | always | Protocol constants: timing, instruction masks, CV numbers, RailCom IDs |
| `dcc_scheduler` | CS | Priority queue, duplicate combining, auto-refresh round-robin |
| `dcc_bit_encoder` | CS | ISR bit framing from the shared fixed-period timer |
| `dcc_railcom_cutout` | CS | RailCom cutout timer state machine |
| `dcc_railcom_command_station` | CS | 4/8 decode of received RailCom, receive buffer |
| `dcc_service_mode_common` | CS | Shared ACK detection, reset sequencing, retry |
| `dcc_service_mode_{direct,paged,register,address}` | CS | Per-mode programming state machines |
| `dcc_bit_decoder` | DECODER | Edge-timestamp → bit classification → byte assembly |
| `dcc_packet_decoder` | DECODER | Parse bytes → structured commands, XOR, address match, dispatch |
| `dcc_cv_storage` | DECODER | CV abstraction, decoder lock, factory reset, fail-safe |
| `dcc_railcom_decoder` | DECODER | 4/8 encode of decoder RailCom responses |

Each module owns an `interface_dcc_<module>_t` of function pointers, populated by
`dcc_config.c`. This makes every dependency mockable in unit tests and lets an
MCU swap touch only the config struct.

## 7. Execution contexts

Three contexts, with a deliberately small locked region:

- **ISR (timer / edge):** bit encoder advances the bit state machine and toggles
  the output pin; RailCom cutout timing fires here; decoder edge classification
  happens here. ISR work is kept minimal. The decoder bit-bangs RailCom Tx from
  ISR context at 4 µs intervals during the cutout (the CPU is otherwise idle then).
- **Main loop (`DccConfig_run()`):** scheduler selection, packet build, service-mode
  state machine, RailCom receive drain, decoded-packet dispatch, fail-safe timing,
  6 ms ACK-pulse timing. **Every application callback fires from this context, never
  from an ISR.**
- **User API (any thread):** the `DccApplication*` calls write into shared state;
  on threaded hosts they must be protected by the user's `lock/unlock`.

The handoff between the ISR-level bit encoder and the main-loop scheduler is
double-buffered: the ISR reads the active buffer and sets a completion flag; the
main loop swaps buffers under `lock_shared_resources`/`unlock_shared_resources`.

## 8. Key invariants

1. No direct cross-module includes in the core library — only via interface structs.
2. Every new function/type is wrapped in the appropriate `DCC_COMPILE_*` guard.
3. No dynamic memory; all sizing comes from `USER_DEFINED_DCC_*` constants.
4. CV numbers are 1-based everywhere; the 0-based wire encoding is confined to the
   packet encoder.
5. Callbacks are main-loop only; ISRs do pin/flag work and defer notification.
