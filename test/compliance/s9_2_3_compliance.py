#!/usr/bin/env python3
"""
NMRA S-9.2.3 -- Service Mode compliance (command-station transmit).

Drives service-mode operations over UART and verifies the bytes the command
station puts ON THE SERVICE TRACK (ch3 / PB4) against an INDEPENDENT spec encoder
(below). Expected encodings are derived from S-9.2.3 §E -- NOT from the library
under test -- so a match means CS and spec agree.

Organized as four per-mode functions (test_direct / test_register / test_address
/ test_paged) plus ack_width_tests, each checking the exact §E command encoding
and sequence structure for that mode.

Parallel-track timing: both the main track (ch0/PB1) and the service track
(ch3/PB4) run off the SAME 58us ISR. Every service-mode operation here is captured
on BOTH channels at once, and `check_track_timing()` (the reusable block) verifies
neither track's half-bit periods are stretched or dropped while service mode runs
-- the exact risk the shared-ISR design carries (see detail_s9_2_3_service_mode.md).

What this verifies NOW (wire-level, no decoder needed):
  - Direct: all 3 instruction types (write byte CC=11, verify byte CC=01, bit
    manipulation CC=10 K=1/0) + 10-bit CV addressing (CV#1 .. CV1023)
  - Register / Address-Only: the page-preset (7D 01 7C) precedes the command;
    VERIFY commands (0111 0RRR / 0111 0000) on the wire with their repeat counts
    (register verify 7+, address verify 5+) via SVC REG/ADDR VERIFY
  - Paged: the page-register write AND the data-register command (now on the wire)
  - Decoder factory reset (7F 08 77)
  - ACK scan-window blanking (S-9.2.3 line 55): a valid pulse fired in-window is
    detected; the same pulse fired EARLY (packet 1) is masked
  - >= 20-bit service-mode long preamble (S-9.2.3 §D), >= 3 reset packets,
    command-packet repeat count (S-9.2.3 §E)
  - main + service track half-bit timing stays in S-9.1 Tbl 2.1 tolerance during SVC
  - ACK pulse-width detection (6 ms +/- 1 ms window): ACK vs NO ACK at the
    MIN-1 / MIN / MAX / MAX+1 boundaries, exercised electrically via the mock-ACK
    loopback (MOCK_ACK_DRIVE PB24 -> jumper -> MOCK_ACK PB9, `SVC MOCKACK <us>`),
    cross-checked against the D4-measured pulse width
  - Read-back / write+verify VALUE correctness (Direct) end-to-end through the real
    ACK path, via the mock decoder (`SVC MOCKCV <cv> <val>`): a held value is read
    back bit-by-bit, a write updates it (verify confirm succeeds), and with the mock
    OFF the same write/read take the failure path (VERIFY FAIL / value 0)

Host-verified (not on the wire here): register VERIFY's 7+ repeat count, and the
exact per-op recovery counts (6 / 10) -- there is no bounded single-verify UART
command, and recovery packets are hard to disambiguate from resets on the wire.

Bench: firmware on the LaunchPad, Logic 2 + Automation API (port 10430),
       MAIN track (PB1) on ch0, SERVICE track (PB4) on ch3, mock-ACK pin on ch4.
       See HIL_SETUP.md.

Run standalone:  .venv/bin/python s9_2_3_compliance.py
"""

import sys
import time
import tempfile

import compliance_lib as lib

SPEC_DOC   = "S-9.2.3"
SPEC_TITLE = "Service Mode (command-station transmit)"
SOURCE_PDF = "documentation/specs/S-9.2.3_2012_07.pdf"
ASPECT     = "service-mode packet encoding + parallel-track timing (ch0 main / ch3 service) + ACK width window (mock-ACK loopback)"

MAIN_CHANNEL    = 0              # Saleae channel on the main track (PB1)
SERVICE_CHANNEL = 3              # Saleae channel on the service track (PB4)
ACK_CHANNEL     = 4              # Saleae channel on the mock-ACK pin (PB9 / PB24 loopback)
CAPTURE_SECONDS = 0.5            # window to catch a service-op packet burst
OP_SETTLE_SEC   = 5.0            # let a (multi-op, retrying) operation finish

