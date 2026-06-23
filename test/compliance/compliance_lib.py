#!/usr/bin/env python3
"""
Shared plumbing for the DCC hardware-in-the-loop compliance suites.

Provides bench config, UART control of the DUT, Saleae capture, DCC signal
decode, a live-streaming Report collector, and HTML report generation.

Each per-spec script (s9_1_compliance.py, ...) imports this module, defines its
own checks, and is BOTH runnable standalone AND importable by run_all.py.

Requires: pip install logic2-automation pyserial   (into test/compliance/.venv)
"""

import os
import csv
import html
import math
import tempfile
import datetime

# ----------------------------------------------------------------------------
# Bench config  -- edit for your setup
# ----------------------------------------------------------------------------
SALEAE_DEVICE_ID = "C62F3A57A5F4E3A0"   # `get_devices`; "" = first physical device
DIGITAL_CHANNEL  = 0                     # Saleae channel on DCC out (PB1)
TRIGGER_CHANNEL  = 1                     # Saleae channel on the test trigger (PB3 / DEBUG)
SAMPLE_RATE_HZ   = 24_000_000            # original Logic max (~42 ns resolution)
CAPTURE_SECONDS  = 0.05                  # 50 ms -> ~20 packets
AUTOMATION_PORT  = 10430                 # logic2-automation gRPC (NOT the MCP 10530)

DRIVE_UART       = True                  # power the track on over UART (no CoolTerm)
SERIAL_PORT      = ""                    # "" = auto-detect DUT among /dev/cu.usbmodem*
SERIAL_BAUD      = 230_400
POWER_OFF_AFTER  = False

# Decode params (generic to all specs)
SHORT_LONG_BOUNDARY_US = 78.0   # a half < this is a "1", else "0"
LONG_MAX_PLAUSIBLE_US  = 12000.0
BOUNDARY_TRIM_EDGES    = 6       # transitions discarded at each capture edge
GLITCH_FLOOR_US        = 40.0   # below this is no valid DCC half-bit -> a glitch

# DCC packet bounds (mirror firmware dcc_types.h / dcc_defines.h).
DCC_PACKET_MAX_BYTES   = 6       # data[] incl. XOR byte
DCC_PREAMBLE_MAX_BITS  = 20      # service-mode preamble (>= ops 14)
ZERO_BIT_FULL_US       = 232.0   # this DUT's zero bit = 2 x 116us half (S-9.1 verified)

# Worst-case packet duration: treat EVERY bit as a (longest) zero bit, max bytes,
# max preamble. Frame = preamble + start(1) + 8*bytes + (bytes-1) separators + end(1).
_WORST_PACKET_BITS = (DCC_PREAMBLE_MAX_BITS + 1 + 8 * DCC_PACKET_MAX_BYTES
                      + (DCC_PACKET_MAX_BYTES - 1) + 1)            # = 75 bits
WORST_PACKET_US = _WORST_PACKET_BITS * ZERO_BIT_FULL_US           # ~17.4 ms
# Saleae DigitalTriggerCaptureMode geometry (measured, two-channel capture):
# the retained window is  [ -trim_data_seconds , +after_trigger_seconds ]  with
# t=0 at the trigger edge -- the two are INDEPENDENT (trim = pre-trigger keep,
# after = post-trigger keep), NOT a total. The DUT pulses PB3 from on_packet_sent
# (~transmit-start), so the packet under test sits at t ~= 0. Keep a small pre-roll
# (catches the target's start; ~1 idle before it) and a post window holding the
# target + a few repeats (~7 ms apart). first_non_idle() then picks the target.
PRE_TRIGGER_SECONDS  = 0.008             # -> trim_data_seconds  (pre-trigger keep)
POST_TRIGGER_SECONDS = 0.030             # -> after_trigger_seconds (post-trigger keep)

REPORTS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "reports")


# ----------------------------------------------------------------------------
# UART control of the DUT
# ----------------------------------------------------------------------------
def send_command(port, cmd, settle=0.3):
    """Open the DUT UART, send one CR-terminated command, return the reply text."""
    import time
    import serial
    with serial.Serial(port, SERIAL_BAUD, timeout=0.5) as s:
        time.sleep(0.15)
        s.reset_input_buffer()
        s.write((cmd + "\r").encode())
        time.sleep(settle)
        return s.read(400).decode(errors="replace")


