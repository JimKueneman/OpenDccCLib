"""
dcc_encode.py -- independent, spec-derived NMRA DCC instruction encoder
                 (S-9.2 / S-9.2.1 / S-9.2.3).

For HIL decoder testing. Each function composes the packet PAYLOAD bytes (address +
instruction), WITHOUT the XOR error byte -- wfplayer.packet_segs() appends that. This is
deliberately derived from the NMRA spec TEXT, not from the DCC library (this module
imports nothing from it), so it is an independent oracle: when this encoder and the
decoder-under-test agree, both are corroborated.

    import dcc_encode as enc, wfplayer as wf
    pkt = enc.speed_128(enc.short_addr(3), 64, forward=True)   # [0x03, 0x3F, 0xC0]
    wf.compose([pkt])                                          # -> segments to play

Multifunction builders take the address byte list first (short_addr / long_addr /
broadcast_addr) and return address + instruction bytes. Accessory builders take the
11-bit accessory address A10..A0 directly (S-9.2.1 2.4). Service-mode builders
(S-9.2.3) have no address byte; play them with wfplayer.PREAMBLE_SERVICE.

Spec text used for every section reference below:
    pdftotext -layout documentation/specs/s-9.2.1_dcc_extended_packet_formats.pdf -
        (S-9.2.1, Jan 24 2025 edition -- section numbers are that edition's)
    pdftotext -layout documentation/specs/s-92-2004-07.pdf -           (S-9.2 Figure 2)
    pdftotext -layout documentation/specs/S-9.2.3_2012_07.pdf -        (service mode)
"""


# ---------------------------------------------------------------------------
# Address (S-9.2 / S-9.2.1 2.1, 2.2)
# ---------------------------------------------------------------------------
def short_addr(n):
    """Short (1-byte) multifunction address 0..127 -> 0AAAAAAA.
    0 is the multifunction BROADCAST address (S-9.2.1 2.2): every multifunction
    decoder executes the instruction. Use broadcast_addr() when you mean that."""
    if not 0 <= n <= 127:
        raise ValueError("short address must be 0..127 (0 = broadcast)")
    return [n]


def broadcast_addr():
    """Multifunction broadcast address 00000000 (S-9.2.1 2.2): the packet
    {preamble} 0 00000000 0 {instruction} 0 EEEEEEEE 1 is executed by all
    multifunction decoders. (Address 0 with instruction byte 00000000 is the
    reset packet -- see reset_packet().)"""
    return [0x00]


def long_addr(n):
    """Long (14-bit) multifunction address, 1..10239 -> 11AAAAAA AAAAAAAA."""
    if not 1 <= n <= 10239:
        raise ValueError("long address must be 1..10239")
    return [0xC0 | ((n >> 8) & 0x3F), n & 0xFF]


# ---------------------------------------------------------------------------
# Speed & direction
# ---------------------------------------------------------------------------
def speed_128(addr, speed, forward=True):
    """128-step Advanced Operations speed (S-9.2.1 2.3.2.1): 0011 1111 (0x3F) + D SSSSSSS,
    D=1 forward. Data byte U0000000 = stop, U0000001 = emergency stop (spec text:
    "a data-byte value of U0000001 is used for emergency stop"), 2..127 = 126 steps."""
    if not 0 <= speed <= 127:
        raise ValueError("speed must be 0..127")
    d = 0x80 if forward else 0x00
    return list(addr) + [0x3F, d | (speed & 0x7F)]


def estop_128(addr, forward=True):
    """Emergency stop via the 128-step instruction (S-9.2.1 2.3.2.1: speed field = 1)."""
    return speed_128(addr, 1, forward)


def speed_28(addr, step, forward=True):
    """28-step speed (S-9.2.1 2.3.3 / S-9.2 Figure 2): 01DC SSSS, D=1 forward, C = the
    speed LSB when CV29 bit 1 = 1. Speed value (C S3 S2 S1 S0 in the S-9.2 table;
    here C is the LSB of `val`): 0 = Stop, 1 = Stop (I), 2 = E-Stop, 3 = E-Stop (I),
    4..31 = steps 1..28 (so step n -> n+3). "(I)" = direction bit may be ignored.
    This builder takes a step 0..28 (0 = Stop); use estop_28 for E-Stop."""
    if not 0 <= step <= 28:
        raise ValueError("28-step: step 0..28")
    val = 0 if step == 0 else step + 3
    d = 0x20 if forward else 0x00
    return list(addr) + [0x40 | d | ((val & 0x01) << 4) | ((val >> 1) & 0x0F)]


