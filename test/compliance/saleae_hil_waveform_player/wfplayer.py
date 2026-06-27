"""
wfplayer.py -- Host library for the generic Waveform Player board.

Two layers:
  1. Protocol codec + DCC composition (PURE, no hardware) -- the frozen contract
     with the firmware. Unit-tested in test/test_wfplayer.py.
  2. WaveformPlayer serial client (needs pyserial + a board) -- thin wrapper that
     loads a buffer, verifies its CRC, and plays it.

The board knows NOTHING about DCC. Every DCC notion (preamble, framing, bit
timing, idle packet, marginal timings) is built HERE and handed over as raw
(level, duration_us) segments. See PROTOCOL.md.
"""

# =============================================================================
# DCC timing constants (mirror src/dcc/dcc_defines.h -- spec nominals)
# =============================================================================
ONE_HALF_US        = 58      # DCC_ONE_BIT_HALF_PERIOD_US  (range 55..61)
ZERO_HALF_US       = 100     # DCC_ZERO_BIT_HALF_PERIOD_US (decoder min 90)
PREAMBLE_OPS       = 16      # DCC_PREAMBLE_BITS_OPS
PREAMBLE_SERVICE   = 20      # DCC_PREAMBLE_BITS_SERVICE
PREAMBLE_DEC_MIN   = 10      # DCC_PREAMBLE_BITS_DECODER_MIN

MAX_SEG_US         = 0xFFFF  # firmware honors <= 65535 us per segment
LEVEL_BIT          = 1 << 31
DUR_MASK           = (1 << 31) - 1

HIGH, LOW = 1, 0

# =============================================================================
# Segment codec  (segment = (level, duration_us) <-> uint32 <-> 8 hex chars)
# =============================================================================

def seg(level, us):
    """Encode one (level, duration_us) segment to a uint32."""
    if level not in (0, 1):
        raise ValueError("level must be 0 or 1")
    if not (1 <= us <= DUR_MASK):
        raise ValueError("duration out of range: %r" % (us,))
    return (LEVEL_BIT if level else 0) | int(us)

def unseg(word):
    """Decode a uint32 back to (level, duration_us)."""
    return (1 if word & LEVEL_BIT else 0, word & DUR_MASK)

def split_long(level, us):
    """Split a run longer than MAX_SEG_US into multiple same-level segments."""
    out = []
    while us > MAX_SEG_US:
        out.append((level, MAX_SEG_US))
        us -= MAX_SEG_US
    out.append((level, us))
    return out

def segs_to_hex(segments):
    """List of (level, us) -> concatenated 8-hex-char-per-segment string."""
    return "".join("%08X" % seg(l, u) for (l, u) in segments)

def hex_to_segs(hexstr):
    """Inverse of segs_to_hex."""
    s = hexstr.strip()
    if len(s) % 8 != 0:
        raise ValueError("hex length not a multiple of 8")
    return [unseg(int(s[i:i + 8], 16)) for i in range(0, len(s), 8)]

def segs_to_bytes(segments):
    """Raw big-endian uint32 bytes of the buffer (what CRC is computed over)."""
    out = bytearray()
    for (l, u) in segments:
        w = seg(l, u)
        out += bytes(((w >> 24) & 0xFF, (w >> 16) & 0xFF, (w >> 8) & 0xFF, w & 0xFF))
    return bytes(out)

# =============================================================================
# CRC-16/CCITT-FALSE  (poly 0x1021, init 0xFFFF, no reflection)
# =============================================================================

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return crc & 0xFFFF

def buffer_crc(segments):
    return crc16(segs_to_bytes(segments))

# =============================================================================
# DCC composition  (bytes + timing -> segment list).  Independent of the library.
# =============================================================================

def one_bit(one_half_us=ONE_HALF_US):
    """A DCC '1' bit = high half then low half (polarity-agnostic to the decoder)."""
    return [(HIGH, one_half_us), (LOW, one_half_us)]

def zero_bit(zero_half_us=ZERO_HALF_US):
    """A DCC '0' bit = high half then low half, longer halves."""
    return [(HIGH, zero_half_us), (LOW, zero_half_us)]

def bits_to_segs(bitstr, one_half_us=ONE_HALF_US, zero_half_us=ZERO_HALF_US):
    out = []
    for c in bitstr:
        out += one_bit(one_half_us) if c == "1" else zero_bit(zero_half_us)
    return out

def xor_checksum(databytes):
    c = 0
    for b in databytes:
        c ^= b
    return c & 0xFF

def packet_bits(databytes, preamble=PREAMBLE_OPS, append_xor=True):
    """Build the DCC bit string for one packet:
        <preamble ones> 0 <byte> 0 <byte> ... 0 <byteN> 1
    The error-detection (XOR) byte is appended as the last data byte by default.
    """
    payload = list(databytes)
    if append_xor:
        payload = payload + [xor_checksum(payload)]
    bits = "1" * preamble
    for b in payload:
        bits += "0"                      # packet-start / byte-separator bit
        bits += format(b & 0xFF, "08b")  # 8 data bits, MSB first
    bits += "1"                          # packet end bit
    return bits

