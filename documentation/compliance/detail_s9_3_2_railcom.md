# RailCom (S-9.3.2) Detail

> Per-spec detail doc for NMRA **S-9.3.2** (bi-directional communication / RailCom).
> Companion to the master [ComplianceMatrix.md](ComplianceMatrix.md) §8 — this doc holds the
> requirement-level design facts and the **Test Coverage Matrix** (requirement → host gTest →
> HIL test). The *how the bench is wired* (pins, Saleae channels) lives in
> [HIL_SETUP.md](../../test/compliance/ti_theia/saleae_hil_compliance/HIL_SETUP.md).

---

## Architecture Overview

RailCom spans four library modules across two roles:

| Module | Role / compile flag | Responsibility |
|--------|--------------------|----------------|
| `dcc_railcom_cutout` | Command-station (`DCC_COMPILE_COMMAND_STATION`) | The cutout: tristate the H-bridge after the packet end-bit so the decoder can transmit. |
| `dcc_railcom_encoder` / `dcc_railcom_decoder` | 4/8 codec | Encode/decode the 8-bit RailCom code words and Ch1/Ch2 datagrams. |
| `dcc_application_decoder_railcom` | Decoder (mobile) | Build the decoder-side responses (POM, dynamic, address, ACK/NACK…). |
| `dcc_application_accessory_decoder_railcom` | Accessory decoder | Accessory SRQ state machine + Status/Time/Error reports. |

**Continuous DCC, no software blanking.** The cutout is a **driver-side strobe** — the H-bridge
output is gated externally via begin/end-cutout hooks and a *separate* one-shot timer; the bit
encoder never stops ticking. (Same invariant the service-mode track relies on — see
[detail_s9_2_3_service_mode.md](detail_s9_2_3_service_mode.md), "Parallel Track Operation.")

---

## Cutout Timing & State Machine

A 5-state one-shot machine drives the cutout. Per-state durations are **defaults**
(`dcc_defines.h`) and are **user-configurable** via `dcc_config_t` (a field of `0` selects the
spec default). Source of truth: `dcc_railcom_cutout.h:34-38`, `dcc_defines.h:305-322`.

| State | Period (µs, default) | Cumulative (spec timestamp) | Action on expiry |
|-------|:--------------------:|:---------------------------:|------------------|
| DELAY    | 26  | **T_CS** = 26  | tristate H-bridge / begin cutout |
| SETTLING | 54  | **T_TS1** = 80  | enable UART Rx (Ch1 opens) |
| CH1      | 97  | **T_TC1** = 177 | disable UART Rx (Ch1 closes) |
| GAP      | 16  | **T_TS2** = 193 | enable UART Rx (Ch2 opens) |
| CH2      | 261 | **T_CE** = 454  | disable UART Rx, restore H-bridge, end cutout |

The S-9.3.2 Table 1 tolerance bands the HIL suite enforces on the wire: **T_CS 26–32 µs**,
**T_CE 454–488 µs**, derived window **T_CE−T_CS 422–462 µs**. The defaults above sit at the
low edge of those bands. `cancel()` mid-cutout restores the H-bridge from any state past DELAY.

---

## 4/8 Codec

- `encode_byte` maps a 6-bit value (0x00–0x3F, 64 values) to an 8-bit code word with a fixed
  1-bit weight (the "4/8" balance); `decode_byte` is the inverse and rejects invalid words.
- **Special code words:** ACK = `0xF0`, NACK = `0x3C` — sent **raw**, not as datagrams. (The
  2026 draft also decodes `0x0F` as ACK.)
- **Channels:** Ch1 = a 2-byte datagram (12-bit payload); Ch2 = up to a 4+ byte datagram.
  The decoder buffers received datagrams (depth-4 ring; oldest dropped on overflow).

---

## Decoder & Accessory Responses (datagram IDs)

**Mobile decoder** (`dcc_application_decoder_railcom`): address feedback alternates
**ADR1 (ID 1, high bits) ↔ ADR2 (ID 2, low bits)**; POM = ID 0; dynamic = ID 7;
track-search = ID 1/2/14; CV auto-transfer = ID 12; ACK/NACK sent as raw code words; plus a
raw passthrough that clamps the byte count. IDs are aligned to the 2026 draft (see master §8).

