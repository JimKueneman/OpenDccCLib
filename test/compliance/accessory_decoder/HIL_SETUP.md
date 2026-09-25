# Accessory-decoder HIL — bench setup & run guide

> **Status: planned — not yet built.** Same two-board rig as the mobile-decoder setup (waveform
> player + DUT) with an **accessory decoder** as the DUT. Prerequisites: the waveform-player
> firmware and an accessory DUT firmware (`saleae_hil_compliance/`, built `DCC_COMPILE_DECODER`
> with **CV541 bit 7 set** — that is what makes a decoder an accessory decoder in this library;
> `DCC_COMPILE_ACCESSORY_DECODER` only compiles the RailCom 4/8 encoders). Pins/channels are
> **TBD** until that firmware is wired.

The bench is identical in shape to [`../mobile_decoder/HIL_SETUP.md`](../mobile_decoder/HIL_SETUP.md):
the player stimulates the DUT; the DUT's reactions are observed via UART report-back and its output /
RailCom pins on the Saleae, common ground across both boards + Saleae. Accessory-specific
differences:

- **Stimulus:** basic accessory packets in decoder-address and output-address form, extended
  accessory packets, and 2.4.3 accessory CV access (S-9.2.1) — all in
  `../mobile_decoder/dcc_encode.py`.
- **Observation:** the `RECV ACC` / `RECV ACCE` report lines (already the oracle on the
  mobile-decoder rig, where `ADDR <n> ACC|ACCE` turns that DUT into an accessory decoder and
  `s9_2_1_compliance.py` rows `DEC-018` / `DEC-019` prove the address filter and type guard), and —
  once a DUT has real outputs — the turnout / signal drive pins on the Saleae.
- **RailCom:** the accessory datagrams of S-9.3.2 are *blocked on decoder-side Tx being wired to
  hardware* (see the RailCom-Tx gaps in the mobile-decoder setup guide). The library's accessory
  **SRQ polling path was removed** on 2026-09-24; there is no SRQ / NOP-poll behaviour to observe
  until that feature is designed whole.

Run instructions to be filled in when suites land here. See [`README.md`](README.md) and the
layout convention in [`../README.md`](../README.md).
