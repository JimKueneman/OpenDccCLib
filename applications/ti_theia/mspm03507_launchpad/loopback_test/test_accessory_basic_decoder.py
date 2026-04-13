"""DCC loopback integration tests — Basic Accessory Decoder.

Tests for basic accessory (turnout) decoder commands, address boundaries,
and cross-type filtering.

Usage:
    pytest test_accessory_basic_decoder.py -m accessory_basic -v
"""

import pytest
from dcc_test_fixture import parse_recv


# =============================================================================
# Accessory Commands — Basic
# =============================================================================

@pytest.mark.accessory_basic
class TestAccessoryBasic:
    """Basic accessory (turnout) commands."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACC")

    def test_acc_on(self, dcc):
        resp = dcc.send_command("ACC 1 0 ON")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACC"
        assert p["board"] == "1"
        assert p["pair"] == "0"
        assert p["activate"] == "ON"

    def test_acc_off(self, dcc):
        resp = dcc.send_command("ACC 1 0 OFF")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["activate"] == "OFF"

    def test_acc_high_address(self, dcc):
        resp = dcc.send_command("ACC 511 3 ON")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["board"] == "511"
        assert p["pair"] == "3"


# =============================================================================
# Accessory Address Boundaries
# =============================================================================

@pytest.mark.accessory_basic
class TestAccessoryBasicAddressBoundaries:
    """Basic accessory address boundary values.

    Board addresses: 0-511 (9-bit)
    Output pairs: 0-3
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACC")

    def test_acc_board_0_pair_0(self, dcc):
        """Minimum board address and pair."""
        resp = dcc.send_command("ACC 0 0 ON")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACC"
        assert p["board"] == "0"
        assert p["pair"] == "0"

    def test_acc_board_511_pair_3(self, dcc):
        """Maximum board address and pair."""
        resp = dcc.send_command("ACC 511 3 ON")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["board"] == "511"
        assert p["pair"] == "3"

    def test_acc_board_256_pair_2(self, dcc):
        """Mid-range board address to verify 9-bit encoding."""
        resp = dcc.send_command("ACC 256 2 ON")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["board"] == "256"
        assert p["pair"] == "2"


# =============================================================================
# Accessory Filtering (Negative Tests)
# =============================================================================

@pytest.mark.accessory_basic
class TestAccessoryBasicFiltering:
    """Verify basic accessory decoder ignores non-accessory commands."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACC")

    def test_multifunction_command_ignored(self, dcc):
        """A multifunction speed command should not be received by ACC decoder."""
        dcc.clear_decoder()
        dcc.send_command("SPEED 3 64 FWD")
        assert dcc.wait_no_recv(timeout=0.5), \
            "ACC decoder received multifunction SPEED command"

    def test_extended_accessory_command_ignored(self, dcc):
        """An extended accessory command should not be received by basic ACC decoder."""
        dcc.clear_decoder()
        dcc.send_command("ACCE 1 0")
        assert dcc.wait_no_recv(timeout=0.5), \
            "ACC decoder received extended accessory ACCE command"

    def test_wrong_accessory_address_filtered(self, dcc):
        """ACC command for a different board should not be received.

        Decoder is set to board 0; send to board 100.
        """
        dcc.clear_decoder()
        dcc.send_command("ACC 100 0 ON")
        assert dcc.wait_no_recv(timeout=0.5), \
            "ACC decoder received command for wrong board address"


# =============================================================================
# Accessory CV Ops-Mode — Byte Write (S-9.2.1)
# =============================================================================

@pytest.mark.accessory_basic
@pytest.mark.cv
class TestAccessoryBasicCvWrite:
    """Basic accessory ops-mode CV byte write per S-9.2.1.

    Packet format: {10AAAAAA} {1AAACDDD} {1110CCVV} {VVVVVVVV} {DDDDDDDD} {EEEEEEEE}
    CC=11 selects byte write.
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACC")

    def test_acc_cv_write_byte(self, dcc):
        """Basic byte write to a typical CV."""
        resp = dcc.send_command("ACC CV WRITE 1 29 6")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0, "No RECV from decoder"
        p = parse_recv(recv[0])
        assert p["type"] == "ACC_CV_WRITE"
        assert p["board"] == "1"
        assert p["cv"] == "29"
        assert p["value"] == "6"

    def test_acc_cv_write_cv1(self, dcc):
        """Write CV1 (primary address)."""
        resp = dcc.send_command("ACC CV WRITE 1 1 42")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACC_CV_WRITE"
        assert p["cv"] == "1"
        assert p["value"] == "42"

    def test_acc_cv_write_cv513(self, dcc):
        """Write CV513 (accessory address LSB) — accessory-specific CV."""
        resp = dcc.send_command("ACC CV WRITE 1 513 10")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["cv"] == "513"
        assert p["value"] == "10"

    def test_acc_cv_write_cv521(self, dcc):
        """Write CV521 (accessory address MSB) — accessory-specific CV."""
        resp = dcc.send_command("ACC CV WRITE 1 521 3")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["cv"] == "521"
        assert p["value"] == "3"

    def test_acc_cv_write_high_board(self, dcc):
        """Write to maximum board address (511, 9-bit)."""
        resp = dcc.send_command("ACC CV WRITE 511 29 6")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["board"] == "511"
        assert p["cv"] == "29"


