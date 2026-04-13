# Application API Refactor Working Plan

**Date:** 2026-04-13  
**Status:** Planning  

## Goal

Reorganize the library's user-facing API into a consistent set of `dcc_application_*.h/c` modules so that users have a clear, well-named surface for every task. Internal implementation stays in existing modules; application modules either move or stub-wrap the internals.

**Legend:** `●` = required, `◐` = required if feature enabled, `○` = optional

---

## Module Overview

| Module | Status | Strategy |
|---|---|---|
| `dcc_config.h` | Exists — needs renames, removals, and new hardware driver | Modify in place |
| `dcc_application_command_station.h/c` | **New** | Move/rename from `dcc_packet_encoder` |
| `dcc_application_main_track.h/c` | Exists — needs renames and `insert()` split | Modify in place |
| `dcc_application_service_track.h/c` | Exists — needs renames | Modify in place |
| `dcc_application_railcom.h/c` | **New** | Stub wrappers around `dcc_railcom_decoder` |
| `dcc_application_decoder.h/c` | **New** | Stub wrappers around `dcc_cv_storage` + `dcc_railcom_encoder` |

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
| `on_datagram` (in `dcc_railcom_hw_t`) | `on_railcom_datagram` |
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
| ◐ | `DccConfig_railcom_cutout_timer_isr()` | `DccConfig_railcom_oneshot_timer_isr()` | Call from RailCom one-shot timer ISR — drives cutout state machine through delay, Ch1, and Ch2 phases |
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
| ○ | `on_datagram` (in `dcc_railcom_hw_t`) | `on_railcom_datagram` | Notifies when a RailCom datagram has been decoded — delivers address, channel, and decoded data |

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

---

## Phase 2: `dcc_application_command_station.h/c` (New)

**Strategy:** Move/rename from `dcc_packet_encoder.h/c`. Pure computational, stateless — the entire file moves as-is including all private helpers and lookup tables.

**ifdef guard:** `DCC_COMPILE_COMMAND_STATION`

**Removed from public API:**
- `DccPacketEncoder_idle()` — internal, scheduler sends automatically

**All functions renamed from `DccPacketEncoder_*` to `DccApplicationCommandStation_load_*`:**

