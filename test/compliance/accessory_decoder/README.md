# Accessory-decoder HIL suites

Compliance suites whose **device-under-test is an accessory decoder** (turnouts, signals),
stimulated by the **waveform player** like the mobile-decoder rig. Covers the accessory side of
S-9.2.1 (basic / extended accessory packets, 2.4.3 CV access) and, once decoder-side RailCom Tx
exists, the accessory datagrams of S-9.3.2.

**What an accessory decoder is in this library:** the same `DCC_COMPILE_DECODER` build as a mobile
decoder, with **CV541 bit 7 set**. The packet decoder then answers basic / extended accessory
packets by its 9-bit board address (CV513 low 6 bits | CV521 low 3 bits << 6), or by the 11-bit
output address when CV541 bit 6 is set, and ignores every multifunction packet, broadcast included.
`DCC_COMPILE_ACCESSORY_DECODER` no longer builds a decoder role — it compiles only the RailCom 4/8
encoders in `dcc_railcom_utilities` (also pulled in by `DCC_COMPILE_DECODER`).

That is why the first accessory rows already run on the **mobile-decoder rig**: its DUT's
`ADDR <n> ACC|ACCE` command sets CV513 / CV521 / CV541 exactly this way, and
`../mobile_decoder/s9_2_1_compliance.py` proves the address filter (`DCC-S9.2.1-DEC-018`) and the
accessory-type guard (`DCC-S9.2.1-DEC-019`). `../mobile_decoder/dcc_encode.py` already has the
spec-derived basic (decoder-address and output-address form), extended and 2.4.3 CV-access
builders.

No suites live here yet. What this directory is for: a DUT with real accessory outputs (turnout /
signal drivers on pins the Saleae can watch) and the accessory RailCom side. Note that the
library's accessory **SRQ polling path was removed** (2026-09-24) — nothing in the receive path
detects an SRQ, so there is no SRQ / NOP-poll behaviour to test until that feature is designed
whole. When the first suite lands, follow the setup in
[`../mobile_decoder/README.md`](../mobile_decoder/README.md) and the role-dir convention in
[`../README.md`](../README.md).
