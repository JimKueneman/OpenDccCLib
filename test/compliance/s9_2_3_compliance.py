#!/usr/bin/env python3
"""
NMRA S-9.2.3 -- Service Mode compliance (command-station transmit).  [TEMPLATE]

Drives service-mode operations over UART and verifies the bytes the command
station puts ON THE SERVICE TRACK (ch3 / PB4) against an INDEPENDENT spec encoder
(below). Expected encodings are derived from S-9.2.3 §E -- NOT from the library
under test -- so a match means CS and spec agree.

Parallel-track timing: both the main track (ch0/PB1) and the service track
(ch3/PB4) run off the SAME 58us ISR. Every service-mode operation here is captured
on BOTH channels at once, and `check_track_timing()` (the reusable block) verifies
neither track's half-bit periods are stretched or dropped while service mode runs
-- the exact risk the shared-ISR design carries (see ServiceModeOperationsMatrix.md).

What this verifies NOW (wire-level, no decoder needed):
  - Direct-mode command bytes (verify/write byte, verify/write bit) match the spec
  - >= 20-bit service-mode long preamble (S-9.2.3 §D)
  - command-packet repeat count (S-9.2.3 §E)
  - reset packets present in the sequence
  - main + service track half-bit timing stays in S-9.1 Tbl 2.1 tolerance during SVC

What is PENDING (needs the mock-ACK loopback -- see HIL_SETUP.md):
  - ACK pulse-width detection (6 ms +/- 1 ms window): SUCCESS vs NO ACK at the
    MIN-1 / MIN / MAX / MAX+1 boundaries. Marked n/a until the mock-ACK GPIO is
    wired to PB12 and `SVC MOCKACK <width_us>` exists.

Bench: firmware on the LaunchPad, Logic 2 + Automation API (port 10430),
       MAIN track (PB1) on ch0 AND SERVICE track (PB4) on ch3.  See HIL_SETUP.md.

Run standalone:  .venv/bin/python s9_2_3_compliance.py
"""

import sys
import time
import tempfile

import compliance_lib as lib

SPEC_DOC   = "S-9.2.3"
SPEC_TITLE = "Service Mode (command-station transmit)"
SOURCE_PDF = "documentation/specs/S-9.2.3_2012_07.pdf"
ASPECT     = "service-mode packet encoding + parallel-track timing (ch0 main / ch3 service); ACK pending mock-ACK"

MAIN_CHANNEL    = 0              # Saleae channel on the main track (PB1)
SERVICE_CHANNEL = 3              # Saleae channel on the service track (PB4)
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


def _verify_direct(rep, s, svc_cmd, expected, label, full_byte_checks):
    """Run one Direct op: dual capture, service-track content check(s), and the
    reusable parallel-track timing block on BOTH channels."""
    main_dec, svc_dec = _svc_op_dual(s, svc_cmd)

    cnt, pre = _find(svc_dec, expected)
    rep.check(SPEC_DOC + " §E", f"{label} on wire", cnt >= 1,
              f"expected [{_hx(expected)}] seen {cnt}x (max preamble {pre} bits)")
    if full_byte_checks:
        rep.check(SPEC_DOC + " §E", f"{label} command repeated >= 5x", cnt >= 5,
                  f"command transmitted {cnt}x (spec: 5+)")
        rep.check(SPEC_DOC + " §D", f"{label} long preamble >= 20 bits", pre >= 20,
                  f"max preamble = {pre} bits")
        rep.check(SPEC_DOC + " §E", f"{label} reset packets present",
                  any(list(d) == RESET for _, d in svc_dec["packets"]),
                  "00 00 00 reset packets seen in the burst")

    # --- the reusable parallel-track timing block, BOTH channels, EVERY op ---
    check_track_timing(rep, main_dec, f"main(PB1)/{svc_cmd}")
    check_track_timing(rep, svc_dec,  f"svc(PB4)/{svc_cmd}")

    time.sleep(0.3)   # brief gap; _svc_op_dual already waited for completion


