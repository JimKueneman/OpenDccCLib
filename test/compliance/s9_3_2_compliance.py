#!/usr/bin/env python3
"""
NMRA S-9.3.2 -- RailCom cutout timing compliance (command-station transmit).

Hardware-in-the-loop: captures ch0 (DCC out / PB1) and ch2 (cutout-active
strobe / PB2) simultaneously, injects a 3/4/5/6-byte packet mix via UART, and
checks the cutout timing against the S-9.3.2 spec:
  T_CS  26-32 µs   (end-bit trailing edge → cutout-active rising edge)
  T_CE  454-488 µs (end-bit trailing edge → cutout-active falling edge)

The end-bit trailing edge on ch0 is identified as the last ch0 transition in
the window [T_rise - 50 µs, T_rise - 10 µs] before each ch2 rising edge.

Shared plumbing lives in compliance_lib.py.

Run standalone:  .venv/bin/python s9_3_2_compliance.py
Or via:          .venv/bin/python run_all.py
"""

import os
import csv
import bisect
import tempfile
import compliance_lib as lib

SPEC_DOC   = "S-9.3.2"
SPEC_TITLE = "RailCom cutout timing (command-station transmit)"
SOURCE_PDF = "documentation/specs/s-9.3.2_railcom.pdf"
ASPECT     = ("cutout-active strobe (ch2/PB2) vs DCC end-bit (ch0/PB1); "
              "3/4/5/6-byte packet mix: idle, short-addr speed, long-addr speed, POM CV")

# --- hardware ---
CUTOUT_CHANNEL = 2          # ch2 = PB2 = cutout-active strobe

# --- capture ---
CAPTURE_SECONDS = 0.5       # 500 ms -> ~70 packets at ~7 ms/packet

# --- S-9.3.2 §3 timing limits (µs from end-bit trailing edge) ---
T_CS_MIN_US = 26.0
T_CS_MAX_US = 32.0
T_CE_MIN_US = 454.0
T_CE_MAX_US = 488.0
WIN_MIN_US  = T_CE_MIN_US - T_CS_MAX_US    # 422.0
WIN_MAX_US  = T_CE_MAX_US - T_CS_MIN_US    # 462.0

# --- search window for the end-bit trailing edge on ch0 ---
# The last ch0 transition before each ch2 rising edge within this band is the
# end-bit transition that arms the cutout one-shot timer.  Widened beyond the
# spec limits to absorb jitter without missing the edge.
_REF_LO_US = 10.0
_REF_HI_US = 50.0


# --------------------------------------------------------------------------
# Helpers
# --------------------------------------------------------------------------

def _rising_edges(rows):
    """Timestamps of transitions TO logic-1."""
    return [rows[i][0] for i in range(1, len(rows)) if rows[i][1] == 1]


def _falling_edges(rows):
    """Timestamps of transitions TO logic-0."""
    return [rows[i][0] for i in range(1, len(rows)) if rows[i][1] == 0]


def _find_ref(ch0_times, t_rise_s):
    """Return the last ch0 transition in [t_rise - _REF_HI_US, t_rise - _REF_LO_US],
    or None if no transition falls in that band.  ch0_times must be sorted."""
    lo = t_rise_s - _REF_HI_US * 1e-6
    hi = t_rise_s - _REF_LO_US * 1e-6
    idx_hi = bisect.bisect_right(ch0_times, hi)
    idx_lo = bisect.bisect_left(ch0_times, lo)
    return ch0_times[idx_hi - 1] if idx_lo < idx_hi else None


def _measure_cutouts(ch0_rows, ch2_rows):
    """Return list of (t_cs_us, t_ce_us, win_us) for each matched cutout window.

    Pairs each ch2 rising edge with the first following falling edge within
    600 µs, then computes T_CS and T_CE relative to the last ch0 transition
    before the rising edge (the end-bit trailing edge).
    """
    ch0_times = [r[0] for r in ch0_rows]   # sorted ascending from CSV
    rises = _rising_edges(ch2_rows)
    falls = _falling_edges(ch2_rows)

    results = []
    fall_idx = 0
    for t_rise in rises:
        # advance fall_idx past falls that are at or before this rise
        while fall_idx < len(falls) and falls[fall_idx] <= t_rise:
            fall_idx += 1
        if fall_idx >= len(falls) or falls[fall_idx] > t_rise + 600e-6:
            continue                         # no matching fall -- boundary artifact
        t_fall = falls[fall_idx]

        ref = _find_ref(ch0_times, t_rise)
        if ref is None:
            continue                         # no ch0 edge in search band -- skip

        t_cs = (t_rise - ref) * 1e6
        t_ce = (t_fall - ref) * 1e6
        results.append((t_cs, t_ce, t_ce - t_cs))

    return results


