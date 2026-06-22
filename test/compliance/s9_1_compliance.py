#!/usr/bin/env python3
"""
S-9.1 Electrical / Timing compliance test  (hardware-in-the-loop)

Captures the DCC signal on a Saleae logic analyzer, decodes it, and checks the
on-the-wire timing and framing against the NMRA S-9.1 standard (command-station
transmit side). Drives the `saleae_hil_compliance` firmware running on the
MSPM0G3507 LaunchPad.

USAGE
    1. pip install logic2-automation pyserial
    2. In Logic 2: Settings > Automation > enable the Automation API.
    3. Flash + run the saleae_hil_compliance firmware (no CoolTerm needed --
       the script powers the track on itself over UART).
    4. .venv/bin/python s9_1_compliance.py
       -> prints a PASS/FAIL report; process exit code 0 = all pass, 1 = fail.

WIRING
    Saleae digital channel 0  ->  DCC out (PB1)

NOT COVERED BY THIS RIG
    S-9.1 signal voltage levels (min 8.5 V output, etc.) cannot be measured by a
    digital logic analyzer. Those clauses need a scope / meter and are reported
    as "NOT TESTABLE" rather than silently skipped.
"""

import os
import sys
import tempfile

# ----------------------------------------------------------------------------
# CONFIG  -- edit these for your bench
# ----------------------------------------------------------------------------
SALEAE_DEVICE_ID   = "C62F3A57A5F4E3A0"   # `get_devices`; "" = first physical device
DIGITAL_CHANNEL    = 0                     # Saleae channel clipped to DCC out (PB1)
SAMPLE_RATE_HZ     = 24_000_000            # original Logic max; ~42 ns resolution
CAPTURE_SECONDS    = 0.05                  # 50 ms -> ~20-30 packets
AUTOMATION_PORT    = 10430                 # logic2-automation gRPC default (NOT the MCP 10530)

# UART control: the script powers on the track itself -- no CoolTerm needed.
DRIVE_UART         = True       # send POWER ON over UART before capturing
SERIAL_PORT        = ""         # "" = auto-detect DUT among /dev/cu.usbmodem*; or pin one
SERIAL_BAUD        = 230_400
POWER_OFF_AFTER    = False      # leave the track powered after the test

# ----------------------------------------------------------------------------
# S-9.1 limits  (each tagged with its spec clause; see DCC_Spec_Reference.md)
# ----------------------------------------------------------------------------
ONE_HALF_MIN_US      = 55.0     # S-9.1 Tbl 2.1: one-bit half 58 us nominal (55-61)
ONE_HALF_MAX_US      = 61.0
ZERO_HALF_MIN_US     = 95.0     # S-9.1 Tbl 2.1: zero-bit half >= 95 us (CS transmit)
ZERO_BIT_TOTAL_MAX_US = 12000.0 # S-9.1: total zero-bit duration must not exceed 12000 us
INTRA_BIT_SYMMETRY_US = 3.0     # S-9.1: the two halves of a bit must be ~equal
PREAMBLE_MIN_BITS    = 14       # S-9.2: "A command station must send a minimum of 14 full preamble bits"

# classification thresholds (us) -- a half is a "one" if short, "zero" if long
SHORT_LONG_BOUNDARY_US = 78.0   # midway between 58 and ~100/116
LONG_MAX_PLAUSIBLE_US  = 12000.0

# A finite capture window arms/stops mid-bit, leaving a truncated partial bit
# (a "runt") at each end. These are measurement boundary artifacts, not DCC.
# Trim a few transitions off each end before analysis. Any runt that survives
# in the INTERIOR is a real anomaly and is reported (GLITCH_FLOOR_US check).
BOUNDARY_TRIM_EDGES = 6          # transitions discarded at each capture edge
GLITCH_FLOOR_US     = 40.0       # below this is no valid DCC half-bit -> a glitch


# ----------------------------------------------------------------------------
# Capture
# ----------------------------------------------------------------------------
def send_command(port, cmd, settle=0.3):
    """Open the DUT UART, send one command line (CR-terminated), return reply text."""
    import time
    import serial
    with serial.Serial(port, SERIAL_BAUD, timeout=0.5) as s:
        time.sleep(0.15)
        s.reset_input_buffer()
        s.write((cmd + "\r").encode())
        time.sleep(settle)
        return s.read(400).decode(errors="replace")