**Accessory decoder** (`dcc_application_accessory_decoder_railcom`): SRQ state machine
**IDLE → PENDING → RESPONDING**; Status 1 = ID 4 (aspect + command-match + setpoint flags);
Status 4 = ID 3 (extended, 2-byte); Time = ID 5; Error = ID 6. The SRQ carries the **full**
address and the Ch1 payload **differs for basic vs extended** format (MSB selects).

---

## ⚠️ Known defect — decoder-side Tx not wired to hardware

Every decoder/accessory **response** below is implemented and unit-tested at the datagram /
4-8 layer **only**. `dcc_config.c` leaves the encoder's `uart_write` permanently `NULL` and
there is no byte→pin bridge or decoder-side Tx one-shot timer, so **nothing is transmitted in
a real build**. Host tests inject a mock `uart_write` and cannot catch this. The ✅ marks on
response rows reflect datagram-layer correctness, not end-to-end transmission. Tracked in the
master [ComplianceMatrix.md](ComplianceMatrix.md) → Known defects.

---

## Test Coverage Matrix

> **Host** = `src/dcc/*_Test.cxx` (gTest, mocked drivers). **HIL** =
> `test/compliance/s9_3_2_compliance.py` (Saleae, on-wire). A blank HIL cell is the norm here:
> the HIL suite measures only the **cutout timing envelope** — it does **not** decode Ch1/Ch2
> datagram *content* on the wire, and the decoder Tx path is not hardware-wired (see above).

### Cutout timing & 5-state machine

| Requirement | Host (gTest) | HIL (s9_3_2) |
|---|---|---|
| Cutout start T_CS (~26 µs) | `cutout.begin_starts_delay_timer` | `checks()` T_CS 26–32 µs |
| Cutout end T_CE (~454 µs) | `cutout.per_state_timer_periods_are_correct` | `checks()` T_CE 454–488 µs |
| Window duration T_CE−T_CS | — | `checks()` window 422–462 µs |
| One cutout per packet | — | `checks()` count vs decoded packets |
| Full 5-state sequence + event order | `cutout.full_sequence_state_transitions_and_actions`, `cutout.full_sequence_event_order` | — |
| Per-state periods (26/54/97/16/261) | `cutout.per_state_timer_periods_are_correct` | — |
| User-configurable timing (`dcc_config_t`, 0=default) | `cutout.initialize_stores_timing_fields`, `cutout.custom_timings_drive_each_state_period` | — |
| Cancel mid-cutout restores H-bridge | `cutout.cancel_during_settling_ends_cutout`, `…_ch1_…`, `…_gap_…`, `…_ch2_…` | — |

### 4/8 codec (§2.5)

| Requirement | Host (gTest) | HIL (s9_3_2) |
|---|---|---|
| 4/8 encode (64 values + boundaries) | `encoder.encode_byte_value_0x00`, `encoder.encode_byte_value_0x3F`, `encoder.round_trip_all_values` | — |
| 4/8 decode + invalid rejection | `decoder.decode_byte_value_0x00_codeword_0xAC`, `decoder.decode_byte_invalid_0x00` | — |
| ACK special code word `0xF0` | `encoder.send_code_word_ack_raw`, `decoder.decode_byte_ack_0xF0` | — |
| NACK special code word `0x3C` | `encoder.send_code_word_nack_raw`, `decoder.decode_byte_nack_0x3C` | — |
| Ch1 2-byte datagram | `decoder.ch1_valid_2_bytes`, `encoder.send_ch1_basic` | — |
| Ch2 multi-byte datagram | `decoder.ch2_valid_4_bytes`, `encoder.send_ch2_multiple_data_bytes` | — |
| Receive buffer (depth-4, overflow) | `decoder.read_returns_buffered_datagram`, `decoder.buffer_overflow_drops_oldest` | — |

### Decoder (mobile) responses — host-only