# =============================================================================
# Accessory CV Ops-Mode — Byte Verify (S-9.2.1)
# =============================================================================

@pytest.mark.accessory_basic
@pytest.mark.cv
class TestAccessoryBasicCvVerify:
    """Basic accessory ops-mode CV byte verify per S-9.2.1.

    CC=01 selects byte verify.
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACC")

    def test_acc_cv_verify_byte(self, dcc):
        """Basic byte verify."""
        resp = dcc.send_command("ACC CV VERIFY 1 29 6")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACC_CV_VERIFY"
        assert p["board"] == "1"
        assert p["cv"] == "29"
        assert p["value"] == "6"

    def test_acc_cv_verify_cv1(self, dcc):
        """Verify CV1."""
        resp = dcc.send_command("ACC CV VERIFY 1 1 42")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACC_CV_VERIFY"
        assert p["cv"] == "1"
        assert p["value"] == "42"


# =============================================================================
# Accessory CV Ops-Mode — Bit Write (S-9.2.1)
# =============================================================================

@pytest.mark.accessory_basic
@pytest.mark.cv
class TestAccessoryBasicCvBit:
    """Basic accessory ops-mode CV bit write per S-9.2.1.

    CC=10 selects bit manipulation. Data byte encodes bit position and value.
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACC")

    def test_acc_cv_bit_write_set(self, dcc):
        """Set bit 3 of CV29."""
        resp = dcc.send_command("ACC CV BIT 1 29 3 1")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACC_CV_BIT"
        assert p["board"] == "1"
        assert p["cv"] == "29"
        assert p["bit"] == "3"
        assert p["value"] == "1"

    def test_acc_cv_bit_write_clear(self, dcc):
        """Clear bit 0 of CV29."""
        resp = dcc.send_command("ACC CV BIT 1 29 0 0")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACC_CV_BIT"
        assert p["cv"] == "29"
        assert p["bit"] == "0"
        assert p["value"] == "0"

    def test_acc_cv_bit_boundary_bit7(self, dcc):
        """Maximum bit position (7)."""
        resp = dcc.send_command("ACC CV BIT 1 29 7 1")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["bit"] == "7"
        assert p["value"] == "1"


# =============================================================================
# Accessory CV Ops-Mode — High CV Numbers (S-9.2.1)
# =============================================================================

@pytest.mark.accessory_basic
@pytest.mark.cv
class TestAccessoryBasicCvHighCv:
    """Basic accessory CV ops-mode with high CV numbers (1-1024)."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(0, "ACC")

    def test_acc_cv_write_cv256(self, dcc):
        """CV 256 — beyond single-byte addressing."""
        resp = dcc.send_command("ACC CV WRITE 1 256 42")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ACC_CV_WRITE"
        assert p["cv"] == "256"
        assert p["value"] == "42"

    def test_acc_cv_write_cv1024(self, dcc):
        """CV 1024 — maximum CV number per S-9.2.1."""
        resp = dcc.send_command("ACC CV WRITE 1 1024 42")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["cv"] == "1024"
        assert p["value"] == "42"


# =============================================================================
# Accessory CV Ops-Mode — Filtering (Negative Tests)
# =============================================================================

@pytest.mark.accessory_basic
@pytest.mark.cv
class TestAccessoryBasicCvFiltering:
    """Verify ACC CV ops-mode commands are filtered by address and type."""

    def test_acc_cv_wrong_board_filtered(self, dcc):
        """ACC CV write for board 100 should not be received by decoder at board 0."""
        dcc.set_decoder_address(0, "ACC")
        dcc.clear_decoder()
        dcc.send_command("ACC CV WRITE 100 29 6")
        assert dcc.wait_no_recv(timeout=0.5), \
            "ACC decoder received CV write for wrong board address"

    def test_acc_cv_multifunction_decoder_ignores(self, dcc):
        """ACC CV write should not be received by multifunction decoder."""
        dcc.set_decoder_address(3, "SHORT")
        dcc.clear_decoder()
        dcc.send_command("ACC CV WRITE 1 29 6")
        assert dcc.wait_no_recv(timeout=0.5), \
            "Multifunction decoder received ACC CV command"
