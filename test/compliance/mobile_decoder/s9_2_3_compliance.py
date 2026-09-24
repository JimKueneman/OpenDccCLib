"""
NMRA S-9.2.3 -- Service Mode: DECODER-side acknowledgment (mobile-decoder receive)

Hardware-in-the-loop, decoder as the DUT. The waveform player streams a real
service-mode sequence (three reset packets, then Direct-mode VERIFY CV BYTE
packets with the 20-bit service preamble) into the decoder's DCC_IN; the Saleae
captures the decoder's ACK_OUT (PB3, channel 1) alongside the DCC line (channel
0) and measures the acknowledgment pulse the DECODER produces.

What this proves that the gTest suite cannot: the whole start -> 6 ms -> stop
path through the library's DccConfig wiring on real hardware. The library owns
the timing -- the app driver only raises the pin on start_ack_pulse and lowers
it on stop_ack_pulse -- so a pulse that never ends, or ends at the wrong time,
shows up here and nowhere else. (The decoder's `ACK TEST` UART command uses the
driver's self-timed hardware one-shot instead and does NOT exercise this path.)

Spec basis: S-9.2.3 Section D -- basic acknowledgment is an increased load of
at least 60 mA for 6 ms +/- 1 ms; Section E -- VERIFY CV BYTE acknowledges only
when the CV holds the given value.

Run:  ../.venv/bin/python s9_2_3_compliance.py     (see HIL_SETUP.md)
"""

import os
import sys
import time
import glob
import tempfile

import compliance_lib as lib
import wfplayer as wf
import dcc_encode as enc
import serial

SPEC_DOC   = "S-9.2.3"
SPEC_TITLE = "Service-mode acknowledgment pulse (mobile-decoder receive)"
SOURCE_PDF = "documentation/specs/S-9.2.3_2012_07.pdf"
ASPECT     = "decoder ACK_OUT (PB3, Saleae D1) on a real service-mode verify sequence"

DEC_ADDR    = 3
A           = enc.short_addr(DEC_ADDR)
ACK_CHANNEL = 1                      # Saleae D1 <- decoder ACK_OUT (PB3), see HIL_SETUP.md
DCC_CHANNEL = lib.DIGITAL_CHANNEL    # Saleae D0 <- DCC line

TEST_CV     = 5                      # a plain read/write CV with no side effects
TEST_VALUE  = 0x5A

ACK_NOMINAL_US = 6000.0              # S-9.2.3 Section D
ACK_MIN_US     = 5000.0
ACK_MAX_US     = 7000.0


# ---------------------------------------------------------------------------
# Ports / DUT helpers (same shape as s9_2_2_compliance.py)
# ---------------------------------------------------------------------------

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


def _dec_cmd(dec, cmd, settle=0.25):
    dec.reset_input_buffer(); dec.write((cmd + "\n").encode()); time.sleep(settle)
    return dec.read(300).decode(errors="replace")


def _pom(player, pkt, dur=0.3):
    """Play an ops-mode packet briefly so the decoder applies it (no ACK in ops mode)."""
    player.load(wf.compose([pkt], lead_idle=2, trail_idle=1))
    player.play(0); time.sleep(dur); player.stop()


# ---------------------------------------------------------------------------
# Stimulus + measurement
# ---------------------------------------------------------------------------

def _service_sequence(verify_pkt, repeats):
    """3 resets, then `repeats` copies of the verify, all with the 20-bit service
    preamble (S-9.2.3 Direct-mode packet sequence). A few idles either side so
    the capture window has quiet edges."""
    pkts = [enc.reset_packet()] * 3 + [verify_pkt] * repeats
    return wf.compose(pkts, lead_idle=3, trail_idle=6, preamble=wf.PREAMBLE_SERVICE)


def _ack_pulses(rows):
    """High pulses on the ACK channel as (start_s, width_us). rows = (t, level)."""
    pulses = []
    rise = None
    for t, lvl in rows:
        if lvl == 1 and rise is None:
            rise = t
        elif lvl == 0 and rise is not None:
            pulses.append((rise, (t - rise) * 1e6))
            rise = None
    if rise is not None:
        pulses.append((rise, float("inf")))          # still high at capture end
    return pulses


def _capture_ack(player, segments, capture_seconds=0.40):
    """Play `segments` once inside a live two-channel capture; return the ACK
    pulses seen on D1 and the number of DCC transitions on D0 (sanity)."""
    player.load(segments)

    def stimulus():
        player.play(1)

    with tempfile.TemporaryDirectory() as d:
        paths = lib.capture_to_csv_multi([DCC_CHANNEL, ACK_CHANNEL], d,
                                         stimulus=stimulus, capture_seconds=capture_seconds)
        ack_rows = lib.read_transitions(paths[ACK_CHANNEL])
        dcc_rows = lib.read_transitions(paths[DCC_CHANNEL])
    player.stop()
    return _ack_pulses(ack_rows), len(dcc_rows)