def _verify_mode(rep, s, svc_cmd, label, expected=None, page_preset_expected=False,
                 seconds=1.0):
    """Run a non-Direct mode op: dual capture, a service-track content check
    (precise command bytes when `expected` is given; page-preset presence when
    `page_preset_expected`), >=20-bit preamble, and the reusable parallel-track
    timing block on BOTH channels.

    Register-1 and Address-only emit `0111C000` directly (no page-preset, verified
    on the wire); Paged emits a page-preset (CV -> page) whose data-register
    command lands after a long phase a short window can't reliably catch, so for
    Paged we assert the page-preset instead."""
    main_dec, svc_dec = _svc_op_dual(s, svc_cmd, seconds=seconds)
    pre = max((p for p, _ in svc_dec["packets"]), default=0)

    if expected is not None:
        cnt, epre = _find(svc_dec, expected)
        rep.check(SPEC_DOC + " §E", f"{label}: command on wire", cnt >= 1,
                  f"expected [{_hx(expected)}] seen {cnt}x (max preamble {epre} bits)")
    if page_preset_expected:
        rep.check(SPEC_DOC + " §E", f"{label}: page-preset on wire",
                  any(_is_page_preset(d) for _, d in svc_dec["packets"]),
                  "page-register write (byte1=0x7D) present in the burst")

    rep.check(SPEC_DOC + " §D", f"{label}: service-mode long preamble >= 20 bits",
              pre >= 20, f"max preamble = {pre} bits")

    # --- the reusable parallel-track timing block, BOTH channels, EVERY op ---
    check_track_timing(rep, main_dec, f"main(PB1)/{svc_cmd}")
    check_track_timing(rep, svc_dec,  f"svc(PB4)/{svc_cmd}")

    time.sleep(0.3)   # brief gap; _svc_op_dual already waited for completion


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

        # Each op: spec-byte content check (service track) + parallel-track
        # timing on BOTH channels (the reusable block).
        _verify_direct(rep, s, "SVC DIRECT WRITE 3 5",
                       direct_write_byte(3, 5),
                       "Direct write-byte CV3=5", full_byte_checks=True)

        _verify_direct(rep, s, "SVC DIRECT READ 3",
                       direct_verify_bit(3, 0, 1),
                       "Direct verify-bit CV3 b0=1", full_byte_checks=False)

        # Register / Address: precise command bytes (validated on the wire).
        # Paged: page-preset (its data-register command lands after a long phase).
        # All three also run the SAME parallel-track timing block on both channels.
        _verify_mode(rep, s, "SVC REG WRITE 1 5 MOBILE", "Register write CV1=5",
                     expected=register_cmd(1, 5, write=True))
        _verify_mode(rep, s, "SVC ADDR WRITE 5", "Address write CV1=5",
                     expected=address_cmd(5, write=True))
        _verify_mode(rep, s, "SVC PAGED WRITE 8 100", "Paged write CV8=100",
                     page_preset_expected=True)

        send("SVC EXIT"); time.sleep(0.3)
        send("POWER OFF"); time.sleep(0.3)

        # --- ACK detection (6 ms +/- 1 ms window) -- PENDING mock-ACK ------
        # Needs the mock-ACK GPIO jumpered to PB12 and `SVC MOCKACK <width_us>`.
        # Boundary vectors (see ServiceModeOperationsMatrix.md):
        #   ~4800us -> NO ACK ; 5000/6000/7000us -> SUCCESS ; ~7200us -> NO ACK.
        rep.na(SPEC_DOC + " §D", "ACK width MIN-1 (~4800us) -> NO ACK",
               "pending mock-ACK loopback (HIL_SETUP.md)")
        rep.na(SPEC_DOC + " §D", "ACK width MIN..MAX (5000-7000us) -> SUCCESS",
               "pending mock-ACK loopback (HIL_SETUP.md)")
        rep.na(SPEC_DOC + " §D", "ACK width MAX+1 (~7200us) -> NO ACK (overrun)",
               "pending mock-ACK loopback (HIL_SETUP.md)")

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
              f"MAIN track (PB1) on ch{MAIN_CHANNEL} AND SERVICE track (PB4) on "
              f"ch{SERVICE_CHANNEL}, firmware running.\n")
        return 2
    path = lib.write_html([rep.as_dict()], lib.report_path("s9_2_3"),
                          title=f"NMRA {SPEC_DOC} Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
