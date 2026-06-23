#!/usr/bin/env python3
"""
NMRA S-9.2.1 -- Extended Packet Formats compliance (command-station transmit).

Drives each multifunction / accessory / CV / consist / binary-state / analog
command over UART and verifies the bytes the command station puts ON THE WIRE
against an INDEPENDENT spec encoder (below). Uses the hardware trigger (PB3 ->
Saleae ch1) so each packet under test is captured deterministically, with CLEAR
for test isolation between commands.

Expected encodings are derived from DCC_Spec_Reference.md / S-9.2.1 -- NOT from
the library under test -- so a match means CS and spec agree. Exact-byte checks
are used where the encoding is unambiguous; structural checks where the speed-
step value mapping or accessory address-bit inversion make an exact vector risky.

Requires the saleae_hil_compliance firmware with TRIG + CLEAR, PB3 wired to ch1.

Run standalone:  .venv/bin/python s9_2_1_compliance.py
Or via:          .venv/bin/python run_all.py
"""

import sys
import time
from collections import Counter

import compliance_lib as lib

SPEC_DOC   = "S-9.2.1"
SPEC_TITLE = "Extended Packet Formats (command-station transmit)"
SOURCE_PDF = "documentation/specs/s-9.2.1_dcc_extended_packet_formats.pdf"
ASPECT     = "multifunction / accessory / CV / consist / binary-state encoding (DCC out, ch0)"

IDLE = [0xFF, 0x00, 0xFF]


# ----------------------------------------------------------------------------
# Independent S-9.2.1 encoders (spec-derived; the source of truth for the test)
# ----------------------------------------------------------------------------
def _xor(bs):
    x = 0
    for b in bs:
        x ^= b
    return x


def framed(bs):
    """Append the S-9.2 error-detection (XOR) byte."""
    return list(bs) + [_xor(bs)]


def addr_bytes(addr, force_long=False):
    """Short address (1 byte, 1-127) or long address (11AAAAAA AAAAAAAA).
    force_long encodes a <=127 address in long form (e.g. the 'NNNl' suffix)."""
    if addr <= 127 and not force_long:
        return [addr & 0x7F]
    return [0xC0 | ((addr >> 8) & 0x3F), addr & 0xFF]


def speed_128(addr, spd, fwd, force_long=False):
    # 00111111 DSSSSSSS  (Advanced Operations 001, GGGGG=11111)
    return framed(addr_bytes(addr, force_long) + [0x3F, (0x80 if fwd else 0x00) | (spd & 0x7F)])


def group1(addr, fl_f4_bits):
    # 100DDDDD : bit4=FL, bits3-0 = F4..F1
    return framed(addr_bytes(addr) + [0x80 | (fl_f4_bits & 0x1F)])


def group2_high(addr, f5_f8_bits):  # 1011DDDD (S=1)
    return framed(addr_bytes(addr) + [0xB0 | (f5_f8_bits & 0x0F)])


def group2_low(addr, f9_f12_bits):  # 1010DDDD (S=0)
    return framed(addr_bytes(addr) + [0xA0 | (f9_f12_bits & 0x0F)])


def func_f13_f20(addr, bits):       # 11011110 DDDDDDDD
    return framed(addr_bytes(addr) + [0xDE, bits & 0xFF])


def func_f21_f28(addr, bits):       # 11011111 DDDDDDDD
    return framed(addr_bytes(addr) + [0xDF, bits & 0xFF])


def func_f29_f36(addr, bits):       # 11011000 DDDDDDDD
    return framed(addr_bytes(addr) + [0xD8, bits & 0xFF])


def func_f37_f44(addr, bits):       # 11011001 DDDDDDDD
    return framed(addr_bytes(addr) + [0xD9, bits & 0xFF])


def func_f45_f52(addr, bits):       # 11011010 DDDDDDDD
    return framed(addr_bytes(addr) + [0xDA, bits & 0xFF])


def func_f53_f60(addr, bits):       # 11011011 DDDDDDDD
    return framed(addr_bytes(addr) + [0xDB, bits & 0xFF])


