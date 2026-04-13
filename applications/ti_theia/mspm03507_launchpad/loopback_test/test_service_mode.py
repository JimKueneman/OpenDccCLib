"""DCC loopback integration tests — Service Mode (Programming Track).

Tests for service track DCC output: direct mode, paged mode,
register mode, address-only mode, ACK compliance, and sequential operations.

Usage:
    pytest test_service_mode.py -m service_mode -v
    pytest test_service_mode.py -m cv -v
    pytest test_service_mode.py -m compliance -v
"""

import pytest
from dcc_test_fixture import parse_recv


# =============================================================================
# Service Mode — Direct (S-9.2.3)
# =============================================================================

@pytest.mark.service_mode
@pytest.mark.cv
class TestServiceModeDirect:
    """Direct mode (S-9.2.3): byte write, byte verify, bit write, bit verify."""

    @pytest.fixture(autouse=True)
    def setup(self, dcc, svc_mode):
        dcc.set_ack_enabled(True)
        dcc.set_ack_width(6000)

    def test_direct_write_byte(self, dcc):
        resp = dcc.send_command("SVC DIRECT WRITE 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_direct_verify_byte_match(self, dcc):
        """Write CV5=42, then verify with 42 — should ACK."""
        dcc.send_command("SVC DIRECT WRITE 5 42")
        dcc.wait_svc_result()
        resp = dcc.send_command("SVC DIRECT VERIFY 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_direct_verify_byte_mismatch(self, dcc):
        """Write CV5=42, then verify with 99 — decoder should NOT ACK."""
        dcc.send_command("SVC DIRECT WRITE 5 42")
        dcc.wait_svc_result()
        resp = dcc.send_command("SVC DIRECT VERIFY 5 99")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "NO ACK", f"Expected NO ACK, got '{result}'"

    def test_direct_write_bit(self, dcc):
        resp = dcc.send_command("SVC DIRECT BITW 5 3 1")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_direct_verify_bit_match(self, dcc):
        """Write CV5=8 (bit 3=1), then bit-verify bit 3=1 — should ACK."""
        dcc.send_command("SVC DIRECT WRITE 5 8")
        dcc.wait_svc_result()
        resp = dcc.send_command("SVC DIRECT BITV 5 3 1")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_direct_verify_bit_mismatch(self, dcc):
        """Write CV5=0 (all bits 0), then bit-verify bit 3=1 — no ACK."""
        dcc.send_command("SVC DIRECT WRITE 5 0")
        dcc.wait_svc_result()
        resp = dcc.send_command("SVC DIRECT BITV 5 3 1")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "NO ACK", f"Expected NO ACK, got '{result}'"


# =============================================================================
# Service Mode — Paged (S-9.2.3)
# =============================================================================

@pytest.mark.service_mode
@pytest.mark.cv
class TestServiceModePaged:
    """Paged mode (S-9.2.3): write and verify via page register."""

    @pytest.fixture(autouse=True)
    def setup(self, dcc, svc_mode):
        dcc.set_ack_enabled(True)
        dcc.set_ack_width(6000)

    def test_paged_write(self, dcc):
        resp = dcc.send_command("SVC PAGED WRITE 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_paged_verify_match(self, dcc):
        dcc.send_command("SVC PAGED WRITE 5 42")
        dcc.wait_svc_result()
        resp = dcc.send_command("SVC PAGED VERIFY 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_paged_verify_mismatch(self, dcc):
        dcc.send_command("SVC PAGED WRITE 5 42")
        dcc.wait_svc_result()
        resp = dcc.send_command("SVC PAGED VERIFY 5 99")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "NO ACK", f"Expected NO ACK, got '{result}'"


# =============================================================================
# Service Mode — Register (S-9.2.3)
# =============================================================================

@pytest.mark.service_mode
@pytest.mark.cv
class TestServiceModeRegister:
    """Register mode (S-9.2.3): write and verify via physical registers."""

    @pytest.fixture(autouse=True)
    def setup(self, dcc, svc_mode):
        dcc.set_ack_enabled(True)
        dcc.set_ack_width(6000)

    def test_register_write(self, dcc):
        resp = dcc.send_command("SVC REG WRITE 1 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_register_verify_match(self, dcc):
        dcc.send_command("SVC REG WRITE 1 42")
        dcc.wait_svc_result()
        resp = dcc.send_command("SVC REG VERIFY 1 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_register_verify_mismatch(self, dcc):
        dcc.send_command("SVC REG WRITE 1 42")
        dcc.wait_svc_result()
        resp = dcc.send_command("SVC REG VERIFY 1 99")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "NO ACK", f"Expected NO ACK, got '{result}'"


# =============================================================================
# Service Mode — Address-Only (S-9.2.3)
# =============================================================================

@pytest.mark.service_mode
@pytest.mark.cv
class TestServiceModeAddress:
    """Address-only mode (S-9.2.3): write CV1 only."""

    @pytest.fixture(autouse=True)
    def setup(self, dcc, svc_mode):
        dcc.set_ack_enabled(True)
        dcc.set_ack_width(6000)

    def test_address_write(self, dcc):
        resp = dcc.send_command("SVC ADDR WRITE 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_address_verify_match(self, dcc):
        """Write address 42, then verify 42 — should ACK."""
        dcc.send_command("SVC ADDR WRITE 42")
        dcc.wait_svc_result()
        resp = dcc.send_command("SVC ADDR VERIFY 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_address_verify_mismatch(self, dcc):
        """Write address 42, then verify 99 — should NOT ACK."""
        dcc.send_command("SVC ADDR WRITE 42")
        dcc.wait_svc_result()
        resp = dcc.send_command("SVC ADDR VERIFY 99")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "NO ACK", f"Expected NO ACK, got '{result}'"


# =============================================================================
# Service Mode — ACK Disabled
# =============================================================================

@pytest.mark.service_mode
@pytest.mark.compliance
class TestServiceModeAckDisabled:
    """Verify that disabling ACK on the decoder forces NO ACK on the CS."""

    @pytest.fixture(autouse=True)
    def setup(self, dcc, svc_mode):
        dcc.set_ack_width(6000)

    def test_write_with_ack_disabled(self, dcc):
        dcc.set_ack_enabled(False)
        resp = dcc.send_command("SVC DIRECT WRITE 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "NO ACK", f"Expected NO ACK, got '{result}'"

    def test_verify_with_ack_disabled(self, dcc):
        """Verify operation also returns NO ACK when decoder ACK is off."""
        # First write with ACK on so value is stored
        dcc.set_ack_enabled(True)
        dcc.send_command("SVC DIRECT WRITE 5 42")
        dcc.wait_svc_result()
        # Now disable ACK and verify matching value — should still fail
        dcc.set_ack_enabled(False)
        resp = dcc.send_command("SVC DIRECT VERIFY 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "NO ACK", f"Expected NO ACK, got '{result}'"

    def test_write_with_ack_reenabled(self, dcc):
        dcc.set_ack_enabled(True)
        resp = dcc.send_command("SVC DIRECT WRITE 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"


# =============================================================================
# Service Mode — ACK Pulse Width Compliance (S-9.2.3)
# =============================================================================

@pytest.mark.service_mode
@pytest.mark.compliance
class TestServiceModeCompliance:
    """ACK pulse width compliance per NMRA S-9.2.3.

    S-9.2.3 Section D: "at least 60 mA for 6 ms +/- 1 ms"
    Minimum valid pulse: 5 ms. No defined maximum.
    """

    @pytest.fixture(autouse=True)
    def setup(self, dcc, svc_mode):
        dcc.set_ack_enabled(True)

    def test_ack_normal_6ms(self, dcc):
        """6 ms pulse — nominal per spec."""
        dcc.set_ack_width(6000)
        resp = dcc.send_command("SVC DIRECT WRITE 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_ack_min_valid_5ms(self, dcc):
        """5 ms pulse — right at NMRA minimum."""
        dcc.set_ack_width(5000)
        resp = dcc.send_command("SVC DIRECT WRITE 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_ack_long_valid_8ms(self, dcc):
        """8 ms pulse — no maximum per NMRA spec."""
        dcc.set_ack_width(8000)
        resp = dcc.send_command("SVC DIRECT WRITE 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_ack_too_short_3ms(self, dcc):
        """3 ms pulse — below 5 ms minimum, should fail."""
        dcc.set_ack_width(3000)
        resp = dcc.send_command("SVC DIRECT WRITE 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "NO ACK", f"Expected NO ACK, got '{result}'"

    def test_ack_too_short_4ms(self, dcc):
        """4 ms pulse — still below 5 ms minimum, should fail."""
        dcc.set_ack_width(4000)
        resp = dcc.send_command("SVC DIRECT WRITE 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "NO ACK", f"Expected NO ACK, got '{result}'"

    def test_ack_long_10ms(self, dcc):
        """10 ms pulse — well above minimum, no max per NMRA."""
        dcc.set_ack_width(10000)
        resp = dcc.send_command("SVC DIRECT WRITE 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_ack_very_long_20ms(self, dcc):
        """20 ms pulse — extreme duration, still valid per NMRA (no max)."""
        dcc.set_ack_width(20000)
        resp = dcc.send_command("SVC DIRECT WRITE 5 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"


# =============================================================================
# Service Mode — Sequential Operations
# =============================================================================

@pytest.mark.service_mode
class TestServiceModeSequential:
    """Verify multiple service mode operations in succession."""

    @pytest.fixture(autouse=True)
    def setup(self, dcc, svc_mode):
        dcc.set_ack_enabled(True)
        dcc.set_ack_width(6000)

    def test_consecutive_writes(self, dcc):
        """Multiple direct writes in a row all succeed."""
        for cv, value in [(5, 10), (6, 20), (7, 30)]:
            resp = dcc.send_command(f"SVC DIRECT WRITE {cv} {value}")
            assert resp.startswith("OK"), f"CV{cv} command failed: {resp}"
            result = dcc.wait_svc_result()
            assert result == "SUCCESS", f"CV{cv}={value}: expected SUCCESS, got '{result}'"

    def test_write_then_verify_multiple_cvs(self, dcc):
        """Write several CVs, then verify each — confirms persistence."""
        test_data = [(5, 55), (6, 66), (7, 77)]

        # Write all
        for cv, value in test_data:
            dcc.send_command(f"SVC DIRECT WRITE {cv} {value}")
            result = dcc.wait_svc_result()
            assert result == "SUCCESS", f"Write CV{cv}={value} failed: {result}"

        # Verify all
        for cv, value in test_data:
            dcc.send_command(f"SVC DIRECT VERIFY {cv} {value}")
            result = dcc.wait_svc_result()
            assert result == "SUCCESS", f"Verify CV{cv}={value} failed: {result}"

    def test_mixed_success_and_failure(self, dcc):
        """Write CV, verify match (SUCCESS), verify mismatch (NO ACK)."""
        dcc.send_command("SVC DIRECT WRITE 5 42")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS"

        dcc.send_command("SVC DIRECT VERIFY 5 42")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS"

        dcc.send_command("SVC DIRECT VERIFY 5 99")
        result = dcc.wait_svc_result()
        assert result == "NO ACK"

    def test_write_after_no_ack_succeeds(self, dcc):
        """A failed verify should not prevent subsequent writes."""
        # Force a NO ACK
        dcc.send_command("SVC DIRECT WRITE 5 42")
        dcc.wait_svc_result()
        dcc.send_command("SVC DIRECT VERIFY 5 99")
        result = dcc.wait_svc_result()
        assert result == "NO ACK"

        # Next write should still succeed
        resp = dcc.send_command("SVC DIRECT WRITE 5 100")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS"


# =============================================================================
# Service Mode — High CV Numbers
# =============================================================================

@pytest.mark.service_mode
@pytest.mark.cv
class TestServiceModeHighCV:
    """Test service mode with high CV numbers (direct mode supports 1-1024)."""

    @pytest.fixture(autouse=True)
    def setup(self, dcc, svc_mode):
        dcc.set_ack_enabled(True)
        dcc.set_ack_width(6000)

    def test_direct_write_cv256(self, dcc):
        """CV 256 — beyond single byte addressing."""
        resp = dcc.send_command("SVC DIRECT WRITE 256 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_direct_verify_cv256(self, dcc):
        """Write then verify CV 256."""
        dcc.send_command("SVC DIRECT WRITE 256 42")
        dcc.wait_svc_result()
        resp = dcc.send_command("SVC DIRECT VERIFY 256 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"

    def test_direct_write_cv1024(self, dcc):
        """CV 1024 — maximum CV number per S-9.2.3."""
        resp = dcc.send_command("SVC DIRECT WRITE 1024 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"


# =============================================================================
# Service Mode — Register Mode Boundaries
# =============================================================================

@pytest.mark.service_mode
@pytest.mark.cv
class TestServiceModeRegisterBoundaries:
    """Register mode boundary tests — registers 1-8 map to CVs 1-8."""

    @pytest.fixture(autouse=True)
    def setup(self, dcc, svc_mode):
        dcc.set_ack_enabled(True)
        dcc.set_ack_width(6000)

    def test_register_1_write_verify(self, dcc):
        """Register 1 maps to CV1 (primary address)."""
        dcc.send_command("SVC REG WRITE 1 42")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS"
        dcc.send_command("SVC REG VERIFY 1 42")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS"

    def test_register_5_write_verify(self, dcc):
        """Register 5 maps to CV5 (Vmax)."""
        dcc.send_command("SVC REG WRITE 5 100")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS"
        dcc.send_command("SVC REG VERIFY 5 100")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS"

    def test_register_8_write(self, dcc):
        """Register 8 maps to CV8 (manufacturer ID / factory reset)."""
        resp = dcc.send_command("SVC REG WRITE 8 42")
        assert resp.startswith("OK")
        result = dcc.wait_svc_result()
        assert result == "SUCCESS", f"Expected SUCCESS, got '{result}'"
