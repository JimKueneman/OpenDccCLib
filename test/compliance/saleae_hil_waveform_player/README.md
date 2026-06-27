# Waveform Player — shared HIL stimulus instrument

A generic, **DCC-agnostic** digital waveform player (two-board rig). It emits the DCC stimulus
that drives the mobile-decoder and accessory-decoder DUTs; their responses are validated on the
Saleae. **Not a role DUT — shared test equipment**, so it lives at the top of `test/compliance/`
rather than under a role folder.

| Item | What it is |
|---|---|
| `SPEC.md` | spec-derived requirements (the authority) |
| `PROTOCOL.md` | the UART wire contract (commands, segment encoding, CRC) |
| `IMPLEMENTATION.md` | requirements → firmware/host plan |
| `HIL_SETUP.md` | bench wiring (Saleae D0→PB1, D1→PB3 — same pins as the CS rig), serial, and the M1 certification procedure |
| `wfplayer.py` | host driver — composes waveforms + drives the board (imported by decoder suites) |
| `test/` | pure host tests + hardware smoke + the **P1–P5 hardware spec-coverage suite** (`test_hw_spec.py`) |
| `saleae_hil_compliance/` | the player **firmware** (CCS project; built, flashed, serial-verified) |
| `saleae_hil_compliance.theia-workspace` | open in CCS to load the firmware |

> **Status: M1 certified** — built, flashed, and Saleae-validated: emits spec-faithful DCC
> (packets decode byte-for-byte), with a constant reload latency `L = 2 µs` measured and
> calibrated out (`load()` auto-compensates → exact on-wire timing). See [`HIL_SETUP.md`](HIL_SETUP.md).

See [`../README.md`](../README.md) for how this fits the compliance layout.
