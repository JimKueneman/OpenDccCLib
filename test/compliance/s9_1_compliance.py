#!/usr/bin/env python3
"""
NMRA S-9.1 -- Electrical / Timing compliance (command-station transmit).

Hardware-in-the-loop: powers the track on over UART, captures the DCC signal on
a Saleae (ch0 = DCC out / PB1), decodes it, and checks on-the-wire timing and
framing against the S-9.1 standard. Shared plumbing lives in compliance_lib.py.

Run standalone:  .venv/bin/python s9_1_compliance.py   (writes an HTML report)
Or via:          .venv/bin/python run_all.py            (combined report)
"""

import sys
import compliance_lib as lib

# --- spec metadata (shown in console banner + HTML report) ---
SPEC_DOC   = "S-9.1"
SPEC_TITLE = "Electrical / Timing (command-station transmit)"
SOURCE_PDF = ("documentation/specs/"
              "s-9.1_electrical_standards_for_digital_command_control.pdf")
ASPECT     = "command-station transmit timing & framing (DCC out, ch0)"

# --- S-9.1 limits (each tagged to its clause; see DCC_Spec_Reference.md) ---
ONE_HALF_MIN_US       = 55.0     # Tbl 2.1: one-bit half 58 us nominal (55-61)
ONE_HALF_MAX_US       = 61.0
ZERO_HALF_MIN_US      = 95.0     # Tbl 2.1: zero-bit half >= 95 us (CS transmit)
ZERO_BIT_TOTAL_MAX_US = 12000.0  # total zero-bit duration must not exceed 12000 us
INTRA_BIT_SYMMETRY_US = 3.0      # the two halves of a bit must be ~equal
PREAMBLE_MIN_BITS     = 14       # S-9.2: "CS must send a minimum of 14 full preamble bits"


def checks(rep, decoded):
    """Run all S-9.1 checks against an already-decoded capture."""
    halves = decoded["bit_halves"]
    ones = [w for (a, b, c) in halves if c == "1" for w in (a, b)]
    zeros = [w for (a, b, c) in halves if c == "0" for w in (a, b)]
    packets = decoded["packets"]
    interior = decoded["interior_glitches"]

    # 0. interior glitches (boundary runts already trimmed; these are real)
    rep.check("HIL", f"no interior glitch pulses (< {lib.GLITCH_FLOOR_US:.0f} us)",
              len(interior) == 0,
              f"{decoded['trimmed_runts']} boundary runt(s) trimmed; "
              f"{len(interior)} interior"
              + (f" -> {[round(w, 2) for w in interior]}" if interior else ""))

    # 1. ONE half-bit timing
    if ones:
        ok = all(ONE_HALF_MIN_US <= w <= ONE_HALF_MAX_US for w in ones)
        rep.check("S-9.1 Tbl2.1", "ONE half-bit 55-61 us (nom 58)", ok,
                  lib.sigma_margin_detail(ones, ONE_HALF_MIN_US, ONE_HALF_MAX_US) + " us")
    else:
        rep.check("S-9.1 Tbl2.1", "ONE half-bit", False, "no one-bits decoded")

    # 2. ZERO half-bit timing + total duration
    if zeros:
        ok = all(w >= ZERO_HALF_MIN_US for w in zeros)
        rep.check("S-9.1 Tbl2.1", "ZERO half-bit >= 95 us", ok,
                  lib.sigma_margin_detail(zeros, ZERO_HALF_MIN_US, None) + " us")
        zero_totals = [a + b for (a, b, c) in halves if c == "0"]
        total_ok = all(t <= ZERO_BIT_TOTAL_MAX_US for t in zero_totals)
        rep.check("S-9.1", "ZERO total bit <= 12000 us", total_ok,
                  lib.sigma_margin_detail(zero_totals, None, ZERO_BIT_TOTAL_MAX_US) + " us")
    else:
        rep.check("S-9.1 Tbl2.1", "ZERO half-bit", False, "no zero-bits decoded")

    # 3. intra-bit symmetry
    if halves:
        deltas = [abs(a - b) for (a, b, c) in halves]
        rep.check("S-9.1", f"intra-bit symmetry <= {INTRA_BIT_SYMMETRY_US} us",
                  max(deltas) <= INTRA_BIT_SYMMETRY_US,
                  lib.sigma_margin_detail(deltas, None, INTRA_BIT_SYMMETRY_US) + " us")

    # 4. preamble length -- exclude inflated first packet; strip merged end bit
    interior_pk = packets[1:]
    if interior_pk:
        preambles = [run - 1 for (run, _) in interior_pk]
        min_pre = min(preambles)
        rep.check("S-9.2", f"preamble >= {PREAMBLE_MIN_BITS} ones (CS transmit)",
                  min_pre >= PREAMBLE_MIN_BITS,
                  f"min preamble = {min_pre} over {len(interior_pk)} packets "
                  f"(first excluded: idle-inflated; end bit stripped)")
    else:
        rep.check("S-9.2", "preamble", False, "no interior packets to measure")

    # 5/6. framing + XOR (a decoded packet implies start/sep/end framing held)
    if packets:
        def _xor(data):
            x = 0
            for b in data:
                x ^= b
            return x
        bad = sum(1 for _, data in packets if _xor(data) != 0)
        rep.check("S-9.2", "packet framing + XOR error byte", bad == 0,
                  f"{len(packets)} packets, {bad} XOR failures")
    else:
        rep.check("S-9.2", "packet framing + XOR", False, "no packets decoded")

    # signal levels -- not measurable on a digital LA
    rep.na("S-9.1 §2", "signal voltage levels (>=8.5 V out)",
           "requires scope/meter or analog capture -- not coverable by this rig")


def run():
    """Drive DCC, capture, decode, run S-9.1 checks. Returns the finished Report."""
    if lib.DRIVE_UART:
        lib.start_dcc()
    decoded, n_tx = lib.capture_and_decode()
    print(f"[decode] {n_tx} transitions, {len(decoded['bits'])} bits, "
          f"{len(decoded['packets'])} packets")
    rep = lib.Report(SPEC_DOC, SPEC_TITLE, SOURCE_PDF, ASPECT)
    checks(rep, decoded)
    return rep.finish()


def main():
    print("\n#### DCC COMPLIANCE TEST ####")
    print(f"Spec under test : NMRA {SPEC_DOC}  Electrical Standards for DCC")
    print(f"Source PDF      : {SOURCE_PDF}")
    print(f"Aspect          : {ASPECT}")
    try:
        rep = run()
    except ImportError as e:
        print(f"\nMissing dependency: {e}\n"
              f"Run: pip install logic2-automation pyserial\n")
        return 2
    except Exception as e:
        print(f"\nERROR: {e}\n"
              f"Check: Logic 2 running, Automation API enabled (port "
              f"{lib.AUTOMATION_PORT}), device connected, firmware running.\n")
        return 2
    path = lib.write_html([rep.as_dict()], lib.report_path("s9_1"),
                          title=f"NMRA {SPEC_DOC} Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