def find_dut_port():
    """Probe candidate ports with the read-only STATUS command; return the one
    the DCC firmware answers on. Harmless to the non-DUT port (bytes go nowhere)."""
    import serial.tools.list_ports
    candidates = [SERIAL_PORT] if SERIAL_PORT else []
    candidates += [p.device for p in serial.tools.list_ports.comports()
                   if "usbmodem" in p.device.lower()]
    seen = set()
    ordered = [c for c in candidates if not (c in seen or seen.add(c))]
    for port in ordered:
        try:
            resp = send_command(port, "STATUS")
            if "STATUS:" in resp or "DCC" in resp.upper():
                return port
        except Exception:
            continue
    return None


def start_dcc():
    """Find the DUT, power the track on, return the port for later use."""
    port = find_dut_port()
    if not port:
        raise RuntimeError(
            "could not find the DUT UART among /dev/cu.usbmodem* "
            "(is the firmware running? set SERIAL_PORT to pin it)")
    reply = send_command(port, "POWER ON")
    last = reply.strip().splitlines()[-1] if reply.strip() else "(no reply)"
    print(f"[uart] {port}: POWER ON -> {last}")
    return port


def capture_to_csv(out_dir):
    """Run a timed capture and export digital.csv into out_dir. Returns its path."""
    from saleae import automation

    dev_cfg = automation.LogicDeviceConfiguration(
        enabled_digital_channels=[DIGITAL_CHANNEL],
        digital_sample_rate=SAMPLE_RATE_HZ,
        # original Logic has a fixed threshold -- do NOT set digital_threshold_volts
    )
    cap_cfg = automation.CaptureConfiguration(
        capture_mode=automation.TimedCaptureMode(duration_seconds=CAPTURE_SECONDS)
    )

    with automation.Manager.connect(port=AUTOMATION_PORT) as mgr:
        print(f"[saleae] starting {CAPTURE_SECONDS*1e3:.0f} ms capture "
              f"@ {SAMPLE_RATE_HZ/1e6:.0f} MS/s on ch{DIGITAL_CHANNEL}")
        with mgr.start_capture(
            device_id=SALEAE_DEVICE_ID or None,
            device_configuration=dev_cfg,
            capture_configuration=cap_cfg,
        ) as cap:
            cap.wait()
            cap.export_raw_data_csv(directory=out_dir,
                                    digital_channels=[DIGITAL_CHANNEL])
    return os.path.join(out_dir, "digital.csv")


# ----------------------------------------------------------------------------
# Decode: CSV transitions -> half-bit widths -> bits -> packets
# ----------------------------------------------------------------------------
def read_transitions(csv_path):
    import csv
    rows = []
    with open(csv_path) as f:
        r = csv.reader(f)
        next(r)  # header: "Time [s],Channel N"
        for line in r:
            if len(line) >= 2:
                rows.append((float(line[0]), int(line[1])))
    return rows