| Requirement | Host (gTest) | HIL (s9_3_2) |
|---|---|---|
| Address feedback ADR1/ADR2 alternation | `app_decoder.send_address_feedback_alternates`, `…_first_call_sends_adr1`, `…_second_call_sends_adr2` | — |
| POM response (ID 0) | `app_decoder.send_pom_response_delegates` | — |
| Dynamic data (ID 7) | `app_decoder.send_dynamic_data_delegates` | — |
| Track-search (ID 1/2/14) | `app_decoder.send_track_search_delegates` | — |
| CV auto-transfer (ID 12) | `app_decoder.send_cv_auto_transfer_delegates` | — |
| ACK / NACK as raw code words | `app_decoder.send_ack_sends_raw_code_word`, `…send_nack_sends_raw_code_word` | — |
| Raw response + count clamp | `app_decoder.send_raw_delegates`, `…send_raw_clamps_count` | — |

### Accessory-decoder responses — host-only

| Requirement | Host (gTest) | HIL (s9_3_2) |
|---|---|---|
| SRQ state machine IDLE/PENDING/RESPONDING | `acc.send_srq_sets_pending_state`, `acc.on_stop_command_transitions_pending_to_responding`, `acc.on_cutout_responding_with_pending_sends_ch2_and_goes_idle` | — |
| Full-address SRQ (basic vs extended) | `acc.on_cutout_basic_and_extended_differ_for_same_address`, `acc.on_cutout_pending_sends_ch1_basic`, `…_ch1_extended` | — |
| Status 1 (ID 4) aspect + flags | `acc.send_status_packs_aspect_only`, `acc.send_status_packs_all_flags` | — |
| Status 4 (ID 3) extended | `acc.send_status_extended_packs_two_datagrams`, `acc.send_status_4_delegates` | — |
| Time report (ID 5) | `acc.send_time_report_with_resolution` | — |
| Error report (ID 6) | `acc.send_error_report_with_additional` | — |

> **The blank HIL column on every response table is the honest picture, not an omission.**
> RailCom Tx integration (a byte→pin bridge + decoder-side Tx timer) is the prerequisite for
> any on-wire HIL coverage of these rows — see the Known defect above and the master backlog.

---

## Why Ch1/Ch2 content is not HIL-tested

This is a **deliberate boundary, not an untested hole.** The library's RailCom decode is pure
logic — 4/8 codeword mapping, Ch1/Ch2 assembly, datagram buffering — and is exhaustively
covered by host tests (`round_trip_all_values`, the channel-assembly and buffer-overflow tests).
The library never touches the wire: it consumes bytes the **application** supplies through the
interface struct (a hardware UART or a bit-banged read). Whether those bytes arrive, and whether
a decoder emitted a correct datagram inside the cutout, are the **application's and the
decoder's/hardware's** responsibilities — not the library's. At the end of the day the hardware
and the decoder must meet their own requirements; the library's job is to encode/decode
correctly given conforming bytes, which the host suite already proves.

So an on-wire content check adds **no confidence in the library** — the decode logic it would
exercise is already proven on the host, more thoroughly than a wire capture could. We could not
identify an on-wire test of datagram content with value for the library, and there is no
transmitting decoder on the CS bench to produce that content in the first place.

**Possible future integration test (gated on hardware).** If a CS-side RailCom *receive* path is
ever wired, the receive→decode→assemble chain could be validated on real hardware with a
**mock-decoder loopback** — firmware driving 4/8 bytes onto the CS Rx line during the cutout,
mirroring the service-mode mock-ACK loopback (PB24→PB9). That would exercise the *application's*
UART-read timing and integration, still not the library's decode logic, and is out of scope
until a detector path exists.

## Other on-wire items not currently measured

These concern the CS's **own** cutout output — testable in principle, simply not yet in the suite:

- **Internal cutout phase boundaries** — the strobe marks only cutout begin/end, so the
  SETTLING/GAP/channel-switch sub-windows are not externally visible on PB2; measuring them
  would need the UART-enable transitions brought out to a probe.
- **DCC signal integrity across the cutout** — the suite checks a packet-count-vs-cutout-count
  match, not the main-track bit timing through the cutout window.

---

*Cross-reference: master [ComplianceMatrix.md](ComplianceMatrix.md) §8 · bench
[HIL_SETUP.md](../../test/compliance/ti_theia/saleae_hil_compliance/HIL_SETUP.md) · cutout
architecture [detail_s9_2_3_service_mode.md](detail_s9_2_3_service_mode.md).*
