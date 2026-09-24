#!/usr/bin/env python3
"""
OpenDccCLib library scheduler -- HIL compliance (command-station transmit).

These are not NMRA-spec packet checks; they verify the LIBRARY's main-track
scheduler behaviour as it shows up ON THE WIRE (ch0 / PB1):

  - Priority selection: a one-shot high-priority packet (ESTOP, DCC_PRIORITY_ESTOP,
    non-refresh) is still selected and transmitted while an auto-refresh slot
    (SPEED, DCC_PRIORITY_SPEED) keeps streaming -- the scheduler interleaves the
    one-shot into the refresh cycle by priority.                 (DCC-Library-CS-001)
  - Auto-refresh round-robin: several SPEED slots are each retransmitted, cycling
    through every active address.                                (DCC-Library-CS-002)
  - Duplicate combining by (address, tag): two SPEED commands for the SAME address
    overwrite ONE refresh slot -- the wire shows only the latest value, never two
    slots accumulating for that address.                         (DCC-Library-CS-003)

All three are driven with the firmware's existing command set (SPEED / ESTOP /
REFRESH / CLEAR) and a plain timed capture -- no firmware change, no decoder.
The scheduler is documented in src/dcc/dcc_scheduler.h (priority selection,
auto-refresh round-robin, duplicate combining via the (address, tag) key).

Bench: firmware on the LaunchPad, Logic 2 + Automation API (port 10430),
       MAIN track (PB1) on ch0. See HIL_SETUP.md.

Run standalone:  .venv/bin/python library_compliance.py
Or via:          .venv/bin/python run_all.py
"""

import sys
import time
import tempfile

import compliance_lib as lib

SPEC_DOC   = "Library"
SPEC_TITLE = "Scheduler behaviour (command-station transmit)"
SOURCE_PDF = "src/dcc/dcc_scheduler.h"
ASPECT     = "scheduler priority selection, auto-refresh round-robin, duplicate combining (DCC out, ch0)"

IDLE  = [0xFF, 0x00, 0xFF]
RESET = [0x00, 0x00, 0x00]

# Short addresses well below the 112-127 service-mode-alias band, so packets go
# back-to-back with no idle spacer (see s9_2 same-address-spacing rule).
LOCO_A, LOCO_B, LOCO_C = 3, 4, 5


def _capture(seconds):
    """Plain timed capture of the main track (ch0); return the decoded dict.
    Auto-refresh packets transmit continuously, so no stimulus is needed."""
    with tempfile.TemporaryDirectory() as d:
        rows = lib.read_transitions(lib.capture_to_csv(d, capture_seconds=seconds))
        return lib.decode(rows)


def _non_idle(dec):
    """Non-idle, non-reset packets in capture order (the scheduler's real output)."""
    return [list(d) for _, d in dec["packets"]
            if list(d) != IDLE and list(d) != RESET]


def _for_addr(dec, addr):
    """Every packet whose first (address) byte == addr, as byte lists."""
    return [list(d) for _, d in dec["packets"] if d and d[0] == addr]


def _distinct(pkts):
    """De-duplicated packet list, order-preserved."""
    seen, out = set(), []
    for p in pkts:
        t = tuple(p)
        if t not in seen:
            seen.add(t)
            out.append(p)
    return out


def _hx(bs):
    return " ".join(f"{b:02X}" for b in bs) if bs else "(none)"


# ----------------------------------------------------------------------------
# DCC-Library-CS-002 -- auto-refresh round-robin of several slots
# ----------------------------------------------------------------------------
# @compliance DCC-Library-CS-002
def test_round_robin(rep, port):
    """Three SPEED slots (addrs 3/4/5) on auto-refresh: every address must appear,
    repeatedly, in one capture -- i.e. the scheduler cycles round-robin through
    all active refresh slots rather than starving any of them."""
    clause = "Library / scheduler (auto-refresh)"
    lib.send_command(port, "CLEAR")
    lib.send_command(port, "REFRESH ON")
    lib.send_command(port, f"SPEED {LOCO_A} 50 FWD")
    lib.send_command(port, f"SPEED {LOCO_B} 60 FWD")
    lib.send_command(port, f"SPEED {LOCO_C} 70 FWD")
    time.sleep(0.3)                       # let all three enter the refresh cycle

    dec = _capture(0.30)                  # wide window: several full cycles
    counts = {a: len(_for_addr(dec, a)) for a in (LOCO_A, LOCO_B, LOCO_C)}

    present = all(counts[a] >= 1 for a in counts)
    rep.check(clause, "all 3 refresh slots appear on the wire", present,
              f"packet counts per address: {counts}")

    cycling = all(counts[a] >= 2 for a in counts)
    rep.check(clause, "each slot is retransmitted (round-robin, >=2x each)", cycling,
              f"per-address repeats: {counts} (each must cycle, not be sent once)")