def decode(rows):
    """
    Returns dict with:
      bit_halves: list of (half_a_us, half_b_us, bit_char) for each paired bit
                  -- only halves that formed a real bit (artifact-trimmed)
      bits:       reconstructed bit string
      packets:    list of (preamble_len, [bytes])
    """
    # half-period widths between successive transitions (us)
    widths = []
    for i in range(1, len(rows)):
        widths.append((rows[i][0] - rows[i - 1][0]) * 1e6)

    # Trim the truncated partial bits at each capture edge (boundary artifacts).
    trimmed_runts = 0
    if len(widths) > 2 * BOUNDARY_TRIM_EDGES:
        edge = widths[:BOUNDARY_TRIM_EDGES] + widths[-BOUNDARY_TRIM_EDGES:]
        trimmed_runts = sum(1 for w in edge if w < GLITCH_FLOOR_US)
        widths = widths[BOUNDARY_TRIM_EDGES:-BOUNDARY_TRIM_EDGES]

    # Any sub-floor pulse remaining is INTERIOR -> a real glitch, not a boundary
    # artifact. Counted and reported; never silently dropped.
    interior_glitches = [w for w in widths if w < GLITCH_FLOOR_US]

    def cls(w):
        if w < SHORT_LONG_BOUNDARY_US:
            return "1"
        if w <= LONG_MAX_PLAUSIBLE_US:
            return "0"
        return "X"

    classes = [cls(w) for w in widths]

    # pair consecutive same-class halves into one DCC bit
    bit_halves = []
    bits = []
    i = 0
    while i < len(classes) - 1:
        a, b = classes[i], classes[i + 1]
        if a == b and a in "01":
            bit_halves.append((widths[i], widths[i + 1], a))
            bits.append(a)
            i += 2
        else:
            i += 1  # resync at packet boundary / glitch

    bitstr = "".join(bits)

    # framing: preamble (>=12 ones) 0 {byte 0}* {byte} 1
    packets = []
    n = len(bitstr)
    i = 0
    while i < n:
        if bitstr[i] == "1":
            j = i
            while j < n and bitstr[j] == "1":
                j += 1
            run = j - i
            if run >= 12 and j < n and bitstr[j] == "0":
                k = j
                data = []
                complete = False           # did we reach a proper end bit?
                while k < n and bitstr[k] == "0":
                    k += 1
                    if k + 8 > n:
                        break              # ran off buffer end -> truncated
                    data.append(int(bitstr[k:k + 8], 2))
                    k += 8
                    if k < n and bitstr[k] == "1":
                        k += 1
                        complete = True    # end bit present -> full packet
                        break
                # only count fully-framed packets; a packet truncated by the
                # capture window (no end bit) is a boundary artifact, not data.
                if data and complete:
                    packets.append((run, data))
                i = k
                continue
        i += 1

    return {"bit_halves": bit_halves, "bits": bitstr, "packets": packets,
            "trimmed_runts": trimmed_runts, "interior_glitches": interior_glitches}


# ----------------------------------------------------------------------------
# S-9.1 checks  -> report
# ----------------------------------------------------------------------------
class Report:
    def __init__(self):
        self.rows = []
        self.failed = False

    def check(self, clause, name, ok, detail):
        status = "PASS" if ok else "FAIL"
        if not ok:
            self.failed = True
        self.rows.append((status, clause, name, detail))

    def na(self, clause, name, detail):
        self.rows.append(("N/A ", clause, name, detail))

    def render(self):
        print("\n" + "=" * 78)
        print("  S-9.1 ELECTRICAL / TIMING COMPLIANCE  (command-station transmit)")
        print("=" * 78)
        for status, clause, name, detail in self.rows:
            print(f"  [{status}] {clause:<14} {name}")
            print(f"         {detail}")
        print("=" * 78)
        print("  RESULT:", "FAIL" if self.failed else "PASS")
        print("=" * 78 + "\n")


def stats(xs):
    return (min(xs), sum(xs) / len(xs), max(xs)) if xs else (0, 0, 0)


