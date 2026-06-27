"""
Hardware smoke test for the Waveform Player board. SKIPPED unless WFPLAYER_PORT
is set to the board's serial device, e.g.:

    WFPLAYER_PORT=/dev/cu.usbmodemXXXX ../.venv/bin/python -m unittest discover -s test -p 'test_hw_smoke.py' -v

Needs pyserial + a flashed board. Does not need the Saleae (that's the separate
fidelity-certification step using the s9_1/s9_2 suites on DCC_OUT).
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import wfplayer as wf

PORT = os.environ.get("WFPLAYER_PORT")


@unittest.skipUnless(PORT, "set WFPLAYER_PORT to run hardware smoke test")
class HwSmoke(unittest.TestCase):
    def setUp(self):
        self.p = wf.WaveformPlayer(PORT)

    def tearDown(self):
        self.p.close()

    def test_ping(self):
        self.assertIn("PONG", self.p.ping())

    def test_id_reports_caps(self):
        self.assertIn("wfplayer", self.p.id())

    def test_load_crc_and_play(self):
        segs = wf.compose([[0x03, 0x3F, 0x40]], lead_idle=10, trail_idle=2)
        n = self.p.load(segs, verify_crc=True)   # raises on CRC mismatch
        self.assertEqual(n, len(segs))
        self.assertIn("PLAYING", self.p.play(count=1))


if __name__ == "__main__":
    unittest.main()