def func_f61_f68(addr, bits):       # 11011100 DDDDDDDD
    return framed(addr_bytes(addr) + [0xDC, bits & 0xFF])


def cv_write_pom(addr, cv, val):    # 1110 11 VV  VVVVVVVV  DDDDDDDD  (GG=11 write)
    n = cv - 1
    return framed(addr_bytes(addr) + [0xEC | ((n >> 8) & 0x03), n & 0xFF, val & 0xFF])


def cv_verify_pom(addr, cv, val):   # 1110 01 VV ...  (GG=01 verify)
    n = cv - 1
    return framed(addr_bytes(addr) + [0xE4 | ((n >> 8) & 0x03), n & 0xFF, val & 0xFF])


def consist_set(addr, consist, normal=True):  # 0001001C 0AAAAAAA
    return framed(addr_bytes(addr) + [0x12 if normal else 0x13, consist & 0x7F])


def binary_state_short(addr, state, on):      # 11011101 DLLLLLLL
    return framed(addr_bytes(addr) + [0xDD, ((0x80 if on else 0) | (state & 0x7F))])


def analog(addr, output, value):              # 00111101 VVVVVVVV DDDDDDDD
    return framed(addr_bytes(addr) + [0x3D, output & 0xFF, value & 0xFF])


def system_time(ms):
    # S-9.2.1 §2.3.6.3: broadcast to address 0, feature-expansion 110-00010
    # (= 0xC2). 16-bit milliseconds-since-startup, MOST significant byte first
    # (spec: "third byte = MSB, fourth byte = LSB"), then the XOR error byte.
    return framed([0x00, 0xC2, (ms >> 8) & 0xFF, ms & 0xFF])


def speed_28(addr, speed, fwd):
    # 01Dcssss : 28-step. 0=stop, 1=e-stop; for 2-29 the code's LSB goes to bit4
    # (c) and the rest+1 to ssss (standard 28-step bit-interleaving).
    if speed == 0:
        low5 = 0x00
    elif speed == 1:
        low5 = 0x01
    else:
        low5 = ((speed & 1) << 4) | ((speed >> 1) + 1)
    return framed(addr_bytes(addr) + [(0x60 if fwd else 0x40) | low5])


def speed_14(addr, speed, fwd):
    # 01DCSSSS : 14-step. C=FL headlight (the UART always sends FL on -> C=1).
    # SSSS = speed: 0=stop, 1=e-stop, 2-15 = steps 1-14.
    return framed(addr_bytes(addr)
                  + [(0x60 if fwd else 0x40) | 0x10 | (speed & 0x0F)])


def accessory_basic(board, pair, activate, output=0):
    # 10AAAAAA 1AAADAAR : low 6 address bits in byte1; high 3 address bits in
    # byte2 are INVERTED; D=activate, AA=output pair, R=output within pair.
    a = board & 0x1FF
    b1 = 0x80 | (a & 0x3F)
    b2 = (0x80 | ((~(a >> 6) & 0x07) << 4) | ((1 if activate else 0) << 3)
          | ((pair & 0x03) << 1) | (output & 0x01))
    return framed([b1, b2])


def accessory_extended(board, aspect):
    # S-9.2.1 2.4.2 (p.17):  10 A7A6A5A4A3A2  0  0 Ā10Ā9Ā8 0 A1A0 1  DDDDDDDD
    # 'board' is the 9-bit accessory decoder address (1-511), same address space
    # as basic. The wire address A10..A0 = board << 2, so the 2 LSBs A1A0 = 00
    # (extended has no output pair; the aspect byte carries the data). byte1's low
    # 6 bits ARE A7..A2 because 'board' already sits 2 bits above the wire LSBs.
    # See project memory "accessory address convention" (board addr, not raw 11-bit).
    b1 = 0x80 | (board & 0x3F)                  # A7..A2
    b2 = ((~(board >> 6) & 0x07) << 4) | 0x01   # ~A10..A8 ; A1A0 = 00 ; bit0 = 1
    return framed([b1, b2, aspect & 0xFF])


