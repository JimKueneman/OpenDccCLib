# OpenDccCLib — Implementation Status

> **Verified against commit `c4dc61e` (2026-04-21), reviewed 2026-06-22.**
> This is the live backlog. For *how* the library is structured, see
> [ARCHITECTURE.md](ARCHITECTURE.md); for spec details, see
> [specs/DCC_Spec_Reference.md](specs/DCC_Spec_Reference.md) and
> [specs/DCC_Draft_Deltas.md](specs/DCC_Draft_Deltas.md).

## Snapshot

- **Roles:** Command Station, Decoder, and Accessory Decoder, selectable via
  `DCC_COMPILE_COMMAND_STATION`, `DCC_COMPILE_DECODER`, `DCC_COMPILE_ACCESSORY_DECODER`.
- **Service modes:** Direct, Paged, Register, Address — all implemented.
- **Tests:** 23 host unit-test binaries, ~923 tests, all passing (host, mocked drivers).
  Coverage ≈ 99% line. Caveat: green host tests inject mock drivers — see the decoder
  RailCom Tx gap under Known defects, which the unit tests cannot catch.
- **Hardware-in-loop:** two-board MSPM0 loopback suite exists (manual gate, not CI).

## Implemented (against released NMRA S-9.x)

- Packet encoding: idle, reset, broadcast stop/e-stop; 14/28/128-step speed;
  function groups 1, 2a, 2b and expansion F13–F68; basic + extended accessory;
  POM CV write/verify/bit; consist set/clear; binary state short/long;
  analog function; speed restriction.
- Bit encoding (ISR state machine, shared fixed-period 58 µs timer) and decoding
  (edge-timestamp classification, preamble detection, byte assembly).
- Packet decoding with XOR validation, address matching, callback dispatch for
  all of the above command types.
- Scheduler: priority queue, duplicate combining by `(address, tag)`, auto-refresh.
- Service mode: common ACK/reset/retry engine plus Direct/Paged/Register/Address.
- CV storage: read/write via user callbacks, decoder lock (CV15/16), factory
  reset (CV8), consist (CV19), fail-safe (CV11/13/14).
- RailCom decoder (CS-side receive) and RailCom encoder (decoder-side responses:
  address feedback, POM response, dynamic data, ACK/NACK, track search,
  CV auto transfer, raw).
- Accessory decoder RailCom: SRQ state machine, Status 1/Status 4/Status extended,
  time report, error report.

> ⚠️ **Caveat — decoder-side RailCom Tx is datagram-layer only.** The encoder and the
> decoder/accessory response APIs above are implemented and unit-tested at the
> datagram / 4-8-encoding layer, but they are **not wired to any hardware output** in a
> real build. See Known defects → "Decoder-side RailCom Tx integration."

## Pending / not started

| Item | State | Notes |
|---|---|---|
| **Decoder-side RailCom Tx integration** ⚠️ | **Needs design attention (larger issue, later)** | The decoder RailCom encoder + response API are implemented and unit-tested, but **not connected to hardware**: `dcc_config.c` leaves `interface_dcc_railcom_encoder_t::uart_write` permanently NULL (stub wiring), so a real build transmits nothing. The encoder is byte-level (`uart_write(byte)`) while the decoder config only offers a bit-level `railcom_tx_pin_set(bool)`; the byte→pin bridge and the decoder-side Tx one-shot timer (archived Open-Question-#2) were never built. Needs a proper follow-on (bit-bang and/or UART backend). |
| **Old/new module duplication** | **Cleanup needed** | Both the pre-refactor modules (`dcc_application_main_track`, `dcc_application_service_track`, `dcc_packet_encoder`) and their role-first replacements (`dcc_application_command_station_*`) exist and are both compiled and tested. The old trio should be retired once nothing depends on them. |
| **XPOM (Extended POM)** | **Not started** | Planned in refactor plan phases 2 & 6; no references in source. 24-bit indexed CV addressing, multi-byte R/W, exceeds 6-byte packet limit. |
| **Logon / auto-registration (S-9.2.1.1)** | **Not started** | Address partitions 253/254, CRC-8+XOR, Data Spaces, bulk transfer. Draft-only. See draft deltas §2. |
| **SUSI bus (S-9.4.x)** | **Not started** | Decoder-to-peripheral bus. Draft deltas §6. Scoping only. |
| **E24 decoder interface (S-9.1.1.6)** | **Not started** | 28-pin small-scale decoder interface. Draft deltas §5. Scoping only. |
| **Draft RailCom timing / code words** | **Largely done (2026-06-22)** | Cutout retimed to spec (5-state, configurable); NACK `0x3C` / ACK `0xF0` now sent as 4/8 special code words; CS decode-table corrected. The decoder-side Tx *hardware* path is still pending (see the integration row above). |

## Known defects

- **Decoder-side RailCom Tx not wired to hardware** ⚠️ *(needs design attention — larger
  issue, deferred).* The decoder RailCom encoder and response API are unit-tested with mock
  drivers, but `dcc_config.c` leaves `interface_dcc_railcom_encoder_t::uart_write` permanently
  NULL and provides no byte→pin bridge, so **no RailCom is transmitted in a real build**. Host
  unit tests cannot catch this because they inject a mock `uart_write`. Full details and fix
  options are in the Pending row "Decoder-side RailCom Tx integration."

### Recently resolved (verify before relying on)

- **Compliance deviation fixes (2026-06-22)** — six "implemented but wrong" findings from
  [ComplianceMatrix.md](ComplianceMatrix.md) fixed: RailCom cutout retimed to a configurable
  5-state machine (spec timing); accessory extended SRQ now transmits the full address;
  service-mode ACK upper bound enforced; speed-restriction (`00111110`) removed; RailCom
  datagram IDs aligned to the 2026 draft (+ a CS-side decode-table bug fixed); bit-encoder
  buffering doc corrected. See
  [archive/compliance_deviation_fixes.md](archive/compliance_deviation_fixes.md).
- **POM vs service-mode ACK** — *Resolved in code, was open in the old `TODO.md`.*
  The decoder CV callbacks (`on_cv_write_command`, `on_cv_verify_command`,
  `on_cv_bit_command`) now carry a `bool service_mode` argument, and
  `dcc_packet_decoder.c` fires the ACK pulse only when `is_service_mode` is true
  (`_cv_write_and_notify` / `_fire_ack`). The application can now distinguish POM
  from service-mode programming, and POM no longer triggers the ACK current load.

## Spec-reference errata (fixed in [specs/DCC_Spec_Reference.md](specs/DCC_Spec_Reference.md))

These were flagged by [specs/DCC_Draft_Deltas.md](specs/DCC_Draft_Deltas.md) and
corrected in place:

1. Analog Function instruction byte is `00111101` (was incorrectly `11111101`).
2. The "Advanced Extended Packets" content (binary state, analog, time/date,
   system time, speed restriction, extended consisting) belongs to **S-9.2.1**,
   not S-9.2.1.1. S-9.2.1.1 actually defines the Logon / Data Space protocol.
