# Waveform Player — HIL bench setup & certification

The waveform player is the shared HIL **stimulus instrument**: an MSPM0G3507 LaunchPad running
this firmware that plays host-composed `(level, duration)` segments on `DCC_OUT`. This doc covers
(a) wiring it to the bench, (b) verifying it over serial, and (c) **certifying** its output is
spec-faithful. For how it drives a decoder DUT, see
[`../mobile_decoder/HIL_SETUP.md`](../mobile_decoder/HIL_SETUP.md).

## Board & serial

- MSPM0G3507 LaunchPad with this firmware flashed (`saleae_hil_compliance/`).
- Command UART: **230400 8N1**, UART0 backchannel (PA10 TX / PA11 RX over the XDS110 USB-CDC).
- The XDS110 enumerates **two** `/dev/cu.usbmodem*` ports; the command UART is the one that
  answers `PING` with `OK PONG` (the other is the auxiliary port).

## Saleae channels — identical to the command-station rig (no probes move)

The player exposes exactly two signals, on the **same PB pins** the CS rig uses for D0/D1:

| Saleae | LaunchPad pin | Signal |
|:--:|:--:|---|
| **D0 (ch 0)** | **PB1** | `DCC_OUT` — the played waveform |
| **D1 (ch 1)** | **PB3** | `TRIG` — pulses during the segment chosen by `TRIG <index>` |
| **GND** | **GND** | common ground |

Both boards are the same LaunchPad pinout, so the existing **D0→PB1 / D1→PB3** probes land
correctly when you move them onto the player — **no probe moving, no channel reconfig**.
`compliance_lib.py` captures `ch0` (DCC) / `ch1` (trigger) by default (`DIGITAL_CHANNEL=0`,
`TRIGGER_CHANNEL=1`), so the `command_station/` `s9_1` / `s9_2` wire-decode reads these unchanged.

> The CS's other channels (D2/PB2, D3/PB4, D4/PB9, D5/PB18) are **not** driven by the player;
> those probes simply sit idle on the player board. `PA15` (`ISR_TIME`) brackets each playback
> ISR if you want to scope it, but it is not a Saleae compliance channel.

## Quick verification (serial only — no Saleae)

Confirms the protocol, buffer integrity, and that the playback engine runs — but **not** the
on-wire timing (that's the certification below).

```bash
ls /dev/cu.usbmodem*                                   # find the two ports
cd test/compliance/saleae_hil_waveform_player
WFPLAYER_PORT=/dev/cu.usbmodemXXXX \
  ../.venv/bin/python -m unittest discover -s test -p 'test_hw_smoke.py' -v
```
Expect `test_ping`, `test_id_reports_caps`, `test_load_crc_and_play` to pass. The **CRC match**
in `load_crc_and_play` proves the host and firmware agree byte-for-byte on the loaded buffer.
By hand, in any 230400 8N1 terminal: `PING`→`OK PONG`, `ID?`→`OK wfplayer v1 …`, `STATE?`→`OK IDLE`.

## Fidelity certification (M1) — ✅ certified

Captured `DCC_OUT` on the Saleae (100 ms @ 24 MS/s, ch0) while the player looped a known speed
packet, decoded with the `command_station/` `s9_1` pipeline:

- **Spec-faithful:** decoded back byte-for-byte (`0x03 0x3F 0x40` + XOR `0x7C`), idle packets
  `0xFF 0x00 0xFF`, **0 glitches**.
- **Reload latency `L` = 2 µs, constant** (on-wire `'1'` half 58→60, `'0'` half 100→102).
- **Calibration verified:** with `L` pre-subtracted the on-wire timing lands **exactly** on the
  requested 58.00 / 100.00 µs. Baked into the host — `wfplayer.CALIBRATION_L_US = 2` and
  `WaveformPlayer.load(calibrate=True)` (default) — so you compose at spec values and the board
  emits them. `L` is **per-board**; re-measure if you change boards or the timer clock.

To re-run: wire **D0→PB1 / D1→PB3** + common ground, Logic2 Automation API on (port 10430),
compose a packet (`wfplayer.py`), `load`+`play` looped, capture ch0, decode with `s9_1`.

## Use in a decoder rig

Two-board rig: the player's **`DCC_OUT` (PB1) → decoder `DCC_IN`**, the Saleae taps the line on
**D0** and the trigger on **D1**, common ground across both boards + Saleae. The decoder's
responses (UART report / RailCom-Tx / ACK pins) are the observation side — see
[`../mobile_decoder/HIL_SETUP.md`](../mobile_decoder/HIL_SETUP.md).

See [`SPEC.md`](SPEC.md) (requirements), [`PROTOCOL.md`](PROTOCOL.md) (wire contract), and
[`IMPLEMENTATION.md`](IMPLEMENTATION.md) (firmware/host design).
