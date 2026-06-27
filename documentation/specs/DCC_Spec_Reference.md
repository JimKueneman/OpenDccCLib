# DCC Specification Reference — Implementation Notes

Quick-reference for implementing OpenDccCLib, distilled from NMRA S-9.x standards.
For authoritative details, consult the PDF specs in this directory.

> Verified claim-by-claim against the released NMRA PDFs in this directory
> (S-9.1, S-9.2, S-9.2.1, S-9.2.2, S-9.2.3, S-9.2.4, S-9.3.2). Cross-standard
> references are noted inline where a fact is defined in a different standard.

---

## Table of Contents
1. [Electrical Layer (S-9.1)](#1-electrical-layer-s-91)
2. [Packet Format (S-9.2)](#2-packet-format-s-92)
3. [Extended Packet Formats (S-9.2.1)](#3-extended-packet-formats-s-921)
4. [Advanced Operations & Feature Expansion (S-9.2.1)](#4-advanced-operations--feature-expansion-s-921)
5. [Configuration Variables (S-9.2.2)](#5-configuration-variables-s-922)
6. [Service Mode Programming (S-9.2.3)](#6-service-mode-programming-s-923)
7. [Fail-Safe (S-9.2.4)](#7-fail-safe-s-924)
8. [Bi-Directional Communication / RailCom (S-9.3.2)](#8-bi-directional-communication--railcom-s-932)

---

## 1. Electrical Layer (S-9.1)

### Bit Encoding (Zero-Stretching)
- **One bit**: each half 58 µs nominal (55–61 µs allowed)
- **Zero bit**: each half 95 µs min / 100 µs nominal (command station); decoder must accept ≥ 90 µs and ≤ 10000 µs per half
- Total zero-bit duration must not exceed 12000 µs
- Decoder must accept a bit whose **first or last** part is stretched (asymmetric zero)

### Signal Levels
- Command station output (scale-dependent): min **8.5 V**, max 22 V (N/HO/S/O) or 24 V (large scale)
- Decoder must operate from **7 V** up to 24 V (N and smaller) or 27 V (HO and larger)

### Baseline Packet Timing
*(These framing rules are defined in **S-9.2**, repeated here for convenience; they are not part of the S-9.1 electrical standard.)*
- Preamble: minimum **14** one-bits (command station must send ≥ 14)
- Decoder must accept preamble of ≥ 10 one-bits
- Packet start bit: one zero-bit after preamble
- Data byte separator: one zero-bit between bytes
- Packet end bit: one one-bit after error detection byte

---

## 2. Packet Format (S-9.2)

### General Packet Structure
```
{preamble} 0 {address-byte} 0 {data-byte} 0 ... 0 {error-detection-byte} 1
```

- **Preamble**: ≥ 14 one-bits (≥ 20 for service mode)
- **Address byte**: 8 bits
- **Data byte(s)**: 8 bits each (variable count)
- **Error detection byte**: XOR of all preceding bytes (address + all data bytes)
- **Packet end bit**: single one-bit

### Address Ranges
*S-9.2 itself only reserves `00000000`, `11111110`, and `11111111`. The accessory / long-address partitioning below is defined in **S-9.2.1**, not S-9.2.*
| Address Bits   | Range        | Use                        |
|---------------|-------------|----------------------------|
| `00000000`     | 0           | Broadcast to all decoders  |
| `00000001–01111111` | 1–127  | Short (7-bit) multi-function decoder addresses |
| `10000000–10111111` | 128–191 | Accessory decoder addresses (9-bit, see S-9.2.1) |
| `11000000–11100111` | 192–231 | Long address high byte (14-bit, prefix `11AAAAAA`) |
| `11101000–11111110` | 232–254 | Reserved                   |
| `11111111`     | 255         | Idle packet address        |

### Key Packets
- **Idle packet**: `11111111 00000000 11111111` — all decoders must accept & ignore
- **Broadcast stop**: address `00000000`, instruction `01DC000S` — S (bit 0) = stop type (0 = bring to stop, 1 = stop delivering energy); D = direction; C (bit 4) = ignore-direction flag. The error-detection byte is a **copy of the instruction byte**, not a computed XOR.
- **Reset packet**: `00000000 00000000 00000000` — broadcast reset

### Refresh / Timing
- Command station must be configurable to send at least one complete packet every **30 ms** (start-bit to start-bit). The **5 ms** figure is the minimum end-bit-to-next-start-bit separation a decoder needs to act on consecutive packets addressed to it — not a "don't send faster" limit.
- Decoder must not act on a single packet until it has been received correctly (error detection must pass)

---

## 3. Extended Packet Formats (S-9.2.1)

### Multi-Function Decoder Instructions (short or long address)

#### Address Encoding
- **Short address** (7-bit): `0AAAAAAA` (1–127)
- **Long address** (14-bit): `11AAAAAA AAAAAAAA` (two bytes, 128–10239 usable)
  - High byte: bits 7-6 = `11`, bits 5-0 = high 6 bits of address
  - Low byte: low 8 bits of address

#### Instruction Byte Format
Bits 7-5 of the first instruction byte determine the instruction type:

| Bits 7-5 | Type                              |
|----------|-----------------------------------|
| `000`    | Decoder and Consist Control       |
| `001`    | Advanced Operations               |
| `010`    | Speed/Direction (reverse, 28-step)|
| `011`    | Speed/Direction (forward, 28-step)|
| `100`    | Function Group One (FL, F1–F4)    |
| `101`    | Function Group Two (F5–F8 or F9–F12) |
| `110`    | Future Expansion                  |
| `111`    | CV Access (long form & short form)|

#### Speed Step Modes

**14-step** (legacy baseline):
- Instruction: `01DCSSSS`, speed in lower 4 bits
- `SSSS=0000` → stop, `SSSS=0001` → emergency stop
- FL (headlight) controlled by bit D

**28-step**:
- Instruction: `01Dcssss`
- Effective speed = `cssss` (c is bit 4 from instruction bit 4, ssss is bits 3-0)
- `cssss=00000` → stop, `cssss=00001` → emergency stop
- Steps 2–29 map to speed steps 1–28

**128-step** (Advanced Operations `001`):
- Instruction: `00111111 DSSSSSSS`
- D = direction, SSSSSSS = speed (0 = stop, 1 = emergency stop, 2–127 = steps 1–126)

#### Function Groups

**Group One** (`100DDDDD`):
- Bit 4: FL (headlight), Bits 3-0: F4, F3, F2, F1

**Group Two** (`101SDDDD`):
- S=1: Bits 3-0 = F8, F7, F6, F5
- S=0: Bits 3-0 = F12, F11, F10, F9

**Feature Expansion** (`110CCCCC`):
- `11011110 DDDDDDDD`: F13–F20 (each bit = one function)
- `11011111 DDDDDDDD`: F21–F28
- `11011000 DDDDDDDD`: F29–F36
- `11011001 DDDDDDDD`: F37–F44
- `11011010 DDDDDDDD`: F45–F52
- `11011011 DDDDDDDD`: F53–F60
- `11011100 DDDDDDDD`: F61–F68

#### Consisting (Multi-Unit)

- **Set consist address**: `0001001C 0AAAAAAA`
  - Last instruction bit = 0 → direction **normal** (CV19 bit 7 = 0); = 1 → **reversed** (CV19 bit 7 = 1)
  - Address 0 = remove from consist
- Consist address is stored in CV 19
- When in a consist, decoder responds to consist address for speed/direction, individual address for CV programming

#### CV Access (Operations Mode — OPS Mode Programming)

**Long form** (`1110GGVV VVVVVVVV DDDDDDDD`):
- GG: `01` = verify byte, `11` = write byte, `10` = bit manipulation
- VV VVVVVVVV: 10-bit CV address (add 1 for CV number: `0000000000` = CV 1)
- Bit manipulation data byte: `111FDBBB` (F=1 write, 0 verify; D=bit value; BBB=bit position). *S-9.2.1 ops mode uses F here; S-9.2.3 service mode uses K for the same field.*

**Short form** (`1111GGGG DDDDDDDD`):
- GGGG selects specific CV(s):
  - `0000` not available
  - `0010` = CV 23 (acceleration adjustment)
  - `0011` = CV 24 (deceleration adjustment)
  - `0100` = Long Address (CV 17, 18, 29)
  - `0101` = Indexed CVs (CV 31, 32)
  - `0110` = Long Consist Address (CV 19, 20)
  - `1001` = see S-9.2.3 Appendix B

**XPOM (Extended Program on Main)** — `1110GGSS` form carrying a 24-bit indexed CV
address; reads or writes up to 4 contiguous CVs (S-9.2.1 §2.3.7.4).

### Accessory Decoder Packets

#### Basic Accessory (9-bit address + output pair)
```
{preamble} 0 10AAAAAA 0 1AAADAAR 0 EEEEEEEE 1
```
- Byte 2 layout `1 AAA D AA R`: the high 3 address bits (after the leading `1`) are **inverted**; D = activate/deactivate; R selects which output of the pair
- Board address range: 1–511 (9-bit)
- *(Legacy notation wrote byte 2 as `1AAACDDD` with a 3-bit output field; current S-9.2.1 uses `1AAADAAR`.)*

#### Extended Accessory (11-bit address + aspect)
```
{preamble} 0 10AAAAAA 0 0AAA0AA1 0 DDDDDDDD 0 EEEEEEEE 1
```
- 11-bit address for signal aspects (the high 3 address bits in byte 2 are **inverted**, as in the basic accessory packet)
- DDDDDDDD = signal aspect data (0–255)

#### Accessory Broadcast / NOP
- The only broadcast address defined is `00000000`. (The `10111111`-prefixed accessory-broadcast forms previously listed here are **not defined in S-9.2.1** and have been removed.)
- S-9.2.1 defines a **NOP** command (`10AAAAAA 0 0AAA1AAT`) that lets bi-directional accessory decoders raise an SRQ without changing output state (T selects basic/extended).

---

## 4. Advanced Operations & Feature Expansion (S-9.2.1)

> **Note on attribution:** these features (Binary State Control, Analog Function,
> Time/Date, System Time) are part of **S-9.2.1** (Advanced Operations group `001`
> and Feature Expansion group `110`).
> They were previously filed under "S-9.2.1.1." The **released S-9.2.1.1** is a
> different standard entirely — the Logon auto-registration protocol, address
> partitions 253/254, CRC-8+XOR error detection, and Data Spaces.

### Binary State Control
- **Short form** (`11011101 DLLLLLLL`):
  - 127 binary states (1–127), D=state value (on/off)
  - L=0 is reserved
- **Long form** (`11000000 DLLLLLLL HHHHHHHH`):
  - 32767 binary states, D=state value
  - State number = `HHHHHHHH LLLLLLL` (high byte, low 7 bits)

### Analog Function Control
```
00111101 VVVVVVVV DDDDDDDD
```
- Instruction byte `00111101` (CCC=001 Advanced Operations, GGGGG=11101).
  (Earlier drafts of this note listed `11111101` — that was incorrect.)
- V = analog function number (`00000001` = Volume Control; all other values reserved)
- D = analog value(s)
- Must NOT be used to control decoder speed.

### Time & Date
- Feature expansion instruction for time/date display decoders

### System Time
- Broadcast with specific multi-function instruction format

### Reserved — Zimo East-West Direction (`00111110`)
- Instruction byte `00111110` (Advanced Operations, GGGGG=11110) is **reserved for the Zimo East-West Direction proposal** (S-9.2.1 §2.3.2.2) — under development, with **no payload defined** in the standard.
- *Correction: earlier notes labeled this "Speed Restriction" with a `[XXXXXXXX] DSSSSSSS` payload. That format is not in S-9.2.1 and has been removed.*

### Consist Function Mapping
- S-9.2.1 has no separate "extended consisting" packet. Consist setup is the single instruction `0001001C 0AAAAAAA` (above); per-function behavior within a consist is governed by **CV21/CV22** (defined in S-9.2.2), not a distinct S-9.2.1 packet.

---

## 5. Configuration Variables (S-9.2.2)

### Key CVs (mandatory unless noted)
| CV  | Name                      | Notes                                  |
|-----|---------------------------|----------------------------------------|
| 1   | Primary Address           | Mandatory. 7-bit (1–127); bit 7 = 0    |
| 7   | Manufacturer Version      | Manufacturer-stored version (S-9.2.2 does not state read-only) |
| 8   | Manufacturer ID           | Mandatory, read-only (NMRA-assigned). Factory reset by writing CV 8 is a common **manufacturer convention**, not part of S-9.2.2 |
| 17  | Extended Address High     | **Optional**; bits 7-6 = `11`. Used only with long addressing |
| 18  | Extended Address Low      | **Optional** (paired with CV17)        |
| 29  | Configuration Data #1     | Mandatory if any configurable feature is provided. Bit flags — see below |

### CV 29 Bit Definitions
| Bit | Value | Meaning                                      |
|-----|-------|----------------------------------------------|
| 0   | 1     | Direction reversed (0 = normal)              |
| 1   | 2     | Speed-step mode in practice (0 = 14-step, 1 = 28/128). *The literal 2012 S-9.2.2 text labels bit 1 "FL location"; common practice / RP-9.2.1 use it as the 14 vs 28/128 selector.* |
| 2   | 4     | Power-source conversion enabled (see CV 12) |
| 3   | 8     | RailCom (bi-directional) enabled             |
| 4   | 16    | Speed table (0 = use CV 2/5/6; 1 = use speed-table CVs). *CV29 bit-4 text cites CVs 66–95; data CVs are 67–94.* |
| 5   | 32    | Use extended address (CVs 17-18) instead of CV 1 |
| 7   | 128   | Decoder type: 0 = multifunction, 1 = accessory |

### Common CVs
| CV   | Name                    |
|------|-------------------------|
| 2    | Vstart (start voltage)  |
| 3    | Acceleration rate       |
| 4    | Deceleration rate       |
| 5    | Vhigh (max voltage)     |
| 6    | Vmid (mid voltage)      |
| 9    | Total PWM period        |
| 10   | EMF feedback cutoff     |
| 11   | Packet timeout value    |
| 13   | Analog mode function status (F1–F8) |
| 14   | Analog mode function status (FL, F9–F12) |
| 15   | Decoder lock key        |
| 16   | Decoder lock compare    |
| 17-18| Extended (long) address |
| 19   | Consist address (0 = not in consist, bit 7 = direction) |
| 21   | Consist function enable F1–F8 |
| 22   | Consist function enable FL, F9–F12 |
| 23   | Acceleration adjustment |
| 24   | Deceleration adjustment |
| 27   | Decoder Automatic Stopping Configuration |
| 28   | RailCom configuration   |
| 29   | Configuration Data #1   |
| 33-46| Function mapping outputs |
| 47-64| Manufacturer-specific   |
| 65   | Kick start              |
| 66   | Forward trim            |
| 67-94| Speed table (28 entries)|
| 95   | Reverse trim            |
| 105-106| User ID bytes         |
| 112-256| Manufacturer-specific |

### Indexed CVs (CV 31-32 as page pointer)
- CV 31 (high byte) + CV 32 (low byte) form a page pointer
- CVs 257-512 are accessed via this index mechanism
- Pages 0–4095 (CV31 = 0x00–0x0F) are reserved for NMRA use

### Accessory Decoder CVs
| CV  | Name                      |
|-----|---------------------------|
| 513 | Decoder Address LSB (≙ CV1)              |
| 519 | Manufacturer Version (≙ CV7)            |
| 520 | Manufacturer ID (≙ CV8)                 |
| 521 | Decoder Address MSB (≙ CV9)             |
| 540 | Bi-Directional Communication Config (≙ CV28) |
| 541 | Accessory Decoder Configuration (≙ CV29) |

---

## 6. Service Mode Programming (S-9.2.3)

### Preamble
- Service mode requires **≥ 20** preamble bits (vs. 14 in normal operations)

### Programming Modes

#### Direct Mode (preferred)
**Byte write/verify** — CV Access Long Form:
```
{long preamble} 0 0111CCVV 0 VVVVVVVV 0 DDDDDDDD 0 EEEEEEEE 1
```
- CC: `01` = verify, `11` = write
- 10-bit CV address (CV 1 = `0000000000`)

**Bit manipulation**:
```
{long preamble} 0 011110AA 0 AAAAAAAA 0 111KDBBB 0 EEEEEEEE 1
```
- K = write (1) / verify (0), D = bit value, BBB = bit position (0–7)

#### Paged Mode
- Uses Register 6 (RRR=101, the Paging Register, default 1); data registers are 1–4
- CV address = **((page register − 1) × 4) + data register + 1**
- Register write/verify: `0111CRRR 0 DDDDDDDD 0 EEEEEEEE` (C = write/verify, RRR = register)
- A Page-Preset packet sets the page before access

#### Physical Register Mode (legacy)
- Addresses registers 1–8 directly
- Register 5 = Basic Configuration Register (CV 29 for mobile / CV 541 for accessory)

#### Address-Only Mode (minimal)
- Writes only the primary address (CV 1)

### Acknowledgment
- Decoder acknowledges by consuming ≥ 60 mA for **6 ms ± 1 ms**
- The command station sends mode-specific packet counts (e.g. Direct: 5+ command packets then 6+ recovery; Address-only: 10+ recovery) — not a fixed 3–5
- Reset packets must be sent between programming sequences

### Decoder Lock
*(Defined in S-9.2.2 / RP-9.2.x, not S-9.2.3 — included here for service-mode context.)*
- CV 15 = key, CV 16 = compare value
- Decoder ignores writes when CV 15 ≠ CV 16 (except writes to CV 15 itself)

---

## 7. Fail-Safe (S-9.2.4)

### Packet Timeout
- If a decoder receives no command packet **addressed to it** within the time defined by CV 11:
  - It **shall bring all controlled devices to a stop** (normative)
  - CV 11 = 1…TIMEOUT_MAX sets the timeout; TIMEOUT_MAX is at least **20 seconds**
  - CV 11 = 0 disables the timeout (no timeout)

### Fail-Safe Behavior
- Speed: the decoder **shall stop** all controlled devices (S-9.2.4 defines no "brake" option for the timeout case)
- The decoder resumes normal operation when valid addressed packets are received again (implied; S-9.2.4 §4 does not state this explicitly for the timeout case)

*Note: CV 13 (F1–F8) and CV 14 (FL, F9–F12) govern function state in the **alternate-power-source / analog conversion** case (S-9.2.2), not the packet-timeout fail-safe — they were previously misattributed here.*

---

## 8. Bi-Directional Communication / RailCom (S-9.3.2)

### Cutout Window (released S-9.3.2, all measured from the last edge of the packet end bit)
- The cutout starts at **T_CS = 26–32 µs** and ends at **T_CE = 454–488 µs** (total ≈ 450 µs)
- **Channel 1**: starts T_TS1 = **80 µs**, ends T_TC1 ≤ **177 µs**
- **Channel 2**: starts T_TS2 = **193 µs**, ends T_TC2 ≤ **454 µs**
- Data rate 250 kbit/s ±2%

### Channel 1 (Short / Broadcast)
- 2 bytes; each byte on the wire = start bit + **8 data bits** (4/8 line code) + stop bit. Net payload = 12 bits (2 × 6)
- Used for: address feedback (adr_low / adr_high)
- Sent by ALL decoders that match the addressed packet

### Channel 2 (Application Data)
- Up to 6 bytes (same 4/8 encoding)
- Used for: CV value readback, dynamic data, DynID, speed, temperature, etc.
- Only the addressed decoder transmits

### Encoding
- 4/8 bit encoding: each 6-bit payload value is encoded as an 8-bit codeword containing four 1s and four 0s (DC balance)
- Specific encode/decode lookup table defined in the standard
- Each 4/8 codeword carries 6 payload bits; Channel 1 = 12 net bits, Channel 2 = up to 36 net bits

### Datagram IDs (released S-9.3.2, mobile decoder / MOB)
| ID    | Content                     |
|-------|-----------------------------|
| 0     | POM — CV readback (Channel 2) |
| 1     | adr_low — address feedback (Channel 1) |
| 2     | adr_high — address feedback (Channel 1) |
| 7     | dyn — dynamic data (optional) |
| 8, 15 | Not approved for use (2012 release) |

*ACK / NACK / BUSY are 6-bit **special codes** in the 4/8 table, not datagram IDs. Separate STAT datagrams exist for accessory decoders.*

### CV 28 (RailCom Configuration, 2012 release)
- Bit 0: Channel 1 enable
- Bit 1: Channel 2 enable
- Bits 2–3: Not approved for use (2012 release)
- Bit 4: Programming address 253 approved
- CV 29 bit 3 must also be set to globally enable RailCom

---

## Implementation Priority Checklist

For building OpenDccCLib, the recommended implementation order:

1. **Packet construction core** — preamble, byte framing, XOR error detection
2. **Address encoding** — short, long, broadcast, idle
3. **Speed commands** — 14-step, 28-step, 128-step
4. **Function commands** — Group 1 (FL,F1–F4), Group 2 (F5–F12), expansion (F13–F68)
5. **Accessory packets** — basic (turnouts), extended (signals)
6. **CV access (ops mode)** — long form read/write/bit
7. **Service mode programming** — direct byte, direct bit, paged
8. **Consisting** — set/clear consist address, function mapping
9. **Packet decoding** — parse received packets back to structured data
10. **RailCom** — 4/8 encoding/decoding, channel framing
11. **Fail-safe** — timeout logic, safe-state function mapping

---

## Byte/Bit Notation Convention

Throughout the specs:
- Bit numbering: bit 0 = LSB, bit 7 = MSB
- Multi-byte fields: first transmitted byte is listed first (MSB first / big-endian on the wire)
- CV numbering: starts at 1 (but address encoding uses 0-based: CV 1 = address `0000000000`)

---

*Generated from NMRA Standards S-9.1, S-9.2, S-9.2.1, S-9.2.1.1, S-9.2.2, S-9.2.3, S-9.2.4, and S-9.3.2.
For authoritative reference, see the original PDFs in this directory.*
