# Waveform Player — Firmware Spec (thin)

A generic, **DCC-agnostic** digital waveform player on an MSPM0G3507 LaunchPad. It
receives a list of `(level, duration)` segments over UART and plays them on one pin.
It contains **no DCC knowledge** — preamble, framing, bit timing, idle, marginal
timings, cutouts are all composed by the Python host ([`wfplayer.py`](wfplayer.py)).
The goal is an instrument we **rarely, ideally never, change**: any new signal is a new
Python composition, not new firmware.

- **Wire contract:** [PROTOCOL.md](PROTOCOL.md) (commands, segment encoding, CRC).
- **Host library + tests:** [`wfplayer.py`](wfplayer.py), [`test/`](test/).

## Spec-derived requirements (the authority)

These come from the NMRA standards (via `documentation/specs/DCC_Spec_Reference.md`), **not**
from any implementation. They are the complete set needed to stimulate a decoder across the
NMRA decoder spec surface (S-9.1, S-9.2, S-9.2.1, S-9.2.2, S-9.2.3, S-9.2.4, S-9.3.2):

| # | Requirement | Spec |
|---|---|---|
| **P1** | Drive a 2-level DCC waveform with **independently-settable half-bit durations**, ~1 µs accuracy (probe the 55 / 61 / 90 / 10000 µs edges; asymmetric halves — which also drive **ABC / asymmetric-DCC braking**, the S-9.2.1 MAN bit, not just stretched-zero acceptance) | S-9.1 · S-9.2.1 |
| **P2** | **Exact preamble count**, including out-of-range (9 / ≥10 / ≥20 service) | S-9.1 · S-9.2 · S-9.2.3 |
| **P3** | **Arbitrary multi-byte payloads** with settable framing (start / separator / end) and an **independently-settable error byte** (correct XOR *and* wrong), up to the longest defined packet (XPOM) | S-9.2 · S-9.2.1 |
| **P4** | **Inter-packet gap control** (sub-ms, across the 5 ms threshold) + **exact repeat count** + **continuous indefinite looped playback** | S-9.2 · S-9.2.3 · S-9.2.4 |
| **P5** | A **trigger** marking a chosen packet, for ACK / RailCom observation alignment | S-9.2.3 · S-9.3.2 |

**This set is closed.** P1–P5 are all 2-level; the player needs nothing more for any decoder
spec. Explicitly *not* required (spec-confirmed): no glitch/runt threshold (S-9.1 defines
none), **no tri-state** (see *Out of scope* below), no decoder power control (a rig concern,
not the player's). XPOM and Logon — **both planned for implementation** — are the
longest-packet drivers; that's a buffer-sizing requirement in P3, not new behavior, so the
player needs no change when they land.

**Decoder specs with no player impact** (closed deliberately — their absence from P1–P5 is
intentional, not an oversight):
- **S-9.2.1.1** (Logon / CRC-8 / Data Spaces) — **planned for implementation**, and the player
  already covers it: Logon / Data-Space commands are byte payloads + a CRC-8 check byte (P3's
  independently-settable error byte), block transfers are just longer packets (P3 buffer
  sizing), and registration responses return over RailCom → decoder-pin observation. **No
  player change** when it lands.
- **S-9.4.X** (SUSI bus) — a separate decoder↔peripheral bus, **not** the DCC track signal;
  outside a DCC waveform player's domain.
- **S-9.1.1.x** (E24 and other decoder-interface standards) — connector / mechanical /
  electrical; no DCC-packet behavior to stimulate.

## Role in the bench (two boards)

```
Python host ──UART──► waveform_player board ──DCC_OUT(PB1)──┬──► decoder DUT  DCC_IN(PB1)
 (wfplayer.py)                          TRIG(PB3) ──────────┼──► Saleae D1 (trigger)
                                                            └──► Saleae D0 (the DCC line)
```

Separate board from the decoder DUT — independent clock, no on-chip contention, and the
player has zero DCC library, so it can never "agree" with a decoder bug. `DCC_OUT` is
jumpered into the decoder's `DCC_IN`; the Saleae taps the same line on **D0** (so the
existing `s9_1`/`s9_2` suites certify the player unchanged) and the trigger on **D1**.
Common ground across both boards + Saleae.

## Hardware

| Pin | Name | Dir | Use |
|---|---|---|---|
| **PB1** | `DCC_OUT` | output | played waveform (Saleae D0; jumper → decoder `DCC_IN`) |
| **PB3** | `TRIG` | output | pulse at play-start (Saleae D1) |
| **PB22** | `LED1` | output | heartbeat ("firmware alive") |
| PA10 / PA11 | UART0 TX/RX | — | host command link, 230400 8N1 |

| Timer | Use | Priority |
|---|---|---|
| one TIMA/TIMG @ 1 MHz | **playback** — one-shot, reloaded per segment | **highest** |
| SysTick | heartbeat blink | lower (must never delay a playback edge) |

## Architecture (modules)

