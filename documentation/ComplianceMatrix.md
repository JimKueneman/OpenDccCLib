# DCC Compliance Matrix

> **Generated 2026-06-22.** Maps each DCC feature area to the released NMRA standard, the
> draft revision (where applicable), and the library's implementation + test coverage.
> Scope: DCC core (S-9.1, S-9.2, S-9.2.1, S-9.2.1.1, S-9.2.2, S-9.2.3, S-9.2.4, S-9.3.2).
> Granularity: feature-area. Spec facts come from the validated [DCC_Spec_Reference.md](specs/DCC_Spec_Reference.md)
> and [DCC_Draft_Deltas.md](specs/DCC_Draft_Deltas.md); code/test facts from a sweep of
> `src/dcc/` (verified against the source, commit `c4dc61e`).

**Legend:** ✅ implemented + tested · ⚠️ implemented but spec-deviation/partial · ❌ not implemented · — n/a
All source paths are under `src/dcc/`. Test files share the source dir (`*_Test.cxx`).

---

## Summary

- **Strong, fully-tested core:** packet encoding (speed/function/accessory/CV-POM/consist/binary-state/analog), scheduler, bit encoder, all four service modes, bit/packet decoder, CV storage, RailCom 4/8 encode+decode, decoder-side RailCom responses, accessory-decoder RailCom. ~921 unit tests, ~99% line coverage.
- **Compliance deviations — ALL FIXED 2026-06-22** (see [archive/compliance_deviation_fixes.md](archive/compliance_deviation_fixes.md); 923 tests pass):
  - ✅ **RailCom cutout** rebuilt as a 5-state machine (DELAY/SETTLING/CH1/GAP/CH2) with spec timing (T_CS 26 / T_TS1 80 / T_TC1 177 / T_TS2 193 / T_CE 454 µs), now user-configurable via `dcc_config_t` (0 = spec default).
  - ✅ Bit-encoder comment corrected to "single-buffered" (matches the code).
  - ✅ ACK detection now enforces the upper bound (`DCC_ACK_MAX_SAMPLES`); an over-long pulse is rejected as over-current, not an ACK.
  - ✅ Accessory **extended SRQ** now transmits the full address (basic ≠ extended).
  - ✅ Speed-restriction (`00111110`) **removed** (byte reserved for Zimo East-West).
  - ✅ RailCom decoder-response **datagram IDs aligned to the 2026 draft**; ACK/NACK now sent as 4/8 special code words; CS-side decode-table bug fixed (`0x0F`→ACK, `0x3C`→NACK).
- **Top functional gaps (not implemented):**
  - ❌ **Fail-safe / CV11 packet timeout (S-9.2.4)** — only dead callback declarations + an unused `#define`.
  - ❌ **XPOM** (S-9.2.1 §2.3.7.4) — no command-station encoder.
  - ❌ **Time/Date & System Time** packets (S-9.2.1) — unused `#define` only.
  - ❌ **Accessory NOP** packet encoder — only auto-schedule comments.
  - ❌ **Indexed CVs (CV31/32)** decoder paging — defines only.
  - ❌ **Factory reset (CV8)** — write-permission exception only, no reset-to-defaults logic.
  - ❌ **Logon / Data Spaces (S-9.2.1.1)** — absent (draft-only; out of current scope).
- **Housekeeping:** old pre-refactor modules (`dcc_packet_encoder`, `dcc_application_service_track`, `dcc_application_main_track`) still coexist with the role-first replacements; both are compiled and tested.

---

## 1. Electrical / Signal (S-9.1)

| Feature | Released | Draft | Implemented | Tested | Status |
|---|---|---|---|---|---|
| One-bit timing 58 µs (55–61) | S-9.1 Tbl 2.1 | — | `dcc_defines.h: DCC_ONE_BIT_HALF_PERIOD_US=58` | bit-encoder tick tests | ✅ |
| Zero-bit 95 min/100 nom, max 12000 µs | S-9.1 Tbl 2.1 | — | `dcc_defines.h: DCC_ZERO_BIT_HALF_PERIOD_US=100`, `_MAX_TOTAL_DURATION_US=12000` | `dcc_config_Test :: DccDefines` | ✅ |
| Preamble ≥14 (CS) / ≥10 (decoder accept) | S-9.1/S-9.2 | — | encoder `DCC_PREAMBLE_BITS_OPS=14`; decoder `_DECODER_MIN=10` | `tick_preamble_is_all_one_bits`; `preamble_exactly_10_produces_packet` | ✅ |
| Asymmetric/stretched zero accepted | S-9.1 | — | `dcc_bit_decoder.c:DccBitDecoder_edge` | `asymmetric_zero_accepted` | ✅ |

