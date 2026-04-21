# DCC Draft Specification Deltas

Changes between the current released NMRA S-9.x standards and the draft revisions
under review (downloaded Apr 13, 2026 from [NMRA Documents Under Revision](https://www.nmra.org/documents-under-revision)).

Use this alongside [DCC_Spec_Reference.md](DCC_Spec_Reference.md) to track what our
implementation will need to handle when drafts are ratified.

---

## Table of Contents
1. [Extended Packet Formats (S-9.2.1)](#1-extended-packet-formats-s-921)
2. [Advanced Extended Packets (S-9.2.1.1)](#2-advanced-extended-packets-s-9211)
3. [Configuration Variables (S-9.2.2)](#3-configuration-variables-s-922)
4. [Bi-Directional Communication / RailCom (S-9.3.2)](#4-bi-directional-communication--railcom-s-932)
5. [E24 Decoder Interface (S-9.1.1.6) — NEW](#5-e24-decoder-interface-s-9116--new)
6. [SUSI Bus (S-9.4.x) — NEW](#6-susi-bus-s-94x--new)

---

## 1. Extended Packet Formats (S-9.2.1)

Draft posted: 15-Nov-2025

### 1.1 Address Partition Change

The reserved range is split:

| Range | Old | New |
|-------|-----|-----|
| 232–252 | Reserved | Reserved for Future Use |
| 253–254 | Reserved | **Advanced Extended Packet Formats (S-9.2.1.1)** |

Addresses 253 and 254 are no longer generic reserved — they are specifically allocated
to the Logon/Data Space protocol defined in S-9.2.1.1.

### 1.2 Decoder and Consist Control (CCC=000)

#### Hard Reset (NEW)

Format: `0000000F`
- F=0: Digital Decoder Reset (existing — erase volatile memory, return to power-up state)
- F=1: **Hard Reset** — CVs 29, 31, 32 reset to factory default, CV 19 set to `00000000`, then a normal Digital Decoder Reset is performed

Command stations shall not send packets to addresses 112–127 for 10 packet times
following a Digital Decoder Reset.

#### TTT Sub-Instructions (NEW detail)

| TTT | Function |
|-----|----------|
| 000 | Digital Decoder Reset (F=0) / Hard Reset (F=1) |
| 001 | Factory Test Instruction (manufacturer use) |
| 010 | Reserved |
| 011 | Reserved |
| 100 | Reserved |
| 101 | Set Advanced Addressing — F=0: short addr (CV1), F=1: long addr (CV17/18) |
| 110 | Reserved |
| 111 | Decoder Acknowledgment Request (F=1 only) |

#### Consist Control — Notation Change

Old: `0001001C 0AAAAAAA` (C=1 normal, C=0 reversed)

New uses TTTT encoding:
- TTTT=0010: activate consist, direction **normal** (CV19 bit 7 = 0)
- TTTT=0011: activate consist, direction **reversed** (CV19 bit 7 = 1)

Functionally identical but different notation. Also adds:
- Functions in Group One/Two continue responding to baseline address
- Bi-directional communication NOT activated for consist address by default
- When consist deactivated (address=0), bi-di settings revert to CVs 26–28

### 1.3 Advanced Operations (CCC=001)

#### 128-Step Speed — Clarification

No functional change. Draft uses `DDDDDDDD` notation: bit 7 = direction ("1" forward),
bit 6 = MSB of speed. `U0000000` = stop, `U0000001` = e-stop, 2–127 = 126 speed steps.

#### Zimo East-West Direction (GGGGG=11110) — NEW

Instruction `00111110`, one data byte:

| Bits | Function |
|------|----------|
| 0–1 | Reserved (00) |
| 3–2 | MU position: 00=not in MU, 10=Lead, 01=Middle, 11=Rear |
| 4 | Shunting button (1=shunt mode on) |
| 5 | West bit |
| 6 | East bit |
| 7 | MAN function (local speed limit / stop via DCC signal modification) |

#### Analog Function Group — MOVED and CORRECTED

**Bug in existing reference**: instruction byte was listed as `11111101`. Correct byte
is `00111101` (CCC=001, GGGGG=11101).

This instruction is now in S-9.2.1 (was incorrectly attributed to S-9.2.1.1):
```
00111101 0 VVVVVVVV 0 DDDDDDDD
```
- V=`00000001`: Volume Control
- All other V values reserved
- Must NOT be used to control decoder speed

#### Target Speed (GGGGG=11011) — NEW

```
00111011 0 DDDDDDDD
```
- DDDDDDDD = target speed and direction (128-step or 28-step format per decoder mode)
- Purpose: transmit target speed at beginning of a speed change so decoders can use
  their own momentum (CVs 3/4) for realistic driving sounds
- **Has no influence on actual driving behavior**

### 1.4 Function Groups — Minor Clarifications

- Any function in Group One (`100`) or Group Two (`101`) may be "directionally qualified"
- Ops-mode acknowledgment required when enabled
- F13–F20 status should be saved in NVRAM (recommended)
- Command stations that don't periodically refresh must send at least 2 repetitions on change

### 1.5 Feature Expansion (CCC=110)

#### Binary State Control Long Form (GGGGG=00000) — MOVED from S-9.2.1.1

Now in S-9.2.1. 3-byte instruction: `11000000 0 DLLLLLLL 0 HHHHHHHH`
- 32767 binary states (1–32767)
- Address 0 = broadcast clear/set ALL states
- Binary States 1–15 reserved for NMRA Bi-directional communication (S-9.3.2)

#### Time Command (GGGGG=00001, CC=00) — NEW full definition

Previously had no byte-level definition. Now fully specified:
```
{preamble} 0 00000000 0 11000001 0 00MMMMMM 0 WWWHHHHH 0 U0BBBBBB 0 EEEEEEEE 1
```
- Always broadcast (address 0)
- MMMMMM = minutes (0–59)
- WWW = day of week (0=Mon … 6=Sun, 7=not supported)
- HHHHH = hours (0–23)
- U = update flag (1=time changed significantly)
- BBBBBB = acceleration factor (0=stopped, 1=real time, 2=2x, …63=63x)

#### Date Command (CC=01) — NEW

```
{preamble} 0 00000000 0 11000001 0 010TTTTT 0 MMMMYYYY 0 YYYYYYYY 0 EEEEEEEE 1
```
- TTTTT = day (1–31), MMMM = month (1–12)
- YYYYYYYYYYYY = year (0–4095, split across bytes 4 and 5)
- Broadcast at least 3 times on change

#### System Time (GGGGG=00010) — NEW full definition

```
{preamble} 0 00000000 0 11000010 0 MMMMMMMM 0 MMMMMMMM 0 EEEEEEEE 1
```
- 16-bit millisecond counter since system startup (wraps at 65535)
- Byte 3 = MSB, Byte 4 = LSB
- Timestamp refers to beginning of start bit
- Recommended: send every ~30 seconds

#### Binary State Control Short Form (GGGGG=11101) — MOVED from S-9.2.1.1

Now in S-9.2.1. `11011101 0 DLLLLLLL` — 127 binary states (1–127).
Binary States 1–15 reserved for bi-directional communication.
Non-volatile storage recommended (binary state packets are NOT refreshed).

### 1.6 CV Access (CCC=111)

#### Short Form — Expanded

Now 3 instruction bytes for paired-CV variants:
```
1111GGGG 0 DDDDDDDD 0 DDDDDDDD
```

New GGGG values:

| GGGG | Description | CVs | Two Identical Packets Required |
|------|-------------|-----|-------------------------------|
| 0000 | Not available | — | — |
| 0010 | Acceleration Adj | CV 23 | NO |
| 0011 | Deceleration Adj | CV 24 | NO |
| **0100** | **Long Address** | **CV 17, 18, 29** | **YES** |
| **0101** | **Indexed CVs** | **CV 31, 32** | **YES** |
| **0110** | **Long Consist Address** | **CV 19, 20** | **YES** |
| 1001 | See S-9.2.3 Appendix B | — | — |

For paired CVs: second command byte = lower CV number, third byte = higher CV number.

#### Long Form — Notation Change

Uses `GG` instead of `CC`, `F` instead of `C` for bit manipulation flag. Same functionality:
- GG=00: Reserved
- GG=01: Verify byte
- GG=11: Write byte
- GG=10: Bit manipulation (`111FDBBB`, F=1 write, F=0 verify)

Two identical packets required. Any other packet to same decoder invalidates the write.

#### XPOM (Extended Program on Main) — NEW

```
1110GGSS 0 VVVVVVVV 0 VVVVVVVV 0 VVVVVVVV 0 {DDDDDDDD ...}
```
- SS = sequence number for bi-directional feedback (00 without bi-di)
- 3 V-bytes = 24-bit indexed CV address (CV31 + CV32 + offset)
- GG=01: Read bytes (no data bytes; contents returned via bi-di)
- GG=11: Write up to 4 sequential CVs
- GG=10: Write bits (5th byte = `1111DBBB`)
- **Intentionally violates 6-byte packet limit** — decoder integrity ensured by requiring two identical packets

### 1.7 Accessory Decoder Packets

#### Basic Accessory — Notation Change

Old byte 2: `1AAACDDD`
New byte 2: `1AAA_DAAR` (harmonized with RCN-213)

Mapping: old C = new D (activate/deactivate), old DDD bit 0 = new R (output pair select).

New R bit convention:
- R=0: diverging / travel left / signal stop
- R=1: normal / travel right / signal proceed

#### Linear vs Non-Linear Addressing (NEW)

Two addressing conventions for user-facing address mapping:
- **Linear**: User address 1–2048 (required for new designs)
- **Non-Linear**: Legacy, different wrap-around behavior

Decoder makes no distinction — both are valid on the wire.

#### Extended Accessory — Timed Output Mode (NEW)

When used for timed output, byte 3 format:
- Bit 7 (R): selects output within pair
- Bits 6–0 (ZZZZZZZ): 14-bit timed value (100 ms resolution, across 2 bytes)
- `0000000` = off, `1111111` = continuous

#### Accessory XPOM (NEW)

Both basic and extended accessory decoders support XPOM via the same instruction
format as multi-function decoders. Distinguished from operating commands by packet length.

#### NOP Command for Accessory Decoders (NEW)

```
{preamble} 0 10AAAAAA 0 0AAA_1AAT 0 EEEEEEEE 1
```
- T=0: basic accessory, T=1: extended accessory
- Enables bi-directional capable accessory decoders to send SRQ without changing status
- Ignored by non-bi-directional decoders

### 1.8 Operations Mode Acknowledgment (NEW section)

The S-9.3.2 mechanism is the **only** valid acknowledgment in operations mode.

### 1.9 New Notation Symbols

| Symbol | Meaning |
|--------|---------|
| G | Instruction Sub Type |
| T | Instruction Tertiary Type |
| F | Flag (instruction implementation) |
| R | Paired State |
| Z | Timed value |
| S | Decoder Sub Address / Sequence Number |
| X | Signal Head Aspect Data Bit |

---

## 2. Advanced Extended Packets (S-9.2.1.1)

Draft posted: 23-Jun-2024

### 2.1 CRITICAL: Existing Reference Section Is Misattributed

**Everything in the current DCC_Spec_Reference.md section 4 (Binary State Control, Analog
Function, Time/Date, System Time, Speed Restriction, Extended Consisting) belongs in
S-9.2.1, NOT S-9.2.1.1.**

S-9.2.1.1 actually defines an entirely different feature set: the **Logon auto-registration
protocol**, address partitions 253/254, extended error detection, Data Spaces, and bulk
data transfer.

### 2.2 Address Partitions 253 and 254

- **Partition 253** (`11111101`): Addressed commands to specific decoders using extended address format
- **Partition 254** (`11111110`): Broadcast/unaddressed commands (Logon, Select, Get Data, etc.)

### 2.3 Extended Address Format

| Address Type | Byte 1 | Byte 2 |
|-------------|--------|--------|
| 14-bit DCC | `xx000000`–`xx100111` | `AAAAAAAA` (CV 18) |
| 11-bit Accessory | `xx101000`–`xx101111` | `AAAAAAAA` (CV 1) |
| 9-bit Accessory | `xx110000`–`xx110111` | `AAAAAANN` (NN = output pair) |
| 7-bit DCC | `xx111000` | `0AAAAAAA` (CV1) |
| Broadcast | `xx111000` | `00000000` |
| Reserved | `xx111001`–`xx111111` | |

MSBs of Byte 1 (`TT`) encode command type for partition 253.

### 2.4 Dual Checksum (CRC-8 + XOR)

- Packets <= 6 bytes: standard XOR only
- Packets > 6 bytes: **CRC-8 inserted before XOR byte**
- CRC-8 polynomial: **x^8 + x^5 + x^4 + 1** (Dallas/Maxim 1-Wire), init=0, not inverted
- **7-byte packets are forbidden** (must not be sent; decoder must ignore)
- Maximum packet length: **32 bytes**

### 2.5 Feedback Rules

- **Partition 253**: Channel 2 only; channel 1 reserved. Mobile decoder must NOT respond in channel 1.
- **Partition 254**: Channels 1 & 2 combined into single extended-length channel. CV28 bit 1 controls permission.

Variable-length feedback header: `XRCLLLLL`
- X: 0=regular, 1=special (single cutout)
- R: 0=response to request, 1=unsolicited
- C: 0=first message, 1=continuation
- LLLLL: payload length (total bytes = Length + 2)

### 2.6 Acknowledgement Rules

- Failed checksum: no feedback
- Partition 254 with no specific response: **8 ACK bytes** (filling ch1+ch2)
- Partition 253 with no specific response: **6 ACK bytes** (filling ch2 only)

### 2.7 Error Codes

| Code | Meaning |
|------|---------|
| 0x1000 | Unspecified permanent error |
| 0x1040 | Unimplemented |
| 0x1042 | Command not supported |
| 0x1080 | Invalid arguments |
| 0x1081 | Data space number unknown |
| 0x1082 | Write offset out of bounds |
| 0x1083 | Data space is read-only |
| 0x2000 | Unspecified temporary error |
| 0x2010 | Timeout |
| 0x2020 | Buffer unavailable / decoder busy |
| 0x2040 | Unexpected sequence of operations |
| 0x2041 | Decoder internal state reset |

### 2.8 Partition 253 Commands (Addressed)

General form:
```
{preamble} 0 11111101 0 TTAAAAAA 0 AAAAAAAA 0 {command} 0 {payload} 0 {checksum} 1
```

TT bits:

| TT | Type |
|----|------|
| 11 | Addressed |
| 10 | Addressed Continue |
| 01 | Addressed Control |
| 00 | Addressed S-9.2/S-9.2.1 Chained |

Addressed command bytes:

| Byte | Command |
|------|---------|
| `0000HHHH` | Manufacturer Specific |
| `11111100` | WriteBlock |
| `11111101` | ReadBackground |
| `11111110` | ReadBlock |

Chained mode: multiple standard S-9.2.1 instruction sets in one packet, max 16 bytes.

### 2.9 Partition 254 Commands (Broadcast)

```
{preamble} 0 11111110 0 {command} 0 {parameters} 0 {checksum} 1
```

| Command Byte | Name |
|-------------|------|
| `00000000` | Get Data Start |
| `00000001` | Get Data Continue |
| `1101HHHH` | Select |
| `1110HHHH` | Logon Assign |
| `111111GG` | Logon Enable |

### 2.10 Logon Protocol

**Key Identifiers:**
- **DID**: 44-bit Decoder ID = 12-bit Manufacturer ID + 32-bit Unique ID
- **CID**: 16-bit Command Station ID (hash of manufacturer + unique ID)
- **Session ID**: 8-bit, incremented each reboot, rolls 255→0

#### Logon Enable (`111111GG`)

Sent periodically, **at least every 300 ms**:
```
{preamble} 0 11111110 0 111111GG 0 CID[15:8] 0 CID[7:0] 0 SessionID 0 {checksum} 1
```
- GG=00: ALL decoders, GG=01: LOCO only, GG=10: ACC only, GG=11: NOW (all, reset backoff)
- Feedback: full 44-bit DID

**Back-off algorithm**: random skip counts escalating 0–7, 0–15, 0–31, 0–63 (capped).

#### Select (`1101HHHH`)

Addresses decoder by full DID. Sub-commands:
- `11111111`: ReadShortInfo
- `11111110`: ReadBlock
- `11111011`: Set Decoder Internal Status

ReadShortInfo response: `1RAAAAAA AAAAAAAA FFFFFFFF PPPPPPPP PPPPPPPP CRC-8`
- A = suggested address, F = highest function, P = protocol support flags

#### Logon Assign (`1110HHHH`)

Assigns session-specific DCC address. Does NOT modify CV1/CV17/CV18/CV19 or CV513/CV521.

Feedback: change flags + 16-bit change counter + protocol support flags.

**CV 28 bit 7** must be set to enable Logon. If clear, decoder ignores all 254 commands
0xE0–0xFF but NOT Get Data or Select.

#### Decoder Startup Behavior

- If no Logon Enable received within **700 ms**, decoder uses permanent address (CV1/17/18)
- Error indication (mobile): double blink of front+rear lights, 1–2 second period

### 2.11 Data Spaces

| Number | Name | Read-Only |
|--------|------|-----------|
| 0 | Capabilities | Yes |
| 1 | Data Space Info | Yes |
| 2 | Short GUI | No |
| 3 | Configuration Variables | No |
| 4–255 | Reserved | |

#### WriteBlock (command `11111100`)

Writes to data space at specified offset. Feedback codes:
- `10000001` = success
- `10000010` = permanent error
- `10000011` = temporary error
- `10000000` = still in progress

700 ms timeout; decoder sends in-progress every 500 ms.

#### ReadBackground (command `11111101`)

Data piggybacks in channel 2 of any addressed 253 packet. First chunk ID13, subsequent ID14.

#### ReadBlock (command `11111110`)

Fast read using Get Data Start/Continue. CRC-8 seed = data space number.

### 2.12 Capabilities Data Space (Space 0)

| Byte | Bit | Capability |
|------|-----|-----------|
| 0 | 6 | XPOM supported |
| 0 | 5 | Select+ReadBlock supported |
| 0 | 4 | ReadBlock supported |
| 0 | 3 | ReadBackground supported |
| 1 | 3 | CV R/W data space supported |
| 1 | 2 | Short GUI data space supported |
| 1 | 1 | Data Space Info supported |
| 1 | 0 | Capabilities data space supported |
| 2 | 7 | Addressed S-9.2/S-9.2.1 Chained supported |
| 3 | 0 | Extended capabilities (byte 4+) |

### 2.13 Short GUI Data Space (Space 2)

| Bytes | Content |
|-------|---------|
| 0–7 | User-defined short name (UTF-8, 0x00 padded) |
| 8–9 | Image index |
| 10 | Bits 7–6: F0 info, bits 3–0: principal symbol |
| 11–28 | F1–F68 function info (2 bits per function) |

Principal symbols (mobile): 0000=Steam, 0001=Diesel, 0010=Electric, 0011=Diesel Railcar,
0100=Electric Railcar, 0101=Cab Car/FRED, 0110=Passenger, 0111=Caboose, 1000=MOW,
1001=Generic Function, 1010–1101=Auto variants, 1110=Other Mobile, 1111=Other non-mobile.

Function info: 00=undefined, 01=latching, 10=momentary, 11=trigger.

### 2.14 CV Space Overlay (Data Spaces → Indexed CVs)

CV31=2 for all standard data spaces:

| CV31 | CV32 | Data Space |
|------|------|-----------|
| 2 | 0 | Capabilities |
| 2 | 1 | Data Space Info |
| 2 | 2 | Short GUI |

### 2.15 Requirements Matrix

| Feature | Logon | Throttle UI | Bulk Write |
|---------|-------|------------|-----------|
| Select (ReadShortInfo) | Mandatory | — | — |
| Logon Assign | Mandatory | — | — |
| Logon Enable | Mandatory | — | — |
| POM | — | Mandatory (min) | — |
| WriteBlock | — | — | Mandatory |
| Addressed Continue | — | — | Mandatory |

---

## 3. Configuration Variables (S-9.2.2)

Draft posted: 11-Feb-2026

### 3.1 New CVs

| CV | Name | Req | Notes |
|----|------|-----|-------|
| 12 | Power Source Conversion | O | Alternate power source ID when CV1=0. CV29 bit 2 must be set. |
| 20 | Extended Consist Address | O | 14-bit consist using CV19+CV20. Address = (CV20 * 100) + CV19[6:0]. Max 10239. |
| 25 | Speed Table / Mid-Range Cab Step | O | 2–127: preset speed table select. 128–154: 28-step position for CV6 mid-range. |
| 26 | Reserved | O | Reserved by NMRA. |
| 27 | Decoder Auto Stopping Config | O | See bit definitions below. |
| 30 | Error Information | O | 0=no error. |
| 96 | Function Assignment Method | O | Selects function mapping method. See table below. |
| 97–104 | Manufacturer Specific | — | Changed from "NMRA Reserved" to manufacturer-assignable. |
| 107–108 | Extended Manufacturer ID | cond M | When CV8=0xEE (238): 12-bit ID. CV108=8 LSBs, CV107[3:0]=4 MSBs. |
| 109–111 | Extended Manufacturer Version | O | Extension of CV7. |

### 3.2 CV 27 Bit Definitions (NEW — was listed as "Decoder lock config")

Renamed to **Decoder Automatic Stopping Configuration**:

| Bit | Description |
|-----|-------------|
| 0 | Auto Stop: asymmetrical DCC, more positive right rail |
| 1 | Auto Stop: asymmetrical DCC, more positive left rail |
| 2 | Auto Stop: Signal Controlled Influence cutout signal |
| 3 | Reserved |
| 4 | Auto Stop: opposite direction DC |
| 5 | Auto Stop: same direction DC |
| 6–7 | Reserved |

### 3.3 CV 96 Function Assignment Methods (NEW)

| Value | Method |
|-------|--------|
| 0 | Invalid (unimplemented CV may return 0) |
| 1 | CVs 33–46 per S-9.2.2 / RCN-225 |
| 2 | CVs 257–512 in bank CV31=0, CV32=40 per RCN-227 §2 |
| 3 | CVs 257–512 in bank CV31=0, CV32=41 per RCN-227 §3.1 |
| 4 | CVs 257–512 in bank CV31=0, CV32=42 per RCN-227 §3.2 |
| 5 | CVs 257–512 in bank CV31=0, CV32=43 per RCN-227 §3.3 |
| 6 | Manufacturer-specific |
| 7 | Reserved |

### 3.4 CV 28 (RailCom) — Expanded from 3 to 8 bits

**Multi-function decoders:**

| Bit | Old | New |
|-----|-----|-----|
| 0 | Channel 1 enable | Enable Ch1 Address Broadcast |
| 1 | Channel 2 enable | Enable Ch2 Data and Acknowledge |
| 2 | Unsolicited datagram expansion | Switch Off Ch1 Enable Automatically |
| 3 | — | Reserved |
| **4** | — | **Enable Programming Address 0003 (Long Address 3)** |
| 5 | — | Reserved |
| **6** | — | **Enable High-current RailCom** |
| **7** | — | **Enable Automatic Registration (RCN-218 / RailComPlus)** |

**Accessory decoders (CV 540):**

| Bit | Description |
|-----|-------------|
| 0 | Enable Unsolicited Decoder Initiated Transmission |
| 1 | Enable Ch2 data and acknowledge |
| 2–5 | Reserved |
| 6 | Enable high-current RailCom |
| 7 | Enable automatic registration |

### 3.5 CV 29 — Changed Bit Definitions

| Bit | Old Name | New Name / Change |
|-----|----------|-------------------|
| 1 | 28/128 speed step mode | **FL location** — 0: bit 4 in Speed/Dir controls FL; 1: bit 4 in Function Group One controls FL |
| 2 | Analog conversion enabled | **Power Source Conversion** — see CV 12 |
| **7** | (undefined) | **Control Type** — 0: Multi-function Decoder; 1: Accessory Decoder |

### 3.6 Changed CV Requirements

| CV | Old | New |
|----|-----|-----|
| 2 (Vstart) | Optional | **Mandatory** |
| 5 (Vhigh) | Optional | **Mandatory** |
| 6 (Vmid) | Optional | **Mandatory** |
| 17–18 (Extended Addr) | Optional | **Mandatory** |
| 19 (Consist Addr) | Optional | **Recommended** |

### 3.7 Changed CV Details

**CV 1**: Default value = 3. If CV1=0 or >127 and CV29 bit 5=0, DCC protocol is disabled.

**CV 8**: When value = 238 (0xEE), CVs 107–108 hold 12-bit extended manufacturer ID.

**CV 9**: Explicit formula: PWM period (us) = (131 + MANTISSA × 4) × 2^EXP
(MANTISSA = bits 0–4, EXP = bits 5–7).

**CV 10**: Valid range now **1–126** (was implied 0–128).

**CV 15–16 (Decoder Lock)**: CV16 values: 1=motor, 2=sound, 3+=other. CV15/CV16 writes
always allowed regardless of lock state. CV16=0 disables lock entirely.

**CV 19–20**: 14-bit consist address = (CV20 × 100) + CV19[6:0]. If >10239, not in consist.
CV19[6:0]=0 AND CV20=0 means not in consist.

**CV 33–46**: Now explicitly 14 outputs. CVs 33–37 → outputs 1–8, CVs 38–42 → outputs 4–11,
CVs 43–46 → outputs 7–14.

### 3.8 Indexed CV Pages (CV 31/32) — Detailed Allocation

| CV31 | CV32 | Content |
|------|------|---------|
| 0 | 0 | CVs 1–256 |
| 0 | 1 | CVs 257–512 |
| 0 | 2 | CVs 513–768 |
| 0 | 3 | CVs 769–1024 (SUSI modules from CV 897) |
| 0 | 40–43 | Function assignment per RCN-227 / S-9.2.1 |
| 0 | 254 | Decoder functionalities (read-only, bit=1 = supported) |
| 0 | 255 | RailCom page per RCN-217 |
| 1 | 0–1 | Loco Info RailComPlus |
| 2 | 0 | Data Space 0 (Capabilities) per RCN-218 |
| 2 | 1 | Data Space 1 (Data Space Info) |
| 2 | 2 | Data Space 2 (Short GUI) |
| 2 | 4–7 | Data Spaces 4–7 |
| 3–15 | * | Reserved |
| 16–255 | * | Manufacturer-specific |

### 3.9 Removed / Deprecated

- **CVs 880–895 (Dynamic CVs)**: Previously defined for Unsolicited Decoder Initiated
  Transmission (892=Decoder Load, 893=Flags, 894=Fuel/Coal, 895=Water). Now all
  **Reserved NMRA/RailCommunity**. Do not implement.
- **CVs 897 start** for SUSI is now explicitly 897 (was 896).

### 3.10 Accessory Decoder CVs — Expanded

Full table now defined with two address storage methods selected by CV29[541] bit 6:

- **Decoder Address** (bit 6=0): CV1 = 6-bit LSB (0–63), CV9 = 3-bit MSB (0–7, non-inverted)
- **Output Address** (bit 6=1): CV1 = 8-bit LSB, CV9 = 3-bit MSB. 11-bit range 0–2047.

New accessory CV 29 [541] bit definitions:

| Bit | Description |
|-----|-------------|
| 3 | Bi-Directional Communications enable |
| 5 | Decoder Type — 0=Basic, 1=Extended |
| 6 | Addressing Method — 0=Decoder Address, 1=Output Address |
| 7 | Control Type — 0=Multifunction, 1=Accessory |

New CV 33 for accessory decoders: output pair status (4 pairs, queryable via RailCom).

### 3.11 Power Source Conversion Codes (Appendix B — Updated)

| Bit | Old Name | New Name |
|-----|----------|----------|
| 0 | Analog Power Conversion | DC (Analog Mode Direct Current) |
| 1 | — | Radio Control |
| 2 | Zero 1 | DCC (digital operation) |
| 3 | TRIX | Selectrix |
| 4 | CTC-16/Railcommand | AC (Analog Mode Alternating Current) |
| 5 | FMZ (Fleischmann) | Motorola (digital operation) |
| **6** | — | **mfx (digital operation)** |
| **7** | — | **Reserved for Future Protocols** |

---

## 4. Bi-Directional Communication / RailCom (S-9.3.2)

Draft posted: 10-Apr-2026 (68 pages — massive expansion)

### 4.1 Timing — Precise Values Replace Approximations

Old reference used approximate values. Draft Table 1 (all measured from zero crossing of
last DCC message end bit):

| Parameter | Min | Max |
|-----------|-----|-----|
| Cut-out Start (T_CS) | 26 us | 32 us |
| Cut-out End (T_CE) | 454 us | 488 us |
| Start Channel 1 (T_TS1) | 80 us | — |
| End Channel 1 (T_TC1) | — | 177 us |
| Start Channel 2 (T_TS2) | 193 us | — |
| End Channel 2 (T_TC2) | — | 454 us |

Old "88 us" and "~1544 us" values do not match.

### 4.2 Physical Layer (NEW — not in existing reference)

| Parameter | Value |
|-----------|-------|
| Bit rate | 250 kbps (4 us/bit, ±2%) |
| Standard transmit current | 24–34 mA |
| High-current mode (CV28 bit 6) | 48–68 mA |
| One-bit detection threshold | Loop current <= 6 mA |
| Zero-bit detection threshold | Loop current >= 10 mA |
| Detector voltage drop | <= 200 mV at 34 mA |
| Rise/fall times | < 0.5 us or 12.5% of bit time |
| Channel 1 capacity | 16 bits (2 code words) |
| Channel 2 capacity | 48 bits (6 code words) |
| Polarity | Positive current into right rail (forward) |

### 4.3 Encoding Changes

- **NACK code word added**: `00111100` (was not in reference)
- **BUSY code word** (`11100001`): **No longer supported**
- Reserved code words `11000011` and `10000111` now formally "Reserved"

### 4.4 Datagram Format (NEW detail)

| Size | Format |
|------|--------|
| 12 bit | ID[3:0] + D[7:6] + D[5:0] |
| 18 bit | ID[3:0] + D[13:12] + D[11:6] + D[5:0] |
| 24 bit | ID[3:0] + D[19:18] + D[17:12] + D[11:6] + D[5:0] |
| 36 bit | ID[3:0] + D[31:30] + D[29:24] + D[23:18] + D[17:12] + D[11:6] + D[5:0] |

### 4.5 Message Types (NEW taxonomy)

| Type | Direction | Channel | Description |
|------|-----------|---------|-------------|
| 1 | Unsolicited | Ch1 | Address broadcast after every valid DCC mobile command. Mandatory. |
| 2 | Solicited | Ch2 | Addressed decoder responds. Determined by address partition. |
| 3 | Solicited | Ch2 | Substitute — decoder sends pending application update instead of ACK. |
| 4 | Solicited | Ch1 | SRQ from accessory decoders. Optional. No identifier. |

### 4.6 Channel Assignment by Address Partition (NEW)

| Address | Ch1 | Ch2 |
|---------|-----|-----|
| 0 (Broadcast) | None | Usually none |
| 1–127 (7-bit mobile) | All known decoders alternate ID1/ID2 | Addressed decoder |
| 128–191 (Accessory) | SRQ if needed | Addressed decoder |
| 192–231 (14-bit mobile) | All known decoders alternate ID1/ID2 | Addressed decoder |
| 253 (Any addressable) | None | Addressed decoder |
| 254 (Auto-discover) | Combined Ch1+Ch2, no ID | — |
| 255 (Idle) | None | None |

### 4.7 Mobile Decoder Datagram IDs — Expanded

**Channel 1:**

| ID | Application | Length |
|----|------------|--------|
| 1 | ADR: Address High | 12 bits |
| 2 | ADR: Address Low | 12 bits |
| 3 | Info1 (optional, sequenced with 1 and 2) | 12 bits |

**Channel 2:**

| ID | Application | Length | Notes |
|----|------------|--------|-------|
| 0 | POM | 12 bits | Mandatory |
| 1 | ADR: Address High (track search) | 12 bits | Mandatory |
| 2 | ADR: Address Low (track search) | 12 bits | Mandatory |
| 3 | EXT: Extended Command Response | 36 bits | Exact location |
| 4 | INFO: Information Transfer | 36 bits | Reserved |
| 7 | DYN: Dynamic Variable | 18 bits (×2) | Optional |
| 8–11 | XPOM (4 sequence IDs) | 36 bits each | Optional |
| 12 | CV automatic transfer | 36 bits | Optional |
| 13 | Logon + Block Transfer | Variable | S-9.2.1.1 |
| 14 | Time / ReadBackground | 12 bits / Variable | |
| 15 | Logon Enable | 48 bits combined | S-9.2.1.1 |

### 4.8 Accessory Decoder Datagram IDs (ENTIRELY NEW)

| ID | Application | Length | Notes |
|----|------------|--------|-------|
| 0 | POM | 12 bits | Mandatory |
| 3 | STAT4: Status 4 (all 4 output pairs) | 12 bits | |
| 4 | STAT1: Status 1 | 12 bits | Mandatory |
| 5 | Time | 12 bits | |
| 6 | Error | 12 bits | |
| 7 | DYN: Dynamic Variable | 18 bits (×2) | Mandatory |
| 8 | XPOM / STATUS 2 | 36 bits | |
| 9–11 | XPOM | 36 bits | |
| 12 | Test Feature Identifier | — | |
| 13 | Logon + Block Transfer | Variable | |
| 14 | ReadBackground | Variable | |
| 15 | Logon Enable | 48 bits combined | |

### 4.9 CV 28 — Full 8-bit Definition

**Mobile decoders:**

| Bit | Description | Req |
|-----|-------------|-----|
| 0 | Enable Ch1 Address Broadcast | M |
| 1 | Enable Ch2 Datagrams and ACK code words | M |
| 2 | Switch Off Ch1 Address Broadcast dynamically | M |
| 3 | Transmit ID3 in Ch1 (unsolicited) | |
| 4 | Enable Programming Address 0003 | |
| 5 | Reserved | |
| 6 | Enable High-current (48–68 mA) | |
| 7 | Enable Automatic Registration | |

**Accessory decoders:**

| Bit | Description | Req |
|-----|-------------|-----|
| 0 | Enable Ch1 Address Broadcast | |
| 1 | Enable Ch2 Datagrams and ACK code words | M |
| 6 | Enable High-current | |
| 7 | Enable Automatic Registration | |

### 4.10 Page 255 Indexed CVs — Mandatory Decoder Identity Block (NEW)

CV31=0, CV32=255:

| Offset | CV | Description | Req |
|--------|-----|-------------|-----|
| 0–1 | 257–258 | Manufacturer ID (LE, matches CVs 107–108) | M |
| 4–7 | 261–264 | Product ID (LE) | |
| 8–11 | 265–268 | MUN — Manufacturer Unique Number / Serial (LE) | |
| 12–15 | 269–272 | Production date (seconds since 1.1.2000, LE, unsigned) | |
| 16–63 | 273–320 | Manufacturer-specific (R/W) | |
| 64–127 | 321–384 | 64 dynamic variables (R/W) | |
| 128–130 | 385–387 | Manufacturer-specific read-only | M |
| 132–143 | 389–400 | Container 1–12 emptying rates (R/W) | |
| 144 | 401 | Write all 12 container levels (W only) | |
| 145 | 402 | Speedometer scaling (value × 2 = max km/h) | |

### 4.11 Dynamic Variables — 64 Defined (NEW)

**Mobile decoders (selection):**

| DV | CV | Description |
|----|-----|-------------|
| 0 | 321 | Uncoded scale speed (0–255 km/h) |
| 1 | 322 | Scale speed overage (if >255 km/h) |
| 3 | 324 | RailCom version (2×4-bit "main.minor", 0x15 = v1.5) |
| 4 | 325 | Change Flags (S-9.2.1.1 / RCN-218) |
| 5 | 326 | Flag Registers |
| 6 | 327 | Input Registers |
| 7 | 328 | Reception statistics (faulty/total %) |
| 8–19 | 329–340 | Container 1–12 contents (0–100%) |
| 20 | 341 | ADR datagram 1+2 contents (Basic Location Service) |
| 21 | 342 | Warning/alarm messages |
| 26 | 347 | Temperature (-50°C to 205°C, linear 0–255) |
| 27 | 348 | Direction status byte (east/west) |

Alarm values (DV 21): 128=Motor short, 129=Function output short, 130=Over temperature.

Direction status (DV 27) bits: 0=VR direction, 1=OW system direction, 2=directional control,
3=change of direction (braking), 4=OW hide, 5=OW inverse.

**Accessory decoders** have a similar but different DV layout. Alarm values include per-output
short circuit reporting (128–135=outputs 1–8, 136=over temperature).

### 4.12 Address (ADR) Location Service

**ADR1 (ID1) + ADR2 (ID2) contents:**

| ADR1 | ADR2 | Address Type |
|------|------|-------------|
| `00000000` | `0 A6-A0` | Base Address (CV1) |
| `01100000` | `R A6-A0` | Consist Address (CV19), R=direction |
| `1 0 A13-A8` | `A7-A0` | Extended Address (CV17,CV18) |

**Info1 (ID3) bits:**
- Bit 4: request to be addressed
- Bit 3: part of consist
- Bit 2: driving condition (1=moving)
- Bit 1: direction of travel
- Bit 0: rerailing direction

**Dynamic Ch1 (CV28 bit 2)**: auto-cessation after 8 commands. One response only on:
restart, address change, or 5+ seconds without addressed commands.

### 4.13 Track Search Location Service (NEW)

Uses binary state XF2 (broadcast): `11011101 00000010`
- Decoder responds for 30 seconds after power-up
- Response: ADR1 + ADR2 + Time datagram (ID14, seconds since power-up)

### 4.14 Exact (EXT) Location Service (NEW)

Uses binary state XF1 (addressed): `11011101 00000001`
- Response ID3: TTTT (location type 0–7) + 11-bit position ID
- If decoder doesn't know location, local detector completes the response

### 4.15 Reserved Binary States XF1–XF15 (NEW)

| XF | Purpose |
|----|---------|
| 1 | Request Location Information (EXT) |
| 2 | Track Search |
| 3 | CV automatic transfer enable/disable |
| 4–15 | Reserved |

### 4.16 Multi-CV Access Commands (NEW)

Short-form paired CV writes with dual datagram responses:

- GGGG=0100: CVs 17+18 (long address) — `11110100 0 D(CV17) 0 D(CV18)`
- GGGG=0101: CVs 31+32 (page index) — `11110101 0 D(CV31) 0 D(CV32)`

Both require two identical packets. Each returns two 12-bit datagrams (ID0) in Ch2.

### 4.17 XPOM via RailCom (NEW detail)

- 4 sequence IDs (8–11) allow pipelining up to 16 CVs
- SS encoding: 00=ID8, 01=ID9, 10=ID10, 11=ID11
- Response: 36-bit datagram with 4 contiguous CV values
- Timeout: not less than 2 seconds
- Unsupported CVs return zero

### 4.18 CV Automatic Transfer (NEW)

Controlled by binary state XF3:
- Enable: `11011101 10000011`
- Disable: `11011101 00000011`
- Uses ID12, 36-bit datagram: 24-bit indexed CV address + 8-bit data
- Triggered by any addressed operational command

### 4.19 NOP Command for Accessory Decoders — Full Spec

- Required within 5 seconds of energizing the track
- Recommended interval: 0.5 seconds
- Includes threshold address for successive approximation search
- Collision resolution via binary search

### 4.20 Accessory Applications (ALL NEW)

| Application | ID | Description |
|------------|-----|-------------|
| SRQ | — | 12-bit in Ch1, no identifier. MSB=0: basic, MSB=1: extended format |
| POM | 0 | Read/write/bit with NACK support. 0.5s timeout |
| Status 1 | 4 | Mandatory. Basic: 5-bit aspect. Extended: 8-bit aspect. Includes command-match bit |
| Status 4 | 3 | All 4 output pairs in 8-bit datagram |
| Time | 5 | Bit 7=resolution (0=0.1s, 1=1s). Range 0–12.7s or 0–127s |
| Error | 6 | See error codes below |
| Status 2 | 8 | Legacy. 4-bit config type + 3-bit status |

### 4.21 Accessory Error Codes (NEW)

| Code | Description |
|------|-------------|
| 0x00 | No Error |
| 0x01 | Command not executed / unsupported aspect |
| 0x02 | Current too high |
| 0x03 | Supply voltage too low |
| 0x04 | Fuse failure |
| 0x05 | Temperature too high |
| 0x06 | Feedback error |
| 0x07 | Manual adjustment |
| 0x10 | Turnout/signal lantern failure |
| 0x20 | Servo failure |
| 0x3F | Internal decoder error |

### 4.22 Status 2 Configuration Types (NEW)

| Config | Description |
|--------|-------------|
| 0x0 | Uncoupler |
| 0x1 | Turnout switch |
| 0x2 | Three-way switch |
| 0x3 | Double-crossing switch |
| 0x4 | Turntable |
| 0x8 | Track lock signal |
| 0x9 | Shape signal Hp0/Hp1 |
| 0xA | Shape signal Hp0/Hp1/Hp2 |
| 0xB | Distant signal Vr0/Vr1 |
| 0xC | Distant signal Vr0/Vr1/Vr2 |
| 0xD | Railway barrier |

---

## 5. E24 Decoder Interface (S-9.1.1.6) — NEW

Draft posted: 15-Feb-2026 (standard + technical note TN-9.1.1.6)

### 5.1 Overview

28-pin interface for DCC decoders in vehicles with limited space (N, TT, smaller HO).
Can also be used for function-only decoders or SUSI modules. Harmonized with RCD-124.

### 5.2 Connectors

- Decoder socket: Molex 505270-2412 (6.65 × 3.85 mm)
- Vehicle plug: Molex 505070-2422 (5.51 × 3.85 mm)
- Pitch: 0.35 mm
- NOT keyed — vehicle board must include mechanical blocking for correct insertion
- Max 30 mating cycles
- Mating force: max 43.2 N

### 5.3 Decoder Dimensions

| Parameter | Max |
|-----------|-----|
| Length | 19.5 mm |
| Width | 8.4 mm |
| Height (no socket, no bottom parts) | 2.6 mm |
| Bottom component height | 0.7 mm |

### 5.4 Electrical Limits

- Max voltage: 50V AC(rms) / DC
- Contact load: 0.3A (pins 1–24), 3.0A (guide pins X1A/B, X2A/B)
- Contact resistance: 80 mΩ (pins), 30 mΩ (guide pins)

### 5.5 Pin Assignments

| Pin | Name | Description | Group |
|-----|------|-------------|-------|
| X1A, X1B | Track Left | Left rail power (3.0A guide pins) | 1 |
| X2A, X2B | Track Right | Right rail power (3.0A guide pins) | 1 |
| 1 | LS_B | Speaker B | 5 |
| 2 | LS_A | Speaker A | 5 |
| 3 | GND | Decoder ground | — |
| 4, 5 | Motor (−) | Motor negative (2 pins, 0.6A total) | 2 |
| 6, 7 | Motor (+) | Motor positive (2 pins, 0.6A total) | 2 |
| 8 | Cap. (+) | Storage capacitor (max 15V charge, switchable) | 6 |
| 9 | F0_f | Headlight Forward | 3 |
| 10 | F0_r | Headlight Reverse | 3 |
| 11 | AUX_1 | Output 1 | 3 |
| 12 | AUX_2 | Output 2 | 3 |
| 13 | AUX_8 | Output 8 | 3 |
| 14 | AUX_7 | Output 7 | 3 |
| 15 | AUX_6 | Output 6 | 3 |
| 16 | AUX_5 | Output 5 | 3 |
| 17 | V+ | Decoder positive (after rectifier) | — |
| 18 | Vcc | Internal decoder voltage (1.8–5.7V) | — |
| 19 | AUX_10 / GPIO_C | Output 10 or GPIO | 4 |
| 20 | AUX_4 | Output 4 | 4 |
| 21 | AUX_3 | Output 3 | 4 |
| 22 | GND | Decoder ground | — |
| 23 | TBCLK / AUX_12 / GPIO_A | Train Bus Clock or Output 12 or GPIO | 4 |
| 24 | TBDATA / AUX_11 / GPIO_B | Train Bus Data or Output 11 or GPIO | 4 |

### 5.6 Signal Groups

- **Group 3 (Function Outputs)**: Open collector/drain, switched against GND. Max 100 mA/output.
- **Group 4 (Logic Outputs)**: TTL/LVTTL. Max 0.5 mA. GPIO_A/B preferred for SUSI Train Bus.
  GPIO_C may be analog input (>100k Ω static, max 3.3V).
- **Group 5 (Speaker)**: 4–8 Ω impedance.
- **Group 6 (Cap)**: Must NOT be supplied from vehicle side. Electrolytic: min 16V. Tantalum: min 25V.

### 5.7 Train Bus (SUSI) on E24

- 470 Ω series resistor on TBCLK (pin 23) and TBDATA (pin 24)
- ≥15k Ω pull-up on TBDATA before the 470 Ω resistor

### 5.8 Minimum Requirements

F0_f, F0_r, AUX_1, AUX_2 must be supported.

### 5.9 SUSI-Only Mode

Only 4 signals used: GND (3, 22), V+ (17), TBCLK (23), TBDATA (24). Track connections unused.

### 5.10 Without Decoder

Jumper plug required: X2A/X2B → Motor+ (6, 7), X1A/X1B → Motor− (4, 5).

---

## 6. SUSI Bus (S-9.4.x) — NEW

Four new standards, all posted 29-Jan-2026.

### 6.1 Communication Interface (S-9.4.1)

#### Connector Variants

| Variant | Connector | Max Current | Pins |
|---------|-----------|-------------|------|
| classicSUSI | JST SM04B-SRSS-TB | 1000 mA | 4 |
| microSUSI | JST 04XSR-36S | 200 mA | 4 |
| powerSUSI | Wurth WR-WTB 2.0mm | 2000 mA | 5 |

4-pin assignment: 1=GND (black), 2=Data (gray), 3=Clock (blue), 4=V+ (red).
powerSUSI adds pin 1=Trigger (open collector sync).

#### Electrical

- Up to 3 SUSI-Modules per Host
- 470 Ω series resistors on Data AND Clock at BOTH Host and Module (4 resistors total)
- ≥15k Ω pull-up on Data at Host
- Vcc: 5V or 3.3V. All modules must tolerate 5V. "SUSI3" label = accepts 3.3V.
- Max cable length: 20 cm recommended

#### Protocol

- Synchronous, unidirectional (or half-duplex with BiDi extension)
- Clock idle: LOW. Data on rising edge, read on falling edge. LSB first.
- Bit timing: high/low min 10 us, total max 500 us (100 us for BiDi CV bank reads)
- Packets: 2 bytes normally, 3 bytes for CV manipulation (starting `0111xxxx`)

#### Synchronization

- 8 ms ±1 ms gap after complete byte → reset (bits discarded)
- Bytes within a command must follow within 7 ms
- Host must insert ≥9 ms gap after every 20 commands (50 for BiDi bank reads)

#### ACK

- Module pulls data LOW through its 470 Ω resistor
- Duration: 1–2 ms, must complete within 20 ms
- Host must accept 0.5–7 ms as valid

#### Commands (2-byte unless noted)

| Cmd | Hex | Description |
|-----|-----|-------------|
| NOP | 0x00 | Blank / no operation |
| Trigger | 0x21 | Trigger pulse for powerSUSI |
| Current | 0x23 | Signed 2's complement motor current (−128 to 127) |
| Analog Fn | 0x28–0x2F | 8 analog function groups |
| Direct Cmd | 0x40–0x43 | Outputs 1–32 (bit=1=on) |
| Actual Speed | 0x50 | `RGGGGGGG` (R=dir, G=0 stop, 1–127=speed) |
| Target Speed | 0x51 | Same format as 0x50 |
| DCC Speed Step | 0x52 | Normalized from 14/28 to 127 steps |
| Module Addr Lo | 0x5E | Address bits 7–0 |
| Module Addr Hi | 0x5F | Address bits 15–8 |
| Function Grp 1–9 | 0x60–0x68 | F0–F68 in groups of 8 |
| Module Control | 0x6C | Bit 0=buffer on/off, Bit 1=reset |
| Binary Short | 0x6D | `D L6-L0` (functions 1–127) |
| Binary Long Lo | 0x6E | `D L6-L0` (sent before 0x6F) |
| Binary Long Hi | 0x6F | `H7-H0` (execution on this cmd) |

**3-byte CV commands** (CV range 897–1024):

| Cmd | Hex | Description |
|-----|-----|-------------|
| CV Check | 0x77 | `0111-0111 1 V6-V0 D7-D0` |
| CV Bit Manip | 0x7B | `0111-1011 1 V6-V0 111 K D B2-B0` |
| CV Write | 0x7F | `0111-1111 1 V6-V0 D7-D0` |

Auto-start: if no commands within 100 ms of reset, modules may start in standalone mode.
Repeat rate: active commands should repeat at least every 200 ms.

### 6.2 Configuration Variables (S-9.4.2)

CV range: 897–1024.

| Range | Assignment |
|-------|-----------|
| 897 | Module Number (bits 0–1: 01/10/11 = modules 1/2/3) |
| 898 | CV Banking — volatile (OBSOLETE, use CV 1021) |
| 899 | Manufacturer-specific (OBSOLETE) |
| 900–939 | Module 1 CVs |
| 940–979 | Module 2 CVs |
| 980–1019 | Module 3 CVs |
| 1020 | Status Byte (shared) |
| 1021 | CV Banking — non-volatile (RECOMMENDED) |
| 1022–1024 | Reserved |

#### CV 900/940/980 (per module, banked)

| Bank | Content |
|------|---------|
| 0 | Manufacturer ID (NMRA assigned, write = reset). Read-only. |
| 1 | Hardware identifier. Read-only. |
| 254 | Extended manufacturer ID (0=use bank 0, 1–15=upper 4 bits of 12-bit ID, ≥16=alternative) |

#### CV 901/941/981 (per module, banked)

| Bank | Content |
|------|---------|
| 0 | Version number. Read-only. |
| 1 | Subversion. Read-only. |
| 254 | Supported SUSI version (decimal × 10, e.g., 10 = v1.0) |

#### CV 1020 — Status Byte

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | WAIT | Sound module requests motor start delay |
| 1 | SLOW | Module requests crawl speed only |
| 2 | HOLD | Speed must be maintained (e.g., gearshift) |
| 3 | STOP | Stop motor(s). Priority over bits 0–2. |
| 4–7 | Reserved | |

#### CV 1021 Banking

- Banks 0–1: fixed meanings (manufacturer ID, hardware, version)
- Banks 2–247: manufacturer-dependent
- Banks 248–253: RCN-218 config data (function info, icon mappings)
- Bank 254: standardized read-only (SUSI version, alt manufacturer ID)
- Bank 255: freely usable (legacy compatibility)

### 6.3 Bidirectional Extension (S-9.4.3)

Adds half-duplex communication: SUSI-Module → Host. Up to 3 BiDi modules.

#### Protocol Flow

1. Host sends BiDi Call (2 bytes, 16 clocks, <2 ms)
2. Module ACKs (1–2 ms pulse within 20 ms)
3. Host sends 32 clocks; module places data on rising edge, host reads on falling edge
4. 1–1.5 ms pause, then normal SUSI commands resume
5. Host must call each module at least every 100 ms

#### BiDi Host Commands (0x0# range)

| Cmd | Hex | Description |
|-----|-----|-------------|
| Module Call | 0x01 | Bits 0–1=module#, Bit 2=forced answer, Bits 3–4=status address |
| CV Bank Read Mod 1 | 0x0C | Bank number 0–255 |
| CV Bank Read Mod 2 | 0x0D | Bank number 0–255 |
| CV Bank Read Mod 3 | 0x0E | Bank number 0–255 |
| Read CV | 0x0F | CV-Nr − 769 |

#### BiDi Module Responses (0x8# range, sent in pairs)

| Resp | Hex | Description |
|------|-----|-------------|
| Signal state | 0x80 | 0=HP0, 4=HP2, 15=HP1 |
| Function directly | 0x81 | Fn# + mode (0=impulse, +32=on, +64=off) |
| Function value DCC | 0x82 | DCC fn 0–28 + mode |
| Short binary states | 0x83 | `DLLLLLLL` |
| Automatic speed | 0x84 | `RGGGGGGG` (128-step) |
| Automatic operation | 0x85 | 0=stop, 2=same dir, 4=fwd shuttle, 6=bwd shuttle, +1=slow |
| Position addr high | 0x88 | A10–A8 + 0=standard/+8=extended |
| Position addr low | 0x89 | A7–A0 |
| Status byte | 0x8A | 4 status variants (selected by call bits 3–4) |
| Analog values A | 0x8C | Channels 1–2 (8-bit each) |
| Analog values B | 0x8D | Channels 3–4 (8-bit each) |
| CV response invalid | 0x8E | 0=unknown, 1=not supported, 2=invalid for module |
| CV response valid | 0x8F | CV value (0–255) |

#### CV Bank Reading (Bulk)

- Reads entire 40-CV bank at once
- Module sends pairs: `0x8F + value` (×20 for 40 bytes)
- CRC-8 checksum: polynomial x^8+x^5+x^4+1, init=0
- Max 50 commands without pause in bank read mode

### 6.4 Train Bus Shift Registers / SIO (S-9.4.4)

Alternative to SUSI on Train Bus pins — simple shift register I/O for additional
function outputs. **Mutually exclusive with SUSI.**

#### Protocol

- Data transferred on rising edge of Clock
- Clock: 400 kHz – 4 MHz
- ≥30 us pause between transfers (latches data)
- Max shift register length: 16 bits
- MSB (highest function) transmitted first
- Unidirectional (Host → shift register only)
- No error detection

#### Logic Levels

| Level | Decoder Output | SR Input |
|-------|---------------|----------|
| Low | ≤ 0.4V | ≤ 0.8V |
| High | ≥ 2.4V | ≥ 2.0V |

#### Reference Circuit

SN74AHCT123APWR (monostable for latch pulse from ≥30 us pause) +
SN74HCT594PW (8-bit shift register). 470 Ω series on Clock and Data.
L7805 for 5V supply.

---

## Implementation Impact Summary

### Must Fix in Current Reference
1. **Analog Function instruction byte**: reference says `11111101`, correct is `00111101`
2. **Section 4 attribution**: all content belongs in S-9.2.1, not S-9.2.1.1

### High Priority — New Features
1. XPOM (24-bit CV addressing, multi-byte R/W, exceeds 6-byte packet limit)
2. Logon protocol (address partitions 253/254, CRC-8+XOR, auto-registration)
3. CV 28 bits 4/6/7 (programming address, high-current RailCom, auto-registration)
4. CV 29 bit 7 (Control Type)
5. 14-bit consist address (CV 19+20)
6. Precise RailCom timing (Table 1 values)
7. NACK control word
8. Short Form CV access new GGGG values (0100, 0101, 0110)

### Medium Priority — New Features
1. Target Speed instruction (`00111011`)
2. Time/Date/System Time full packet formats
3. NOP command for accessory decoders
4. Accessory XPOM
5. Dynamic Variables (64 defined)
6. Accessory bi-directional support (SRQ, Status 1/2/4, Errors, Time)
7. Data Spaces and bulk transfer
8. CV 27 auto-stopping configuration
9. CV 96 function assignment method

### New Standards to Consider
1. E24 decoder interface (S-9.1.1.6) — relevant if targeting small-scale decoders
2. SUSI bus (S-9.4.x) — relevant if implementing decoder-to-peripheral communication

### Deprecated / Removed
1. Dynamic CVs 880–895 — do not implement
2. BUSY RailCom code word (`11100001`) — no longer supported

---

*Generated from NMRA draft specifications downloaded 13-Apr-2026.*
*For released (non-draft) spec notes, see [DCC_Spec_Reference.md](DCC_Spec_Reference.md).*
*For draft file index, see [drafts/DCC_Draft_Spec_Reference.md](drafts/DCC_Draft_Spec_Reference.md).*
