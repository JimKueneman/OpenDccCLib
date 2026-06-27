"""
Pure (no-hardware) tests for the Waveform Player host library and protocol.

Run:  python3 -m unittest discover -s test       (from the project dir; stdlib only)
  or:  ../.venv/bin/python -m unittest discover -s test -v   (repo venv)

The strongest test here is test_roundtrip_*: it decodes the composed segment
list back to bytes with an INDEPENDENT decoder (edge widths -> bits -> bytes),
proving the DCC composition is spec-correct, not merely self-consistent with the
encoder.
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import wfplayer as wf


# --- independent reference decoder: segments -> (preamble, [bytes]) -----------
def decode_segments(segments, threshold_us=80):
    """Mirror of the bench decoder: each segment is one half-bit; a half shorter
    than threshold is a '1', else '0'. Then frame: <ones>=preamble, 0, byte, 0,
    ..., 1. Half-bits come in pairs (high/low) per bit, so collapse pairs."""
    halves = [u for (_l, u) in segments]
    # pair up halves into bits (high half + low half = one bit)
    bits = ""
    for i in range(0, len(halves) - 1, 2):
        a, b = halves[i], halves[i + 1]
        # both halves of a bit are equal width; classify on the first
        bits += "1" if a < threshold_us else "0"
    # framing
    i = 0
    n = len(bits)
    # preamble = leading run of ones (>= decoder min)
    pre = 0
    while i < n and bits[i] == "1":
        pre += 1
        i += 1
    assert i < n and bits[i] == "0", "no packet-start bit after preamble"
    i += 1  # consume start bit
    out = []
    while True:
        if i + 8 > n:
            break
        byte = int(bits[i:i + 8], 2)
        i += 8
        out.append(byte)
        if i < n and bits[i] == "1":
            break        # end bit
        i += 1           # separator 0
    return pre, out


class SegmentCodec(unittest.TestCase):
    def test_roundtrip_word(self):
        for level in (0, 1):
            for us in (1, 58, 100, 12000, wf.MAX_SEG_US):
                self.assertEqual(wf.unseg(wf.seg(level, us)), (level, us))

    def test_hex_roundtrip(self):
        segs = [(1, 58), (0, 58), (1, 100), (0, 12000)]
        self.assertEqual(wf.hex_to_segs(wf.segs_to_hex(segs)), segs)

    def test_seg_range_guard(self):
        with self.assertRaises(ValueError):
            wf.seg(1, 0)
        with self.assertRaises(ValueError):
            wf.seg(1, wf.DUR_MASK + 1)
        with self.assertRaises(ValueError):
            wf.seg(2, 58)

    def test_split_long(self):
        parts = wf.split_long(0, 200000)
        self.assertTrue(all(u <= wf.MAX_SEG_US for (_l, u) in parts))
        self.assertEqual(sum(u for (_l, u) in parts), 200000)
        self.assertTrue(all(l == 0 for (l, _u) in parts))


class Crc(unittest.TestCase):
    def test_ccitt_false_check_vector(self):
        # CRC-16/CCITT-FALSE("123456789") == 0x29B1
        self.assertEqual(wf.crc16(b"123456789"), 0x29B1)

    def test_buffer_crc_changes_with_data(self):
        a = wf.buffer_crc([(1, 58), (0, 58)])
        b = wf.buffer_crc([(1, 58), (0, 59)])
        self.assertNotEqual(a, b)


class DccTiming(unittest.TestCase):
    def test_one_zero_nominal(self):
        self.assertEqual(wf.one_bit(), [(1, 58), (0, 58)])
        self.assertEqual(wf.zero_bit(), [(1, 100), (0, 100)])

    def test_marginal_timing_propagates(self):
        segs = wf.packet_segs([0x03, 0x40], one_half_us=55, zero_half_us=90)
        widths = {u for (_l, u) in segs}
        self.assertTrue(widths <= {55, 90})


class DccComposition(unittest.TestCase):
    def test_xor_checksum(self):
        self.assertEqual(wf.xor_checksum([0x03, 0x3F, 0x40]), 0x03 ^ 0x3F ^ 0x40)

    def test_packet_bits_framing(self):
        bits = wf.packet_bits([0x03, 0x40], preamble=16)
        self.assertTrue(bits.startswith("1" * 16 + "0"))   # preamble + start
        self.assertTrue(bits.endswith("1"))                # end bit
        # preamble(16) + start(1) + 3 bytes*(8+sep) ... end already counted
        # 3 payload bytes (addr,data,xor): each 1 sep + 8 bits, last sep replaced by end
        self.assertEqual(len(bits), 16 + 1 + 8 + 1 + 8 + 1 + 8 + 1)

    def test_roundtrip_speed_packet(self):
        # address 3, 128-step fwd 64 -> [0x03, 0x3F, 0x40]; xor appended by encoder
        data = [0x03, 0x3F, 0x40]
        pre, got = decode_segments(wf.packet_segs(data, preamble=16))
        self.assertGreaterEqual(pre, wf.PREAMBLE_DEC_MIN)
        self.assertEqual(got, data + [wf.xor_checksum(data)])

    def test_roundtrip_idle_packet(self):
        pre, got = decode_segments(wf.idle_packet_segs(preamble=14))
        self.assertEqual(got, [0xFF, 0x00, 0xFF])

    def test_roundtrip_marginal(self):
        data = [0x03, 0x40]
        segs = wf.packet_segs(data, preamble=12, one_half_us=61, zero_half_us=90)
        pre, got = decode_segments(segs)
        self.assertEqual(pre, 12)
        self.assertEqual(got, data + [wf.xor_checksum(data)])


class Framing(unittest.TestCase):
    def test_seg_lines_chunk_and_reassemble(self):
        segs = wf.compose([[0x03, 0x3F, 0x40]], lead_idle=3, trail_idle=1)
        lines = wf.seg_lines(segs, per_line=32)
        self.assertTrue(all(ln.startswith("SEG ") for ln in lines))
        rejoined = "".join(ln[4:] for ln in lines)
        self.assertEqual(wf.hex_to_segs(rejoined), segs)

    def test_compose_is_gap_free_flat(self):
        segs = wf.compose([[0x03, 0x40], [0x05, 0x60]], lead_idle=2, gap_idle=1, trail_idle=1)
        # flat list of (level,us) tuples, alternating high/low halves
        self.assertTrue(all(isinstance(s, tuple) and len(s) == 2 for s in segs))
        self.assertTrue(len(segs) > 0)


class PerPulseOverrides(unittest.TestCase):
    def test_set_half_overrides_one_pulse_only(self):
        segs = wf.packet_segs([0x03, 0x40], preamble=14)
        before = list(segs)
        wf.set_half(segs, 0, "first", 54)
        self.assertEqual(segs[0], (1, 54))      # level preserved (HIGH), duration changed
        self.assertEqual(segs[1], before[1])    # the bit's other half untouched
        self.assertEqual(segs[2:], before[2:])  # every other pulse untouched

    def test_set_half_second_is_low_half(self):
        segs = wf.packet_segs([0x03, 0x40])
        wf.set_half(segs, 0, "second", 61)
        self.assertEqual(segs[1], (0, 61))      # 'second' = the LOW half

    def test_asym_bit_tr1d(self):
        segs = wf.packet_segs([0x03, 0x40])      # default preamble -> bit 5 is a '1'
        wf.asym_bit(segs, 5, 58, 64)             # 6 us asymmetry
        first, second = segs[10], segs[11]
        self.assertEqual(first, (1, 58))
        self.assertEqual(second, (0, 64))
        self.assertEqual(abs(first[1] - second[1]), 6)

    def test_guards(self):
        segs = wf.packet_segs([0x03, 0x40])
        with self.assertRaises(IndexError):
            wf.set_half(segs, len(segs), "first", 58)
        with self.assertRaises(ValueError):
            wf.set_half(segs, 0, "middle", 58)

    def test_within_window_override_still_decodes(self):
        data = [0x03, 0x3F, 0x40]
        segs = wf.packet_segs(data, preamble=16)
        wf.asym_bit(segs, 3, 55, 61)             # preamble '1' bit, both halves in 55-61
        pre, got = decode_segments(segs)
        self.assertGreaterEqual(pre, wf.PREAMBLE_DEC_MIN)
        self.assertEqual(got, data + [wf.xor_checksum(data)])


class Calibration(unittest.TestCase):
    def test_subtracts_and_clamps(self):
        segs = [(1, 58), (0, 100), (1, 2)]
        self.assertEqual(wf.apply_calibration(segs, us=2), [(1, 56), (0, 98), (1, 1)])

    def test_zero_is_noop(self):
        segs = [(1, 58), (0, 100)]
        self.assertEqual(wf.apply_calibration(segs, us=0), segs)


class SpecComposition(unittest.TestCase):
    """Composition-side coverage of SPEC.md P1-P5 (on-wire coverage is in test_hw_spec.py)."""

    def test_P2_exact_preamble_count(self):
        for pre in (9, 16, 20):                       # below-min / ops / service
            bits = wf.packet_bits([0x03, 0x40], preamble=pre)
            self.assertTrue(bits.startswith("1" * pre + "0"))   # exactly `pre` ones, then start bit
            self.assertEqual(bits[pre - 1:pre + 1], "10")       # not pre+1 ones

    def test_P3_error_byte_independently_settable(self):
        data = [0x03, 0x3F, 0x40]
        segs = wf.packet_segs(data + [0x00], append_xor=False)  # force a WRONG error byte
        _pre, got = decode_segments(segs)
        self.assertEqual(got, data + [0x00])                    # emitted verbatim, not the XOR
        self.assertNotEqual(0x00, wf.xor_checksum(data))

    def test_P3_long_packet_round_trips(self):
        long_data = [0x03, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66]  # 7 bytes > 6-byte baseline
        _pre, got = decode_segments(wf.packet_segs(long_data))
        self.assertEqual(got, long_data + [wf.xor_checksum(long_data)])

    def test_P4_gap_inserts_idle_packets(self):
        none_ = wf.compose([[0x03, 0x40], [0x05, 0x60]], lead_idle=0, gap_idle=0, trail_idle=0)
        gapped = wf.compose([[0x03, 0x40], [0x05, 0x60]], lead_idle=0, gap_idle=2, trail_idle=0)
        self.assertGreater(len(gapped), len(none_))             # gap added idle packets


if __name__ == "__main__":
    unittest.main()
