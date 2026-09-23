#!/usr/bin/env python3
"""
Bench preflight -- is the command-station HIL rig wired, and is the Saleae reachable?

Not a spec test. Run it after re-wiring the bench (or before a full run) so a loose
probe or a closed Logic 2 shows up HERE, with the wire colour and header pin named,
instead of as a mysterious failure deep inside a spec suite.

Checks each layer in order and stops at the first one that fails:
  1. DUT UART  -- the port is found and the firmware answers HELP with the HIL command
                  set (distinguishes the CS firmware from the waveform player / example app).
  2. Saleae    -- Logic 2 Automation API reachable on AUTOMATION_PORT, the pinned device
                  attached.
  3. Wiring    -- two short captures on all six channels while the harness drives the DUT.
                  Each channel must carry the signal its pin produces, not just "toggle":
                    D0 decodes as main-track DCC packets
                    D1 shows the one TRIG pulse
                    D2 shows one cutout-width strobe per packet, aligned to D0 packet ends
                    D5 shows the two Rx-window pulses inside each D2 strobe
                    D3 decodes as service-mode packets (>= 20-bit preamble)
                    D4 shows the 6 ms mock-ACK pulse (and the DUT reports ACK DETECTED)

Run from test/compliance/:      .venv/bin/python command_station/bench_preflight.py
As the first step of a full run: .venv/bin/python command_station/run_all.py --preflight
Exit: 0 = bench OK, 1 = a wiring/signal check failed, 2 = setup error (no DUT / no Saleae).

Leaves the DUT with service mode exited, the scheduler cleared and track power OFF.
"""

import sys
import time
import bisect
import tempfile
import statistics

import compliance_lib as lib

# --- channel map: (saleae ch, wire colour, MCU pin, LaunchPad header, signal) ----------
# Mirrors HIL_SETUP.md §2 + "LaunchPad header locations". Keep the two in sync.
CHANNELS = {
    0: ("black",  "PB1",  "J4.39", "main-track DCC (MAIN_DCC)"),
    1: ("brown",  "PB3",  "J1.10", "test trigger (PACKET_LOAD)"),
    2: ("red",    "PB2",  "J1.9",  "RailCom cutout strobe (RAILCOM_CUTOUT)"),
    3: ("orange", "PB4",  "J4.40", "service-track DCC (SERVICE_MODE_DCC)"),
    4: ("yellow", "PB9",  "J1.7",  "mock-ACK in (MOCK_ACK; jumper from PB24/J1.6)"),
    5: ("green",  "PB18", "J3.25", "RailCom Rx-window mirror (RAILCOM_RX_WINDOW)"),
}
MAIN_CH, TRIG_CH, CUTOUT_CH, SVC_CH, ACK_CH, WINDOW_CH = 0, 1, 2, 3, 4, 5

# --- expectations (loose: this is a wiring check, the spec suites do the precision) -----
MAIN_CAPTURE_S      = 1.0      # CLEAR + SPEED + TRIG land inside; ~100 packets
SVC_CAPTURE_S       = 1.0      # SVC MOCKACK op: reset burst + verify + 6 ms pulse
MIN_MAIN_PACKETS    = 5
TRIG_PULSES_MIN     = 1        # one TRIG arm -> one pulse
TRIG_PULSES_MAX     = 5        # more than this = the probe is on a DCC pin, not PB3
CUTOUT_US           = (300.0, 700.0)   # PB2 high ~442 us (T_CE - T_CS at the calibrated timing)
WINDOW_US           = (40.0, 400.0)    # Ch1 ~97 us, Ch2 ~263 us
CUTOUT_AFTER_END_US = 150.0    # a D2 rise must follow a D0 packet end within this
MOCK_ACK_US         = 6000
MOCK_ACK_TOL_US     = 200
SVC_PREAMBLE_MIN    = 20

MOCK_ACK_ARMED = "SVC MOCKACK:"


class SetupError(RuntimeError):
    """A layer below the wiring failed (no DUT, no Saleae): nothing else can be checked."""