def find_dut_port():
    """Probe candidate ports with read-only STATUS; return the one the DCC
    firmware answers on. Harmless to a non-DUT port (bytes go nowhere)."""
    import serial.tools.list_ports
    candidates = [SERIAL_PORT] if SERIAL_PORT else []
    candidates += [p.device for p in serial.tools.list_ports.comports()
                   if "usbmodem" in p.device.lower()]
    seen = set()
    for port in [c for c in candidates if not (c in seen or seen.add(c))]:
        try:
            resp = send_command(port, "STATUS")
            if "STATUS:" in resp or "DCC" in resp.upper():
                return port
        except Exception:
            continue
    return None


def start_dcc():
    """Find the DUT, power the track on, return the port."""
    port = find_dut_port()
    if not port:
        raise RuntimeError("could not find the DUT UART among /dev/cu.usbmodem* "
                           "(is the firmware running? set SERIAL_PORT to pin it)")
    reply = send_command(port, "POWER ON")
    last = reply.strip().splitlines()[-1] if reply.strip() else "(no reply)"
    print(f"[uart] {port}: POWER ON -> {last}")
    return port


# ----------------------------------------------------------------------------
# Saleae capture
# ----------------------------------------------------------------------------
def capture_to_csv(out_dir, stimulus=None, capture_seconds=None):
    """Run a timed capture and export digital.csv into out_dir. Returns its path.

    stimulus: optional callable invoked right after the capture is armed -- use it
              to drive UART command(s) into the live capture window.
    capture_seconds: override the default duration (wider for stimulus runs so the
              command reliably lands inside the window).
    """
    from saleae import automation

    secs = CAPTURE_SECONDS if capture_seconds is None else capture_seconds
    dev_cfg = automation.LogicDeviceConfiguration(
        enabled_digital_channels=[DIGITAL_CHANNEL],
        digital_sample_rate=SAMPLE_RATE_HZ,
        # original Logic has a fixed threshold -- do NOT set digital_threshold_volts
    )
    cap_cfg = automation.CaptureConfiguration(
        capture_mode=automation.TimedCaptureMode(duration_seconds=secs)
    )
    with automation.Manager.connect(port=AUTOMATION_PORT) as mgr:
        print(f"[saleae] {secs*1e3:.0f} ms capture @ "
              f"{SAMPLE_RATE_HZ/1e6:.0f} MS/s on ch{DIGITAL_CHANNEL}")
        with mgr.start_capture(
            device_id=SALEAE_DEVICE_ID or None,
            device_configuration=dev_cfg,
            capture_configuration=cap_cfg,
        ) as cap:
            if stimulus is not None:
                stimulus()          # drive UART command(s) into the live window
            cap.wait()
            cap.export_raw_data_csv(directory=out_dir,
                                    digital_channels=[DIGITAL_CHANNEL])
    return os.path.join(out_dir, "digital.csv")


# ----------------------------------------------------------------------------
# Decode: CSV transitions -> half-bit widths -> bits -> packets
# ----------------------------------------------------------------------------
def read_transitions(csv_path):
    rows = []
    with open(csv_path) as f:
        r = csv.reader(f)
        next(r)  # header
        for line in r:
            if len(line) >= 2:
                rows.append((float(line[0]), int(line[1])))
    return rows