def estop_28(addr, forward=True, ignore_direction=False):
    """28-step emergency stop (S-9.2 Figure 2): SSSS = 0001 with C = 0 -> E-Stop
    (01D0 0001), or C = 1 -> E-Stop (I) (01D1 0001), direction-bit-ignorable."""
    d = 0x20 if forward else 0x00
    c = 0x10 if ignore_direction else 0x00
    return list(addr) + [0x40 | d | c | 0x01]


# Note: there is no separate 14-step encoder -- 14- and 28-step share the wire
# format (01DxSSSS); the decoder selects 14 vs 28 from CV29 bit 1. To exercise
# 14-step, write CV29 bit 1 = 0 (cv_bit_pom) then send a speed_28-format packet.


# ---------------------------------------------------------------------------
# Function groups (S-9.2.1 2.3.4, 2.3.5)
# ---------------------------------------------------------------------------
def function_group_1(addr, f0=0, f1=0, f2=0, f3=0, f4=0):
    """Function Group One (S-9.2.1 2.3.4): 100DDDDD, FL(F0) = bit 4, F1..F4 = bits 0..3."""
    b = (0x80 | (bool(f0) << 4) | (bool(f4) << 3) | (bool(f3) << 2)
              | (bool(f2) << 1) | bool(f1))
    return list(addr) + [b]


def function_f5_f8(addr, f5=0, f6=0, f7=0, f8=0):
    """Function Group Two, S=1 (S-9.2.1 2.3.5): 1011DDDD, F5..F8 = bits 0..3."""
    b = 0xB0 | (bool(f8) << 3) | (bool(f7) << 2) | (bool(f6) << 1) | bool(f5)
    return list(addr) + [b]


def function_f9_f12(addr, f9=0, f10=0, f11=0, f12=0):
    """Function Group Two, S=0 (S-9.2.1 2.3.5): 1010DDDD, F9..F12 = bits 0..3."""
    b = 0xA0 | (bool(f12) << 3) | (bool(f11) << 2) | (bool(f10) << 1) | bool(f9)
    return list(addr) + [b]


# ---------------------------------------------------------------------------
# Feature expansion -- function control F13-F68 (S-9.2.1 2.3.6.5 .. 2.3.6.11)
#   110GGGGG 0 DDDDDDDD, bit 0 = lowest function of the group, bit 7 = highest
# ---------------------------------------------------------------------------
_FEATURE_GROUPS = (
    (13, 0xDE),    # 2.3.6.5  GGGGG=11110  F13-F20
    (21, 0xDF),    # 2.3.6.6  GGGGG=11111  F21-F28
    (29, 0xD8),    # 2.3.6.7  GGGGG=11000  F29-F36
    (37, 0xD9),    # 2.3.6.8  GGGGG=11001  F37-F44
    (45, 0xDA),    # 2.3.6.9  GGGGG=11010  F45-F52
    (53, 0xDB),    # 2.3.6.10 GGGGG=11011  F53-F60
    (61, 0xDC),    # 2.3.6.11 GGGGG=11100  F61-F68
)


def function_expansion_group(addr, first_func, bits):
    """One F13-F68 group as an 8-bit mask (S-9.2.1 2.3.6.5-2.3.6.11): first_func is
    the group's lowest function (13, 21, 29, 37, 45, 53 or 61); bit 0 of `bits`
    is that function, bit 7 the highest of the group."""
    for first, inst in _FEATURE_GROUPS:
        if first == first_func:
            return list(addr) + [inst, bits & 0xFF]
    raise ValueError("first_func must be one of 13, 21, 29, 37, 45, 53, 61")


def function_expansion(addr, func_num, state=True):
    """A single function F13..F68 on or off (S-9.2.1 2.3.6.5-2.3.6.11); every other
    function in its group is sent as off."""
    for first, inst in _FEATURE_GROUPS:
        if first <= func_num <= first + 7:
            return list(addr) + [inst, ((1 if state else 0) << (func_num - first)) & 0xFF]
    raise ValueError("function_expansion supports F13..F68")


# ---------------------------------------------------------------------------
# Decoder and consist control (S-9.2.1 2.3.1.4)
# ---------------------------------------------------------------------------
def consist_set(addr, consist_addr, direction_normal=True):
    """Consist Control (S-9.2.1 2.3.1.4): 0001TTTT 0 0AAAAAAA. TTTT = 0010 (0x12)
    activates the consist at AAAAAAA with this unit in its normal direction (CV19 bit
    7 := 0); TTTT = 0011 (0x13) with this unit reversed (CV19 bit 7 := 1). AAAAAAA is
    1..127; "If the address is 0000000 then the consist is deactivated" -- so
    consist_addr = 0 is the spec's way to remove the unit from a consist."""
    if not 0 <= consist_addr <= 127:
        raise ValueError("consist address must be 0..127 (0 deactivates)")
    inst = 0x12 if direction_normal else 0x13
    return list(addr) + [inst, consist_addr & 0x7F]