# ----------------------------------------------------------------------------
# helpers
# ----------------------------------------------------------------------------
def _label(ch):
    wire, pin, hdr, sig = CHANNELS[ch]
    return f"D{ch} {wire}", f"{pin} ({hdr}) {sig}"


def _read_until_quiet(s, quiet=0.3, total=3.0):
    """Drain the UART until it has been silent for `quiet` s (or `total` s elapsed)."""
    end = time.time() + total
    buf, last = "", time.time()
    while time.time() < end:
        chunk = s.read(512).decode(errors="replace")
        if chunk:
            buf += chunk
            last = time.time()
        elif time.time() - last > quiet:
            break
    return buf


def _high_pulses(rows):
    """[(t_rise_s, width_us)] for every logic-high run in a transition list."""
    out, t_rise = [], None
    for t, v in rows:
        if v == 1 and t_rise is None:
            t_rise = t
        elif v == 0 and t_rise is not None:
            out.append((t_rise, (t - t_rise) * 1e6))
            t_rise = None
    return out


def _in(lo_hi, w):
    return lo_hi[0] <= w <= lo_hi[1]


def _wstats(ws):
    if not ws:
        return "none"
    return (f"n={len(ws)} min={min(ws):.0f} median={statistics.median(ws):.0f} "
            f"max={max(ws):.0f} us")


# ----------------------------------------------------------------------------
# 1. DUT UART
# ----------------------------------------------------------------------------
def check_dut(rep):
    import serial

    port = lib.find_dut_port()
    if not port:
        raise SetupError("no DUT answered STATUS on any /dev/cu.usbmodem* port -- LaunchPad "
                         "plugged in? CS firmware flashed? (the waveform player answers ID?, "
                         "not STATUS; set SERIAL_PORT to pin a port)")

    with serial.Serial(port, lib.SERIAL_BAUD, timeout=0.2) as s:
        time.sleep(0.15)
        s.reset_input_buffer()
        s.write(b"HELP\r")
        help_text = _read_until_quiet(s)
        s.write(b"STATUS\r")
        status = _read_until_quiet(s, total=1.0).strip()

    n_lines = len(help_text.splitlines())
    is_hil = "MOCKACK" in help_text and "TRIG" in help_text
    rep.check("DUT UART", f"{port} answers HELP with the HIL command set", is_hil,
              f"{n_lines} HELP lines; MOCKACK {'present' if 'MOCKACK' in help_text else 'MISSING'}, "
              f"TRIG {'present' if 'TRIG' in help_text else 'MISSING'}; "
              f"{status.splitlines()[-1] if status else '(no STATUS reply)'}")
    if not is_hil:
        raise SetupError(f"{port} is a DCC firmware but not the saleae_hil_compliance "
                         f"command-station build (HELP lacks SVC MOCKACK / TRIG) -- reflash "
                         f"the command_station/ project")
    return port


# ----------------------------------------------------------------------------
# 2. Saleae
# ----------------------------------------------------------------------------
def check_saleae(rep):
    from saleae import automation

    try:
        with automation.Manager.connect(port=lib.AUTOMATION_PORT) as mgr:
            devices = mgr.get_devices()
    except Exception as e:
        raise SetupError(f"Logic 2 Automation API not reachable on port {lib.AUTOMATION_PORT} "
                         f"({type(e).__name__}: {e}) -- Logic 2 running? Preferences > "
                         f"Automation > enable API?")

    physical = [d for d in devices if not d.is_simulation]
    ids = ", ".join(f"{d.device_id} ({d.device_type.name})" for d in physical) or "none"
    rep.check("Saleae", "a physical Logic analyzer is attached", bool(physical),
              f"devices: {ids}")
    if not physical:
        raise SetupError("Logic 2 is running but reports no physical device -- Saleae USB?")

    if lib.SALEAE_DEVICE_ID:
        present = any(d.device_id == lib.SALEAE_DEVICE_ID for d in physical)
        rep.check("Saleae", f"pinned device {lib.SALEAE_DEVICE_ID} is the one attached",
                  present, f"attached: {ids}")
        if not present:
            raise SetupError(f"attached Saleae is not SALEAE_DEVICE_ID={lib.SALEAE_DEVICE_ID} "
                             f"(compliance_lib.py) -- different analyzer? update the id")


