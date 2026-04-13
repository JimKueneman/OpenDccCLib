"""DCC service track CS-only test — fire and forget, no decoder connected.

Enters service mode (which drives track select HIGH and outputs DCC on PB4),
sends every service mode command, and checks that the CS accepts each one.
No decoder is connected so there is no ACK — we only verify the CS accepts
the command (OK response).  SVC RESULT lines are ignored.

Usage:
    python3 dcc_service_track_cs_only_test.py --port /dev/cu.usbmodemMG3500011
"""

import serial
import time
import argparse
import sys


def send(cs, cmd, timeout=2.0):
    """Send command to CS, return first OK/ERR response line."""
    cs.reset_input_buffer()
    cs.write((cmd + "\n").encode())
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = cs.readline().decode(errors="replace").strip()
        if line.startswith("OK") or line.startswith("ERR"):
            return line
    return "<timeout>"


def run_test(cs, name, cmd, delay=0.3):
    """Send one command, check for OK, ignore SVC RESULT."""
    resp = send(cs, cmd)
    ok = resp.startswith("OK")
    status = "PASS" if ok else "FAIL"
    print(f"  [{status}] {name:50s} -> {resp}")
    time.sleep(delay)
    return ok


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True, help="CS serial port")
    parser.add_argument("--baud", type=int, default=230400)
    parser.add_argument("--delay", type=float, default=0.3,
                        help="Delay between commands (seconds)")
    args = parser.parse_args()

    cs = serial.Serial(args.port, args.baud, timeout=2)
    time.sleep(0.5)
    cs.reset_input_buffer()

    passed = 0
    failed = 0
    d = args.delay

    def test(name, cmd):
        nonlocal passed, failed
        if run_test(cs, name, cmd, d):
            passed += 1
        else:
            failed += 1

    # -----------------------------------------------------------------
    print("=== Setup — Enter Service Mode ===")
    resp = send(cs, "SVC ENTER")
    print(f"  SVC ENTER: {resp}")
    if not resp.startswith("OK"):
        print("  FATAL: could not enter service mode")
        cs.close()
        sys.exit(1)
    time.sleep(0.5)

    # -----------------------------------------------------------------
    print("\n=== Direct Mode — Byte Write ===")
    test("direct_write_cv5_42",     "SVC DIRECT WRITE 5 42")
    test("direct_write_cv1_3",      "SVC DIRECT WRITE 1 3")
    test("direct_write_cv29_6",     "SVC DIRECT WRITE 29 6")
    test("direct_write_cv6_20",     "SVC DIRECT WRITE 6 20")
    test("direct_write_cv7_30",     "SVC DIRECT WRITE 7 30")

    # -----------------------------------------------------------------
    print("\n=== Direct Mode — Byte Verify ===")
    test("direct_verify_cv5_42",    "SVC DIRECT VERIFY 5 42")
    test("direct_verify_cv29_6",    "SVC DIRECT VERIFY 29 6")
    test("direct_verify_cv1_99",    "SVC DIRECT VERIFY 1 99")

    # -----------------------------------------------------------------
    print("\n=== Direct Mode — Bit Write ===")
    test("direct_bitw_cv5_b3_1",    "SVC DIRECT BITW 5 3 1")
    test("direct_bitw_cv5_b0_0",    "SVC DIRECT BITW 5 0 0")
    test("direct_bitw_cv29_b2_1",   "SVC DIRECT BITW 29 2 1")

    # -----------------------------------------------------------------
    print("\n=== Direct Mode — Bit Verify ===")
    test("direct_bitv_cv5_b3_1",    "SVC DIRECT BITV 5 3 1")
    test("direct_bitv_cv5_b0_0",    "SVC DIRECT BITV 5 0 0")

    # -----------------------------------------------------------------
    print("\n=== Direct Mode — High CV Numbers ===")
    test("direct_write_cv256_42",   "SVC DIRECT WRITE 256 42")
    test("direct_verify_cv256_42",  "SVC DIRECT VERIFY 256 42")
    test("direct_write_cv1024_42",  "SVC DIRECT WRITE 1024 42")

    # -----------------------------------------------------------------
    print("\n=== Paged Mode ===")
    test("paged_write_cv5_42",      "SVC PAGED WRITE 5 42")
    test("paged_verify_cv5_42",     "SVC PAGED VERIFY 5 42")
    test("paged_write_cv1_3",       "SVC PAGED WRITE 1 3")
    test("paged_verify_cv1_99",     "SVC PAGED VERIFY 1 99")

    # -----------------------------------------------------------------
    print("\n=== Register Mode ===")
    test("reg_write_r1_42",         "SVC REG WRITE 1 42")
    test("reg_verify_r1_42",        "SVC REG VERIFY 1 42")
    test("reg_write_r5_100",        "SVC REG WRITE 5 100")
    test("reg_verify_r5_100",       "SVC REG VERIFY 5 100")
    test("reg_write_r8_42",         "SVC REG WRITE 8 42")

    # -----------------------------------------------------------------
    print("\n=== Address-Only Mode ===")
    test("addr_write_42",           "SVC ADDR WRITE 42")
    test("addr_write_1",            "SVC ADDR WRITE 1")
    test("addr_write_127",          "SVC ADDR WRITE 127")

    # -----------------------------------------------------------------
    print("\n=== Sequential Operations ===")
    test("seq_write_cv5_10",        "SVC DIRECT WRITE 5 10")
    test("seq_write_cv6_20",        "SVC DIRECT WRITE 6 20")
    test("seq_write_cv7_30",        "SVC DIRECT WRITE 7 30")
    test("seq_verify_cv5_10",       "SVC DIRECT VERIFY 5 10")
    test("seq_verify_cv6_20",       "SVC DIRECT VERIFY 6 20")
    test("seq_verify_cv7_30",       "SVC DIRECT VERIFY 7 30")

    # -----------------------------------------------------------------
    print("\n=== Cleanup — Exit Service Mode ===")
    resp = send(cs, "SVC EXIT")
    print(f"  SVC EXIT: {resp}")

    # -----------------------------------------------------------------
    # Summary
    total = passed + failed
    print(f"\n{'='*60}")
    print(f"  {passed}/{total} passed, {failed} failed (CS accepted)")
    print(f"{'='*60}")

    cs.close()
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
