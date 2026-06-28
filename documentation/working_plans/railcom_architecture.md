# RailCom Architecture — Working Notes

> **Status:** thinking / organizing doc. NOT a committed design. Goal: capture spec
> facts, decompose decoder-Tx and command-station sides, and surface the **patterns,
> overlaps, and reuse** before we commit to an implementation.
> **Started:** 27 Jun 2026. Spec read: `documentation/specs/S-9.3.2_2012_12_10.pdf`
> (released). Draft (`drafts/s-9.3.2_bidirectional_communications_draft.pdf`) not yet
> cross-checked — provenance TODOs flagged below.

---

## 1. Spec anchors (cite before designing — don't reason from memory)

**Timing (§2.4).** All times referenced from *the zero crossing of the last edge of the
packet end bit*. The cutout belongs to the packet that just ended.

| Param | Name | Min | Max |
|---|---|---|---|
| Cutout start | T_CS | 26 µs | 32 µs |
| Start Channel 1 | T_TS1 | 80 µs | — |
| End Channel 1 | T_TC1 | — | 177 µs |
| Start Channel 2 | T_TS2 | 193 µs | — |
| End Channel 2 / cutout end | T_CE / T_TC2 | 454 µs | 488 µs |

- Ch1 = 12 net bits (2 bytes). Ch2 = up to 36 net bits (6 bytes). 250 kbaud ±2%, 4/8 coded.
- Decoder transmits **from its internal cache** during the cutout (§2.2) → data must be ready.

**Correlation (§3).** *"The meaning of the datagrams depends on the address of the
preceding DCC packet."* → correlation is **positional**: a datagram answers the packet
immediately before its cutout. There is no correlation ID.

**POM is deferred (§5.1.1 / §6.2.1), verbatim:** *"The corresponding response datagram
(ID0) must not be sent in the same packet frame, but can be sent at a later time… The
decoder responds with BUSY as long as the operation is not completed… transmits the
result on the appropriate, renewed re-read command. If the decoder does not return the
data within 0.5 s, the operation is considered failed."* CS must keep the decoder
addressed, **repeat** the command, and send **no other** command to it meanwhile.

**Address broadcast (§5.2):** *"In the cutout, after each DCC packet addressed to a mobile
decoder, a mobile decoder sends a packet with its active address"* (Ch1, ADR1/ADR2
alternating). Precomputed, unsolicited.

**Tokens (Table 2):** ACK `11110000`, NACK `00001111`, BUSY `11100001` — 6-bit special
code words, sent raw (bypass the 4/8 table).

---

## 2. The three-box decomposition (decoder ⟷ CS mirror)

| # | Box | Decoder side | Command-station side |
|---|---|---|---|
| 1 | DCC bitstream | **receive** (bit + packet decode) | **generate** (continuous, never stops) |
| 2 | Cutout window | **derive** off end-of-packet + **blank own edge IRQ** during it | **create** (tristate H-bridge) + time the sub-windows |
| 3 | RailCom data | **Tx**: encode → 250 kbaud UART | **Rx**: UART → 4/8 decode → datagram |

- **Gate (decoder):** CV29 bit 3 = RailCom enable. The library forces **only reserved bit 6**;
  the **app** filters CV29 (`cv29_filter_unsupported_features`) and clears `railcom_enabled`
  when it doesn't implement RailCom, so the *stored* bit 3 is always honest. (The earlier
  library-side bit-3 mask was reverted — the library no longer touches bit 3.) The app also
  owns whether RailCom exists at all (NULL hooks / not compiling `DCC_COMPILE_RAILCOM`). No
  auto-enable. The bit-3 check is a **single gate at the top of the per-cutout Tx state
  machine** (evaluated when the cutout is armed off end-of-packet); once armed it runs to
  completion — bit 3 can't change mid-window (a CV29 write is itself a packet, already ended).
  See §9.1 note.
- Each box is an independent control path (separate pin/timer/module). Pure-subtraction
  compile gate already in place (`DCC_COMPILE_RAILCOM`).

---

## 3. Message taxonomy (what actually rides the cutout)