| Req | Current | Proposed | Why the User Needs It |
|---|---|---|---|
| ○ | `DccPacketEncoder_reset()` | `DccApplicationCommandStation_load_reset()` | Load a broadcast reset into a packet — all decoders clear volatile state and return to power-up |
| ○ | `DccPacketEncoder_estop_all()` | `DccApplicationCommandStation_load_estop_all()` | Load a broadcast emergency stop into a packet — panic stop every decoder on the track |
| ○ | `DccPacketEncoder_speed_128()` | `DccApplicationCommandStation_load_speed_128()` | Load a 128-step speed and direction command into a packet |
| ○ | `DccPacketEncoder_speed_28()` | `DccApplicationCommandStation_load_speed_28()` | Load a 28-step speed and direction command into a packet |
| ○ | `DccPacketEncoder_speed_14()` | `DccApplicationCommandStation_load_speed_14()` | Load a 14-step speed, direction, and headlight command into a packet |
| ○ | `DccPacketEncoder_func_group_1()` | `DccApplicationCommandStation_load_func_group_1()` | Load a function group 1 command into a packet (FL, F1–F4) |
| ○ | `DccPacketEncoder_func_group_2a()` | `DccApplicationCommandStation_load_func_group_2a()` | Load a function group 2a command into a packet (F5–F8) |
| ○ | `DccPacketEncoder_func_group_2b()` | `DccApplicationCommandStation_load_func_group_2b()` | Load a function group 2b command into a packet (F9–F12) |
| ○ | `DccPacketEncoder_func_f13_f20()` | `DccApplicationCommandStation_load_func_f13_f20()` | Load an expansion function command into a packet (F13–F20) |
| ○ | `DccPacketEncoder_func_f21_f28()` | `DccApplicationCommandStation_load_func_f21_f28()` | Load an expansion function command into a packet (F21–F28) |
| ○ | `DccPacketEncoder_func_f29_f36()` | `DccApplicationCommandStation_load_func_f29_f36()` | Load an expansion function command into a packet (F29–F36) |
| ○ | `DccPacketEncoder_func_f37_f44()` | `DccApplicationCommandStation_load_func_f37_f44()` | Load an expansion function command into a packet (F37–F44) |
| ○ | `DccPacketEncoder_func_f45_f52()` | `DccApplicationCommandStation_load_func_f45_f52()` | Load an expansion function command into a packet (F45–F52) |
| ○ | `DccPacketEncoder_func_f53_f60()` | `DccApplicationCommandStation_load_func_f53_f60()` | Load an expansion function command into a packet (F53–F60) |
| ○ | `DccPacketEncoder_func_f61_f68()` | `DccApplicationCommandStation_load_func_f61_f68()` | Load an expansion function command into a packet (F61–F68) |
| ○ | `DccPacketEncoder_accessory_basic()` | `DccApplicationCommandStation_load_accessory_basic()` | Load a basic accessory command into a packet (turnouts, stationary decoders) |
| ○ | `DccPacketEncoder_accessory_extended()` | `DccApplicationCommandStation_load_accessory_extended()` | Load an extended accessory command into a packet (signal aspects) |
| ○ | `DccPacketEncoder_accessory_basic_cv_write()` | `DccApplicationCommandStation_load_accessory_basic_cv_write()` | Load a basic accessory POM CV write command into a packet |
| ○ | `DccPacketEncoder_accessory_basic_cv_verify()` | `DccApplicationCommandStation_load_accessory_basic_cv_verify()` | Load a basic accessory POM CV verify command into a packet |
| ○ | `DccPacketEncoder_accessory_basic_cv_bit()` | `DccApplicationCommandStation_load_accessory_basic_cv_bit()` | Load a basic accessory POM CV bit write/verify command into a packet |
| ○ | `DccPacketEncoder_accessory_extended_cv_write()` | `DccApplicationCommandStation_load_accessory_extended_cv_write()` | Load an extended accessory POM CV write command into a packet |
| ○ | `DccPacketEncoder_accessory_extended_cv_verify()` | `DccApplicationCommandStation_load_accessory_extended_cv_verify()` | Load an extended accessory POM CV verify command into a packet |
| ○ | `DccPacketEncoder_accessory_extended_cv_bit()` | `DccApplicationCommandStation_load_accessory_extended_cv_bit()` | Load an extended accessory POM CV bit write/verify command into a packet |
| ○ | `DccPacketEncoder_cv_write_ops()` | `DccApplicationCommandStation_load_cv_write_pom()` | Load a locomotive POM CV write command into a packet |
| ○ | `DccPacketEncoder_cv_verify_ops()` | `DccApplicationCommandStation_load_cv_verify_pom()` | Load a locomotive POM CV verify command into a packet |
| ○ | `DccPacketEncoder_cv_bit_ops()` | `DccApplicationCommandStation_load_cv_bit_pom()` | Load a locomotive POM CV bit write/verify command into a packet |
| ○ | `DccPacketEncoder_consist_set()` | `DccApplicationCommandStation_load_consist_set()` | Load a consist address assignment command into a packet |
| ○ | `DccPacketEncoder_consist_clear()` | `DccApplicationCommandStation_load_consist_clear()` | Load a consist address clear command into a packet |
| ○ | `DccPacketEncoder_binary_state_short()` | `DccApplicationCommandStation_load_binary_state_short()` | Load a short-form binary state command into a packet (states 0–127) |
| ○ | `DccPacketEncoder_binary_state_long()` | `DccApplicationCommandStation_load_binary_state_long()` | Load a long-form binary state command into a packet (states 0–32767) |
| ○ | `DccPacketEncoder_analog_function()` | `DccApplicationCommandStation_load_analog_function()` | Load an analog function command into a packet (volume, dimming) |
| ○ | `DccPacketEncoder_speed_restriction()` | `DccApplicationCommandStation_load_speed_restriction()` | Load a speed restriction command into a packet (slow zones, block signaling) |

**Note:** `DccPacketEncoder_idle()` must remain accessible internally for the scheduler. It stays as a static/internal function in the .c file but is not declared in the public header.

---

## Phase 3: `dcc_application_main_track.h/c` Changes

**ifdef guard:** `DCC_COMPILE_COMMAND_STATION`

**Remove from public API:**
- `DccApplicationMainTrack_initialize()` — wired internally by `dcc_config.c`

