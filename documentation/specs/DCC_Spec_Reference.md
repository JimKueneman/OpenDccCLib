# DCC Specification Reference — Implementation Notes

Quick-reference for implementing OpenDccCLib, distilled from NMRA S-9.x standards.
For authoritative details, consult the PDF specs in this directory.

---

## Table of Contents
1. [Electrical Layer (S-9.1)](#1-electrical-layer-s-91)
2. [Packet Format (S-9.2)](#2-packet-format-s-92)
3. [Extended Packet Formats (S-9.2.1)](#3-extended-packet-formats-s-921)
4. [Advanced Extended Packets (S-9.2.1.1)](#4-advanced-extended-packets-s-9211)
5. [Configuration Variables (S-9.2.2)](#5-configuration-variables-s-922)
6. [Service Mode Programming (S-9.2.3)](#6-service-mode-programming-s-923)
7. [Fail-Safe (S-9.2.4)](#7-fail-safe-s-924)
8. [Bi-Directional Communication / RailCom (S-9.3.2)](#8-bi-directional-communication--railcom-s-932)

---

## 1. Electrical Layer (S-9.1)

### Bit Encoding (Zero-Stretching)
- **One bit**: each half 58 µs nominal (55–61 µs allowed)
- **Zero bit**: each half ≥ 100 µs (command station); decoder must accept ≥ 90 µs
- Total zero-bit duration must not exceed 12000 µs
- Decoder must accept a bit whose first half is stretched (asymmetric zero)

### Signal Levels
- Command station output: ±(7–22) V across the rails (depending on scale)
- Decoder must operate at ±(7–24) V

### Baseline Packet Timing
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
- **Broadcast stop**: address `00000000`, instruction `01DC0001` (D=direction, C=0 for stop, 1 for emergency stop)
- **Reset packet**: `00000000 00000000 00000000` — broadcast reset

### Refresh / Timing
- Command station should repeat packets; no packet should be sent with < 5 ms gap
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
  - C=1: direction normal; C=0: direction reversed
  - Address 0 = remove from consist
- Consist address is stored in CV 19
- When in a consist, decoder responds to consist address for speed/direction, individual address for CV programming

#### CV Access (Operations Mode — OPS Mode Programming)

**Long form** (`1110CCVV VVVVVVVV DDDDDDDD`):
- CC: `01` = verify byte, `11` = write byte, `10` = bit manipulation
- VV VVVVVVVV: 10-bit CV address (add 1 for CV number: `0000000000` = CV 1)
- Bit manipulation data byte: `111CDBBB` (C=1 write, 0 verify; D=bit value; BBB=bit position)

**Short form** (`1111CCCC DDDDDDDD`):
- CCCC selects specific CV:
  - `0000` not available
  - `0010` = CV 23 (acceleration adjustment)
  - `0011` = CV 24 (deceleration adjustment)
  - `1001` = S-9.2.3 decoder lock (CV 15/16)

### Accessory Decoder Packets

#### Basic Accessory (9-bit address + output pair)
```
{preamble} 0 10AAAAAA 0 1AAACDDD 0 EEEEEEEE 1
```
- Address: 9 bits split across two bytes (note: high 3 bits in byte 2 are inverted)
- C = activate/deactivate
- DDD = output pair number (0–7)
- Board address range: 1–511

#### Extended Accessory (11-bit address + aspect)
```
{preamble} 0 10AAAAAA 0 0AAA0AA1 0 DDDDDDDD 0 EEEEEEEE 1
```
- 11-bit address for signal aspects
- DDDDDDDD = signal aspect data (0–255)

#### Broadcast Accessory
- Address `10111111 1000CDDD` = broadcast to all basic accessory decoders
- Address `10111111 00000111` = broadcast to all extended accessory decoders

---

## 4. Advanced Extended Packets (S-9.2.1.1)

### Binary State Control
- **Short form** (`11011101 DLLLLLLL`):
  - 127 binary states (1–127), D=state value (on/off)
  - L=0 is reserved
- **Long form** (`11000000 DLLLLLLL HHHHHHHH`):
  - 32767 binary states, D=state value
  - State number = `HHHHHHHH LLLLLLL` (high byte, low 7 bits)

### Analog Function Control
```
11011101 VVVVVVVV DDDDDDDD
```
- Wait — actually this is `11111101` instruction:
  - `11111101 VVVVVVVV DDDDDDDD [DDDDDDDD]`
  - V = analog function number (volume = 0x01, etc.)
  - D = analog value(s)

### Time & Date
- Feature expansion instruction for time/date display decoders

### System Time
- Broadcast with specific multi-function instruction format

### Speed Restriction
```
{preamble} 0 {address} 0 00111110 0 [XXXXXXXX] 0 DSSSSSSS 0 EEEEEEEE 1
```
- Advanced operations sub-instruction for restricting decoder speed

### Extended Consisting
- Provides additional consisting control beyond basic consist setup
- Supports function mapping within consists

---

## 5. Configuration Variables (S-9.2.2)

### Mandatory CVs
| CV  | Name                      | Notes                                  |
|-----|---------------------------|----------------------------------------|
| 1   | Primary Address           | 7-bit (1–127); bit 7 = 0              |
| 7   | Manufacturer Version      | Read-only                              |
| 8   | Manufacturer ID           | Read-only; write `8` to CV 8 → factory reset |
| 17  | Extended Address High     | Bits 7-6 = `11`                        |
| 18  | Extended Address Low      |                                        |
| 29  | Configuration Data #1     | Bit flags — see below                  |

### CV 29 Bit Definitions
| Bit | Value | Meaning                                      |
|-----|-------|----------------------------------------------|
| 0   | 1     | Direction reversed (0 = normal)              |
| 1   | 2     | 28/128 speed step mode (0 = 14-step)        |
| 2   | 4     | Analog conversion enabled                    |
| 3   | 8     | RailCom bi-directional enabled               |
| 4   | 16    | Speed table (0 = use Vstart/Vmid/Vmax; 1 = CV 67–94 table) |
| 5   | 32    | Use extended address (CVs 17-18) instead of CV 1 |

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
| 27   | Decoder lock config     |
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
- Page 0 is reserved for NMRA future use

### Accessory Decoder CVs
| CV  | Name                      |
|-----|---------------------------|
| 513 | Accessory address LSB     |
| 519 | Configuration data        |
| 521 | Manufacturer version      |
| 540 | Bi-directional config     |
| 541 | Accessory address MSB     |

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
{long preamble} 0 01111CVV 0 VVVVVVVV 0 111CDBBB 0 EEEEEEEE 1
```
- C = write/verify, D = bit value, BBB = bit position (0–7)

#### Paged Mode
- Uses register 6 (page register) to set high address bits
- CV address = (page register × 4) + register address
- Register write: `0111CCCC 0 DDDDDDDD 0 EEEEEEEE`

#### Physical Register Mode (legacy)
- Addresses registers 1–8 directly
- Register 5 selects 14/28-step mode

#### Address-Only Mode (minimal)
- Writes only the primary address (CV 1)

### Acknowledgment
- Decoder acknowledges by consuming ≥ 60 mA pulse for 5–7 ms
- Command station sends 3–5 identical packets, then looks for ACK
- Reset packets must be sent between programming sequences

### Decoder Lock
- CV 15 = key, CV 16 = compare value
- Decoder ignores writes when CV 15 ≠ CV 16 (except writes to CV 15 itself)

---

## 7. Fail-Safe (S-9.2.4)

### Packet Timeout
- If decoder receives no valid packet for the time defined by CV 11:
  - Must enter fail-safe state
  - CV 11 value × (see manufacturer) defines timeout
  - CV 11 = 0 means no timeout (run forever)

### Fail-Safe Behavior
- Speed: decoder should stop (or apply brake per manufacturer)
- Functions: controlled by CV 13 (F1-F8) and CV 14 (FL, F9-F12)
  - Each bit: 1 = function stays ON in fail-safe, 0 = function turns OFF
- Decoder must resume normal operation when valid packets are received again

---

## 8. Bi-Directional Communication / RailCom (S-9.3.2)

### Cutout Window
- Command station provides a "cutout" in the DCC signal after each packet end bit
- Channel 1: 88 µs after packet end — short window (~464 µs)
- Channel 2: starts after Ch1 — longer window (~1544 µs total from cutout start)

### Channel 1 (Short / Broadcast)
- 2 bytes (start + 6 data bits + stop per byte = 12-bit encoding)
- Used for: address feedback, basic acknowledgment
- Sent by ALL decoders that match the addressed packet

### Channel 2 (Application Data)
- Up to 6 bytes (same 4/8 encoding)
- Used for: CV value readback, dynamic data, DynID, speed, temperature, etc.
- Only the addressed decoder transmits

### Encoding
- 4/8 bit encoding: each 4-bit nibble encoded as an 8-bit byte for DC balance
- Specific encode/decode lookup table defined in the standard
- Error detection: 6 data bits per channel byte, with start/stop structure

### Datagram IDs (partial list)
| ID    | Content                     |
|-------|-----------------------------|
| 0     | Mobility (address feedback) |
| 1     | Mobility (address feedback) |
| 2     | Mobility (address feedback) |
| 7     | CV value (read back)        |
| 8     | CV value (read back)        |
| 15    | DCC ack                     |
| Others| Dynamic data, app-specific  |

### CV 28 (RailCom Configuration)
- Bit 0: Channel 1 enable
- Bit 1: Channel 2 enable
- Bit 2: Enable unsolicited datagram expansion
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
