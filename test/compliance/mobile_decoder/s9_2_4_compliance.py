#!/usr/bin/env python3
"""
NMRA S-9.2.4 -- packet-timeout fail-safe (mobile-decoder RECEIVE).

S-9.2.4 §4: while in Operations Mode, if no command packet addressed to the
decoder arrives within the CV11 time-out period, the decoder shall bring all
controlled devices to a stop. CV11 = 0 disables the time-out. This library maps
CV11 to 0.1 s per LSB (DCC_FAILSAFE_CV11_UNIT_US); the app reports the edges as
RECV FAILSAFE_ENTER / RECV FAILSAFE_EXIT.

One row:

  DEC-001  Packet-timeout fail-safe (CV11)  -- set a short CV11, go silent past it
           and observe FAILSAFE_ENTER; resume packets and observe FAILSAFE_EXIT;
           then set CV11=0 and confirm silence no longer trips it.

gTest covers the timer/edge LOGIC on host (dcc_failsafe_Test.cxx, 12 tests); this
suite proves the timeout fires against a real free-running clock on silicon -- the
part gTest can't reach.

State hygiene: the run leaves CV11=0 (fail-safe disabled) and address 3, in a
finally so a mid-run failure can't leave the DUT armed.

NOTE: requires the decoder firmware rebuilt with dcc_failsafe.c (auto-compiled by
the .cproject glob) and reflashed. Until then DEC-001 will FAIL -- that is honest,
not a regression.

Run:  PLAYER_PORT=... DECODER_PORT=... ../.venv/bin/python s9_2_4_compliance.py
"""
import os
import sys
import time
import glob

import compliance_lib as lib
import wfplayer as wf
import dcc_encode as enc
import serial

SPEC_DOC   = "S-9.2.4"
SPEC_TITLE = "Packet-timeout fail-safe (mobile-decoder receive)"
SOURCE_PDF = "documentation/specs/s-9_2_4_fail-safe.pdf"
ASPECT     = "loss of addressed packets -> controlled stop (DCC into DCC_IN/PB1; RECV oracle)"

DEC_ADDR = 3
A = enc.short_addr(DEC_ADDR)

# 0.1 s/LSB (DCC_FAILSAFE_CV11_UNIT_US). CV11=10 -> a 1.0 s timeout: long enough
# to clear inter-step packet gaps, short enough to keep the suite quick.
CV11_TEST = 10
TIMEOUT_S = CV11_TEST * 0.1


def _discover():
    player = os.environ.get("PLAYER_PORT")
    decoder = os.environ.get("DECODER_PORT")
    if player and decoder:
        return player, decoder
    for p in sorted(glob.glob("/dev/cu.usbmodem*")) + sorted(glob.glob("/dev/ttyACM*")):
        try:
            s = serial.Serial(p, 230400, timeout=0.5); time.sleep(0.3); s.reset_input_buffer()
            s.write(b"ID?\n");  time.sleep(0.15); idr = s.read(120).decode(errors="replace")
            s.reset_input_buffer()
            s.write(b"HELP\n"); time.sleep(0.20); hlp = s.read(160).decode(errors="replace")
            s.close()
            if not player and "wfplayer" in idr:
                player = p
            elif not decoder and ("ADDR" in hlp or "Decoder" in hlp):
                decoder = p
        except Exception:
            pass
    return player, decoder


def _pom(player, pkt, dur=0.3):
    """Play a packet (e.g. a POM CV write) briefly so the decoder applies it."""
    player.load(wf.compose([pkt], lead_idle=2, trail_idle=1))
    player.play(0); time.sleep(dur); player.stop()


def _setaddr(dec, cmd):
    dec.reset_input_buffer(); dec.write((cmd + "\n").encode()); time.sleep(0.25); dec.read(300)


def _drain(dec):
    dec.reset_input_buffer()
    quiet = time.time() + 0.12
    while time.time() < quiet:
        if dec.in_waiting:
            dec.read(dec.in_waiting); quiet = time.time() + 0.12
        else:
            time.sleep(0.01)


