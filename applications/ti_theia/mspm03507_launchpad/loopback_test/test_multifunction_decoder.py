"""DCC loopback integration tests — Multifunction (Locomotive) Decoder.

Tests for multifunction decoder commands: speed, functions, consist,
binary state, analog output, ops-mode CV, and
address boundaries.

Usage:
    pytest test_multifunction_decoder.py -m multifunction -v
    pytest test_multifunction_decoder.py -m speed -v
    pytest test_multifunction_decoder.py -m cv -v
"""

import pytest
import time
from dcc_test_fixture import parse_recv


# =============================================================================
# Speed Commands — Short Address
# =============================================================================

@pytest.mark.multifunction
@pytest.mark.speed
class TestSpeedShort:
    """Speed commands with short (7-bit) addresses."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_speed_stop(self, dcc):
        resp = dcc.send_command("SPEED 3 0 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0, "No RECV from decoder"
        p = parse_recv(recv[0])
        assert p["type"] == "SPEED"
        assert p["addr"] == "3"
        assert p["speed"] == "0"
        assert p["dir"] == "FWD"
        assert p["mode"] == "128"

    def test_speed_mid(self, dcc):
        resp = dcc.send_command("SPEED 3 64 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "SPEED"
        assert p["addr"] == "3"
        assert p["speed"] == "64"
        assert p["dir"] == "FWD"

    def test_speed_max(self, dcc):
        resp = dcc.send_command("SPEED 3 127 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["speed"] == "127"
        assert p["dir"] == "FWD"

    def test_speed_reverse(self, dcc):
        resp = dcc.send_command("SPEED 3 64 REV")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["speed"] == "64"
        assert p["dir"] == "REV"

    def test_speed_1_is_estop(self, dcc):
        """Speed value 1 in 128-step mode is emergency stop."""
        resp = dcc.send_command("SPEED 3 1 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ESTOP"
        assert p["addr"] == "3"


# =============================================================================
# Speed Commands — 28-Step Mode (S-9.2)
# =============================================================================

@pytest.mark.multifunction
@pytest.mark.speed
class TestSpeed28Step:
    """28-step speed mode per S-9.2.

    In 28-step mode:
        speed 0 = stop
        speed 1 = emergency stop
        speed 2-29 = speed steps 1-28
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_speed_28_stop(self, dcc):
        resp = dcc.send_command("SPEED 3 0 FWD 28")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "SPEED"
        assert p["speed"] == "0"
        assert p["mode"] == "28"

    def test_speed_28_estop(self, dcc):
        """Speed 1 in 28-step mode is emergency stop."""
        resp = dcc.send_command("SPEED 3 1 FWD 28")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ESTOP"

    def test_speed_28_min(self, dcc):
        """Speed 2 is minimum real speed in 28-step."""
        resp = dcc.send_command("SPEED 3 2 FWD 28")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "SPEED"
        assert p["speed"] == "2"
        assert p["mode"] == "28"

    def test_speed_28_mid(self, dcc):
        resp = dcc.send_command("SPEED 3 15 FWD 28")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["speed"] == "15"
        assert p["mode"] == "28"

    def test_speed_28_max(self, dcc):
        """Speed 29 is maximum in 28-step mode (step 28)."""
        resp = dcc.send_command("SPEED 3 29 FWD 28")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["speed"] == "29"

    def test_speed_28_reverse(self, dcc):
        resp = dcc.send_command("SPEED 3 15 REV 28")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["dir"] == "REV"
        assert p["mode"] == "28"


# =============================================================================
# Speed Commands — 14-Step Mode (S-9.2)
# =============================================================================