# ----------------------------------------------------------------------------
# DCC-Library-CS-003 -- duplicate combining by (address, tag)
# ----------------------------------------------------------------------------
# @compliance DCC-Library-CS-003
def test_duplicate_combining(rep, port):
    """Two SPEED commands for the SAME address share one (address, DCC_TAG_SPEED)
    slot: the second overwrites the first. The wire must show only the latest
    value for that address -- exactly one distinct addr packet, and the earlier
    value's packet must be gone -- proving the slot was combined, not duplicated."""
    clause = "Library / scheduler (duplicate combining)"
    lib.send_command(port, "CLEAR")
    lib.send_command(port, "REFRESH ON")

    # First value.
    lib.send_command(port, f"SPEED {LOCO_A} 10 FWD")
    time.sleep(0.3)
    first = _distinct(_for_addr(_capture(0.20), LOCO_A))
    rep.check(clause, f"addr {LOCO_A}: exactly ONE refresh slot after first SPEED",
              len(first) == 1, f"distinct addr-{LOCO_A} packets: {[_hx(p) for p in first]}")
    pkt_v10 = first[0] if first else None

    # Second value to the SAME address -> must overwrite the same slot.
    lib.send_command(port, f"SPEED {LOCO_A} 20 FWD")
    time.sleep(0.3)
    second = _distinct(_for_addr(_capture(0.20), LOCO_A))

    rep.check(clause, f"addr {LOCO_A}: still ONE slot after second SPEED (combined)",
              len(second) == 1,
              f"distinct addr-{LOCO_A} packets now: {[_hx(p) for p in second]} "
              f"(two slots would mean no combining)")

    pkt_v20 = second[0] if second else None
    changed = pkt_v20 is not None and pkt_v20 != pkt_v10
    rep.check(clause, f"addr {LOCO_A}: wire now carries the LATEST value only", changed,
              f"first=[{_hx(pkt_v10)}] latest=[{_hx(pkt_v20)}] (must differ; old value gone)")

    old_gone = pkt_v10 is not None and pkt_v10 not in second
    rep.check(clause, f"addr {LOCO_A}: previous value no longer on the wire", old_gone,
              f"old packet [{_hx(pkt_v10)}] {'absent' if old_gone else 'STILL present'}")


# ----------------------------------------------------------------------------
# DCC-Library-CS-001 -- priority selection: one-shot vs refresh
# ----------------------------------------------------------------------------
# @compliance DCC-Library-CS-001
def test_priority(rep, port):
    """With a SPEED slot auto-refreshing (DCC_PRIORITY_SPEED), fire a one-shot
    broadcast ESTOP (DCC_PRIORITY_ESTOP, non-refresh). The scheduler's priority
    selection must still emit the one-shot ON THE WIRE while the refresh stream
    continues -- the one-shot is interleaved into the refresh cycle, not dropped."""
    clause = "Library / scheduler (priority selection)"
    lib.send_command(port, "CLEAR")
    lib.send_command(port, "REFRESH ON")
    lib.send_command(port, f"SPEED {LOCO_A} 50 FWD")    # refresh slot streaming
    time.sleep(0.3)

    # Fire the one-shot ESTOP broadcast repeatedly into a live capture so it
    # reliably lands; the refresh SPEED keeps streaming throughout.
    dec, _ = lib.capture_with_command("ESTOP", capture_seconds=0.30,
                                      fires=8, spacing=0.020, port=port)

    refresh_present = len(_for_addr(dec, LOCO_A)) >= 1
    rep.check(clause, "auto-refresh slot keeps streaming during the one-shot",
              refresh_present,
              f"addr-{LOCO_A} refresh packets seen: {len(_for_addr(dec, LOCO_A))}")

    # The one-shot ESTOP is a broadcast (address 0) packet that is NOT the reset.
    one_shots = [p for p in _non_idle(dec) if p and p[0] == 0x00]
    rep.check(clause, "one-shot ESTOP is selected & transmitted (priority)",
              len(one_shots) >= 1,
              f"broadcast one-shot packets seen: {[_hx(p) for p in _distinct(one_shots)]}")

    both = refresh_present and len(one_shots) >= 1
    rep.check(clause, "one-shot interleaved WITH refresh (both on one capture)", both,
              "priority selection emitted the one-shot without starving the refresh slot"
              if both else "did not observe one-shot and refresh together")