consist = consist_set     # older name, kept for the existing suites


# ---------------------------------------------------------------------------
# Feature expansion -- binary state (S-9.2.1 2.3.6.1, 2.3.6.4) and analog (2.3.2.3)
# ---------------------------------------------------------------------------
def binary_state_short(addr, state_num, on=True):
    """Binary State Control short form (S-9.2.1 2.3.6.4): 1101 1101 (0xDD) + D LLLLLLL
    (state 1..127)."""
    return list(addr) + [0xDD, ((1 if on else 0) << 7) | (state_num & 0x7F)]


def binary_state_long(addr, state_num, on=True):
    """Binary State Control long form (S-9.2.1 2.3.6.1): 1100 0000 (0xC0) + D LLLLLLL +
    HHHHHHHH (15-bit state, low 7 bits first)."""
    lo = ((1 if on else 0) << 7) | (state_num & 0x7F)
    hi = (state_num >> 7) & 0xFF
    return list(addr) + [0xC0, lo, hi]


def analog_function(addr, output, value):
    """Analog Function Group (S-9.2.1 2.3.2.3): 0011 1101 (0x3D) + output + value."""
    return list(addr) + [0x3D, output & 0xFF, value & 0xFF]


# ---------------------------------------------------------------------------
# Configuration Variable access -- Long Form, operations mode / POM (S-9.2.1 2.3.7.3)
#   1110 GGVV  VVVVVVVV  DDDDDDDD      (VV..V = CV number minus 1, 10-bit)
#   GG = 01 verify byte, 11 write byte, 10 bit manipulation
# ---------------------------------------------------------------------------
def _cv_hi_lo(cv):
    if not 1 <= cv <= 1024:
        raise ValueError("CV number must be 1..1024")
    a = cv - 1
    return (a >> 8) & 0x03, a & 0xFF


def _cv_bit_data(bit, value, write):
    """Bit-manipulation data byte 111FDBBB (S-9.2.1 2.3.7.3; S-9.2.3 calls F 'K'):
    F/K = 1 write bit, 0 verify bit; D = bit value; BBB = bit position 0..7."""
    if not 0 <= bit <= 7:
        raise ValueError("bit must be 0..7")
    return 0xE0 | ((1 if write else 0) << 4) | ((value & 1) << 3) | (bit & 0x07)


def cv_write_pom(addr, cv, value):
    """POM write byte (S-9.2.1 2.3.7.3, GG=11): 0xEC|VV, CV-1 low byte, value."""
    hi, lo = _cv_hi_lo(cv)
    return list(addr) + [0xEC | hi, lo, value & 0xFF]


def cv_verify_pom(addr, cv, value):
    """POM verify byte (S-9.2.1 2.3.7.3, GG=01): 0xE4|VV, CV-1 low byte, value."""
    hi, lo = _cv_hi_lo(cv)
    return list(addr) + [0xE4 | hi, lo, value & 0xFF]


def cv_bit_pom(addr, cv, bit, value, write=True):
    """POM bit manipulation (S-9.2.1 2.3.7.3, GG=10): 0xE8|VV, CV-1 low byte,
    data 111FDBBB (F=1 write / 0 verify, D = bit value, BBB = bit position 0..7)."""
    hi, lo = _cv_hi_lo(cv)
    return list(addr) + [0xE8 | hi, lo, _cv_bit_data(bit, value, write)]


# ---------------------------------------------------------------------------
# Accessory decoder packets (S-9.2.1 2.4)
#
# The 11-bit accessory address A10..A0 is split the same way in every form:
#   byte 1 = 10 A7 A6 A5 A4 A3 A2
#   byte 2 bits 6..4 = ones' complement of A10 A9 A8 ; bits 2..1 = A1 A0
# S-9.2.2 CV513 (6 bits) + CV521 (3 bits) hold A10..A2 -- the 9-bit "board"
# (decoder) address -- and A1 A0 select one of the board's four output pairs.
# ---------------------------------------------------------------------------
def _acc_address_bytes(address):
    """(byte 1, ones'-complement A10A9A8 as a 3-bit value, A1A0) for A10..A0 = address."""
    if not 0 <= address <= 2047:
        raise ValueError("accessory address must be 0..2047 (11-bit A10..A0)")
    byte1 = 0x80 | ((address >> 2) & 0x3F)
    high_inv = (~(address >> 8)) & 0x07
    low2 = address & 0x03
    return byte1, high_inv, low2


