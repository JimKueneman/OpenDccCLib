#!/usr/bin/env python3
"""
NMRA S-9.2.2 -- decoder CV behavioral effects (mobile-decoder RECEIVE).

CVs are verified the only honest way on the wire: write a CV via POM, then observe
the decoder's behavior change. Two rows:

  DEC-001  CV read/write via callbacks  -- a POM write of CV1 moves the decoder's address
  DEC-002  Decoder lock (CV15/CV16)     -- a lock blocks a CV1 write; unlocking restores it
  DEC-003  factory reset via CV8        -- CV8 := 8 restores the default address
  DEC-004  CV29 decode + notify         -- a CV29 write reports its named flags
  DEC-005  indexed CV access            -- CV31/CV32 route a CV257-512 write to page/offset
  DEC-006  CV21/CV22 consist functions  -- (added 2026-09-25) with CV19 = 5, functions sent
           to the consist address are delivered only where CV21 (F1-F8, bit 0 = F1) /
           CV22 bits 2-5 (F9-F12) enable them; the own address is unaffected
  DEC-007  CV22 FL direction gating     -- (added 2026-09-25) CV22 bit 0 / bit 1 deliver FL
           on the consist address only after a forward / reverse speed respectively

Both effects were spiked on hardware before this suite existed: the POM path applies
CV1 at runtime, and the lock is enforced on the POM path (not just in the library).
gTest already covers the storage/lock LOGIC (dcc_cv_storage_Test.cxx, 16 tests); this
suite proves the effect is real on silicon -- the part gTest can't reach.

State hygiene: every check restores the decoder (ADDR 3, lock cleared, CV19/21/22 = 0);
run() also restores in a finally so a mid-run failure can't leave the DUT locked or
in a consist.

Run:  PLAYER_PORT=... DECODER_PORT=... ../.venv/bin/python s9_2_2_compliance.py
"""
import os
import sys
import time
import glob

import compliance_lib as lib
import wfplayer as wf
import dcc_encode as enc
import serial

SPEC_DOC   = "S-9.2.2"
SPEC_TITLE = "Decoder CV behavioral effects (mobile-decoder receive)"
SOURCE_PDF = ("documentation/specs/"
              "s-9.2.2_configuration_variables_for_digital_command_control.pdf")
ASPECT     = "decoder CV write -> behavioral effect (DCC into DCC_IN/PB1; RECV oracle)"

DEC_ADDR = 3
A = enc.short_addr(DEC_ADDR)


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


def _responds_to(player, dec, n):
    """True if the decoder reports a SPEED decode for short address n."""
    player.load(wf.compose([enc.speed_128(enc.short_addr(n), 64, True)], lead_idle=0, trail_idle=0))
    _drain(dec)
    player.play(0); time.sleep(0.35)
    hit = False
    deadline = time.time() + 0.4
    while time.time() < deadline:
        ln = dec.readline().decode(errors="replace").strip()
        if ln.startswith("RECV SPEED addr=%d" % n):
            hit = True
    player.stop()
    return hit


def _recv(player, dec, pkt, settle=0.35, window=0.4):
    """Loop a single packet; return the decoder's RECV lines (uncontaminated)."""
    player.load(wf.compose([pkt], lead_idle=0, trail_idle=0))
    _drain(dec)
    player.play(0); time.sleep(settle)
    lines, deadline = [], time.time() + window
    while time.time() < deadline:
        ln = dec.readline().decode(errors="replace").strip()
        if ln.startswith("RECV"):
            lines.append(ln)
    player.stop()
    return lines


def _funcs(player, dec, pkt, addr):
    """Loop a function packet; return the SET of function numbers the decoder reported
    for `addr` (empty set = no FUNC line within the window = the packet was gated)."""
    out = set()
    for ln in _recv(player, dec, pkt):
        if ln.startswith("RECV FUNC addr=%d func=" % addr):
            out.add(int(ln.split("func=")[1].split()[0]))
    return out


def _restore(player, dec):
    """Return the DUT to a known state: unlocked, no consist, address 3."""
    _pom(player, enc.cv_write_pom(A, 15, 0))
    _pom(player, enc.cv_write_pom(A, 16, 0))
    _pom(player, enc.cv_write_pom(A, 19, 0))                 # no consist ...
    _pom(player, enc.cv_write_pom(A, 21, 0))                 # ... and no consist functions
    _pom(player, enc.cv_write_pom(A, 22, 0))
    _setaddr(dec, "ADDR %d SHORT" % DEC_ADDR)


