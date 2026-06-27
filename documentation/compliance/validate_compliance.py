#!/usr/bin/env python3
"""
Validate compliance.data.js against the actual code — bidirectional, strict.

The DB (compliance.data.js) is the single source of truth. Every feature carries a
stable test id (tid, e.g. DCC-S9.1-CS-003) and lists the tests / HIL checks that
cover it in `refs.tests` / `refs.hilChecks`. Tests are tagged in code with that tid
via a `@compliance <tid>[, <tid>...]` comment. This script makes the linkage
drift-proof by enforcing FULL mutual consistency:

  DB -> code
    * a feature claiming gTest/HIL coverage (state ok/partial) must list at least
      one ref, and every referenced test must (a) EXIST as a real TEST(...) in the
      named *_Test.cxx and (b) carry `@compliance <tid>` directly above it.
    * every referenced HIL check's file must carry `@compliance <tid>`.
    * a supported=ok feature's refs.symbols must still exist in src/dcc.

  code -> DB
    * every `@compliance` tid in code must exist in the DB (no orphan tags).
    * every gTest tag must sit on a TEST that the DB lists for that tid (no ghost
      tags the dashboard can't show) — this is the reverse of the EXIST check, so
      the named test and its tag can never diverge.
    * every HIL tag's file must be listed in that tid's refs.hilChecks.

Because a rename/move that misses either side becomes a hard error, the test name
and its compliance tag are locked together — no naming convention needed.

Run:    python3 validate_compliance.py            # advisory report, exit 0
        python3 validate_compliance.py --strict   # exit 1 if any ERROR (for CI)
"""

import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent                      # documentation/compliance -> repo root
SRC = ROOT / "src" / "dcc"
HIL = ROOT / "test" / "compliance"
DATA = HERE / "compliance.data.js"

TAG_RE = re.compile(r"@compliance\s+([A-Za-z0-9.\-]+(?:\s*,\s*[A-Za-z0-9.\-]+)*)")
TEST_RE = re.compile(r"^\s*TEST(?:_F|_P)?\(\s*([A-Za-z0-9_]+)\s*,\s*([A-Za-z0-9_]+)\s*\)")


def load_db():
    text = DATA.read_text(encoding="utf-8")
    eq = text.index("=", text.index("window.COMPLIANCE"))
    db = json.loads(text[eq + 1:].strip().rstrip(";").strip())
    feats = []
    for spec in db["specs"]:
        for f in spec["features"]:
            f["_spec"] = spec["spec"]
            feats.append(f)
    return feats


def tids_on_line(line):
    m = TAG_RE.search(line)
    if not m:
        return []
    return [t.strip() for t in m.group(1).split(",")]


def parse_cxx(path):
    """Returns (present, tagged, file_tids):
       present   = set of (suite, name) TESTs defined in the file
       tagged    = {(suite, name): set(tids)} from the @compliance comment block
                   directly above each TEST
       file_tids = {tid: count} of every @compliance tid anywhere in the file
    """
    lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    present, tagged, file_tids = set(), {}, {}
    for i, line in enumerate(lines):
        for tid in tids_on_line(line):
            file_tids[tid] = file_tids.get(tid, 0) + 1
        m = TEST_RE.match(line)
        if not m:
            continue
        key = (m.group(1), m.group(2))
        present.add(key)
        tids, j = set(), i - 1
        while j >= 0 and lines[j].lstrip().startswith("//"):
            tids.update(tids_on_line(lines[j]))
            j -= 1
        if tids:
            tagged.setdefault(key, set()).update(tids)
    return present, tagged, file_tids


def parse_hil(path):
    """Returns {tid: count} of every @compliance tid in the file."""
    file_tids = {}
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        for tid in tids_on_line(line):
            file_tids[tid] = file_tids.get(tid, 0) + 1
    return file_tids


