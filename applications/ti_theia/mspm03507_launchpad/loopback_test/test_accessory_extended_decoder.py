"""DCC loopback integration tests — Extended Accessory (Signal) Decoder.

Tests for extended accessory decoder commands (signal aspects), address
boundaries, and cross-type filtering.

Usage:
    pytest test_accessory_extended_decoder.py -m accessory_extended -v
"""

import pytest
from dcc_test_fixture import parse_recv


# =============================================================================
# Accessory Commands — Extended (Signal Aspects)
# =============================================================================

@pytest.mark.accessory_extended
class TestAccessoryExtended:
    """Extended accessory (signal aspect) commands."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACCE")

    def test_acce_aspect_0(self, dcc):
        resp = dcc.send_command("ACCE 1 0")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACCE"
        assert p["addr"] == "1"
        assert p["aspect"] == "0"

    def test_acce_aspect_31(self, dcc):
        resp = dcc.send_command("ACCE 1 31")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["aspect"] == "31"


# =============================================================================
# Extended Accessory Address Boundaries
# =============================================================================

@pytest.mark.accessory_extended
class TestAccessoryExtendedAddressBoundaries:
    """Extended accessory address and aspect boundary values.

    Addresses: 0-2047 (11-bit)
    Aspects: 0-255 (8-bit, though typically 0-31 used)
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACCE")

    def test_acce_addr_0_aspect_0(self, dcc):
        """Minimum address and aspect."""
        resp = dcc.send_command("ACCE 0 0")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACCE"
        assert p["addr"] == "0"
        assert p["aspect"] == "0"

    def test_acce_addr_2047_aspect_255(self, dcc):
        """Maximum address and aspect."""
        resp = dcc.send_command("ACCE 2047 255")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["addr"] == "2047"
        assert p["aspect"] == "255"

    def test_acce_addr_1_aspect_31(self, dcc):
        """Typical signal aspect range upper bound."""
        resp = dcc.send_command("ACCE 1 31")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["addr"] == "1"
        assert p["aspect"] == "31"


# =============================================================================
# Extended Accessory Filtering (Negative Tests)
# =============================================================================

@pytest.mark.accessory_extended
class TestAccessoryExtendedFiltering:
    """Verify extended accessory decoder ignores non-matching commands."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACCE")

    def test_multifunction_command_ignored(self, dcc):
        """A multifunction speed command should not be received by ACCE decoder."""
        dcc.clear_decoder()
        dcc.send_command("SPEED 3 64 FWD")
        assert dcc.wait_no_recv(timeout=0.5), \
            "ACCE decoder received multifunction SPEED command"

    def test_basic_accessory_command_ignored(self, dcc):
        """A basic accessory command should not be received by ACCE decoder."""
        dcc.clear_decoder()
        dcc.send_command("ACC 1 0 ON")
        assert dcc.wait_no_recv(timeout=0.5), \
            "ACCE decoder received basic ACC command"

    def test_wrong_extended_address_filtered(self, dcc):
        """ACCE command for a different address should not be received.

        Decoder is set to address 0; send to address 100.
        """
        dcc.clear_decoder()
        dcc.send_command("ACCE 100 0")
        assert dcc.wait_no_recv(timeout=0.5), \
            "ACCE decoder received command for wrong address"


# =============================================================================
# Extended Accessory CV Ops-Mode — Byte Write (S-9.2.2)
# =============================================================================

@pytest.mark.accessory_extended
@pytest.mark.cv
class TestAccessoryExtendedCvWrite:
    """Extended accessory ops-mode CV byte write per S-9.2.2.

    Packet format: {10AAAAAA} {0AAA0AA1} {1110CCVV} {VVVVVVVV} {DDDDDDDD} {EEEEEEEE}
    CC=11 selects byte write.
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACCE")

    def test_acce_cv_write_byte(self, dcc):
        """Basic byte write to a typical CV."""
        resp = dcc.send_command("ACCE CV WRITE 1 29 6")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0, "No RECV from decoder"
        p = parse_recv(recv[0])
        assert p["type"] == "ACCE_CV_WRITE"
        assert p["addr"] == "1"
        assert p["cv"] == "29"
        assert p["value"] == "6"

    def test_acce_cv_write_cv1(self, dcc):
        """Write CV1."""
        resp = dcc.send_command("ACCE CV WRITE 1 1 42")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACCE_CV_WRITE"
        assert p["cv"] == "1"
        assert p["value"] == "42"

    def test_acce_cv_write_cv541(self, dcc):
        """Write CV541 (accessory decoder config) — signal-specific CV."""
        resp = dcc.send_command("ACCE CV WRITE 1 541 32")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["cv"] == "541"
        assert p["value"] == "32"

    def test_acce_cv_write_high_addr(self, dcc):
        """Write to maximum extended address (2047, 11-bit)."""
        resp = dcc.send_command("ACCE CV WRITE 2047 29 6")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["addr"] == "2047"
        assert p["cv"] == "29"