# ----------------------------------------------------------------------------
# DCC-Library-CS-005 -- auto-refresh pacing (issue #5): burst, keep-alive, ceiling
# ----------------------------------------------------------------------------
# Library policy values (the DCC_REFRESH_* table in dcc_defines.h). Everything
# below is counted in PACKET CYCLES: each scheduler cycle puts exactly one packet
# on the wire (a refresh, a one-shot or an idle), so "cycles between two copies
# of a slot" is the number of packets between them, whatever their length.
REFRESH_PROMPT_SENDS    = 3
REFRESH_COLD_CYCLES     = 60
REFRESH_COLD_MAX_CYCLES = 120
POOL = list(range(1, 9))          # eight short addresses below the 112-127 alias band


def _positions(dec, addr, speed_byte=None):
    """Capture-order indices of every packet addressed to addr (and, if given,
    carrying speed_byte as its third byte: a 128-step speed packet)."""
    return [i for i, (_, d) in enumerate(dec["packets"])
            if d and d[0] == addr
            and (speed_byte is None or (len(d) >= 3 and d[2] == speed_byte))]


def _gaps(idx):
    return [b - a for a, b in zip(idx, idx[1:])]


def _fill_pool(port):
    """Eight refresh slots, then long enough for every burst to be spent and
    every slot to be cold (3 + 60 cycles is well under 1 s)."""
    lib.send_command(port, "CLEAR")
    lib.send_command(port, "REFRESH ON")
    for a in POOL:
        lib.send_command(port, f"SPEED {a} 20 FWD")
    time.sleep(1.0)


# @compliance DCC-Library-CS-005
def test_burst_then_keep_alive(rep, port):
    """One fresh refresh slot on an otherwise idle stream: the packet is sent
    REFRESH_PROMPT_SENDS times back to back, then once every REFRESH_COLD_CYCLES
    packets. Hardware-triggered on the first copy; the window holds three
    keep-alives."""
    clause = "Library / scheduler (refresh pacing)"

    def stimulus():
        lib.send_command(port, "CLEAR")          # idle-only stream first
        lib.send_command(port, "REFRESH ON")
        lib.send_command(port, "TRIG")           # first copy pulses PB3
        lib.send_command(port, f"SPEED {LOCO_A} 50 FWD")

    dec, _ = lib.capture_triggered(stimulus, pre_seconds=0.02, after_seconds=1.35)
    pos = _positions(dec, LOCO_A)
    gaps = _gaps(pos)
    burst = gaps[:REFRESH_PROMPT_SENDS - 1]
    keep = gaps[REFRESH_PROMPT_SENDS - 1:]
    times = dec["packet_times"]

    ok_burst = len(pos) >= REFRESH_PROMPT_SENDS and all(g == 1 for g in burst)
    rep.check(clause, f"fresh slot: {REFRESH_PROMPT_SENDS} copies back to back", ok_burst,
              f"copy positions {pos[:REFRESH_PROMPT_SENDS + 1]}, gaps between copies "
              f"(packets) {gaps[:REFRESH_PROMPT_SENDS + 1]}")

    ok_keep = len(keep) >= 2 and all(g == REFRESH_COLD_CYCLES for g in keep)
    secs = [f"{times[b] - times[a]:.3f} s" for a, b in zip(pos, pos[1:])][REFRESH_PROMPT_SENDS - 1:]
    rep.check(clause, f"then one keep-alive every {REFRESH_COLD_CYCLES} cycles", ok_keep,
              f"keep-alive gaps in packets {keep} = {secs}")

    only_idle = all(list(d) == IDLE for _, d in dec["packets"] if d and d[0] != LOCO_A)
    rep.check(clause, "nothing but idle packets between the copies", only_idle,
              f"other non-idle packets: {[_hx(d) for _, d in dec['packets'] if d and d[0] != LOCO_A and list(d) != IDLE][:4]}")


# @compliance DCC-Library-CS-005
def test_keep_alive_cadence(rep, port):
    """Eight idle refresh slots over a 2 s capture: every slot keeps being
    refreshed, never inside REFRESH_COLD_CYCLES of its last copy and never
    later than REFRESH_COLD_MAX_CYCLES after it (the CV11 argument)."""
    clause = "Library / scheduler (refresh pacing)"
    _fill_pool(port)
    dec = _capture(2.0)

    per = {a: _gaps(_positions(dec, a)) for a in POOL}
    counts = {a: len(_positions(dec, a)) for a in POOL}
    all_gaps = [g for gs in per.values() for g in gs]

    rep.check(clause, "every idle slot keeps being refreshed (>= 2 copies each in 2 s)",
              all(c >= 2 for c in counts.values()), f"copies per address: {counts}")

    rep.check(clause, f"no idle slot is refreshed inside {REFRESH_COLD_CYCLES} cycles of its last copy",
              bool(all_gaps) and min(all_gaps) >= REFRESH_COLD_CYCLES,
              f"smallest gap {min(all_gaps) if all_gaps else None} packets; per address {per}")

    rep.check(clause, f"no idle slot goes unrefreshed past {REFRESH_COLD_MAX_CYCLES} cycles",
              bool(all_gaps) and max(all_gaps) <= REFRESH_COLD_MAX_CYCLES,
              f"largest gap {max(all_gaps) if all_gaps else None} packets")


