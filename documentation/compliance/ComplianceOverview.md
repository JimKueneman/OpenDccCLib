# DCC Compliance Overview

> **Generated 2026-06-22.** Maps each DCC feature area to the released NMRA standard, the
> draft revision (where applicable), and the library's implementation + test coverage.
> Scope: DCC core (S-9.1, S-9.2, S-9.2.1, S-9.2.1.1, S-9.2.2, S-9.2.3, S-9.2.4, S-9.3.2).
> Granularity: feature-area. Spec facts come from the validated [DCC_Spec_Reference.md](../specs/DCC_Spec_Reference.md)
> (draft provenance now lives in compliance.data.js); code/test facts from a sweep of
> `src/dcc/` (verified against the source, commit `c4dc61e`).

**Legend:** ✅ implemented + tested · ⚠️ implemented but spec-deviation/partial · ❌ not implemented · — n/a
All source paths are under `src/dcc/`. Test files share the source dir (`*_Test.cxx`).

---

## Snapshot

- **Roles:** Command Station, Decoder, Accessory Decoder — `DCC_COMPILE_COMMAND_STATION` / `_DECODER` / `_ACCESSORY_DECODER`.
- **Service modes:** Direct, Paged, Register, Address — all implemented.
- **Tests:** 28 host unit-test binaries, 1158 tests passing (host, mocked drivers); ≈99% line coverage. Green host tests inject mock drivers — see **Known defects** (decoder RailCom Tx), which they cannot catch.
- **Hardware-in-loop:** Saleae HIL compliance suites (S-9.1 electrical/timing, S-9.2.1 packets, S-9.3.2 RailCom cutout, S-9.2.3 service mode — the last 150 checks incl. mock-ACK loopback) plus the two-board MSPM0 loopback suite (manual gate, not CI).

---

## Summary

- **Strong, fully-tested core:** packet encoding (speed/function/accessory/CV-POM/consist/binary-state/analog), scheduler, bit encoder, all four service modes (full §E page-preset sequences), bit/packet decoder, CV storage, RailCom 4/8 encode+decode, decoder-side RailCom responses, accessory-decoder RailCom. 1158 unit tests, ~99% line coverage.
- **Compliance deviations — ALL FIXED 2026-06-22** (see [archive/compliance_deviation_fixes.md](../archive/compliance_deviation_fixes.md); 1158 tests pass):
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
  - ❌ **Logon / Data Spaces (S-9.2.1.1)** — absent. *Released standard (2022), not draft-only — the 2026 draft only expands it.* Out of current scope.
- **Housekeeping:** old pre-refactor modules (`dcc_packet_encoder`, `dcc_application_service_track`, `dcc_application_main_track`) still coexist with the role-first replacements; both are compiled and tested.

---

## 1. Electrical / Signal (S-9.1)

Badges — **Supported:** ✅ in library · ⚠️ partial · ❌ no · — out of library scope.
**gTest** (host unit tests) / **HIL** (on-hardware, Saleae): ✅ covered · — n/a · ❌ gap ·
🚧 planned. Limitation notes sit inline with the badge.

| Feature | Released | Draft | Supported | gTest | HIL |
|---|---|---|---|---|---|
| One-bit half-period 58 µs (55–61) | S-9.1 Tbl 2.1 | — | ✅ | ✅ | ✅ 55–61 µs on wire |
| Zero-bit half ≥95 µs (nom 100); stretched total ≤12000 µs | S-9.1 Tbl 2.1 | — | ✅ | ✅ | ✅ ≥95 µs, total ≤12000 µs |
| Preamble ≥14 (CS transmit) / ≥10 (decoder accept) | S-9.1 / S-9.2 | — | ✅ CS sends 16 (+margin for cutout) | ✅ | ✅ ≥14 ones |
| Asymmetric / stretched zero accepted | S-9.1 | — | ✅ | ✅ | — decoder-side; HIL captures CS output only |
| Signal voltage / levels | S-9.1 §2 | — | — app H-bridge driver | — | — needs analog capture |

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
| Logon auto-registration, partitions 253/254 | S-9.2.1.1 (released, 2022) | §2 (expanded) | ❌ no `logon` anywhere in `src/` | — | ❌ released standard, not implemented |
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

