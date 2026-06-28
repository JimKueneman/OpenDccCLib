#!/usr/bin/env python3
"""
NMRA S-9.2.2 -- decoder CV behavioral effects (mobile-decoder RECEIVE).

CVs are verified the only honest way on the wire: write a CV via POM, then observe
the decoder's behavior change. Two rows:

  DEC-001  CV read/write via callbacks  -- a POM write of CV1 moves the decoder's address
  DEC-002  Decoder lock (CV15/CV16)     -- a lock blocks a CV1 write; unlocking restores it

Both effects were spiked on hardware before this suite existed: the POM path applies
CV1 at runtime, and the lock is enforced on the POM path (not just in the library).
gTest already covers the storage/lock LOGIC (dcc_cv_storage_Test.cxx, 16 tests); this
suite proves the effect is real on silicon -- the part gTest can't reach.

State hygiene: every check restores the decoder (ADDR 3, lock cleared); run() also
restores in a finally so a mid-run failure can't leave the DUT locked.

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


def _restore(player, dec):
    """Return the DUT to a known state: unlocked, address 3."""
    _pom(player, enc.cv_write_pom(A, 15, 0))
    _pom(player, enc.cv_write_pom(A, 16, 0))
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
