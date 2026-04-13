"""DCC loopback integration tests — Protocol Infrastructure.

Track-level protocol behavior not specific to any decoder type: idle packets,
repeat counts, failsafe, edge cases, and cross-decoder filtering.

Usage:
    pytest test_protocol_infrastructure.py -m protocol -v
    pytest test_protocol_infrastructure.py -m compliance -v
"""

import pytest
import time
from dcc_test_fixture import parse_recv


# =============================================================================
# Idle Packet Verification
# =============================================================================

@pytest.mark.protocol
class TestIdlePackets:
    """Verify idle packets are transmitted when no commands are pending.

    The decoder should receive idle packets (addr=255, data=0x00) continuously
    when the command station has nothing else to send. Idle packets should NOT
    trigger any decoder callbacks — they are silently consumed.
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_idle_packets_do_not_trigger_callbacks(self, dcc):
        """With no commands sent, only idle packets flow. No RECV expected."""
        dcc.clear_decoder()
        # Wait long enough for several idle packets to be sent
        assert dcc.wait_no_recv(timeout=1.0), \
            "Decoder fired callback on idle packet"


# =============================================================================
# Repeat Count Verification (S-9.2.1: CV writes need >= 2 identical packets)
# =============================================================================

@pytest.mark.protocol
class TestRepeatCounts:
    """Verify packets are sent enough times for decoder to act.

    S-9.2.1 requires ops-mode CV writes to be sent at least twice.
    Our command station sends repeat_count=3 for all one-shot packets.
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_cv_write_received_at_least_once(self, dcc):
        """CV write with repeat_count=3 should be received by decoder."""
        resp = dcc.send_command("CV WRITE 3 1 42")
        assert resp.startswith("OK")
        recv = dcc.wait_recv(timeout=0.5)
        assert len(recv) > 0, "CV write not received — insufficient repeats?"
        p = parse_recv(recv[0])
        assert p["type"] == "CV_WRITE"
        assert p["cv"] == "1"
        assert p["value"] == "42"

    def test_speed_received_with_refresh_off(self, dcc):
        """Speed packet with refresh OFF (repeat_count=3) should be received."""
        resp = dcc.send_command("SPEED 3 50 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv(timeout=0.5)
        assert len(recv) > 0, "Speed packet not received with refresh OFF"
        p = parse_recv(recv[0])
        assert p["type"] == "SPEED"
        assert p["speed"] == "50"


# =============================================================================
# Failsafe Enter/Exit (S-9.2.4)
# =============================================================================

@pytest.mark.protocol
@pytest.mark.compliance
class TestFailsafe:
    """Failsafe enter/exit callbacks per S-9.2.4.

    When DCC signal stops (power off), the decoder should fire
    on_failsafe_entered. When signal resumes, on_failsafe_exited fires.
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_failsafe_entered_on_power_off(self, dcc):
        """Decoder fires FAILSAFE_ENTER when DCC signal stops."""
        dcc.clear_decoder()
        dcc.power_off()
        # Failsafe timeout depends on decoder implementation; allow up to 2s
        recv = dcc.wait_recv(timeout=2.0)
        found = any("FAILSAFE_ENTER" in line for line in recv)
        assert found, f"No FAILSAFE_ENTER received, got: {recv}"
        # Restore power for subsequent tests
        dcc.power_on()
        dcc.wait_recv(timeout=2.0)  # drain FAILSAFE_EXIT and any others
        dcc.clear_decoder()

    def test_failsafe_exited_on_power_restore(self, dcc):
        """Decoder fires FAILSAFE_EXIT when DCC signal resumes."""
        dcc.clear_decoder()
        dcc.power_off()
        # Wait for failsafe entry
        dcc.wait_recv(timeout=2.0)
        dcc.clear_decoder()
        # Restore power
        dcc.power_on()
        recv = dcc.wait_recv(timeout=2.0)
        found = any("FAILSAFE_EXIT" in line for line in recv)
        assert found, f"No FAILSAFE_EXIT received, got: {recv}"

    def test_no_failsafe_with_power_on(self, dcc):
        """No spurious failsafe callbacks while DCC signal is active."""
        dcc.clear_decoder()
        # Power should already be on from previous test cleanup
        recv = dcc.wait_recv(timeout=1.0)
        failsafe = [line for line in recv if "FAILSAFE" in line]
        assert len(failsafe) == 0, f"Spurious failsafe: {failsafe}"


# =============================================================================
# Edge Cases
# =============================================================================

@pytest.mark.protocol
class TestEdgeCases:
    """Edge cases and negative tests."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_power_off_stops_dcc(self, dcc):
        """No packets received when power is off."""
        dcc.power_off()
        time.sleep(0.1)
        dcc.clear_decoder()
        # Use address 99 so stale packets in the scheduler won't match
        # decoder address 3 when power is restored
        dcc.send_command("SPEED 99 64 FWD")
        assert dcc.wait_no_recv(timeout=0.5), "Received DCC with power off"
        # Restore power for remaining tests
        dcc.power_on()
        # Drain any stale packets that may fire after power restore
        dcc.wait_no_recv(timeout=0.5)
        dcc.clear_decoder()

    def test_wrong_address_filtered(self, dcc):
        """Packets for address 5 should not be received by decoder set to 3."""
        dcc.clear_decoder()
        dcc.send_command("SPEED 5 64 FWD")
        assert dcc.wait_no_recv(timeout=0.5), "Decoder received wrong address"

    def test_address_type_mismatch(self, dcc):
        """Long address 3 should not match decoder set to short address 3."""
        dcc.clear_decoder()
        # Drain any in-flight packets from previous tests
        dcc.wait_no_recv(timeout=0.3)
        dcc.clear_decoder()
        dcc.send_command("SPEED 3L 64 FWD")
        assert dcc.wait_no_recv(timeout=0.5), "Decoder matched wrong address type"


# =============================================================================
# Cross-Decoder Filtering
# =============================================================================

@pytest.mark.protocol
class TestCrossDecoderFiltering:
    """Comprehensive cross-decoder type filtering tests.

    Verifies that commands for one decoder type are not received by decoders
    configured for a different type.
    """

    def test_multifunction_to_acc_decoder(self, dcc):
        """Multifunction speed command ignored by ACC decoder."""
        dcc.set_decoder_address(0, "ACC")
        dcc.clear_decoder()
        dcc.send_command("SPEED 3 64 FWD")
        assert dcc.wait_no_recv(timeout=0.5), \
            "ACC decoder received multifunction SPEED"

    def test_multifunction_to_acce_decoder(self, dcc):
        """Multifunction speed command ignored by ACCE decoder."""
        dcc.set_decoder_address(0, "ACCE")
        dcc.clear_decoder()
        dcc.send_command("SPEED 3 64 FWD")
        assert dcc.wait_no_recv(timeout=0.5), \
            "ACCE decoder received multifunction SPEED"

    def test_acc_to_multifunction_decoder(self, dcc):
        """Basic accessory command ignored by multifunction decoder."""
        dcc.set_decoder_address(3, "SHORT")
        dcc.clear_decoder()
        dcc.send_command("ACC 1 0 ON")
        assert dcc.wait_no_recv(timeout=0.5), \
            "Multifunction decoder received ACC command"

    def test_acce_to_multifunction_decoder(self, dcc):
        """Extended accessory command ignored by multifunction decoder."""
        dcc.set_decoder_address(3, "SHORT")
        dcc.clear_decoder()
        dcc.send_command("ACCE 1 0")
        assert dcc.wait_no_recv(timeout=0.5), \
            "Multifunction decoder received ACCE command"

    def test_acc_to_acce_decoder(self, dcc):
        """Basic accessory command ignored by extended accessory decoder."""
        dcc.set_decoder_address(0, "ACCE")
        dcc.clear_decoder()
        dcc.send_command("ACC 1 0 ON")
        assert dcc.wait_no_recv(timeout=0.5), \
            "ACCE decoder received basic ACC command"

    def test_acce_to_acc_decoder(self, dcc):
        """Extended accessory command ignored by basic accessory decoder."""
        dcc.set_decoder_address(0, "ACC")
        dcc.clear_decoder()
        dcc.send_command("ACCE 1 0")
        assert dcc.wait_no_recv(timeout=0.5), \
            "ACC decoder received ACCE command"

    def test_long_addr_to_short_addr_decoder(self, dcc):
        """Long address command ignored by short address decoder."""
        dcc.set_decoder_address(3, "SHORT")
        dcc.clear_decoder()
        dcc.send_command("SPEED 3L 64 FWD")
        assert dcc.wait_no_recv(timeout=0.5), \
            "Short address decoder received long address command"

    def test_short_addr_to_long_addr_decoder(self, dcc):
        """Short address command ignored by long address decoder."""
        dcc.set_decoder_address(200, "LONG")
        dcc.clear_decoder()
        dcc.send_command("SPEED 3 64 FWD")
        assert dcc.wait_no_recv(timeout=0.5), \
            "Long address decoder received short address command"