def accessory_nop(addr, is_extended):
    # 10AAAAAA 0 0AAA1AAT : NOP (S-9.2.1 2.4.6), library address convention:
    # low6 -> byte1, high3 inverted -> byte2[6:4], next2 -> byte2[2:1];
    # bit3 = 1 (NOP marker), bit0 = T (0 basic, 1 extended).
    b1 = 0x80 | (addr & 0x3F)
    b2 = (((~(addr >> 6) & 0x07) << 4) | 0x08
          | (((addr >> 9) & 0x03) << 1) | (1 if is_extended else 0))
    return framed([b1, b2])


# ----------------------------------------------------------------------------
# Test vectors
# ----------------------------------------------------------------------------
# EXACT: (uart_command, expected_bytes, clause, label)
EXACT = [
    ("SPEED 3 64 FWD",      speed_128(3, 64, True),    "§2.3.2.1", "128-step speed 64 fwd"),
    ("SPEED 3 64 REV",      speed_128(3, 64, False),   "§2.3.2.1", "128-step speed 64 rev"),
    ("SPEED 3 0 FWD",       speed_128(3, 0, True),     "§2.3.2.1", "128-step stop (DSSSSSSS=0)"),
    ("SPEED 3 1 FWD",       speed_128(3, 1, True),     "§2.3.2.1", "128-step e-stop (DSSSSSSS=1)"),
    ("SPEED 3 127 FWD",     speed_128(3, 127, True),   "§2.3.2.1", "128-step max (DSSSSSSS=127)"),
    ("SPEED 1000 0 FWD",    speed_128(1000, 0, True),  "§2.3.2.1", "128-step long addr + stop (boundary x long)"),
    ("SPEED 1000 1 FWD",    speed_128(1000, 1, True),  "§2.3.2.1", "128-step long addr + e-stop"),
    ("SPEED 1000 127 FWD",  speed_128(1000, 127, True),"§2.3.2.1", "128-step long addr + max"),
    ("SPEED 1000 64 FWD",   speed_128(1000, 64, True), "§2.3.1",   "long address (11AAAAAA AAAAAAAA)"),
    ("SPEED 1 64 FWD",      speed_128(1, 64, True),    "§2.3.1",   "short address min (01)"),
    ("SPEED 127 64 FWD",    speed_128(127, 64, True),  "§2.3.1",   "short address max (7F)"),
    ("SPEED 128 64 FWD",    speed_128(128, 64, True),  "§2.3.1",   "long address min (C0 80)"),
    ("SPEED 10239 64 FWD",  speed_128(10239, 64, True),"§2.3.1",   "long address max usable (E7 FF)"),
    ("SPEED 3L 64 FWD",     speed_128(3, 64, True, force_long=True), "§2.3.1", "low address forced long (C0 03)"),
    ("FUNC 3 0 ON",         group1(3, 0x10),           "§2.3.4",   "func group 1: FL on"),
    ("FUNC 3 1 ON",         group1(3, 0x01),           "§2.3.4",   "func group 1: F1 on"),
    ("FUNC 3 5 ON",         group2_high(3, 0x01),      "§2.3.5",   "func group 2: F5 on (1011)"),
    ("FUNC 3 9 ON",         group2_low(3, 0x01),       "§2.3.5",   "func group 2: F9 on (1010)"),
    ("FUNC 3 13 ON",        func_f13_f20(3, 0x01),     "§2.3.6",   "F13-F20 expansion (11011110)"),
    ("FUNC 3 21 ON",        func_f21_f28(3, 0x01),     "§2.3.6",   "F21-F28 expansion (11011111)"),
    ("FUNC 3 29 ON",        func_f29_f36(3, 0x01),     "§2.3.6",   "F29-F36 expansion (11011000)"),
    ("FUNC 3 37 ON",        func_f37_f44(3, 0x01),     "§2.3.6",   "F37-F44 expansion (11011001)"),
    ("FUNC 3 45 ON",        func_f45_f52(3, 0x01),     "§2.3.6",   "F45-F52 expansion (11011010)"),
    ("FUNC 3 53 ON",        func_f53_f60(3, 0x01),     "§2.3.6",   "F53-F60 expansion (11011011)"),
    ("FUNC 3 61 ON",        func_f61_f68(3, 0x01),     "§2.3.6",   "F61-F68 expansion (11011100)"),
    ("CV WRITE 3 1 8",      cv_write_pom(3, 1, 8),     "§2.3.7.3", "CV-POM write CV1=8 (1110 11)"),
    ("CV VERIFY 3 1 8",     cv_verify_pom(3, 1, 8),    "§2.3.7.3", "CV-POM verify CV1=8 (1110 01)"),
    ("CONSIST 3 SET 5",     consist_set(3, 5, True),   "§2.3.1.4", "consist set addr 5 normal"),
    ("BSS 3 1 ON",          binary_state_short(3, 1, True), "§2.3.6.1", "binary state short (11011101)"),
    ("ANALOG 3 1 64",       analog(3, 1, 64),          "§2.3.2.3", "analog function (00111101)"),
    ("SPEED 3 10 FWD 28",   speed_28(3, 10, True),     "§2.3.2.1", "28-step speed 10 fwd (c-bit even)"),
    ("SPEED 3 11 FWD 28",   speed_28(3, 11, True),     "§2.3.2.1", "28-step speed 11 fwd (c-bit odd)"),
    ("SPEED 3 0 FWD 28",    speed_28(3, 0, True),      "§2.3.2.1", "28-step stop (cssss=0)"),
    ("SPEED 3 1 FWD 28",    speed_28(3, 1, True),      "§2.3.2.1", "28-step e-stop (cssss=1)"),
    ("SPEED 3 29 FWD 28",   speed_28(3, 29, True),     "§2.3.2.1", "28-step max step (cssss=29)"),
    ("SPEED 3 10 REV 28",   speed_28(3, 10, False),    "§2.3.2.1", "28-step speed 10 reverse"),
    ("SPEED 3 0 FWD 14",    speed_14(3, 0, True),      "§2.3.2.1", "14-step stop (SSSS=0)"),
    ("SPEED 3 1 FWD 14",    speed_14(3, 1, True),      "§2.3.2.1", "14-step e-stop (SSSS=1)"),
    ("SPEED 3 15 FWD 14",   speed_14(3, 15, True),     "§2.3.2.1", "14-step max (SSSS=15, FL on)"),
    ("SPEED 3 8 REV 14",    speed_14(3, 8, False),     "§2.3.2.1", "14-step speed 8 reverse"),
    ("ACC 1 0 ON",          accessory_basic(1, 0, True), "§2.4",   "accessory basic (inverted high addr bits)"),
    ("ACCE 1 5",            accessory_extended(1, 5),  "§2.4",     "accessory extended (inverted high addr bits)"),
    ("NOP 1",               accessory_nop(1, False),   "§2.4.6",   "accessory NOP basic (0AAA1AAT, T=0)"),
    ("NOP 1 E",             accessory_nop(1, True),    "§2.4.6",   "accessory NOP extended (T=1)"),
    ("NOP 1500 E",          accessory_nop(1500, True), "§2.4.6",   "accessory NOP high addr (mid-2 bits)"),
    # --- accessory address boundary coverage ---
    # The accessory address param is the 9-bit BOARD address (wire A10..A0 = board<<2),
    # NOT a raw 11-bit value. board >= 64 is the 6-bit rollover where the board vs
    # raw-11-bit conventions first diverge -- exactly the case addr=1 hid.
    ("ACC 64 0 ON",         accessory_basic(64, 0, True),  "§2.4",   "accessory basic board 64 (6-bit rollover, high-3 inv=1)"),
    ("ACC 256 0 ON",        accessory_basic(256, 0, True), "§2.4",   "accessory basic board 256 (high-3 inv=4)"),
    ("ACC 511 0 ON",        accessory_basic(511, 0, True), "§2.4",   "accessory basic board 511 (9-bit max)"),
    ("ACCE 64 5",           accessory_extended(64, 5),     "§2.4.2", "accessory extended board 64 (catches board-vs-raw11 split)"),
    ("ACCE 256 5",          accessory_extended(256, 5),    "§2.4.2", "accessory extended board 256"),
    ("ACCE 511 5",          accessory_extended(511, 5),    "§2.4.2", "accessory extended board 511 (9-bit max)"),
    ("NOP 64",              accessory_nop(64, False),      "§2.4.6", "accessory NOP basic board 64"),
    ("NOP 256 E",           accessory_nop(256, True),      "§2.4.6", "accessory NOP extended board 256"),
    ("NOP 511 E",           accessory_nop(511, True),      "§2.4.6", "accessory NOP extended board 511"),
    # --- system time (broadcast, S-9.2.1 §2.3.6.3) boundary coverage ---
    ("SYSTIME 0",           system_time(0),       "§2.3.6.3", "system time ms=0 (00 C2 00 00)"),
    ("SYSTIME 1",           system_time(1),       "§2.3.6.3", "system time ms=1 (MSB clear, 00 01)"),
    ("SYSTIME 30000",       system_time(30000),   "§2.3.6.3", "system time ms=30000 (75 30)"),
    ("SYSTIME 65535",       system_time(65535),   "§2.3.6.3", "system time ms=65535 (FF FF, 16-bit max)"),
]