```
saleae_hil_waveform_player.c     main: SYSCFG init, main loop
wfplayer_engine.[ch]             segment buffer + playback ISR + CRC + play/stop/state
wfplayer_command_parser.[ch]     UART line -> verb dispatch -> OK/ERR reply (PROTOCOL.md)
application_drivers/ti_driverlib_uart_driver.[ch]   reused from the other rigs
```

Main loop = three non-blocking calls: `WfEngine_service()` (emit deferred `OK DONE`),
`UartDriver_*`, `CmdParser_process()`. **UART replies happen only in main-loop context;
the playback ISR never touches UART.**

## Playback engine

- Segment = `(level, duration_µs)`, `uint32`, 1 µs tick. Encoding per [PROTOCOL.md](PROTOCOL.md).
- **One-shot reload, per segment:** `PLAY` sets the pin to `level[0]`, loads the timer with
  `dur[0]`, starts it (and pulses `TRIG`). The expiry ISR — kept tiny — **sets the pin
  first** (minimizes edge latency), then advances index, handles wrap/repeat, reloads the
  timer for the next segment. Finite plays park the pin LOW and set a `done_flag`; the main
  loop emits `OK DONE`.
- **Gap-free repeat:** `PLAY count` wraps at buffer end in the ISR (no UART gap between
  repeats); `count = 0` = continuous until `STOP`. The host composes a full stimulus
  (lead-in idle → packet → repeats → trailing) into one buffer so the stream is unbroken.
- Idle level between plays / after `STOP` = LOW.

## Resolved design decisions

- **Engine = one-shot reload ISR, not DMA.** Sub-µs ISR jitter is negligible against DCC's
  µs-wide windows, and explicit `(level,duration)` keeps non-alternating / fault-injection
  patterns possible. (DMA-fed toggle would force strictly-alternating waveforms — rejected.)
- **Keep the heartbeat.** SysTick LED blink stays as an "alive" signal. Made jitter-safe by
  priority: the playback timer ISR is highest, so it preempts SysTick and no DCC edge is ever
  delayed by the blink.
- **Echo off by default** (machine interface — only `OK`/`ERR` lines reach the host).
  `ECHO ON` is an optional hand-debug escape hatch.
- **2-level, final (LOW/HIGH).** The player never tri-states — RailCom and ACK are decoder
  pin-level outputs the *hardware* acts on (see *Out of scope*). The 1-bit segment level is a
  closed decision, not a v1 placeholder.

## Out of scope — and why no tri-state

- **No Hi-Z / tri-state, ever.** RailCom and ACK are *not* the player tri-stating or
  current-loading the line. In this system **no MCU touches the track electrically — the MCU's
  job ends at a pin.** This is the **literal spec architecture**: S-9.3.2 (Physical Layer) has
  the *booster* create the cutout by **short-circuiting the rails**, and the decoder transmit
  via a **current source** (≤34 mA into a current loop) — both *hardware*; the MCUs only emit
  pin signals at the right time. Just as the command station emits **continuous DCC + a separate
  cutout-window strobe** and lets the external H-bridge do the gating, the decoder MCU emits the
  **RailCom 4/8 on a Tx pin** and the **ACK on an ACK pin**, and *its* hardware drives the track /
  sinks the current. Therefore:
  - the player's "cutout" is a **held-level quiet window** modeling the hardware-gated track —
    an ordinary 2-level segment, **not** a tri-state;
  - the decoder's **RailCom-Tx pin and ACK pin are observation pins** the Saleae watches and
    validates (content + timing, against the player's `TRIG`) — they are *decoder* outputs, not
    player capabilities.

  This is why the 2-level player is complete and final for the whole decoder spec surface.
- **Multiple output channels / event-timeline scheduler** — a single channel covers every signal
  we can test; not built.

## Build & test

- **Firmware:** build the Debug config in CCS (SysConfig regenerates `ti_msp_dl_config.h`).
- **Host contract (pure, no hardware):** `python3 -m unittest discover -s test -p 'test_wfplayer.py'`
  — codec/CRC/composition + per-pulse overrides + calibration + the P1–P5 composition coverage. Must be green.
- **Hardware smoke:** `WFPLAYER_PORT=/dev/cu.usbmodemXXXX ../.venv/bin/python -m unittest discover -s test -p 'test_hw_smoke.py'`.
- **Spec coverage (P1–P5 on the wire):** `WFPLAYER_PORT=… ../.venv/bin/python -m unittest discover -s test -p 'test_hw_spec.py'`
  — composes each requirement, plays it, captures D0 (and D1 for P5), and asserts the decoded result. Needs the Saleae wired (D0→PB1, D1→PB3) + Logic2 Automation API.
- **Fidelity certification (M1):** capture `DCC_OUT` on the Saleae and run the existing
  `s9_1` / `s9_2` wire-decode suites against it — proves the player emits spec-faithful DCC
  before it is trusted to stimulate a decoder.