def _find_channel_col(header, hw_channel):
    """Return the column index for hw_channel in the CSV header.
    The saleae multi-channel export produces a single digital.csv with header:
      Time [s], Channel 0, Channel 2, ...
    """
    for i, h in enumerate(header):
        if i > 0 and str(hw_channel) in h:
            return i
    raise RuntimeError(
        f"Channel {hw_channel} not found in CSV header: {header}")


def _read_channel_transitions(csv_path, col):
    """Extract (time_s, value) rows for column `col` from a multi-channel CSV.
    Only emits a row when that column's value changes (i.e. actual transitions)."""
    rows = []
    prev = None
    with open(csv_path) as f:
        reader = csv.reader(f)
        next(reader)                    # skip header
        for line in reader:
            if len(line) <= col:
                continue
            v = int(line[col])
            if prev is None or v != prev:
                rows.append((float(line[0]), v))
                prev = v
    return rows


# --------------------------------------------------------------------------
# Capture + decode
# --------------------------------------------------------------------------

def _capture_and_measure(port):
    from saleae import automation

    def stimulus():
        if port is None:
            return
        # Inject 4 packet lengths into the live capture window:
        #   idle (3 bytes) runs as background after CLEAR resets auto-refresh.
        #   SPEED 3   -> short-addr 128-step: 4 bytes  (auto-refresh)
        #   SPEED 200 -> long-addr 128-step:  5 bytes  (auto-refresh)
        #   CV WRITE  -> long-addr POM CV:    6 bytes  (one-shot, repeat=3)
        lib.send_command(port, "CLEAR",                settle=0.02)
        lib.send_command(port, "SPEED 3 50 FWD 128",   settle=0.02)
        lib.send_command(port, "SPEED 200 50 FWD 128", settle=0.02)
        lib.send_command(port, "CV WRITE 200 1 42",    settle=0.02)

    dev_cfg = automation.LogicDeviceConfiguration(
        enabled_digital_channels=[lib.DIGITAL_CHANNEL, CUTOUT_CHANNEL],
        digital_sample_rate=lib.SAMPLE_RATE_HZ,
    )
    cap_cfg = automation.CaptureConfiguration(
        capture_mode=automation.TimedCaptureMode(duration_seconds=CAPTURE_SECONDS)
    )

    with tempfile.TemporaryDirectory() as d:
        with automation.Manager.connect(port=lib.AUTOMATION_PORT) as mgr:
            print(f"[saleae] {CAPTURE_SECONDS*1e3:.0f} ms dual capture "
                  f"ch{lib.DIGITAL_CHANNEL}+ch{CUTOUT_CHANNEL} @ "
                  f"{lib.SAMPLE_RATE_HZ/1e6:.0f} MS/s  "
                  f"(idle + 4/5/6-byte mix)")
            with mgr.start_capture(
                device_id=lib.SALEAE_DEVICE_ID or None,
                device_configuration=dev_cfg,
                capture_configuration=cap_cfg,
            ) as cap:
                stimulus()
                cap.wait()
                cap.export_raw_data_csv(
                    directory=d,
                    digital_channels=[lib.DIGITAL_CHANNEL, CUTOUT_CHANNEL],
                )

        csv_path = os.path.join(d, "digital.csv")
        with open(csv_path) as f:
            header = next(csv.reader(f))
        ch0_col = _find_channel_col(header, lib.DIGITAL_CHANNEL)
        ch2_col = _find_channel_col(header, CUTOUT_CHANNEL)
        ch0_rows = _read_channel_transitions(csv_path, ch0_col)
        ch2_rows = _read_channel_transitions(csv_path, ch2_col)

    decoded      = lib.decode(ch0_rows)
    measurements = _measure_cutouts(ch0_rows, ch2_rows)

    n_wins = len(_rising_edges(ch2_rows))
    print(f"[decode] ch0: {len(ch0_rows)} transitions, "
          f"{len(decoded['packets'])} packets decoded")
    print(f"[decode] ch2: {len(ch2_rows)} transitions, "
          f"{n_wins} cutout-window rises, "
          f"{len(measurements)} matched")

    if port is not None:
        lib.send_command(port, "CLEAR", settle=0.05)
        print("[uart] CLEAR (post-capture scheduler flush)")

    return decoded, measurements


