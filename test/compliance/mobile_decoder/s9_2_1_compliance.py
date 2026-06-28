#!/usr/bin/env python3
"""
NMRA S-9.2.1 -- decoder instruction decode, end-to-end (mobile-decoder RECEIVE).

Full-chain HIL integration: the waveform PLAYER sources each instruction type as a
real waveform; the decoder runs the whole path (edge ISR -> bit decoder -> packet
decoder -> dispatch -> callback -> UART) and reports the decoded fields as `RECV`.
We assert the reported fields match what was sent.

Boundary: the dispatcher's exhaustive field/edge-case matrix lives in
dcc_packet_decoder_Test.cxx (gTest, byte-level). This suite is the complementary
*integration* layer gTest can't reach -- one representative decode per instruction
TYPE proven on silicon from a real waveform. Stimulus bytes are wire-grounded
(dcc_encode, verified encoder==wire on the Saleae).

Note on speed values: 128-step reports the raw 7-bit field; 28-step reports V-2
(= step+1); both are the decoder's reporting convention, gTest-pinned. Here we
assert the speed MODE (the integration signal), not re-pin the field value.

Run:  PLAYER_PORT=... DECODER_PORT=... ../.venv/bin/python s9_2_1_compliance.py
"""
import os
import sys
import time
import glob

import compliance_lib as lib
import wfplayer as wf
import dcc_encode as enc
import serial

SPEC_DOC   = "S-9.2.1"
SPEC_TITLE = "Decoder instruction decode, end-to-end (mobile-decoder receive)"
SOURCE_PDF = ("documentation/specs/"
              "s-9.2.1_extended_packet_formats_for_digital_command_control.pdf")
ASPECT     = "decoder instruction decode integration (DCC into DCC_IN/PB1; RECV oracle)"

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


def _drain(dec, quiet_s=0.12):
    """Read and discard until the decoder is silent for quiet_s -- flushes the RECV
    ring left over from the previous test so the next window is uncontaminated."""
    dec.reset_input_buffer()
    quiet = time.time() + quiet_s
    while time.time() < quiet:
        if dec.in_waiting:
            dec.read(dec.in_waiting)
            quiet = time.time() + quiet_s
        else:
            time.sleep(0.01)


def _recv(player, dec, pkt, settle=0.35, window=0.4):
    """Loop a single packet; return the decoder's RECV lines (uncontaminated)."""
    player.load(wf.compose([pkt], lead_idle=0, trail_idle=0))
    _drain(dec)
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


def _send(player, pkt, dur=0.3):
    """Play a packet briefly (no read) -- for CV writes / reconfiguration."""
    player.load(wf.compose([pkt], lead_idle=2, trail_idle=1))
    player.play(0); time.sleep(dur); player.stop()


