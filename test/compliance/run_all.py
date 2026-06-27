#!/usr/bin/env python3
"""
Run every DCC spec compliance suite and write ONE combined HTML report.

Each suite module exposes run() -> Report (see s9_1_compliance.py). Add new
spec modules to SUITES as they are written; they run in order, sharing the same
bench config in compliance_lib.py.

Run:  .venv/bin/python run_all.py
Exit: 0 = all specs pass, 1 = at least one fail, 2 = setup/hardware error.
"""

import sys
import compliance_lib as lib

import s9_1_compliance
import s9_2_compliance
import s9_2_1_compliance
import s9_2_3_compliance
import s9_3_2_compliance

SUITES = [
    s9_1_compliance,
    s9_2_compliance,
    s9_2_1_compliance,
    s9_2_3_compliance,
    s9_3_2_compliance,
]


def main():
    print("\n#### DCC FULL COMPLIANCE RUN ####")
    print(f"Suites: {', '.join(m.SPEC_DOC for m in SUITES)}")
    try:
        results = [mod.run().as_dict() for mod in SUITES]
    except ImportError as e:
        print(f"\nMissing dependency: {e}\n"
              f"Run: pip install logic2-automation pyserial\n")
        return 2
    except Exception as e:
        print(f"\nERROR: {e}\n"
              f"Check: Logic 2 running, Automation API enabled (port "
              f"{lib.AUTOMATION_PORT}), device connected, firmware running.\n")
        return 2

    path = lib.write_html(results, lib.report_path("dcc_compliance_all"),
                          title="DCC Compliance Report — all specs")
    overall = "FAIL" if any(r["result"] == "FAIL" for r in results) else "PASS"
    tp = sum(r["n_pass"] for r in results)
    tf = sum(r["n_fail"] for r in results)
    tn = sum(r["n_na"] for r in results)
    print("\n" + "#" * 60)
    print(f"  OVERALL: {overall}   ({tp} pass, {tf} fail, {tn} n/a "
          f"across {len(results)} spec(s))")
    print(f"  Combined report: {path}")
    print("#" * 60 + "\n")
    return 1 if overall == "FAIL" else 0


if __name__ == "__main__":
    sys.exit(main())
