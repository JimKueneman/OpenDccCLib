# Accessory-decoder HIL — bench setup & run guide

> **Status: planned — not yet built.** Same two-board rig as the mobile-decoder setup (waveform
> player + DUT) with an **accessory decoder** as the DUT. Prerequisites: the waveform-player
> firmware and the accessory DUT firmware (`saleae_hil_compliance/`, built
> `DCC_COMPILE_ACCESSORY_DECODER`). Pins/channels are **TBD** until that firmware is wired.

The bench is identical in shape to [`../mobile_decoder/HIL_SETUP.md`](../mobile_decoder/HIL_SETUP.md):
the player stimulates the DUT; the DUT's reactions are observed via UART report-back and its
RailCom / status pins on the Saleae (validated against the player trigger), common ground across
both boards + Saleae. Accessory-specific differences:

- **Stimulus:** basic / extended accessory packets (S-9.2.1) and accessory NOP / SRQ triggers.
- **Observation:** accessory RailCom **SRQ + Status 1/4 / Time / Error** datagrams (S-9.3.2),
  captured on the Saleae and decoded on the host. *(Blocked on decoder-side Tx being wired to
  hardware — see ComplianceOverview "Known defects".)*

Run instructions to be filled in when suites land here. See [`README.md`](README.md) and the
layout convention in [`../README.md`](../README.md).