def run_checks(decoded):
    rep = Report()
    halves = decoded["bit_halves"]
    ones = [w for (a, b, c) in halves if c == "1" for w in (a, b)]
    zeros = [w for (a, b, c) in halves if c == "0" for w in (a, b)]
    packets = decoded["packets"]
    interior = decoded["interior_glitches"]

    # 0. interior glitches (boundary runts already trimmed; these are real)
    rep.check("HIL", f"no interior glitch pulses (< {GLITCH_FLOOR_US:.0f} us)",
              len(interior) == 0,
              f"{decoded['trimmed_runts']} boundary runt(s) trimmed; "
              f"{len(interior)} interior glitch(es)"
              + (f" -> {[round(w,2) for w in interior]}" if interior else ""))

    # 1. ONE half-bit timing
    if ones:
        lo, mean, hi = stats(ones)
        ok = all(ONE_HALF_MIN_US <= w <= ONE_HALF_MAX_US for w in ones)
        rep.check("S-9.1 Tbl2.1", "ONE half-bit 55-61 us (nom 58)", ok,
                  f"n={len(ones)} min={lo:.2f} mean={mean:.3f} max={hi:.2f} us")
    else:
        rep.check("S-9.1 Tbl2.1", "ONE half-bit", False, "no one-bits decoded")

    # 2. ZERO half-bit timing + total duration
    if zeros:
        lo, mean, hi = stats(zeros)
        ok = all(w >= ZERO_HALF_MIN_US for w in zeros)
        rep.check("S-9.1 Tbl2.1", "ZERO half-bit >= 95 us", ok,
                  f"n={len(zeros)} min={lo:.2f} mean={mean:.3f} max={hi:.2f} us")
        total_ok = all((a + b) <= ZERO_BIT_TOTAL_MAX_US
                       for (a, b, c) in halves if c == "0")
        rep.check("S-9.1", "ZERO total bit <= 12000 us", total_ok,
                  f"max zero-bit total = {max((a+b) for (a,b,c) in halves if c=='0'):.2f} us")
    else:
        rep.check("S-9.1 Tbl2.1", "ZERO half-bit", False, "no zero-bits decoded")

    # 3. intra-bit symmetry
    if halves:
        worst = max(abs(a - b) for (a, b, c) in halves)
        ok = worst <= INTRA_BIT_SYMMETRY_US
        rep.check("S-9.1", f"intra-bit symmetry <= {INTRA_BIT_SYMMETRY_US} us", ok,
                  f"worst half-pair delta = {worst:.2f} us")

    # 4. preamble length.
    #    - Exclude the FIRST packet: its leading run of ones is inflated by
    #      idle/power-on line time, not a true preamble (head boundary).
    #    - Strip the merged prior end bit: between packets the run of ones is
    #      end-bit(1) + preamble, and S-9.2 footnote 3 lets the end bit count as
    #      a preamble bit. We test the CS's duty to send 14 *preamble* bits, so
    #      attribute the shared bit to the terminator -> preamble = run - 1.
    interior_pk = packets[1:]
    if interior_pk:
        preambles = [run - 1 for (run, _) in interior_pk]
        min_pre = min(preambles)
        ok = min_pre >= PREAMBLE_MIN_BITS
        rep.check("S-9.2", f"preamble >= {PREAMBLE_MIN_BITS} ones (CS transmit)", ok,
                  f"min preamble = {min_pre} over {len(interior_pk)} packets "
                  f"(first excluded: idle-inflated; end bit stripped)")
    else:
        rep.check("S-9.2", "preamble", False, "no interior packets to measure")

    # 5/6. framing + XOR (a decoded packet implies start/sep/end framing held)
    if packets:
        bad = 0
        for _, data in packets:
            x = 0
            for byte in data:
                x ^= byte
            if x != 0:
                bad += 1
        rep.check("S-9.2", "packet framing + XOR error byte", bad == 0,
                  f"{len(packets)} packets, {bad} XOR failures")
    else:
        rep.check("S-9.2", "packet framing + XOR", False, "no packets decoded")

    # signal levels -- not measurable on a digital LA
    rep.na("S-9.1 §2", "signal voltage levels (>=8.5 V out)",
           "requires scope/meter or analog capture -- not coverable by this rig")
    return rep


def main():
    try:
        port = start_dcc() if DRIVE_UART else None
        with tempfile.TemporaryDirectory() as d:
            csv_path = capture_to_csv(d)
            rows = read_transitions(csv_path)
            print(f"[decode] {len(rows)} transitions captured")
            decoded = decode(rows)
            print(f"[decode] {len(decoded['bits'])} bits, "
                  f"{len(decoded['packets'])} packets")
            rep = run_checks(decoded)
            rep.render()
            if DRIVE_UART and POWER_OFF_AFTER and port:
                print(f"[uart] {port}: POWER OFF -> "
                      f"{send_command(port, 'POWER OFF').strip().splitlines()[-1:]}")
            return 1 if rep.failed else 0
    except ImportError as e:
        print(f"\nMissing dependency: {e}\n"
              f"Run: pip install logic2-automation pyserial\n")
        return 2
    except Exception as e:
        print(f"\nERROR: {e}\n"
              f"Check: Logic 2 running, Automation API enabled (port {AUTOMATION_PORT}), "
              f"device connected, DCC powered on.\n")
        return 2


if __name__ == "__main__":
    sys.exit(main())