## 2. Packet Format (S-9.2)

| Feature | Released | Draft | Implemented | Tested | Status |
|---|---|---|---|---|---|
| Preamble/start/data/end framing + XOR | S-9.2 | — | encode `dcc_bit_encoder.c`; decode `dcc_bit_decoder.c` + `dcc_packet_decoder.c:_validate_xor` | bit-encoder (11), `bad_xor_ignored` | ✅ |
| Idle packet | S-9.2 | — | `..._packet.c:CSPacket_idle` | `idle_packet_*` | ✅ |
| Reset packet | S-9.2 | — | `..._packet.c:CSPacket_reset` | `reset_packet_*` | ✅ |
| Broadcast e-stop | S-9.2 | — | `..._packet.c:CSPacket_estop_all` | `estop_all_*` | ✅ |
| Broadcast normal stop (S=0) | S-9.2 | — | no dedicated function (only e-stop) | — | ⚠️ partial |
| Address ranges / matching | S-9.2 + S-9.2.1 | — | `dcc_packet_decoder.c:process_packet` | `matching_address_dispatched`, broadcast/long tests | ✅ |

## 3. Extended Packets — Multifunction (S-9.2.1)

| Feature | Released | Draft | Implemented | Tested | Status |
|---|---|---|---|---|---|
| Speed 14 / 28 / 128-step | S-9.2.1 §2.3.2-3 | S-9.2.1 (128-step clarified) | `CSPacket_speed_14/_28/_128`; decode `_dispatch_speed_*` | 25+ encode, 18+ decode | ✅ |
| Function group 1 / 2a / 2b | S-9.2.1 §2.3.4-5 | — | `CSPacket_func_group_1/_2a/_2b` | `func_group*` | ✅ |
| Function expansion F13–F68 | S-9.2.1 §2.3.6 | — | `CSPacket_func_f13_f20 … _f61_f68` | `func_f13_f20 … _f61_f68` | ✅ |
| Accessory basic / extended | S-9.2.1 §2.4 | notation `1AAADAAR` | `CSPacket_accessory_basic/_extended`; decode `_dispatch_accessory_*` | basic/extended encode+decode | ✅ |
| Accessory basic/extended **stop** | S-9.2.1 | Tbl 36 | `CSPacket_accessory_basic_stop/_extended_stop` | `accessory_*_stop_*` | ✅ (new module only) |
| Accessory **NOP** | S-9.2.1 §2.4.6 | NEW | ❌ no encoder (auto-sched comments only) | — | ❌ |
| CV ops-mode (POM) write/verify/bit | S-9.2.1 §2.3.7.3 | GG/F notation | `CSPacket_cv_write_pom/_verify_pom/_cv_bit_pom`; decode `_dispatch_cv_access` | `cv_write_ops_*`, `cv_bit_ops_*` | ✅ |
| **XPOM** (24-bit indexed, read/write/bit) | S-9.2.1 §2.3.7.4 | §1.6 | ❌ absent | — | ❌ |
| Consist set / clear | S-9.2.1 §2.3.1.4 | TTTT notation | `CSPacket_consist_set/_clear`; decode `_dispatch_instruction` | `consist_*` | ✅ |
| Binary state short / long | S-9.2.1 §2.3.6.1/.4 | moved here from 9.2.1.1 | `CSPacket_binary_state_short/_long`; decode `_dispatch_feature_expansion` | `binary_state_*` | ✅ |
| Analog function (`00111101`) | S-9.2.1 §2.3.2.3 | byte corrected | `CSPacket_analog_function`; decode `_dispatch_advanced_ops` | `analog_function*` | ✅ |
| Speed restriction (`00111110`) | — | reserved: Zimo East-West | **removed** | — | ✅ removed (byte left reserved) |
| Time/Date, System Time | S-9.2.1 §2.3.6.2/.3 | full layouts NEW | ❌ unused `#define DCC_FEAT_TIME_DATE` only | — | ❌ |

## 4. Advanced Extended / Logon (S-9.2.1.1)

| Feature | Released | Draft | Implemented | Tested | Status |
|---|---|---|---|---|---|
| Logon auto-registration, partitions 253/254 | S-9.2.1.1 | §2 (expanded) | ❌ no `logon` anywhere in `src/` | — | ❌ (draft-scope) |
| CRC-8 + XOR for >6-byte packets | S-9.2.1.1 | §2.4 | ❌ | — | ❌ |
| Data Spaces / WriteBlock / ReadBlock | S-9.2.1.1 | §2.11 | ❌ | — | ❌ |

