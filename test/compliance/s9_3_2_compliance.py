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
ASPECT     = ("cutout-active strobe (ch2/PB2) vs DCC end-bit (ch0/PB1), plus the "
              "RAILCOM_RX_WINDOW mirror (ch5/PB18) for the 5 interior sub-window boundaries; "
              "3/4/5/6-byte packet mix: idle, short-addr speed, long-addr speed, POM CV")

# --- hardware ---
CUTOUT_CHANNEL = 2          # ch2 = PB2 = cutout-active strobe (begin T_CS / end T_CE)
WINDOW_CHANNEL = 5          # ch5 = RAILCOM_RX_WINDOW mirror pin: high while a Ch1/Ch2
                            # window is open (uart_rx_enable/disable edges). Set this
                            # to the Saleae channel you probe the new pin with.

# --- capture ---
CAPTURE_SECONDS = 0.5       # 500 ms -> ~70 packets at ~7 ms/packet

# --- S-9.3.2 §3.2 per-state interior periods (us), as configured in the HIL
# firmware (saleae_hil_compliance.c). SETTLING/CH1/GAP are the spec defaults;
# DELAY (23) and CH2 (263) are pre-adjusted there to re-center T_CS / T_CE under
# bench latency. Measured as differences of mirror edges, so latency largely
# cancels -- a generous tolerance absorbs the residual ISR jitter. ---
SUBSTATE_EXPECT_US = {"SETTLING": 54.0, "CH1": 97.0, "GAP": 16.0, "CH2": 263.0}
SUBSTATE_TOL_US    = 10.0

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


def _measure_substates(ch2_rows, win_rows):
    """Per-cutout interior periods from the RAILCOM_RX_WINDOW mirror channel.

    The mirror is HIGH while a RailCom channel window is open. Within each cutout
    [t_rise, t_fall] on ch2 its edges are: rise@T_TS1 (Ch1 opens), fall@T_TC1 (Ch1
    closes), rise@T_TS2 (Ch2 opens); the final close coincides with t_fall (T_CE).
    Returns (substates, n_ordered, n_unordered) where substates is a list of
    {SETTLING, CH1, GAP, CH2} (us), one per cutout whose mirror edges are present
    and correctly ordered (T_CS < T_TS1 < T_TC1 < T_TS2 < T_CE)."""
    rises = _rising_edges(ch2_rows)
    falls = _falling_edges(ch2_rows)
    win_rises = _rising_edges(win_rows)
    win_falls = _falling_edges(win_rows)

    substates, n_ordered, n_unordered = [], 0, 0
    fall_idx = 0
    for t_rise in rises:
        while fall_idx < len(falls) and falls[fall_idx] <= t_rise:
            fall_idx += 1
        if fall_idx >= len(falls) or falls[fall_idx] > t_rise + 600e-6:
            continue
        t_fall = falls[fall_idx]

        r_in = [t for t in win_rises if t_rise < t < t_fall]
        f_in = [t for t in win_falls if t_rise < t < t_fall]
        if len(r_in) < 2 or len(f_in) < 1:
            continue                        # mirror edges missing -> pin unwired/flat
        ts1, tc1, ts2 = r_in[0], f_in[0], r_in[1]

        if not (t_rise < ts1 < tc1 < ts2 < t_fall):
            n_unordered += 1
            continue
        n_ordered += 1
        substates.append({
            "SETTLING": (ts1 - t_rise) * 1e6,
            "CH1":      (tc1 - ts1) * 1e6,
            "GAP":      (ts2 - tc1) * 1e6,
            "CH2":      (t_fall - ts2) * 1e6,
        })
    return substates, n_ordered, n_unordered


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

    chans = [lib.DIGITAL_CHANNEL, CUTOUT_CHANNEL, WINDOW_CHANNEL]
    dev_cfg = automation.LogicDeviceConfiguration(
        enabled_digital_channels=chans,
        digital_sample_rate=lib.SAMPLE_RATE_HZ,
    )
    cap_cfg = automation.CaptureConfiguration(
        capture_mode=automation.TimedCaptureMode(duration_seconds=CAPTURE_SECONDS)
    )

    with tempfile.TemporaryDirectory() as d:
        with automation.Manager.connect(port=lib.AUTOMATION_PORT) as mgr:
            print(f"[saleae] {CAPTURE_SECONDS*1e3:.0f} ms capture "
                  f"ch{lib.DIGITAL_CHANNEL}+ch{CUTOUT_CHANNEL}+ch{WINDOW_CHANNEL} @ "
                  f"{lib.SAMPLE_RATE_HZ/1e6:.0f} MS/s  "
                  f"(idle + 4/5/6-byte mix)")
            with mgr.start_capture(
                device_id=lib.SALEAE_DEVICE_ID or None,
                device_configuration=dev_cfg,
                capture_configuration=cap_cfg,
            ) as cap:
                stimulus()
                cap.wait()
                cap.export_raw_data_csv(directory=d, digital_channels=chans)

        csv_path = os.path.join(d, "digital.csv")
        with open(csv_path) as f:
            header = next(csv.reader(f))
        ch0_col = _find_channel_col(header, lib.DIGITAL_CHANNEL)
        ch2_col = _find_channel_col(header, CUTOUT_CHANNEL)
        win_col = _find_channel_col(header, WINDOW_CHANNEL)
        ch0_rows = _read_channel_transitions(csv_path, ch0_col)
        ch2_rows = _read_channel_transitions(csv_path, ch2_col)
        win_rows = _read_channel_transitions(csv_path, win_col)

    decoded      = lib.decode(ch0_rows)
    measurements = _measure_cutouts(ch0_rows, ch2_rows)
    substates, n_ord, n_unord = _measure_substates(ch2_rows, win_rows)

    n_wins = len(_rising_edges(ch2_rows))
    print(f"[decode] ch0: {len(ch0_rows)} transitions, "
          f"{len(decoded['packets'])} packets decoded")
    print(f"[decode] ch2: {len(ch2_rows)} transitions, "
          f"{n_wins} cutout-window rises, {len(measurements)} matched")
    print(f"[decode] ch{WINDOW_CHANNEL} (RAILCOM_RX_WINDOW): {len(win_rows)} transitions, "
          f"{n_ord} sub-window sets ordered, {n_unord} mis-ordered")

    if port is not None:
        lib.send_command(port, "CLEAR", settle=0.05)
        print("[uart] CLEAR (post-capture scheduler flush)")

    return decoded, measurements, substates, (n_ord, n_unord)