# ----------------------------------------------------------------------------
# 3a. main-track side: D0 DCC, D1 trigger, D2 cutout, D5 Rx-window  (one capture)
# ----------------------------------------------------------------------------
def check_main_side(rep, port):
    def stimulus():
        lib.send_command(port, "CLEAR",             settle=0.02)
        lib.send_command(port, "SPEED 3 50 FWD 128", settle=0.02)   # non-idle stream
        lib.send_command(port, "TRIG",              settle=0.02)   # -> one PB3 pulse

    chans = [MAIN_CH, TRIG_CH, CUTOUT_CH, WINDOW_CH]
    with tempfile.TemporaryDirectory() as d:
        paths = lib.capture_to_csv_multi(chans, d, stimulus=stimulus,
                                         capture_seconds=MAIN_CAPTURE_S)
        rows = {ch: lib.read_transitions(paths[ch]) for ch in chans}

    # --- D0: decodes as DCC ------------------------------------------------
    dec = lib.decode(rows[MAIN_CH])
    pk = dec["packets"]
    ones = sum(1 for (_, _, c) in dec["bit_halves"] if c == "1")
    zeros = sum(1 for (_, _, c) in dec["bit_halves"] if c == "0")
    non_idle = sum(1 for (_, data) in pk if list(data) != [0xFF, 0x00, 0xFF])
    clause, name = _label(MAIN_CH)
    ok0 = len(pk) >= MIN_MAIN_PACKETS and ones > 0 and zeros > 0
    rep.check(clause, f"{name}: decodes as DCC packets", ok0,
              f"{len(rows[MAIN_CH])} transitions, {len(pk)} packets ({non_idle} non-idle), "
              f"{ones} one-bits / {zeros} zero-bits"
              + ("" if rows[MAIN_CH] else " -- NO EDGES: black wire on PB1/J4.39? common GND?"))

    # --- D1: exactly the one TRIG pulse ------------------------------------
    p1 = _high_pulses(rows[TRIG_CH])
    clause, name = _label(TRIG_CH)
    ok1 = TRIG_PULSES_MIN <= len(p1) <= TRIG_PULSES_MAX
    hint = ""
    if not p1:
        hint = " -- no pulse: brown wire on PB3/J1.10? (TRIG armed while SPEED streamed)"
    elif len(p1) > TRIG_PULSES_MAX:
        hint = " -- toggling continuously: probe on a DCC pin, not PB3?"
    rep.check(clause, f"{name}: one TRIG pulse", ok1,
              f"{len(p1)} pulse(s): {_wstats([w for _, w in p1])}{hint}")

    # --- D2: cutout-width strobe, one per packet, right after the end bit ----
    p2 = _high_pulses(rows[CUTOUT_CH])
    w2 = [w for _, w in p2]
    in2 = [w for w in w2 if _in(CUTOUT_US, w)]
    clause, name = _label(CUTOUT_CH)
    ok2 = len(in2) >= MIN_MAIN_PACKETS and len(in2) >= 0.8 * len(w2)
    hint = ""
    if not rows[CUTOUT_CH]:
        hint = " -- no edges: red wire on PB2/J1.9?"
    elif w2 and not in2:
        hint = " -- edges but wrong width: red/green swapped? probe on a DCC pin?"
    rep.check(clause, f"{name}: cutout-width strobes ({CUTOUT_US[0]:.0f}-{CUTOUT_US[1]:.0f} us)",
              ok2, f"{len(in2)}/{len(w2)} in range; {_wstats(w2)}{hint}")

    # cross-check D0 <-> D2: each decoded packet end is followed by a strobe rise
    ends = dec.get("packet_end_times", [])
    rises = sorted(t for t, _ in p2)
    matched = 0
    for te in ends:
        # first rise at/after this end
        i = bisect.bisect_left(rises, te - 5e-6)
        if i < len(rises) and (rises[i] - te) * 1e6 <= CUTOUT_AFTER_END_US:
            matched += 1
    frac = matched / len(ends) if ends else 0.0
    rep.check("D0 <-> D2", f"cutout strobe follows each D0 packet end (within {CUTOUT_AFTER_END_US:.0f} us)",
              bool(ends) and frac >= 0.8,
              f"{matched}/{len(ends)} packet ends matched ({frac*100:.0f}%)"
              + ("" if frac >= 0.8 or not ends else
                 " -- D0 and D2 disagree: is D0 on the MAIN track (PB1), not service (PB4)?"))

    # --- D5: two Rx-window pulses per cutout, inside the D2 strobe -----------
    p5 = _high_pulses(rows[WINDOW_CH])
    w5 = [w for _, w in p5]
    in5 = [w for w in w5 if _in(WINDOW_US, w)]
    clause, name = _label(WINDOW_CH)
    ok5 = len(in5) >= 2 * MIN_MAIN_PACKETS and len(in5) >= 0.8 * len(w5)
    hint = ""
    if not rows[WINDOW_CH]:
        hint = (" -- no edges: green wire on PB18/J3.25? or firmware built before the "
                "RAILCOM_RX_WINDOW pin existed (rebuild + reflash)")
    elif w5 and not in5:
        hint = " -- edges but wrong width: red/green swapped?"
    rep.check(clause, f"{name}: Rx-window pulses ({WINDOW_US[0]:.0f}-{WINDOW_US[1]:.0f} us)",
              ok5, f"{len(in5)}/{len(w5)} in range; {_wstats(w5)}{hint}")

    # cross-check D2 <-> D5: every window pulse starts while the cutout strobe is high
    hi2 = [(t, t + w * 1e-6) for t, w in p2]
    starts2 = [a for a, _ in hi2]
    inside = 0
    for t5, _ in p5:
        i = bisect.bisect_right(starts2, t5) - 1
        if i >= 0 and hi2[i][0] <= t5 <= hi2[i][1]:
            inside += 1
    frac = inside / len(p5) if p5 else 0.0
    rep.check("D2 <-> D5", "Rx-window pulses sit inside the cutout strobe",
              bool(p5) and frac >= 0.9,
              f"{inside}/{len(p5)} window pulses inside a D2 high ({frac*100:.0f}%)")

    lib.send_command(port, "CLEAR", settle=0.05)
    return ok0 and ok1 and ok2 and ok5