## 5. Configuration Variables (S-9.2.2)

| Feature | Released | Draft | Implemented | Tested | Status |
|---|---|---|---|---|---|
| CV read/write via callbacks | S-9.2.2 | — | `dcc_cv_storage.c:DccCvStorage_read/_write` | `dcc_cv_storage_Test` (16) | ✅ |
| Decoder lock (CV15/16) | S-9.2.2 | CV16=0 disables | `DccCvStorage_is_locked` (+ app-layer dup) | lock tests | ✅ |
| Factory reset (CV8) | S-9.2.2 (mfr convention) | — | write-permission exception only; **no reset-to-defaults** | `factory_reset_allowed_when_locked` | ⚠️ partial |
| CV29 decode | S-9.2.2 | bit1 FL-loc, bit2 power, bit7 type | bits 0/1/5 only, inline in `_update_address_cv_cache` | `cv29_*` | ⚠️ bits 2/3/4/7 not acted on |
| Indexed CVs (CV31/32) | S-9.2.2 | page table | ❌ defines only, no paging | — | ❌ |
| Accessory CVs (513/521/540/541, addr split) | S-9.2.2 | CV15-18 added, linear/non-linear | `_dispatch_acc_cv_access` + `DCC_CV_ACC_*` | `acc_*_cv_*` (~20) | ✅ (core); CV15-18/linear-table not modeled |

## 6. Service Mode (S-9.2.3)

| Feature | Released | Draft | Implemented | Tested | Status |
|---|---|---|---|---|---|
| Long preamble ≥20 | S-9.2.3 | — | `DCC_PREAMBLE_BITS_SERVICE=20` | per-mode packet tests | ✅ |
| Common ACK detect / reset seq / retry | S-9.2.3 | — | `dcc_service_mode_common.c` | `..._common_Test` (53) | ✅ |
| ACK ≥60 mA for 6 ms ±1 ms | S-9.2.3 | — | threshold + min/max samples (`MIN/MAX_DURATION_US`) | ACK tests | ✅ window [MIN,MAX] enforced |
| Direct write/verify byte & bit | S-9.2.3 | — | `dcc_service_mode_direct.c` (4 fns) | `..._direct_Test` (30) | ✅ |
| Paged write/verify | S-9.2.3 | — | `dcc_service_mode_paged.c`; CV=`((cv-1)/4)+1`/`((cv-1)%4)+1` | `..._paged_Test` (22) | ✅ |
| Register write/verify | S-9.2.3 | — | `dcc_service_mode_register.c` | `..._register_Test` (16) | ✅ |
| Address-only write/verify | S-9.2.3 | — | `dcc_service_mode_address.c` | `..._address_Test` (20) | ✅ (recovery count hardcoded 10) |
| Decoder-side service-mode CV ops | S-9.2.3 | — | `dcc_packet_decoder.c:_dispatch_service_mode*` (+`service_mode` flag) | svc_direct/register/verify tests | ✅ |

## 7. Fail-Safe (S-9.2.4)

| Feature | Released | Draft | Implemented | Tested | Status |
|---|---|---|---|---|---|
| Packet-timeout → stop all (CV11) | S-9.2.4 §4 | — | ❌ **not implemented** | — | ❌ |
| CV11 timeout value (≥20 s max) | S-9.2.4 | — | ❌ `DCC_CV_PACKET_TIMEOUT=11` defined, referenced nowhere | — | ❌ |
| `on_failsafe_entered/exited` | (lib API) | — | ❌ callbacks declared but never wired or called | — | ❌ |

## 8. RailCom / Bi-Directional (S-9.3.2)