# --------------------------------------------------------------------------
# Checks
# --------------------------------------------------------------------------

def checks(rep, decoded, measurements):
    n_packets = len(decoded["packets"])
    n_cutouts = len(measurements)

    # 1. One cutout window per decoded packet (±2 boundary tolerance)
    rep.check(
        # @compliance DCC-S9.3.2-CS-004
        "S-9.3.2 §3.1", "one cutout window per packet",
        abs(n_cutouts - n_packets) <= 2,
        f"{n_cutouts} cutout windows matched, {n_packets} packets decoded "
        f"(boundary tolerance ±2)",
    )

    if not measurements:
        for clause, name in [
            ("S-9.3.2 §3.2", "T_CS 26-32 us"),
            ("S-9.3.2 §3.2", "T_CE 454-488 us"),
            ("S-9.3.2 §3.2", f"window {WIN_MIN_US:.0f}-{WIN_MAX_US:.0f} us"),
        ]:
            rep.check(clause, name, False, "no cutout windows measured")
        return

    t_cs_vals = [m[0] for m in measurements]
    t_ce_vals = [m[1] for m in measurements]
    win_vals  = [m[2] for m in measurements]

    # 2. T_CS  @compliance DCC-S9.3.2-CS-001
    rep.check(
        "S-9.3.2 §3.2", "T_CS 26-32 us  (end-bit edge → cutout start)",
        all(T_CS_MIN_US <= v <= T_CS_MAX_US for v in t_cs_vals),
        lib.sigma_margin_detail(t_cs_vals, T_CS_MIN_US, T_CS_MAX_US) + " us",
    )

    # 3. T_CE  @compliance DCC-S9.3.2-CS-002
    rep.check(
        "S-9.3.2 §3.2", "T_CE 454-488 us  (end-bit edge → cutout end)",
        all(T_CE_MIN_US <= v <= T_CE_MAX_US for v in t_ce_vals),
        lib.sigma_margin_detail(t_ce_vals, T_CE_MIN_US, T_CE_MAX_US) + " us",
    )

    # 4. Window duration (derived: tightest combination of both walls)
    rep.check(
        "S-9.3.2 §3.2",
        # @compliance DCC-S9.3.2-CS-003
        f"window T_CE-T_CS  {WIN_MIN_US:.0f}-{WIN_MAX_US:.0f} us",
        all(WIN_MIN_US <= v <= WIN_MAX_US for v in win_vals),
        lib.sigma_margin_detail(win_vals, WIN_MIN_US, WIN_MAX_US) + " us",
    )


# --------------------------------------------------------------------------
# Entry points
# --------------------------------------------------------------------------

def run():
    """Drive DCC, capture dual-channel, measure cutouts, run checks."""
    port = None
    if lib.DRIVE_UART:
        port = lib.start_dcc()
    decoded, measurements = _capture_and_measure(port)
    rep = lib.Report(SPEC_DOC, SPEC_TITLE, SOURCE_PDF, ASPECT)
    checks(rep, decoded, measurements)
    return rep.finish()


def main():
    import sys
    print("\n#### DCC COMPLIANCE TEST ####")
    print(f"Spec under test : NMRA {SPEC_DOC}  RailCom Cutout Timing")
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
              f"{lib.AUTOMATION_PORT}), device connected, ch2/PB2 wired.\n")
        return 2
    path = lib.write_html([rep.as_dict()], lib.report_path("s9_3_2"),
                          title=f"NMRA {SPEC_DOC} Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
