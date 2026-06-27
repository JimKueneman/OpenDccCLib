# Waveform Player — Implementation Plan

How the spec-derived requirements ([SPEC.md](SPEC.md), P1–P5) become firmware + host.
Companion to the wire contract ([PROTOCOL.md](PROTOCOL.md)) and host library
([`wfplayer.py`](wfplayer.py)). The guiding invariant: **the firmware has no DCC
knowledge — it plays a list of `(level, duration)` segments**; all signal meaning is
composed in the host.

## 0. Pipeline

```
wfplayer.py ──compose──► segment list ──SEG/CRC/PLAY (UART 230400)──► player firmware
 (all DCC meaning)        (level,µs)×N                                  buffer + 1-shot reload engine
                                                                              │
                                              DCC_OUT (PB1) ─────────────────┤──► decoder DCC_IN
                                              TRIG    (PB3) ──────────────────│──► Saleae D1
                                                                              └──► Saleae D0
   decoder RailCom-Tx pin ──► Saleae   (separate-pin observation, validated against TRIG)
   decoder ACK pin        ──► Saleae
```

## 1. Data model (the spine)

- A **segment = `(pin level, timer-load count)`**, encoded `uint32`: bit 31 = level,
  bits 0..30 = duration µs. Firmware honors ≤ 65535 µs/segment; the host splits longer
  runs (`split_long`). **Explicit level** (not toggle-only) so the post-`STOP` park level
  and the held cutout-gap level are deterministic, and non-alternating fault patterns are
  free; for real DCC the levels simply alternate.
- A **DCC bit = 2 segments**: first/high half at index `2b`, second/low half at `2b+1`.
  This one fact is what makes the requirement mapping below mechanical.

## 2. Requirement → mechanism

| Req | Spec need | Host composes | Firmware plays |
|---|---|---|---|
| **P1** | half-bit timing ~1 µs; probe 52/55/61/64/90/10000 edges; `tr1d` ≤6 µs; ABC asymmetry | `packet_segs(one_half_us, zero_half_us)` (global) + `set_half`/`asym_bit` (per-pulse) | loads exact count/segment; resolution = timer prescale, **not** ISR rate |
| **P2** | exact preamble (9 / ≥10 / ≥20) | `packet_bits(preamble=N)` | more segments |
| **P3** | framing + settable error byte (XOR/CRC-8); up to XPOM/Logon length | `packet_segs(data, append_xor)`; arbitrary final byte | segments; buffer sized for longest packet |
| **P4** | gap (sub-ms across 5 ms) / exact N / indefinite | `compose(lead_idle, gap_idle, …)`; `PLAY count` | gap = long held segment; repeat/continuous = **gap-free buffer loop in the ISR** |
| **P5** | trigger on a *chosen* packet | place packet at a known segment offset; `TRIG <index>` | pulse TRIG when `idx == trig_at` |

Two override layers for P1 (pure host list-ops on the same segments the player plays):
**global** for "whole signal at the window edge" acceptance, **`set_half`/`asym_bit`** for a
single off-nominal pulse (rejection) and one-bit asymmetry (`tr1d`).

## 3. The engine — one-shot reload

```c
seg_t   buf[MAXSEG];               // uint32: level<<31 | dur_us
uint16  n, count, trig_at;
volatile uint16 idx, rep;
volatile bool   playing, done_flag;

// PLAY count:  if (playing) -> ERR busy
//   idx = rep = 0;  set_pin(level[0]);  TIMER_LOAD(dur[0] - L);  TIMER_START_ONESHOT();
//   if (trig_at == 0) TRIG_high();  playing = true;

void TIMER_IRQ(void) {             // HIGHEST NVIC priority
    if (++idx >= n) {              // wrap
        idx = 0;
        if (count && ++rep >= count) {     // finite play complete
            TIMER_STOP();  set_pin(LOW);  TRIG_low();
            playing = false;  done_flag = true;  return;   // main loop prints OK DONE
        }
    }
    set_pin(level[idx]);           // edge FIRST — minimize entry-to-edge latency
    TIMER_LOAD(dur[idx] - L);      // reload for next segment
    TIMER_START_ONESHOT();
    if (idx == trig_at) TRIG_high();
}
```