# @compliance DCC-Library-CS-005
def test_change_latency_to_wire(rep, port):
    """Eight cold refresh slots; change one loco's speed. "TRIG INSERT" raises
    PB3 the moment the command is queued, so t = 0 is the insert: the changed
    packet must be the very next packet to start (latency under one packet
    time, the packet in flight) and its burst must follow back to back."""
    clause = "Library / scheduler (refresh pacing)"
    _fill_pool(port)
    new_speed = 90
    speed_byte = 0x80 | new_speed

    def stimulus():
        lib.send_command(port, "TRIG INSERT")
        lib.send_command(port, f"SPEED {LOCO_A} {new_speed} FWD")

    dec, _ = lib.capture_triggered(stimulus, pre_seconds=0.03, after_seconds=0.12)
    times = dec["packet_times"]
    hits = _positions(dec, LOCO_A, speed_byte)
    after = [i for i, t in enumerate(times) if t >= 0.0]       # packets starting after the insert
    spacing = [b - a for a, b in zip(times, times[1:])]
    packet_time = sum(spacing) / len(spacing) if spacing else float("nan")

    first_after = after[0] if after else None
    ok_next = bool(hits) and first_after is not None and hits[0] == first_after
    rep.check(clause, "changed packet is the next packet to start after the insert", ok_next,
              f"first packet after t=0 is index {first_after} "
              f"{_hx(dec['packets'][first_after][1]) if first_after is not None else ''}; "
              f"changed-packet indices {hits[:4]}")

    latency = times[hits[0]] if hits else float("nan")
    rep.check(clause, "command-to-wire latency under one packet time", bool(hits) and latency < packet_time,
              f"latency {latency * 1e3:.2f} ms; mean packet time {packet_time * 1e3:.2f} ms")

    ok_burst = len(hits) >= REFRESH_PROMPT_SENDS and _gaps(hits[:REFRESH_PROMPT_SENDS]) == [1] * (REFRESH_PROMPT_SENDS - 1)
    rep.check(clause, f"the change's {REFRESH_PROMPT_SENDS} copies follow back to back", ok_burst,
              f"changed-packet indices {hits[:REFRESH_PROMPT_SENDS + 1]}")


# ----------------------------------------------------------------------------
# Suite
# ----------------------------------------------------------------------------
def run():
    rep = lib.Report(SPEC_DOC, SPEC_TITLE, SOURCE_PDF, ASPECT)

    port = lib.find_dut_port()
    if not port:
        raise RuntimeError("DUT not found among /dev/cu.usbmodem* "
                           "(is the firmware running?)")
    lib.send_command(port, "POWER ON")
    time.sleep(0.5)
    try:
        test_round_robin(rep, port)
        test_duplicate_combining(rep, port)
        test_priority(rep, port)
        test_burst_then_keep_alive(rep, port)
        test_keep_alive_cadence(rep, port)
        test_change_latency_to_wire(rep, port)
    finally:
        lib.send_command(port, "CLEAR")
        lib.send_command(port, "POWER OFF")

    return rep.finish()


def main():
    print("\n#### DCC COMPLIANCE TEST ####")
    print(f"Spec under test : OpenDccCLib {SPEC_DOC} scheduler")
    print(f"Source          : {SOURCE_PDF}")
    print(f"Aspect          : {ASPECT}")
    try:
        rep = run()
    except ImportError as e:
        print(f"\nMissing dependency: {e}\nRun: pip install logic2-automation pyserial\n")
        return 2
    except Exception as e:
        print(f"\nERROR: {e}\nCheck: Logic 2 + Automation API (port {lib.AUTOMATION_PORT}), "
              f"MAIN track (PB1) on ch{lib.DIGITAL_CHANNEL}, firmware running.\n")
        return 2
    path = lib.write_html([rep.as_dict()], lib.report_path("library"),
                          title=f"OpenDccCLib {SPEC_DOC} Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
