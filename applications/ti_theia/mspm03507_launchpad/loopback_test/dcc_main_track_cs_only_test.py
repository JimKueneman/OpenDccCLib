"""DCC main track CS-only test — sends all commands, decoder watched externally.

Sends every command from the main track test suite to the CS only.
The decoder port is NOT opened — watch it in your own terminal.

Usage:
    python3 dcc_main_track_cs_only_test.py --port /dev/cu.usbmodemMG3500011
"""

import serial
import time
import argparse
import sys


def send(cs, cmd):
    """Send command to CS, print response."""
    cs.reset_input_buffer()
    cs.write((cmd + "\n").encode())
    deadline = time.time() + 2.0
    while time.time() < deadline:
        line = cs.readline().decode(errors="replace").strip()
        if line.startswith("OK") or line.startswith("ERR"):
            return line
    return "<timeout>"


def run_test(cs, name, cmd, delay=0.3):
    """Send one command, print result, pause for decoder to process."""
    resp = send(cs, cmd)
    ok = "PASS" if resp.startswith("OK") else "FAIL"
    print(f"  [{ok}] {name:45s} -> {resp}")
    time.sleep(delay)
    return ok == "PASS"


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

    # Power on, refresh off
    print("=== Setup ===")
    send(cs, "REFRESH OFF")
    resp = send(cs, "POWER ON")
    print(f"  POWER ON: {resp}")
    time.sleep(0.5)

    # -----------------------------------------------------------------
    print("\n=== Speed — Short Address (128-step) ===")
    test("speed_stop",        "SPEED 3 0 FWD")
    test("speed_mid",         "SPEED 3 64 FWD")
    test("speed_max",         "SPEED 3 127 FWD")
    test("speed_reverse",     "SPEED 3 64 REV")
    test("speed_1_estop",     "SPEED 3 1 FWD")

    # -----------------------------------------------------------------
    print("\n=== Speed — 28-Step ===")
    test("speed_28_stop",     "SPEED 3 0 FWD 28")
    test("speed_28_estop",    "SPEED 3 1 FWD 28")
    test("speed_28_min",      "SPEED 3 2 FWD 28")
    test("speed_28_mid",      "SPEED 3 15 FWD 28")
    test("speed_28_max",      "SPEED 3 29 FWD 28")
    test("speed_28_reverse",  "SPEED 3 15 REV 28")

    # -----------------------------------------------------------------
    print("\n=== Speed — 14-Step ===")
    # Set CV29 for 14-step mode
    send(cs, "CV WRITE 3 29 4")
    time.sleep(0.5)
    test("speed_14_stop",     "SPEED 3 0 FWD 14")
    test("speed_14_estop",    "SPEED 3 1 FWD 14")
    test("speed_14_min",      "SPEED 3 2 FWD 14")
    test("speed_14_max",      "SPEED 3 15 FWD 14")
    test("speed_14_reverse",  "SPEED 3 8 REV 14")
    # Restore 28-step mode
    send(cs, "CV WRITE 3 29 6")
    time.sleep(0.5)

    # -----------------------------------------------------------------
    print("\n=== Speed — Long Address ===")
    test("speed_long_default",  "SPEED 200 64 FWD")
    test("speed_long_explicit", "SPEED 200L 64 FWD")

    # -----------------------------------------------------------------
    print("\n=== Speed — Forced Short ===")
    test("speed_forced_short",  "SPEED 40S 64 FWD")
    test("speed_auto_short",    "SPEED 40 64 FWD")

    # -----------------------------------------------------------------
    print("\n=== E-Stop ===")
    test("estop_addressed",   "ESTOP 3")
    test("estop_broadcast",   "ESTOP")

    # -----------------------------------------------------------------
    print("\n=== Functions — F0-F28 ===")
    test("func_fl",  "FUNC 3 0 ON")
    test("func_f1",  "FUNC 3 1 ON")
    test("func_f4",  "FUNC 3 4 ON")
    test("func_f5",  "FUNC 3 5 ON")
    test("func_f8",  "FUNC 3 8 ON")
    test("func_f9",  "FUNC 3 9 ON")
    test("func_f12", "FUNC 3 12 ON")
    test("func_f13", "FUNC 3 13 ON")
    test("func_f20", "FUNC 3 20 ON")
    test("func_f21", "FUNC 3 21 ON")
    test("func_f28", "FUNC 3 28 ON")
    test("func_f1_off", "FUNC 3 1 OFF")

    # -----------------------------------------------------------------
    print("\n=== Functions — F29-F68 ===")
    test("func_f29", "FUNC 3 29 ON")
    test("func_f36", "FUNC 3 36 ON")
    test("func_f37", "FUNC 3 37 ON")
    test("func_f44", "FUNC 3 44 ON")
    test("func_f45", "FUNC 3 45 ON")
    test("func_f52", "FUNC 3 52 ON")
    test("func_f53", "FUNC 3 53 ON")
    test("func_f60", "FUNC 3 60 ON")
    test("func_f61", "FUNC 3 61 ON")
    test("func_f68", "FUNC 3 68 ON")
    test("func_f29_off", "FUNC 3 29 OFF")

    # -----------------------------------------------------------------
    print("\n=== Accessory — Basic ===")
    test("acc_on",           "ACC 1 0 ON")
    test("acc_off",          "ACC 1 0 OFF")
    test("acc_high_address", "ACC 511 3 ON")

    # -----------------------------------------------------------------
    print("\n=== Accessory — Extended ===")
    test("acce_aspect_0",  "ACCE 1 0")
    test("acce_aspect_31", "ACCE 1 31")

    # -----------------------------------------------------------------
    print("\n=== CV Ops Mode ===")
    test("cv_write_29",   "CV WRITE 3 29 6")
    test("cv_write_1",    "CV WRITE 3 1 3")
    test("cv_verify",     "CV VERIFY 3 29 6")
    test("cv_bit_write",  "CV BIT 3 29 3 1")
    test("cv_bit_clear",  "CV BIT 3 29 0 0")

    # -----------------------------------------------------------------
    print("\n=== Consist ===")
    test("consist_set_normal",  "CONSIST 3 SET 10 NORMAL")
    test("consist_set_reverse", "CONSIST 3 SET 10 REVERSE")
    test("consist_clear",       "CONSIST 3 CLEAR")

    # -----------------------------------------------------------------
    print("\n=== Binary State — Short ===")
    test("bss_on",  "BSS 3 1 ON")
    test("bss_off", "BSS 3 1 OFF")
    test("bss_max", "BSS 3 127 ON")

    # -----------------------------------------------------------------
    print("\n=== Binary State — Long ===")
    test("bsl_on",   "BSL 3 1 ON")
    test("bsl_off",  "BSL 3 1 OFF")
    test("bsl_high", "BSL 3 32767 ON")

    # -----------------------------------------------------------------
    print("\n=== Analog Function ===")
    test("analog_volume", "ANALOG 3 1 128")
    test("analog_zero",   "ANALOG 3 1 0")
    test("analog_max",    "ANALOG 3 1 255")

    # -----------------------------------------------------------------
    print("\n=== Speed Restriction ===")
    test("restrict_on",  "RESTRICT 3 ON 50")
    test("restrict_off", "RESTRICT 3 OFF")

    # -----------------------------------------------------------------
    print("\n=== Speed Boundaries ===")
    test("speed_2_minimum", "SPEED 3 2 FWD")
    test("speed_126",       "SPEED 3 126 FWD")

    # -----------------------------------------------------------------
    print("\n=== Address Boundaries ===")
    test("short_addr_1",   "SPEED 1S 64 FWD")
    test("short_addr_127", "SPEED 127S 64 FWD")
    test("long_addr_128",  "SPEED 128 64 FWD")
    test("long_addr_9999", "SPEED 9999L 64 FWD")

    # -----------------------------------------------------------------
    print("\n=== CV Decoder Lock ===")
    test("cv_write_lock_open", "CV WRITE 3 29 6")

    # -----------------------------------------------------------------
    print("\n=== Repeat Counts ===")
    test("cv_write_repeat", "CV WRITE 3 1 42")
    test("speed_repeat",    "SPEED 3 50 FWD")

    # -----------------------------------------------------------------
    # Summary
    total = passed + failed
    print(f"\n{'='*60}")
    print(f"  {passed}/{total} passed, {failed} failed (CS accepted)")
    print(f"{'='*60}")

    send(cs, "POWER OFF")
    cs.close()

    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