def decode(rows):
    """edges -> bits -> packets, with capture-boundary artifacts removed.

    Returns dict: bit_halves [(a_us,b_us,bit)], bits, packets [(preamble,[bytes])],
    packet_times (abs start time in s of each packet's preamble, parallel to
    packets), trimmed_runts (count at edges), interior_glitches (sub-floor pulses).
    """
    widths = [(rows[i][0] - rows[i - 1][0]) * 1e6 for i in range(1, len(rows))]
    # Absolute start time (s) of each interval, index-aligned with widths.
    times = [rows[i - 1][0] for i in range(1, len(rows))]

    # Trim truncated partial bits at each capture edge (boundary artifacts).
    trimmed_runts = 0
    if len(widths) > 2 * BOUNDARY_TRIM_EDGES:
        edge = widths[:BOUNDARY_TRIM_EDGES] + widths[-BOUNDARY_TRIM_EDGES:]
        trimmed_runts = sum(1 for w in edge if w < GLITCH_FLOOR_US)
        widths = widths[BOUNDARY_TRIM_EDGES:-BOUNDARY_TRIM_EDGES]
        times = times[BOUNDARY_TRIM_EDGES:-BOUNDARY_TRIM_EDGES]

    # Sub-floor pulses still present are INTERIOR -> real glitches, never hidden.
    interior_glitches = [w for w in widths if w < GLITCH_FLOOR_US]

    def cls(w):
        if w < SHORT_LONG_BOUNDARY_US:
            return "1"
        if w <= LONG_MAX_PLAUSIBLE_US:
            return "0"
        return "X"

    classes = [cls(w) for w in widths]

    bit_halves, bits, bit_times = [], [], []
    i = 0
    while i < len(classes) - 1:
        a, b = classes[i], classes[i + 1]
        if a == b and a in "01":
            bit_halves.append((widths[i], widths[i + 1], a))
            bits.append(a)
            bit_times.append(times[i])     # bit starts at its first half
            i += 2
        else:
            i += 1  # resync at packet boundary / glitch

    bitstr = "".join(bits)

    # framing: preamble (>=12 ones) 0 {byte 0}* {byte} 1
    packets, packet_times = [], []
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
                complete = False
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
                if data and complete:      # drop window-truncated partials
                    packets.append((run, data))
                    packet_times.append(bit_times[i])   # preamble start time
                i = k
                continue
        i += 1

    return {"bit_halves": bit_halves, "bits": bitstr, "packets": packets,
            "packet_times": packet_times, "trimmed_runts": trimmed_runts,
            "interior_glitches": interior_glitches}


def capture_and_decode():
    """Capture once and decode. Returns (decoded_dict, n_transitions)."""
    with tempfile.TemporaryDirectory() as d:
        rows = read_transitions(capture_to_csv(d))
        return decode(rows), len(rows)


def capture_with_command(commands, capture_seconds=0.20, fires=10, spacing=0.012,
                         port=None):
    """Capture while repeatedly firing UART command(s) into the live window.

    For ONE-SHOT packets (e-stop, CV-POM, accessory) that aren't auto-refreshed:
    the command is sent `fires` times, `spacing` apart, across a wider capture
    window so it reliably lands inside regardless of the analyzer's arm latency.
    (Auto-refresh commands like SPEED/FUNC don't need this -- just send once,
    then capture_and_decode, since they transmit continuously.)

    commands: a command string or list of strings (each CR-terminated on send).
    Returns (decoded_dict, n_transitions). Track power must already be ON.
    """
    import time
    import serial

    if isinstance(commands, str):
        commands = [commands]
    if port is None:
        port = find_dut_port()
    if not port:
        raise RuntimeError("could not find the DUT UART (is the firmware running?)")

    def stimulus():
        with serial.Serial(port, SERIAL_BAUD, timeout=0.3) as s:
            time.sleep(0.005)
            for _ in range(fires):
                for c in commands:
                    s.write((c + "\r").encode())
                time.sleep(spacing)

    print(f"[uart] firing {commands} x{fires} into the live capture")
    with tempfile.TemporaryDirectory() as d:
        path = capture_to_csv(d, stimulus=stimulus, capture_seconds=capture_seconds)
        rows = read_transitions(path)
        return decode(rows), len(rows)


