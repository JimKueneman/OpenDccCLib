"""Serial communication helper for DCC loopback testing.

Manages two serial connections: one to the command station (Board A)
and one to the decoder (Board B). Provides methods to send commands,
wait for responses, and manage decoder state.
"""

import serial
import time


class DCCTestFixture:
    """Manages serial connections to both DCC boards."""

    def __init__(self, cmd_port, dec_port, baud=230400, timeout=1.0):
        """Open serial connections to both boards.

        Args:
            cmd_port: Serial port for Board A (command station).
            dec_port: Serial port for Board B (decoder).
            baud: Baud rate (default 230400).
            timeout: Default serial read timeout in seconds.
        """
        self.cmd = serial.Serial(cmd_port, baud, timeout=timeout)
        self.dec = serial.Serial(dec_port, baud, timeout=timeout)
        # Allow boards to reset after serial open
        time.sleep(0.5)
        self._flush_both()

    def close(self):
        """Close both serial connections."""
        self.cmd.close()
        self.dec.close()

    def _flush_both(self):
        """Discard any stale data in both serial buffers."""
        self.cmd.reset_input_buffer()
        self.dec.reset_input_buffer()

    # =========================================================================
    # Board A (Command Station) methods
    # =========================================================================

    def send_command(self, cmd, timeout=1.0):
        """Send a command to Board A and wait for the response line.

        Args:
            cmd: Command string (e.g., "SPEED 3 64 FWD").
            timeout: Max seconds to wait for response.

        Returns:
            Response string (e.g., "OK: SPEED addr=3 speed=64 dir=FWD").
        """
        self.cmd.reset_input_buffer()
        self.cmd.write((cmd + "\r").encode())
        self.cmd.timeout = timeout

        # Read lines until we get one starting with OK or ERR
        # (skip echo and empty lines)
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = self.cmd.readline().decode(errors="replace").strip()
            if line.startswith("OK") or line.startswith("ERR"):
                return line
        return ""

    def power_on(self):
        """Enable track power on Board A."""
        resp = self.send_command("POWER ON")
        # Allow idle packets to stabilize
        time.sleep(0.2)
        return resp

    def power_off(self):
        """Disable track power on Board A."""
        return self.send_command("POWER OFF")

    def set_refresh(self, on):
        """Enable or disable auto-refresh on Board A.

        Args:
            on: True for auto-refresh ON, False for OFF.
        """
        return self.send_command("REFRESH ON" if on else "REFRESH OFF")

    # =========================================================================
    # Board B (Decoder) methods
    # =========================================================================

    def set_decoder_address(self, addr, addr_type="SHORT"):
        """Set the decoder listen address on Board B.

        Args:
            addr: DCC address number.
            addr_type: One of "SHORT", "LONG", "ACC", "ACCE".
        """
        self.dec.reset_input_buffer()
        self.dec.write(f"ADDR {addr} {addr_type}\r".encode())
        self.dec.timeout = 1.0
        # Consume response
        deadline = time.time() + 1.0
        while time.time() < deadline:
            line = self.dec.readline().decode(errors="replace").strip()
            if line.startswith("OK"):
                return line
        return ""

    def clear_decoder(self):
        """Flush the decoder RECV ring buffer and serial input."""
        self.dec.write(b"CLEAR\r")
        time.sleep(0.05)
        self.dec.reset_input_buffer()

    def wait_recv(self, timeout=0.5, count=1):
        """Wait for RECV lines from Board B.

        Args:
            timeout: Max seconds to wait for all lines.
            count: Minimum number of RECV lines to collect before returning
                   early. Will still wait up to timeout for more.

        Returns:
            List of RECV line strings.
        """
        self.dec.timeout = timeout
        lines = []
        deadline = time.time() + timeout

        while time.time() < deadline:
            remaining = deadline - time.time()
            if remaining <= 0:
                break
            self.dec.timeout = remaining
            raw = self.dec.readline()
            if not raw:
                if len(lines) >= count:
                    break
                continue
            line = raw.decode(errors="replace").strip()
            if line.startswith("RECV"):
                lines.append(line)

        return lines

    def wait_no_recv(self, timeout=0.5):
        """Verify that NO RECV lines arrive within timeout.

        Args:
            timeout: Seconds to wait.

        Returns:
            True if no RECV received (test passes), False if any RECV seen.
        """
        lines = self.wait_recv(timeout=timeout, count=0)
        return len(lines) == 0

    # =========================================================================
    # Service mode helpers
    # =========================================================================

    def svc_enter(self):
        """Enter service mode on the command station."""
        return self.send_command("SVC ENTER")

    def svc_exit(self):
        """Exit service mode on the command station."""
        return self.send_command("SVC EXIT")

    def wait_svc_result(self, timeout=5.0):
        """Wait for a 'SVC RESULT:' line from the command station.

        Service mode operations are asynchronous -- the CS sends 'OK' when
        the operation starts, then later emits 'SVC RESULT: SUCCESS' or
        'SVC RESULT: NO ACK' when the ACK detection sequence completes.

        Args:
            timeout: Max seconds to wait (default 5s to allow retries).

        Returns:
            Result string (e.g., "SUCCESS", "NO ACK", "ERROR") or "" on timeout.
        """
        self.cmd.timeout = timeout
        deadline = time.time() + timeout
        while time.time() < deadline:
            remaining = deadline - time.time()
            if remaining <= 0:
                break
            self.cmd.timeout = remaining
            raw = self.cmd.readline()
            if not raw:
                continue
            line = raw.decode(errors="replace").strip()
            if line.startswith("SVC RESULT:"):
                return line.split(":", 1)[1].strip()
        return ""

    def set_ack_width(self, width_us):
        """Set the ACK pulse width on the decoder board.

        Args:
            width_us: Pulse width in microseconds (1000-20000).

        Returns:
            Response string from decoder.
        """
        self.dec.reset_input_buffer()
        self.dec.write(f"ACK {width_us}\r".encode())
        self.dec.timeout = 1.0
        deadline = time.time() + 1.0
        while time.time() < deadline:
            line = self.dec.readline().decode(errors="replace").strip()
            if line.startswith("OK") or line.startswith("ERR"):
                return line
        return ""

    def set_ack_enabled(self, enabled):
        """Enable or disable ACK pulse generation on the decoder board.

        Args:
            enabled: True to enable, False to disable.

        Returns:
            Response string from decoder.
        """
        cmd = "ACK ON" if enabled else "ACK OFF"
        self.dec.reset_input_buffer()
        self.dec.write(f"{cmd}\r".encode())
        self.dec.timeout = 1.0
        deadline = time.time() + 1.0
        while time.time() < deadline:
            line = self.dec.readline().decode(errors="replace").strip()
            if line.startswith("OK") or line.startswith("ERR"):
                return line
        return ""


def parse_recv(line):
    """Parse a RECV line into a dict of key=value pairs.

    Example:
        "RECV SPEED addr=3 speed=64 dir=FWD mode=128"
        → {"type": "SPEED", "addr": "3", "speed": "64", "dir": "FWD", "mode": "128"}

    Args:
        line: RECV line string.

    Returns:
        Dict with "type" key and all key=value pairs.
    """
    parts = line.split()
    if len(parts) < 2 or parts[0] != "RECV":
        return {}

    result = {"type": parts[1]}
    for part in parts[2:]:
        if "=" in part:
            key, value = part.split("=", 1)
            result[key] = value

    return result
