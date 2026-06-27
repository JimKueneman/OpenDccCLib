"""
Hardware spec-coverage suite — proves the player meets SPEC.md P1-P5 ON THE WIRE.

SKIPPED unless WFPLAYER_PORT is set. Needs: the player flashed; the Saleae wired
DCC_OUT->D0 and TRIG->D1 with a common ground; Logic2 running with the Automation
API (port 10430); the repo venv (logic2-automation + pyserial).

Run from the player folder:
    WFPLAYER_PORT=/dev/cu.usbmodemXXXX \
      ../.venv/bin/python -m unittest discover -s test -p 'test_hw_spec.py' -v

Each test composes a stimulus, plays it looped, captures ch0 (and ch1 for P5),
and decodes with the shared command_station capture/decode pipeline
(compliance_lib). Timing is asserted post-L-calibration, so on-wire == requested.
"""
import os
import sys
import time
import tempfile
import statistics
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
_PLAYER = os.path.dirname(_HERE)                 # .../saleae_hil_waveform_player
_COMPL = os.path.dirname(_PLAYER)                # .../test/compliance
sys.path.insert(0, _PLAYER)                      # wfplayer
sys.path.insert(0, _COMPL)                       # compliance_lib

import wfplayer as wf

PORT = os.environ.get("WFPLAYER_PORT")
try:
    import compliance_lib as lib
    _HAVE = True
except Exception:
    _HAVE = False

DATA = [0x03, 0x3F, 0x40]                         # speed: addr 3, 128-step fwd 64
DXOR = wf.xor_checksum(DATA)