def capture_triggered(stimulus, trigger_channel=None,
                      pre_seconds=None, after_seconds=None):
    """Hardware-triggered capture: arm on a RISING edge on the trigger channel,
    run `stimulus` (which arms the DUT trigger + sends the command under test so
    the DUT pulses that pin on the packet), then keep a window spanning
    `pre_seconds` BEFORE the trigger to `after_seconds` after. Decodes the DCC
    channel. Returns (decoded, n_transitions).

    Geometry (measured): the retained window is [-pre_seconds, +after_seconds]
    with t=0 at the trigger edge -- pre_seconds maps to trim_data_seconds and
    after_seconds to after_trigger_seconds, INDEPENDENTLY. The DUT pulses PB3 from
    on_packet_sent (~transmit-start), so the packet under test sits at t ~= 0: the
    pre-roll catches its start (~1 idle ahead of it) and the post window holds the
    target plus a few ~7 ms-spaced repeats. Use first_non_idle() to pick it out.
    Requires the DUT trigger pin (PB3) wired to Saleae TRIGGER_CHANNEL.

    NOTE: blocks in wait() until the trigger fires -- if the pin is unwired or the
    command never produces a non-idle packet, it will hang. The stimulus is built
    to guarantee a trigger when wired correctly.
    """
    from saleae import automation

    if pre_seconds is None:
        pre_seconds = PRE_TRIGGER_SECONDS        # pre-trigger keep (-> trim_data_seconds)
    if after_seconds is None:
        after_seconds = POST_TRIGGER_SECONDS     # post-trigger keep (-> after_trigger_seconds)
    trig = TRIGGER_CHANNEL if trigger_channel is None else trigger_channel
    chans = sorted({DIGITAL_CHANNEL, trig})
    dev_cfg = automation.LogicDeviceConfiguration(
        enabled_digital_channels=chans, digital_sample_rate=SAMPLE_RATE_HZ)
    # Window geometry (MEASURED with a two-channel capture, t=0 at the trigger edge):
    #   retained data = [ -trim_data_seconds , +after_trigger_seconds ]
    # The two are INDEPENDENT -- trim_data_seconds is the PRE-trigger keep, NOT a
    # total. The packet under test sits at t~=0 (DUT pulses PB3 at transmit-start),
    # so a small pre-roll + a post window of a few packet-times is all that's needed;
    # first_non_idle() then selects it.
    #   *** DO NOT set trim_data_seconds = pre_seconds + after_seconds. *** That was
    #   the original bug: with after=80ms it made the pre-window ~82ms of idle packets
    #   and made the target look ~24ms late (it is NOT -- there is no encoder pipeline).
    cap_cfg = automation.CaptureConfiguration(
        capture_mode=automation.DigitalTriggerCaptureMode(
            trigger_type=automation.DigitalTriggerType.RISING,
            trigger_channel_index=trig,
            after_trigger_seconds=after_seconds,    # post-trigger keep
            trim_data_seconds=pre_seconds))          # pre-trigger keep (independent!)

    with tempfile.TemporaryDirectory() as d:
        with automation.Manager.connect(port=AUTOMATION_PORT) as mgr:
            print(f"[saleae] digital trigger: ch{trig} RISING, "
                  f"-{pre_seconds*1e3:.0f}/+{after_seconds*1e3:.0f} ms window")
            with mgr.start_capture(
                device_id=SALEAE_DEVICE_ID or None,
                device_configuration=dev_cfg,
                capture_configuration=cap_cfg,
            ) as cap:
                stimulus()        # arm DUT + send command -> DUT pulses trigger pin
                cap.wait()        # returns once triggered + after_trigger_seconds
                cap.export_raw_data_csv(directory=d,
                                        digital_channels=[DIGITAL_CHANNEL])
        rows = read_transitions(os.path.join(d, "digital.csv"))
        return decode(rows), len(rows)


def trigger_command(command, pre_seconds=None, after_seconds=None,
                    trigger_channel=None):
    """Drive a command under a hardware-triggered capture. Arms the DUT trigger
    (UART 'TRIG') then sends `command` (str or list); the next non-idle packet
    pulses the trigger pin so the capture lands on it. Track power must be ON.
    Returns (decoded, n_transitions)."""
    port = find_dut_port()
    if not port:
        raise RuntimeError("could not find the DUT UART (is the firmware running?)")
    cmds = [command] if isinstance(command, str) else list(command)

    def stimulus():
        send_command(port, "CLEAR")   # test isolation: idle-only stream first
        send_command(port, "TRIG")
        for c in cmds:
            send_command(port, c)

    print(f"[uart] CLEAR + TRIG + {cmds} under hardware trigger")
    return capture_triggered(stimulus, trigger_channel=trigger_channel,
                             pre_seconds=pre_seconds, after_seconds=after_seconds)