def accessory_basic_output(output_address, r, activate=True):
    """Basic Accessory Decoder packet, output-address form (S-9.2.1 2.4.1):
        {preamble} 0 10A7A6A5A4A3A2 0 1 A10' A9' A8' D A1 A0 R 0 EEEEEEEE 1
    output_address = A10..A0 (0..2047, the 11-bit address S-9.2.2 CV541 bit 6 selects),
    R = which output of the pair (spec convention: 0 diverging/left/stop,
    1 normal/right/proceed), D = activate (bit 3). A10..A8 go out inverted.
    Example: output address 4, R=1, D=1 -> 0x81 0xF9."""
    byte1, high_inv, low2 = _acc_address_bytes(output_address)
    byte2 = (0x80 | (high_inv << 4) | (0x08 if activate else 0x00)
             | (low2 << 1) | (r & 0x01))
    return [byte1, byte2]


def accessory_basic(board, output_pair, activate=True):
    """Basic Accessory Decoder packet, decoder-address form (S-9.2.1 2.4.1 in the
    prior-edition notation 1AAACDDD, footnote 12): board = the 9-bit decoder address
    A10..A2 (0..511, S-9.2.2 CV513 low 6 bits | CV521 low 3 bits << 6), output_pair =
    the 3-bit DDD field 0..7 (= A1 A0 R: pair number << 1 | output within the pair),
    activate = C/D (bit 3). Same wire bytes as accessory_basic_output."""
    if not 0 <= board <= 511:
        raise ValueError("board address must be 0..511")
    if not 0 <= output_pair <= 7:
        raise ValueError("output pair (DDD) must be 0..7")
    return accessory_basic_output((board << 2) | (output_pair >> 1), output_pair & 0x01,
                                  activate)


def accessory_extended(address, aspect):
    """Extended Accessory Decoder Control packet (S-9.2.1 2.4.2):
        {preamble} 0 10A7A6A5A4A3A2 0 0 A10' A9' A8' 0 A1 A0 1 0 XXXXXXXX 0 EEEEEEEE 1
    address = A10..A0 (0..2047); A10..A8 inverted; byte 2 bit 7 = 0, bit 3 = 0, bit 0 = 1
    distinguish it from the basic form. XXXXXXXX = aspect (0 = absolute stop).
    Spec example: "address 1" is 10000001 01110001, i.e. A10..A0 = 4.
    For a decoder addressed by board (S-9.2.2 CV513/CV521 = A10..A2) pass board << 2."""
    byte1, high_inv, low2 = _acc_address_bytes(address)
    return [byte1, (high_inv << 4) | (low2 << 1) | 0x01, aspect & 0xFF]


# Accessory Decoder Configuration Variable Access (S-9.2.1 2.4.3): the 2.3.7.3 long
# form (1110GGVV 0 VVVVVVVV 0 DDDDDDDD) behind an accessory address header --
#   basic    (2.4.3.1): 10AAAAAA 0 1 A10' A9' A8' 1 A1 A0 0   (bit 3 = 1: "New programming
#                        devices must by default set bit 3 of the second byte to 1")
#   extended (2.4.3.2): 10AAAAAA 0 0 A10' A9' A8' 0 A1 A0 1
# -- a 6-byte packet; the spec notes the form is told from an operating command only
# by packet length (footnotes 17/18).
def _acc_cv_packet(address, extended, gg, cv, data):
    byte1, high_inv, low2 = _acc_address_bytes(address)
    if extended:
        byte2 = (high_inv << 4) | (low2 << 1) | 0x01
    else:
        byte2 = 0x80 | (high_inv << 4) | 0x08 | (low2 << 1)
    hi, lo = _cv_hi_lo(cv)
    return [byte1, byte2, 0xE0 | (gg << 2) | hi, lo, data & 0xFF]


def accessory_cv_write(address, cv, value, extended=False):
    """Accessory ops-mode CV write byte (S-9.2.1 2.4.3.1 basic / 2.4.3.2 extended header
    + 2.3.7.3 GG=11). address = A10..A0 (board << 2 | pair bits)."""
    return _acc_cv_packet(address, extended, 0x03, cv, value)


def accessory_cv_verify(address, cv, value, extended=False):
    """Accessory ops-mode CV verify byte (S-9.2.1 2.4.3 + 2.3.7.3 GG=01)."""
    return _acc_cv_packet(address, extended, 0x01, cv, value)