| Message | Dir | Ch | Placement (§3.1) | Solicited? | **Handled by** | ID | §  |
|---|---|---|---|---|---|---|---|
| **ADR** (address) | dec→CS | 1 | P1 · immediate | unsolicited | **library** (addr cache) | ID1/ID2 | 5.2 |
| **DYN** (speed, fuel, flags) | dec→CS | 2 | P1 · immediate | unsolicited | **app** (Ch2 slot) | ID7 | 5.4 |
| **POM** read/write | dec→CS | 2 | **P2 · deferred** | solicited (BUSY→result) | **library** (`cv_read`/`cv_write`) | ID0 | 5.1 |
| **EXT** (location / "where am I") | dec→CS | 2 | P1 · immediate | solicited | **app** (Ch2 slot) | ID3 | 5.3 |
| **XF / filling** (refuel) | dec→CS | 2 | P1 · immediate | solicited | **app** (Ch2 slot) | ID7 | 5.3.2 |
| **SRQ** (service request) | acc→CS | 1 | **P3 · repeating** | unsolicited (attention) | **app · raises** | none | 6.1 |
| **STAT1 / STAT2** (status/aspect) | acc→CS | 2 | P1 · immediate | mixed | **app** (Ch2 slot) | ID4 / ID3 | 6.3 / 6.7 |
| **TIME**, **ERROR** | acc→CS | 2 | P1 · immediate | mixed | **app** (Ch2 slot) | ID5 / ID6 | 6.4 / 6.5 |
| **ACK / NACK / BUSY** | dec→CS | 2 | P1 · immediate | token | **library** (cmd outcome) | — | Table 2 |

*Placement values point at the §3.1 diagrams: **P1** immediate · **P2** deferred (POM only) ·
**P3** repeating (SRQ). Channel (1/2) is in the Ch column; the µs windows are in the §3.1
anatomy diagram.*

**Key reframe:** the *common* traffic is **unsolicited** — ADR on Ch1 every cutout, DYN on
Ch2 — not POM. POM is the rare transaction that interrupts the steady broadcast.

> Provenance TODO: XPOM (ID8–11), CV-auto (ID12), TIME(14), LOGON(15) appear in
> `dcc_defines.h` — confirm which are released vs draft-only before relying on them.

### 3.1 Cutout placement — timing diagrams

**Anatomy — one packet, one cutout** (the cutout belongs to the packet that just *ended*;
all sub-windows are measured from its end bit):

```
DCC stream:  ───│   PACKET N   │◄──────────── CUTOUT N ────────────►│  PACKET N+1  │───
                       end bit ─┘
   from end bit (µs):  0      26        80        177  193          454
                       ├───────┼─────────┼─────────┼────┼────────────┤
                       │ T_CS  │ settle  │   Ch1   │gap │    Ch2      │
                       │tristate (no Tx) │ ≤2 bytes│    │  ≤6 bytes   │
```

**Pattern 1 (P1) — immediate** (ADR, DYN, ACK/NACK, EXT, XF, STAT/TIME/ERROR). Reply rides the
cutout of its own trigger packet:

```
CS:   ───│ pkt → loco A │═══ CUTOUT ═══│ pkt → loco B │═══ CUTOUT ═══│───
                         │                             │
loco A:                  Ch1: ADR(A)                   (silent — not addressed)
                         Ch2: DYN(A)
                         └─ answers the packet to A, in that same cutout
```

**Pattern 2 (P2) — deferred** (POM read/write only). BUSY now; value on a *later repeated*
command, within 0.5 s:

```
CS:   ───│POM rd CV3→A│═CUT═│POM rd CV3→A│═CUT═│POM rd CV3→A│═CUT═│───
            (1st send)  │     (repeat)     │     (repeat)     │
loco A:            Ch2: BUSY         Ch2: BUSY         Ch2: <value>   ← ready
                   (reading…)        (still…)          (≤ 0.5 s total)
```

*Why deferred:* a CV read/write touches **persistent memory** (EEPROM / Flash). A Flash write
in particular runs into **milliseconds** — far past the ~193 µs to the Ch2 window. BUSY is the
"not yet" token that buys that time, and the 0.5 s deadline is sized for slow stores. This
maps straight onto the library: `cv_read`/`cv_write` are *allowed to be slow*; the BUSY/repeat
state machine — not the cutout — absorbs the latency. (Same reason POM stays library-internal:
the library owns the store interface and the BUSY timing.)

**Pattern 3 (P3) — repeating** (accessory SRQ). Ch1 of every eligible cutout until serviced:

