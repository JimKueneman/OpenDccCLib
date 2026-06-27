# Mobile-decoder HIL — bench setup & run guide

> **Status: planned — not yet built.** Prerequisites: the waveform-player firmware
> (`../saleae_hil_waveform_player`) and the decoder DUT firmware (`saleae_hil_compliance/`,
> built `DCC_COMPILE_DECODER`). Pins marked **TBD** are fixed when those firmwares are wired in
> SysConfig.

## Rig (two boards)

The **waveform player** emits the DCC stimulus into the **decoder DUT**; the decoder's reactions
are observed two ways — its UART decode report, and its RailCom-Tx / ACK pins on the Saleae. The
player is DCC-agnostic; all DCC semantics are composed on the host (`wfplayer.py`). See the
player's `SPEC.md` / `IMPLEMENTATION.md` for the stimulus side.

```
  Python host ──UART──► player board ──DCC_OUT(PB1)──► decoder DUT  DCC_IN(TBD)
   (wfplayer.py)              TRIG(PB3) ───────────────► Saleae D1 (trigger)
                              DCC line  ───────────────► Saleae D0
   decoder UART ──► host         (decode report-back — the functional-decode oracle)
   decoder RailCom-Tx (TBD) ──► Saleae  (4/8 content + timing, validated vs TRIG)
   decoder ACK (TBD)        ──► Saleae  (service-mode ACK pulse width)
```

## Wiring (Saleae channels)

| Saleae | Signal | Source |
|:--:|---|---|
| D0 | DCC line | player `DCC_OUT` (PB1) → decoder `DCC_IN` (**TBD**) |
| D1 | trigger | player `TRIG` (PB3) — marks the packet-under-test |
| D2 | decoder RailCom-Tx | decoder pin (**TBD**) — observation |
| D3 | decoder ACK | decoder pin (**TBD**) — service-mode ACK |

- **Common ground** across both boards + the Saleae.
- Player `DCC_OUT` → decoder `DCC_IN` via a jumper; the Saleae taps the line on D0.

## Observation channels (what closes each loop)

- **Functional decode** (speed / function / CV-POM / accessory / consist / binary / analog):
  the decoder fires its `on_*` callbacks and **reports the decode over UART**; the host compares
  to what it sent. No Saleae needed for these.
- **Service-mode CV** (S-9.2.3): the decoder asserts an **ACK pulse**; the Saleae measures its
  width (6 ms ± 1 ms) relative to the player trigger.
- **RailCom-Tx** (S-9.3.2-DEC): the decoder transmits 4/8 datagrams on its Tx pin; the Saleae
  captures and the host decodes, validated against the trigger. *(Blocked on decoder-side Tx
  being wired to hardware — see ComplianceOverview "Known defects".)*

## Run

To be filled in when suites land in this folder. From `test/compliance/`:
`.venv/bin/python mobile_decoder/<suite>_compliance.py`.

See [`README.md`](README.md) and the layout convention in [`../README.md`](../README.md).
