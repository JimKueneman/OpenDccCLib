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
| `wfplayer.py` | host driver — composes waveforms + drives the board (imported by decoder suites) |
| `test/` | host unit tests (no hardware) + a hardware smoke test |
| `saleae_hil_compliance/` | the player **firmware** (CCS project; cloned from the CS firmware, being adapted to the player per `IMPLEMENTATION.md`) |
| `saleae_hil_compliance.theia-workspace` | open in CCS to load the firmware |

> The firmware is currently a verbatim clone of the command-station project — the next step is
> to strip the DCC bits (`dcc_lib`, `callbacks_dcc`, `ti_driverlib_dcc_driver`,
> `dcc_user_config.h` role flags) and add the segment-player engine. See `IMPLEMENTATION.md`.

See [`../README.md`](../README.md) for how this fits the compliance layout.
