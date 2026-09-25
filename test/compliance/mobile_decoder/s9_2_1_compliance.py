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

Rows added 2026-09-25 (CV19 consist addressing and accessory filtering landed in
the library; the DUT's `ADDR <n> ACC|ACCE` sets CV513/CV521/CV541 and reloads the
address cache):
  DEC-016  consist set / set-reversed / set-0 write CV19 (bit 7 = reversed) + CONSIST
  DEC-017  the CV19 consist address answers speed, direction (reversed unit reports
           the flipped direction) and e-stop; a third address does not; consist 0 stops it
  DEC-018  an accessory-configured decoder answers only its own board (basic) or
           11-bit (extended) address
  DEC-019  an accessory-configured decoder ignores every multifunction packet,
           broadcast included, and comes back as short address 3 afterwards

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


def _no_recv(player, dec, pkt, window=0.4):
    """Loop a packet and expect the decoder to stay silent: (True, []) when no RECV
    line arrives within `window` seconds after settling, else (False, lines)."""
    lines = _recv(player, dec, pkt, window=window)
    return (len(lines) == 0), lines


def _cmd(dec, line):
    """Send one DUT UART command (e.g. 'ADDR 7 ACC') and swallow its reply."""
    dec.reset_input_buffer(); dec.write((line + "\n").encode()); time.sleep(0.25); dec.read(300)


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

    # 016 consist set/clear writes CV19  @compliance DCC-S9.2.1-DEC-016
    # 017 consist address answers speed/dir/e-stop  @compliance DCC-S9.2.1-DEC-017
    # One flow, two rows: 016 collects the CV19/CONSIST evidence of each consist
    # instruction, 017 the speed/e-stop evidence in between (S-9.2.1 2.3.1.4: speed and
    # direction instructions apply to the consist address; CV19 bit 7 reverses the unit).
    C5, C6 = enc.short_addr(5), enc.short_addr(6)
    r = _recv(player, dec, enc.consist_set(A, 5, True))                  # set 5, normal
    cv5  = pick(r, "CV_WRITE cv=19 value=5")
    cs5  = pick(r, "CONSIST addr=3 consist=5 dir=NORMAL")
    sp5  = pick(_recv(player, dec, enc.speed_128(C5, 64, True)), "SPEED addr=5 speed=64 dir=FWD mode=128")
    es5  = pick(_recv(player, dec, enc.estop_128(C5)), "ESTOP addr=5")
    q6, l6 = _no_recv(player, dec, enc.speed_128(C6, 64, True))         # not us, not consist
    r = _recv(player, dec, enc.consist_set(A, 5, False))                 # set 5, reversed
    cv5r = pick(r, "CV_WRITE cv=19 value=133")                           # 0x85 = 5 | bit 7
    cs5r = pick(r, "CONSIST addr=3 consist=5 dir=REVERSE")
    rev5 = pick(_recv(player, dec, enc.speed_128(C5, 64, True)), "SPEED addr=5 speed=64 dir=REV mode=128")
    r = _recv(player, dec, enc.consist_set(A, 0, True))                  # set 0 = deactivate
    cv0  = pick(r, "CV_WRITE cv=19 value=0")
    cs0  = pick(r, "CONSIST addr=3 consist=0")
    q5, l5 = _no_recv(player, dec, enc.speed_128(C5, 64, True))         # consist gone
    rep.check("S-9.2.1", "consist set 5 / set 5 rev / set 0 -> CV19 = 5, 133, 0 + CONSIST lines",
              all([cv5, cs5, cv5r, cs5r, cv0, cs0]),
              "set5: %s | %s ; rev: %s | %s ; clr: %s | %s"
              % (cv5 or "no CV19=5", cs5 or "no CONSIST", cv5r or "no CV19=133", cs5r or "no CONSIST rev",
                 cv0 or "no CV19=0", cs0 or "no CONSIST 0"))
    rep.check("S-9.2.1", "consist addr 5 answers speed / e-stop / reversed dir; addr 6 and cleared consist silent",
              all([sp5, es5, q6, rev5, q5]),
              "speed@5: %s ; estop@5: %s ; addr6 silent: %s%s ; rev unit dir=REV: %s ; after clear silent: %s%s"
              % (sp5 or "none", es5 or "none", q6, "" if q6 else " %s" % l6[:2],
                 rev5 or "none", q5, "" if q5 else " %s" % l5[:2]))

    # 018 accessory address filter  @compliance DCC-S9.2.1-DEC-018
    # 019 accessory-type guard      @compliance DCC-S9.2.1-DEC-019
    # `ADDR 7 ACC` writes CV513 = 7 & 0x3F, CV521 = 7 >> 6 = 0, CV541 = 0x80 (accessory,
    # basic, decoder-address mode) -> the library's board address is 7; `ADDR 7 ACCE` adds
    # CV541 bit 5 (extended). Basic packets carry the board in A10..A2 (accessory_basic);
    # the extended 11-bit address for board 7, pair bits 00, is 7 << 2 (S-9.2.1 2.4.2).
    _cmd(dec, "ADDR 7 ACC")
    acc7 = pick(_recv(player, dec, enc.accessory_basic(7, 0, True)), "ACC board=7 pair=0 activate=ON")
    q8, l8 = _no_recv(player, dec, enc.accessory_basic(8, 0, True))
    qs7, ls7 = _no_recv(player, dec, enc.speed_128(enc.short_addr(7), 64, True))   # same number, wrong type
    qbc, lbc = _no_recv(player, dec, enc.estop_128(enc.broadcast_addr()))         # multifunction broadcast
    _cmd(dec, "ADDR 7 ACCE")
    ext7 = pick(_recv(player, dec, enc.accessory_extended(7 << 2, 5)), "ACCE addr=7 aspect=5")
    q8e, l8e = _no_recv(player, dec, enc.accessory_extended(8 << 2, 5))
    _cmd(dec, "ADDR %d SHORT" % DEC_ADDR)                                          # restore
    back = pick(_recv(player, dec, enc.speed_128(A, 64, True)), "SPEED addr=3")
    rep.check("S-9.2.1", "accessory addr filter: board 7 basic + ext 7 decode, board/ext 8 silent",
              all([acc7, q8, ext7, q8e]),
              "basic@7: %s ; basic@8 silent: %s%s ; ext@7: %s ; ext@8 silent: %s%s"
              % (acc7 or "none", q8, "" if q8 else " %s" % l8[:2],
                 ext7 or "none", q8e, "" if q8e else " %s" % l8e[:2]))
    rep.check("S-9.2.1", "accessory-configured decoder ignores speed@7 and broadcast e-stop; ADDR 3 SHORT restores",
              all([qs7, qbc, back]),
              "speed@7 silent: %s%s ; bcast estop silent: %s%s ; back@3: %s"
              % (qs7, "" if qs7 else " %s" % ls7[:2], qbc, "" if qbc else " %s" % lbc[:2], back or "none"))

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
        try:                                             # restore even after a mid-run error:
            _cmd(dec, "ADDR %d SHORT" % DEC_ADDR)        # back to short 3 (also clears CV541.7) ...
            _send(player, enc.consist_set(enc.short_addr(DEC_ADDR), 0, True))   # ... and CV19 = 0
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
