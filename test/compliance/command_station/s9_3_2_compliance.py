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
import time
import bisect
import tempfile
import compliance_lib as lib

SPEC_DOC   = "S-9.3.2"
SPEC_TITLE = "RailCom cutout timing (command-station transmit)"
SOURCE_PDF = "documentation/specs/s-9.3.2_railcom.pdf"
ASPECT     = ("cutout-active strobe (ch2/PB2) vs the DECODED packet end bit's last edge (ch0/PB1), plus the "
              "RAILCOM_RX_WINDOW mirror (ch5/PB18) for the 5 interior sub-window boundaries; "
              "3/4/5/6-byte packet mix: idle, short-addr speed, long-addr speed, POM CV; "
              "plus the RailCom RECEIVE path via the mock-decoder loopback (PB6 -> PB16, ch6)")

# --- hardware ---
CUTOUT_CHANNEL = 2          # ch2 = PB2 = cutout-active strobe (begin T_CS / end T_CE)
WINDOW_CHANNEL = 5          # ch5 = RAILCOM_RX_WINDOW mirror pin: high while a Ch1/Ch2
                            # window is open (uart_rx_enable/disable edges). Set this
                            # to the Saleae channel you probe the new pin with.
LOOPBACK_CHANNEL = 6        # ch6 = PB16 = RAILCOM_RX: the RailCom receive UART, fed by the
                            # mock decoder transmitter (PB6) through a board jumper.
RC_BAUD = 250_000           # S-9.3.2 RailCom bit rate

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

# --- T_CS / T_CE reference: the DECODED packet end bit's last edge ---
# S-9.3.2 Table 1 measures every cutout parameter from the zero crossing of the
# packet end bit's LAST edge. The reference is taken from the decoded packet
# (compliance_lib.decode -> packet_end_times), NOT from "the last ch0 edge
# before the strobe": the DUT's logic pin keeps clocking through the cutout, so
# that edge can be the end bit's MID-bit transition -- a cutout armed one
# half-bit early (issue #3) then measured as a perfect 26 us. Each strobe rise
# is paired with the nearest packet end within +/-_REF_MATCH_US so an early
# cutout reports a NEGATIVE T_CS instead of a false pass.
_REF_MATCH_US = 150.0


# --------------------------------------------------------------------------
# Helpers
# --------------------------------------------------------------------------

def _rising_edges(rows):
    """Timestamps of transitions TO logic-1."""
    return [rows[i][0] for i in range(1, len(rows)) if rows[i][1] == 1]


def _falling_edges(rows):
    """Timestamps of transitions TO logic-0."""
    return [rows[i][0] for i in range(1, len(rows)) if rows[i][1] == 0]


def _nearest_packet_end(end_times, t_rise_s):
    """Return the packet end-bit last-edge time nearest to t_rise_s if it lies
    within +/-_REF_MATCH_US, else None.  end_times must be sorted."""
    idx = bisect.bisect_left(end_times, t_rise_s)
    best = None
    for cand in (idx - 1, idx):
        if 0 <= cand < len(end_times):
            d = abs(t_rise_s - end_times[cand])
            if d <= _REF_MATCH_US * 1e-6 and (best is None or d < abs(t_rise_s - best)):
                best = end_times[cand]
    return best