Implementation properties that matter:

- **One ISR per edge, never a fast fixed tick.** The timer counts at 1 MHz and is loaded
  with each segment's duration; it interrupts only at expiry. The CPU sleeps between edges.
- **Constant restart latency `L`.** One-shot reload stretches every segment by a fixed `L`
  (interrupt entry → GPIO write → restart). Because the playback ISR is highest priority,
  neither the heartbeat nor UART can delay it, so `L` is stable → it lengthens every half
  equally and **preserves all deltas/asymmetry**. We measure `L` once on the Saleae (M1) and
  the host **pre-subtracts it** (`dur - L`) so on-wire timing is exact to ~the timer tick.
- **Resolution = prescaler, not ISR frequency.** 1 µs now; raise the timer clock for finer,
  ISR rate unchanged. Spec bands are µs-scale, so 1 µs suffices.
- **Gap-free loop** is the wrap branch — long/continuous streams come from *looping a small
  buffer*, never from streaming (the UART can't sustain the bit rate; load-then-play only).
- **Jitter-optimal option:** precompute the next `(level, load)` at the end of each ISR so the
  next ISR's first instructions are raw register writes. Sub-µs without it; a knob if M1 asks.

## 4. Firmware modules & bench

```
saleae_hil_waveform_player.c   main loop: WfEngine_service(); UartDriver; CmdParser
wfplayer_engine.[ch]           buffer + ISR above + CRC + play/stop/state/trig
wfplayer_command_parser.[ch]   UART line → verb → OK/ERR   (PROTOCOL.md)
ti_driverlib_uart_driver.[ch]  reused
```

| Pin | Use | | Timer | Use | Priority |
|---|---|---|---|---|---|
| PB1 `DCC_OUT` | Saleae D0 + decoder DCC_IN | | one @ 1 MHz | playback (one-shot reload) | **highest** |
| PB3 `TRIG` | Saleae D1 | | SysTick | heartbeat blink | lower (cannot delay an edge) |
| PB22 `LED1` | heartbeat | | | | |
| PA10/11 | UART0 230400 8N1 | | | | |

UART replies happen **only in main-loop context**; the ISR never touches UART — that's why
`PLAY` returns `OK PLAYING` immediately and the deferred `OK DONE` is emitted from the main
loop on `done_flag`.

## 5. Host (`wfplayer.py`)

`bytes → bits → segments` (`one_bit/zero_bit/bits_to_segs/packet_bits/packet_segs/compose`),
the two override layers (`set_half`/`asym_bit`), `L` compensation, and protocol framing
(`seg_lines` chunking, **`CRC?` verify before `PLAY`** so a corrupted load can never play).
The 20 host unit tests lock this contract with no hardware.

## 6. Verification path

- **M0** — scaffold builds in CCS (SysConfig regenerates `ti_msp_dl_config.h`).
- **M1** — player only: capture `DCC_OUT` on the Saleae, run the existing `s9_1`/`s9_2`
  wire-decode suites against it → proves the player is spec-faithful **and** yields `L` for
  the host's timing compensation. Only then is the player trusted to stimulate a decoder.

## 7. Settled decisions

2-level / **no Hi-Z** (RailCom + ACK are decoder-pin observation — the booster/decoder
*hardware* does the cutout short-circuit and current-loop drive, per S-9.3.2 §2);
**one-shot reload** engine (not DMA); **load-then-play + gap-free loop** (not streaming —
UART bandwidth); **explicit `(level,duration)`** encoding; **`TRIG <index>`** (chosen packet,
not just play-start); **text/hex** protocol; **echo-off** default; heartbeat kept &
jitter-safe by priority; **CRC-gated** load.