| Feature | Released | Draft | Implemented | Tested | Status |
|---|---|---|---|---|---|
| Cutout timing | S-9.3.2 Tbl 1 (T_CS 26–32, T_CE 454–488) | §4.1 (same) | `dcc_railcom_cutout.c`; defaults 26/54/97/16/261 µs, configurable via `dcc_config_t` | `..._cutout_Test` (25) | ✅ spec timing |
| Cutout state machine | S-9.3.2 | 5-phase (settling/gap) | 5-state (DELAY/SETTLING/CH1/GAP/CH2) | cutout tests | ✅ |
| 4/8 decode + Ch1/Ch2 + receive buffer | S-9.3.2 §2.5 | — | `dcc_railcom_decoder.c` | `..._decoder_Test` (27) | ✅ |
| 4/8 encode (decoder responses) | S-9.3.2 | — | `dcc_railcom_encoder.c:encode_byte` | `..._encoder_Test` (13) | ✅ |
| Address feedback ADR1/ADR2 alternation | S-9.3.2 §3.1 | §4.12 | `DccApplicationDecoderRailcom_send_address_feedback` | alternation tests | ✅ |
| POM / dynamic / ACK / NACK / track-search / CV-auto / raw responses | S-9.3.2 | §4.7 expanded | `dcc_application_decoder_railcom.c` (7 fns) | `..._decoder_railcom_Test` (21) | ✅ IDs aligned to 2026 draft; ACK/NACK as 4/8 special code words |
| CV28 / CV29 bit-3 RailCom enable | S-9.3.2 §4.2 | 8-bit CV28 | decoder reads CV29; CV28 bits not acted on | — | ⚠️ partial |
| Accessory SRQ + state machine | — | §4.20 | `dcc_application_accessory_decoder_railcom.c` | `..._accessory_..._Test` (27) | ✅ full address (basic ≠ extended) |
| Accessory Status 1 / Status 4 / Time / Error | — | §4.20 | `_send_status/_status_extended/_status_4/_time_report/_error_report` | accessory tests | ✅ (Status1 setpoint bit per draft) |
| NACK code word `00111100`; BUSY removed | — | §4.3 | ACK `0xF0` / NACK `0x3C` sent as 4/8 special code words | encoder/decoder tests | ✅ |

> ⚠️ **Decoder-side RailCom Tx is not hardware-wired (deferred design issue).** Every
> decoder/accessory RailCom *response* row above (POM, dynamic, ACK/NACK, track search,
> CV-auto, raw, SRQ, Status 1/4) is implemented and unit-tested at the datagram / 4-8 layer
> only. `dcc_config.c` leaves the encoder's `uart_write` NULL and there is no byte→pin bridge
> or decoder-side Tx timer, so **nothing is transmitted in a real build**. The ✅ marks reflect
> datagram-layer correctness + tests, not end-to-end transmission. See
> [STATUS.md](STATUS.md) → "Decoder-side RailCom Tx integration."

## Command-station main-track / scheduler (cross-cutting)

| Feature | Released | Draft | Implemented | Tested | Status |
|---|---|---|---|---|---|
| Scheduler priority / combining / auto-refresh | (lib design) | — | `dcc_scheduler.c` | `..._scheduler_Test` (22) | ✅ |
| Bit-encoder buffering | (lib design) | — | single `active_packet` + `packet_loaded` flag | — | ✅ doc corrected |

---

## Deviations & gaps — action list

**Spec-compliance fixes — DONE (2026-06-22):**
1. ✅ RailCom cutout retimed + 5-state machine + configurable (`dcc_defines.h`, `dcc_railcom_cutout.c/.h`, `dcc_config.h/.c`, MSPM0 app).
2. ✅ ACK upper bound enforced (`dcc_service_mode_common.c`).
3. ✅ Accessory extended SRQ transmits the full address.
4. ✅ Speed-restriction removed (`00111110` left reserved).
5. ✅ RailCom decoder-response datagram IDs aligned to the 2026 draft + CS-side decode-table fix.
6. ✅ Bit-encoder double-buffer doc corrected.

**Functional gaps (not implemented):**
5. Fail-safe / CV11 packet timeout (S-9.2.4) — wire `on_failsafe_entered/exited` and a timeout timer.
6. XPOM command-station encoder (S-9.2.1 §2.3.7.4).
7. Time/Date & System Time packet encoders (S-9.2.1).
8. Accessory NOP packet encoder (S-9.2.1 §2.4.6) — needed by the SRQ auto-scheduling that already references it.
9. Indexed-CV (CV31/32) decoder paging; factory-reset-to-defaults on CV8; CV29 bits 2/3/4/7 behavior.

**Out of current scope (draft-only):** Logon / Data Spaces (S-9.2.1.1), CRC-8 framing, NACK code word, SUSI, E24.

**Housekeeping:** retire the duplicate pre-refactor modules (`dcc_packet_encoder`, `dcc_application_service_track`, `dcc_application_main_track`) now that the role-first modules exist.

*Cross-references: [STATUS.md](STATUS.md) (pending-work backlog), [ARCHITECTURE.md](ARCHITECTURE.md) (module map).*