# ----------------------------------------------------------------------------
# 3b. service side: D3 service-track DCC, D4 mock-ACK  (one capture, in service mode)
# ----------------------------------------------------------------------------
def check_service_side(rep, port):
    import serial

    with serial.Serial(port, lib.SERIAL_BAUD, timeout=0.3) as s:
        time.sleep(0.15)
        s.reset_input_buffer()
        s.write(b"SVC ENTER\r")
        entered = _read_until_quiet(s, total=1.0)
        rep.check("DUT UART", "SVC ENTER accepted", "OK" in entered,
                  entered.strip().splitlines()[-1] if entered.strip() else "(no reply)")

        s.reset_input_buffer()

        def stimulus():
            s.write(f"SVC MOCKACK {MOCK_ACK_US}\r".encode())

        chans = [SVC_CH, ACK_CH]
        try:
            with tempfile.TemporaryDirectory() as d:
                paths = lib.capture_to_csv_multi(chans, d, stimulus=stimulus,
                                                 capture_seconds=SVC_CAPTURE_S)
                rows = {ch: lib.read_transitions(paths[ch]) for ch in chans}
            # the op's async verdict -- wait for it so the singleton is idle before EXIT
            end = time.time() + 30.0
            buf = ""
            while time.time() < end and MOCK_ACK_ARMED not in buf and "failed" not in buf:
                buf += s.read(256).decode(errors="replace")
        finally:
            s.write(b"SVC EXIT\r"); time.sleep(0.3)
            s.write(b"CLEAR\r");    time.sleep(0.1)
            s.write(b"POWER OFF\r"); time.sleep(0.2)
            s.read(1024)

    verdict = next((l.strip() for l in buf.splitlines() if MOCK_ACK_ARMED in l), "(no verdict)")

    # --- D3: decodes as service-mode DCC (long preamble) ----------------------
    dec = lib.decode(rows[SVC_CH])
    pk = dec["packets"]
    max_pre = max((run for run, _ in pk), default=0)
    clause, name = _label(SVC_CH)
    ok3 = len(pk) >= 1 and max_pre >= SVC_PREAMBLE_MIN
    hint = ""
    if not rows[SVC_CH]:
        hint = " -- no edges: orange wire on PB4/J4.40? (service mode was entered, op ran)"
    elif pk and max_pre < SVC_PREAMBLE_MIN:
        hint = " -- short preambles: probe on the MAIN track (PB1), not service (PB4)?"
    rep.check(clause, f"{name}: decodes as service-mode packets (preamble >= {SVC_PREAMBLE_MIN})",
              ok3, f"{len(rows[SVC_CH])} transitions, {len(pk)} packets, "
                   f"longest preamble {max_pre} bits{hint}")

    # --- D4: the one 6 ms mock-ACK pulse, and the DUT saw it too --------------
    p4 = _high_pulses(rows[ACK_CH])
    longest = max((w for _, w in p4), default=0.0)
    clause, name = _label(ACK_CH)
    ok4 = abs(longest - MOCK_ACK_US) <= MOCK_ACK_TOL_US
    detected = "ACK DETECTED" in verdict
    hint = ""
    if not p4 and detected:
        hint = " -- DUT saw the ACK but D4 did not: yellow wire on PB9/J1.7?"
    elif not p4 and not detected:
        hint = " -- neither D4 nor the DUT saw a pulse: PB24->PB9 jumper (J1.6->J1.7) missing?"
    elif p4 and not detected:
        hint = " -- D4 saw a pulse the DUT did not: firmware/ACK path, not wiring"
    rep.check(clause, f"{name}: {MOCK_ACK_US} us mock-ACK pulse (+/-{MOCK_ACK_TOL_US})",
              ok4, f"longest high {longest:.0f} us ({len(p4)} pulse(s)); DUT: {verdict}{hint}")
    rep.check("DUT UART", "DUT reports ACK DETECTED for the mock pulse", detected, verdict)

    return ok3 and ok4 and detected


