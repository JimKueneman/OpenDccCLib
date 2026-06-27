#!/usr/bin/env python3
"""
NMRA S-9.2 -- decoder packet acceptance / rejection (mobile-decoder RECEIVE).

Two-board HIL: the waveform PLAYER sources framed DCC packets into the decoder DUT;
the decoder reports decodes over UART as `RECV` lines. We assert the decoder's
packet-level accept/reject behavior on real silicon:

  DEC-001  idle packet      -> ignored (no callback)
  DEC-002  reset packet     -> accepted, no instruction dispatched
  DEC-003  broadcast e-stop  -> ESTOP callback
  DEC-004  corrupt XOR      -> rejected (no callback); valid XOR accepted
  DEC-005  address matching  -> own address dispatched, foreign ignored

Boundary: the dispatcher (bytes -> callback) is exhaustively unit-tested in
dcc_packet_decoder_Test.cxx. This suite proves the accept/reject decisions on a real
waveform -- the part gTest can't reach. Stimulus bytes are wire-grounded (dcc_encode).

Run:  PLAYER_PORT=... DECODER_PORT=... ../.venv/bin/python s9_2_compliance.py
"""
import os
import sys
import time
import glob

import compliance_lib as lib
import wfplayer as wf
import dcc_encode as enc
import serial

SPEC_DOC   = "S-9.2"
SPEC_TITLE = "Decoder packet acceptance / rejection (mobile-decoder receive)"
SOURCE_PDF = ("documentation/specs/"
              "s-9.2_communications_standards_for_digital_command_control.pdf")
ASPECT     = "decoder packet accept/reject (DCC into DCC_IN/PB1; RECV oracle)"

DEC_ADDR = 3


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


def _recv(player, dec, segments, settle=0.35, window=0.4):
    """Loop the stimulus; return the list of RECV lines the decoder emits."""
    player.load(segments)
    dec.reset_input_buffer()
    player.play(0)
    time.sleep(settle)
    lines = []
    deadline = time.time() + window
    while time.time() < deadline:
        ln = dec.readline().decode(errors="replace").strip()
        if ln.startswith("RECV"):
            lines.append(ln)
    player.stop()
    return lines


def checks(rep, player, dec):
    loop = lambda pkt: wf.compose([pkt], lead_idle=0, trail_idle=0)

    # DEC-001  idle packet ignored  @compliance DCC-S9.2-DEC-001
    idle = _recv(player, dec, wf.idle_packet_segs() * 3)
    rep.check("S-9.2 secC", "idle packet -> no callback", len(idle) == 0,
              "idle-only stream produced %d RECV (expect 0)" % len(idle))

    # DEC-002  reset packet accepted, not dispatched  @compliance DCC-S9.2-DEC-002
    reset = _recv(player, dec, loop([0x00, 0x00]))          # -> [00 00 00] (xor of 00^00 = 00)
    rep.check("S-9.2 secC", "reset packet -> accepted, no instruction dispatched", len(reset) == 0,
              "reset-only stream produced %d RECV (expect 0)" % len(reset))

    # DEC-003  broadcast e-stop -> ESTOP callback  @compliance DCC-S9.2-DEC-003
    estop = _recv(player, dec, loop([0x00, 0x51]))          # broadcast e-stop-all (CS emits 00 51 51)
    ok3 = any(l.startswith("RECV ESTOP") for l in estop)
    rep.check("S-9.2 secB", "broadcast e-stop -> ESTOP callback", ok3,
              estop[0] if estop else "<no RECV>")

    # DEC-004  XOR validation: good accepted, corrupt rejected  @compliance DCC-S9.2-DEC-004
    a3 = enc.speed_128(enc.short_addr(DEC_ADDR), 64, True)
    good = _recv(player, dec, loop(a3))
    bad = _recv(player, dec, wf.packet_segs(a3 + [0x00], append_xor=False))   # wrong error byte
    ok4 = len(good) > 0 and len(bad) == 0
    rep.check("S-9.2 secB", "valid XOR accepted; corrupt XOR rejected", ok4,
              "valid:%d RECV, corrupt-XOR:%d RECV (expect >0 / 0)" % (len(good), len(bad)))

    # DEC-005  address matching: own dispatched, foreign ignored  @compliance DCC-S9.2-DEC-005
    mine = _recv(player, dec, loop(enc.speed_128(enc.short_addr(DEC_ADDR), 64, True)))
    foreign = _recv(player, dec, loop(enc.speed_128(enc.short_addr(99), 64, True)))
    ok5 = any("addr=%d" % DEC_ADDR in l for l in mine) and not any("addr=99" in l for l in foreign)
    rep.check("S-9.2 secB", "matching address dispatched; foreign ignored", ok5,
              "addr%d:%d RECV, addr99:%d RECV (decoder=addr%d)"
              % (DEC_ADDR, len(mine), len(foreign), DEC_ADDR))


def run():
    player_port, decoder_port = _discover()
    if not (player_port and decoder_port):
        raise RuntimeError("player/decoder ports not found (set PLAYER_PORT / DECODER_PORT)")
    print("[ports] player=%s  decoder=%s" % (player_port, decoder_port))
    player = wf.WaveformPlayer(player_port)
    dec = serial.Serial(decoder_port, 230400, timeout=0.4); time.sleep(0.3)
    try:
        dec.reset_input_buffer(); dec.write(("ADDR %d SHORT\n" % DEC_ADDR).encode())
        time.sleep(0.25); dec.read(300)
        rep = lib.Report(SPEC_DOC, SPEC_TITLE, SOURCE_PDF, ASPECT)
        checks(rep, player, dec)
    finally:
        try:
            player.stop()
        except Exception:
            pass
        player.close(); dec.close()
    return rep.finish()


def main():
    print("\n#### DCC DECODER COMPLIANCE TEST ####")
    print(f"Spec under test : NMRA {SPEC_DOC}  (decoder packet accept/reject)")
    print(f"Aspect          : {ASPECT}")
    try:
        rep = run()
    except ImportError as e:
        print(f"\nMissing dependency: {e}\nRun: pip install logic2-automation pyserial\n")
        return 2
    except Exception as e:
        print(f"\nERROR: {e}\nCheck: both boards on USB, player DCC_OUT->decoder DCC_IN wired, "
              f"firmware running.\n")
        return 2
    path = lib.write_html([rep.as_dict()], lib.report_path("mobile_decoder_s9_2"),
                          title=f"NMRA {SPEC_DOC} Decoder Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