def accessory_cv_bit(address, cv, bit, value, write=True, extended=False):
    """Accessory ops-mode CV bit manipulation (S-9.2.1 2.4.3 + 2.3.7.3 GG=10, data 111FDBBB)."""
    return _acc_cv_packet(address, extended, 0x02, cv, _cv_bit_data(bit, value, write))


# ---------------------------------------------------------------------------
# Service mode (S-9.2.3) -- long-preamble packets, no address byte
# ---------------------------------------------------------------------------
def reset_packet():
    """Digital Decoder Reset packet (S-9.2 / S-9.2.3): 00000000 00000000 (XOR 00).
    Three or more of these open a service-mode sequence."""
    return [0x00, 0x00]


# Direct mode (S-9.2.3 "Service Mode Instruction Packets for Direct Mode", ~line 110):
#   long-preamble 0 0111CCAA 0 AAAAAAAA 0 DDDDDDDD 0 EEEEEEEE 1
#   CC = 01 verify byte, 11 write byte, 10 bit manipulation; AA AAAAAAAA = CV - 1.
def svc_direct_verify_byte(cv, value):
    """Direct-mode VERIFY CV BYTE (S-9.2.3, CC=01): 0x74|AA, CV-1 low byte, expected
    value. Sent with the 20-bit service-mode preamble; the decoder ACKs only if the
    CV holds `value`."""
    hi, lo = _cv_hi_lo(cv)
    return [0x74 | hi, lo, value & 0xFF]


def svc_direct_write_byte(cv, value):
    """Direct-mode WRITE CV BYTE (S-9.2.3, CC=11): 0x7C|AA, CV-1 low byte, value.
    The decoder may ACK on completion of the write."""
    hi, lo = _cv_hi_lo(cv)
    return [0x7C | hi, lo, value & 0xFF]


def svc_direct_bit(cv, bit, value, write=True):
    """Direct-mode CV BIT MANIPULATION (S-9.2.3, CC=10):
        long-preamble 0 011110AA 0 AAAAAAAA 0 111KDBBB 0 EEEEEEEE 1
    K=1 write bit / K=0 verify bit, D = bit value, BBB = bit position (000 = bit 0)."""
    hi, lo = _cv_hi_lo(cv)
    return [0x78 | hi, lo, _cv_bit_data(bit, value, write)]


# Physical Register and Paged addressing share one 3-byte form (S-9.2.3 ~lines 180/220):
#   long-preamble 0 0111CRRR 0 DDDDDDDD 0 EEEEEEEE 1      C = 1 write, 0 verify
# RRR = register - 1. Registers 1-4 are the data registers (paged mode: the four CVs
# of the selected page), 5 (RRR=100) the Basic Configuration Register (CV29, or CV541
# for an accessory decoder), 6 (RRR=101) the Paging Register (default 1).
SVC_PAGE_REGISTER = 6


def svc_register_write(register_number, value):
    """Physical Register WRITE (S-9.2.3, 0111 1RRR + data): register 1..8 -> RRR = n-1."""
    if not 1 <= register_number <= 8:
        raise ValueError("register must be 1..8")
    return [0x78 | (register_number - 1), value & 0xFF]


def svc_register_verify(register_number, value):
    """Physical Register VERIFY (S-9.2.3, 0111 0RRR + data): ACK if the register holds value."""
    if not 1 <= register_number <= 8:
        raise ValueError("register must be 1..8")
    return [0x70 | (register_number - 1), value & 0xFF]


def svc_page_select(page):
    """Paged mode: write the Paging Register (register 6, RRR=101) -- S-9.2.3 Paged CV
    Addressing. Page 1 is the default; page p exposes CVs 4(p-1)+1 .. 4(p-1)+4 as data
    registers 1..4."""
    if not 1 <= page <= 255:
        raise ValueError("page must be 1..255")
    return svc_register_write(SVC_PAGE_REGISTER, page)


def svc_paged_write(data_register, value):
    """Paged mode WRITE of data register 1..4 on the currently selected page (same wire
    form as svc_register_write, S-9.2.3 Paged CV Addressing)."""
    if not 1 <= data_register <= 4:
        raise ValueError("paged data register must be 1..4")
    return svc_register_write(data_register, value)


def svc_paged_verify(data_register, value):
    """Paged mode VERIFY of data register 1..4 on the currently selected page."""
    if not 1 <= data_register <= 4:
        raise ValueError("paged data register must be 1..4")
    return svc_register_verify(data_register, value)


def paged_cv_number(page, data_register):
    """The CV a (page, data register 1..4) pair addresses (S-9.2.3: (page-1)*4 + (reg-1) + 1)."""
    return (page - 1) * 4 + (data_register - 1) + 1