> **Detail:** requirement-level design and per-test traceability (host gTest + Saleae HIL)
> in [Appendix A: Service Mode detail](#appendix-a-service-mode-s-923-detail) below.

| Feature | Released | Draft | Implemented | Tested | Status |
|---|---|---|---|---|---|
| Long preamble ≥20 | S-9.2.3 | — | `DCC_PREAMBLE_BITS_SERVICE=20` | per-mode packet tests | ✅ |
| Common ACK detect / reset seq / retry | S-9.2.3 | — | `dcc_service_mode_common.c`; per-op `command_repeat` + `recovery_count` | `..._common_Test` (64) | ✅ |
| ACK ≥60 mA for 6 ms ±1 ms | S-9.2.3 | — | threshold + min/max samples (`MIN/MAX_DURATION_US`) | ACK tests + HIL mock-ACK loopback | ✅ window [MIN,MAX] enforced |
| Direct write/verify byte & bit | S-9.2.3 | — | `dcc_service_mode_direct.c` (4 fns) | `..._direct_Test` (30) | ✅ all 3 instr types |
| Paged write/verify | S-9.2.3 | — | `dcc_service_mode_paged.c`; CV=`((cv-1)/4)+1`/`((cv-1)%4)+1`; page-select→data proceeds unconditionally (§E line 215) | `..._paged_Test` (22) | ✅ |
| Register write/verify | S-9.2.3 | — | `dcc_service_mode_register.c`; **page-preset** then command; verify 7 repeats; Reg 1 write recovery 10 | `..._register_Test` (20) | ✅ full §E sequence |
| Address-only write/verify | S-9.2.3 | — | `dcc_service_mode_address.c`; **page-preset** then CV#1 command; write recovery 10 | `..._address_Test` (21) | ✅ full §E sequence |
| Decoder-side service-mode CV ops | S-9.2.3 | — | `dcc_packet_decoder.c:_dispatch_service_mode*` (+`service_mode` flag) | svc_direct/register/verify tests | ✅ |

## 7. Fail-Safe (S-9.2.4)

| Feature | Released | Draft | Implemented | Tested | Status |
|---|---|---|---|---|---|
| Packet-timeout → stop all (CV11) | S-9.2.4 §4 | — | ❌ **not implemented** | — | ❌ |
| CV11 timeout value (≥20 s max) | S-9.2.4 | — | ❌ `DCC_CV_PACKET_TIMEOUT=11` defined, referenced nowhere | — | ❌ |
| `on_failsafe_entered/exited` | (lib API) | — | ❌ callbacks declared but never wired or called | — | ❌ |

## 8. RailCom / Bi-Directional (S-9.3.2)

> **Detail:** requirement-level design and per-test traceability (host gTest + Saleae HIL)
> in [Appendix B: RailCom detail](#appendix-b-railcom-s-932-detail) below.

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
> datagram-layer correctness + tests, not end-to-end transmission. See the
> **Backlog → Known defects** entry below.

## Command-station main-track / scheduler (cross-cutting)

| Feature | Released | Draft | Implemented | Tested | Status |
|---|---|---|---|---|---|
| Scheduler priority / combining / auto-refresh | (lib design) | — | `dcc_scheduler.c` | `..._scheduler_Test` (22) | ✅ |
| Bit-encoder buffering | (lib design) | — | single `active_packet` + `packet_loaded` flag | — | ✅ doc corrected |

---

## Backlog

Open work, tagged by where each item is defined. Most are **released-standard
conformance gaps**, not draft features. (Per-feature detail is in the §1–8 tables above.)

| Item | Spec | State | Notes |
|---|---|---|---|
| **Fail-safe / packet timeout (CV11)** | Released **S-9.2.4** | Not implemented | Only dead `on_failsafe_entered/exited` callbacks + an unused `DCC_CV_PACKET_TIMEOUT` define; no timeout timer or fail-safe state. |
| **XPOM (Extended POM)** | Released **S-9.2.1** §2.3.7.4 | Not started | 24-bit indexed CV read/write/bit (`1110GGSS`); exceeds the 6-byte packet limit. |
| **Time/Date & System Time packets** | Released **S-9.2.1** §2.3.6.2/.3 | Not started | Broadcast clock packets; only an unused `#define DCC_FEAT_TIME_DATE`. |
| **Accessory NOP packet** | Released **S-9.2.1** §2.4.6 | Not started | No encoder; the accessory SRQ auto-scheduling references it. |
| **Indexed CVs (CV31/32)** | Released **S-9.2.2** | Not started | Page-pointer access to CVs 257-512; defines exist, no paging logic. |
| **CV29 bits 2/3/4/7 behavior** | Released **S-9.2.2** | Partial | Decoder acts on bits 0/1/5 only; bit 2 (power-source), 3 (RailCom), 4 (speed table), 7 (decoder type) not acted on. |
| **Factory reset to defaults (CV8)** | Manufacturer convention | Partial | Write-permission exception only; no reset-to-defaults logic. |
| **Logon / Data Spaces** | Released **S-9.2.1.1** (2022) | Not started | Partitions 253/254, CRC-8+XOR, Logon auto-registration, Data Spaces. Released, not draft-only — the 2026 draft (deltas §2) only expands it. |
| **Decoder-side RailCom Tx integration** ⚠️ | Library | Needs design attention | See **Known defects** below. |
| **Old/new module duplication** | Library | Cleanup | Retire pre-refactor `dcc_packet_encoder` / `dcc_application_main_track` / `dcc_application_service_track` now that the role-first modules exist. |
| **SUSI bus** | Draft **S-9.4.x** (new std) | Not started | Decoder-to-peripheral bus; out of current scope. Deltas §6. |
| **E24 decoder interface** | Draft **S-9.1.1.6** (new std) | Not started | 28-pin small-scale decoder interface; out of current scope. Deltas §5. |

### Known defects

- **Decoder-side RailCom Tx not wired to hardware** ⚠️ *(deferred design issue).* The decoder
  RailCom encoder + response API are unit-tested with mock drivers, but `dcc_config.c` leaves the
  encoder's `uart_write` permanently NULL and there is no byte→pin bridge or decoder-side Tx
  one-shot timer (archived Open-Question-#2) — so **no RailCom is transmitted in a real build**.
  Host unit tests cannot catch it (they inject a mock `uart_write`). Needs a bit-bang and/or UART
  backend. Tracked plan context: [archive/compliance_deviation_fixes.md](../archive/compliance_deviation_fixes.md).

### Recently resolved (2026-06-22)

- The six compliance deviations (see Summary) — RailCom cutout retiming, accessory extended SRQ,
  ACK upper bound, speed-restriction removal, datagram-ID alignment + CS decode-table fix,
  bit-encoder doc. Plan: [archive/compliance_deviation_fixes.md](../archive/compliance_deviation_fixes.md).
- **POM vs service-mode ACK** — decoder CV callbacks now carry a `service_mode` flag and the ACK
  pulse fires only in service mode (`dcc_packet_decoder.c`), so POM no longer triggers the ACK load.

### Spec-reference errata

Corrected in [specs/DCC_Spec_Reference.md](../specs/DCC_Spec_Reference.md): analog-function instruction
byte is `00111101` (was `11111101`); the "Advanced Extended Packets" content (binary state, analog,
time/date, system time) belongs to **S-9.2.1**, not S-9.2.1.1 (which is the Logon / Data-Space standard).

*Cross-reference: [ARCHITECTURE.md](../ARCHITECTURE.md) (module map).*
* 

---

## Appendix A: Service Mode (S-9.2.3) detail


> Source: NMRA S-9.2.3 (July 2012). All four modes are defined in §E.
> "Read" in service mode is always accomplished via **Verify** — the CS sends a verify
> packet and an ACK pulse from the decoder means the value matches. A true read of an
> unknown value requires iterating verify operations (bit-by-bit or byte-by-byte).

---

### Operations Supported per Mode

| Mode             | Write Byte | Read Byte | Bit Write | Bit Read | CV Scope |
|------------------|:----------:|:---------:|:---------:|:--------:|----------|
| **Direct**       | ✅ native — 1 op | ✅ native — exactly **8 ops** (one `verify_bit` per bit) | ✅ native — 1 op | ✅ native — 1 op | CV 1–1023 |
| **Paged**        | ✅ native — 1 op | ✅ iterate `verify_byte` 0→255 — up to **255 ops** | ✅ read-modify-write — up to **256 ops** | ✅ read byte, extract bit — up to **255 ops** | CV 1–1024 (via page register) |
| **Register**     | ✅ native — 1 op | ✅ iterate `verify_byte` 0→255 — up to **255 ops** | ✅ read-modify-write — up to **256 ops** | ✅ read byte, extract bit — up to **255 ops** | 8 registers → specific CVs |
| **Address-Only** | ✅ native — 1 op | ✅ iterate `verify_byte` 0→127 — up to **127 ops** (CV#1 is 7-bit) | ✅ read-modify-write — up to **128 ops** | ✅ read byte, extract bit — up to **127 ops** | CV 1 only (short address) |

---

### Mode Notes (from §E and §F)

**Direct** — 4-byte packet `long-preamble 0 0111CCAA 0 AAAAAAAA 0 DDDDDDDD 0 EEEEEEEE 1`.
AA+AAAAAAAA = 10-bit CV address (CV# = address + 1). CC selects operation type.
Bit manipulation uses a special data byte `111KDBBB` (K=write/verify, D=value, BBB=bit position).
Required on all conformant Command Stations since 1-Aug-2002.

**Paged** — 3-byte packet `long-preamble 0 0111CRRR 0 DDDDDDDD 0 EEEEEEEE 1`.
Registers 1–4 (RRR=000–011) are Data Registers pointing to CVs on the current page.
Register 6 (RRR=101) is the Paging Register. Page preset required before each operation.
CV formula: CV# = (page−1)×4 + data_register + 1.

**Register (Physical Register)** — Same packet format as Paged (`0111CRRR`).
8 fixed registers (RRR=000–111). Page preset required before operations.
Recovery time after writing Register 1 is 10 packets (same as Address-Only mode).

Register-to-CV mapping differs between Mobile and Accessory decoders (§E, page 5):

| Register | RRR | Mobile Decoder CV | Accessory Decoder CV |
|:--------:|:---:|-------------------|----------------------|
| 1 | 000 | Address (CV #1) | Lower Address (CV #513) |
| 2 | 001 | Start Voltage (CV #2) | Undefined — see Mfg. documentation |
| 3 | 010 | Acceleration (CV #3) | Undefined — see Mfg. documentation |
| 4 | 011 | Deceleration (CV #4) | Undefined — see Mfg. documentation |
| 5 | 100 | Basic Config Register (CV #29) | Undefined — see Mfg. documentation |
| 6 | 101 | (Reserved for Page Register) | Undefined — see Mfg. documentation |
| 7 | 110 | Version Number (CV #7) | Version Number (CV #7) |
| 8 | 111 | Manufacturer ID (CV #8) | Manufacturer ID (CV #520) |

The register task module must know whether it is addressing a Mobile or Accessory decoder
to correctly map register numbers to CV numbers. Register 1 recovery time (10 packets)
applies to Mobile decoders writing the address CV; for Accessory decoders, Register 1
writes CV#513 and the same 10-packet recovery applies.

**Address-Only** — 3-byte packet `long-preamble 0 0111C000 0 0DDDDDDD 0 EEEEEEEE 1`.
Only accesses CV #1 (7-bit short address). Requires Page-Preset-Instruction sequence first
(5+ writes to Page Register → 6+ reset packets). Writing CV#1 via any method must also
clear the extended-addressing bit (CV#29 bit 5) and the consist address (CV#19).

---

### Packet Sequence Requirements (§E)

| Mode | Pre-op resets | Page preset | Pre-CV resets | Command repeats | Recovery (write) | Post-op resets |
|------|:------------:|:-----------:|:-------------:|:---------------:|:----------------:|:--------------:|
| Direct | ≥3 | — | — | ≥5 | ≥6 | ≥1 |
| Paged | ≥3 | 5+ page writes + 6+ resets | ≥3 | ≥5 | ≥6 | ≥1 |
| Register | ≥3 | 5+ page writes + 6+ resets | ≥3 | ≥5 (≥7 verify) | ≥6 (≥10 if Reg 1) | ≥1 |
| Address-Only | ≥3 | 5+ page writes + 6+ resets | ≥3 | ≥5 | ≥10 | ≥1 |

All three non-Direct modes have a **second block of ≥3 reset packets** between the page preset
recovery and the actual CV access operation. The Pre-CV resets column captures this.
Register and Address-Only modes additionally allow an optional power-off/on cycle between
the page preset recovery and the pre-CV resets (footnote 3 — see Spec Constraints below).
Paged mode does NOT allow a power cycle at this point.

#### Implementation status — these sequences are enforced in the library

As of the service-mode sequence work, the primitives produce the full §E sequences above:

- **Register** and **Address-Only** prepend a **page-preset** (write page register → page 1,
  `7D 01 7C`) before the command — a two-step chain mirroring Paged (`begin_operation`
  page-preset → on complete → command). Previously they emitted the command directly.
- **Register VERIFY** uses **7** command packets (`DCC_SERVICE_MODE_REGISTER_VERIFY_REPEAT`);
  a write to **Register 1** (and to **CV #1** in Address-Only) uses the **10**-packet recovery
  (`DCC_SERVICE_MODE_RECOVERY_COUNT_LONG`). `begin_operation` gained a per-op `command_repeat`
  parameter to support this (recovery was already per-op).
- **Paged** proceeds from the page-select to the data step **unconditionally** — the page-write
  phase completes by ACK *or* by packet count, and ACK is optional (§D), so requiring the
  page-select to ACK would break paged on decoders that never assert one (S-9.2.3 line 215).
- The preset/page-select's own ACK result does **not** gate the command step in any mode.

Every one of these sequences is verified at both layers; the **Test Coverage Matrix** below
lists the exact host (`*_Test.cxx`) and HIL (`command_station/s9_2_3_compliance.py`) test per requirement.
The bench wiring for the HIL checks is in
[HIL_SETUP.md](../../test/compliance/command_station/HIL_SETUP.md).

---

### Test Coverage Matrix

> WHAT each spec requirement is verified by, and WHERE. The *how the bench is wired* —
> pins, Saleae channels, jumpers, UART commands — lives in
> [HIL_SETUP.md](../../test/compliance/command_station/HIL_SETUP.md), not here.
>
> **Host** = `src/dcc/*_Test.cxx` (gTest, mocked drivers). **HIL** =
> `test/compliance/command_station/s9_2_3_compliance.py` (Saleae, on-wire). A blank cell means "not covered
> at that layer," not "untested."

#### ACK detection (§D — 6 ms ±1 ms window)

| Requirement | Host (gTest) | HIL (s9_2_3) |
|---|---|---|
| ACK accepted at exact min sample count (85) | `common.ack_exact_min_samples_detected` | `ack_width_tests` (4930 µs = MIN) |
| ACK rejected one below min (84) | `common.ack_one_below_min_samples_not_detected` | `ack_width_tests` (4872 µs) |
| ACK accepted within max window | `common.ack_within_max_window_detected` | `ack_width_tests` (6960 µs = MAX) |
| ACK rejected over max / over-current | `common.ack_beyond_max_window_not_detected` | `ack_width_tests` (7018 µs) |
| Interrupted pulse resets the width counter | `common.interrupted_pulse_resets_counter` | — |
| ACK sampled only in COMMAND state (scan window) | `common.ack_during_pre_reset_ignored`, `common.ack_during_post_reset_does_not_change_result` | `ack_width_tests` (in-window vs EARLY) |
| Injected pulse width measured independently | — | `ack_width_tests` (D4 tap, `_measure_high_us`) |

#### Packet sequence (§E)

| Requirement | Host (gTest) | HIL (s9_2_3) |
|---|---|---|
| Pre-reset = 3 packets | `common.pre_reset_sends_correct_count` | `_count_resets ≥ 3` |
| Post-reset = 6 packets | `common.post_reset_sends_correct_packet_count` | — |
| ACK terminates command phase early | `common.ack_detected_terminates_command_phase_early` | — |
| ≥20-bit preamble (service packets) | — | `_check_command` (all modes) |
| Register VERIFY = 7 command repeats | `register.verify_uses_seven_repeats_zero_recovery` | `test_register` (`min_repeat`) |
| Register 1 / CV#1 write = 10-packet recovery | `register.write_register1_uses_long_recovery`, `address.write_passes_write_flag_and_long_recovery` | — |
| Standard write recovery = 6 packets | `register.write_register2_uses_standard_recovery` | — |

#### Per-mode encoding & behavior

| Requirement | Host (gTest) | HIL (s9_2_3) |
|---|---|---|
| Direct exact bytes (write 0x7C / verify 0x74 / bit) | `direct.write_byte_cv1_value_0x55`, `direct.verify_byte_cv1_value_0x55`, `direct.write_bit_cv1_bit3_high`, `direct.verify_bit_cv1_bit5_high` | `test_direct` (3 instr types) |
| Direct 10-bit CV addressing | `direct.write_byte_cv1024_value_0xAA` | `test_direct` (CV#1/#3/#1023) |
| Page-preset before Register/Address command | `register.write_starts_with_page_preset`, `address.write_starts_with_page_preset` | `test_register`, `test_address` |
| Page-preset NO_ACK still proceeds | `register.command_proceeds_even_if_preset_no_ack`, `paged.write_page_select_no_ack_still_proceeds` | — |
| Paged page-write + data-register command | `paged.write_cv1_page_select_packet`, `paged.write_cv1_data_access_after_page_select_success` | `test_paged` |
| Register↔CV mapping (Mobile + Accessory) | `task_register` mapping tests (CV1→Reg1 … CV520→Reg8) | — |
| Factory reset = value 8 → Register 8 (`7F 08 77`) | `task_register.factory_reset_writes_8_to_register_8` | `test_register` |

#### Task orchestrator (iteration / read-modify-write / verify)

| Requirement | Host (gTest) | HIL (s9_2_3) |
|---|---|---|
| read_cv iterates; ACK returns the found value | `task_register.read_cv_ack_on_value_42_returns_42`, `task_address.read_ack_on_address_3_returns_3` | — |
| write_cv = write → verify | `task_register.write_cv_verifies_after_write_complete`, `task_address.write_verifies_after_write_complete` | — |
| Bit op = read-modify-write | `task_register.write_bit_scans_then_writes_with_bit_set`, `task_address.write_bit_clears_bit` | — |
| Read-back / write-back value correctness | — | `test_readback` (mock decoder, `SVC MOCKCV`) |
| Parallel-track timing (main + service) | — | `check_track_timing` / `_check_timing_both` (every op) |

---

### Conformance Groups (§F)

| Programmer Type | Modes Required |
|-----------------|----------------|
| Address-Only | Address-Only only |
| Register-Mode CV | Address-Only + Physical Register |
| Paged CV | Address-Only + Physical Register + Paged |
| Direct CV | Address-Only + Physical Register + Direct |
| Universal CV | All four modes |

Effective 1-Aug-2002: all conformant Command Stations must implement **Direct Mode**.

---

### Task Orchestrator Architecture

#### Module Structure

One task module per service mode type — independent compile-gating, focused interface
structs, and isolated test files. Each module sits above its corresponding primitive module
and below the application layer.

```
dcc_service_mode_task_direct.c/.h     → DCC_COMPILE_SERVICE_MODE_DIRECT
dcc_service_mode_task_paged.c/.h      → DCC_COMPILE_SERVICE_MODE_PAGED
dcc_service_mode_task_register.c/.h   → DCC_COMPILE_SERVICE_MODE_REGISTER
dcc_service_mode_task_address.c/.h    → DCC_COMPILE_SERVICE_MODE_ADDRESS
dcc_service_mode_task_detect.c/.h     → DCC_COMPILE_COMMAND_STATION
```

**Why per-mode, not consolidated:**
- Each module is 4–5 functions with a small focused interface struct and mock in tests.
- Test files (`_Test.cxx`) mock only that mode's function pointers — a consolidated module
  would require mocking all four modes even when testing one.
- Independently compile-gated — unused modes compile out entirely.
- Mirrors the existing primitive module structure directly below in the stack.

#### Singleton — No Context Pointer

All task modules are singletons. State is held in a static internal variable; no context
pointer is passed to any API function.

**Hardware justification:** the shared 58 µs DCC timer already drives the main track and
service track in parallel. A second programming track would risk missing ISR deadlines.
There is therefore only ever one service track — multi-instance support has no use case
and context passing would add noise to every call site for no benefit.

Matches the existing application module pattern (`DccApplicationCommandStationServiceTrack_*`
functions take no context).

#### HIL Timing Requirement — Parallel Track Operation

The HIL compliance suite for service mode **must** run both the main DCC track output and the
service track output simultaneously and verify that neither drops a bit or stretches a
half-bit period beyond spec tolerance. (Channel/pin assignments are in
[HIL_SETUP.md](../../test/compliance/command_station/HIL_SETUP.md).)

**What to verify:**
- Main track continues producing correctly timed DCC packets (preamble, bits, error byte)
  with no glitches while service mode operations run on the programming track.
- Service track produces correctly timed service mode packets (≥20-bit preamble, correct
  bit encoding) with no glitches while the main track runs.
- No ISR deadline is missed — half-bit periods on both channels stay within DCC timing
  tolerance (±6 µs for a 58 µs half-bit).

**Why this matters:** the shared 58 µs timer drives both tracks from one ISR. Any task
module code that runs too long in the ACK callback or state advance path will stretch the
next half-bit period on both tracks. The HIL rig can catch this directly by measuring
both channels simultaneously on the Saleae.

**Existing architecture that protects timing (`dcc_config.c:813`):** both track pin
toggles are pulled to the very top of `DccConfig_58us_timer_isr()` before any state
machine processing. Main track (PB1) and service track (PB4) GPIO toggles fire
back-to-back immediately on ISR entry; the heavier `DccBitEncoder_tick_isr()` calls for
both encoders follow after. Bit timing on the wire is set by the GPIO write, not by
subsequent state processing — so task module callback overhead cannot cause bit-period
jitter on either channel.

**Why "continuous DCC, no software blanking" is what makes this safe.** Both tracks run
off the *one* 58 µs timer/ISR, and the encoder **never stops** — it ticks the bit cadence
continuously. RailCom is implemented as a **driver-side cutout strobe** (the H-bridge output
is gated externally via the begin/end-cutout hooks and a *separate* one-shot timer), **not**
by blanking or pausing the bitstream in software. The timing engine is therefore never
paused for a cutout. This matters directly for service mode: because the service track
shares that same 58 µs ISR, *anything that perturbed the shared timer would corrupt
service-mode bit timing too*. If RailCom were done the old way — blanking/pausing the DCC in
software to carve out the cutout — that disruption would propagate onto the service track
exactly when it needs clean ≥20-bit-preamble verify packets and a stable ACK-sample window.
The continuous-encoder + toggle-first-ISR design decouples both tracks from cutout events
and from each other's state-machine load. This is why the main track holds ~58.0 µs ± ~0.05 µs
half-bits even while a service-mode operation runs (measured on the HIL rig).

**The parallel-track timing check guards this property** (see the **Test Coverage Matrix**).
`command_station/s9_2_3_compliance.py` captures the main and service tracks *simultaneously* during every
service-mode operation and runs `check_track_timing()` on both — so a future change that
reintroduces software blanking (or otherwise stretches/drops a bit on either track) would
**fail the suite** instead of silently corrupting service-mode timing.

#### ACK Pulse-Width Verification (6 ms ±1 ms window, S-9.2.3 §D)

ACK detection lives entirely in `dcc_service_mode_common` (`ack_sample`): the 58 µs ISR
feeds it the `current_sense_read()` level (the application's comparator output on the ACK
sense pin), and it counts consecutive above-threshold samples to validate the pulse width
against the configured window. Window constants (typical config):

- `DCC_ACK_MIN_SAMPLES = (USER_DEFINED_DCC_ACK_MIN_DURATION_US / 58) − 1` = `(5000/58) − 1` = **85**
- `DCC_ACK_MAX_SAMPLES = USER_DEFINED_DCC_ACK_MAX_DURATION_US / 58` = `7000/58` = **120**

A run is accepted as an ACK only when, on the falling edge, its length is in
`[MIN, MAX]` (inclusive) and was never flagged over-current.

**Scan-window blanking (S-9.2.3 line 55).** `ack_sample` ignores current until the ACK scan
window opens — set once more than `DCC_SERVICE_MODE_ACK_BLANK_PACKETS` (2) command packets
have been sent. This masks the first two command packets, where a decoder switching into
service-mode code can glitch its motor-drive pins and put a transient on the same
current-sense line the ACK uses. It is lossless for a conformant decoder, which per §E cannot
ACK until it has received two identical packets anyway (a two-sided contract: decoder
won't ACK early, programmer won't look early). The window opens on the packet-complete event,
so it is packet-accurate; the `−1` in `MIN_SAMPLES` absorbs the one-sample edge quantization,
so an ACK that legitimately begins right at the 2nd packet's end is not clipped.

**Dropout filter (noisy loads).** Because the ACK *is* the decoder pulsing its motor, the
sensed current is noisy — commutation/PWM ripple can drop the comparator below threshold for
a sample or two mid-pulse. `ack_sample` bridges up to `DCC_ACK_DROPOUT_SAMPLES` consecutive
low samples into the run (measuring the elevated-current *span*, not strict-high time); only a
longer low run is a real falling edge. Tunable via **`USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US`**
(default 116 µs ≈ 2 samples; `0` = strict consecutive-high, the original behavior). Keep it a
small fraction of the 6 ms ACK or it defeats the width discrimination.

**Coverage** is in the **Test Coverage Matrix** (host width-counter / dropout-filter /
window-blanking tests, and the HIL `ack_width_tests` vectors). The host tests verify the
*logic*; the HIL suite injects a **real electrical pulse** (a firmware mock-ACK looped back
on-board) and checks both the UART verdict **and** the Saleae-measured pulse width against
these boundary vectors — widths chosen so `width_us / 58` lands on an exact sample count:

| width_us | samples (=width/58) | expected verdict   |
|:--------:|:-------------------:|:------------------:|
| 4872     | 84 (< MIN 85)       | NO ACK             |
| 4930     | 85 (= MIN)          | ACK DETECTED       |
| 6000     | 103                 | ACK DETECTED       |
| 6960     | 120 (= MAX)         | ACK DETECTED       |
| 7018     | 121 (> MAX 120)     | NO ACK (overrun)   |

This makes ACK-width an on-hardware check, not a synthetic value injected behind the driver.
The loopback wiring (the PB24→PB9 jumper, the D4 tap, and the `SVC MOCKACK` command) is
documented in [HIL_SETUP.md](../../test/compliance/command_station/HIL_SETUP.md).

#### API per Module

Every task module exposes the same four operations plus one ACK entry point.
Implementation and cost differ by mode:

| Operation | Direct ops | Non-Direct ops | Notes |
|-----------|:----------:|:--------------:|-------|
| `write_cv` | **2** (write + verify) | **2** (write + verify) | Value is known after write — one targeted verify, never iterate |
| `read_cv` | **8** (one `verify_bit` per bit) | **up to 255** (iterate `verify_byte` 0→N) | Value unknown — must iterate for non-Direct |
| `write_bit` | **2** (write_bit + verify_bit) | **up to 257** (read byte + modify + write + verify) | Non-Direct: read-modify-write; read step must iterate |
| `read_bit` | **1** (`verify_bit`) | **up to 255** (read byte, extract bit in software) | Non-Direct: full byte read required to get bit |

**Callback types:**

```c
// CV operations: value is the CV byte or bit value found/validated
typedef void (*dcc_service_mode_task_on_complete_callback_t)(dcc_service_mode_result_t result, uint8_t value);

// Detect operation: supported_modes is a bitmask of ALL supported modes
typedef void (*dcc_service_mode_task_on_detect_callback_t)(dcc_service_mode_result_t result, uint8_t supported_modes);

typedef void (*dcc_service_mode_task_on_progress_callback_t)(dcc_task_phase_enum phase, uint8_t current_step, uint8_t estimated_steps);
```

`on_complete` value is always meaningful: for reads it is the CV/bit value found; for writes
it is the value read back during validation.

`on_detect` returns a bitmask of every supported mode, not a single "best" mode, using the
`DCC_SERVICE_MODE_SUPPORTED_*` flags:

```c
#define DCC_SERVICE_MODE_SUPPORTED_DIRECT   (1u << 0)
#define DCC_SERVICE_MODE_SUPPORTED_PAGED    (1u << 1)
#define DCC_SERVICE_MODE_SUPPORTED_REGISTER (1u << 2)
#define DCC_SERVICE_MODE_SUPPORTED_ADDRESS  (1u << 3)
```

`supported_modes == 0` means no mode was detected (and `result` is `DCC_SERVICE_MODE_NO_ACK`);
otherwise `result` is `DCC_SERVICE_MODE_SUCCESS`. The application picks whichever supported
mode it prefers and selects the matching task module.

`on_progress` carries a phase so the application can map to its own strings or UI state:

```c
typedef enum {
    DCC_TASK_PHASE_READ,     // byte or bit iteration to find unknown value
    DCC_TASK_PHASE_WRITE,    // writing value to the decoder
    DCC_TASK_PHASE_VERIFY    // read-back validation after write
} dcc_task_phase_enum;
```

`estimated_steps` is the worst-case ceiling — the operation ends early via `on_complete`
the moment ACK is received. All operations accept `on_progress` as nullable; callers
pass `NULL` when progress reporting is not needed.

Phase sequence per operation:

| Operation | Phase sequence | Notes |
|-----------|---------------|-------|
| `read_cv` | READ | Ends at first ACK |
| `write_cv` | WRITE → VERIFY | Always 2 steps — value known after write |
| `read_bit` | READ | Ends at first ACK |
| `write_bit` Direct | WRITE → VERIFY | Always 2 steps |
| `write_bit` non-Direct | READ → WRITE → VERIFY | READ phase can be up to 255 steps |

**Interface structs** (all wired by `dcc_config.c`, all function pointers only):

```c
// --- Direct ---
typedef struct {

    bool (*verify_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value);
    bool (*verify_byte)(uint16_t cv_number, uint8_t value);
    bool (*write_byte)(uint16_t cv_number, uint8_t value);
    bool (*write_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value);
    bool (*is_idle)(void);
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_direct_t;

// --- Paged ---
typedef struct {

    bool (*paged_verify)(uint16_t cv_number, uint8_t value);
    bool (*paged_write)(uint16_t cv_number, uint8_t value);
    bool (*is_idle)(void);
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_paged_t;

// --- Register ---
typedef struct {

    bool (*register_verify)(uint8_t register_number, uint8_t value);
    bool (*register_write)(uint8_t register_number, uint8_t value);
    bool (*is_idle)(void);
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_register_t;

// --- Address-Only ---
typedef struct {

    bool (*address_verify)(uint8_t address);
    bool (*address_write)(uint8_t address);
    bool (*is_idle)(void);
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_address_t;

// --- Detect ---
typedef struct {

    bool (*direct_verify_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value);
    bool (*paged_verify)(uint16_t cv_number, uint8_t value);
    bool (*register_verify)(uint8_t register_number, uint8_t value);
    bool (*address_verify)(uint8_t address);
    bool (*is_idle)(void);
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_detect_t;
```

Detect needs `address_verify` because Address-Only is probed for real (it accesses only
CV#1 and cannot be inferred from the CV#8 probes used by the other three modes).

**Public API** (Direct shown; Paged and Address-Only follow same shape without `decoder_type`;
Register adds `decoder_type` per-call since decoder type can change between operations):

```c
// Direct
void DccServiceModeTaskDirect_initialize(const interface_dcc_service_mode_task_direct_t *interface);
bool DccServiceModeTaskDirect_read_cv(uint16_t cv, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskDirect_write_cv(uint16_t cv, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskDirect_read_bit(uint16_t cv, uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskDirect_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
void DccServiceModeTaskDirect_on_primitive_complete(dcc_service_mode_result_t result);

// Paged — same shape as Direct
void DccServiceModeTaskPaged_initialize(const interface_dcc_service_mode_task_paged_t *interface);
bool DccServiceModeTaskPaged_read_cv(uint16_t cv, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskPaged_write_cv(uint16_t cv, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskPaged_read_bit(uint16_t cv, uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskPaged_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
void DccServiceModeTaskPaged_on_primitive_complete(dcc_service_mode_result_t result);

// Register — decoder_type per-call (user may switch between loco and accessory decoder)
void DccServiceModeTaskRegister_initialize(const interface_dcc_service_mode_task_register_t *interface);
bool DccServiceModeTaskRegister_read_cv(uint16_t cv, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskRegister_write_cv(uint16_t cv, uint8_t value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskRegister_read_bit(uint16_t cv, uint8_t bit, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskRegister_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
void DccServiceModeTaskRegister_on_primitive_complete(dcc_service_mode_result_t result);
bool DccServiceModeTaskRegister_factory_reset(dcc_service_mode_task_on_complete_callback_t on_complete);

// Address-Only — CV#1 only; decoder handles CV#29/CV#19 side effects automatically
void DccServiceModeTaskAddress_initialize(const interface_dcc_service_mode_task_address_t *interface);
bool DccServiceModeTaskAddress_read(dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskAddress_write(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskAddress_read_bit(uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskAddress_write_bit(uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
void DccServiceModeTaskAddress_on_primitive_complete(dcc_service_mode_result_t result);

// Detect — own callback type: result is a supported-modes bitmask, not a CV byte
void DccServiceModeTaskDetect_initialize(const interface_dcc_service_mode_task_detect_t *interface);
bool DccServiceModeTaskDetect_detect_mode(dcc_service_mode_task_on_detect_callback_t on_detect);
void DccServiceModeTaskDetect_on_primitive_complete(dcc_service_mode_result_t result);
```

**ACK detection — internal, not the task's concern.** ACK detection lives in the
`dcc_service_mode_common` module, not in the task layer or the application. The application's
driver supplies only a raw level via `current_sense_read()` (e.g. a comparator output on a
GPIO pin reported as above-threshold/zero). The 58 µs ISR feeds that into
`DccServiceModeCommon_ack_sample()`, which counts consecutive above-threshold samples to
measure the pulse width and validate it against the S-9.2.3 §D window (5–7 ms, with
over-current rejection). The outcome is folded into the primitive's result code:
`DCC_SERVICE_MODE_SUCCESS` = valid ACK, otherwise no ACK. The split is: **hardware owns
amplitude/threshold, the common module owns width/windowing, the task owns sequencing.**

**`on_primitive_complete` design:** `dcc_config.c` wires each primitive's `on_complete` slot
to `DccServiceModeTask<Mode>_on_primitive_complete(result)`. The primitive fires this once its
full operation (command + recovery) is finished. The task derives the ACK directly from the
result — `ack = (result == DCC_SERVICE_MODE_SUCCESS)` — and advances its state machine. There
is **no** `on_ack` entry point: the task never needs the application to time or report the
pulse, because the common module already did.

Event sequence for each primitive step:
```
task calls interface->verify_bit(cv, bit, 1)
  → primitive (via common) sends command packets
  → 58µs ISR feeds current_sense_read() to common.ack_sample() the whole time
  → common measures the pulse width, sets result = SUCCESS (ACK) / NO_ACK
  → primitive finishes recovery → on_primitive_complete(result) fires
  → task: ack = (result == SUCCESS) → advances state
```

> Note: `on_start_ack_scan` remains in the task interface structs but is currently unused —
> the common module gates ACK sampling itself by its COMMAND/RECOVERY state, so the task has
> no need to tell the application "start scanning now." It is a candidate for removal.

**`decoder_type` rationale (Register mode only):** the user may program a locomotive and
then switch to an accessory decoder on the same programming track. Fixing decoder type at
init time would require re-initialization on every switch. Passing it per-call makes the
decoder type explicit and eliminates hidden session state.

#### Internal State Machine Shape

The same state machine pattern applies to all five modules:

```
IDLE
  ↓ operation called (read_cv / write_cv / read_bit / write_bit)
PHASE_EXECUTE      ← send packet via interface, wait for on_primitive_complete()
  ↓ on_primitive_complete(result) received  (ack = result == SUCCESS)
PHASE_ADVANCE      ← record result, decide: next iteration OR move to next phase OR done
  ↓ next iteration
PHASE_EXECUTE      ← (loop back — next bit / next byte value)
  ↓ done / phase complete
PHASE_COMPLETE     ← call on_complete(result, value), return to IDLE
```

What differs per mode is what EXECUTE sends and how ADVANCE decides:

| Operation | Mode | EXECUTE sends | ADVANCE logic |
|-----------|------|--------------|---------------|
| `read_cv` | Direct | `verify_bit(cv, bit, 1)` | ACK→bit=1, no ACK→bit=0; bit++ until 8 done |
| `read_cv` | Paged/Reg/Addr | `verify(cv, current_value)` | ACK→found; no ACK→value++ until 255/127 |
| `write_cv` | All | `write(cv, value)` then `verify(cv, value)` | Write phase→Verify phase→Complete |
| `read_bit` | Direct | `verify_bit(cv, bit, 1)` | ACK→bit=1; no ACK→bit=0; Complete |
| `read_bit` | Paged/Reg/Addr | `verify(cv, 0)` iterate | Same as read_cv; extract bit from result |
| `write_bit` | Direct | `write_bit(cv, bit, val)` then `verify_bit` | Write→Verify→Complete |
| `write_bit` | Paged/Reg/Addr | read byte first (iterate), modify bit, write, verify | Read phase→Modify→Write phase→Verify phase→Complete |

#### Detection Module

`dcc_service_mode_task_detect` determines **all** supported service modes and returns them
as a `DCC_SERVICE_MODE_SUPPORTED_*` bitmask via `on_detect`. It runs four stages in order:

1. **Direct** — the spec-defined method (S-9.2.3 p.3 lines 94-99): two `verify_bit` on CV#8
   bit 7 (try value=0, then value=1); ACK on either → Direct supported. If supported, the
   remaining 7 bits of CV#8 are read so the **byte value is now known**.
2. **Paged** — `paged_verify` on CV#8. If the value is already known (from the Direct read,
   or a prior scan), this is a **single value-verify**; otherwise it is a 0→255 scan that
   *learns* the value for the stages that follow.
3. **Register** — same as Paged, against register 8 (= CV#8 Manufacturer ID), reusing the
   known value when available.
4. **Address-Only** — a separate 0→127 scan of CV#1. It cannot reuse the CV#8 value (CV#1 is
   a different CV), and is **not** assumed to be universally supported — S-9.2.3 §F mandates
   Address-Only for *command stations*, not for every *decoder*.

Value reuse is the key cost optimization: only the **first** mode to reveal CV#8's byte value
pays a full scan (worst case 256); every later CV#8-family mode then costs a single probe.
A Direct-capable decoder (the common case) costs ~9 probes for the Direct read + 1 each for
Paged and Register. If no stage acknowledges, `supported_modes == 0` and `result` is
`DCC_SERVICE_MODE_NO_ACK`.

Detection cost is explicit — the application calls `DccServiceModeTaskDetect_detect_mode(on_detect)`
and knows it is paying that cost. The library never runs detection silently inside another
operation.

#### API Design Rationale

- **Mode-specific calls, not a unified `read_cv`** — auto-detection inside `read_cv` would
  hide a potentially multi-second probe sequence behind a simple-looking call. The caller
  picks the mode; if they do not know the decoder type, they call `detect_mode` first.
- **All four modes support all four operations** — bit read/write in non-Direct modes is
  implemented by the task layer as read-modify-write (read byte, modify bit in software,
  write byte back). The packet-level primitive modules have no bit support for these modes.
- **Progress and cancellation** — any operation that iterates (read_cv in non-Direct, any
  bit op in non-Direct) can run for seconds. Progress callbacks and a cancel path are
  required in the task module interface structs.

#### Library Design Goal

This library targets **Universal CV Programmer** capability (all four modes). The spec
permits tiered conformance (§F) but as a library design requirement all four modes are
supported so applications can program any decoder, including pre-2002 decoders with no
Direct mode path.

**Address-Only** writes to CV#1 must also enforce CV#29 bit 5 clear and CV#19 clear —
the task module must handle this as a compound write sequence, not a single packet.

---

### Spec Constraints and Special Cases (S-9.2.3)

#### Fundamental Decoder Execution Requirement

**Two identical packets before execution (§E, line 64-65):**
"A Digital Decoder must not execute any Verify or Write operations unless it is in service
mode and has received **two identical** Service Mode packets without any intervening valid
packets." This is why command repeat counts are ≥5 (≥7 for Register mode verify) and why
the ACK scanning window opens at the Packet End bit of the **second** service mode packet.
The task orchestrator must send at least 2 identical command packets before expecting
execution or ACK.

#### Timing Constraints

**20ms decoder exit timeout (§C, line 30):**
The decoder exits service mode if no valid reset or service mode packet is received within
20ms. The task orchestrator must never allow a gap of ≥20ms between packets. No callback,
state-advance code, or application handler invoked during a task may block longer than
~20ms or the decoder silently drops out of service mode mid-operation.

**Power-On Cycle (§E, line 75-80):**
After applying power to the programming track, the CS must transmit ≥20 valid packets
before initiating any service mode operation. If current draw exceeds 250mA sustained for
>100ms at any point during power-on, this is an over-current fault condition.

**100mA idle current limit after power-up (§E, line 81):**
After the power-up sequence, a decoder with all outputs turned off (lamps, amplifiers, etc.)
shall not draw more than 100mA of current except when processing an acknowledgment. Current
above this baseline during normal operation indicates a decoder fault.

**Operations mode re-entry protection (§C, line 31-33):**
After exiting service mode, the decoder only re-enters operations mode on a valid operations
mode packet NOT identical to a service mode packet. Service mode instruction packets use
short addresses 112–127 decimal — these cannot accidentally trigger operations mode re-entry.

#### ACK Timing and Constraints

**Verify ACK = "shall" / Write ACK = "may" (§E, all modes):**
For all four modes, the spec is unambiguous: a VERIFY operation **shall** generate an ACK
if values match; a WRITE operation **may** generate an ACK upon completion. Write ACK is
never guaranteed. The task module must handle both the ACK and no-ACK cases for writes —
never assume a write succeeded or failed based solely on the presence or absence of ACK.

**ACK scanning window (§D, line 55-57):**
The programmer must begin scanning for ACK at the Packet End bit of the **second** service
mode instruction packet — this follows directly from the two-identical-packets requirement.
Scanning continues through all required instruction repeats and through the end of
Decoder-Recovery-Time for writes. The `on_start_ack_scan` callback in the interface struct
signals the application to begin monitoring at this exact point.

**Write ACK is delayed (§D, line 54-55):**
For write operations, the decoder must not issue ACK until all affected non-volatile storage
has been fully written. The ACK pulse may therefore arrive significantly later than the
packet transmission. The task module must not time out ACK scanning prematurely on writes.

**Decoder Recovery Time ends early on ACK (§E, line 83-84):**
"Command Station/Programmer shall send the same service mode write packets or reset packets
during the Decoder-Recovery-Time until the specified packet time has been met **or** until
the command station/programmer has received a valid acknowledgment." Recovery can be cut
short if ACK arrives — the task module does not need to send the full recovery packet count.

**CS must not stop sending packets early (§D, line 58-59):**
The CS may not stop sending packets to the programming track (which would cut power to the
decoder) until the end of Decoder-Recovery-Time, even if ACK is received before that point.

#### Paged Mode Register Rules

**Page Register not reset by Reset packets (§E, line 270-272):**
"If the decoder maintains an internal Page Register copy that is only valid when power is
applied, it should NOT be initialized when a Reset packet is received." The task orchestrator
can therefore set the Page Register once at the start of a paged session and run multiple
operations without re-sending the page preset. The page register persists across reset
packets within the same power session.

**Registers 5, 7 and 8 unaffected by Page Register (§E, line 251):**
"Registers 5, 7 and 8 are unaffected by the contents of the Page Register value." CV#29
(Register 5), CV#7 (Register 7), and CV#8 (Register 8 mobile / CV#520 accessory) are always
accessible at their fixed register addresses regardless of the current page setting.

**Paged decoder detection (§E, line 205):**
"If a decoder does not implement Paged CV addressing, it must not respond to Paged CV
programming commands when the page register has a value greater than 1." The detect module
can probe for Paged support by setting page > 1 and issuing a verify — no ACK = Paged not
supported.

**Compatibility: power cycle after page preset (§E footnote 3):**
Some older decoders require a power off/on cycle after the page preset sequence and before
the CV access. This optional power cycle appears in the Register and Address-Only packet
sequences but is NOT allowed during a paged operation.

#### Special Task Operations

**Hard-Reset-Cycle vs Decoder Factory Reset — two distinct operations:**

*Hard-Reset-Cycle (§E, line 90):* a hard reset (RP-9.2.1) followed by 10 idle or reset
packets. Returns the decoder to its **initial predefined state** immediately. Used when the
CS wants to reset the decoder's operating state without touching stored CVs.

*Decoder Factory Reset (§E, line 280-291):* a Physical Register write of value 8 to
Register 8. Packet: `long-preamble 0 01111111 0 00001000 0 01110111 1`. Instructs the
decoder to restore **factory default CVs** — but this does NOT happen immediately. The
actual CV restoration occurs across each subsequent power-on cycle until complete. The
decoder places 255 in CV#8 as a "restore in progress" indicator until all CVs are restored.
Follows Physical Register packet sequence. Should be exposed as:
`DccServiceModeTaskRegister_factory_reset(on_complete, on_progress)`

**Page Register cleanup after Paged programming (§E, line 275):**
After any paged CV programming session, the task module must reset the Page Register
(Register 6) to value 1 before exiting service mode. This ensures compatibility with
older Command Stations/Programmers that assume the Page Register defaults to 1.

**Register 1 special recovery time (§E, line 182-183):**
Writing to Physical Register 1 (CV#1 / address) requires 10 recovery packets, not the
standard 6. The register task module must detect writes to Register 1 and use the
extended recovery count.

#### Legacy and Optional Instructions (Appendices)

**Appendix A — Address Query Instruction:**
Legacy instruction for older decoders that cannot respond to CV#1 verify operations.
Format: `long-preamble 0 AAAAAAAA 0 11111001 0 EEEEEEEE 1`. Address range 1–111 only.
Spec recommends: attempt Direct or Paged verify of CV#1 first; invoke Address Query only
on failure. Relevant to the detect module's fallback strategy for very old decoders.

**Appendix B — Service Mode Decoder Lock (optional):**
Locks all decoders on the programming track except one specified short address, allowing
programming of a single decoder without physical track isolation. A DCC product need not
implement this but if it does must implement it completely.
Format: `long-preamble 0 00000000 0 11111001 0 0aaaaaaa 0 EEEEEEEE 1` (4-byte packet).
Unlock: decoder receives any valid DCC packet other than a service mode packet or a
Decoder Lock packet with a non-matching address. If targeted at Universal CV Programmer
capability, this should be a task: `DccServiceModeTaskDetect_lock_decoder(address)`.


---

## Appendix B: RailCom (S-9.3.2) detail


> Per-spec detail doc for NMRA **S-9.3.2** (bi-directional communication / RailCom).
> Expands **§8 RailCom / Bi-Directional** above — this section holds the
> requirement-level design facts and the **Test Coverage Matrix** (requirement → host gTest →
> HIL test). The *how the bench is wired* (pins, Saleae channels) lives in
> [HIL_SETUP.md](../../test/compliance/command_station/HIL_SETUP.md).

---

### Architecture Overview

RailCom spans four library modules across two roles:

| Module | Role / compile flag | Responsibility |
|--------|--------------------|----------------|
| `dcc_railcom_cutout` | Command-station (`DCC_COMPILE_COMMAND_STATION`) | The cutout: tristate the H-bridge after the packet end-bit so the decoder can transmit. |
| `dcc_railcom_encoder` / `dcc_railcom_decoder` | 4/8 codec | Encode/decode the 8-bit RailCom code words and Ch1/Ch2 datagrams. |
| `dcc_application_decoder_railcom` | Decoder (mobile) | Build the decoder-side responses (POM, dynamic, address, ACK/NACK…). |
| `dcc_application_accessory_decoder_railcom` | Accessory decoder | Accessory SRQ state machine + Status/Time/Error reports. |

**Continuous DCC, no software blanking.** The cutout is a **driver-side strobe** — the H-bridge
output is gated externally via begin/end-cutout hooks and a *separate* one-shot timer; the bit
encoder never stops ticking. (Same invariant the service-mode track relies on — see
[Appendix A: Service Mode detail](#appendix-a-service-mode-s-923-detail), "Parallel Track Operation.")

---

### Cutout Timing & State Machine

A 5-state one-shot machine drives the cutout. Per-state durations are **defaults**
(`dcc_defines.h`) and are **user-configurable** via `dcc_config_t` (a field of `0` selects the
spec default). Source of truth: `dcc_railcom_cutout.h:34-38`, `dcc_defines.h:305-322`.

| State | Period (µs, default) | Cumulative (spec timestamp) | Action on expiry |
|-------|:--------------------:|:---------------------------:|------------------|
| DELAY    | 26  | **T_CS** = 26  | tristate H-bridge / begin cutout |
| SETTLING | 54  | **T_TS1** = 80  | enable UART Rx (Ch1 opens) |
| CH1      | 97  | **T_TC1** = 177 | disable UART Rx (Ch1 closes) |
| GAP      | 16  | **T_TS2** = 193 | enable UART Rx (Ch2 opens) |
| CH2      | 261 | **T_CE** = 454  | disable UART Rx, restore H-bridge, end cutout |

The S-9.3.2 Table 1 tolerance bands the HIL suite enforces on the wire: **T_CS 26–32 µs**,
**T_CE 454–488 µs**, derived window **T_CE−T_CS 422–462 µs**. The defaults above sit at the
low edge of those bands. `cancel()` mid-cutout restores the H-bridge from any state past DELAY.

---

### 4/8 Codec

- `encode_byte` maps a 6-bit value (0x00–0x3F, 64 values) to an 8-bit code word with a fixed
  1-bit weight (the "4/8" balance); `decode_byte` is the inverse and rejects invalid words.
- **Special code words:** ACK = `0xF0`, NACK = `0x3C` — sent **raw**, not as datagrams. (The
  2026 draft also decodes `0x0F` as ACK.)
- **Channels:** Ch1 = a 2-byte datagram (12-bit payload); Ch2 = up to a 4+ byte datagram.
  The decoder buffers received datagrams (depth-4 ring; oldest dropped on overflow).

---

### Decoder & Accessory Responses (datagram IDs)

**Mobile decoder** (`dcc_application_decoder_railcom`): address feedback alternates
**ADR1 (ID 1, high bits) ↔ ADR2 (ID 2, low bits)**; POM = ID 0; dynamic = ID 7;
track-search = ID 1/2/14; CV auto-transfer = ID 12; ACK/NACK sent as raw code words; plus a
raw passthrough that clamps the byte count. IDs are aligned to the 2026 draft (see master §8).

**Accessory decoder** (`dcc_application_accessory_decoder_railcom`): SRQ state machine
**IDLE → PENDING → RESPONDING**; Status 1 = ID 4 (aspect + command-match + setpoint flags);
Status 4 = ID 3 (extended, 2-byte); Time = ID 5; Error = ID 6. The SRQ carries the **full**
address and the Ch1 payload **differs for basic vs extended** format (MSB selects).

---

### ⚠️ Known defect — decoder-side Tx not wired to hardware

Every decoder/accessory **response** below is implemented and unit-tested at the datagram /
4-8 layer **only**. `dcc_config.c` leaves the encoder's `uart_write` permanently `NULL` and
there is no byte→pin bridge or decoder-side Tx one-shot timer, so **nothing is transmitted in
a real build**. Host tests inject a mock `uart_write` and cannot catch this. The ✅ marks on
response rows reflect datagram-layer correctness, not end-to-end transmission. Tracked in the
the [Known defects](#known-defects) section above.

---

### Test Coverage Matrix

> **Host** = `src/dcc/*_Test.cxx` (gTest, mocked drivers). **HIL** =
> `test/compliance/command_station/s9_3_2_compliance.py` (Saleae, on-wire). A blank HIL cell is the norm here:
> the HIL suite measures only the **cutout timing envelope** — it does **not** decode Ch1/Ch2
> datagram *content* on the wire, and the decoder Tx path is not hardware-wired (see above).

#### Cutout timing & 5-state machine

| Requirement | Host (gTest) | HIL (s9_3_2) |
|---|---|---|
| Cutout start T_CS (~26 µs) | `cutout.begin_starts_delay_timer` | `checks()` T_CS 26–32 µs |
| Cutout end T_CE (~454 µs) | `cutout.per_state_timer_periods_are_correct` | `checks()` T_CE 454–488 µs |
| Window duration T_CE−T_CS | — | `checks()` window 422–462 µs |
| One cutout per packet | — | `checks()` count vs decoded packets |
| Full 5-state sequence + event order | `cutout.full_sequence_state_transitions_and_actions`, `cutout.full_sequence_event_order` | — |
| Per-state periods (26/54/97/16/261) | `cutout.per_state_timer_periods_are_correct` | — |
| User-configurable timing (`dcc_config_t`, 0=default) | `cutout.initialize_stores_timing_fields`, `cutout.custom_timings_drive_each_state_period` | — |
| Cancel mid-cutout restores H-bridge | `cutout.cancel_during_settling_ends_cutout`, `…_ch1_…`, `…_gap_…`, `…_ch2_…` | — |

#### 4/8 codec (§2.5)

| Requirement | Host (gTest) | HIL (s9_3_2) |
|---|---|---|
| 4/8 encode (64 values + boundaries) | `encoder.encode_byte_value_0x00`, `encoder.encode_byte_value_0x3F`, `encoder.round_trip_all_values` | — |
| 4/8 decode + invalid rejection | `decoder.decode_byte_value_0x00_codeword_0xAC`, `decoder.decode_byte_invalid_0x00` | — |
| ACK special code word `0xF0` | `encoder.send_code_word_ack_raw`, `decoder.decode_byte_ack_0xF0` | — |
| NACK special code word `0x3C` | `encoder.send_code_word_nack_raw`, `decoder.decode_byte_nack_0x3C` | — |
| Ch1 2-byte datagram | `decoder.ch1_valid_2_bytes`, `encoder.send_ch1_basic` | — |
| Ch2 multi-byte datagram | `decoder.ch2_valid_4_bytes`, `encoder.send_ch2_multiple_data_bytes` | — |
| Receive buffer (depth-4, overflow) | `decoder.read_returns_buffered_datagram`, `decoder.buffer_overflow_drops_oldest` | — |

#### Decoder (mobile) responses — host-only

| Requirement | Host (gTest) | HIL (s9_3_2) |
|---|---|---|
| Address feedback ADR1/ADR2 alternation | `app_decoder.send_address_feedback_alternates`, `…_first_call_sends_adr1`, `…_second_call_sends_adr2` | — |
| POM response (ID 0) | `app_decoder.send_pom_response_delegates` | — |
| Dynamic data (ID 7) | `app_decoder.send_dynamic_data_delegates` | — |
| Track-search (ID 1/2/14) | `app_decoder.send_track_search_delegates` | — |
| CV auto-transfer (ID 12) | `app_decoder.send_cv_auto_transfer_delegates` | — |
| ACK / NACK as raw code words | `app_decoder.send_ack_sends_raw_code_word`, `…send_nack_sends_raw_code_word` | — |
| Raw response + count clamp | `app_decoder.send_raw_delegates`, `…send_raw_clamps_count` | — |

#### Accessory-decoder responses — host-only

| Requirement | Host (gTest) | HIL (s9_3_2) |
|---|---|---|
| SRQ state machine IDLE/PENDING/RESPONDING | `acc.send_srq_sets_pending_state`, `acc.on_stop_command_transitions_pending_to_responding`, `acc.on_cutout_responding_with_pending_sends_ch2_and_goes_idle` | — |
| Full-address SRQ (basic vs extended) | `acc.on_cutout_basic_and_extended_differ_for_same_address`, `acc.on_cutout_pending_sends_ch1_basic`, `…_ch1_extended` | — |
| Status 1 (ID 4) aspect + flags | `acc.send_status_packs_aspect_only`, `acc.send_status_packs_all_flags` | — |
| Status 4 (ID 3) extended | `acc.send_status_extended_packs_two_datagrams`, `acc.send_status_4_delegates` | — |
| Time report (ID 5) | `acc.send_time_report_with_resolution` | — |
| Error report (ID 6) | `acc.send_error_report_with_additional` | — |

> **The blank HIL column on every response table is the honest picture, not an omission.**
> RailCom Tx integration (a byte→pin bridge + decoder-side Tx timer) is the prerequisite for
> any on-wire HIL coverage of these rows — see the Known defect above and the master backlog.

---

### Why Ch1/Ch2 content is not HIL-tested

This is a **deliberate boundary, not an untested hole.** The library's RailCom decode is pure
logic — 4/8 codeword mapping, Ch1/Ch2 assembly, datagram buffering — and is exhaustively
covered by host tests (`round_trip_all_values`, the channel-assembly and buffer-overflow tests).
The library never touches the wire: it consumes bytes the **application** supplies through the
interface struct (a hardware UART or a bit-banged read). Whether those bytes arrive, and whether
a decoder emitted a correct datagram inside the cutout, are the **application's and the
decoder's/hardware's** responsibilities — not the library's. At the end of the day the hardware
and the decoder must meet their own requirements; the library's job is to encode/decode
correctly given conforming bytes, which the host suite already proves.

So an on-wire content check adds **no confidence in the library** — the decode logic it would
exercise is already proven on the host, more thoroughly than a wire capture could. We could not
identify an on-wire test of datagram content with value for the library, and there is no
transmitting decoder on the CS bench to produce that content in the first place.

**Possible future integration test (gated on hardware).** If a CS-side RailCom *receive* path is
ever wired, the receive→decode→assemble chain could be validated on real hardware with a
**mock-decoder loopback** — firmware driving 4/8 bytes onto the CS Rx line during the cutout,
mirroring the service-mode mock-ACK loopback (PB24→PB9). That would exercise the *application's*
UART-read timing and integration, still not the library's decode logic, and is out of scope
until a detector path exists.

### Other on-wire items not currently measured

These concern the CS's **own** cutout output — testable in principle, simply not yet in the suite:

- **Internal cutout phase boundaries** — the strobe marks only cutout begin/end, so the
  SETTLING/GAP/channel-switch sub-windows are not externally visible on PB2; measuring them
  would need the UART-enable transitions brought out to a probe.
- **DCC signal integrity across the cutout** — the suite checks a packet-count-vs-cutout-count
  match, not the main-track bit timing through the cutout window.

---

*Cross-reference: **§8 RailCom / Bi-Directional** above · bench
[HIL_SETUP.md](../../test/compliance/command_station/HIL_SETUP.md) · cutout
architecture [Appendix A: Service Mode detail](#appendix-a-service-mode-s-923-detail).*