```
CS:   ───│ NOP │═CUT═│ acc cmd │═CUT═│ NOP │═CUT═│ E-stop → acc │═CUT═│───
                │                │           │                   │
acc:       Ch1: SRQ        Ch1: SRQ    Ch1: SRQ            (serviced → SRQ stops)
           └── repeats every eligible cutout until it gets stop / coil-off on its address
```

### 3.2 Who handles it — library vs app (one app surface)

The **Handled by** column is really just two cases (interface sketch in §9.1):

- **library** — ADR (address cache), POM (CV store + BUSY/repeat), ACK/NACK/BUSY (command
  outcome). The app isn't in the RailCom loop; POM's value arrives through the
  `cv_read`/`cv_write` the app already provides. (Accessory **SRQ** is **app · raises** — the
  app flags it, the library repeats it on Ch1 until serviced.)
- **app** — everything else app-provided (DYN telemetry, EXT, XF, accessory STAT/TIME/ERROR)
  flows through **one surface**: the app fills the **next Ch2 datagram**. There is *no*
  library-owned typed cache (no DV table) — §5.4 says the *decoder* decides which DVs to send,
  and decoder = product = app, so the app supplies the datagram. The library holds only a single
  generic pending-Ch2 slot.

The app fills that slot **two ways, same mechanism**:
- **pre-load (async)** — write it whenever convenient, from any context (covers continuous
  telemetry); the library reads it at the cutout. Zero timing pressure.
- **just-in-time** — for a reply that depends on the specific command, fill the slot from the
  **XOR-window callback** below, in time for the immediate cutout.