# S-9.1 Table 2.1 half-bit tolerances (command-station transmit). Both tracks
# share the one 58us ISR, so service-mode work must not stretch either track's
# bits -- these are checked on BOTH channels during every SVC operation.
ONE_HALF_MIN_US, ONE_HALF_MAX_US = 55.0, 61.0
ZERO_HALF_MIN_US      = 95.0
ZERO_BIT_TOTAL_MAX_US = 12000.0

RESET = [0x00, 0x00, 0x00]
IDLE  = [0xFF, 0x00, 0xFF]


# ----------------------------------------------------------------------------
# Independent S-9.2.3 §E Direct-mode encoders (spec-derived; source of truth).
#
#   Direct packet : 0111CCAA  AAAAAAAA  DDDDDDDD  EEEEEEEE
#     CC = 01 verify byte, 11 write byte, 10 bit manipulation
#     AA AAAAAAAA = 10-bit CV address = (CV number - 1)
#   Bit-manip data byte : 111KDBBB   (K=1 write / K=0 verify, D=value, BBB=bit)
#
# Verified against a live capture: read CV#3 bit0 -> 78 02 E8 92.
# ----------------------------------------------------------------------------
def _xor(bs):
    x = 0
    for b in bs:
        x ^= b
    return x


def framed(bs):
    """Append the S-9.2 error-detection (XOR) byte."""
    return list(bs) + [_xor(bs)]


def _cv_addr(cv):
    """10-bit CV address = CV number - 1; return (high 2 bits, low byte)."""
    a = (cv - 1) & 0x3FF
    return (a >> 8) & 0x03, a & 0xFF


def direct_verify_byte(cv, val):
    hi, lo = _cv_addr(cv)
    return framed([0x70 | (0b01 << 2) | hi, lo, val & 0xFF])


def direct_write_byte(cv, val):
    hi, lo = _cv_addr(cv)
    return framed([0x70 | (0b11 << 2) | hi, lo, val & 0xFF])


def direct_verify_bit(cv, bit, val):
    hi, lo = _cv_addr(cv)
    data = 0xE0 | (0 << 4) | ((val & 1) << 3) | (bit & 0x07)   # K=0 verify
    return framed([0x70 | (0b10 << 2) | hi, lo, data])


def direct_write_bit(cv, bit, val):
    hi, lo = _cv_addr(cv)
    data = 0xE0 | (1 << 4) | ((val & 1) << 3) | (bit & 0x07)   # K=1 write
    return framed([0x70 | (0b10 << 2) | hi, lo, data])


# Physical Register / Paged data-register command: 0111CRRR DDDDDDDD EEEEEEEE
#   C = 0 verify / 1 write ; RRR = register-1 (000=reg1 .. 111=reg8)
# Verified vs capture: verify reg8 -> 77 00 77 ; factory reset (write reg8=8) -> 7F 08 77.
def register_cmd(reg, value, write):
    rrr = (reg - 1) & 0x07
    return framed([0x70 | ((1 if write else 0) << 3) | rrr, value & 0xFF])


# Page-Preset = write the Page Register (register 6, RRR=101) with a page number.
# Verified vs capture: 7D 02 7F.
def page_preset(page):
    return framed([0x70 | (1 << 3) | 0b101, page & 0xFF])


# Address-only command: 0111C000 0DDDDDDD EEEEEEEE  (CV#1 short address only)
#   C = 0 verify / 1 write
def address_cmd(addr, write):
    return framed([0x70 | ((1 if write else 0) << 3) | 0b000, addr & 0x7F])