@pytest.mark.multifunction
@pytest.mark.speed
class TestSpeed14Step:
    """14-step speed mode per S-9.2.

    In 14-step mode:
        speed 0 = stop
        speed 1 = emergency stop
        speed 2-15 = speed steps 1-14
    Headlight (FL) is encoded in the speed instruction (not function group 1).
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")
        # CV29 bit 1 = 0 selects 14-step mode (default 0x06 = 28-step)
        dcc.send_command("CV WRITE 3 29 4")
        dcc.clear_decoder()
        yield
        # Restore 28-step mode for subsequent tests
        dcc.send_command("CV WRITE 3 29 6")
        dcc.clear_decoder()

    def test_speed_14_stop(self, dcc):
        resp = dcc.send_command("SPEED 3 0 FWD 14")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "SPEED"
        assert p["speed"] == "0"
        assert p["mode"] == "14"

    def test_speed_14_estop(self, dcc):
        """Speed 1 in 14-step mode is emergency stop."""
        resp = dcc.send_command("SPEED 3 1 FWD 14")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ESTOP"

    def test_speed_14_min(self, dcc):
        """Speed 2 is minimum real speed in 14-step."""
        resp = dcc.send_command("SPEED 3 2 FWD 14")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "SPEED"
        assert p["speed"] == "2"
        assert p["mode"] == "14"

    def test_speed_14_max(self, dcc):
        """Speed 15 is maximum in 14-step mode (step 14)."""
        resp = dcc.send_command("SPEED 3 15 FWD 14")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["speed"] == "15"

    def test_speed_14_reverse(self, dcc):
        resp = dcc.send_command("SPEED 3 8 REV 14")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["dir"] == "REV"
        assert p["mode"] == "14"


# =============================================================================
# Speed Commands — Long Address
# =============================================================================

@pytest.mark.multifunction
@pytest.mark.speed
class TestSpeedLong:
    """Speed commands with long (14-bit) addresses."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(200, "LONG")

    def test_speed_long_default(self, dcc):
        """Address 200 auto-types to long (>=128)."""
        resp = dcc.send_command("SPEED 200 64 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "SPEED"
        assert p["addr"] == "200"
        assert p["speed"] == "64"

    def test_speed_long_explicit(self, dcc):
        """Explicit L suffix forces long address."""
        resp = dcc.send_command("SPEED 200L 64 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["addr"] == "200"


# =============================================================================
# Speed Commands — Forced Short
# =============================================================================

@pytest.mark.multifunction
@pytest.mark.speed
class TestSpeedForcedShort:
    """Speed commands with S suffix to force short address."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(40, "SHORT")

    def test_speed_forced_short(self, dcc):
        resp = dcc.send_command("SPEED 40S 64 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["addr"] == "40"
        assert p["speed"] == "64"

    def test_speed_auto_short(self, dcc):
        """Address 40 auto-types to short (<=127)."""
        resp = dcc.send_command("SPEED 40 64 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["addr"] == "40"


# =============================================================================
# Speed Boundary Values (S-9.2.1: 128-step encoding)
# =============================================================================

@pytest.mark.multifunction
@pytest.mark.speed
class TestSpeedBoundaries:
    """128-step speed encoding boundary values per S-9.2.1.

    In 128-step mode:
        speed 0 = stop
        speed 1 = emergency stop
        speed 2 = minimum real speed (step 1)
        speed 127 = maximum speed (step 126)
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_speed_2_minimum_real(self, dcc):
        """Speed 2 is the minimum actual movement speed in 128-step."""
        resp = dcc.send_command("SPEED 3 2 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "SPEED"
        assert p["speed"] == "2"

    def test_speed_126(self, dcc):
        """Speed 126 is near-max in 128-step."""
        resp = dcc.send_command("SPEED 3 126 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["speed"] == "126"


# =============================================================================
# E-Stop Commands
# =============================================================================

@pytest.mark.multifunction
@pytest.mark.speed
class TestEstop:
    """Emergency stop commands."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_estop_addressed(self, dcc):
        resp = dcc.send_command("ESTOP 3")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ESTOP"
        assert p["addr"] == "3"

    def test_estop_broadcast(self, dcc):
        """Broadcast e-stop should be received by all decoders."""
        resp = dcc.send_command("ESTOP")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ESTOP"
        assert p["addr"] == "0"


# =============================================================================
# Function Commands
# =============================================================================

@pytest.mark.multifunction
@pytest.mark.functions
class TestFunctions:
    """Function commands across all groups.

    Note: Function commands are cumulative bitmasks. Each test sends one
    FUNC ON command. The decoder fires on_function_command for EVERY function
    in the group. We check that the target function appears in the RECV lines.
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")
        # Reboot or fresh loco state — function bitmasks start at 0
        # Since we use REFRESH OFF, old loco state from prior tests
        # may persist. Each test checks that the target function appears.

    def _send_func_and_check(self, dcc, func_num, state="ON"):
        """Helper: send FUNC command and verify target function in RECV.

        DCC function commands encode the entire group as a bitmask, so
        the decoder fires on_function_command for every function in the
        group (up to 8 RECV lines).  We collect enough lines to capture
        the whole group, then search for the target function.
        """
        resp = dcc.send_command(f"FUNC 3 {func_num} {state}")
        assert resp.startswith("OK"), f"Command failed: {resp}"
        recv = dcc.wait_recv(timeout=1.0, count=8)
        assert len(recv) > 0, f"No RECV for FUNC 3 {func_num} {state}"

        # Find RECV line for the target function
        found = False
        for line in recv:
            p = parse_recv(line)
            if (p.get("type") == "FUNC" and
                    p.get("func") == str(func_num) and
                    p.get("state") == state):
                found = True
                break
        assert found, (f"Expected FUNC func={func_num} state={state} "
                       f"not found in: {recv}")

    # Group 1: FL (F0), F1-F4
    def test_func_fl(self, dcc):
        self._send_func_and_check(dcc, 0, "ON")

    def test_func_f1(self, dcc):
        self._send_func_and_check(dcc, 1, "ON")

    def test_func_f4(self, dcc):
        self._send_func_and_check(dcc, 4, "ON")

    # Group 2a: F5-F8
    def test_func_f5(self, dcc):
        self._send_func_and_check(dcc, 5, "ON")

    def test_func_f8(self, dcc):
        self._send_func_and_check(dcc, 8, "ON")

    # Group 2b: F9-F12
    def test_func_f9(self, dcc):
        self._send_func_and_check(dcc, 9, "ON")

    def test_func_f12(self, dcc):
        self._send_func_and_check(dcc, 12, "ON")

    # F13-F20
    def test_func_f13(self, dcc):
        self._send_func_and_check(dcc, 13, "ON")

    def test_func_f20(self, dcc):
        self._send_func_and_check(dcc, 20, "ON")

    # F21-F28
    def test_func_f21(self, dcc):
        self._send_func_and_check(dcc, 21, "ON")

    def test_func_f28(self, dcc):
        self._send_func_and_check(dcc, 28, "ON")

    # OFF
    def test_func_f1_off(self, dcc):
        # First turn it on, then off
        dcc.send_command("FUNC 3 1 ON")
        dcc.wait_recv(timeout=0.3)
        dcc.clear_decoder()
        self._send_func_and_check(dcc, 1, "OFF")


# =============================================================================
# Function Commands — F29-F68 (S-9.2.1)
# =============================================================================

@pytest.mark.multifunction
@pytest.mark.functions
class TestFunctionsExtended:
    """Extended function commands F29-F68 per S-9.2.1.

    Tests one representative function from each extended group:
        F29-F36, F37-F44, F45-F52, F53-F60, F61-F68
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def _send_func_and_check(self, dcc, func_num, state="ON"):
        """Helper: send FUNC command and verify target function in RECV.

        DCC function commands encode the entire group as a bitmask, so
        the decoder fires on_function_command for every function in the
        group (up to 8 RECV lines).  We collect enough lines to capture
        the whole group, then search for the target function.
        """
        resp = dcc.send_command(f"FUNC 3 {func_num} {state}")
        assert resp.startswith("OK"), f"Command failed: {resp}"
        recv = dcc.wait_recv(timeout=1.0, count=8)
        assert len(recv) > 0, f"No RECV for FUNC 3 {func_num} {state}"

        found = False
        for line in recv:
            p = parse_recv(line)
            if (p.get("type") == "FUNC" and
                    p.get("func") == str(func_num) and
                    p.get("state") == state):
                found = True
                break
        assert found, (f"Expected FUNC func={func_num} state={state} "
                       f"not found in: {recv}")

    # F29-F36
    def test_func_f29(self, dcc):
        self._send_func_and_check(dcc, 29, "ON")

    def test_func_f36(self, dcc):
        self._send_func_and_check(dcc, 36, "ON")

    # F37-F44
    def test_func_f37(self, dcc):
        self._send_func_and_check(dcc, 37, "ON")

    def test_func_f44(self, dcc):
        self._send_func_and_check(dcc, 44, "ON")

    # F45-F52
    def test_func_f45(self, dcc):
        self._send_func_and_check(dcc, 45, "ON")

    def test_func_f52(self, dcc):
        self._send_func_and_check(dcc, 52, "ON")

    # F53-F60
    def test_func_f53(self, dcc):
        self._send_func_and_check(dcc, 53, "ON")

    def test_func_f60(self, dcc):
        self._send_func_and_check(dcc, 60, "ON")

    # F61-F68
    def test_func_f61(self, dcc):
        self._send_func_and_check(dcc, 61, "ON")

    def test_func_f68(self, dcc):
        self._send_func_and_check(dcc, 68, "ON")

    # OFF for an extended function
    def test_func_f29_off(self, dcc):
        dcc.send_command("FUNC 3 29 ON")
        dcc.wait_recv(timeout=0.3)
        dcc.clear_decoder()
        self._send_func_and_check(dcc, 29, "OFF")


# =============================================================================
# Consist Control (S-9.2.1)
# =============================================================================

@pytest.mark.multifunction
class TestConsist:
    """Consist (multi-unit) control commands per S-9.2.1."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_consist_set_normal(self, dcc):
        resp = dcc.send_command("CONSIST 3 SET 10 NORMAL")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "CONSIST"
        assert p["addr"] == "3"
        assert p["consist"] == "10"
        assert p["dir"] == "NORMAL"

    def test_consist_set_reverse(self, dcc):
        resp = dcc.send_command("CONSIST 3 SET 10 REVERSE")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "CONSIST"
        assert p["consist"] == "10"
        assert p["dir"] == "REVERSE"

    def test_consist_clear(self, dcc):
        resp = dcc.send_command("CONSIST 3 CLEAR")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "CONSIST"
        assert p["consist"] == "0"


# =============================================================================
# Binary State Control — Short (S-9.2.1)
# =============================================================================

@pytest.mark.multifunction
class TestBinaryStateShort:
    """Binary state control (short form, 1-127) per S-9.2.1."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_bss_on(self, dcc):
        resp = dcc.send_command("BSS 3 1 ON")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "BSS"
        assert p["addr"] == "3"
        assert p["state"] == "1"
        assert p["active"] == "ON"

    def test_bss_off(self, dcc):
        resp = dcc.send_command("BSS 3 1 OFF")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "BSS"
        assert p["active"] == "OFF"

    def test_bss_max(self, dcc):
        """Maximum short binary state number (127)."""
        resp = dcc.send_command("BSS 3 127 ON")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["state"] == "127"


# =============================================================================
# Binary State Control — Long (S-9.2.1)
# =============================================================================

@pytest.mark.multifunction
class TestBinaryStateLong:
    """Binary state control (long form, 1-32767) per S-9.2.1."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_bsl_on(self, dcc):
        resp = dcc.send_command("BSL 3 1 ON")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "BSL"
        assert p["addr"] == "3"
        assert p["state"] == "1"
        assert p["active"] == "ON"

    def test_bsl_off(self, dcc):
        resp = dcc.send_command("BSL 3 1 OFF")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["active"] == "OFF"

    def test_bsl_high(self, dcc):
        """High binary state number (32767)."""
        resp = dcc.send_command("BSL 3 32767 ON")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["state"] == "32767"


# =============================================================================
# Analog Function (S-9.2.1)
# =============================================================================

@pytest.mark.multifunction
class TestAnalogFunction:
    """Analog function output commands per S-9.2.1."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_analog_volume(self, dcc):
        """Analog output 1 (volume) at half level."""
        resp = dcc.send_command("ANALOG 3 1 128")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "ANALOG"
        assert p["addr"] == "3"
        assert p["output"] == "1"
        assert p["value"] == "128"

    def test_analog_zero(self, dcc):
        resp = dcc.send_command("ANALOG 3 1 0")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["value"] == "0"

    def test_analog_max(self, dcc):
        resp = dcc.send_command("ANALOG 3 1 255")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["value"] == "255"


# =============================================================================
# CV Commands — Ops Mode
# =============================================================================

@pytest.mark.multifunction
@pytest.mark.cv
class TestCvOpsMode:
    """CV programming in ops (main track) mode."""

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_cv_write_29(self, dcc):
        resp = dcc.send_command("CV WRITE 3 29 6")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "CV_WRITE"
        assert p["cv"] == "29"
        assert p["value"] == "6"

    def test_cv_write_1(self, dcc):
        resp = dcc.send_command("CV WRITE 3 1 3")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["cv"] == "1"
        assert p["value"] == "3"

    def test_cv_verify(self, dcc):
        resp = dcc.send_command("CV VERIFY 3 29 6")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "CV_VERIFY"
        assert p["cv"] == "29"
        assert p["value"] == "6"

    def test_cv_bit_write(self, dcc):
        """CV BIT command: write bit 3 of CV29 to 1."""
        resp = dcc.send_command("CV BIT 3 29 3 1")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "CV_BIT"
        assert p["cv"] == "29"
        assert p["bit"] == "3"
        assert p["value"] == "1"

    def test_cv_bit_clear(self, dcc):
        """CV BIT command: clear bit 0 of CV29."""
        resp = dcc.send_command("CV BIT 3 29 0 0")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "CV_BIT"
        assert p["cv"] == "29"
        assert p["bit"] == "0"
        assert p["value"] == "0"


# =============================================================================
# Ops-Mode CV — Long Address (S-9.2.1)
# =============================================================================

@pytest.mark.multifunction
@pytest.mark.cv
class TestCvOpsModeLongAddress:
    """Ops-mode CV programming with long (14-bit) addresses per S-9.2.1.

    Long address CV commands use a different encoding path than short address.
    The address byte pair is 11AAAAAA AAAAAAAA instead of 0AAAAAAA.
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(200, "LONG")

    def test_cv_write_long_addr(self, dcc):
        """CV write on long address 200."""
        resp = dcc.send_command("CV WRITE 200 29 6")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "CV_WRITE"
        assert p["cv"] == "29"
        assert p["value"] == "6"

    def test_cv_verify_long_addr(self, dcc):
        """CV verify on long address 200."""
        resp = dcc.send_command("CV VERIFY 200 29 6")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "CV_VERIFY"
        assert p["cv"] == "29"
        assert p["value"] == "6"

    def test_cv_bit_write_long_addr(self, dcc):
        """CV bit write on long address 200."""
        resp = dcc.send_command("CV BIT 200 29 3 1")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "CV_BIT"
        assert p["cv"] == "29"
        assert p["bit"] == "3"
        assert p["value"] == "1"

    def test_cv_write_high_long_addr(self, dcc):
        """CV write on high long address 9999."""
        dcc.set_decoder_address(9999, "LONG")
        dcc.clear_decoder()
        resp = dcc.send_command("CV WRITE 9999 1 42")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "CV_WRITE"
        assert p["cv"] == "1"
        assert p["value"] == "42"


# =============================================================================
# CV29 Configuration Bit Interactions (S-9.2.2)
# =============================================================================

@pytest.mark.multifunction
@pytest.mark.cv
class TestCv29Interactions:
    """CV29 configuration bit interactions per S-9.2.2.

    CV29 bits:
        bit 0: direction reversed (0=normal, 1=reversed)
        bit 1: speed step mode (0=14-step, 1=28/128-step)
        bit 5: address mode (0=short CV1, 1=long CV17/CV18)

    These tests write CV29 via ops-mode and verify that subsequent
    commands reflect the changed behavior.
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_cv29_direction_reverse_bit(self, dcc):
        """Setting CV29 bit 0 reverses reported direction.

        With bit 0 = 1, a FWD command should be reported as REV by the decoder.
        """
        # First verify normal: FWD reports FWD
        resp = dcc.send_command("SPEED 3 64 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["dir"] == "FWD"
        dcc.clear_decoder()

        # Set CV29 bit 0 = 1 (direction reversed), keep bit 1 = 1 (28/128 mode)
        # CV29 default is 0x06 (bits 1,2). Set to 0x07 (add bit 0).
        dcc.send_command("CV WRITE 3 29 7")
        dcc.wait_recv(timeout=0.5)
        dcc.clear_decoder()

        # Now FWD should report as REV
        resp = dcc.send_command("SPEED 3 64 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["dir"] == "REV", \
            f"Expected REV after direction reverse bit set, got {p['dir']}"

        # Restore CV29 to default (0x06)
        dcc.send_command("CV WRITE 3 29 6")
        dcc.wait_recv(timeout=0.5)
        dcc.clear_decoder()

    def test_cv29_direction_reverse_rev_becomes_fwd(self, dcc):
        """With CV29 bit 0 set, a REV command should report as FWD."""
        # Set CV29 = 0x07 (direction reversed + 28/128 mode)
        dcc.send_command("CV WRITE 3 29 7")
        dcc.wait_recv(timeout=0.5)
        dcc.clear_decoder()

        resp = dcc.send_command("SPEED 3 64 REV")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["dir"] == "FWD", \
            f"Expected FWD after direction reverse bit set with REV command, got {p['dir']}"

        # Restore CV29
        dcc.send_command("CV WRITE 3 29 6")
        dcc.wait_recv(timeout=0.5)
        dcc.clear_decoder()

    def test_cv29_14step_mode_bit(self, dcc):
        """Clearing CV29 bit 1 switches decoder to 14-step mode.

        With bit 1 = 0, decoder should report mode=14 for speed commands.
        """
        # CV29 default = 0x06 (bits 1,2 set). Clear bit 1 -> 0x04.
        dcc.send_command("CV WRITE 3 29 4")
        dcc.wait_recv(timeout=0.5)
        dcc.clear_decoder()

        resp = dcc.send_command("SPEED 3 10 FWD 14")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["mode"] == "14", \
            f"Expected mode=14 after clearing CV29 bit 1, got {p.get('mode')}"

        # Restore CV29 to default (0x06)
        dcc.send_command("CV WRITE 3 29 6")
        dcc.wait_recv(timeout=0.5)
        dcc.clear_decoder()

    def test_cv29_long_address_bit(self, dcc):
        """Setting CV29 bit 5 switches decoder to long address mode.

        With bit 5 = 1, the decoder uses CV17/CV18 for its address instead of CV1.
        After setting this bit, commands to short address 3 should no longer match.
        """
        # Write CV17/CV18 for long address 200 first
        # CV17 = 0xC0 | (200 >> 8) = 0xC0, CV18 = 200 & 0xFF = 200
        dcc.send_command("CV WRITE 3 17 192")
        dcc.wait_recv(timeout=0.5)
        dcc.clear_decoder()
        dcc.send_command("CV WRITE 3 18 200")
        dcc.wait_recv(timeout=0.5)
        dcc.clear_decoder()

        # Set CV29 bit 5 = 1 (long address mode): 0x06 | 0x20 = 0x26
        dcc.send_command("CV WRITE 3 29 38")
        dcc.wait_recv(timeout=0.5)
        dcc.clear_decoder()

        # Short address 3 should no longer match
        dcc.send_command("SPEED 3 64 FWD")
        no_match = dcc.wait_no_recv(timeout=0.5)
        assert no_match, "Decoder still responding to short address after CV29 bit 5 set"

        # Verify long address 200 DOES match now
        # Use the test harness ADDR command to tell Board B to also accept
        # long 200 (the library cached it from CV17/CV18, but the ADDR
        # command sets the filter separately for the test harness)
        dcc.set_decoder_address(200, "LONG")
        dcc.clear_decoder()
        resp = dcc.send_command("SPEED 200 64 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "SPEED"
        assert p["addr"] == "200"

        # Restore: write CV29 back via the long address (decoder is now long 200)
        dcc.send_command("CV WRITE 200 29 6")
        dcc.wait_recv(timeout=0.5)
        dcc.clear_decoder()

        # Restore decoder to short address 3 for subsequent tests
        dcc.set_decoder_address(3, "SHORT")
        dcc.clear_decoder()


# =============================================================================
# CV Decoder Lock (S-9.2.2: CV15/CV16 mechanism)
# =============================================================================

@pytest.mark.multifunction
@pytest.mark.cv
class TestCvDecoderLock:
    """CV decoder lock mechanism per S-9.2.2.

    CV15 = lock key, CV16 = lock compare value.
    When CV15 != CV16, all CV writes (except CV15/CV16 themselves) are rejected.
    When CV15 == CV16, writes are accepted.

    Requires Board B decoder to support CLEAR command to reset CV store,
    and a way to read back CV state. We test by:
    1. Writing CV with lock open -> should produce RECV CV_WRITE
    2. Locking decoder (write CV16 to different value) -> CV write should NOT produce RECV
    3. Unlocking (write CV15 to match CV16) -> CV write should produce RECV again
    """

    @pytest.fixture(autouse=True)
    def setup_decoder(self, dcc):
        dcc.set_decoder_address(3, "SHORT")

    def test_cv_write_with_lock_open(self, dcc):
        """CV write succeeds when CV15 == CV16 (both default to 0)."""
        resp = dcc.send_command("CV WRITE 3 29 6")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "CV_WRITE"

    def test_cv_write_blocked_when_locked(self, dcc):
        """CV write blocked when CV15 != CV16.

        Write CV16=5 (via ops mode) to change lock compare value.
        Now CV15=0 != CV16=5, so subsequent writes should be rejected.
        """
        # Lock the decoder by writing CV16 to a non-zero value.
        # After a long test suite the CS scheduler may still be refreshing
        # function state, so we must wait until the specific CV_WRITE RECV
        # appears (not just any generic RECV which could be a FUNC refresh).
        dcc.send_command("CV WRITE 3 16 5")
        deadline = time.time() + 3.0
        cv16_confirmed = False
        while time.time() < deadline:
            recv = dcc.wait_recv(timeout=0.5)
            for line in recv:
                if "CV_WRITE" in line and "cv=16" in line:
                    cv16_confirmed = True
                    break
            if cv16_confirmed:
                break
        assert cv16_confirmed, "CV16 lock write was not received by decoder"
        # Drain all repeated packets from the CS scheduler
        dcc.wait_no_recv(timeout=1.0)
        dcc.clear_decoder()

        # Now try to write CV29 — should be blocked.
        # Use a longer timeout and check specifically for CV_WRITE cv=29
        # because the slower RECV drain may still be emitting stale FUNC
        # refresh lines from the scheduler.
        dcc.send_command("CV WRITE 3 29 6")
        recv = dcc.wait_recv(timeout=1.0, count=8)
        cv29_found = any("CV_WRITE" in line and "cv=29" in line for line in recv)
        assert not cv29_found, "CV write succeeded with lock engaged"

    def test_cv_write_after_unlock(self, dcc):
        """CV write succeeds after unlocking (set CV15 to match CV16).

        CV16 was set to 5 in previous test. Write CV15=5 to unlock.
        """
        # Unlock by writing CV15=5 to match CV16=5
        dcc.send_command("CV WRITE 3 15 5")
        dcc.wait_recv(timeout=0.3)  # consume CV15 write RECV
        dcc.clear_decoder()

        # Now CV29 write should succeed
        resp = dcc.send_command("CV WRITE 3 29 6")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["type"] == "CV_WRITE"
        assert p["cv"] == "29"

        # Reset lock state so subsequent test runs don't start locked
        dcc.send_command("CV WRITE 3 15 0")
        dcc.wait_recv(timeout=0.5)
        dcc.send_command("CV WRITE 3 16 0")
        dcc.wait_recv(timeout=0.5)
        dcc.clear_decoder()


# =============================================================================
# Address Boundary Values (S-9.2, S-9.2.1)
# =============================================================================

@pytest.mark.multifunction
class TestAddressBoundaries:
    """Address range boundaries per NMRA specs.

    Short: 1-127 (7-bit, 0AAAAAAA)
    Long: 128-10239 (14-bit, 11AAAAAA AAAAAAAA)
    Broadcast: 0
    """

    def test_short_addr_1(self, dcc):
        """Minimum short address."""
        dcc.set_decoder_address(1, "SHORT")
        dcc.clear_decoder()
        resp = dcc.send_command("SPEED 1S 64 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["addr"] == "1"

    def test_short_addr_127(self, dcc):
        """Maximum short address."""
        dcc.set_decoder_address(127, "SHORT")
        dcc.clear_decoder()
        resp = dcc.send_command("SPEED 127S 64 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["addr"] == "127"

    def test_long_addr_128(self, dcc):
        """Minimum long address (boundary with short)."""
        dcc.set_decoder_address(128, "LONG")
        dcc.clear_decoder()
        resp = dcc.send_command("SPEED 128 64 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["addr"] == "128"

    def test_long_addr_9999(self, dcc):
        """High long address (near max of 10239)."""
        dcc.set_decoder_address(9999, "LONG")
        dcc.clear_decoder()
        resp = dcc.send_command("SPEED 9999L 64 FWD")
        assert resp.startswith("OK")
        recv = dcc.wait_recv()
        assert len(recv) > 0
        p = parse_recv(recv[0])
        assert p["addr"] == "9999"

    def test_short_long_boundary_127_vs_128(self, dcc):
        """Address 127 is short, 128 is long — verify no cross-match."""
        # Decoder set to short 127 should NOT receive long 128
        dcc.set_decoder_address(127, "SHORT")
        dcc.clear_decoder()
        dcc.send_command("SPEED 128 64 FWD")
        assert dcc.wait_no_recv(timeout=0.5), "Short 127 received long 128 packet"