def first_non_idle(decoded, idle):
    """Packet under test from a post-trigger capture: the first non-idle packet in
    chronological order. Because the trigger fires at the packet's START, the first
    real packet after the trigger IS the one under test -- robust for one-shots
    (no 'most common' vote to lose) and immune to the previous packet bleeding in
    from a pre-trigger window. Returns the byte list, or None if only idle seen."""
    for _, data in decoded["packets"]:
        if list(data) != list(idle):
            return list(data)
    return None


def stats(xs):
    return (min(xs), sum(xs) / len(xs), max(xs)) if xs else (0, 0, 0)


def sigma_margin_detail(values, lo_limit=None, hi_limit=None):
    """Stats string with σ and margin to each spec wall in σ units.

    lo_limit / hi_limit: spec boundary (None = no wall on that side).
    A positive margin means the population mean is that many σ inside the limit.
    """
    n = len(values)
    if n < 2:
        return f"n={n} (need >= 2 for σ)"
    lo  = min(values)
    mean = sum(values) / n
    hi  = max(values)
    std = math.sqrt(sum((x - mean) ** 2 for x in values) / (n - 1))
    parts = [f"n={n}", f"min={lo:.2f}", f"mean={mean:.3f}",
             f"max={hi:.2f}", f"σ={std:.3f}"]
    if std > 0:
        if lo_limit is not None:
            parts.append(f"lo=+{(mean - lo_limit) / std:.1f}σ")
        if hi_limit is not None:
            parts.append(f"hi=+{(hi_limit - mean) / std:.1f}σ")
    else:
        parts.append("(all identical)")
    return "  ".join(parts)


# ----------------------------------------------------------------------------
# Report: streams each check live, collects results for the HTML report
# ----------------------------------------------------------------------------
class Report:
    def __init__(self, doc, title, source_pdf="", aspect=""):
        self.doc, self.title = doc, title
        self.source_pdf, self.aspect = source_pdf, aspect
        self.checks = []
        self.failed = False
        print("\n" + "=" * 78)
        print(f"  NMRA {doc} COMPLIANCE  —  {title}")
        print("=" * 78)

    def _emit(self, status, clause, name, detail):
        self.checks.append({"status": status, "clause": clause,
                            "name": name, "detail": detail})
        print(f"  [{status:<4}] {clause:<14} {name}")
        print(f"         {detail}", flush=True)

    def check(self, clause, name, ok, detail):
        if not ok:
            self.failed = True
        self._emit("PASS" if ok else "FAIL", clause, name, detail)

    def na(self, clause, name, detail):
        self._emit("N/A", clause, name, detail)

    @property
    def counts(self):
        p = sum(1 for c in self.checks if c["status"] == "PASS")
        f = sum(1 for c in self.checks if c["status"] == "FAIL")
        n = sum(1 for c in self.checks if c["status"] == "N/A")
        return p, f, n

    def finish(self):
        p, f, n = self.counts
        print("=" * 78)
        print(f"  RESULT: {'FAIL' if self.failed else 'PASS'}   "
              f"({p} pass, {f} fail, {n} n/a)")
        print("=" * 78 + "\n", flush=True)
        return self

    def as_dict(self):
        p, f, n = self.counts
        return {"doc": self.doc, "title": self.title, "source_pdf": self.source_pdf,
                "aspect": self.aspect, "checks": self.checks,
                "result": "FAIL" if self.failed else "PASS",
                "n_pass": p, "n_fail": f, "n_na": n}