# ----------------------------------------------------------------------------
def _summary(rep):
    """Per-channel table with the wire colour and header pin, for the bench."""
    status = {}
    for c in rep.checks:
        cl = c["clause"]
        if cl.startswith("D") and " " in cl and "<->" not in cl:
            ch = int(cl[1])
            status[ch] = "FAIL" if c["status"] == "FAIL" or status.get(ch) == "FAIL" else "PASS"
    print("\n  ch  wire     pin    header  signal                                   result")
    print("  --  -------  -----  ------  ---------------------------------------  ------")
    for ch, (wire, pin, hdr, sig) in CHANNELS.items():
        print(f"  D{ch}  {wire:<7}  {pin:<5}  {hdr:<6}  {sig:<39}  {status.get(ch, 'n/a')}")
    print()


def run():
    rep = lib.Report("BENCH", "Command-station HIL rig preflight",
                     "command_station/HIL_SETUP.md",
                     "DUT UART, Logic 2 / Saleae, and all six probe channels")
    port = check_dut(rep)          # raises SetupError -> nothing else is meaningful
    check_saleae(rep)              # raises SetupError
    print(f"[uart] {port}: POWER ON")
    lib.send_command(port, "POWER ON")
    check_main_side(rep, port)
    check_service_side(rep, port)  # also powers the track OFF on exit
    rep.finish()
    _summary(rep)
    return rep


def main():
    print("\n#### HIL BENCH PREFLIGHT (command-station rig) ####")
    try:
        rep = run()
    except ImportError as e:
        print(f"\nMissing dependency: {e}\nRun: pip install logic2-automation pyserial "
              f"(into test/compliance/.venv)\n")
        return 2
    except SetupError as e:
        print(f"\nBENCH NOT READY: {e}\n")
        return 2
    except Exception as e:
        print(f"\nERROR: {type(e).__name__}: {e}\n")
        return 2
    path = lib.write_html([rep.as_dict()], lib.report_path("bench_preflight"),
                          title="HIL Bench Preflight")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