**Split `insert()` into two functions and rename:**

| Req | Current | Proposed | Why the User Needs It |
|---|---|---|---|
| ● | `DccApplicationMainTrack_power_on()` | `DccApplicationMainTrack_power_on()` | Turn on the main track, start DCC signal generation |
| ● | `DccApplicationMainTrack_power_off()` | `DccApplicationMainTrack_power_off()` | Turn off the main track, stop DCC signal |
| ○ | `DccApplicationMainTrack_insert()` | `DccApplicationMainTrack_send_packet()` | Send a packet once — packet data is copied, caller's packet does not need to persist |
| ○ | *(split from insert w/ auto_refresh=true)* | `DccApplicationMainTrack_add_to_auto_refresh()` | Add a packet to the auto refresh cycle — packet data is copied, caller's packet does not need to persist |
| ○ | `DccApplicationMainTrack_remove_address()` | `DccApplicationMainTrack_remove_from_auto_refresh()` | Stop auto refreshing a specific address |
| ○ | `DccApplicationMainTrack_clear()` | `DccApplicationMainTrack_remove_all_auto_refresh()` | Remove all packets from the auto refresh cycle |

---

## Phase 4: `dcc_application_service_track.h/c` Changes

**ifdef guard:** `DCC_COMPILE_COMMAND_STATION` (plus per-mode `DCC_COMPILE_SERVICE_MODE_*` guards)

**Remove from public API:**
- `DccApplicationServiceTrack_initialize()` — wired internally by `dcc_config.c`

**Renames and full API:**

| Req | Current | Proposed | Why the User Needs It |
|---|---|---|---|
| ◐ | `DccApplicationServiceTrack_power_on()` | `DccApplicationServiceTrack_power_on()` | Power the programming track |
| ◐ | `DccApplicationServiceTrack_power_off()` | `DccApplicationServiceTrack_power_off()` | Kill programming track power |
| ◐ | `DccApplicationServiceTrack_enter()` | `DccApplicationServiceTrack_enter_service_mode()` | Begin exclusive programming session on the service track |
| ◐ | `DccApplicationServiceTrack_exit()` | `DccApplicationServiceTrack_exit_service_mode()` | End programming session, shut down service track |
| ◐ | `DccApplicationServiceTrack_is_active()` | `DccApplicationServiceTrack_is_programming_active()` | Check if a programming operation is still in progress |
| ◐ | `DccApplicationServiceTrack_direct_write_byte()` | `DccApplicationServiceTrack_direct_write_byte()` | Write a CV byte using direct mode — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_direct_verify_byte()` | `DccApplicationServiceTrack_direct_verify_byte()` | Verify/read a CV byte using direct mode — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_direct_write_bit()` | `DccApplicationServiceTrack_direct_write_bit()` | Write a single CV bit using direct mode — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_direct_verify_bit()` | `DccApplicationServiceTrack_direct_verify_bit()` | Verify/read a single CV bit using direct mode — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_paged_write()` | `DccApplicationServiceTrack_paged_write()` | Write a CV using paged mode — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_paged_verify()` | `DccApplicationServiceTrack_paged_verify()` | Verify a CV using paged mode — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_register_write()` | `DccApplicationServiceTrack_register_write()` | Write registers 1–8 (legacy) — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_register_verify()` | `DccApplicationServiceTrack_register_verify()` | Verify registers 1–8 (legacy) — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_address_write()` | `DccApplicationServiceTrack_address_write()` | Write CV1 only (simplest mode) — result delivered via on_service_mode_result callback |
| ◐ | `DccApplicationServiceTrack_address_verify()` | `DccApplicationServiceTrack_address_verify()` | Verify CV1 only (simplest mode) — result delivered via on_service_mode_result callback |

---

## Phase 5: `dcc_application_railcom.h/c` (New)

**Strategy:** Stub wrappers around `dcc_railcom_decoder.c`. Internal state, circular buffer, ISR volatile flags, and decode table stay in the existing module.

**ifdef guard:** `DCC_COMPILE_COMMAND_STATION`