# --------------------------------------------------------------------------
# Runtime cutout control: configurable timing (CS-007) + cancel (CS-008)
# --------------------------------------------------------------------------

def _capture_ch2_ch5(port, seconds=CAPTURE_SECONDS, pre=None, apply_before=False):
    """Capture PB2 (ch2) + RAILCOM_RX_WINDOW (ch5); return (substates, ch2_rows).
    `pre` runs either BEFORE the capture is armed (apply_before=True -- for a state
    change like RAILCOM TIMING that must be settled across the whole window) or
    DURING it, just after arming (apply_before=False -- for an event like RAILCOM
    CANCEL that must land inside the window)."""
    from saleae import automation
    chans = [CUTOUT_CHANNEL, WINDOW_CHANNEL]
    dev_cfg = automation.LogicDeviceConfiguration(
        enabled_digital_channels=chans, digital_sample_rate=lib.SAMPLE_RATE_HZ)
    cap_cfg = automation.CaptureConfiguration(
        capture_mode=automation.TimedCaptureMode(duration_seconds=seconds))
    with tempfile.TemporaryDirectory() as d:
        with automation.Manager.connect(port=lib.AUTOMATION_PORT) as mgr:
            if pre is not None and apply_before:
                pre()                     # settle the new timing before capturing
            with mgr.start_capture(device_id=lib.SALEAE_DEVICE_ID or None,
                                   device_configuration=dev_cfg,
                                   capture_configuration=cap_cfg) as cap:
                if pre is not None and not apply_before:
                    pre()                 # fire the event inside the window
                cap.wait()
                cap.export_raw_data_csv(directory=d, digital_channels=chans)
        csv_path = os.path.join(d, "digital.csv")
        with open(csv_path) as f:
            header = next(csv.reader(f))
        ch2 = _read_channel_transitions(csv_path, _find_channel_col(header, CUTOUT_CHANNEL))
        win = _read_channel_transitions(csv_path, _find_channel_col(header, WINDOW_CHANNEL))
    substates, _, _ = _measure_substates(ch2, win)
    return substates, ch2


def _pb2_cutout_durations(ch2_rows, drop_edges=True):
    """PB2 high-duration (us) of each cutout. drop_edges removes the first/last
    pulse (capture-boundary partials) so a truncated cutout stands out cleanly."""
    out, t_rise = [], None
    for t, v in ch2_rows:
        if v == 1:
            t_rise = t
        elif v == 0 and t_rise is not None:
            out.append((t - t_rise) * 1e6)
            t_rise = None
    return out[1:-1] if drop_edges and len(out) > 2 else out


# @compliance DCC-S9.3.2-CS-007
def configurable_timing_test(rep, port):
    """S-9.3.2 CS-007: user-configurable cutout timing. Reconfigure the CH1 window
    over UART and confirm the new period reaches the wire (measured on PB18), then
    confirm a 0 selects the spec default. CH1's spec default (97) equals the bench
    value, so this varies CH1 without disturbing the T_CS/T_CE calibration."""
    clause = "S-9.3.2 §3.2 (configurable cutout timing)"

    def ch1_vals(timing):
        # Apply + settle the new timing BEFORE the capture so the whole window
        # reflects it (the change takes effect on the next cutout).
        subs, _ = _capture_ch2_ch5(
            port, apply_before=True,
            pre=lambda: lib.send_command(port, "RAILCOM " + timing, settle=0.3))
        return [s["CH1"] for s in subs]

    # Custom CH1 = 150 us (default 97). Others held at the bench-calibrated values.
    custom = ch1_vals("TIMING 23 54 150 16 263")
    ok = bool(custom) and all(abs(v - 150) <= 12 for v in custom)
    rep.check(clause, "custom CH1=150us reaches the wire", ok,
              (lib.sigma_margin_detail(custom, 138, 162) + " us" if custom
               else "no sub-windows measured (pin wired? firmware reflashed?)"))

    # CH1 = 0 -> spec default (97 us).
    default = ch1_vals("TIMING 23 54 0 16 263")
    ok = bool(default) and all(abs(v - 97) <= 12 for v in default)
    rep.check(clause, "CH1=0 selects the spec default (97us)", ok,
              (lib.sigma_margin_detail(default, 85, 109) + " us" if default
               else "no sub-windows measured"))

    # Restore the bench calibration for any later work.
    lib.send_command(port, "RAILCOM TIMING 23 54 97 16 263", settle=0.2)


