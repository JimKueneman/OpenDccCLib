# Waveform Player — UART Protocol v1

A **generic, DCC-agnostic** digital waveform player. The firmware knows nothing
about DCC, packets, preambles, or bit timing. It plays an arbitrary list of
`(level, duration)` segments on one output pin. **All** signal semantics live in
the Python host (`wfplayer.py`); the board is a "do what I say" instrument so it
rarely — ideally never — needs to change.

```
  Python host ──UART(230400 8N1)──► waveform_player board ──DCC_OUT──► decoder DUT / Saleae
   (wfplayer.py)   text commands                                trigger ──► Saleae
```

## Model

- One output channel (`DCC_OUT`). Idle level = LOW.
- A **segment** = `(level, duration_us)`: drive the pin to `level` for
  `duration_us`, then advance to the next segment.
- Encoded as a `uint32`, 8 hex chars: **bit 31 = level** (1=high, 0=low),
  **bits 0..30 = duration in microseconds**.
- The firmware timer honors up to **65535 µs per segment**; the host splits any
  longer run into multiple same-level segments (`wfplayer.split_long`).
- `PLAY count` emits the whole buffer `count` times **gap-free** (the playback
  ISR wraps at buffer end — no UART gap between repeats). `count = 0` = continuous
  until `STOP`. The host composes a full stimulus (lead-in idle, packet, repeats,
  trailing) into one buffer so the stream is uninterrupted.
- The **trigger** fires at a host-chosen segment index (`TRIG <index>`), so it can
  mark the start of a specific packet mid-sequence — not just play-start — which is
  what lines the Saleae up with a decoder's ACK or RailCom response.

## Commands

Every line is `\n`-terminated. Every command replies with exactly one line that
starts with `OK` or `ERR` so the host can synchronize.

| Command | Reply | Meaning |
|---|---|---|
| `PING` | `OK PONG` | liveness |
| `ID?` | `OK wfplayer v1 ch=1 maxseg=4096 tick_us=1` | capability discovery |
| `CLEAR` | `OK n=0` | empty the segment buffer |
| `SEG <hex8>[<hex8>...]` | `OK n=<count>` | append segments (chunkable across many lines) |
| `N?` | `OK n=<count>` | current segment count |
| `CRC?` | `OK <crc16_hex>` | CRC-16/CCITT-FALSE over the raw segment bytes (verify load integrity before play) |
| `TRIG <index>` / `TRIG OFF` | `OK` | pulse the trigger pin each time playback reaches segment `<index>` (`0` = play-start); `OFF` disables. Lets the host mark the exact packet-under-test for ACK / RailCom alignment |
| `PLAY [count]` | `OK PLAYING` … then `OK DONE` | start playback; `OK DONE` is emitted asynchronously when a finite play finishes |
| `STOP` | `OK STOPPED` | halt; park output LOW |
| `STATE?` | `OK IDLE` / `OK PLAYING <rep>/<count> seg <i>` | status |

Errors: `ERR badseg`, `ERR overflow`, `ERR busy` (load while playing),
`ERR syntax`. The host treats any `ERR` as a hard failure.

## Why this stays frozen

A bug in the player can only ever be a **timing-fidelity** bug (it played the
edges slightly wrong), never a *DCC* bug — because it has no DCC code. Fidelity
is certified once by capturing `DCC_OUT` on the Saleae and running the existing
`s9_1` / `s9_2` wire-decode suites against it. New signal types (service mode,
marginal-timing acceptance, future protocols) are new **Python compositions**,
not firmware changes.