def checks(rep, player, dec):
    # DEC-001  POM CV1 write moves the decoder's address  @compliance DCC-S9.2.2-DEC-001
    _setaddr(dec, "ADDR %d SHORT" % DEC_ADDR)
    _pom(player, enc.cv_write_pom(A, 1, 7))
    moved = _responds_to(player, dec, 7) and not _responds_to(player, dec, 3)
    _setaddr(dec, "ADDR %d SHORT" % DEC_ADDR)                 # restore address
    back = _responds_to(player, dec, DEC_ADDR)
    rep.check("S-9.2.2", "POM CV1 write moves decoder address (write path effective)",
              moved and back,
              "CV1:=7 -> responds@7 & not@3: %s ; restored@3: %s" % (moved, back))

    # DEC-002  decoder lock (CV15/CV16) blocks a CV write  @compliance DCC-S9.2.2-DEC-002
    _pom(player, enc.cv_write_pom(A, 15, 1))                  # CV15 != CV16 -> locked
    _pom(player, enc.cv_write_pom(A, 16, 2))
    _pom(player, enc.cv_write_pom(A, 1, 9))                   # attempt address change while locked
    blocked = (not _responds_to(player, dec, 9)) and _responds_to(player, dec, 3)
    _pom(player, enc.cv_write_pom(A, 16, 1))                  # CV16 == CV15 -> unlocked
    _pom(player, enc.cv_write_pom(A, 1, 9))                   # same write now permitted
    allowed = _responds_to(player, dec, 9)
    _restore(player, dec)
    rep.check("S-9.2.2", "decoder lock blocks CV write; unlock permits it",
              blocked and allowed,
              "locked->blocked(@3, not@9): %s ; unlocked->written(@9): %s" % (blocked, allowed))

    # @compliance DCC-S9.2.2-DEC-003 -- factory reset via CV8. A write of 8 to CV8 (the read-only
    # Manufacturer ID) restores defaults -- NOT writes the value -- so the address returns to 3.
    # NOTE: after CV1:=7 the decoder is at address 7, so the CV8 POM write must target 7 (not the
    # original address). Verified on hardware (needs the CV8 cache-refresh fix in dcc_packet_decoder.c).
    _setaddr(dec, "ADDR %d SHORT" % DEC_ADDR)
    _pom(player, enc.cv_write_pom(A, 1, 7))                  # move address to 7
    moved = _responds_to(player, dec, 7) and not _responds_to(player, dec, 3)
    _pom(player, enc.cv_write_pom(enc.short_addr(7), 8, 8))  # CV8:=8 -> factory reset (decoder is now at addr 7, POM must target 7)
    reset_ok = _responds_to(player, dec, DEC_ADDR) and not _responds_to(player, dec, 7)
    _restore(player, dec)
    rep.check("S-9.2.2", "factory reset (write CV8=8) restores default address",
              moved and reset_ok,
              "CV1:=7 moved@7: %s ; CV8:=8 restored default@%d: %s" % (moved, DEC_ADDR, reset_ok))

    # @compliance DCC-S9.2.2-DEC-005 -- indexed CV access. CV31/CV32 select a page; CV257-512 is
    # the 256-byte window into it. Point at page 2, write CV(257+5)=262=99, and the decoder
    # reports the routed indexed write (RECV CVIDX page=2 off=5 val=99). Verified on hardware.
    _pom(player, enc.cv_write_pom(A, 31, 0))                 # CV31 page high = 0
    _pom(player, enc.cv_write_pom(A, 32, 2))                 # CV32 page low  = 2
    player.load(wf.compose([enc.cv_write_pom(A, 262, 99)], lead_idle=2, trail_idle=1))
    _drain(dec); player.play(0); time.sleep(0.4)
    idx, dl = [], time.time() + 0.4
    while time.time() < dl:
        ln = dec.readline().decode(errors="replace").strip()
        if ln.startswith("RECV CVIDX"):
            idx.append(ln)
    player.stop()
    _pom(player, enc.cv_write_pom(A, 31, 0)); _pom(player, enc.cv_write_pom(A, 32, 0))   # reset page pointer
    hit = next((l for l in idx if "page=2" in l and "off=5" in l and "val=99" in l), None)
    rep.check("S-9.2.2", "indexed CV write routes through CV31/32 to page/offset",
              hit is not None, hit or ("<no RECV CVIDX> (got %s)" % (idx[:2] or "none")))

    # @compliance DCC-S9.2.2-DEC-004 -- CV29 decode + notify. The library decodes a CV29 write
    # into named flags and hands them to the app, which reports them (RECV CV29 ...). Reserved-
    # bit (bit6) sanitization is gTest-verified on host (not observable on the wire without a
    # read-back). Verified on hardware.
    CV29_VAL = 0x2A                                          # bit1 steps + bit3 railcom + bit5 extended
    player.load(wf.compose([enc.cv_write_pom(A, 29, CV29_VAL)], lead_idle=2, trail_idle=1))
    _drain(dec); player.play(0); time.sleep(0.4)
    c29, dl = [], time.time() + 0.4
    while time.time() < dl:
        ln = dec.readline().decode(errors="replace").strip()
        if ln.startswith("RECV CV29"):
            c29.append(ln)
    player.stop()
    _pom(player, enc.cv_write_pom(A, 29, 0x06))              # restore CV29 to default
    hit = next((l for l in c29 if "steps=1" in l and "railcom=1" in l
                and "extaddr=1" in l and "dir=0" in l), None)
    rep.check("S-9.2.2", "CV29 write decodes to named config flags (on_cv29_config_changed)",
              hit is not None, hit or ("<no RECV CV29> (got %s)" % (c29[:2] or "none")))

    # @compliance DCC-S9.2.2-DEC-006 -- CV21/CV22 gate the functions a consist-addressed packet
    # may drive (S-9.2.1 2.3.1.4: FG1/FG2 "also respond to the consist address if the
    # appropriate bits in CVs 21 and 22 have been activated"). CV19 := 5 via the consist
    # instruction; CV21 bit n-1 = Fn (F1-F8); CV22 bits 2-5 = F9-F12. The FUNC lines carry the
    # packet's address (5), so the set of reported function numbers is the evidence.
    C5 = enc.short_addr(5)
    FG1_ALL5 = enc.function_group_1(C5, 1, 1, 1, 1, 1)                  # FL, F1-F4 all on
    _setaddr(dec, "ADDR %d SHORT" % DEC_ADDR)
    _pom(player, enc.cv_write_pom(A, 21, 0)); _pom(player, enc.cv_write_pom(A, 22, 0))
    _pom(player, enc.consist_set(A, 5, True))                            # CV19 := 5, normal
    f_none = _funcs(player, dec, FG1_ALL5, 5)                            # CV21 = 0 -> nothing
    _pom(player, enc.cv_write_pom(A, 21, 0x05))                          # F1, F3 enabled
    f13 = _funcs(player, dec, FG1_ALL5, 5)
    _pom(player, enc.cv_write_pom(A, 22, 0x24))                          # bits 2,5 = F9, F12
    f912 = _funcs(player, dec, enc.function_f9_f12(C5, 1, 1, 1, 1), 5)
    own = _funcs(player, dec, enc.function_group_1(A, 1, 1, 1, 1, 1), DEC_ADDR)   # own addr: all
    rep.check("S-9.2.2", "CV21/CV22 gate consist-addr functions: none -> F1,F3 -> F9,F12; own addr all",
              f_none == set() and f13 == {1, 3} and f912 == {9, 12} and own == {0, 1, 2, 3, 4},
              "CV21=0: %s ; CV21=0x05: %s ; CV22=0x24 FG2b: %s ; own FG1: %s"
              % (sorted(f_none) or "none", sorted(f13), sorted(f912), sorted(own)))

    # @compliance DCC-S9.2.2-DEC-007 -- CV22 bit 0 (FL forward) / bit 1 (FL reverse): FL on the
    # consist address follows the direction the decoder last reported to on_speed_command.
    # CV22 = 0x01: FL is delivered after a forward speed, not after a reverse speed.
    _pom(player, enc.cv_write_pom(A, 22, 0x01))
    _pom(player, enc.speed_128(C5, 30, True))                            # last dir = forward
    fl_fwd = 0 in _funcs(player, dec, FG1_ALL5, 5)
    _pom(player, enc.speed_128(C5, 30, False))                           # last dir = reverse
    fl_rev = 0 in _funcs(player, dec, FG1_ALL5, 5)
    _restore(player, dec)                                                # CV19/21/22 := 0
    rep.check("S-9.2.2", "CV22.bit0 FL gating on consist addr: FL after forward speed, not after reverse",
              fl_fwd and not fl_rev,
              "FL after fwd: %s ; FL after rev: %s [CV19/21/22 restored to 0]" % (fl_fwd, fl_rev))


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
            _restore(player, dec)                            # never leave the DUT locked
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
    print(f"Spec under test : NMRA {SPEC_DOC}  (decoder CV behavioral effects)")
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
    path = lib.write_html([rep.as_dict()], lib.report_path("mobile_decoder_s9_2_2"),
                          title=f"NMRA {SPEC_DOC} Decoder Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
