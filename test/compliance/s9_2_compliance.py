#!/usr/bin/env python3
"""
NMRA S-9.2 -- Baseline Packet Format compliance (command-station transmit).

Hardware-in-the-loop: captures the DCC signal on a Saleae (ch0 = DCC out / PB1),
decodes it, and checks the baseline packet format against the S-9.2 standard.
This first cut verifies format/structure/error-detection/idle on idle traffic
(no timing races). Driven baseline packets (reset, broadcast-stop) and the 30 ms
refresh-timing check need a stimulus-during-capture helper -- reported N/A here.

Shared plumbing lives in compliance_lib.py.

Run standalone:  .venv/bin/python s9_2_compliance.py
Or via:          .venv/bin/python run_all.py
"""

import sys
import time
import compliance_lib as lib

SPEC_DOC   = "S-9.2"
SPEC_TITLE = "Baseline Packet Format (command-station transmit)"
SOURCE_PDF = "documentation/specs/s-92-2004-07.pdf"
ASPECT     = "baseline packet structure, error detection & idle packet (DCC out, ch0)"

IDLE_PACKET = [0xFF, 0x00, 0xFF]   # S-9.2: 11111111 00000000 11111111


def _xor(data):
    x = 0
    for b in data:
        x ^= b
    return x


def checks(rep, decoded):
    packets = [d for _, d in decoded["packets"]]
    if not packets:
        rep.check("S-9.2", "packets present", False, "no packets decoded")
        return

    # 1. General packet format: address byte + data* + error byte (>= 2 bytes)
    malformed = [d for d in packets if len(d) < 2]
    # @compliance DCC-S9.2-CS-005
    rep.check("S-9.2 §A", "packet structure (addr + data* + error byte)",
              not malformed,
              f"{len(packets)} packets, all >= 2 bytes (addr+error); "
              f"{len(malformed)} malformed")

    # 2. Error-detection byte = XOR of all preceding bytes (S-9.2's mechanism)
    bad = [d for d in packets if _xor(d) != 0]
    # @compliance DCC-S9.2-CS-006
    rep.check("S-9.2", "error-detection byte = XOR of preceding bytes",
              not bad, f"{len(packets)} packets, {len(bad)} XOR mismatches")

    # 3. Idle packet present & exact: 11111111 00000000 11111111
    idles = [d for d in packets if d == IDLE_PACKET]
    # @compliance DCC-S9.2-CS-001
    rep.check("S-9.2", "idle packet = FF 00 FF",
              len(idles) > 0,
              f"{len(idles)}/{len(packets)} packets are idle "
              f"(addr=0xFF idle-reserved, data=0x00, error=0xFF)")

    # 4. Address byte uses S-9.2-legal values; idle uses reserved 0xFF.
    #    S-9.2 reserves 0x00 (broadcast), 0xFE, 0xFF (idle); others = decoders.
    addrs = sorted({d[0] for d in packets})
    idle_addr_ok = bool(idles) and all(d[0] == 0xFF for d in idles)
    rep.check("S-9.2", "address byte: idle uses reserved 0xFF",
              idle_addr_ok,
              f"address bytes seen: {[hex(a) for a in addrs]}")

    # 5. Refresh timing (S-9.2): a complete packet at least every 30 ms
    #    (start-bit to start-bit). Measured on idle traffic -- no stimulus needed.
    times = decoded["packet_times"]
    if len(times) >= 2:
        gaps_ms = [(times[i + 1] - times[i]) * 1000.0 for i in range(len(times) - 1)]
        maxgap = max(gaps_ms)
        # @compliance DCC-S9.2-CS-007
        rep.check("S-9.2", "refresh: a packet at least every 30 ms", maxgap <= 30.0,
                  f"max start-to-start gap = {maxgap:.2f} ms over {len(times)} packets")
    else:
        rep.check("S-9.2", "refresh: a packet at least every 30 ms", False,
                  "not enough packets to measure")


def _target(decoded):
    """Packet under test = first non-idle packet after the trigger (post-trigger
    capture; the trigger fires at the packet's START). CLEAR isolates it."""
    return lib.first_non_idle(decoded, IDLE_PACKET)


def _hx(bs):
    return " ".join(f"{b:02X}" for b in bs) if bs else "(none)"