@unittest.skipUnless(PORT and _HAVE, "needs WFPLAYER_PORT + compliance_lib + Saleae")
class HwSpec(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p = wf.WaveformPlayer(PORT)

    @classmethod
    def tearDownClass(cls):
        cls.p.stop()
        cls.p.close()

    # ---- helpers ----
    def _play_capture(self, segments, secs=0.10):
        """Load (auto-calibrated) + loop + capture ch0; return decoded dict."""
        self.p.load(segments)
        self.p.play(0)
        time.sleep(0.05)
        try:
            with tempfile.TemporaryDirectory() as d:
                return lib.decode(lib.read_transitions(lib.capture_to_csv(d, capture_seconds=secs)))
        finally:
            self.p.stop()

    @staticmethod
    def _halves(res, bit):
        return [w for (a, b, bt) in res["bit_halves"] if bt == bit for w in (a, b)]

    @staticmethod
    def _byte_lists(res):
        return [d for _, d in res["packets"]]

    # ============================ P1 — timing ============================
    def test_P1_one_bit_edges_land_on_wire(self):
        for half in (55, 61):                     # the '1' spec edges
            res = self._play_capture(wf.compose([DATA], lead_idle=2, trail_idle=1, one_half_us=half))
            self.assertAlmostEqual(statistics.median(self._halves(res, "1")), half, delta=1.0,
                                   msg="'1' half requested %d" % half)

    def test_P1_zero_bit_decoder_min(self):
        res = self._play_capture(wf.compose([DATA], lead_idle=2, trail_idle=1, zero_half_us=90))
        self.assertAlmostEqual(statistics.median(self._halves(res, "0")), 90, delta=1.0)

    def test_P1_per_pulse_asymmetry(self):
        segs = wf.compose([DATA], lead_idle=2, trail_idle=1)
        wf.asym_bit(segs, 5, 58, 64)              # one bit's 2nd half -> 64; everything else 58
        res = self._play_capture(segs)
        ones = self._halves(res, "1")
        self.assertAlmostEqual(statistics.median(ones), 58, delta=1.0)   # rest untouched
        self.assertAlmostEqual(max(ones), 64, delta=1.5)                 # only the one pulse moved

    # ============================ P2 — preamble ============================
    def test_P2_preamble_count(self):
        for pre in (16, 20):                      # ops vs service preamble
            res = self._play_capture(wf.compose([DATA], lead_idle=2, trail_idle=1, preamble=pre))
            runs = [run for run, d in res["packets"] if d == DATA + [DXOR]]
            self.assertTrue(any(abs(r - pre) <= 1 for r in runs),
                            "preamble %d not seen on wire; runs=%s" % (pre, runs))

    # ====================== P3 — payloads / framing / error byte ======================
    def test_P3_multibyte_decodes_exactly(self):
        res = self._play_capture(wf.compose([DATA], lead_idle=2, trail_idle=1))
        self.assertIn(DATA + [DXOR], self._byte_lists(res))

    def test_P3_error_byte_is_independently_settable(self):
        # emit a packet with a DELIBERATELY WRONG error byte; the player must put it on the
        # wire verbatim (the host owns the error byte -- correct XOR *and* wrong, per P3).
        buf = wf.idle_packet_segs() * 2 \
            + wf.packet_segs(DATA + [0x00], append_xor=False) \
            + wf.idle_packet_segs()
        res = self._play_capture(buf)
        self.assertIn(DATA + [0x00], self._byte_lists(res))               # wrong byte on wire
        self.assertNotEqual(0x00, DXOR)                                   # it really is wrong

    def test_P3_long_packet_over_six_bytes(self):
        long_data = [0x03, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66]            # 7 bytes > 6-byte baseline
        res = self._play_capture(wf.compose([long_data], lead_idle=2, trail_idle=1))
        self.assertIn(long_data + [wf.xor_checksum(long_data)], self._byte_lists(res))

    # ====================== P4 — gap / repeat / continuous ======================
    def test_P4_finite_repeat_completes(self):
        self.p.load(wf.compose([DATA], lead_idle=1, trail_idle=1))
        self.assertIn("PLAYING", self.p.play(2))                          # finite: 2 reps
        deadline = time.time() + 2.0
        while time.time() < deadline and "PLAYING" in self.p._cmd("STATE?"):
            time.sleep(0.05)
        self.assertIn("IDLE", self.p._cmd("STATE?"))                      # it stopped on its own

    def test_P4_continuous_advances_then_stops(self):
        self.p.load([(1, 58), (0, 58)] * 8)
        self.p.play(0)
        time.sleep(0.1); s1 = self.p._cmd("STATE?")
        time.sleep(0.1); s2 = self.p._cmd("STATE?")
        self.assertIn("PLAYING", s1)
        self.assertIn("PLAYING", s2)
        self.assertNotEqual(s1, s2)                                       # rep advanced
        self.assertIn("STOPPED", self.p.stop())
        self.assertIn("IDLE", self.p._cmd("STATE?"))

    def test_P4_inter_packet_gap(self):
        # gap_idle inserts whole idle packets between data packets -> more idle than data.
        res = self._play_capture(wf.compose([DATA, DATA], lead_idle=2, gap_idle=2, trail_idle=2))
        bl = self._byte_lists(res)
        idles = sum(1 for d in bl if d == [0xFF, 0x00, 0xFF])
        datas = sum(1 for d in bl if d == DATA + [DXOR])
        self.assertGreater(idles, datas)                                 # spacing present

    # ============================ P5 — trigger ============================
    def test_P5_trigger_marks_chosen_packet(self):
        # TRIG at the segment where the data packet starts; capture D0 (DCC) + D1 (TRIG).
        lead = 2
        trig_at = len(wf.idle_packet_segs()) * lead                      # data packet start segment
        self.p.load(wf.compose([DATA], lead_idle=lead, trail_idle=1))
        self.p.trig(trig_at)
        self.p.play(0)
        time.sleep(0.05)
        try:
            with tempfile.TemporaryDirectory() as d:
                paths = lib.capture_to_csv_multi([lib.DIGITAL_CHANNEL, lib.TRIGGER_CHANNEL],
                                                 d, capture_seconds=0.10)
                dcc = lib.decode(lib.read_transitions(paths[lib.DIGITAL_CHANNEL]))
                trig_rows = lib.read_transitions(paths[lib.TRIGGER_CHANNEL])
        finally:
            self.p.stop()
        self.assertIn(DATA + [DXOR], self._byte_lists(dcc))              # DCC still valid
        rises = sum(1 for _, lvl in trig_rows if lvl == 1)
        self.assertGreater(rises, 0, "no TRIG pulse seen on D1")         # trigger fired


if __name__ == "__main__":
    unittest.main()