# =============================================================================
# Extended Accessory CV Ops-Mode — Byte Verify (S-9.2.2)
# =============================================================================

@pytest.mark.accessory_extended
@pytest.mark.cv
class TestAccessoryExtendedCvVerify:
    """Extended accessory ops-mode CV byte verify per S-9.2.2.

    CC=01 selects byte verify.
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACCE")

    def test_acce_cv_verify_byte(self, dcc):
        """Basic byte verify."""
        resp = dcc.send_command("ACCE CV VERIFY 1 29 6")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACCE_CV_VERIFY"
        assert p["addr"] == "1"
        assert p["cv"] == "29"
        assert p["value"] == "6"

    def test_acce_cv_verify_cv1(self, dcc):
        """Verify CV1."""
        resp = dcc.send_command("ACCE CV VERIFY 1 1 42")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACCE_CV_VERIFY"
        assert p["cv"] == "1"
        assert p["value"] == "42"


# =============================================================================
# Extended Accessory CV Ops-Mode — Bit Write (S-9.2.2)
# =============================================================================

@pytest.mark.accessory_extended
@pytest.mark.cv
class TestAccessoryExtendedCvBit:
    """Extended accessory ops-mode CV bit write per S-9.2.2.

    CC=10 selects bit manipulation.
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACCE")

    def test_acce_cv_bit_write_set(self, dcc):
        """Set bit 3 of CV29."""
        resp = dcc.send_command("ACCE CV BIT 1 29 3 1")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACCE_CV_BIT"
        assert p["addr"] == "1"
        assert p["cv"] == "29"
        assert p["bit"] == "3"
        assert p["value"] == "1"

    def test_acce_cv_bit_write_clear(self, dcc):
        """Clear bit 0 of CV29."""
        resp = dcc.send_command("ACCE CV BIT 1 29 0 0")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACCE_CV_BIT"
        assert p["cv"] == "29"
        assert p["bit"] == "0"
        assert p["value"] == "0"


# =============================================================================
# Extended Accessory CV Ops-Mode — High CV Numbers (S-9.2.2)
# =============================================================================

@pytest.mark.accessory_extended
@pytest.mark.cv
class TestAccessoryExtendedCvHighCv:
    """Extended accessory CV ops-mode with high CV numbers (1-1024)."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACCE")

    def test_acce_cv_write_cv256(self, dcc):
        """CV 256 — beyond single-byte addressing."""
        resp = dcc.send_command("ACCE CV WRITE 1 256 42")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACCE_CV_WRITE"
        assert p["cv"] == "256"
        assert p["value"] == "42"

    def test_acce_cv_write_cv1024(self, dcc):
        """CV 1024 — maximum CV number per S-9.2.2."""
        resp = dcc.send_command("ACCE CV WRITE 1 1024 42")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["cv"] == "1024"
        assert p["value"] == "42"


# =============================================================================
# Extended Accessory CV Ops-Mode — Filtering (Negative Tests)
# =============================================================================

@pytest.mark.accessory_extended
@pytest.mark.cv
class TestAccessoryExtendedCvFiltering:
    """Verify ACCE CV ops-mode commands are filtered by address and type."""

    def test_acce_cv_wrong_addr_filtered(self, dcc):
        """ACCE CV write for addr 100 should not be received by decoder at addr 0."""
        dcc.set_decoder_address(0, "ACCE")
        dcc.clear_decoder()
        dcc.send_command("ACCE CV WRITE 100 29 6")
        assert dcc.wait_no_recv(timeout=0.5), \
            "ACCE decoder received CV write for wrong address"

    def test_acce_cv_multifunction_decoder_ignores(self, dcc):
        """ACCE CV write should not be received by multifunction decoder."""
        dcc.set_decoder_address(3, "SHORT")
        dcc.clear_decoder()
        dcc.send_command("ACCE CV WRITE 1 29 6")
        assert dcc.wait_no_recv(timeout=0.5), \
            "Multifunction decoder received ACCE CV command"

    def test_acce_cv_basic_acc_decoder_ignores(self, dcc):
        """ACCE CV write should not be received by basic ACC decoder."""
        dcc.set_decoder_address(0, "ACC")
        dcc.clear_decoder()
        dcc.send_command("ACCE CV WRITE 1 29 6")
        assert dcc.wait_no_recv(timeout=0.5), \
            "Basic ACC decoder received ACCE CV command"
