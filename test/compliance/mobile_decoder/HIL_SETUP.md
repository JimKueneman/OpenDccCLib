# Mobile-decoder HIL — bench setup & run guide

> **Status: rig live.** DUT firmware built/flashed (`saleae_hil_compliance/`, `DCC_COMPILE_DECODER`).
> Functional decode + ACK pulse are **hardware-verified** (see *First light* below). RailCom-Tx is
> pending (decoder-side Tx not yet implemented).

## Rig (two boards)

The **waveform player** emits the DCC stimulus into the **decoder DUT**; the decoder's reactions are
observed two ways — its UART decode report (the functional-decode oracle), and its ACK / RailCom-Tx
pins on the Saleae. The player is DCC-agnostic; all DCC semantics are composed on the host
(`wfplayer.py`). See the player's `SPEC.md` / `IMPLEMENTATION.md` for the stimulus side.

```
  Python host ──UART──► player board ──DCC_OUT(PB1)──┬──► decoder DUT  DCC_IN(PB1)
   (wfplayer.py)                                     └──► Saleae D0 (DCC line)
   decoder UART ──► host        (RECV decode report — the functional-decode oracle)
   decoder ACK_OUT(PB3)     ──► Saleae D1   (service-mode ACK pulse, 6 ms)
   decoder RAILCOM_TX(PB2)  ──► Saleae D2   (reserved — Tx not yet implemented)
```

> The decoder rig **does not use the player's trigger**: D1/PB3 carries the *decoder's* `ACK_OUT`
> instead (per the `GPIO_GRP_SALEAE` definition). The player's PB3 is simply left unconnected here.

## Wiring (Saleae channels — decoder `GPIO_GRP_SALEAE`)

| Saleae | Pin | Signal | Source / notes |
|:--:|:--:|---|---|
| **D0** | **PB1** | DCC line | player `DCC_OUT`(PB1) → decoder `DCC_IN`(PB1) via jumper; Saleae taps the line |
| **D1** | **PB3** | decoder `ACK_OUT` | service-mode ACK pulse (active-high, 6 ms) |
| **D2** | **PB2** | decoder `RAILCOM_TX` | reserved — RailCom-Tx not yet wired in firmware |
| **GND** | **GND** | common ground | across both boards **and** the Saleae |

These reuse the **same PB pins / Saleae channels as the command-station rig** (D0/PB1, D2/PB2;
D1/PB3 carries ACK here instead of the CS trigger) — no probe-moving, no channel reconfig.

## Serial ports

Each board's XDS110 enumerates **two** `/dev/cu.usbmodem*` ports; the command port is the one that
answers `ID?` → `OK wfplayer …` (player) or `HELP` → decoder menu (decoder). `decoder_smoke.py`
**auto-discovers** both (override with `PLAYER_PORT` / `DECODER_PORT`).

## Observation channels (what closes each loop)

- **Functional decode** (speed / function / CV-POM / accessory / consist / binary / analog) —
  set the decoder's address (`ADDR <n> <SHORT|LONG|ACC|ACCE>`), play a packet to it, and the
  decoder **reports the decode over UART** (`RECV …`); the host compares to what it sent. No Saleae
  needed. ✅ **verified.**
- **Service-mode CV** (S-9.2.3) — the decoder asserts an **ACK pulse** on `ACK_OUT`/PB3; the Saleae
  measures its width (6 ms ± 1 ms) on D1. ACK path bench-checked via `ACK TEST` (6001 µs). ✅
- **RailCom-Tx** (S-9.3.2-DEC) — the decoder transmits 4/8 datagrams on `RAILCOM_TX`/PB2; the
  Saleae captures and the host decodes. ⏳ *Blocked on decoder-side Tx being implemented.*

## Run

```bash
cd test/compliance/mobile_decoder
../.venv/bin/python decoder_smoke.py        # auto-discovers ports; sets addr; plays; checks RECV
```

## First light (verified)

Player streamed a 128-step speed packet `[0x03 0x3F 0x40]` to the decoder (addr 3); the decoder
reported, byte-accurate, on every loop:

```
RECV SPEED addr=3 speed=64 dir=REV mode=128
```

This exercised the full path — player `DCC_OUT`/PB1 → decoder `DCC_IN`/PB1 edge ISR → DCC library
decode → `RECV` oracle. The `ACK_OUT`/PB3 pin was separately confirmed at 6 ms.

See [`README.md`](README.md) and the layout convention in [`../README.md`](../README.md).
