# Application API Refactor Working Plan

> **ARCHIVED / HISTORICAL — record of a largely-executed plan.** Phases 1–9
> (renames, module moves, ACK-pulse refactor) have shipped; phase 10 (accessory
> RailCom) is partially in. Still-pending items (RailCom cutout retiming, XPOM,
> etc.) are tracked in [../STATUS.md](../STATUS.md). Kept here as the record of
> what changed and why.

**Date:** 2026-04-13  
**Status:** Planning  

## Goal

Reorganize the library's user-facing API into a consistent set of `dcc_application_*.h/c` modules so that users have a clear, well-named surface for every task. Internal implementation stays in existing modules; application modules either move or stub-wrap the internals.

Modules use role-first naming: `dcc_application_command_station_*` and `dcc_application_decoder_*`.

**Legend:** `●` = required, `◐` = required if feature enabled, `○` = optional

---

## Implementation Context

This section provides everything a new conversation needs to implement the plan without prior context.

### Project Layout

| Path | Contents |
|---|---|
| `src/dcc/` | All library source (.c/.h) and test files (*_Test.cxx) — colocated |
| `applications/ti_theia/` | Application code (MSPM0 command station and decoder examples) |
| `documentation/specs/` | NMRA spec PDFs |
| `documentation/working_plans/` | This plan |

### Key Source Files