def _measure_cutouts(decoded, ch2_rows):
    """Return (measurements, unmatched) where measurements is a list of
    (t_cs_us, t_ce_us, win_us) per cutout window and unmatched counts the
    strobe rises with no decoded packet end within +/-_REF_MATCH_US.

    Pairs each ch2 rising edge with the first following falling edge within
    600 us, then computes T_CS and T_CE relative to the decoded packet's
    end-bit last edge (S-9.3.2 Table 1 reference).  T_CS is NEGATIVE when the
    cutout begins before the end bit has finished.
    """
    end_times = sorted(decoded["packet_end_times"])
    rises = _rising_edges(ch2_rows)
    falls = _falling_edges(ch2_rows)

    results = []
    unmatched = 0
    fall_idx = 0
    for t_rise in rises:
        # advance fall_idx past falls that are at or before this rise
        while fall_idx < len(falls) and falls[fall_idx] <= t_rise:
            fall_idx += 1
        if fall_idx >= len(falls) or falls[fall_idx] > t_rise + 600e-6:
            continue                         # no matching fall -- boundary artifact
        t_fall = falls[fall_idx]

        ref = _nearest_packet_end(end_times, t_rise)
        if ref is None:
            unmatched += 1                   # strobe with no decoded packet end nearby
            continue

        t_cs = (t_rise - ref) * 1e6
        t_ce = (t_fall - ref) * 1e6
        results.append((t_cs, t_ce, t_ce - t_cs))

    return results, unmatched


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
        #   CV WRITE  -> long-addr POM CV:    6 bytes  (one-shot, library default 2 sends)
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
    measurements, unmatched = _measure_cutouts(decoded, ch2_rows)
    substates, n_ord, n_unord = _measure_substates(ch2_rows, win_rows)

    n_wins = len(_rising_edges(ch2_rows))
    print(f"[decode] ch0: {len(ch0_rows)} transitions, "
          f"{len(decoded['packets'])} packets decoded")
    print(f"[decode] ch2: {len(ch2_rows)} transitions, "
          f"{n_wins} cutout-window rises, {len(measurements)} matched to a decoded "
          f"packet end, {unmatched} unmatched")
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
        "S-9.3.2 §3.2", "T_CS 26-32 us  (end-bit LAST edge → cutout start)",
        all(T_CS_MIN_US <= v <= T_CS_MAX_US for v in t_cs_vals),
        lib.sigma_margin_detail(t_cs_vals, T_CS_MIN_US, T_CS_MAX_US) + " us",
    )

    # 3. T_CE  @compliance DCC-S9.3.2-CS-002
    rep.check(
        "S-9.3.2 §3.2", "T_CE 454-488 us  (end-bit LAST edge → cutout end)",
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
# RailCom RECEIVE path via the mock-decoder loopback (CS-010..CS-014)
#
# The DUT's real receive path: RAILCOM_RX (UART2, PB16) -> .uart_read -> the
# library's 4/8 decode + datagram assembly -> RC RESULT lines on the command
# UART. Stimulus is the DUT's own mock transmitter (UART1, PB6), jumpered to
# PB16 and started by the library's window-open hooks, so the bytes land
# exactly where a decoder puts them. Saleae ch6 taps the jumper so every check
# is grounded in what was on the wire, not in what the firmware says it sent.
#
# Host-side 4/8 encoder: transcribed from the S-9.3.2 draft (Apr 2026) Table 2,
# independent of the library. One deliberate departure: the draft prints 0x0B as
# 10001101, the same word it gives 0x0D -- a typesetting error (a 4-of-8 table
# cannot repeat a word); RCN-217 and the 2012 S-9.3.2 give 0x96, used here.
# --------------------------------------------------------------------------

RC_CODE = [
    0xAC, 0xAA, 0xA9, 0xA5, 0xA3, 0xA6, 0x9C, 0x9A,   # 0x00-0x07
    0x99, 0x95, 0x93, 0x96, 0x8E, 0x8D, 0x8B, 0xB1,   # 0x08-0x0F  (0x0B: see note)
    0xB2, 0xB4, 0xB8, 0x74, 0x72, 0x6C, 0x6A, 0x69,   # 0x10-0x17
    0x65, 0x63, 0x66, 0x5C, 0x5A, 0x59, 0x55, 0x53,   # 0x18-0x1F
    0x56, 0x4E, 0x4D, 0x4B, 0x47, 0x71, 0xE8, 0xE4,   # 0x20-0x27
    0xE2, 0xD1, 0xC9, 0xC5, 0xD8, 0xD4, 0xD2, 0xCA,   # 0x28-0x2F
    0xC6, 0xCC, 0x78, 0x17, 0x1B, 0x1D, 0x1E, 0x2E,   # 0x30-0x37
    0x36, 0x3A, 0x27, 0x2B, 0x2D, 0x35, 0x39, 0x33,   # 0x38-0x3F
]
RC_ACK      = 0x0F     # Table 2: ACK (either form)
RC_ACK_ALT  = 0xF0
RC_NACK     = 0x3C     # Table 2: optional NACK
RC_RESERVED = 0xE1     # Table 2: reserved (formerly BUSY), must be rejected
RC_ID_POM, RC_ID_ADR1, RC_ID_ADR2, RC_ID_DYN = 0, 1, 2, 7

assert len(set(RC_CODE)) == 64 and all(bin(w).count("1") == 4 for w in RC_CODE)


def rc_encode12(datagram_id, data8):
    """12-bit datagram (4-bit ID + 8 data bits) -> two 4/8 code words."""
    v = ((datagram_id & 0x0F) << 8) | (data8 & 0xFF)
    return [RC_CODE[(v >> 6) & 0x3F], RC_CODE[v & 0x3F]]


def rc_encode6(values):
    """Extra 6-bit data words (Channel 2 tail) -> code words."""
    return [RC_CODE[v & 0x3F] for v in values]


def _hex(bs):
    return "".join(f"{b:02X}" for b in bs) if bs else "-"


def _rc_session(port):
    """Open the command UART for the loopback cases (one port for the series)."""
    import serial
    s = serial.Serial(port, lib.SERIAL_BAUD, timeout=0.2)
    time.sleep(0.15)
    s.reset_input_buffer()
    return s


def _rc_cmd(s, cmd, settle=0.15):
    s.reset_input_buffer()
    s.write((cmd + "\r").encode())
    time.sleep(settle)
    return s.read(600).decode(errors="replace")


def _rc_status(s):
    """Parse 'RC STATUS: armed=.. cutouts=.. rx_ok=.. rx_dropped=.. tx=.. results=..'."""
    txt = _rc_cmd(s, "RC STATUS")
    line = next((l for l in txt.splitlines() if "RC STATUS:" in l), "")
    out = {}
    for kv in line.split("RC STATUS:")[-1].split():
        if "=" in kv:
            k, v = kv.split("=", 1)
            out[k] = int(v)
    return out


def _rc_results(s, quiet=0.25, total=1.5):
    """Collect RC RESULT lines until the UART goes quiet. Returns list of dicts."""
    end = time.time() + total
    buf, last = "", time.time()
    while time.time() < end:
        chunk = s.read(512).decode(errors="replace")
        if chunk:
            buf += chunk
            last = time.time()
        elif time.time() - last > quiet and "RC RESULT" in buf:
            break
    _rc_results.last_raw = buf              # kept for diagnostics (see _rc_case "raw")
    res = []
    for line in buf.splitlines():
        if "RC RESULT:" not in line:
            continue
        d = {}
        body = line.split("RC RESULT:")[-1].strip()
        for kv in body.split():
            if "=" in kv:
                k, v = kv.split("=", 1)
                d[k] = v
        d["addr"] = int(d.get("addr", -1)); d["ch"] = int(d.get("ch", 0))
        d["res"] = d.get("res", "?")
        d["id"] = int(d.get("id", -1)); d["n"] = int(d.get("n", 0))
        data = body.split("data=")[-1].strip() if "data=" in body else ""
        d["data"] = [int(x, 16) for x in data.split()] if data else []
        res.append(d)
    return res


def _rc_case(s, ch1, ch2, late=False, seconds=0.3, arm_delay=0.03, pre=None):
    """Arm one mock reply inside a live capture of ch0/ch2/ch5/ch6, then gather the
    DUT's RC RESULT lines and counters. Returns a dict with everything a check needs.
    `pre` (optional, list of UART commands) is written at the start of the stimulus,
    before the arm delay: used to re-issue SPEED commands so the reply rides those
    locomotives' refresh bursts (a cold refresh slot is only kept alive every
    DCC_REFRESH_COLD_CYCLES packets; between keep-alives the stream is idle)."""
    _rc_cmd(s, "RC MOCK OFF", settle=0.1)
    cmd = f"RC MOCK {_hex(ch1)} {_hex(ch2)}" + (" LATE" if late else "")
    chans = [lib.DIGITAL_CHANNEL, CUTOUT_CHANNEL, WINDOW_CHANNEL, LOOPBACK_CHANNEL]
    s.reset_input_buffer()

    def stimulus():
        for p in (pre or []):
            s.write((p + "\r").encode())     # bursts start a few ms from now
        time.sleep(arm_delay)                 # a few cutouts first, then arm
        s.write((cmd + "\r").encode())

    with tempfile.TemporaryDirectory() as d:
        paths = lib.capture_to_csv_multi(chans, d, stimulus=stimulus, capture_seconds=seconds)
        rows = {ch: lib.read_transitions(paths[ch]) for ch in chans}
    results = _rc_results(s)
    status = _rc_status(s)

    frames = lib.decode_uart(rows[LOOPBACK_CHANNEL], baud=RC_BAUD)
    return {"cmd": cmd, "ch1": ch1, "ch2": ch2, "late": late, "rows": rows,
            "frames": frames, "results": results, "status": status,
            "raw": getattr(_rc_results, "last_raw", "")}


def _tagged_address(case):
    """From the capture: the address of the packet whose cutout carried the mock
    bytes (the last decoded packet ending before the first ch6 start bit)."""
    if not case["frames"]:
        return None, None
    t6 = case["frames"][0][0]
    dec = lib.decode(case["rows"][lib.DIGITAL_CHANNEL])
    ends = dec["packet_end_times"]
    i = bisect.bisect_left(ends, t6) - 1
    if i < 0:
        return None, None
    return lib.packet_address(dec["packets"][i][1]), (t6 - ends[i]) * 1e6


def _windows(case):
    """[(open_s, close_s)] of the RAILCOM_RX_WINDOW mirror highs around the mock bytes."""
    rows = case["rows"][WINDOW_CHANNEL]
    out, t_rise = [], None
    for t, v in rows:
        if v == 1:
            t_rise = t
        elif v == 0 and t_rise is not None:
            out.append((t_rise, t)); t_rise = None
    return out


def _wire_checks(rep, case, clause):
    """Saleae-side truth for a WINDOW-mode reply: the bytes on ch6 are the queued
    bytes, framed at 250 kbaud, each inside the right channel window."""
    bit = 1.0 / RC_BAUD
    frames = case["frames"]
    queued = list(case["ch1"]) + list(case["ch2"])
    got = [b for _, b, _ in frames]
    rep.check(clause, f"ch6 carries the queued bytes ({_hex(queued)})",
              got == queued and all(ok for _, _, ok in frames),
              f"decoded {len(frames)} frame(s) on ch6: {_hex(got)}; "
              f"stop bits ok: {all(ok for _, _, ok in frames)}")
    if len(frames) >= 2:
        gaps = [(frames[i + 1][0] - frames[i][0]) * 1e6 for i in range(len(frames) - 1)
                if (frames[i + 1][0] - frames[i][0]) < 60e-6]          # back-to-back only
        rep.check(clause, "250 kbaud framing (40 us per byte, back-to-back)",
                  all(38.0 <= g <= 44.0 for g in gaps) if gaps else False,
                  (f"n={len(gaps)} start-to-start min={min(gaps):.2f} mean={sum(gaps)/len(gaps):.2f} "
                   f"max={max(gaps):.2f} us (limits 38-44)") if gaps else "no back-to-back bytes")
    wins = _windows(case)
    n1 = len(case["ch1"])
    placed = []
    for k, (t0, _, _) in enumerate(frames):
        t_end = t0 + 10 * bit
        w = next(((a, b) for a, b in wins if a <= t0 <= b), None)
        placed.append(w is not None and t_end <= w[1] + 2e-6)
    # Ch1 bytes must all sit in ONE window, Ch2 bytes in the NEXT one
    win_of = [next((i for i, (a, b) in enumerate(wins) if a <= t0 <= b), None)
              for t0, _, _ in frames]
    same1 = len(set(win_of[:n1])) <= 1 and None not in win_of[:n1]
    same2 = len(set(win_of[n1:])) <= 1 and None not in win_of[n1:]
    ordered = (not case["ch1"] or not case["ch2"] or
               (win_of[0] is not None and win_of[n1] is not None and win_of[n1] == win_of[0] + 1))
    rep.check(clause, "every byte starts and ends inside its channel window (ch5)",
              bool(frames) and all(placed) and same1 and same2 and ordered,
              f"{sum(placed)}/{len(placed)} inside a window; Ch1 in window {set(win_of[:n1])}, "
              f"Ch2 in window {set(win_of[n1:])}")


def _expect(rep, clause, name, case, ch, dg_id, data, present=True):
    """Assert the DUT reported a good datagram on channel ch, or (present=False)
    reported nothing at all on channel ch."""
    hits = [r for r in case["results"] if r["ch"] == ch]
    if not present:
        rep.check(clause, name, not hits,
                  f"ch{ch} results: {len(hits)} (expected none); all: {case['results']}")
        return
    ok = (len(hits) == 1 and hits[0]["res"] == "OK" and hits[0]["id"] == dg_id
          and hits[0]["data"] == list(data))
    rep.check(clause, name, ok,
              f"expected ch{ch} res=OK id={dg_id} data={_hex(data)}; got "
              + (f"res={hits[0]['res']} id={hits[0]['id']} n={hits[0]['n']} "
                 f"data={_hex(hits[0]['data'])}" if hits
                 else "no ch%d result" % ch)
              + (f" (+{len(hits) - 1} extra)" if len(hits) > 1 else ""))


def _expect_result(rep, clause, name, case, ch, res):
    """Assert the DUT reported exactly one result on channel ch, with result code
    res (ACK, NACK or an error) and no data."""
    hits = [r for r in case["results"] if r["ch"] == ch]
    ok = len(hits) == 1 and hits[0]["res"] == res and hits[0]["n"] == 0
    rep.check(clause, name, ok,
              f"expected one ch{ch} res={res} n=0; got "
              + (", ".join(f"res={r['res']} n={r['n']}" for r in hits) if hits
                 else "no ch%d result" % ch))


def _address_check(rep, clause, case):
    """The DUT tagged the reply with the address of the packet whose cutout it rode."""
    tagged, dt_us = _tagged_address(case)
    got = {r["addr"] for r in case["results"]}
    rep.check(clause, "reply tagged with the address of the packet before its cutout",
              tagged is not None and got == {tagged},
              f"wire: packet addr {tagged}, bytes {dt_us:.0f} us after its end bit; "
              f"DUT tagged {sorted(got)}" if tagged is not None else "no ch6 bytes captured")


def railcom_loopback_tests(rep, port):
    """S-9.3.2 CS-010..CS-014 on the wire, through the DUT's real receive path."""
    s = _rc_session(port)
    try:
        _rc_cmd(s, "CLEAR", 0.1)
        _rc_cmd(s, "SPEED 3 50 FWD 128", 0.15)        # every packet addressed to 3
        pom_val = 0x2A

        ride3 = ["SPEED 3 50 FWD 128"]       # the reply rides an address-3 burst
        ride3_arm = 0.012                     # arm inside that 3-copy burst (~5-26 ms)

        # --- CS-013: Channel 1 two-byte datagram (ADR1 for a short address) -----
        clause = "S-9.3.2 §3.4 (Channel 1 datagram)"
        c = _rc_case(s, rc_encode12(RC_ID_ADR1, 0x00), [], pre=ride3, arm_delay=ride3_arm)
        # @compliance DCC-S9.3.2-CS-013
        _expect(rep, clause, "Ch1 ADR1 datagram decoded (id=1, data=00)", c, 1, RC_ID_ADR1, [0x00])
        _expect(rep, clause, "no Channel 2 datagram when none was sent", c, 2, 0, [], present=False)
        _wire_checks(rep, c, clause)
        _address_check(rep, clause, c)

        # --- CS-014 + CS-010: Channel 2 POM read-back after a Ch1 ADR2 -------------
        clause = "S-9.3.2 §3.4 (Channel 2 datagram)"
        c = _rc_case(s, rc_encode12(RC_ID_ADR2, 3), rc_encode12(RC_ID_POM, pom_val),
                     pre=ride3, arm_delay=ride3_arm)
        # @compliance DCC-S9.3.2-CS-014
        _expect(rep, clause, f"Ch2 POM read-back decoded (id=0, data={pom_val:02X})", c, 2, RC_ID_POM, [pom_val])
        # @compliance DCC-S9.3.2-CS-010
        _expect(rep, clause, "Ch1 ADR2 decoded alongside it (id=2, data=03)", c, 1, RC_ID_ADR2, [0x03])
        _wire_checks(rep, c, clause)
        _address_check(rep, clause, c)

        # --- CS-014: a 4-byte Channel 2 datagram (DYN: id 7 + data + 2 extra words) --
        c = _rc_case(s, rc_encode12(RC_ID_ADR1, 0x00),
                     rc_encode12(RC_ID_DYN, 0x55) + rc_encode6([0x12, 0x3F]))
        _expect(rep, clause, "4-byte Ch2 datagram: id=7 data=55 12 3F", c, 2, RC_ID_DYN, [0x55, 0x12, 0x3F])
        _wire_checks(rep, c, clause)

        # --- CS-011: ACK padding after the datagram (both ACK forms) ---------------
        clause = "S-9.3.2 §3.3 (ACK code words)"
        c = _rc_case(s, rc_encode12(RC_ID_ADR1, 0x00),
                     rc_encode12(RC_ID_POM, pom_val) + [RC_ACK, RC_ACK_ALT])
        # @compliance DCC-S9.3.2-CS-011
        _expect(rep, clause, "Ch2 datagram kept when followed by ACK 0x0F 0xF0 padding", c, 2, RC_ID_POM, [pom_val])
        _wire_checks(rep, c, clause)
        c = _rc_case(s, rc_encode12(RC_ID_ADR1, 0x00), [RC_ACK, RC_ACK, RC_ACK_ALT, RC_ACK_ALT])
        _expect_result(rep, clause, "all-ACK Channel 2 reported as ACK, no datagram", c, 2, "ACK")
        _expect(rep, clause, "  ...while its Ch1 still decodes", c, 1, RC_ID_ADR1, [0x00])

        # --- CS-012: NACK -----------------------------------------------------------
        clause = "S-9.3.2 §3.3 (NACK code word)"
        c = _rc_case(s, rc_encode12(RC_ID_ADR1, 0x00), [RC_NACK, RC_NACK])
        # @compliance DCC-S9.3.2-CS-012
        _expect_result(rep, clause, "NACK-only Channel 2 reported as NACK, no datagram", c, 2, "NACK")
        _expect(rep, clause, "  ...while its Ch1 still decodes", c, 1, RC_ID_ADR1, [0x00])

        # --- CS-010: invalid / reserved code words are rejected ----------------------
        clause = "S-9.3.2 §3.1 (4/8 code, invalid words)"
        c = _rc_case(s, [RC_CODE[0x01], 0xFF], rc_encode12(RC_ID_POM, pom_val))
        _expect_result(rep, clause, "Ch1 with a non-4/8 byte (0xFF) reported as INVALID_CODEWORD",
                       c, 1, "INVALID_CODEWORD")
        _expect(rep, clause, "  ...Ch2 after it still decodes", c, 2, RC_ID_POM, [pom_val])
        c = _rc_case(s, rc_encode12(RC_ID_ADR1, 0x00), [RC_RESERVED, RC_CODE[0x02]])
        _expect_result(rep, clause, "Ch2 starting with reserved 0xE1 reported as INVALID_CODEWORD",
                       c, 2, "INVALID_CODEWORD")

        # --- receive gate: bytes outside every window never become a datagram --------
        clause = "S-9.3.2 §3.2 (receive gated to the channel windows)"
        c = _rc_case(s, rc_encode12(RC_ID_ADR2, 3), rc_encode12(RC_ID_POM, pom_val), late=True)
        rep.check(clause, "bytes sent at T_CE (gate closed) produce no RC RESULT",
                  not c["results"] and len(c["frames"]) == 4,
                  f"{len(c['frames'])} frames on ch6 after the cutout; results: {c['results']}")
        rep.check(clause, "DUT counted them as dropped-outside-window, none accepted",
                  c["status"].get("rx_dropped", 0) >= 4 and c["status"].get("rx_ok", 0) == 0,
                  f"RC STATUS after: {c['status']}")

        # --- address tagging under a two-loco stream (PR #1 two-stage capture) --------
        clause = "S-9.3.2 §3.4 (reply address tagging)"
        _rc_cmd(s, "SPEED 200 50 FWD 128", 0.15)
        tags_ok, seen = [], []
        for k in range(8):
            # Re-issue both SPEEDs at the start of the stimulus: their bursts then
            # alternate 3 / 200 on the wire (~6 packets, ~45 ms) while the mock is
            # armed. Vary the arm phase against the ~7 ms cadence so the reply lands
            # on packets of BOTH addresses across the trials, not always the same one.
            c = _rc_case(s, rc_encode12(RC_ID_ADR1, 0x00), [], arm_delay=0.03 + k * 0.00175,
                         pre=["SPEED 3 50 FWD 128", "SPEED 200 50 FWD 128"])
            tagged, _ = _tagged_address(c)
            got = {r["addr"] for r in c["results"]}
            seen.append((tagged, sorted(got)))
            tags_ok.append(tagged is not None and got == {tagged})
        addrs_hit = {t for t, _ in seen if t}
        rep.check(clause, "with locos 3 and 200 streaming, each reply carries its own packet's address",
                  all(tags_ok) and addrs_hit == {3, 200},
                  f"(wire addr, DUT tag) per trial: {seen}; addresses exercised: {sorted(addrs_hit)}"
                  + ("" if addrs_hit == {3, 200} else " -- both 3 and 200 must be hit"))

        # --- Channel 2-only reply (issue #7) -----------------------------------------
        # A decoder with the Ch1 broadcast off (CV28 bit 0, draft 5.2.1) sends only
        # Ch2. uart_read tags every byte with the window it arrived in, so the
        # library must report it on Channel 2 and report nothing on Channel 1.
        clause = "S-9.3.2 §3.4 (Channel 2-only reply)"
        c = _rc_case(s, [], rc_encode12(RC_ID_POM, pom_val))
        # @compliance DCC-S9.3.2-CS-014
        _expect(rep, clause, "Ch2-only reply reported on Channel 2", c, 2, RC_ID_POM, [pom_val])
        _expect(rep, clause, "  ...and nothing reported on Channel 1", c, 1, 0, [], present=False)
        _wire_checks(rep, c, clause)

        _rc_cmd(s, "RC MOCK OFF", 0.1)
        _rc_cmd(s, "CLEAR", 0.1)
    finally:
        s.close()

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
        railcom_loopback_tests(rep, port)
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
              f"{lib.AUTOMATION_PORT}), device connected, ch2/PB2, ch5/PB18, ch6/PB16 wired.\n")
        return 2
    path = lib.write_html([rep.as_dict()], lib.report_path("s9_3_2"),
                          title=f"NMRA {SPEC_DOC} Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
