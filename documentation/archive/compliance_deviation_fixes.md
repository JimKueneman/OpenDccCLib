# Working Plan — Compliance Deviation Fixes

> **ARCHIVED — executed record.** All six deviation fixes shipped; kept for history.

**Date:** 2026-06-22
**Status:** ✅ EXECUTED 2026-06-22 — all six items implemented; full test suite builds clean, 923 tests pass.

> **Follow-on / known defect surfaced during this work:** the decoder-side RailCom **Tx** path
> is not wired to hardware — `dcc_config.c` leaves the encoder's `uart_write` permanently NULL,
> and there is no byte→pin bridge or decoder-side Tx one-shot timer (archived Open-Question-#2).
> The encoder + response API are unit-tested with mocks but transmit nothing in a real build.
> This is a larger design issue, deferred. Tracked in [../ComplianceOverview.md](../compliance/ComplianceOverview.md) →
> "Decoder-side RailCom Tx integration."

## Goal

Fix the **"implemented but wrong"** findings from [../ComplianceOverview.md](../compliance/ComplianceOverview.md) —
features that exist in the code but deviate from the NMRA spec. The **not-implemented**
features (fail-safe/CV11, XPOM, Time/Date, accessory NOP, indexed CVs, Logon) are new
functionality and are **out of scope** for this plan; they belong in a separate roadmap.

Source of truth for spec facts: [../specs/DCC_Spec_Reference.md](../specs/DCC_Spec_Reference.md)
(verified claim-by-claim against the NMRA PDFs).

## Ground rules

1. **Feature-flag gating:** every new field/function wrapped in the correct `DCC_COMPILE_*`
   guard. The relevant flag is stated per item below and must be confirmed before writing code.
2. **Interface structs only:** no core module `#include`s another module's header to call it;
   all cross-module calls go through the `interface_dcc_*_t` function-pointer structs wired in
   `dcc_config.c`.
3. **Tests alongside each fix.** Where a corrected constant invalidates an existing test
   expectation, update the expectation to the spec value (this is legitimate, not a force-pass).
   If a test that should pass fails, stop and report rather than working around it.
4. **One item at a time, explicit approval before each.** No batching across modules.

## Decisions (locked)

| Item | Decision |
|---|---|
| RailCom cutout | **Full 5-state machine + user-configurable timing** (per archived Open-Question-#2 design) |
| Bit encoder double-buffer | **Documentation fix only** (code stays single-buffered) |
| Speed restriction `00111110` | **Remove** (byte is reserved for Zimo East-West in S-9.2.1) |
| RailCom decoder-response datagram IDs | **Align to the 2026 draft S-9.3.2 taxonomy** |

---

## Suggested sequence

Land the small, self-contained fixes first; do the cutout rework last (it changes the public
`dcc_config_t` and the most tests).

**P3.4 (doc) → P3.5 (remove speed restriction) → P1.2 (extended SRQ) → P2.3 (ACK upper bound) → P3.6 (datagram IDs) → P1.1 (cutout rework).**

---

## P3.4 — Bit encoder "double-buffered" doc mismatch  *(doc only)*

- **Problem:** `dcc_bit_encoder.h` comment claims double-buffering; the code uses a single
  `active_packet` + `packet_loaded` flag (no front/back swap).
- **Fix:** correct the header comment to describe the actual single-buffer + reload design.
- **Files / flags:** `dcc_bit_encoder.h` only (`DCC_COMPILE_COMMAND_STATION`). No code, no test change.

## P3.5 — Remove speed restriction (`00111110`)

- **Problem:** code encodes/decodes a payload for `00111110`, but S-9.2.1 §2.3.2.2 reserves
  that byte for the (undeveloped) Zimo East-West Direction proposal — no defined payload.
- **Fix:** remove the encode path, the decode dispatch, and the callback; leave `00111110`
  reserved (note it in the matrix).
- **Files / flags:**
  - `dcc_application_command_station_packet.c` + legacy `dcc_packet_encoder.c`: remove
    `..._speed_restriction` / `DccPacketEncoder_speed_restriction` (`DCC_COMPILE_COMMAND_STATION`).
  - `dcc_packet_decoder.c`: remove the `DCC_ADV_OPS_SPEED_RESTRICTION` dispatch
    (`DCC_COMPILE_DECODER`).
  - `dcc_config.h/.c`: remove `on_speed_restriction_command` wiring.
  - `dcc_defines.h`: remove `DCC_ADV_OPS_SPEED_RESTRICTION`.
- **Tests:** delete the `speed_restriction*` cases in the encoder and packet-decoder test files.

## P1.2 — Accessory extended SRQ drops high address bits

- **Problem:** in `dcc_application_accessory_decoder_railcom.c` the `is_extended` and basic
  branches send the identical Ch1 payload (`_srq_address & 0xFF`); high bits are dropped. The
  existing test only checks the low byte, so it passes despite the bug.
- **Fix:** encode the extended-format SRQ address per S-9.3.2 §4.20 (the SRQ datagram MSB
  selects basic vs extended; transmit the full address bits for each format).
- **Files / flags:** `dcc_application_accessory_decoder_railcom.c` + its test
  (`DCC_COMPILE_ACCESSORY_DECODER`).
- **Tests:** add a case asserting the high address bits for an extended SRQ (the current
  `on_cutout_pending_sends_ch1_extended` uses `0x03FF` but only checks `0xFF`).

## P2.3 — ACK upper-bound never enforced  *(verify spec first)*

- **Problem:** `USER_DEFINED_DCC_ACK_MAX_DURATION_US` is required (validated in `dcc_types.h`)
  but `dcc_service_mode_common.c` only checks threshold + min-samples — an overlong pulse is
  still accepted as a valid ACK.
- **Step 0 (verify):** confirm against the S-9.2.3 PDF whether an overlong current pulse should
  be *rejected as ACK* or treated as overcurrent. Report the finding before coding.
- **Fix (if rejection is correct):** derive `DCC_ACK_MAX_SAMPLES` from `MAX_DURATION_US` and
  bound the detector so a pulse that never releases is not counted as an ACK.
- **Files / flags:** `dcc_service_mode_common.c` + `dcc_service_mode_common_Test.cxx`
  (`DCC_COMPILE_COMMAND_STATION`).

## P3.6 — RailCom decoder-response datagram IDs → align to 2026 draft

- **Problem:** `dcc_application_decoder_railcom.c` sends ACK/NACK as datagram **ID15** and reuses
  ID2 for track search; per released S-9.3.2, ID15 is "not approved" and ACK/NACK are 4/8
  *special codes*, not datagrams. The CV-auto-transfer ID is a magic literal `12`.
- **Fix:** reconcile the response paths and the `DCC_RAILCOM_ID_*` defines to the **2026 draft**
  taxonomy (S-9.3.2 §4.7): proper ACK/NACK handling, correct ID assignments, named constant for
  ID12. Document the chosen target at the top of the module.
- **Files / flags:** `dcc_defines.h` (`DCC_RAILCOM_ID_*`), `dcc_application_decoder_railcom.c`,
  its test (`DCC_COMPILE_DECODER` / `DCC_COMPILE_ACCESSORY_DECODER`).

## P1.1 — RailCom cutout: full 5-state machine + configurable timing  *(largest; do last)*

- **Problem:** `dcc_defines.h` uses `DCC_RAILCOM_CUTOUT_START_US=88 / CH1_WINDOW_US=464 /
  TOTAL_US=1544`. Spec (released **and** draft) is T_CS 26–32, Ch1 80→≤177, Ch2 193→≤454, total
  ~454–488 µs — the 1544 µs cutout is ~3× too long and non-compliant. The state machine is
  4-state (IDLE/DELAY/CH1/CH2) and omits the settling and Ch1→Ch2 gap phases; it also tristates
  the H-bridge at ~T_TS1 instead of T_CS.

- **Fix — CS-side one-shot state machine** (per archived `application_api_refactor.md`
  Open-Question-#2 design):

  | State | One-shot loaded for | ISR action on fire |
  |---|---|---|
  | DELAY | `railcom_cutout_start_delay_us` (default 26 µs) | tristate H-bridge (begin cutout) |
  | SETTLING | `railcom_uart_rx_delay_us` (default 54 µs) | enable UART Rx |
  | CH1 | `railcom_ch1_window_us` (default 97 µs) | disable UART Rx |
  | GAP | `railcom_ch1_ch2_gap_us` (default 16 µs) | enable UART Rx |
  | CH2 | `railcom_ch2_window_us` (default 261 µs) | disable UART Rx, restore H-bridge → IDLE |

  Defaults reproduce the spec event times (T_CS 26, T_TS1 80, T_TC1 177, T_TS2 193, T_CE 454 µs).
  (Exact state/action edges to be finalized against the archived design table during
  implementation.)

- **New user-configurable fields** in `dcc_config_t` (or the `dcc_railcom_hw_t` sub-struct),
  defaults = spec values, gated by `DCC_COMPILE_COMMAND_STATION`: the five timing values above.
  These let platforms compensate for ISR latency.

- **Files / flags:** `dcc_railcom_cutout.c/.h`, `dcc_defines.h` (retire/redefine the three
  hard-coded constants), `dcc_config.h` (new fields), `dcc_config.c` (wiring). All
  `DCC_COMPILE_COMMAND_STATION`. Decoder-side Tx timing (2-event one-shot, `DCC_COMPILE_DECODER`)
  is a documented follow-on, not part of this item.

- **Tests:** rewrite `dcc_railcom_cutout_Test.cxx` (`timer_periods_are_correct`, sequence and
  cancel tests) to the new event times; add settling + gap coverage.

- **Downstream:** because `dcc_config_t` gains fields, update the MSPM0 example
  (`applications/ti_theia/mspm03507_launchpad/command_station/command_station.c`) and the
  `dcc_user_config.h` templates (`templates/typical/`, `test/user_config/typical/`) to populate
  the new fields with defaults. Include this in the same change.

---

## Out of scope (tracked elsewhere)

- Not-implemented features → see [../ComplianceOverview.md](../compliance/ComplianceOverview.md) backlog: fail-safe/CV11, XPOM,
  Time/Date & System Time, accessory NOP encoder, indexed CVs (CV31/32), factory-reset-to-defaults,
  CV29 bits 2/3/4/7 behavior, Logon/Data Spaces.
- Retiring the duplicate pre-refactor modules (`dcc_packet_encoder`, `dcc_application_service_track`,
  `dcc_application_main_track`) — housekeeping, separate change.