def driven_checks(rep):
    """Driven baseline packets via the hardware trigger (needs TRIG/CLEAR + PB3
    + the ESTOP/STOP/RESET firmware commands)."""

    # Broadcast emergency stop -- baseline 01DC000S, S=1.
    dec, _ = lib.trigger_command("ESTOP")
    got = _target(dec)
    # @compliance DCC-S9.2-CS-003
    rep.check("S-9.2", "broadcast stop = baseline 01DC000S (emergency S=1)",
              got == [0x00, 0x51, 0x51], f"ESTOP -> [{_hx(got)}] expected [00 51 51]")

    # Broadcast controlled stop -- baseline 01DC000S, S=0.
    dec, _ = lib.trigger_command("STOP")
    got = _target(dec)
    # @compliance DCC-S9.2-CS-004
    rep.check("S-9.2", "broadcast controlled stop = baseline 01DC000S (S=0)",
              got == [0x00, 0x50, 0x50], f"STOP -> [{_hx(got)}] expected [00 50 50]")

    # Broadcast reset packet -- 00 00 00.
    dec, _ = lib.trigger_command("RESET")
    got = _target(dec)
    # @compliance DCC-S9.2-CS-002
    rep.check("S-9.2", "reset packet = 00 00 00",
              got == [0x00, 0x00, 0x00], f"RESET -> [{_hx(got)}] expected [00 00 00]")


def same_address_spacing_check(rep):
    """CS-008 (S-9.2 Section C fn.11): two packets for the SAME short address
    112-127 must not be < 5 ms apart -- their first byte (0x70-0x7F) aliases a
    service-mode register/paged command. With a single such loco in the refresh
    cycle the scheduler inserts an idle spacer, so the wire shows
    <addr>, idle, <addr>, idle ... Addresses below 112 (and long addresses) are
    sent back-to-back."""
    spc = SPEC_DOC + " Section C fn.11"
    port = lib.find_dut_port()
    if not port:
        rep.check(spc, "DUT present for same-address spacing test", False,
                  "no DUT UART found")
        return

    def _addr_stream(addr):
        lib.send_command(port, "CLEAR")
        lib.send_command(port, "SPEED %d 50 FWD" % addr)
        time.sleep(0.3)
        decoded, _ = lib.capture_and_decode()
        pkts = [d for _, d in decoded["packets"]]
        times = decoded["packet_times"]
        idx = [i for i, p in enumerate(pkts) if p and p[0] == addr]
        return times, idx

    # --- short address 115 (0x73, aliases service mode) -> idle spacer required ---
    times, idx = _addr_stream(115)
    if len(idx) < 2:
        # @compliance DCC-S9.2-CS-008
        rep.check(spc, "short addr 115 present on the wire (>=2 packets)", False,
                  "captured only %d packet(s) addressed to 115" % len(idx))
    else:
        adjacent = any(idx[k + 1] == idx[k] + 1 for k in range(len(idx) - 1))
        # @compliance DCC-S9.2-CS-008
        rep.check(spc, "short addr 115: idle spacer between repeats (no two adjacent)",
                  not adjacent,
                  "two 115 packets were sent back-to-back" if adjacent
                  else "%d packets to 115, each separated by an idle" % len(idx))
        gaps = [(times[idx[k + 1]] - times[idx[k]]) * 1000.0
                for k in range(len(idx) - 1)]
        # @compliance DCC-S9.2-CS-008
        rep.check(spc, "short addr 115: same-address start-to-start >= 5 ms",
                  min(gaps) >= 5.0, "min same-address gap = %.2f ms" % min(gaps))

    # --- control: short address 100 (<112) -> back-to-back, no spacer ---
    times, idx = _addr_stream(100)
    if len(idx) >= 2:
        adjacent = any(idx[k + 1] == idx[k] + 1 for k in range(len(idx) - 1))
        # @compliance DCC-S9.2-CS-008
        rep.check(spc, "control: addr 100 (<112) sent back-to-back (no spacer)",
                  adjacent,
                  "addr 100 was unexpectedly spaced" if not adjacent
                  else "addr 100 repeats are adjacent, as expected")

    lib.send_command(port, "CLEAR")   # restore the idle-only stream


def run():
    """Capture, decode, run S-9.2 checks. Returns the finished Report."""
    if lib.DRIVE_UART:
        lib.start_dcc()
    decoded, n_tx = lib.capture_and_decode()
    print(f"[decode] {n_tx} transitions, {len(decoded['bits'])} bits, "
          f"{len(decoded['packets'])} packets")
    rep = lib.Report(SPEC_DOC, SPEC_TITLE, SOURCE_PDF, ASPECT)
    checks(rep, decoded)
    if lib.DRIVE_UART:
        driven_checks(rep)
        same_address_spacing_check(rep)
    return rep.finish()


def main():
    print("\n#### DCC COMPLIANCE TEST ####")
    print(f"Spec under test : NMRA {SPEC_DOC}  Baseline Packet Format")
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
    path = lib.write_html([rep.as_dict()], lib.report_path("s9_2"),
                          title=f"NMRA {SPEC_DOC} Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