| File | Role |
|---|---|
| `src/dcc/dcc_config.h` / `.c` | User-facing config: hardware drivers, callbacks, ISR entry points, `initialize()`, `run()` |
| `src/dcc/dcc_types.h` | Shared type definitions (`dcc_packet_t`, `dcc_railcom_response_t`, etc.) |
| `src/dcc/dcc_defines.h` | Constants: RailCom datagram IDs, timing values, limits |
| `src/dcc/dcc_bit_encoder.h` / `.c` | DCC waveform generation — 58µs timer drives bit output, triggers RailCom cutout at end bit |
| `src/dcc/dcc_railcom_cutout.h` / `.c` | One-shot timer state machine for RailCom cutout window (currently has timing bug — see Open Question #2) |
| `src/dcc/dcc_railcom_encoder.h` / `.c` | RailCom 4/8 encoding and Tx — this is what the new decoder RailCom API wraps |
| `src/dcc/dcc_railcom_decoder.h` / `.c` | RailCom Rx — decodes incoming 4/8 encoded bytes on CS side |
| `src/dcc/dcc_packet_encoder.h` / `.c` | DCC packet builders — moves to `dcc_application_command_station_packet.h/c` in Phase 2 |

### Architecture Constraints

1. **Static memory only.** No `malloc`. All buffers and state are statically allocated. This affects how context structs and datagram buffers are sized.

2. **Interface structs for cross-module calls.** All inter-module communication uses function pointer structs (dependency injection). Never `#include` a module header to call its functions directly in core library files. This enables unit testing with mock implementations. See existing `interface_dcc_railcom_cutout_t` in `dcc_railcom_cutout.h` for the pattern.

3. **Compile-time feature flags.** Every function/type must be wrapped in the appropriate `DCC_COMPILE_*` ifdef guard. Key flags: `DCC_COMPILE_COMMAND_STATION`, `DCC_COMPILE_DECODER`, `DCC_COMPILE_ACCESSORY_DECODER`, `DCC_COMPILE_SERVICE_MODE_*`.

4. **RailCom 4/8 encoding.** RailCom uses 4-to-8 bit encoding (4 data bits → 8 wire bits with error detection). The encoding table is in `dcc_railcom_encoder.c`. Each encoded byte is 10 bits on the wire (start + 8 data + stop) at 250 kbaud = 40µs/byte.

5. **Bit-bang Tx for decoder.** The decoder transmits RailCom responses by bit-banging a GPIO pin at 4µs intervals from ISR context. No UART hardware required. Rationale: per-byte one-shot timer ISR overhead exceeded the Ch2 window margin (261µs for 6 bytes), and the decoder CPU is idle during cutout anyway, so a ~320µs stall is acceptable.

6. **Callbacks fire from `DccConfig_run()`.** All user-facing callbacks are deferred to the main loop context via `DccConfig_run()`, never called directly from ISR. Hardware driver callbacks (Tx pin, UART enable, H-bridge) are called from ISR context.

### Spec Documents

- **2012 published:** `documentation/specs/S-9.3.2_2012_12_10.pdf`
- **2026 draft:** `https://www.nmra.org/sites/default/files/standards/sandrp/DCC/S/s-9-3-2_bi-directional_communications_draft.pdf`
- Key sections: Figure 2 (cutout timing), Table 1 (timing parameters), Table 2 (ACK/NACK control words), Tables 5–6 (Ch1/Ch2 MOB datagram IDs), Section 5.2 (CV 28/29 config), Sections 6–8 (applications)

### Phase Dependencies

Phases can be implemented in order 1→10, but some have hard dependencies:
- **Phase 1** (renames/removals) should complete before Phase 2 (move/rename packet encoder)
- **Phase 7** (wiring) depends on Phases 5 and 6 (new decoder modules exist)
- **Phase 8** (tests) should be updated alongside each phase, not batched at the end
- **Phase 9** (application code) depends on all rename phases completing
- **Phase 10** (accessory RailCom) depends on Phase 6 (shares bit-bang Tx engine and encoding)

### Key Design Decisions Made During Planning

1. **CS RailCom application module eliminated.** `begin_cutout`/`end_cutout` are hardware driver callbacks (user provides them), not application functions. The `on_railcom_datagram_result` callback is the only CS RailCom user-facing element, and it lives in `dcc_config.h`.

2. **High-level decoder API over raw structs.** Named functions (`send_address_feedback`, `send_pom_response`, etc.) instead of exposing `dcc_railcom_response_t` with datagram IDs. `send_raw` is the escape hatch.

3. **ADR1/ADR2 alternation is internal.** `send_address_feedback(address)` maintains internal state to alternate — the user doesn't manage this.

4. **All cutout timing is user-configurable.** 5 CS parameters + 2 decoder parameters with spec-derived defaults. This compensates for ISR latency differences across MCU platforms.

5. **Decoder edge interrupt blanking during cutout.** `decoder_edge_irq_enable(false/true)` prevents noise from corrupting the bit decoder. User chooses whether to disable hardware interrupt or set a flag.

6. **ACK pulse timing moved to library.** `start_ack_pulse` + `stop_ack_pulse` with 6ms timing managed by polling `get_timestamp_usec` in `DccConfig_run()`, replacing the old `fire_ack_pulse` where the user had to time 6ms.

---

## Module Overview

| Module | Status | Strategy |
|---|---|---|
| `dcc_config.h` | Exists — needs renames, removals, and new hardware driver | Modify in place |
| `dcc_application_command_station_packet.h/c` | **New** | Move/rename from `dcc_packet_encoder` |
| `dcc_application_command_station_main_track.h/c` | Exists as `dcc_application_main_track` — needs rename, renames, and `insert()` split | Modify in place |
| `dcc_application_command_station_service_track.h/c` | Exists as `dcc_application_service_track` — needs rename and renames | Modify in place |
| `dcc_application_decoder_cv.h/c` | **New** | Stub wrappers around `dcc_cv_storage` |
| `dcc_application_decoder_railcom.h/c` | **New** | Stub wrappers around `dcc_railcom_encoder` |
| `dcc_application_accessory_decoder_railcom.h/c` | **New** | Accessory decoder RailCom: SRQ, Status, Time, Error |

---

## Phase 1: `dcc_config.h` Changes

### Remove Dead Code

| Item | Action |
|---|---|
| `DccConfig_main_track_isr()` | Remove — dead code, replaced by shared timer |
| `DccConfig_service_track_isr()` | Remove — dead code, replaced by shared timer |
| `DccBitEncoder_half_bit_isr()` | Remove — internal, only used by removed per-channel ISRs |
| Per-channel variable-period timer architecture | Remove from `dcc_bit_encoder.c` |
| `on_idle` callback | Remove — never fires when auto refresh is in use |
| `timer_set_period` in `dcc_output_hw_t` | Remove — only used by removed variable-period architecture |

### Rename ISR Entry Points

| Current | Proposed |
|---|---|
| `DccConfig_shared_timer_isr()` | `DccConfig_58us_timer_isr()` |
| `DccConfig_railcom_cutout_timer_isr()` | `DccConfig_railcom_oneshot_timer_isr()` |
| `DccConfig_decoder_edge()` | `DccConfig_decoder_edge_isr()` |

### Rename Callbacks

| Current | Proposed |
|---|---|
| `on_service_mode_complete` | `on_service_mode_result` |
| `on_datagram` (in `dcc_railcom_hw_t`) | `on_railcom_datagram_result` |
| `on_emergency_stop` | `on_emergency_stop_command` |
| `on_binary_state_short` | `on_binary_state_short_command` |
| `on_binary_state_long` | `on_binary_state_long_command` |
| `on_analog_function` | `on_analog_function_command` |
| `on_speed_restriction` | `on_speed_restriction_command` |
| `on_cv_write` | `on_cv_write_command` |
| `on_cv_verify` | `on_cv_verify_command` |
| `on_cv_bit` | `on_cv_bit_command` |

### ACK Pulse Refactor

| Current | Proposed |
|---|---|
| `fire_ack_pulse` (single callback, user handles 6ms timing) | `start_ack_pulse` + `stop_ack_pulse` (two hardware driver callbacks, library handles 6ms timing by polling `get_timestamp_usec` from `DccConfig_run()`) |

Move from decoder callbacks section to decoder hardware drivers section.

### Full `dcc_config.h` User-Facing API

#### System Lifecycle

| Req | Current Name | Proposed Name | Why the User Needs It |
|---|---|---|---|
| ● | `DccConfig_initialize()` | `DccConfig_initialize()` | Initialize the entire DCC stack with user-provided hardware drivers and callbacks |
| ● | `DccConfig_run()` | `DccConfig_run()` | Main loop processing — call repeatedly, fires all application callbacks from this context |

#### Command Station ISR Entry Points

| Req | Current Name | Proposed Name | Why the User Needs It |
|---|---|---|---|
| — | `DccConfig_main_track_isr()` | **REMOVE** — dead code, replaced by shared timer | — |
| — | `DccConfig_service_track_isr()` | **REMOVE** — dead code, replaced by shared timer | — |
| ● | `DccConfig_shared_timer_isr()` | `DccConfig_58us_timer_isr()` | Call from 58us shared timer ISR — drives both main and service track bit encoders |
| ◐ | `DccConfig_railcom_cutout_timer_isr()` | `DccConfig_railcom_oneshot_timer_isr()` | Call from RailCom one-shot timer ISR — drives cutout state machine through delay, Ch1, and Ch2 phases. Required if using RailCom. |
| ● | `DccConfig_100ms_timer_tick()` | `DccConfig_100ms_timer_tick()` | Call from a 100ms periodic timer — used for timeouts and housekeeping |

#### Decoder ISR Entry Points

| Req | Current Name | Proposed Name | Why the User Needs It |
|---|---|---|---|
| ● | `DccConfig_decoder_edge()` | `DccConfig_decoder_edge_isr()` | Call from input-capture ISR on each signal edge — library classifies bits from edge timing |

#### Command Station Callbacks — Main Track

| Req | Current Name | Proposed Name | Why the User Needs It |
|---|---|---|---|
| — | `on_idle` | **REMOVE** — never fires when auto refresh is in use | — |
| ○ | `on_packet_sent` | `on_packet_sent` | Notifies when a packet has been fully transmitted on the wire |

#### Command Station Callbacks — Service Mode

| Req | Current Name | Proposed Name | Why the User Needs It |
|---|---|---|---|
| ◐ | `on_service_mode_complete` | `on_service_mode_result` | Notifies when a programming operation has finished — delivers pass/fail result and data |

#### Command Station Callbacks — RailCom

| Req | Current Name | Proposed Name | Why the User Needs It |
|---|---|---|---|
| ◐ | `on_datagram` (in `dcc_railcom_hw_t`) | `on_railcom_datagram_result` | Notifies when a RailCom datagram has been decoded — delivers address, channel, and decoded data. Required if using RailCom. |

#### Command Station Hardware Drivers — RailCom

| Req | Current Name | Proposed Name | Why the User Needs It |
|---|---|---|---|
| ◐ | *(new/moved)* | `begin_railcom_cutout` | Tristate the H-bridge output stage — called by library at T_CS. Required if using RailCom. |
| ◐ | *(new/moved)* | `end_railcom_cutout` | Restore H-bridge to normal drive mode — called by library at T_CE. Required if using RailCom. |
| ◐ | *(simplified)* | `uart_rx_enable` | Enable UART Rx — called by library at T_TS1 and T_TS2. Required if using RailCom. |
| ◐ | *(simplified)* | `uart_rx_disable` | Disable UART Rx — called by library at T_TC1 and T_CE. Required if using RailCom. |

#### Decoder Callbacks — Main Track

| Req | Current Name | Proposed Name | Why the User Needs It |
|---|---|---|---|
| ○ | `on_speed_command` | `on_speed_command` | Speed and direction command received for this decoder |
| ○ | `on_emergency_stop` | `on_emergency_stop_command` | Emergency stop received for this decoder's address |
| ○ | `on_function_command` | `on_function_command` | Function on/off command received (F0–F68) |
| ○ | `on_accessory_basic_command` | `on_accessory_basic_command` | Basic accessory command received (turnout throw/close) |
| ○ | `on_accessory_extended_command` | `on_accessory_extended_command` | Extended accessory command received (signal aspect) |
| ○ | `on_consist_command` | `on_consist_command` | Consist address assignment or clear received |
| ○ | `on_binary_state_short` | `on_binary_state_short_command` | Short-form binary state command received (states 0–127) |
| ○ | `on_binary_state_long` | `on_binary_state_long_command` | Long-form binary state command received (states 0–32767) |
| ○ | `on_analog_function` | `on_analog_function_command` | Analog function output command received (volume, dimming) |
| ○ | `on_speed_restriction` | `on_speed_restriction_command` | Speed restriction command received (slow zone, block signaling) |
| ○ | `on_failsafe_entered` | `on_failsafe_entered` | Decoder lost valid DCC signal — entered fail-safe mode |
| ○ | `on_failsafe_exited` | `on_failsafe_exited` | Decoder recovered valid DCC signal — exited fail-safe mode |

#### Decoder Callbacks — Service Mode

| Req | Current Name | Proposed Name | Why the User Needs It |
|---|---|---|---|
| ◐ | `on_cv_write` | `on_cv_write_command` | CV write command received on this decoder |
| ◐ | `on_cv_verify` | `on_cv_verify_command` | CV verify command received on this decoder |
| ◐ | `on_cv_bit` | `on_cv_bit_command` | CV bit manipulation command received on this decoder |

#### Decoder Hardware Drivers — Service Mode

| Req | Current Name | Proposed Name | Why the User Needs It |
|---|---|---|---|
| ◐ | `fire_ack_pulse` | `start_ack_pulse` | Turn on the ACK current load — library handles 6ms timing and calls stop automatically |
| ◐ | *(new)* | `stop_ack_pulse` | Turn off the ACK current load — called by library after 6ms |

#### Decoder Hardware Drivers — RailCom

| Req | Current Name | Proposed Name | Why the User Needs It |
|---|---|---|---|
| ◐ | *(new)* | `railcom_tx_pin_set(bool high)` | Set the RailCom Tx GPIO high or low. Library bit-bangs 4/8 encoded bytes at 4µs intervals from ISR context. No UART hardware needed. Required if using RailCom. |
| ◐ | *(new)* | `decoder_edge_irq_enable(bool enabled)` | Enable or disable the DCC edge-detect interrupt during RailCom cutout. Called with `false` at end bit before cutout, `true` after cutout ends. User may disable hardware interrupt or set a flag to skip — user's choice. Required if using RailCom. |

---

## Phase 2: `dcc_application_command_station_packet.h/c` (New)

**Strategy:** Move/rename from `dcc_packet_encoder.h/c`. Pure computational, stateless — the entire file moves as-is including all private helpers and lookup tables.

**ifdef guard:** `DCC_COMPILE_COMMAND_STATION`

**Removed from public API:**
- `DccPacketEncoder_idle()` — internal, scheduler sends automatically

**All functions renamed from `DccPacketEncoder_*` to `DccApplicationCommandStationPacket_load_*`:**

| Req | Current | Proposed | Why the User Needs It |
|---|---|---|---|
| ○ | `DccPacketEncoder_reset()` | `DccApplicationCommandStationPacket_load_reset()` | Load a broadcast reset into a packet — all decoders clear volatile state and return to power-up |
| ○ | `DccPacketEncoder_estop_all()` | `DccApplicationCommandStationPacket_load_estop_all()` | Load a broadcast emergency stop into a packet — panic stop every decoder on the track |
| ○ | `DccPacketEncoder_speed_128()` | `DccApplicationCommandStationPacket_load_speed_128()` | Load a 128-step speed and direction command into a packet |
| ○ | `DccPacketEncoder_speed_28()` | `DccApplicationCommandStationPacket_load_speed_28()` | Load a 28-step speed and direction command into a packet |
| ○ | `DccPacketEncoder_speed_14()` | `DccApplicationCommandStationPacket_load_speed_14()` | Load a 14-step speed, direction, and headlight command into a packet |
| ○ | `DccPacketEncoder_func_group_1()` | `DccApplicationCommandStationPacket_load_func_group_1()` | Load a function group 1 command into a packet (FL, F1–F4) |
| ○ | `DccPacketEncoder_func_group_2a()` | `DccApplicationCommandStationPacket_load_func_group_2a()` | Load a function group 2a command into a packet (F5–F8) |
| ○ | `DccPacketEncoder_func_group_2b()` | `DccApplicationCommandStationPacket_load_func_group_2b()` | Load a function group 2b command into a packet (F9–F12) |
| ○ | `DccPacketEncoder_func_f13_f20()` | `DccApplicationCommandStationPacket_load_func_f13_f20()` | Load an expansion function command into a packet (F13–F20) |
| ○ | `DccPacketEncoder_func_f21_f28()` | `DccApplicationCommandStationPacket_load_func_f21_f28()` | Load an expansion function command into a packet (F21–F28) |
| ○ | `DccPacketEncoder_func_f29_f36()` | `DccApplicationCommandStationPacket_load_func_f29_f36()` | Load an expansion function command into a packet (F29–F36) |
| ○ | `DccPacketEncoder_func_f37_f44()` | `DccApplicationCommandStationPacket_load_func_f37_f44()` | Load an expansion function command into a packet (F37–F44) |
| ○ | `DccPacketEncoder_func_f45_f52()` | `DccApplicationCommandStationPacket_load_func_f45_f52()` | Load an expansion function command into a packet (F45–F52) |
| ○ | `DccPacketEncoder_func_f53_f60()` | `DccApplicationCommandStationPacket_load_func_f53_f60()` | Load an expansion function command into a packet (F53–F60) |
| ○ | `DccPacketEncoder_func_f61_f68()` | `DccApplicationCommandStationPacket_load_func_f61_f68()` | Load an expansion function command into a packet (F61–F68) |
| ○ | `DccPacketEncoder_accessory_basic()` | `DccApplicationCommandStationPacket_load_accessory_basic()` | Load a basic accessory command into a packet (turnouts, stationary decoders) |
| ○ | `DccPacketEncoder_accessory_extended()` | `DccApplicationCommandStationPacket_load_accessory_extended()` | Load an extended accessory command into a packet (signal aspects) |
| ○ | `DccPacketEncoder_accessory_basic_cv_write()` | `DccApplicationCommandStationPacket_load_accessory_basic_cv_write()` | Load a basic accessory POM CV write command into a packet |
| ○ | `DccPacketEncoder_accessory_basic_cv_verify()` | `DccApplicationCommandStationPacket_load_accessory_basic_cv_verify()` | Load a basic accessory POM CV verify command into a packet |
| ○ | `DccPacketEncoder_accessory_basic_cv_bit()` | `DccApplicationCommandStationPacket_load_accessory_basic_cv_bit()` | Load a basic accessory POM CV bit write/verify command into a packet |
| ○ | *(new)* | `DccApplicationCommandStationPacket_load_accessory_basic_cv_read_xpom()` | Load a basic accessory XPOM read command — reads 4 contiguous indexed CVs (S-9.3.2 Section 8.9) |
| ○ | *(new)* | `DccApplicationCommandStationPacket_load_accessory_basic_cv_write_xpom()` | Load a basic accessory XPOM write command — writes 1–4 contiguous indexed CV bytes. Must be sent twice per spec. |
| ○ | *(new)* | `DccApplicationCommandStationPacket_load_accessory_basic_cv_bit_xpom()` | Load a basic accessory XPOM bit write command — writes 1–4 contiguous indexed CV bits. Must be sent twice per spec. |
| ○ | `DccPacketEncoder_accessory_extended_cv_write()` | `DccApplicationCommandStationPacket_load_accessory_extended_cv_write()` | Load an extended accessory POM CV write command into a packet |
| ○ | `DccPacketEncoder_accessory_extended_cv_verify()` | `DccApplicationCommandStationPacket_load_accessory_extended_cv_verify()` | Load an extended accessory POM CV verify command into a packet |
| ○ | `DccPacketEncoder_accessory_extended_cv_bit()` | `DccApplicationCommandStationPacket_load_accessory_extended_cv_bit()` | Load an extended accessory POM CV bit write/verify command into a packet |
| ○ | *(new)* | `DccApplicationCommandStationPacket_load_accessory_extended_cv_read_xpom()` | Load an extended accessory XPOM read command — reads 4 contiguous indexed CVs (S-9.3.2 Section 8.9) |
| ○ | *(new)* | `DccApplicationCommandStationPacket_load_accessory_extended_cv_write_xpom()` | Load an extended accessory XPOM write command — writes 1–4 contiguous indexed CV bytes. Must be sent twice per spec. |
| ○ | *(new)* | `DccApplicationCommandStationPacket_load_accessory_extended_cv_bit_xpom()` | Load an extended accessory XPOM bit write command — writes 1–4 contiguous indexed CV bits. Must be sent twice per spec. |
| ○ | `DccPacketEncoder_cv_write_ops()` | `DccApplicationCommandStationPacket_load_cv_write_pom()` | Load a locomotive POM CV write command into a packet |
| ○ | `DccPacketEncoder_cv_verify_ops()` | `DccApplicationCommandStationPacket_load_cv_verify_pom()` | Load a locomotive POM CV verify command into a packet |
| ○ | `DccPacketEncoder_cv_bit_ops()` | `DccApplicationCommandStationPacket_load_cv_bit_pom()` | Load a locomotive POM CV bit write/verify command into a packet |
| ○ | *(new)* | `DccApplicationCommandStationPacket_load_cv_read_xpom()` | Load a locomotive XPOM read command into a packet — reads 4 contiguous indexed CVs using 24-bit address and sequence identifier (S-9.3.2 Section 6.6.1) |
| ○ | *(new)* | `DccApplicationCommandStationPacket_load_cv_write_xpom()` | Load a locomotive XPOM write command into a packet — writes 1–4 contiguous indexed CV bytes (S-9.3.2 Section 6.6.2). Must be sent twice per spec. |
| ○ | *(new)* | `DccApplicationCommandStationPacket_load_cv_bit_xpom()` | Load a locomotive XPOM bit write command into a packet — writes 1–4 contiguous indexed CV bits (S-9.3.2 Section 6.6.3). Must be sent twice per spec. |
| ○ | `DccPacketEncoder_consist_set()` | `DccApplicationCommandStationPacket_load_consist_set()` | Load a consist address assignment command into a packet |
| ○ | `DccPacketEncoder_consist_clear()` | `DccApplicationCommandStationPacket_load_consist_clear()` | Load a consist address clear command into a packet |
| ○ | `DccPacketEncoder_binary_state_short()` | `DccApplicationCommandStationPacket_load_binary_state_short()` | Load a short-form binary state command into a packet (states 0–127) |
| ○ | `DccPacketEncoder_binary_state_long()` | `DccApplicationCommandStationPacket_load_binary_state_long()` | Load a long-form binary state command into a packet (states 0–32767) |
| ○ | `DccPacketEncoder_analog_function()` | `DccApplicationCommandStationPacket_load_analog_function()` | Load an analog function command into a packet (volume, dimming) |
| ○ | `DccPacketEncoder_speed_restriction()` | `DccApplicationCommandStationPacket_load_speed_restriction()` | Load a speed restriction command into a packet (slow zones, block signaling) |
| ○ | *(new)* | `DccApplicationCommandStationPacket_load_accessory_nop()` | Load an accessory decoder NOP command into a packet — facilitates accessory decoder service requests (SRQ). CS must send within 5 seconds of energizing track per S-9.3.2 Section 6.3.3. |
| ○ | *(new)* | `DccApplicationCommandStationPacket_load_accessory_basic_stop()` | Load a basic accessory decoder stop command — instructs decoder to cease SRQ and send its update (Table 36) |
| ○ | *(new)* | `DccApplicationCommandStationPacket_load_accessory_extended_stop()` | Load an extended accessory decoder stop command — instructs decoder to cease SRQ and send its update (Table 36) |

**Note:** `DccPacketEncoder_idle()` must remain accessible internally for the scheduler. It stays as a static/internal function in the .c file but is not declared in the public header.

---

## Phase 3: `dcc_application_command_station_main_track.h/c` Changes

**Strategy:** Rename from `dcc_application_main_track.h/c`. Modify in place.

**ifdef guard:** `DCC_COMPILE_COMMAND_STATION`

**Remove from public API:**
- `DccApplicationMainTrack_initialize()` — wired internally by `dcc_config.c`

**Split `insert()` into two functions and rename:**

| Req | Current | Proposed | Why the User Needs It |
|---|---|---|---|
| ● | `DccApplicationMainTrack_power_on()` | `DccApplicationCommandStationMainTrack_power_on()` | Turn on the main track, start DCC signal generation |
| ● | `DccApplicationMainTrack_power_off()` | `DccApplicationCommandStationMainTrack_power_off()` | Turn off the main track, stop DCC signal |
| ○ | `DccApplicationMainTrack_insert()` | `DccApplicationCommandStationMainTrack_send_packet()` | Send a packet once — packet data is copied, caller's packet does not need to persist |
| ○ | *(split from insert w/ auto_refresh=true)* | `DccApplicationCommandStationMainTrack_add_to_auto_refresh()` | Add a packet to the auto refresh cycle for continuous retransmission — packet data is copied, caller's packet does not need to persist |
| ○ | `DccApplicationMainTrack_remove_address()` | `DccApplicationCommandStationMainTrack_remove_from_auto_refresh()` | Stop auto refreshing a specific address |
| ○ | `DccApplicationMainTrack_clear()` | `DccApplicationCommandStationMainTrack_remove_all_auto_refresh()` | Remove all packets from the auto refresh cycle |

---

## Phase 4: `dcc_application_command_station_service_track.h/c` Changes

**Strategy:** Rename from `dcc_application_service_track.h/c`. Modify in place.

**ifdef guard:** `DCC_COMPILE_COMMAND_STATION` (plus per-mode `DCC_COMPILE_SERVICE_MODE_*` guards)

**Remove from public API:**
- `DccApplicationServiceTrack_initialize()` — wired internally by `dcc_config.c`

**Renames and full API:**

| Req | Current | Proposed | Why the User Needs It |
|---|---|---|---|
| ◐ | `DccApplicationServiceTrack_power_on()` | `DccApplicationCommandStationServiceTrack_power_on()` | Power the programming track |
| ◐ | `DccApplicationServiceTrack_power_off()` | `DccApplicationCommandStationServiceTrack_power_off()` | Kill programming track power |
| ◐ | `DccApplicationServiceTrack_enter()` | `DccApplicationCommandStationServiceTrack_enter_service_mode()` | Begin exclusive programming session on the service track |
| ◐ | `DccApplicationServiceTrack_exit()` | `DccApplicationCommandStationServiceTrack_exit_service_mode()` | End programming session — user is responsible for calling this after on_service_mode_result fires |
| ◐ | `DccApplicationServiceTrack_is_active()` | `DccApplicationCommandStationServiceTrack_is_service_mode_active()` | Check if the library is currently in service mode |
| ◐ | `DccApplicationServiceTrack_direct_write_byte()` | `DccApplicationCommandStationServiceTrack_direct_write_byte()` | Write a CV byte using direct mode — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_direct_verify_byte()` | `DccApplicationCommandStationServiceTrack_direct_verify_byte()` | Verify/read a CV byte using direct mode — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_direct_write_bit()` | `DccApplicationCommandStationServiceTrack_direct_write_bit()` | Write a single CV bit using direct mode — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_direct_verify_bit()` | `DccApplicationCommandStationServiceTrack_direct_verify_bit()` | Verify/read a single CV bit using direct mode — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_paged_write()` | `DccApplicationCommandStationServiceTrack_paged_write()` | Write a CV using paged mode — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_paged_verify()` | `DccApplicationCommandStationServiceTrack_paged_verify()` | Verify a CV using paged mode — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_register_write()` | `DccApplicationCommandStationServiceTrack_register_write()` | Write registers 1–8 (legacy) — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_register_verify()` | `DccApplicationCommandStationServiceTrack_register_verify()` | Verify registers 1–8 (legacy) — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_address_write()` | `DccApplicationCommandStationServiceTrack_address_write()` | Write CV1 only (simplest mode) — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_address_verify()` | `DccApplicationCommandStationServiceTrack_address_verify()` | Verify CV1 only (simplest mode) — result delivered via on_service_mode_result callback |

---

## Phase 5: `dcc_application_decoder_cv.h/c` (New)

**Strategy:** Stub wrappers around `dcc_cv_storage.c`.

**ifdef guard:** `DCC_COMPILE_DECODER`

| Req | Current | Proposed | Why the User Needs It |
|---|---|---|---|
| ○ | `DccCvStorage_read()` | `DccApplicationDecoderCv_read()` | Read a CV value from the decoder's persistent storage |
| ○ | `DccCvStorage_write()` | `DccApplicationDecoderCv_write()` | Write a CV value — enforces decoder lock (CV15 must equal CV16) |
| ○ | `DccCvStorage_is_locked()` | `DccApplicationDecoderCv_is_locked()` | Check if the decoder is locked against configuration changes (CV15 != CV16) |

**Not exposed:** `DccCvStorage_initialize()` — wired internally by `dcc_config.c`

---

## Phase 6: `dcc_application_decoder_railcom.h/c` (New)

**Strategy:** High-level named functions that hide datagram IDs, 4/8 encoding, and Ch1/Ch2 splitting from the user. Wraps `dcc_railcom_encoder.c` internals. A `send_raw` escape hatch covers XPOM responses, CV auto transfer, time datagrams, and future datagram types.

**ifdef guard:** `DCC_COMPILE_DECODER`

| Req | Proposed | Why the User Needs It |
|---|---|---|
| ◐ | `DccApplicationDecoderRailcom_send_address_feedback(uint16_t address)` | Send Ch1 address datagram (ADR). Library alternates ADR1 (ID1) / ADR2 (ID2) automatically across consecutive cutouts per S-9.3.2 Section 6.2.1 |
| ◐ | `DccApplicationDecoderRailcom_send_pom_response(uint16_t cv_address, uint8_t value)` | Send Ch2 POM CV read/write response (ID0, 12-bit datagram) — returns CV data after executing POM command |
| ○ | `DccApplicationDecoderRailcom_send_dynamic_data(uint8_t subid, uint8_t value)` | Send Ch2 dynamic variable update (ID7, 18-bit datagram). `subid` selects DV 0–63 per Table 25 (e.g. 0=speed, 26=temperature, 27=direction) |
| ○ | `DccApplicationDecoderRailcom_send_ack()` | Send Ch2 ACK control word — confirms command receipt |
| ○ | `DccApplicationDecoderRailcom_send_nack()` | Send Ch2 NACK control word — command not yet complete, CV not supported, or CV is read-only |
| ○ | `DccApplicationDecoderRailcom_send_track_search_response(uint16_t address, uint8_t seconds_since_powerup)` | Send Ch2 track search response — packs ADR1 (ID1) + ADR2 (ID2) + Time (ID14) into one Ch2 transmission. Called in response to broadcast XF2 command for 30 seconds after power-up (S-9.3.2 Section 6.2.3) |
| ○ | `DccApplicationDecoderRailcom_send_cv_auto_transfer(uint32_t indexed_cv_address, uint8_t value)` | Send Ch2 CV automatic transfer response (ID12, 36-bit datagram). Library encodes the 24-bit indexed CV address (page index + offset) and data value. Sent in response to operational commands when XF3 is enabled (S-9.3.2 Section 6.7) |
| ○ | `DccApplicationDecoderRailcom_send_raw(uint8_t datagram_id, const uint8_t *data, uint8_t count)` | Send arbitrary RailCom datagram — escape hatch for XPOM responses (IDs 8–11, 36-bit) or future/custom datagram types. `count` is number of data bytes (max 4 for 36-bit datagrams). |

**Removed from public API:**
- `DccRailcomEncoder_send_ch1()` / `DccRailcomEncoder_send_ch2()` — replaced by high-level functions above
- `DccRailcomEncoder_encode_byte()` — internal 4/8 encoding, not user-facing
- `DccRailcomEncoder_initialize()` — wired internally by `dcc_config.c`

**Implementation notes:**

1. **ADR1/ADR2 alternation:** `send_address_feedback` maintains internal state to alternate between ADR1 (ID1) and ADR2 (ID2) across consecutive cutouts. Address is split per Table 19 based on type: 7-bit base address (CV1), 7-bit consist address (CV19), or 14-bit extended address (CV17/CV18). The optional ID3 info datagram (driving data) can be sent via `send_raw`.

2. **36-bit datagram support:** `send_raw` must support up to 4 data bytes → 6 encoded bytes in Ch2 for XPOM (IDs 8–11) and CV auto transfer (ID12). Bit-bang timing: 6 × 40µs = 240µs in a 261µs Ch2 window — 21µs margin, tight but feasible.

3. **Dynamic Ch1 cessation (CV 28 bit 2):** When enabled, the library auto-ceases Ch1 address responses after the decoder receives 8 addressed commands. Resumes only on three conditions (Table 21): decoder restart, address change, or 5 seconds with no addressed commands. This is internal library logic, not exposed in the API.

4. **ACK/NACK usage per spec:** For POM read — send NACK until read completes, then send ID0 datagram. For POM write — send NACK until write completes, then send ID0 datagram. If CV is unsupported, send ACK then NACK. If CV is read-only (write), send ACK then NACK. Both control words may be sent in the same cutout (Section 6.1). 0.5 second timeout applies to all POM/XPOM transactions.

5. **Ch1/Ch2 channel assignment:** Ch1 functions (`send_address_feedback`) produce 12-bit datagrams (2 encoded bytes). Ch2 functions (`send_pom_response`, `send_dynamic_data`, `send_ack`, `send_nack`) produce 12-bit to 36-bit datagrams (2–6 encoded bytes). `send_raw` can target either channel depending on datagram ID and size.

---

## Phase 7: Update `dcc_config.c` Wiring

- Wire new application module initializes internally during `DccConfig_initialize()`
- Keep `DccPacketEncoder_idle()` accessible internally for the scheduler's `build_idle_packet` interface
- Implement 6ms ACK pulse timing: `start_ack_pulse` called when ACK needed, poll `get_timestamp_usec` in `DccConfig_run()`, call `stop_ack_pulse` after 6ms
- Remove per-channel timer wiring (`half_bit_isr`, `timer_set_period` usage)
- **Bug fix:** Wire `DccRailcomCutout` module to call `DccRailcomDecoder_begin_cutout()` / `DccRailcomDecoder_end_cutout()` so the decoder's `cutout_pending` flag is set and `DccRailcomDecoder_run()` knows when to process incoming UART bytes. Currently these are never called — the one-shot timer cutout module manages H-bridge and UART but never signals the decoder.

---

## Phase 8: Update Tests

17 test files exist. All must be checked for renamed references. Files are grouped by impact level:

**Renamed module tests (primary — file rename + all references):**
- `dcc_packet_encoder_Test.cxx` → `dcc_application_command_station_packet_Test.cxx`
- `dcc_application_main_track_Test.cxx` → `dcc_application_command_station_main_track_Test.cxx`
- `dcc_application_service_track_Test.cxx` → `dcc_application_command_station_service_track_Test.cxx`
- `dcc_railcom_encoder_Test.cxx` → `dcc_application_decoder_railcom_Test.cxx`
- `dcc_cv_storage_Test.cxx` → `dcc_application_decoder_cv_Test.cxx`

**Integration / cross-module tests (reference renamed functions):**
- `dcc_config_Test.cxx` — references functions from multiple renamed modules
- `dcc_scheduler_Test.cxx` — references packet encoder functions
- `dcc_railcom_decoder_Test.cxx` — references cutout begin/end functions
- `dcc_railcom_cutout_Test.cxx` — may reference renamed config/wiring

**Service mode backend tests (reference renamed application layer):**
- `dcc_service_mode_common_Test.cxx`
- `dcc_service_mode_direct_Test.cxx`
- `dcc_service_mode_paged_Test.cxx`
- `dcc_service_mode_register_Test.cxx`
- `dcc_service_mode_address_Test.cxx`

**Likely unchanged (internal modules not renamed):**
- `dcc_bit_encoder_Test.cxx` — dead code removal only (half_bit_isr tests)
- `dcc_bit_decoder_Test.cxx`
- `dcc_packet_decoder_Test.cxx`

**New test coverage needed:**
- `send_packet()` / `add_to_auto_refresh()` split (Phase 3)
- 6ms ACK pulse timing (Phase 1)
- `DccApplicationDecoderRailcom_send_address_feedback()` — ADR1/ADR2 alternation logic (Phase 6)
- `DccApplicationDecoderRailcom_send_pom_response()` — ID0 datagram encoding (Phase 6)
- `DccApplicationDecoderRailcom_send_dynamic_data()` — ID7 datagram with sub-index (Phase 6)
- `DccApplicationDecoderRailcom_send_ack()` / `send_nack()` — control word encoding (Phase 6)
- `DccApplicationDecoderRailcom_send_track_search_response()` — ADR1+ADR2+Time packing (Phase 6)
- `DccApplicationDecoderRailcom_send_cv_auto_transfer()` — ID12 36-bit datagram encoding (Phase 6)
- `DccApplicationDecoderRailcom_send_raw()` — 12-bit, 18-bit, and 36-bit datagram sizes (Phase 6)
- XPOM packet builders — mobile, basic accessory, and extended accessory (Phase 2)
- Accessory NOP packet builder (Phase 2)
- RailCom decoder cutout_pending wiring fix (Phase 7)
- Dynamic Ch1 cessation logic — CV 28 bit 2 auto-cease after 8 commands, resume conditions (Phase 6)

---

## Phase 9: Update Application Code

- Update `applications/ti_theia/mspm03507_launchpad/command_station/` to use new function names
- Replace `DccConfig_shared_timer_isr()` with `DccConfig_58us_timer_isr()`
- Replace `fire_ack_pulse` with `start_ack_pulse` / `stop_ack_pulse` pair

---

## Phase 10: Accessory Decoder RailCom Support

**Spec reference:** S-9.3.2 Sections 7–8 (2026 draft)

This phase adds bi-directional communication support for accessory decoders. The architecture reuses the existing RailCom cutout timing (Phase 1/Open Question #2) and bit-bang Tx (Phase 6). New work is the SRQ protocol, accessory-specific datagram types, and CS-side NOP scheduling.

### Feature Flags

| Flag | Scope |
|---|---|
| `DCC_COMPILE_ACCESSORY_DECODER` | Decoder-side accessory RailCom functions |
| `DCC_COMPILE_COMMAND_STATION` | CS-side SRQ detection, NOP scheduling |

Shared functions from Phase 6 (`send_pom_response`, `send_ack`, `send_nack`, `send_dynamic_data`, `send_raw`) need their ifdef guard widened to `#if defined(DCC_COMPILE_DECODER) || defined(DCC_COMPILE_ACCESSORY_DECODER)`.

### 10.1 Accessory Decoder RailCom API — `dcc_application_accessory_decoder_railcom.h/c` (New)

**Strategy:** New module for accessory-specific RailCom functions. Shares the underlying bit-bang Tx engine and 4/8 encoding with `dcc_application_decoder_railcom.c`.

**ifdef guard:** `DCC_COMPILE_ACCESSORY_DECODER`

| Req | Proposed | Why the User Needs It |
|---|---|---|
| ◐ | `DccApplicationAccessoryDecoderRailcom_send_srq(uint16_t address, bool is_extended)` | Send Ch1 SRQ datagram (12-bit, no identifier). `is_extended` selects 9-bit basic or 11-bit extended address format. Library manages SRQ persistence — keeps sending on every cutout until stop command received (Section 7.1). |
| ◐ | `DccApplicationAccessoryDecoderRailcom_send_status(uint8_t aspect_state, bool command_match, bool is_setpoint)` | Send Ch2 Status 1 response (ID4, 12-bit). Basic format: 5-bit aspect state in bits 0–4, command/status match flags in bits 5–6. Mandatory for all accessory decoders with RailCom (Section 7.3.1, Table 45). |
| ○ | `DccApplicationAccessoryDecoderRailcom_send_status_extended(uint8_t aspect_state, bool command_match, bool is_setpoint)` | Send Ch2 Status 1 response (ID4, 2 × 12-bit). Extended format: 8-bit aspect state split across two datagrams — lower 5 bits in first, upper 3 bits in second (Section 7.3.2, Table 46). |
| ○ | `DccApplicationAccessoryDecoderRailcom_send_status_4(uint8_t all_pairs_state)` | Send Ch2 Status 4 response (ID3, 12-bit). Reports all 4 output pairs' aspect states in a single datagram. Basic format decoders only (Section 7.4, Tables 47–48). |
| ○ | `DccApplicationAccessoryDecoderRailcom_send_time_report(uint8_t time_value, bool resolution_1s)` | Send Ch2 Time response (ID5, 12-bit). Reports time remaining until command completion. `resolution_1s`: false = 0.1s/bit (0–12.7s), true = 1s/bit (0–127s). Use time=0 for instantaneous state changes (Section 7.5, Table 49). |
| ○ | `DccApplicationAccessoryDecoderRailcom_send_error_report(uint8_t error_code, bool additional)` | Send Ch2 Error response (ID6, 12-bit). Error codes per Table 50 (0x00=none, 0x01=invalid cmd, 0x02=current high, 0x03=voltage low, 0x04=fuse, 0x05=temperature, 0x06=feedback error, 0x07=manual adjust, 0x10=lantern failure, 0x20=servo failure, 0x3F=internal error). `additional`=true for subsequent errors in multi-error reports. |

**Shared with mobile decoders (Phase 6, ifdef guard widened):**
- `send_pom_response` — same ID0 format, accessory decoder POM (Section 7.2)
- `send_dynamic_data` — same ID7 format, accessory DV table (Table 52, CVs 321–384)
- `send_ack` / `send_nack` — same control words
- `send_raw` — escape hatch for XPOM responses (IDs 8–11)

### 10.2 SRQ State Machine (Decoder Side)

The SRQ protocol requires persistent state across multiple cutout windows.

**States:**

| State | Behavior |
|---|---|
| IDLE | No pending SRQ. Normal operation. |
| SRQ_PENDING | Decoder has an update. Sends SRQ in Ch1 on every cutout following addressed control commands or NOP commands. Will not respond to other addressed commands. |
| SRQ_RESPONDING | Stop command received. Decoder sends Type 3 solicited response (status, error, time, DYN, or POM) in Ch2 of the cutout following the stop command, then returns to IDLE. |

**Transitions:**
1. User calls `send_srq()` → IDLE → SRQ_PENDING
2. Library receives addressed stop command → SRQ_PENDING → SRQ_RESPONDING
3. Library sends queued Type 3 response in Ch2 → SRQ_RESPONDING → IDLE
4. After transaction complete, if decoder has another update → user calls `send_srq()` again

**SRQ via NOP (Section 7.2):** Decoder only responds to NOP if its address ≤ NOP threshold address. The library compares internally and suppresses the SRQ if address > threshold.

**Errors application special case (Section 7.2):** When responding to NOP, only the Errors application (ID6) may send its Ch2 update in the same cutout as the SRQ. All other applications must wait for the stop command.

### 10.3 CS-Side Changes

#### New Callbacks in `dcc_config.h`

| Req | Proposed | Why the User Needs It |
|---|---|---|
| ◐ | `on_accessory_srq(uint16_t address, bool is_extended)` | Notifies when an accessory decoder SRQ is detected in Ch1. The user should respond by sending a stop command (`load_accessory_basic_stop` or `load_accessory_extended_stop`) to collect the decoder's update. |

**Note:** Accessory decoder Type 3 solicited responses (status, error, time, DYN) are delivered through the existing `on_railcom_datagram_result` callback with the appropriate datagram ID (ID3–ID7). The user identifies the response type from the datagram ID.

#### NOP Auto-Scheduling

- When the main track is powered on and accessory RailCom is enabled, the library auto-inserts NOP packets every ≤5 seconds per S-9.3.2 Section 6.3.3.
- NOP timing managed in `DccConfig_run()` using the existing 100ms timer tick.
- NOP collision arbitration: if SRQ collision is detected (garbled Ch1 data), the CS lowers the NOP threshold address and re-sends. This narrows the responder pool until a single decoder responds. Internal logic, transparent to the user.

#### New Packet Builders (added to Phase 2)

- `load_accessory_basic_stop()` — stop command for basic format decoder (Table 36)
- `load_accessory_extended_stop()` — stop command for extended format decoder (Table 36)

### 10.4 Test Coverage

- `DccApplicationAccessoryDecoderRailcom_send_srq()` — SRQ datagram format, basic vs extended address
- SRQ state machine — IDLE→SRQ_PENDING→SRQ_RESPONDING→IDLE transitions
- SRQ persistence — verify decoder keeps sending until stop command
- SRQ NOP threshold filtering — suppress SRQ when address > threshold
- `send_status()` / `send_status_extended()` — ID4 datagram encoding, bit layout
- `send_status_4()` — ID3 datagram, 4 output pairs bit mapping (Table 47)
- `send_time_report()` — ID5 datagram, resolution bit
- `send_error_report()` — ID6 datagram, error codes, additional flag
- CS NOP auto-scheduling — verify NOP inserted within 5 seconds
- CS NOP collision arbitration — threshold adjustment on collision
- CS `on_accessory_srq` callback — SRQ detection and address extraction
- Stop command packet builders — basic and extended format
- Shared function ifdef guard widening — verify `send_pom_response`, `send_ack`, etc. compile under `DCC_COMPILE_ACCESSORY_DECODER`

---

## Open Questions

1. ~~**Decoder DCC input during RailCom cutout**~~ **RESOLVED** — The library calls `decoder_edge_irq_enable(false)` at the end bit before the cutout starts, and `decoder_edge_irq_enable(true)` after the cutout ends. The user can either disable the hardware interrupt or set a flag to skip calling the library — their choice. This prevents noise edges during the cutout (H-bridge settling, other decoders' RailCom transmissions) from corrupting the bit decoder.

2. ~~**RailCom cutout timing fix (CS and Decoder)**~~ **RESOLVED** — The current `DccRailcomCutout` state machine tristates the H-bridge at T_TS1 (~88µs) instead of T_CS (26-32µs). Per the spec (S-9.3.2 Figure 2, Table 1), the H-bridge should tristate at T_CS with the gap to T_TS1 being settling time before UART enable. Full state machine design documented below.

   **CS side — one-shot timer state machine (5 events):**

   | State | One-shot loaded for | ISR action |
   |---|---|---|
   | DELAY | `railcom_cutout_start_delay_us` (default 26µs) | — |
   | SETTLING | `railcom_uart_rx_delay_us` (default 54µs) | Tristate H-bridge |
   | CH1 | `railcom_ch1_window_us` (default 97µs) | Enable UART Rx |
   | GAP | `railcom_ch1_ch2_gap_us` (default 16µs) | Disable UART Rx |
   | CH2 | `railcom_ch2_window_us` (default 261µs) | Enable UART Rx |
   | IDLE | — | Disable UART Rx, restore H-bridge |

   All timing parameters are user-configurable in `dcc_config_t` to allow compensation for ISR latency on different platforms. Defaults match spec timing (T_CS=26µs, T_TS1=80µs, T_TC1=177µs, T_TS2=193µs, T_CE=454µs).

   **Decoder side — one-shot timer (2 events):**

   | State | One-shot loaded for | ISR action |
   |---|---|---|
   | CH1_WAIT | `railcom_tx_start_delay_us` (default 80µs) | — |
   | CH1_TX | `railcom_ch2_tx_delay_us` (default 113µs) | Load Ch1 bytes into UART Tx |
   | CH2_TX | — | Load Ch2 bytes into UART Tx (if any) |

   The decoder does not need to time T_CE — the byte count for each datagram type is known in advance and fits within the channel window by definition. Both delays are user-configurable.

   Ch1 timing margin: 97µs window, 2 bytes = ~82µs (including 2% UART tolerance), leaving ~15µs for ISR + UART setup.

3. ~~**CS RailCom UART enable/disable simplification**~~ **RESOLVED** — Simplified to `uart_rx_enable` / `uart_rx_disable` (2 callbacks instead of 4). The library calls them at T_TS1/T_TC1/T_TS2/T_CE via the one-shot timer state machine. UART is disabled during the Ch1→Ch2 gap (16µs) because corrupted data could prevent reliable byte counting.

4. ~~**Accessory decoder RailCom**~~ **RESOLVED** — Fully planned in Phase 10. Includes: `dcc_application_accessory_decoder_railcom.h/c` module (SRQ, Status 1/4, Time, Error, DYN), SRQ state machine (IDLE→SRQ_PENDING→SRQ_RESPONDING→IDLE), CS-side NOP auto-scheduling with collision arbitration, `on_accessory_srq` callback, stop command packet builders, and shared function ifdef guard widening.

5. ~~**Track search and time datagram**~~ **RESOLVED** — Dedicated `send_track_search_response(address, seconds_since_powerup)` added to Phase 6 API. Library packs ADR1 + ADR2 + Time (ID14) into one Ch2 transmission.

6. ~~**CV automatic transfer (Section 6.7)**~~ **RESOLVED** — Dedicated `send_cv_auto_transfer(indexed_cv_address, value)` added to Phase 6 API. Library encodes 24-bit indexed CV address and data into ID12 36-bit datagram. Internal auto-send logic when XF3 is enabled remains future scope.

---

## Resolved Questions

1. **`begin_cutout` / `end_cutout`** — User-provided hardware drivers in `dcc_config.h` (`begin_railcom_cutout` / `end_railcom_cutout`). CS RailCom application module eliminated.
2. **`timer_set_period` in `dcc_output_hw_t`** — Remove from config struct. Dead code from removed variable-period architecture.
3. **`DccPacketEncoder_idle()` placement** — Static in the `.c` file. Not user-facing.