# Paged-mode data-register access for a CV: the data register is ((cv-1) % 4)+1
# (RRR = 0..3), addressed with the same 0111CRRR register form. Page = (cv-1)//4 + 1.
def paged_page(cv):
    return ((cv - 1) // 4) + 1


def paged_data_cmd(cv, value, write):
    data_register = ((cv - 1) % 4) + 1
    return register_cmd(data_register, value, write)


def _is_page_preset(data):
    """A page-preset packet writes the page register (byte1 = 0x7D)."""
    return len(data) >= 1 and data[0] == 0x7D


# ----------------------------------------------------------------------------
# Capture: fire an SVC op into a SIMULTANEOUS main+service timed capture.
# ----------------------------------------------------------------------------
def _wait_result(s, timeout=30.0):
    """Read the DUT UART until the op's async 'SVC RESULT' (or DETECT / failed)
    line arrives, so the singleton task is IDLE before the next op is fired.
    Returns the text seen (empty-ish if it timed out)."""
    end = time.time() + timeout
    buf = ""
    while time.time() < end:
        buf += s.read(256).decode(errors="replace")
        if ("SVC RESULT" in buf or "SVC DETECT" in buf
                or "failed to start" in buf):
            return buf
    return buf


def _wait_mockack(s, timeout=30.0):
    """Read the DUT UART until the 'SVC MOCKACK:' async verdict line arrives.
    Returns the text seen ('ACK DETECTED' or 'NO ACK'), empty-ish on timeout."""
    end = time.time() + timeout
    buf = ""
    while time.time() < end:
        buf += s.read(256).decode(errors="replace")
        if "SVC MOCKACK:" in buf or "failed to start" in buf:
            return buf
    return buf


def _send_ok(s, cmd):
    """Send a non-op config command (e.g. SVC MOCKCV) and drain its 'OK' reply."""
    s.reset_input_buffer()
    s.write((cmd + "\r").encode())
    time.sleep(0.3)
    return s.read(400).decode(errors="replace")


def _svc_result(s, cmd, timeout=30.0):
    """Send an SVC op and return its 'SVC RESULT: ...' line (or '(no result)')."""
    s.reset_input_buffer()
    s.write((cmd + "\r").encode())
    buf = _wait_result(s, timeout=timeout)
    for line in buf.splitlines():
        if "SVC RESULT" in line:
            return line.strip()
    return "(no result)"


def _measure_high_us(rows):
    """Longest contiguous logic-high run in a transition list, in microseconds.
    The ACK pin idles low, so the one mock pulse is the dominant high run."""
    best = 0.0
    t_rise = None
    for t, v in rows:
        if v == 1 and t_rise is None:
            t_rise = t
        elif v == 0 and t_rise is not None:
            best = max(best, (t - t_rise) * 1e6)
            t_rise = None
    return best


def _svc_op_dual(s, svc_cmd, seconds=CAPTURE_SECONDS):
    """Send `svc_cmd` into one capture of both the main (ch0) and service (ch3)
    channels, then wait for the op to finish. Return (main_decoded, svc_decoded)."""
    s.reset_input_buffer()

    def stim():
        s.write((svc_cmd + "\r").encode())

    with tempfile.TemporaryDirectory() as d:
        paths = lib.capture_to_csv_multi([MAIN_CHANNEL, SERVICE_CHANNEL], d,
                                         stimulus=stim, capture_seconds=seconds)
        main_dec = lib.decode(lib.read_transitions(paths[MAIN_CHANNEL]))
        svc_dec  = lib.decode(lib.read_transitions(paths[SERVICE_CHANNEL]))
    _wait_result(s)   # op runs for seconds (no decoder); finish before the next
    return main_dec, svc_dec


def _find(dec, expected):
    """(count, max_preamble) for packets whose bytes == expected."""
    m = [pre for pre, data in dec["packets"] if list(data) == expected]
    return len(m), (max(m) if m else 0)


def _hx(bs):
    return " ".join(f"{b:02X}" for b in bs) if bs else "(none)"


# ----------------------------------------------------------------------------
# REUSABLE parallel-track timing block -- run on BOTH channels for EVERY SVC op.
# ----------------------------------------------------------------------------
# @compliance DCC-S9.2.3-CS-025
def check_track_timing(rep, dec, track):
    """Verify one decoded channel's half-bit periods stay within S-9.1 Tbl 2.1
    tolerance and it is still emitting packets -- i.e. service-mode ISR work did
    not stretch or drop a bit on this track. Call for the main and the service
    channel after every service-mode operation."""
    halves = dec["bit_halves"]
    ones  = [h for (a, b, bit) in halves if bit == "1" for h in (a, b)]
    zeros = [h for (a, b, bit) in halves if bit == "0" for h in (a, b)]
    ztot  = [a + b for (a, b, bit) in halves if bit == "0"]
    clause = "S-9.1 Tbl2.1 (parallel-track during SVC)"

    if ones:
        ok = all(ONE_HALF_MIN_US <= h <= ONE_HALF_MAX_US for h in ones)
        rep.check(clause, f"{track}: ONE half-bit 55-61us", ok,
                  lib.sigma_margin_detail(ones, ONE_HALF_MIN_US, ONE_HALF_MAX_US) + " us")
    else:
        rep.check(clause, f"{track}: ONE half-bit present", False,
                  "no one-bits decoded on this channel")

    if zeros:
        ok = all(h >= ZERO_HALF_MIN_US for h in zeros)
        rep.check(clause, f"{track}: ZERO half-bit >= 95us", ok,
                  lib.sigma_margin_detail(zeros, ZERO_HALF_MIN_US, None) + " us")
        ok2 = all(t <= ZERO_BIT_TOTAL_MAX_US for t in ztot)
        rep.check(clause, f"{track}: ZERO total <= 12000us", ok2,
                  lib.sigma_margin_detail(ztot, None, ZERO_BIT_TOTAL_MAX_US) + " us")

    rep.check(clause, f"{track}: still emitting packets", len(dec["packets"]) > 0,
              f"{len(dec['packets'])} packets on {track} during the SVC op")


def _has_packet(dec, pred):
    """True if any decoded packet satisfies pred(list_of_bytes)."""
    return any(pred(list(d)) for _, d in dec["packets"])


# @compliance DCC-S9.2.3-CS-007
def _count_resets(dec):
    return sum(1 for _, d in dec["packets"] if list(d) == RESET)


def _check_timing_both(rep, main_dec, svc_dec, label):
    """The reusable parallel-track timing block, on BOTH channels, every op."""
    check_track_timing(rep, main_dec, f"main(PB1)/{label}")
    check_track_timing(rep, svc_dec,  f"svc(PB4)/{label}")
    time.sleep(0.3)   # brief gap; _svc_op_dual already waited for completion


# @compliance DCC-S9.2.3-CS-010
def _check_command(rep, clause, dec, expected, label, min_repeat=1):
    """Verify a service-mode command appears on the service track with the exact
    spec bytes and a >=20-bit long preamble; optionally the command-repeat count."""
    cnt, pre = _find(dec, expected)
    rep.check(clause, f"{label} on wire [{_hx(expected)}]", cnt >= 1,
              f"seen {cnt}x (max preamble {pre} bits)")
    if cnt >= 1:
        rep.check(clause, f"{label}: long preamble >= 20 bits", pre >= 20,
                  f"max preamble {pre} bits")
    if min_repeat > 1:
        rep.check(clause, f"{label}: command repeated >= {min_repeat}x", cnt >= min_repeat,
                  f"command seen {cnt}x (spec: {min_repeat}+)")


# ----------------------------------------------------------------------------
# Per-mode test functions (S-9.2.3 sec E). Each runs its ops through the dual
# capture, checks the exact command encoding + sequence structure on the service
# track, and runs the parallel-track timing block on BOTH channels.
# ----------------------------------------------------------------------------
def test_direct(rep, s):
    """Direct mode (lines 93-141): all three instruction types -- write byte
    (CC=11), verify byte (CC=01), bit manipulation (CC=10, K write/verify) --
    plus 10-bit CV addressing (CV#1 .. CV1023)."""
    D = SPEC_DOC + " Direct"

    # write_cv emits the write byte (CC=11) then a verify-byte confirm (CC=01),
    # so this one op exercises two of the three required instruction types.
    main_dec, svc_dec = _svc_op_dual(s, "SVC DIRECT WRITE 3 5", seconds=1.0)
    # @compliance DCC-S9.2.3-CS-014
    _check_command(rep, D, svc_dec, direct_write_byte(3, 5), "write-byte CV3=5 (CC=11)", min_repeat=5)
    _check_command(rep, D, svc_dec, direct_verify_byte(3, 5), "verify-byte confirm CV3=5 (CC=01)")
    rep.check(D, "write-byte: >= 3 reset packets (pre-command)", _count_resets(svc_dec) >= 3,
              f"{_count_resets(svc_dec)} reset packets in the burst")
    _check_timing_both(rep, main_dec, svc_dec, "DIRECT WRITE 3 5")

    # 10-bit CV addressing: CV#1 (AA=00, addr byte 0x00) and CV1023 (AA=11).
    main_dec, svc_dec = _svc_op_dual(s, "SVC DIRECT WRITE 1 9", seconds=1.0)
    _check_command(rep, D, svc_dec, direct_write_byte(1, 9), "write-byte CV#1 (addr 00 00000000)", min_repeat=5)
    _check_timing_both(rep, main_dec, svc_dec, "DIRECT WRITE 1 9")

    main_dec, svc_dec = _svc_op_dual(s, "SVC DIRECT WRITE 1023 200", seconds=1.0)
    # @compliance DCC-S9.2.3-CS-015
    _check_command(rep, D, svc_dec, direct_write_byte(1023, 200), "write-byte CV1023 (AA=11)", min_repeat=5)
    _check_timing_both(rep, main_dec, svc_dec, "DIRECT WRITE 1023 200")

    # Bit manipulation: write bit (K=1) and verify bit (K=0).
    main_dec, svc_dec = _svc_op_dual(s, "SVC DIRECT BITW 5 3 1")
    _check_command(rep, D, svc_dec, direct_write_bit(5, 3, 1), "write-bit CV5 b3=1 (CC=10 K=1)", min_repeat=5)
    _check_timing_both(rep, main_dec, svc_dec, "DIRECT BITW 5 3 1")

    main_dec, svc_dec = _svc_op_dual(s, "SVC DIRECT BITR 3 0")
    _check_command(rep, D, svc_dec, direct_verify_bit(3, 0, 1), "verify-bit CV3 b0 (CC=10 K=0)")
    _check_timing_both(rep, main_dec, svc_dec, "DIRECT BITR 3 0")


def test_register(rep, s):
    """Physical register mode (lines 169-201): page-preset (page reg -> 1) then
    the 0111CRRR register command, registers 1-8; plus decoder factory reset."""
    R = SPEC_DOC + " Register"

    main_dec, svc_dec = _svc_op_dual(s, "SVC REG WRITE 1 5 MOBILE", seconds=1.5)
    rep.check(R, "write reg1: page-preset present [%s]" % _hx(page_preset(1)),
              _has_packet(svc_dec, lambda d: d == page_preset(1)),
              "page-register write to page 1 precedes the command")
    # @compliance DCC-S9.2.3-CS-016
    _check_command(rep, R, svc_dec, register_cmd(1, 5, write=True), "write reg1=5", min_repeat=5)
    rep.check(R, "write reg1: >= 3 reset packets", _count_resets(svc_dec) >= 3,
              f"{_count_resets(svc_dec)} reset packets")
    _check_timing_both(rep, main_dec, svc_dec, "REG WRITE 1 5")

    main_dec, svc_dec = _svc_op_dual(s, "SVC REG WRITE 8 100 MOBILE", seconds=1.5)
    _check_command(rep, R, svc_dec, register_cmd(8, 100, write=True), "write reg8=100 (CV8)", min_repeat=5)
    _check_timing_both(rep, main_dec, svc_dec, "REG WRITE 8 100")

    # Decoder factory reset = write register 8 value 8 -> 7F 08 77 (line 285).
    main_dec, svc_dec = _svc_op_dual(s, "SVC REG RESET", seconds=1.5)
    # @compliance DCC-S9.2.3-CS-020
    _check_command(rep, R, svc_dec, register_cmd(8, 8, write=True), "factory reset", min_repeat=5)
    _check_timing_both(rep, main_dec, svc_dec, "REG RESET")

    # Register VERIFY: command encoding (0111 0RRR) + the 7+ repeat count (line 172),
    # now exercised directly via SVC REG VERIFY (one bounded verify, no decoder ACK).
    main_dec, svc_dec = _svc_op_dual(s, "SVC REG VERIFY 1 0 MOBILE", seconds=1.5)
    rep.check(R, "verify reg1: page-preset present [%s]" % _hx(page_preset(1)),
              _has_packet(svc_dec, lambda d: d == page_preset(1)),
              "page-register write to page 1 precedes the verify")
    # @compliance DCC-S9.2.3-CS-011
    _check_command(rep, R, svc_dec, register_cmd(1, 0, write=False), "verify reg1=0", min_repeat=7)
    _check_timing_both(rep, main_dec, svc_dec, "REG VERIFY 1 0")


def test_address(rep, s):
    """Address-only mode (lines 142-168): page-preset then the CV#1 command
    (register-1 form, 0111C000), 0DDDDDDD 7-bit data byte."""
    A = SPEC_DOC + " Address"

    main_dec, svc_dec = _svc_op_dual(s, "SVC ADDR WRITE 5", seconds=1.5)
    rep.check(A, "address write: page-preset present [%s]" % _hx(page_preset(1)),
              _has_packet(svc_dec, lambda d: d == page_preset(1)),
              "page-register write to page 1 precedes the command")
    # @compliance DCC-S9.2.3-CS-026
    _check_command(rep, A, svc_dec, address_cmd(5, write=True), "write CV#1 addr=5", min_repeat=5)
    rep.check(A, "address write: >= 3 reset packets", _count_resets(svc_dec) >= 3,
              f"{_count_resets(svc_dec)} reset packets")
    _check_timing_both(rep, main_dec, svc_dec, "ADDR WRITE 5")

    main_dec, svc_dec = _svc_op_dual(s, "SVC ADDR WRITE 99", seconds=1.5)
    _check_command(rep, A, svc_dec, address_cmd(99, write=True), "write CV#1 addr=99", min_repeat=5)
    _check_timing_both(rep, main_dec, svc_dec, "ADDR WRITE 99")

    # Address VERIFY: command encoding (0111 0000) + 5+ repeat count, via SVC ADDR
    # VERIFY (one bounded verify, no decoder ACK).
    main_dec, svc_dec = _svc_op_dual(s, "SVC ADDR VERIFY 5", seconds=1.5)
    rep.check(A, "address verify: page-preset present [%s]" % _hx(page_preset(1)),
              _has_packet(svc_dec, lambda d: d == page_preset(1)),
              "page-register write to page 1 precedes the verify")
    _check_command(rep, A, svc_dec, address_cmd(5, write=False), "verify CV#1 addr=5", min_repeat=5)
    _check_timing_both(rep, main_dec, svc_dec, "ADDR VERIFY 5")


def test_paged(rep, s):
    """Paged mode (lines 202-277): page-register write then the data-register
    access. With paged proceeding unconditionally, the data command is now on the
    wire even with no decoder."""
    P = SPEC_DOC + " Paged"

    # CV8 -> page 2, data register 3 (RRR=011 -> 0x7B); value 100 = 0x64.
    main_dec, svc_dec = _svc_op_dual(s, "SVC PAGED WRITE 8 100", seconds=2.0)
    rep.check(P, "write CV8: page-register write [%s]" % _hx(page_preset(paged_page(8))),
              _has_packet(svc_dec, lambda d: d == page_preset(paged_page(8))),
              "page set to %d ((8-1)//4 + 1) precedes the data access" % paged_page(8))
    _check_command(rep, P, svc_dec, paged_data_cmd(8, 100, write=True), "data-register write CV8=100", min_repeat=5)
    _check_timing_both(rep, main_dec, svc_dec, "PAGED WRITE 8 100")

    # CV1 -> page 1, data register 1 (RRR=000 -> 0x78).
    main_dec, svc_dec = _svc_op_dual(s, "SVC PAGED WRITE 1 42", seconds=2.0)
    rep.check(P, "write CV1: page-register write [%s]" % _hx(page_preset(paged_page(1))),
              _has_packet(svc_dec, lambda d: d == page_preset(paged_page(1))),
              "page set to 1 precedes the data access")
    # @compliance DCC-S9.2.3-CS-018
    _check_command(rep, P, svc_dec, paged_data_cmd(1, 42, write=True), "data-register write CV1=42", min_repeat=5)
    _check_timing_both(rep, main_dec, svc_dec, "PAGED WRITE 1 42")


# @compliance DCC-S9.2.3-CS-024
def test_readback(rep, s):
    """Read-back and write+verify VALUE correctness via the mock decoder (Direct
    mode, line 116). The mock decoder (SVC MOCKCV) holds a CV value and ACKs only
    matching verifies, so the whole read/write path runs end-to-end through the
    real ACK detection on hardware -- both the success and the failure case."""
    RB = SPEC_DOC + " Direct read/write-back"

    # --- success: mock decoder holds CV8 = 90 (0x5A) ---
    # SVC DIRECT WRITE / MOCKCV values are DECIMAL (firmware parses with atoi).
    _send_ok(s, "SVC MOCKCV 8 90")
    r = _svc_result(s, "SVC DIRECT READ 8")
    rep.check(RB, "read-back CV8 (mock=90/0x5A) -> value 0x5A", "(0x5A)" in r, f"result: {r}")

    # write a new value; the mock accepts the write, so the verify-byte confirm
    # matches and the op succeeds.
    r = _svc_result(s, "SVC DIRECT WRITE 8 51")
    rep.check(RB, "write CV8=51 (mock present) -> SUCCESS", "SUCCESS" in r, f"result: {r}")
    r = _svc_result(s, "SVC DIRECT READ 8")
    rep.check(RB, "read-back after write -> value 51 (0x33)", "(0x33)" in r, f"result: {r}")

    # --- failure: mock OFF -> no decoder ACKs ---
    _send_ok(s, "SVC MOCKCV OFF")
    r = _svc_result(s, "SVC DIRECT WRITE 8 51")
    rep.check(RB, "write with mock OFF -> VERIFY FAIL", "VERIFY FAIL" in r, f"result: {r}")
    r = _svc_result(s, "SVC DIRECT READ 8")
    rep.check(RB, "read with mock OFF -> value 0x00", "(0x00)" in r, f"result: {r}")


def ack_width_tests(rep, s):
    """ACK pulse-width detection (6 ms +/- 1 ms, lines 47-49) via the mock-ACK
    loopback. Window in 58us samples is [85, 120]; each width lands on an exact
    sample count. Checks both the UART verdict and the D4-measured pulse."""
    # @compliance DCC-S9.2.3-CS-001, DCC-S9.2.3-CS-002, DCC-S9.2.3-CS-003, DCC-S9.2.3-CS-004
    ack_vectors = [
        (4872, False, "MIN-1 (84 samples / 4872us)"),
        (4930, True,  "MIN (85 samples / 4930us)"),
        (6000, True,  "mid (103 samples / 6000us)"),
        (6960, True,  "MAX (120 samples / 6960us)"),
        (7018, False, "MAX+1 (121 samples / 7018us, overrun)"),
    ]
    for width_us, expect_ack, label in ack_vectors:
        s.reset_input_buffer()

        def stim(c=width_us):
            s.write((f"SVC MOCKACK {c}\r").encode())

        with tempfile.TemporaryDirectory() as d:
            paths = lib.capture_to_csv_multi([ACK_CHANNEL], d, stimulus=stim,
                                             capture_seconds=CAPTURE_SECONDS)
            ack_rows = lib.read_transitions(paths[ACK_CHANNEL])

        resp = _wait_mockack(s)
        got_ack = "ACK DETECTED" in resp
        last = resp.strip().splitlines()[-1] if resp.strip() else "(timeout)"
        measured = _measure_high_us(ack_rows)
        expected_us = (width_us // 58) * 58

        rep.check(SPEC_DOC + " §D",
                  f"ACK width {label} -> verdict {'ACK' if expect_ack else 'NO ACK'}",
                  got_ack == expect_ack, f"reported: {last}")
        rep.check(SPEC_DOC + " §D",
                  f"ACK width {label}: D4 pulse ~{expected_us}us",
                  measured > 0 and abs(measured - expected_us) <= 90,
                  f"measured {measured:.0f}us on D4 (asked {width_us}us, expect ~{expected_us}us)")
        time.sleep(0.2)

    # --- ACK scan-window blanking boundary (S-9.2.3 line 55) ----------------
    # The SAME valid-width pulse: fired in-window -> detected; fired EARLY (on the
    # first, blanked command packet) -> masked. Proves the blanking on the wire.
    s.reset_input_buffer()
    s.write(b"SVC MOCKACK 6000\r")
    in_window = _wait_mockack(s)
    rep.check(SPEC_DOC + " line 55", "valid ACK in-window -> ACK DETECTED",
              "ACK DETECTED" in in_window, f"reported: {in_window.strip().splitlines()[-1] if in_window.strip() else '(timeout)'}")
    time.sleep(0.2)

    s.reset_input_buffer()
    # @compliance DCC-S9.2.3-CS-006
    s.write(b"SVC MOCKACK 6000 EARLY\r")
    early = _wait_mockack(s)
    rep.check(SPEC_DOC + " line 55", "same ACK on packet 1 (blanked) -> NO ACK",
              "NO ACK" in early, f"reported: {early.strip().splitlines()[-1] if early.strip() else '(timeout)'}")
    time.sleep(0.2)


# ----------------------------------------------------------------------------
# Suite
# ----------------------------------------------------------------------------
def run():
    import serial

    rep = lib.Report(SPEC_DOC, SPEC_TITLE, SOURCE_PDF, ASPECT)

    port = lib.find_dut_port()
    if not port:
        raise RuntimeError("DUT not found among /dev/cu.usbmodem* "
                           "(is the firmware running?)")
    s = serial.Serial(port, lib.SERIAL_BAUD, timeout=0.3)
    time.sleep(0.2)
    s.reset_input_buffer()

    def send(c):
        s.write((c + "\r").encode())

    try:
        send("POWER ON"); time.sleep(0.5)    # main track streams idle on ch0
        send("SVC ENTER"); time.sleep(0.5)   # service track active on ch3

        # Each mode: exact command encoding + sequence structure (service track)
        # and the parallel-track timing block on BOTH channels, per op.
        test_direct(rep, s)
        test_register(rep, s)
        test_address(rep, s)
        test_paged(rep, s)

        # Read-back / write+verify value correctness via the mock decoder.
        test_readback(rep, s)

        # ACK pulse-width detection via the mock-ACK loopback (PB24 -> PB9, D4).
        ack_width_tests(rep, s)

        send("SVC EXIT"); time.sleep(0.3)
        send("POWER OFF"); time.sleep(0.3)

    finally:
        s.close()

    return rep.finish()


def main():
    print("\n#### DCC COMPLIANCE TEST ####")
    print(f"Spec under test : NMRA {SPEC_DOC}  Service Mode")
    print(f"Source PDF      : {SOURCE_PDF}")
    print(f"Aspect          : {ASPECT}")
    try:
        rep = run()
    except ImportError as e:
        print(f"\nMissing dependency: {e}\nRun: pip install logic2-automation pyserial\n")
        return 2
    except Exception as e:
        print(f"\nERROR: {e}\nCheck: Logic 2 + Automation API (port {lib.AUTOMATION_PORT}), "
              f"MAIN track (PB1) on ch{MAIN_CHANNEL}, SERVICE track (PB4) on "
              f"ch{SERVICE_CHANNEL}, mock-ACK pin on ch{ACK_CHANNEL}, firmware running.\n")
        return 2
    path = lib.write_html([rep.as_dict()], lib.report_path("s9_2_3"),
                          title=f"NMRA {SPEC_DOC} Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