def main():
    strict = "--strict" in sys.argv
    feats = load_db()
    by_id = {f["tid"]: f for f in feats}
    db_tids = set(by_id)

    cxx = {p.name: parse_cxx(p) for p in sorted(SRC.glob("*_Test.cxx"))}
    hil = {p.name: parse_hil(p) for p in sorted(HIL.glob("*_compliance.py"))}
    src_blob = "\n".join(p.read_text(encoding="utf-8", errors="ignore")
                         for p in list(SRC.glob("*.c")) + list(SRC.glob("*.h")))

    errors, warns = [], []
    # tids tagged anywhere (for the summary + orphan scan)
    host_tids = {t for _, _, ft in cxx.values() for t in ft}
    hil_tids = {t for ft in hil.values() for t in ft}

    # ---- DB -> code -------------------------------------------------------
    for f in feats:
        tid = f["tid"]
        tests = f["refs"].get("tests", [])
        checks = f["refs"].get("hilChecks", [])

        if f["gtest"]["state"] in ("ok", "partial"):
            if not tests:
                errors.append(f"{tid}: gTest='{f['gtest']['state']}' but refs.tests is empty")
            for t in tests:
                fn, nm = t.get("file", ""), t.get("name", "")
                if fn not in cxx:
                    errors.append(f"{tid}: refs.tests file '{fn}' not found in src/dcc")
                    continue
                if "." not in nm:
                    errors.append(f"{tid}: refs.tests name '{nm}' is not Suite.test_name")
                    continue
                key = tuple(nm.split(".", 1))
                present, tagged, _ = cxx[fn]
                if key not in present:
                    errors.append(f"{tid}: test {nm} not found in {fn} (renamed/removed?)")
                elif tid not in tagged.get(key, set()):
                    errors.append(f"{tid}: test {nm} exists in {fn} but is not tagged @compliance {tid}")

        if f["hil"]["state"] in ("ok", "partial"):
            if not checks:
                errors.append(f"{tid}: HIL='{f['hil']['state']}' but refs.hilChecks is empty")
            for h in checks:
                fn = h.get("file", "")
                if fn not in hil:
                    errors.append(f"{tid}: refs.hilChecks file '{fn}' not found in test/compliance")
                elif tid not in hil[fn]:
                    errors.append(f"{tid}: HIL check in {fn} is not tagged @compliance {tid}")

        if f["hil"]["state"] in ("no", "planned", "nobs") and tid in hil_tids:
            warns.append(f"{tid}: HIL state is '{f['hil']['state']}' yet a HIL check is tagged")

        if f["supported"]["state"] == "ok":
            for sym in f["refs"].get("symbols", []):
                if sym not in src_blob:
                    errors.append(f"{tid}: supported=ok but symbol '{sym}' not found in src/dcc")

    # ---- code -> DB -------------------------------------------------------
    for tid in sorted(host_tids | hil_tids):
        if tid not in db_tids:
            where = [n for n, (_, _, ft) in cxx.items() if tid in ft] + \
                    [n for n, ft in hil.items() if tid in ft]
            errors.append(f"ORPHAN tag {tid} in {', '.join(sorted(where))} — no such tid in the DB")

    # gtest tag must sit on a test the DB lists for that tid; warn if a tag floats
    for fn, (present, tagged, file_tids) in cxx.items():
        attached = {t for tids in tagged.values() for t in tids}
        for tid in file_tids:
            if tid in db_tids and tid not in attached:
                warns.append(f"{tid}: @compliance tag in {fn} is not directly above a TEST")
        for key, tids in tagged.items():
            nm = f"{key[0]}.{key[1]}"
            for tid in tids:
                if tid not in db_tids:
                    continue
                listed = any(t.get("file") == fn and t.get("name") == nm
                             for t in by_id[tid]["refs"].get("tests", []))
                if not listed:
                    errors.append(f"{tid}: code tags test {nm} ({fn}) but the DB does not list it in refs.tests")

    # hil tag's file must be listed in refs.hilChecks for that tid
    for fn, file_tids in hil.items():
        for tid in file_tids:
            if tid not in db_tids:
                continue
            listed = any(h.get("file") == fn for h in by_id[tid]["refs"].get("hilChecks", []))
            if not listed:
                errors.append(f"{tid}: code tags HIL in {fn} but the DB does not list it in refs.hilChecks")

    # ---- report -----------------------------------------------------------
    print(f"\nCompliance validation — {len(feats)} feature(s), "
          f"{len(host_tids)} gTest tid(s) tagged, {len(hil_tids)} HIL tid(s) tagged\n")
    for tid in sorted(db_tids):
        f = by_id[tid]
        g = "gtest" if tid in host_tids else ("—" if f["gtest"]["state"] in ("na", "no", "nobs") else "MISSING")
        h = "hil" if tid in hil_tids else ("—" if f["hil"]["state"] in ("na", "no", "planned", "nobs") else "MISSING")
        print(f"  {tid:<20} sup={f['supported']['state']:<7} gTest={f['gtest']['state']:<7}[{g}] HIL={f['hil']['state']:<8}[{h}]")

    if warns:
        print("\nWARNINGS:")
        for w in warns:
            print(f"  ! {w}")
    if errors:
        print("\nERRORS:")
        for e in errors:
            print(f"  x {e}")
    else:
        print("\nNo errors — DB, tests, and symbols are fully consistent.")

    print(f"\n{len(errors)} error(s), {len(warns)} warning(s).")
    sys.exit(1 if (strict and errors) else 0)


if __name__ == "__main__":
    main()