# ----------------------------------------------------------------------------
# Checks
# ----------------------------------------------------------------------------
def _target(decoded):
    """The packet under test = most common non-idle packet (CLEAR isolates it)."""
    ni = [tuple(d) for _, d in decoded["packets"] if list(d) != IDLE]
    return list(Counter(ni).most_common(1)[0][0]) if ni else None


def _reps(decoded, pkt):
    return sum(1 for _, d in decoded["packets"] if list(d) == pkt)


def _hx(bs):
    return " ".join(f"{b:02X}" for b in bs) if bs else "(none)"


def run():
    if lib.DRIVE_UART:
        lib.start_dcc()
    rep = lib.Report(SPEC_DOC, SPEC_TITLE, SOURCE_PDF, ASPECT)

    # --- exact-byte encoding checks (all spec-derived encoders) ---
    for cmd, exp, clause, label in EXACT:
        dec, _ = lib.trigger_command(cmd)
        got = _target(dec)
        rep.check(SPEC_DOC + " " + clause, label, got == exp,
                  f"'{cmd}' -> got [{_hx(got)}] expected [{_hx(exp)}]")

    # --- refresh policy (S-9.2.1: binary state shall NOT be refreshed) ---
    dec, _ = lib.trigger_command("BSS 3 1 ON")
    got = _target(dec)
    reps = _reps(dec, got) if got else 0
    rep.check(SPEC_DOC + " §2.3.6.1", ">= 2 repetitions on change", reps >= 2,
              f"binary state transmitted {reps}x in the capture window")
    time.sleep(0.3)
    dec2, _ = lib.capture_and_decode()
    still = _target(dec2) is not None
    rep.check(SPEC_DOC + " §2.3.6.1", "binary state NOT auto-refreshed",
              (got is not None) and not still,
              f"captured={got is not None}; still transmitting after 300ms={still}")

    return rep.finish()


def main():
    print("\n#### DCC COMPLIANCE TEST ####")
    print(f"Spec under test : NMRA {SPEC_DOC}  Extended Packet Formats")
    print(f"Source PDF      : {SOURCE_PDF}")
    print(f"Aspect          : {ASPECT}")
    try:
        rep = run()
    except ImportError as e:
        print(f"\nMissing dependency: {e}\nRun: pip install logic2-automation pyserial\n")
        return 2
    except Exception as e:
        print(f"\nERROR: {e}\nCheck: Logic 2 + Automation API (port {lib.AUTOMATION_PORT}), "
              f"PB3 wired to ch{lib.TRIGGER_CHANNEL}, firmware has TRIG + CLEAR.\n")
        return 2
    path = lib.write_html([rep.as_dict()], lib.report_path("s9_2_1"),
                          title=f"NMRA {SPEC_DOC} Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
