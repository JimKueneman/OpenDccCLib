# Mobile-decoder HIL suites

Compliance suites whose **device-under-test is a mobile decoder**, stimulated by the certified
**waveform player** (`../saleae_hil_waveform_player`). Two-board rig: the player emits the DCC
stimulus into the decoder; the decoder decodes it and **reports back over its UART** (`RECV …`
lines — the functional-decode oracle), and later drives its RailCom-Tx / ACK pins for the
Saleae. See [`HIL_SETUP.md`](HIL_SETUP.md) for wiring.

```
  host --UART--> player ==DCC(PB1)==> decoder --UART(RECV lines)--> host (compares to what was sent)
```

What's here:

| Item | What it is |
|---|---|
| `saleae_hil_compliance/` | the **decoder DUT firmware** — cloned from `applications/.../decoder` (GPIO edge-capture → `DccConfig_decoder_edge_isr` → `RECV` report-back), built `DCC_COMPILE_DECODER`. Open `saleae_hil_compliance.theia-workspace` in CCS. |
| `decoder_smoke.py` | first suite **(stub)** — drives the player to emit a known packet, reads the decoder's `RECV` line, compares. Proves the loop. |
| `compliance_lib.py`, `wfplayer.py` | symlinks → the shared Saleae/decode lib and the player's host driver |
| `HIL_SETUP.md` | the decoder-rig bench wiring (planned) |

## Status / next steps

The DUT firmware is cloned but **not yet adapted or flashed**, and the rig isn't wired. To get
the first real run:

1. **Flash the decoder DUT** — build `saleae_hil_compliance/` in CCS (it already does edge-detect
   + `RECV` report-back; `DCC_IN` is on PB1, matching the player's `DCC_OUT`).
2. **Wire** player `DCC_OUT` (PB1) → decoder `DCC_IN` (PB1), common ground; each board on its own
   USB UART.
3. **Run** `PLAYER_PORT=… DECODER_PORT=… ../.venv/bin/python decoder_smoke.py`.

Then grow it into the per-spec suites (`s9_2_1_compliance.py`, …): add an independent DCC
**instruction encoder** (speed/function/accessory/CV/…), acceptance/rejection tests using the
player's marginal-timing (P1) + preamble (P2), and `@compliance <tid>` tags so the DEC rows in
`compliance.data.js` light up. See [`../README.md`](../README.md) for the role-dir convention.