# ----------------------------------------------------------------------------
# HTML report
# ----------------------------------------------------------------------------
_CSS = """
body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;margin:2rem;color:#1a1a1a;background:#fafafa}
h1{font-size:1.5rem;margin-bottom:.2rem}
h2{font-size:1.1rem;margin-top:1.8rem;border-bottom:1px solid #ddd;padding-bottom:.3rem}
.meta{color:#666;font-size:.85rem;margin:.2rem 0 1rem}
code{font-size:.78rem;color:#555}
table{border-collapse:collapse;width:100%;margin:.5rem 0;font-size:.85rem}
th,td{text-align:left;padding:.35rem .6rem;border-bottom:1px solid #eee;vertical-align:top}
th{background:#f0f0f0;font-weight:600}
td.st,td.cl{white-space:nowrap;font-variant-numeric:tabular-nums}
td.dt{color:#555;font-family:ui-monospace,Menlo,monospace;font-size:.8rem}
tr.pass td.st{color:#138000;font-weight:700}
tr.fail td.st{color:#c00;font-weight:700}
tr.na td.st{color:#888;font-weight:700}
.badge{display:inline-block;padding:.05rem .55rem;border-radius:.4rem;font-size:.8rem;color:#fff;vertical-align:middle}
.badge.pass{background:#138000}.badge.fail{background:#c00}
.summary{font-size:.8rem;color:#666}
"""


def _section(r):
    rows = []
    for c in r["checks"]:
        cls = {"PASS": "pass", "FAIL": "fail", "N/A": "na"}[c["status"]]
        rows.append(
            f'<tr class="{cls}"><td class="st">{c["status"]}</td>'
            f'<td class="cl">{html.escape(c["clause"])}</td>'
            f'<td>{html.escape(c["name"])}</td>'
            f'<td class="dt">{html.escape(c["detail"])}</td></tr>')
    badge = "pass" if r["result"] == "PASS" else "fail"
    return (
        f'<section><h2>{html.escape(r["doc"])} — {html.escape(r["title"])} '
        f'<span class="badge {badge}">{r["result"]}</span></h2>'
        f'<p class="meta">{html.escape(r.get("aspect", ""))}<br>'
        f'<code>{html.escape(r.get("source_pdf", ""))}</code></p>'
        f'<table><thead><tr><th>Status</th><th>Clause</th><th>Check</th>'
        f'<th>Measured / detail</th></tr></thead><tbody>{"".join(rows)}</tbody></table>'
        f'<p class="summary">{r["n_pass"]} pass &middot; {r["n_fail"]} fail '
        f'&middot; {r["n_na"]} n/a</p></section>')


def write_html(results, path, title="DCC Compliance Report", timestamp=None):
    """Write an HTML report for one or more spec results. Returns the path."""
    timestamp = timestamp or datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    overall = "FAIL" if any(r["result"] == "FAIL" for r in results) else "PASS"
    tp = sum(r["n_pass"] for r in results)
    tf = sum(r["n_fail"] for r in results)
    tn = sum(r["n_na"] for r in results)
    rollup = "".join(
        f'<tr class="{"pass" if r["result"] == "PASS" else "fail"}">'
        f'<td>{html.escape(r["doc"])}</td><td>{html.escape(r["title"])}</td>'
        f'<td class="st">{r["result"]}</td>'
        f'<td>{r["n_pass"]}/{r["n_fail"]}/{r["n_na"]}</td></tr>'
        for r in results)
    badge = "pass" if overall == "PASS" else "fail"
    doc = (
        f'<!doctype html><html lang="en"><head><meta charset="utf-8">'
        f'<title>{html.escape(title)}</title><style>{_CSS}</style></head><body>'
        f'<h1>{html.escape(title)} <span class="badge {badge}">{overall}</span></h1>'
        f'<p class="meta">Generated {timestamp} &middot; hardware-in-the-loop (Saleae) '
        f'&middot; {tp} pass / {tf} fail / {tn} n/a</p>'
        f'<h2>Summary</h2><table><thead><tr><th>Spec</th><th>Title</th>'
        f'<th>Result</th><th>P/F/N</th></tr></thead><tbody>{rollup}</tbody></table>'
        f'{"".join(_section(r) for r in results)}</body></html>')
    os.makedirs(REPORTS_DIR, exist_ok=True)
    with open(path, "w") as f:
        f.write(doc)
    return path


def report_path(stem):
    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    os.makedirs(REPORTS_DIR, exist_ok=True)
    return os.path.join(REPORTS_DIR, f"{stem}_{ts}.html")