**Ch2 slot priority (library-enforced):** a pending *library* response for this cutout (POM
BUSY/result, ACK/NACK) takes Ch2 over the app's datagram; otherwise the app's datagram goes.
That selection is the per-cutout scheduler (§10 #1).

**The just-in-time path — the XOR-window callback.** A DCC instruction is self-describing in
length, so the decoder knows the **full command at the last data byte** — before the trailing
XOR byte finishes. The library fires the callback **there**, giving the app the **~1 ms** of
XOR-byte transmission (+ 26 µs T_CS) to compute the reply and `set_ch2()` it — ready when Ch2
opens (~193 µs). That's how an app-computed reply still makes the *immediate* cutout.

```
loco A: ──preamble── addr  instr  data │ XOR byte │◄─cutout─►
                                  ▲     └─ ~1 ms ─┘    ▲
                            command known        reply must be ready
                            → fire app callback  by Ch2 (193 µs)
                              here (~1 ms budget)
```

Caveats:
- **Fast only** (~1 ms). Anything touching slow persistent memory (Flash) can't make it — which
  is exactly why **POM is *deferred*** (BUSY/repeat) instead. Rule of thumb: fast per-request →
  XOR-window callback + immediate cutout; slow → deferred BUSY.
- **Requires prompt/incremental decode** — the callback fires at the last-data-byte boundary, so
  the decode path must process bytes as they arrive (not the current main-loop-after-packet
  drain), and the callback must not block.
- More budget than the at-cutout pull in §9.1: firing ~1 ms before the window (vs ~80 µs at
  cutout-prep) is what makes a per-request computation feasible.
- *Confirm against spec:* only **POM** is verified-deferred so far; whether EXT / XF / STAT are
  truly immediate (eligible for the XOR-window path) vs. also deferred needs a §5.3/§6 re-read.

---

## 4. Decoder Tx — the per-cutout decision

Each cutout after the decoder is addressed, fill two slots:

- **Ch1:** almost always **ADR** (cached address, alternate ADR1/ADR2). Zero latency.
- **Ch2:** priority-pick from a small queue:
  1. pending **POM result** (if a transaction completed),
  2. pending **EXT/XF** response,
  3. **DYN** telemetry (speed/fuel) if due,
  4. else nothing (Ch1 only).

Sub-structure:
- **Box 2 timing** must be armed off a *fast* end-of-packet signal (hardware timer / ISR),
  **independent of the laggy main-loop decode** — otherwise the window drifts.
- **Box 3 Tx** clocks the chosen Ch1/Ch2 datagrams onto the UART inside their windows.
- **Producer** is the app-layer (`DccApplicationDecoderRailcom_*`): prepares datagrams.
- **POM on the decoder:** emit **BUSY** in the immediate cutout; process async (≤0.5 s budget
  across the CS's repeats); transmit the result on a *later repeated* read's cutout. Needs a
  tiny per-request transaction state (which CV, ready?, value).

### 4.1 Status of the old push API (superseded)

The pre-existing `dcc_application_decoder_railcom` / `dcc_application_accessory_decoder_railcom`
modules expose ~14 `send_*` "transmit now" functions. That design predates understanding the
cutout timing and is **flawed** — an app-driven send runs in the main loop, *after* the packet
callback, so it can never hit the cutout its reply belongs to. It was never wired anywhere, and
is **superseded by §9.1**: the library owns trigger/timing + ADR/POM/ACK/NACK; everything else
collapses to one `set_ch2()` slot (pre-load or via `on_railcom_request`); accessory **SRQ** stays
a genuine app-raised push — the interrupt.

The module **files are kept** (they'll hold the §9.1 redesign), but their premature **S-9.3.2
`DEC-001…007` / `ACC-001…006` compliance was stripped** — the gTests remain as untagged
regression tests; only the encoder's `DEC-008` (4/8 codec) still stands in the matrix.

---

## 5. CS side — the mirror transaction

- **POM transaction manager:** `SENT → BUSY(repeat) → RESULT | TIMEOUT(0.5 s)`.
- **Scheduler "hold + repeat + exclude":** the decoder under a POM transaction gets the same
  command re-injected each cycle, and *no other* programming command until done.
- **Rx → scheduler feedback (positional):** the datagram answers the just-sent packet; it
  drives the transaction state. `on_railcom_datagram_result` feeds the manager, not just bytes.
- **Async API:** `pom_read/write(addr, cv)` starts; completion via callback + timeout.
- **Address detection:** passive Ch1 ADR collection per detector → occupancy/localization.
- **Accessory SRQ:** CS sends periodic **NOP** to enable SRQs; decoder repeats SRQ until it
  gets an E-stop / coil-off on its own address.
- **Arbitration:** one POM read can monopolize repeats up to 0.5 s; refresh for every *other*
  loco must continue. Need a concurrency/interleave policy.

---

## 6. Patterns & overlaps  ⭐ (the reason for this doc)

1. **Decoder-Tx 3 boxes mirror CS-Rx 3 boxes** — same decomposition, opposite direction.
   `encoder ⟷ decoder` (4/8 codec both ways), `cutout-derive ⟷ cutout-generate`.
2. **CS POM transaction == service-mode transaction shape.** Both are
   `BUSY/ACK + repeat + timeout` state machines. The only difference is the completion
   signal: RailCom **datagram** vs service-mode **ACK current pulse**. → reuse the
   `dcc_service_mode_task_*` pattern; possibly a shared "transaction runner."
3. **"Per-slot fill decision" appears twice** — decoder per-cutout Ch1/Ch2 fill, and CS
   scheduler refresh-slot selection. Both are *priority pickers over a queue*. Candidate for
   a shared small priority-select helper.
4. **One token codec** (ACK/NACK/BUSY) is reused across POM (mobile + accessory) and SRQ.
   Already in the encoder/decoder as raw code words — keep it single-sourced.
5. **Producer/consumer queue** recurs: decoder Ch2 datagram queue, CS POM transaction,
   accessory SRQ pending-list. Same shape (enqueue from slow context, drain from
   timing-driven context). Worth one queue abstraction.
6. **Positional correlation** is the same rule on both sides — "datagram ⇿ preceding packet."
   Decoder uses it to know *which* address to broadcast; CS uses it to bind a reply to a
   command. One concept, two consumers.

---

## 7. Existing modules — built vs gap

| Module | Role | Status |
|---|---|---|
| `dcc_railcom_encoder` | Tx 4/8 codec + Ch1/Ch2 + ACK/NACK | ✅ built, tested |
| `dcc_railcom_decoder` | Rx 4/8 decode + datagram buffer (CS) | ✅ built, tested |
| `dcc_railcom_cutout` | CS cutout generator (timer state machine) | ✅ built, tested |
| `dcc_application_decoder_railcom` | flawed `send_*` push API (superseded by §9.1) | ⚠️ **kept** as placeholder for the §9.1 `set_ch2` redesign; compliance stripped; tests kept untagged |
| `dcc_application_accessory_decoder_railcom` | accessory SRQ/STAT/TIME/ERROR (+ `on_cutout`) | ⚠️ **kept** as placeholder; SRQ (the app-raised interrupt) survives the reshape; compliance stripped; tests kept untagged |
| **decoder cutout-timing + Tx hardware** | box 2/3 on the decoder | ❌ unbuilt |
| **decoder per-cutout scheduler** | the Ch1/Ch2 fill decision | ❌ unbuilt |
| **CS POM transaction manager** | BUSY/repeat/timeout + scheduler hold-repeat | ❌ unbuilt |

---

## 8. Open questions / decisions

**Open:**
- Box-2 decoder cutout timing precision vs main-loop decode latency — fast end-of-packet →
  one-shot timer path. Feasible on the target MCU?
- Ch2 queue depth and priority order (POM result vs EXT vs DYN).
- CS: max concurrent POM transactions; how to interleave with the refresh cycle.
- UART vs bit-bang for decoder Tx — **leaning UART** (RailCom *is* a 250 kbaud UART byte stream).
- Released-vs-draft provenance for XPOM / CV-auto / TIME / LOGON.

**Settled so far:**
- `DCC_COMPILE_RAILCOM` compile gate (role-stripped, pure subtraction). ✅
- CV29 feature filtering owned by the **app** (`cv29_filter_unsupported_features`): the app
  clears every feature it doesn't implement (incl. RailCom bit 3); the library forces **only
  reserved bit 6**, then re-encodes/stores what remains. The earlier library-side bit-3 mask
  was reverted — the library no longer touches bit 3. No auto-enable.

---

## 9. Reshaped interfaces (SKETCH — for review, not committed)

Both sides fall out of one rule: **the library owns the trigger + the cutout timing + every
message it already has the data for; the app surface shrinks to "fill the next Ch2 datagram"
(decoder) and "start a transaction" (CS).**

### 9.1 Decoder (mobile) RailCom Tx

**Hardware hooks** (`dcc_config_t`, under `DCC_COMPILE_RAILCOM`):
```c
/* Transmit one 4/8-encoded byte on the RailCom UART (250 kbaud, 8N1). REQUIRED for Tx. */
void (*railcom_tx_write_byte)(uint8_t byte);
/* Blank the decoder's own DCC edge IRQ during the cutout (false at end-bit, true after). REQUIRED. */
void (*decoder_edge_irq_enable)(bool enabled);
/* One-shot timer the library uses to time the cutout sub-windows off end-of-packet. REQUIRED. */
void (*railcom_oneshot_timer_start)(uint16_t usec);
void (*railcom_oneshot_timer_stop)(void);
```
(`railcom_tx_pin_set` / bit-bang variant dropped — UART model chosen.)

**App → library: fill the next Ch2 datagram** — ONE surface. Call it **async to pre-load**
(telemetry, any context) or **from the XOR-window callback** for a per-request reply:
```c
/* Set the Ch2 payload for the upcoming cutout. Latest set wins. The library owns ADR (Ch1)
   and any pending POM / ACK / NACK, which take priority over this slot (§3.2). */
void DccDecoderRailcom_set_ch2(uint8_t datagram_id, const uint8_t *data, uint8_t count);
/* optional typed wrappers for convenience: _set_ch2_dyn(subID,value), _set_ch2_cv_auto(…) … */
```

**Optional XOR-window callback** — the just-in-time fill point (replaces the cutout-prep pull):
```c
/* Fired at the LAST DATA BYTE of a packet addressed to this decoder — command fully known,
   ~1 ms of XOR transmission left. The app may call set_ch2() from here to answer the specific
   command in the immediate cutout. Must be fast / non-blocking. OPTIONAL (NULL = skip). */
void (*on_railcom_request)(const dcc_railcom_request_t *command);
```

**Library-internal — NO app function:**
- **ADR** — from the cached match-address; Ch1 every cutout after an addressed packet.
- **POM r/w** — packet decoder already decodes it; library uses `cv_read`/`cv_write`, runs the
  `BUSY → result` state machine, emits Ch2. App's `cv_read` supplies the value; it's not
  otherwise in the loop.
- **ACK / NACK** — library emits per command outcome.

**Dropped app fns:** `send_address_feedback`, `send_pom_response`, `send_ack`, `send_nack`
→ library-internal · `send_dynamic_data` / `send_track_search_response` / `send_cv_auto_transfer`
/ `send_raw` → the single **`set_ch2()`** (pre-load or via `on_railcom_request`). **No typed DV
cache, no `set_dynamic` / `set_location` — the library is no longer in the middle of app data.**

**Note — bit-3 gating is the state-machine entry condition.** The Tx is a state machine
(DELAY → Ch1 → gap → Ch2 → done) armed off end-of-packet. Check CV29 bit 3 **once, at arm
time**; if it passes, the machine runs to completion with no re-check. The bit cannot flip
mid-window — changing it requires a CV29 write, which is its own packet that has already
ended. So "where does the Tx re-check bit 3" → it doesn't; the gate is the arm condition.

**Note — physical safety on a non-RailCom track.** If bit 3 is enabled but the loco is placed
on a track whose command station makes **no cutout**, the decoder still arms and transmits in
its derived window while the CS is driving the bus. This is benign because the RailCom Tx is a
defined **current source** (30 +4/−6 mA for a "0", §2.2), not a voltage source — sourcing into
a low-impedance driven bus just gets absorbed, no fight, no damage. §2.2 also requires the
decoder's power source be protected from external track voltage during the (expected) cutout.
- *Related nuance to verify:* the decoder also **blanks its own DCC edge IRQ** during the
  derived window. On a non-RailCom track there's no real cutout, so blanking eats real preamble
  bits. The spec's preamble margin (CS sends ≥16, decoder needs ≥10 — §2.4 remark) covers a few
  lost bits, but a non-RailCom CS sending the bare-minimum preamble *could* be marginal. Confirm
  the worst case; may argue for keeping the blank window minimal, or detecting "no cutout."

*Open:* where the per-cutout Ch1+Ch2 **selection** lives (the decoder "scheduler"); does any
POM-read value need app compute beyond `cv_read` (if so, that IS the hook).

### 9.2 Command station RailCom (Rx + transactions) — the mirror

**Hardware hooks:** unchanged — existing `dcc_railcom_hw_t` (begin/end cutout, uart_rx
enable/disable, `uart_read`) + the cutout one-shot timer. Already built.

**App → library: start an async transaction:**
```c
bool DccCommandStationRailcom_pom_read(uint16_t address, uint16_t cv);
bool DccCommandStationRailcom_pom_write(uint16_t address, uint16_t cv, uint8_t value);
```

**Library → app: completion + passive receive feeds** (`dcc_config_t` callbacks):
```c
/* POM transaction finished; ok=false on 0.5s timeout / NACK. */
void (*on_pom_result)(uint16_t address, uint16_t cv, uint8_t value, bool ok);
/* Passive ADR feed: a decoder announced its address in this detector's cutout (occupancy). */
void (*on_address_detected)(uint8_t detector, uint16_t address);
/* Passive DYN feed: telemetry from a decoder. */
void (*on_dynamic_received)(uint16_t address, uint8_t subid, uint8_t value);
/* Accessory SRQ (already exists). */
void (*on_accessory_srq)(uint16_t address, bool is_extended);
/* Catch-all for datagrams not specialized above. */
void (*on_railcom_datagram_result)(uint16_t address, uint8_t channel, const dcc_railcom_datagram_t *datagram);
```

**Library-internal:**
- **POM transaction manager** — `SENT → BUSY(repeat) → RESULT | TIMEOUT(0.5s)`, driven by Rx
  datagrams, with the scheduler **hold + repeat + exclude**. Reuses the service-mode task
  shape (§6.2) — candidate shared "transaction runner."
- **Positional binding** of each datagram to the packet just sent.

*Open:* scheduler changes for hold/repeat/exclude; max concurrent POM transactions and how
they interleave with the refresh cycle; how much can be literally shared with
`dcc_service_mode_task_*`.

---

## 10. Next session (pick up here)

- Decide where the **decoder per-cutout scheduler** lives and its Ch1/Ch2 selection logic
  (the one gap that turns the app's `set_ch2()` slot + library-pending POM/ACK into actual
  transmitted datagrams — including the Ch2 priority rule from §3.2).
- Validate the **box-2 timing path** (fast end-of-packet → one-shot timer) on the MSPM0.
- Cross-check **draft S-9.3.2** for: immediate-cutout requirement wording, and provenance of
  XPOM / CV-auto / TIME / LOGON.
- Decide how much of the CS POM transaction manager can literally reuse
  `dcc_service_mode_task_*`.