def checks(rep, player, dec):
    A = enc.short_addr(DEC_ADDR)
    def pick(lines, *frags):
        """The line that satisfies the check (contains all frags), or None."""
        return next((l for l in lines if all(f in l for f in frags)), None)

    def chk(label, lines, *frags):
        m = pick(lines, *frags)
        rep.check("S-9.2.1", label, m is not None, m or "<no match in %d RECV>" % len(lines))

    # 001 128-step speed  @compliance DCC-S9.2.1-DEC-001
    chk("128-step speed -> SPEED mode=128 speed=64",
        _recv(player, dec, enc.speed_128(A, 64, True)),
        "SPEED addr=3 speed=64 dir=FWD mode=128")

    # 002 28-step speed  @compliance DCC-S9.2.1-DEC-002
    chk("28-step speed -> SPEED mode=28",
        _recv(player, dec, enc.speed_28(A, 10, True)), "SPEED addr=3", "mode=28")

    # 003 14-step speed (CV29 bit1=0)  @compliance DCC-S9.2.1-DEC-003
    _send(player, enc.cv_bit_pom(A, 29, 1, 0))                    # -> 14-step mode
    r = _recv(player, dec, enc.speed_28(A, 7, True))
    _send(player, enc.cv_bit_pom(A, 29, 1, 1))                    # restore 28/128-step
    m = pick(r, "SPEED addr=3", "mode=14")
    rep.check("S-9.2.1", "14-step speed (CV29.bit1=0) -> SPEED mode=14",
              m is not None, (m or "<no mode=14 in %d RECV>" % len(r)) + " [CV29 restored]")

    # 004 function group 1  @compliance DCC-S9.2.1-DEC-004
    chk("FG1 F0 -> FUNC func=0 ON", _recv(player, dec, enc.function_group_1(A, f0=1)),
        "FUNC addr=3 func=0 state=ON")

    # 005 function group 2a (F5-F8)  @compliance DCC-S9.2.1-DEC-005
    chk("FG2a F5 -> FUNC func=5 ON", _recv(player, dec, enc.function_f5_f8(A, f5=1)),
        "FUNC addr=3 func=5 state=ON")

    # 006 function group 2b (F9-F12)  @compliance DCC-S9.2.1-DEC-006
    chk("FG2b F9 -> FUNC func=9 ON", _recv(player, dec, enc.function_f9_f12(A, f9=1)),
        "FUNC addr=3 func=9 state=ON")

    # 007 function expansion F13-F68 (F13, F21 representative)  @compliance DCC-S9.2.1-DEC-007
    m13 = pick(_recv(player, dec, enc.function_expansion(A, 13)), "func=13 state=ON")
    m21 = pick(_recv(player, dec, enc.function_expansion(A, 21)), "func=21 state=ON")
    rep.check("S-9.2.1", "function expansion F13 & F21 -> FUNC ON",
              bool(m13 and m21), "%s | %s" % (m13 or "F13 none", m21 or "F21 none"))

    # 010 CV ops-mode write / verify / bit  @compliance DCC-S9.2.1-DEC-010
    mw = pick(_recv(player, dec, enc.cv_write_pom(A, 8, 90)), "CV_WRITE cv=8 value=90")
    mv = pick(_recv(player, dec, enc.cv_verify_pom(A, 8, 90)), "CV_VERIFY cv=8 value=90")
    mb = pick(_recv(player, dec, enc.cv_bit_pom(A, 8, 3, 1)), "CV_BIT cv=8 bit=3 value=1")
    rep.check("S-9.2.1", "CV ops-mode write/verify/bit", all([mw, mv, mb]),
              "%s | %s | %s" % (mw or "write none", mv or "verify none", mb or "bit none"))

    # 011 consist set  @compliance DCC-S9.2.1-DEC-011
    chk("consist set -> CONSIST consist=5 NORMAL", _recv(player, dec, enc.consist(A, 5, True)),
        "CONSIST addr=3 consist=5 dir=NORMAL")

    # 012 binary state short  @compliance DCC-S9.2.1-DEC-012
    chk("binary state short -> BSS state=5 ON", _recv(player, dec, enc.binary_state_short(A, 5, True)),
        "BSS addr=3 state=5 active=ON")

    # 013 binary state long  @compliance DCC-S9.2.1-DEC-013
    chk("binary state long -> BSL state=300 ON", _recv(player, dec, enc.binary_state_long(A, 300, True)),
        "BSL addr=3 state=300 active=ON")

    # 014 analog function group  @compliance DCC-S9.2.1-DEC-014
    chk("analog function -> ANALOG output=1 value=128", _recv(player, dec, enc.analog_function(A, 1, 128)),
        "ANALOG addr=3 output=1 value=128")

    # 015 short and long addressing  @compliance DCC-S9.2.1-DEC-015
    ms = pick(_recv(player, dec, enc.speed_128(A, 64, True)), "SPEED addr=3")
    dec.reset_input_buffer(); dec.write(b"ADDR 1234 LONG\n"); time.sleep(0.25); dec.read(300)
    ml = pick(_recv(player, dec, enc.speed_128(enc.long_addr(1234), 50, True)), "SPEED addr=1234")
    dec.reset_input_buffer(); dec.write(("ADDR %d SHORT\n" % DEC_ADDR).encode()); time.sleep(0.25); dec.read(300)
    rep.check("S-9.2.1", "short addr 3 and long addr 1234 both decode",
              bool(ms and ml), "%s | %s [addr restored]" % (ms or "short none", ml or "long none"))

    # 008/009 basic & extended accessory: accessory-decoder role -- covered there, not here.
    rep.na("S-9.2.1", "basic / extended accessory decode",
           "accessory-decoder role (separate rig); not a mobile-decoder concern")


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
    print(f"Spec under test : NMRA {SPEC_DOC}  (decoder instruction decode)")
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
    path = lib.write_html([rep.as_dict()], lib.report_path("mobile_decoder_s9_2_1"),
                          title=f"NMRA {SPEC_DOC} Decoder Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