# @compliance DCC-S9.3.2-CS-008
def cancel_midcutout_test(rep, port):
    """S-9.3.2 CS-008: cancel mid-cutout restores the H-bridge. Arm a one-shot
    cancel; the firmware fires it early (SETTLING/CH1) in the next cutout, so on PB2
    exactly one cutout is truncated to a short pulse while the rest stay ~full."""
    clause = "S-9.3.2 §3.1 (cancel mid-cutout restores H-bridge)"

    # Baseline: cutouts are uniform (~440 us PB2 high) with no cancel.
    _, base_rows = _capture_ch2_ch5(port)
    base = _pb2_cutout_durations(base_rows)
    base_ok = len(base) >= 5 and all(d >= 380 for d in base)
    rep.check(clause, "baseline cutouts are full (~440us, uniform)", base_ok,
              lib.sigma_margin_detail(base, 380, None) + " us" if base else "no cutouts")

    # Arm a cancel into a fresh capture; one cutout should come out truncated.
    _, rows = _capture_ch2_ch5(
        port, seconds=0.4,
        pre=lambda: lib.send_command(port, "RAILCOM CANCEL", settle=0.0))
    durs = _pb2_cutout_durations(rows)
    short  = [d for d in durs if d < 300]
    normal = [d for d in durs if d >= 380]

    rep.check(clause, "one cutout is truncated (H-bridge restored mid-cutout)",
              len(short) >= 1,
              f"truncated pulses (us): {[f'{d:.0f}' for d in short]}; "
              f"full cutouts: {len(normal)}")
    rep.check(clause, "cancel is one-shot (other cutouts unaffected)",
              len(short) <= 2 and len(normal) >= 4,
              f"{len(short)} short, {len(normal)} full (~440us) cutouts in the window")


# --------------------------------------------------------------------------
# Checks
# --------------------------------------------------------------------------

def substate_checks(rep, substates, order_counts):
    """S-9.3.2 §3.2 interior 5-state machine, from the RAILCOM_RX_WINDOW mirror:
    event ordering (CS-005) and per-state default periods (CS-006)."""
    n_ord, n_unord = order_counts
    clause = "S-9.3.2 §3.2 (cutout sub-windows)"

    if not substates:
        # @compliance DCC-S9.3.2-CS-005
        rep.check(clause, "5-state sub-window edges present on the wire", False,
                  f"no ordered sub-window sets on ch{WINDOW_CHANNEL} -- reflash firmware "
                  f"(RAILCOM_RX_WINDOW pin) and probe it on ch{WINDOW_CHANNEL}")
        # @compliance DCC-S9.3.2-CS-006
        rep.check(clause, "per-state periods (54/97/16/263 us)", False,
                  "no sub-windows measured")
        return

    # CS-005: every captured cutout's interior events are correctly ordered.
    # @compliance DCC-S9.3.2-CS-005
    rep.check(clause, "5-state event order T_CS<T_TS1<T_TC1<T_TS2<T_CE",
              n_unord == 0 and n_ord > 0,
              f"{n_ord} cutouts correctly ordered, {n_unord} mis-ordered")

    # CS-006: each interior period sits within tolerance of its configured default.
    all_ok = True
    detail_bits = []
    for state in ("SETTLING", "CH1", "GAP", "CH2"):
        vals = [s[state] for s in substates]
        exp = SUBSTATE_EXPECT_US[state]
        ok = all(abs(v - exp) <= SUBSTATE_TOL_US for v in vals)
        all_ok = all_ok and ok
        detail_bits.append(f"{state}~{exp:.0f}: " +
                           lib.sigma_margin_detail(vals, exp - SUBSTATE_TOL_US,
                                                   exp + SUBSTATE_TOL_US))
    # @compliance DCC-S9.3.2-CS-006
    rep.check(clause, "per-state periods (SETTLING 54 / CH1 97 / GAP 16 / CH2 263 us)",
              all_ok, "  |  ".join(detail_bits) + " us")


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
    decoded, measurements, substates, order_counts = _capture_and_measure(port)
    rep = lib.Report(SPEC_DOC, SPEC_TITLE, SOURCE_PDF, ASPECT)
    checks(rep, decoded, measurements)
    substate_checks(rep, substates, order_counts)
    if port is not None:
        configurable_timing_test(rep, port)
        cancel_midcutout_test(rep, port)
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