def _arm_then_silence(player, dec):
    """Feed one addressed packet (re-arming the timer), then drop the signal."""
    player.load(wf.compose([enc.speed_128(A, 64, True)], lead_idle=0, trail_idle=0))
    _drain(dec)
    player.play(0); time.sleep(0.3); player.stop()


def _listen_for(dec, needle, timeout):
    """Return (line, elapsed) for the first RECV line containing needle, else (None, None)."""
    t0 = time.time()
    deadline = t0 + timeout
    while time.time() < deadline:
        ln = dec.readline().decode(errors="replace").strip()
        if needle in ln:
            return ln, time.time() - t0
    return None, None


def _restore(player, dec):
    """Leave the DUT safe: fail-safe disabled (CV11=0), address 3."""
    _pom(player, enc.cv_write_pom(A, 11, 0))
    _setaddr(dec, "ADDR %d SHORT" % DEC_ADDR)


def checks(rep, player, dec):
    # DEC-001  packet-timeout fail-safe round trip  @compliance DCC-S9.2.4-DEC-001
    _setaddr(dec, "ADDR %d SHORT" % DEC_ADDR)
    _pom(player, enc.cv_write_pom(A, 11, CV11_TEST))         # arm: CV11 -> 1.0 s timeout

    # (1) lose the packet stream -> FAILSAFE_ENTER within ~TIMEOUT_S
    _arm_then_silence(player, dec)
    enter_ln, enter_dt = _listen_for(dec, "RECV FAILSAFE_ENTER", TIMEOUT_S + 1.5)
    entered = enter_ln is not None

    # (2) resume addressed packets -> FAILSAFE_EXIT
    _drain(dec)
    player.load(wf.compose([enc.speed_128(A, 64, True)], lead_idle=0, trail_idle=0))
    player.play(0)
    exit_ln, _ = _listen_for(dec, "RECV FAILSAFE_EXIT", 1.5)
    player.stop()
    exited = exit_ln is not None

    # (3) CV11 = 0 disables the time-out: the same silence must NOT trip it
    _pom(player, enc.cv_write_pom(A, 11, 0))
    _arm_then_silence(player, dec)
    no_trip_ln, _ = _listen_for(dec, "RECV FAILSAFE_ENTER", TIMEOUT_S + 1.0)
    disabled = no_trip_ln is None

    _restore(player, dec)
    rep.check("S-9.2.4", "CV11 packet-timeout enters/exits fail-safe; 0 disables",
              entered and exited and disabled,
              "enter@%ss: %s ; exit: %s ; cv11=0 no-trip: %s"
              % (("%.2f" % enter_dt) if entered else "-", entered, exited, disabled))


def run():
    player_port, decoder_port = _discover()
    if not (player_port and decoder_port):
        raise RuntimeError("player/decoder ports not found (set PLAYER_PORT / DECODER_PORT)")
    print("[ports] player=%s  decoder=%s" % (player_port, decoder_port))
    player = wf.WaveformPlayer(player_port)
    dec = serial.Serial(decoder_port, 230400, timeout=0.4); time.sleep(0.3)
    try:
        rep = lib.Report(SPEC_DOC, SPEC_TITLE, SOURCE_PDF, ASPECT)
        checks(rep, player, dec)
    finally:
        try:
            _restore(player, dec)                            # never leave the DUT armed
        except Exception:
            pass
        try:
            player.stop()
        except Exception:
            pass
        player.close(); dec.close()
    return rep.finish()


def main():
    print("\n#### DCC DECODER COMPLIANCE TEST ####")
    print(f"Spec under test : NMRA {SPEC_DOC}  (packet-timeout fail-safe)")
    print(f"Aspect          : {ASPECT}")
    try:
        rep = run()
    except ImportError as e:
        print(f"\nMissing dependency: {e}\nRun: pip install logic2-automation pyserial\n")
        return 2
    except Exception as e:
        print(f"\nERROR: {e}\nCheck: both boards on USB, player DCC_OUT->decoder DCC_IN wired, "
              f"firmware running (rebuilt with dcc_failsafe.c).\n")
        return 2
    path = lib.write_html([rep.as_dict()], lib.report_path("mobile_decoder_s9_2_4"),
                          title=f"NMRA {SPEC_DOC} Decoder Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
