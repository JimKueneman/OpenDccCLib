"""
dcc_encode.py — independent, spec-derived NMRA DCC instruction encoder (S-9.2 / S-9.2.1).

For HIL decoder testing. Each function composes the packet PAYLOAD bytes (address +
instruction), WITHOUT the XOR error byte — wfplayer.packet_segs() appends that. This is
deliberately derived from the NMRA spec, NOT from the DCC library, so it is an independent
oracle: when this encoder and the decoder-under-test agree, both are corroborated.

    import dcc_encode as enc, wfplayer as wf
    pkt = enc.speed_128(enc.short_addr(3), 64, forward=True)   # [0x03, 0x3F, 0xC0]
    wf.compose([pkt])                                          # -> segments to play

Every instruction function takes the address byte list first (use short_addr / long_addr)
and returns address + instruction bytes.
"""


# ---------------------------------------------------------------------------
# Address (S-9.2)
# ---------------------------------------------------------------------------
def short_addr(n):
    """Short (1-byte) multifunction address, 1..127 -> 0AAAAAAA."""
    if not 1 <= n <= 127:
        raise ValueError("short address must be 1..127")
    return [n]


def long_addr(n):
    """Long (14-bit) multifunction address, 1..10239 -> 11AAAAAA AAAAAAAA."""
    if not 1 <= n <= 10239:
        raise ValueError("long address must be 1..10239")
    return [0xC0 | ((n >> 8) & 0x3F), n & 0xFF]


# ---------------------------------------------------------------------------
# Speed & direction
# ---------------------------------------------------------------------------
def speed_128(addr, speed, forward=True):
    """128-step Advanced Operations speed: 0011 1111 (0x3F) + D SSSSSSS.
    speed field: 0 = stop, 1 = emergency stop, 2..127 = speed steps."""
    if not 0 <= speed <= 127:
        raise ValueError("speed must be 0..127")
    d = 0x80 if forward else 0x00
    return list(addr) + [0x3F, d | (speed & 0x7F)]


def estop_128(addr, forward=True):
    """Emergency stop via the 128-step instruction (speed field = 1)."""
    return speed_128(addr, 1, forward)


# ---------------------------------------------------------------------------
# Function groups (S-9.2)
# ---------------------------------------------------------------------------
def function_group_1(addr, f0=0, f1=0, f2=0, f3=0, f4=0):
    """F0-F4: 100DDDDD, with F0(FL)=bit4 and F4..F1 in bits 3..0."""
    b = (0x80 | (bool(f0) << 4) | (bool(f4) << 3) | (bool(f3) << 2)
              | (bool(f2) << 1) | bool(f1))
    return list(addr) + [b]


def function_f5_f8(addr, f5=0, f6=0, f7=0, f8=0):
    """F5-F8 (Function Group Two, S=1): 1011DDDD, F8..F5 in bits 3..0."""
    b = 0xB0 | (bool(f8) << 3) | (bool(f7) << 2) | (bool(f6) << 1) | bool(f5)
    return list(addr) + [b]


def function_f9_f12(addr, f9=0, f10=0, f11=0, f12=0):
    """F9-F12 (Function Group Two, S=0): 1010DDDD, F12..F9 in bits 3..0."""
    b = 0xA0 | (bool(f12) << 3) | (bool(f11) << 2) | (bool(f10) << 1) | bool(f9)
    return list(addr) + [b]


# ---------------------------------------------------------------------------
# Configuration Variable access — Long Form, operations mode / POM (S-9.2.1)
#   1110 CCVV  VVVVVVVV  DDDDDDDD      (VV..V = CV number minus 1, 10-bit)
#   CC = 01 verify byte, 11 write byte, 10 bit manipulation
# ---------------------------------------------------------------------------
def _cv_hi_lo(cv):
    if not 1 <= cv <= 1024:
        raise ValueError("CV number must be 1..1024")
    a = cv - 1
    return (a >> 8) & 0x03, a & 0xFF


def cv_write_pom(addr, cv, value):
    """POM write byte: CC=11 -> 0xEC|VV, CV-1 low, value."""
    hi, lo = _cv_hi_lo(cv)
    return list(addr) + [0xEC | hi, lo, value & 0xFF]


def cv_verify_pom(addr, cv, value):
    """POM verify byte: CC=01 -> 0xE4|VV, CV-1 low, value."""
    hi, lo = _cv_hi_lo(cv)
    return list(addr) + [0xE4 | hi, lo, value & 0xFF]


def cv_bit_pom(addr, cv, bit, value, write=True):
    """POM bit manipulation: CC=10 -> 0xE8|VV, CV-1 low, data 111K DBBB
    (K=1 write / 0 verify, D=bit value, BBB=bit position 0..7)."""
    if not 0 <= bit <= 7:
        raise ValueError("bit must be 0..7")
    hi, lo = _cv_hi_lo(cv)
    data = 0xE0 | ((1 if write else 0) << 4) | ((value & 1) << 3) | (bit & 0x07)
    return list(addr) + [0xE8 | hi, lo, data]


# ---------------------------------------------------------------------------
# Phase 2 -- additional instruction types (S-9.2 / S-9.2.1)
# ---------------------------------------------------------------------------
def speed_28(addr, step, forward=True):
    """28-step speed: 01DC SSSS.  C (bit4) is the speed LSB.
    5-bit value: 0=stop, 1=estop, 4..31 = steps 1..28 (so step n -> n+3)."""
    if not 0 <= step <= 28:
        raise ValueError("28-step: step 0..28")
    val = 0 if step == 0 else step + 3
    d = 0x20 if forward else 0x00
    return list(addr) + [0x40 | d | ((val & 0x01) << 4) | ((val >> 1) & 0x0F)]


# Note: there is no separate 14-step encoder -- 14- and 28-step share the wire
# format (01DxSSSS); the decoder selects 14 vs 28 from CV29 bit 1. To exercise
# 14-step, write CV29 bit 1 = 0 (cv_bit_pom) then send a speed_28-format packet.


def function_expansion(addr, func_num, state=True):
    """Function expansion F13-F28: F13-20 via 1101 1110 (0xDE), F21-28 via 1101 1111 (0xDF)."""
    if 13 <= func_num <= 20:
        inst, bit = 0xDE, func_num - 13
    elif 21 <= func_num <= 28:
        inst, bit = 0xDF, func_num - 21
    else:
        raise ValueError("function_expansion supports F13..F28")
    return list(addr) + [inst, ((1 if state else 0) << bit) & 0xFF]


def consist(addr, consist_addr, direction_normal=True):
    """Consist control: 0001 001D + consist address (7-bit). D=0 normal, 1 reverse."""
    inst = 0x12 if direction_normal else 0x13
    return list(addr) + [inst, consist_addr & 0x7F]


def binary_state_short(addr, state_num, on=True):
    """Binary State Short: 1101 1101 (0xDD) + D LLLLLLL (state 1..127)."""
    return list(addr) + [0xDD, ((1 if on else 0) << 7) | (state_num & 0x7F)]


def binary_state_long(addr, state_num, on=True):
    """Binary State Long: 1100 0000 (0xC0) + D LLLLLLL  LHHHHHHH (15-bit state)."""
    lo = ((1 if on else 0) << 7) | (state_num & 0x7F)
    hi = (state_num >> 7) & 0xFF
    return list(addr) + [0xC0, lo, hi]


def analog_function(addr, output, value):
    """Analog Function Group: 0011 1101 (0x3D) + output number + value."""
    return list(addr) + [0x3D, output & 0xFF, value & 0xFF]
