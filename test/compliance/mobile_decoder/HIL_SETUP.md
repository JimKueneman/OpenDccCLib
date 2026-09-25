# Mobile-decoder HIL — bench setup & run guide

> **Status: rig live.** DUT firmware built/flashed (`saleae_hil_compliance/`, `DCC_COMPILE_DECODER`).
> Functional decode + ACK pulse are **hardware-verified** (see *First light* below). RailCom-Tx is
> pending — three firmware-side gaps, listed under *Observation channels*.
>
> The 2026-09-25 suite additions (consist addressing, CV21/CV22 gating, accessory filter/guard,
> fail-safe re-arm rules) and the `ADDR` command's switch to `DccConfig_reload_address_cvs()` have
> **not** been bench-run yet: rebuild the DUT in CCS, reflash, run `decoder_smoke.py`, then the suites.

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

| Saleae | Wire | Pin | Signal | Source / notes |
|:--:|:--:|:--:|---|---|
| **D0** | black | **PB1** | DCC line | player `DCC_OUT`(PB1) → decoder `DCC_IN`(PB1) via jumper; Saleae taps the line |
| **D1** | brown | **PB3** | decoder `ACK_OUT` | service-mode ACK pulse (active-high, 6 ms) |
| **D2** | red | **PB2** | decoder `RAILCOM_TX` | reserved — RailCom-Tx not yet wired in firmware |
| **GND** | gray | **GND** | common ground | across both boards **and** the Saleae |

These reuse the **same PB pins / Saleae channels as the command-station rig** (D0/PB1, D2/PB2;
D1/PB3 carries ACK here instead of the CS trigger) — no probe-moving, no channel reconfig.

For where these pins sit on the LaunchPad's 40-pin headers, see **LaunchPad header locations**
in [`../command_station/HIL_SETUP.md`](../command_station/HIL_SETUP.md) (PB1 = J4.39, PB3 = J1.10,
PB2 = J1.9, GND = J3.22 / J2.20).

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
- **RailCom-Tx** (S-9.3.2-DEC) — the decoder would transmit its 4/8 datagrams on `RAILCOM_TX`/PB2
  for the Saleae to capture and the host to decode. ⏳ *Not yet possible; what is missing is all on
  the firmware side (`saleae_hil_compliance/`), the library's Tx path exists:*
  1. `decoder.c` wires `dcc_config.railcom_tx_pin_set = NULL` — there is no PB2 GPIO driver — and
     the library reads a NULL pin driver as "no RailCom Tx" (`railcom_delay_us` is wired but idle).
  2. `dcc_config.on_railcom_request` is not wired, so even with a pin driver the decoder would send
     only the Channel-1 ADR datagrams and never a Channel-2 reply.
  3. the GPIOB edge ISR only stores timestamps in a ring that the main loop later drains into
     `DccConfig_decoder_edge_isr()`. The library bit-bangs the cutout reply from the end-bit path,
     so with a deferred drain the reply would start after the cutout window has closed; for Tx the
     edge ISR has to call `DccConfig_decoder_edge_isr()` directly (and `lock_shared_resources` must
     then mask that IRQ during the cutout so the decoder's own current pulse cannot re-trigger it).

## Run

```bash
cd test/compliance/mobile_decoder
../.venv/bin/python decoder_smoke.py        # auto-discovers ports; sets addr; plays; checks RECV
../.venv/bin/python s9_1_compliance.py      # timing acceptance / rejection (marginal timing, preamble)
../.venv/bin/python s9_2_compliance.py      # packet accept / reject (idle, reset, broadcast, XOR, address)
../.venv/bin/python s9_2_1_compliance.py    # instruction decode + consist addressing + accessory filter/guard
../.venv/bin/python s9_2_2_compliance.py    # CV effects: CV1, lock, CV8 reset, CV29, indexed, CV21/CV22 gating
../.venv/bin/python s9_2_3_compliance.py    # ACK_OUT pulse on D1: 6 ms ± 1 ms per matching verify (needs Saleae)
../.venv/bin/python s9_2_4_compliance.py    # CV11 fail-safe: trip time, exit, disable, re-arm rules
```

Each suite writes `reports/mobile_decoder_<spec>.html`. Only `s9_2_3` needs the Saleae; the rest use
the `RECV` oracle alone. The suites leave the DUT at short address 3 with CV19/CV21/CV22 = 0,
unlocked and CV11 = 0, restoring in a `finally` after a mid-run error.

## First light (verified)

Player streamed a 128-step speed packet `[0x03 0x3F 0x40]` to the decoder (addr 3); the decoder
reported, byte-accurate, on every loop:

```
RECV SPEED addr=3 speed=64 dir=REV mode=128
```

This exercised the full path — player `DCC_OUT`/PB1 → decoder `DCC_IN`/PB1 edge ISR → DCC library
decode → `RECV` oracle. The `ACK_OUT`/PB3 pin was separately confirmed at 6 ms.

See [`README.md`](README.md) and the layout convention in [`../README.md`](../README.md).
