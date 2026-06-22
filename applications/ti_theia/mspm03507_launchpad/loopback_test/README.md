# DCC Loopback Test — Setup & Run Guide

## Overview

This test validates the OpenDCC command station by physically transmitting DCC
packets between two MSPM0G3507 LaunchPad boards:

- **Board A (Command Station)** — generates DCC packets on a GPIO pin
- **Board B (Decoder)** — receives DCC via edge-interrupt GPIO and decodes them
- **PC** — a Python/pytest script sends commands to Board A over serial, then
  reads decoded results from Board B and asserts correctness

The suite covers speed, e-stop, functions (F0-F68), accessories, CV programming,
consist, binary state, analog function, speed restriction, and edge cases per
NMRA S-9.2, S-9.2.1, and S-9.2.2.

## Prerequisites

| Item | Notes |
|------|-------|
| 2x LP-MSPM0G3507 LaunchPads | TI MSPM0G3507 LaunchPad dev boards |
| 2x USB cables | Micro-USB, one per board |
| 3x jumper wires | Female-to-female recommended |
| Python 3.8+ | With `pip` available |
| TI Theia IDE (or CCS) | To build and flash firmware |

## Step 1 — Flash the Firmware

1. Open TI Theia IDE (or Code Composer Studio).
2. Import and build the **command_station** project from:
   ```
   applications/ti_theia/mspm03507_launchpad/command_station/
   ```
3. Flash it to **Board A**.
4. Import and build the **decoder** project from:
   ```
   applications/ti_theia/mspm03507_launchpad/decoder/
   ```
5. Flash it to **Board B**.

Both boards should show a blinking heartbeat LED (PB22) once flashed and running.

## Step 2 — Wire the Boards

Signals connect **same-label pin to same-label pin** between the two boards
(all pins are on PORTB):

```
  Board A (Command Station)               Board B (Decoder)
  ========================                =================

  PB1   (DCC signal out)    ─────────────>  PB1   (DCC signal in)
  PB4   (service track out) ─────────────>  PB4   (service track in)
  PB12  (ACK sense in)      <─────────────  PB12  (ACK out)
  PB17  (track select out)  ─────────────>  PB17  (track select in)
  GND                       ───────────────  GND
```

**Required for the main-track tests** (speed, functions, accessories, binary state, analog, ops-mode CV):
- **PB1 → PB1** — the DCC signal wire; carries the actual DCC bitstream.
- **GND ↔ GND** — common ground between the boards. Without it the signal levels
  are undefined and decoding will fail.

**Also required for the service-mode / CV-programming tests:**
- **PB4 → PB4** — service-track DCC signal.
- **PB12 → PB12** — the decoder asserts its ACK current load on PB12; the command
  station senses it on PB12.
- **PB17 → PB17** — track select: the command station drives this to tell the
  decoder whether the main (PB1) or service (PB4) input is active.

**Optional (scope / logic-analyzer only — no inter-board wire needed):** Board A
toggles **PB3** (debug) and **PB2** (DCC mirror); Board B toggles **PB3** on each
decoded edge.

Both boards also connect to the PC via their own USB cable (UART backchannel at
230400 baud).

```
         ┌──── USB ────  Board A (Command Station)
   PC ───┤                   PB1  ──── PB1
         └──── USB ────  Board B (Decoder)
                             GND ────── GND
```

## Step 3 — Identify Serial Ports

Plug both boards into the PC. Each LaunchPad exposes a USB serial (backchannel
UART) port. You need to identify which port belongs to which board.

**macOS:**
```bash
ls /dev/tty.usbmodem*
```

**Linux:**
```bash
ls /dev/ttyACM*
```

**Windows:**
Open Device Manager and look under "Ports (COM & LPT)" for two XDS110 entries.

**Tip:** Plug in one board at a time to identify which port maps to which board.
The command station (Board A) port is `--cmd-port` and the decoder (Board B) port
is `--dec-port`.

You can verify a port by opening a serial terminal (e.g., `screen`, `minicom`, or
PuTTY) at **230400 baud** and typing `STATUS` followed by Enter. Board A will
reply with a `STATUS:` line; Board B will reply with `OK: STATUS ...` including
its decoder address.

## Step 4 — Install Python Dependencies

From the `loopback_test/` directory:

```bash
cd applications/ti_theia/mspm03507_launchpad/loopback_test
pip install -r requirements.txt
```

This installs:
- `pyserial` >= 3.5
- `pytest` >= 7.0

## Step 5 — Run the Tests

```bash
pytest dcc_loopback_test.py \
    --cmd-port /dev/tty.usbmodemXXXX \
    --dec-port /dev/tty.usbmodemYYYY \
    -v
```

Replace the port paths with the actual ports identified in Step 3.

**Useful pytest options:**

| Option | Effect |
|--------|--------|
| `-v` | Verbose — show each test name and result |
| `-x` | Stop on first failure |
| `-k "TestSpeed"` | Run only tests matching a name pattern |
| `--tb=short` | Shorter tracebacks on failure |

A full passing run takes roughly 1-2 minutes depending on serial timeouts.

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `POWER ON failed` or no response from Board A | Wrong `--cmd-port` / `--dec-port` swapped | Swap the two port arguments |
| All RECV assertions fail (no packets received) | Missing GND wire between boards | Connect GND ↔ GND |
| All RECV assertions fail (boards powered) | DCC wire not connected or wrong pin | Verify PB1 → PB1 (DCC) wire |
| Sporadic test failures | Loose jumper wire connection | Reseat the wires firmly |
| `serial.serialutil.SerialException` | Port in use by another program | Close any open serial terminals |
| Board not appearing as a serial port | Board not flashed or USB issue | Re-flash firmware; try a different USB cable |
| Heartbeat LED not blinking | Firmware not running | Re-flash the correct project to the correct board |