| Req | Current | Proposed | Why the User Needs It |
|---|---|---|---|
| ◐ | `DccRailcomDecoder_run()` | `DccApplicationRailcom_run()` | Process received RailCom bytes — call from main loop |
| ◐ | `DccRailcomDecoder_begin_cutout()` | `DccApplicationRailcom_begin_cutout()` | Mark start of RailCom cutout window — called from ISR context by bit encoder |
| ◐ | `DccRailcomDecoder_end_cutout()` | `DccApplicationRailcom_end_cutout()` | Mark end of cutout window — called from ISR context by bit encoder |
| ○ | `DccRailcomDecoder_read()` | `DccApplicationRailcom_read_datagram()` | Pop the next decoded RailCom datagram from the buffer — returns false if empty |
| ○ | `DccRailcomDecoder_available()` | `DccApplicationRailcom_datagrams_available()` | Check how many decoded RailCom datagrams are waiting to be read |
| ○ | `DccRailcomDecoder_decode_byte()` | `DccApplicationRailcom_decode_byte()` | Manually decode a raw RailCom 4/8 encoded byte to its 6-bit value |

**Not exposed:** `DccRailcomDecoder_initialize()` — wired internally by `dcc_config.c`

---

## Phase 6: `dcc_application_decoder.h/c` (New)

**Strategy:** Stub wrappers around `dcc_cv_storage.c` and `dcc_railcom_encoder.c`. Both underlying modules hold interface pointers for hardware callbacks.

**ifdef guard:** `DCC_COMPILE_DECODER`

| Req | Current | Proposed | Why the User Needs It |
|---|---|---|---|
| ○ | `DccCvStorage_read()` | `DccApplicationDecoder_cv_read()` | Read a CV value from the decoder's persistent storage |
| ○ | `DccCvStorage_write()` | `DccApplicationDecoder_cv_write()` | Write a CV value — enforces decoder lock (CV15 must equal CV16) |
| ○ | `DccCvStorage_is_locked()` | `DccApplicationDecoder_is_locked()` | Check if the decoder is locked against configuration changes (CV15 != CV16) |
| ◐ | `DccRailcomEncoder_encode_byte()` | `DccApplicationDecoder_railcom_encode_byte()` | Encode a 6-bit value to an 8-bit RailCom 4/8 DC-balanced codeword |
| ◐ | `DccRailcomEncoder_send_ch1()` | `DccApplicationDecoder_railcom_send_ch1()` | Send a Channel 1 RailCom response during cutout window (address feedback) |
| ◐ | `DccRailcomEncoder_send_ch2()` | `DccApplicationDecoder_railcom_send_ch2()` | Send a Channel 2 RailCom response during cutout window (CV data, diagnostics) |

**Not exposed:** `DccCvStorage_initialize()` and `DccRailcomEncoder_initialize()` — wired internally by `dcc_config.c`

---

## Phase 7: Update `dcc_config.c` Wiring

- Wire new application module initializes internally during `DccConfig_initialize()`
- Keep `DccPacketEncoder_idle()` accessible internally for the scheduler's `build_idle_packet` interface
- Implement 6ms ACK pulse timing: `start_ack_pulse` called when ACK needed, poll `get_timestamp_usec` in `DccConfig_run()`, call `stop_ack_pulse` after 6ms
- Remove per-channel timer wiring (`half_bit_isr`, `timer_set_period` usage)

---

## Phase 8: Update Tests

- Update all test files referencing renamed functions
- Add tests for new `send_packet()` / `add_to_auto_refresh()` split
- Add tests for 6ms ACK pulse timing
- Verify `DccPacketEncoder_idle()` remains functional internally via scheduler tests

---

## Phase 9: Update Application Code

- Update `applications/ti_theia/mspm03507_launchpad/command_station/` to use new function names
- Replace `DccConfig_shared_timer_isr()` with `DccConfig_58us_timer_isr()`
- Replace `fire_ack_pulse` with `start_ack_pulse` / `stop_ack_pulse` pair

---

## Open Questions

1. **`begin_cutout` / `end_cutout` in railcom** — are these user-facing or internal (called by bit encoder ISR)? If internal, they should not be in `dcc_application_railcom.h`.
2. **`timer_set_period` in `dcc_output_hw_t`** — only used by the removed variable-period architecture. Should it be removed from the config struct?
3. **`DccPacketEncoder_idle()` placement** — keep as static in `dcc_application_command_station.c`, or move to a separate internal header?
