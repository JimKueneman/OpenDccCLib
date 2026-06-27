"""
mobile_decoder — decode smoke test (first real player→decoder→RECV loop).

Proves the two-board loop end to end: the waveform PLAYER emits a known DCC packet
into the decoder DUT (player DCC_OUT/PB1 -> decoder DCC_IN/PB1, common ground), and
the decoder reports what it decoded over its UART as `RECV ...` lines. We set the
decoder's address, play a speed packet to it, and confirm the report.

  host --UART--> player ==DCC(PB1)==> decoder --UART(RECV lines)--> host (oracle)

Ports: set PLAYER_PORT / DECODER_PORT, or let the script auto-discover them (the
player answers `ID?` with 'wfplayer'; the decoder answers `HELP` with its menu).

    ../.venv/bin/python decoder_smoke.py

TODO to grow into the real mobile_decoder suites:
  - an independent DCC instruction encoder (speed/function/accessory/CV/...) so every
    S-9.2.1-DEC row can be driven, not one hardcoded packet;
  - acceptance/rejection tests using the player's marginal-timing (P1) + preamble (P2);
  - `@compliance <tid>` tags + run() -> Report so the DEC rows in compliance.data.js light up.
"""
import os
import time
import glob

import wfplayer as wf          # symlink -> ../saleae_hil_waveform_player/wfplayer.py
import serial

DEC_ADDR = 3
PACKET   = [0x03, 0x3F, 0x40]              # addr 3, 128-step speed, data 0x40 (speed 64, REV)
EXPECT   = "RECV SPEED addr=3 speed=64"    # what the decoder must report back


def _discover():
    """Return (player_port, decoder_port); probe the usbmodem ports if env vars are unset."""
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


def run():
    player_port, decoder_port = _discover()
    if not (player_port and decoder_port):
        print("SKIP: could not find player (%s) and/or decoder (%s)" % (player_port, decoder_port))
        return None
    print("player=%s  decoder=%s" % (player_port, decoder_port))

    player = wf.WaveformPlayer(player_port)
    dec = serial.Serial(decoder_port, 230400, timeout=0.4)
    try:
        time.sleep(0.3); dec.reset_input_buffer()
        dec.write(("ADDR %d SHORT\n" % DEC_ADDR).encode()); time.sleep(0.2)
        ack = [l.strip() for l in dec.read(200).decode(errors="replace").splitlines()
               if "OK" in l or "ERR" in l]
        print("set decoder addr:", ack)

        player.load(wf.compose([PACKET], lead_idle=2, trail_idle=1))   # auto-calibrated
        dec.reset_input_buffer()
        player.play(0)                                                 # stream continuously
        time.sleep(0.4)
        lines, deadline = [], time.time() + 0.6
        while time.time() < deadline:
            ln = dec.readline().decode(errors="replace").strip()
            if ln:
                lines.append(ln)
        player.stop()
    finally:
        player.close()
        dec.close()

    recv = [l for l in lines if l.startswith("RECV")]
    hit = [l for l in recv if l.startswith(EXPECT)]
    print("decoder RECV lines: %d; matching %r: %d" % (len(recv), EXPECT, len(hit)))
    for l in hit[:3]:
        print("   ", l)
    ok = bool(hit)
    if not ok:
        print("   FAIL — no matching decode. Check PB1->PB1 jumper + common ground between boards.")
    print("RESULT:", "PASS" if ok else "FAIL")
    return ok


if __name__ == "__main__":
    import sys
    sys.exit(1 if run() is False else 0)     # skip (None) -> 0, pass -> 0, fail -> 1