def _fmt(pulses):
    return ", ".join("%.0f us" % w if w != float("inf") else "STUCK HIGH" for _, w in pulses) or "none"


# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------

def checks(rep, player, dec):
    D = SPEC_DOC + " Section D (acknowledgment)"
    E = SPEC_DOC + " Section E (Direct mode verify)"

    # Known state: address 3, ACK hardware enabled, CV5 = 0x5A via an ops-mode write.
    _dec_cmd(dec, "ADDR %d SHORT" % DEC_ADDR)
    _dec_cmd(dec, "ACK ON")
    _pom(player, enc.cv_write_pom(A, TEST_CV, TEST_VALUE))

    # --- one matching verify -> exactly one pulse, 6 ms +/- 1 ms -----------------
    pulses, n_dcc = _capture_ack(player, _service_sequence(
        enc.svc_direct_verify_byte(TEST_CV, TEST_VALUE), repeats=1))
    rep.check(D, "DCC stimulus reached the analyzer (D0 has transitions)", n_dcc > 100,
              "%d transitions on D0" % n_dcc)
    rep.check(E, "matching VERIFY CV BYTE after 3 resets -> one ACK pulse on ACK_OUT",
              len(pulses) == 1, "pulses: " + _fmt(pulses))
    if len(pulses) == 1:
        w = pulses[0][1]
        rep.check(D, "ACK pulse ends: library stop_ack_pulse fired (not stuck high)",
                  w != float("inf"), "width: " + _fmt(pulses))
        rep.check(D, "ACK pulse width 5000-7000 us (nominal 6000)",
                  ACK_MIN_US <= w <= ACK_MAX_US,
                  "%.0f us (%+.0f us from nominal)" % (w, w - ACK_NOMINAL_US)
                  if w != float("inf") else "stuck high")

    # --- the spec-shaped sequence: 5 verifies -> 5 distinct in-window pulses -----
    pulses, _ = _capture_ack(player, _service_sequence(
        enc.svc_direct_verify_byte(TEST_CV, TEST_VALUE), repeats=5))
    widths = [w for _, w in pulses]
    rep.check(E, "5 matching verifies -> one ACK per packet (5 pulses, none merged)",
              len(pulses) == 5, "pulses: " + _fmt(pulses))
    rep.check(D, "every repeated ACK pulse is 5000-7000 us",
              bool(widths) and all(ACK_MIN_US <= w <= ACK_MAX_US for w in widths),
              "widths: " + _fmt(pulses))

    # --- non-matching verify -> silence ------------------------------------------
    pulses, _ = _capture_ack(player, _service_sequence(
        enc.svc_direct_verify_byte(TEST_CV, TEST_VALUE ^ 0xFF), repeats=5))
    rep.check(E, "non-matching VERIFY CV BYTE -> no ACK pulse",
              len(pulses) == 0, "pulses: " + _fmt(pulses))

    # --- verify without the reset preamble -> silence (service mode not armed) ---
    segs = wf.compose([enc.svc_direct_verify_byte(TEST_CV, TEST_VALUE)] * 5,
                      lead_idle=3, trail_idle=6, preamble=wf.PREAMBLE_SERVICE)
    pulses, _ = _capture_ack(player, segs)
    rep.check(E, "matching verify with no reset packets first -> no ACK (not in service mode)",
              len(pulses) == 0, "pulses: " + _fmt(pulses))


def _restore(player, dec):
    _dec_cmd(dec, "ADDR %d SHORT" % DEC_ADDR)


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
            _restore(player, dec)
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
    print(f"Spec under test : NMRA {SPEC_DOC}  (decoder-side acknowledgment)")
    print(f"Aspect          : {ASPECT}")
    try:
        rep = run()
    except ImportError as e:
        print(f"\nMissing dependency: {e}\nRun: pip install logic2-automation pyserial\n")
        return 2
    except Exception as e:
        print(f"\nERROR: {e}\nCheck: both boards on USB, player DCC_OUT->decoder DCC_IN wired, "
              f"decoder ACK_OUT (PB3) -> Saleae D1, Logic 2 automation on port "
              f"{lib.AUTOMATION_PORT}, firmware running.\n")
        return 2
    path = lib.write_html([rep.as_dict()], lib.report_path("mobile_decoder_s9_2_3"),
                          title=f"NMRA {SPEC_DOC} Decoder Compliance Report")
    print(f"[report] HTML -> {path}")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