def packet_segs(databytes, preamble=PREAMBLE_OPS, append_xor=True,
                one_half_us=ONE_HALF_US, zero_half_us=ZERO_HALF_US):
    """One DCC packet as a segment list. Pass one_half_us / zero_half_us to push
    timings to the acceptance-window edges (e.g. 55 or 61, 90)."""
    return bits_to_segs(packet_bits(databytes, preamble, append_xor),
                        one_half_us, zero_half_us)

def idle_packet_segs(preamble=PREAMBLE_OPS, **kw):
    """The DCC idle packet: address 0xFF, data 0x00 (XOR appended -> 0xFF)."""
    return packet_segs([0xFF, 0x00], preamble=preamble, append_xor=True, **kw)

def compose(packets, lead_idle=10, gap_idle=0, trail_idle=2,
            preamble=PREAMBLE_OPS, **timing):
    """Compose a complete, gap-free stimulus buffer:
        lead_idle x idle, then each packet (separated by gap_idle idles),
        then trail_idle x idle.
    `packets` is a list of data-byte lists. Returns a flat segment list."""
    out = []
    for _ in range(lead_idle):
        out += idle_packet_segs(preamble=preamble, **timing)
    for i, pkt in enumerate(packets):
        if i and gap_idle:
            for _ in range(gap_idle):
                out += idle_packet_segs(preamble=preamble, **timing)
        out += packet_segs(pkt, preamble=preamble, **timing)
    for _ in range(trail_idle):
        out += idle_packet_segs(preamble=preamble, **timing)
    return out

# =============================================================================
# Per-pulse overrides  (target individual half-bits in a built segment list)
#
# The wire format is per-segment, so off-nominal timing is per-pulse, not a
# global mode. A bit at bitstream index `b` occupies two segments: its FIRST
# (high) half at index 2*b and its SECOND (low) half at 2*b+1 (see bits_to_segs).
# Bitstream index 0 is the first preamble bit; use packet_bits() to map logical
# positions to bit indices. These mutate the list in place and return it.
# =============================================================================

def _half_index(bit_index, which):
    if which not in ("first", "second"):
        raise ValueError("which must be 'first' or 'second', got %r" % (which,))
    return 2 * bit_index + (0 if which == "first" else 1)

def set_half(segments, bit_index, which, us):
    """Override one half-bit's duration, leaving its level and all other segments
    untouched. `which` = 'first' (high half) or 'second' (low half)."""
    i = _half_index(bit_index, which)
    if not (0 <= i < len(segments)):
        raise IndexError("half-bit %d/%s out of range (%d segments)"
                         % (bit_index, which, len(segments)))
    if not (1 <= us <= DUR_MASK):
        raise ValueError("duration out of range: %r" % (us,))
    level, _old = segments[i]
    segments[i] = (level, us)
    return segments

def asym_bit(segments, bit_index, first_us, second_us):
    """Make one bit asymmetric — set its first and last half to distinct
    durations (e.g. the tr1d test: a '1' bit whose halves differ by <= 6 us)."""
    set_half(segments, bit_index, "first", first_us)
    set_half(segments, bit_index, "second", second_us)
    return segments


# =============================================================================
# UART framing helpers
# =============================================================================

def seg_lines(segments, per_line=32):
    """Chunk a segment list into `SEG <hex...>` command lines (per_line segments
    each) so no single UART line gets unwieldy."""
    lines = []
    for i in range(0, len(segments), per_line):
        chunk = segments[i:i + per_line]
        lines.append("SEG " + segs_to_hex(chunk))
    return lines

# =============================================================================
# Serial client  (hardware path -- lazy-imports pyserial)
# =============================================================================

class WaveformPlayer:
    def __init__(self, port, baud=230400, timeout=1.0):
        import serial
        self.s = serial.Serial(port, baud, timeout=timeout)

    def _cmd(self, line):
        self.s.reset_input_buffer()
        self.s.write((line + "\n").encode())
        resp = self.s.readline().decode(errors="replace").strip()
        if not resp.startswith("OK"):
            raise RuntimeError("player error for %r -> %r" % (line, resp))
        return resp

    def ping(self):       return self._cmd("PING")
    def id(self):         return self._cmd("ID?")
    def clear(self):      return self._cmd("CLEAR")
    def stop(self):       return self._cmd("STOP")

    def trig(self, segment_index=0):
        """Arm the trigger to pulse when playback reaches this segment index
        (0 = play-start) — used to mark the packet-under-test."""
        return self._cmd("TRIG %d" % int(segment_index))

    def trig_off(self):
        return self._cmd("TRIG OFF")

    def load(self, segments, per_line=32, verify_crc=True):
        """Clear, stream the buffer, and (optionally) verify the board's CRC
        matches the host's before returning."""
        self.clear()
        for line in seg_lines(segments, per_line):
            self._cmd(line)
        if verify_crc:
            resp = self._cmd("CRC?")
            board = int(resp.split()[1], 16)
            host = buffer_crc(segments)
            if board != host:
                raise RuntimeError("CRC mismatch board=%04X host=%04X" % (board, host))
        return len(segments)

    def play(self, count=1):
        return self._cmd("PLAY %d" % count)

    def close(self):
        try:
            self.s.close()
        except Exception:
            pass
