#!/usr/bin/env python3
"""
NMRA S-9.1 -- decoder timing acceptance (mobile-decoder RECEIVE).

Two-board HIL: the waveform PLAYER sources DCC with controlled bit timing / preamble
into the decoder DUT (player DCC_OUT/PB1 -> decoder DCC_IN/PB1, common ground); the
decoder reports decodes over UART as `RECV` lines. We assert the decoder ACCEPTS
spec-valid stimuli (a `RECV SPEED` appears) and REJECTS clearly out-of-spec ones
(no `RECV`).

Boundary note: the decoder's bit-decode logic is unit-tested exhaustively in
dcc_bit_decoder_Test.cxx (exact 9-vs-10 preamble, window edges). This suite does the
part gTest structurally cannot -- proving acceptance on real silicon driven by an
actual waveform with real edge timing. It does NOT re-test the dispatcher.

The stimulus is a speed packet whose bytes were independently ground-truthed on the
Saleae (encoder == wire), so a `RECV SPEED addr=3` here is a real, valid decode.

Run:  PLAYER_PORT=... DECODER_PORT=... ../.venv/bin/python s9_1_compliance.py
      (ports auto-discover if unset)
"""
import os
import sys
import time
import glob

import compliance_lib as lib
import wfplayer as wf
import dcc_encode as enc
import serial

SPEC_DOC   = "S-9.1"
SPEC_TITLE = "Decoder timing acceptance (mobile-decoder receive)"
SOURCE_PDF = ("documentation/specs/"
              "s-9.1_electrical_standards_for_digital_command_control.pdf")
ASPECT     = "decoder bit-timing & preamble acceptance (DCC into DCC_IN/PB1; RECV oracle)"

DEC_ADDR = 3
SPEED    = enc.speed_128(enc.short_addr(DEC_ADDR), 64, forward=True)   # ground-truthed stimulus
EXPECT   = "RECV SPEED addr=%d" % DEC_ADDR


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


def _decodes(player, dec, segments, settle=0.35, window=0.4):
    """Loop the stimulus; return True if the decoder reports a SPEED decode for our address."""
    player.load(segments)
    dec.reset_input_buffer()
    player.play(0)
    time.sleep(settle)
    hit = False
    deadline = time.time() + window
    while time.time() < deadline:
        ln = dec.readline().decode(errors="replace").strip()
        if ln.startswith(EXPECT):
            hit = True
    player.stop()
    return hit


def checks(rep, player, dec):
    seg = lambda **kw: wf.compose([SPEED], lead_idle=0, trail_idle=0, **kw)

    # 1. ONE half-bit accepted across the full 55-61 us window  @compliance DCC-S9.1-DEC-001
    res = [(h, _decodes(player, dec, seg(one_half_us=h))) for h in (55, 58, 61)]
    rep.check("S-9.1 Tbl2.1", "ONE half-bit 55/58/61 us accepted (decode)",
              all(g for _, g in res),
              ", ".join("%dus:%s" % (h, "OK" if g else "no-decode") for h, g in res))

    # 2. preamble accept >=10 / reject short  @compliance DCC-S9.1-DEC-002
    acc = _decodes(player, dec, seg(preamble=10))
    rej = _decodes(player, dec, seg(preamble=5))
    rep.check("S-9.2 sec5", "preamble >=10 accepted; short (5) rejected",
              acc and not rej,
              "preamble10: %s; preamble5: %s"
              % ("decode" if acc else "none", "decode(BAD)" if rej else "none(reject)"))

    # 3. stretched zero halves accepted  @compliance DCC-S9.1-DEC-003
    res = [(z, _decodes(player, dec, seg(zero_half_us=z))) for z in (90, 200)]
    rep.check("S-9.1 sec2", "stretched zero (90/200 us half) accepted",
              all(g for _, g in res),
              ", ".join("%dus:%s" % (z, "OK" if g else "no-decode") for z, g in res))


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
    print(f"Spec under test : NMRA {SPEC_DOC}  (decoder timing acceptance)")
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
    path = lib.write_html([rep.as_dict()], lib.report_path("mobile_decoder_s9_1"),
                          title=f"NMRA {SPEC_DOC} Decoder Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
