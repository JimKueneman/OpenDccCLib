# Saleae Hardware-in-the-Loop (HIL) Compliance — Setup & Run Guide

This firmware (`saleae_hil_compliance`) turns an MSPM0G3507 LaunchPad into a DCC
command-station device-under-test (DUT) that the Python compliance harness drives over
UART while a Saleae logic analyzer wire-verifies the DCC output against the NMRA specs
(S-9.1 / S-9.2 / S-9.2.1 / S-9.3.2, with S-9.2.3 service mode in progress).

```
  ┌─────────────┐   USB-CDC UART (230400)    ┌──────────────────────┐
  │  Python      │ ─────────────────────────►│  MSPM0G3507 LaunchPad │
  │  harness     │   "SPEED 3 64 FWD" etc.   │  (this firmware)      │
  │ (test/       │◄───────────────────────── │                       │
  │  compliance) │   "OK: ..." responses     └───────┬───────────────┘
  └──────┬───────┘                                   │ DCC out (PB1), trigger (PB3)
         │ logic2-automation gRPC (port 10430)       ▼
  ┌──────▼───────┐                            ┌──────────────┐
  │  Saleae       │◄───────── probes ─────────│  Saleae Logic │
  │  Logic 2 app  │                           │  analyzer     │
  └───────────────┘                           └──────────────┘
```

The harness sends a command, the Saleae captures the resulting DCC waveform, and the
suite decodes the bits and checks them against the spec. The firmware pulses a **test
trigger** (PB3) on the packet under test so the analyzer can hardware-trigger on the exact
packet.

---

## 0. Quick re-setup checklist (after travel)

Tear-down: just unplug. Nothing is stored on the bench except the firmware (already in the
LaunchPad flash) and the Python venv (in this repo). To bring it back up:

1. **USB:** plug in the **LaunchPad** (XDS110 — gives the UART + flashing) and the **Saleae**.
2. **Probe wires** (Saleae → LaunchPad), common ground first:
   - **D0 → PB1** (main DCC out)
   - **D1 → PB3** (test trigger / DEBUG)
   - **D3 → PB4** (service-track DCC out)
   - **GND → GND**
3. **Logic 2:** launch it; Preferences → **Automation** → enable the API (port **10430**). Leave it running.
4. **Firmware:** already flashed. If the board was wiped, re-flash the `saleae_hil_compliance`
   Debug build from CCS. (After any service-mode test, tap **RESET** to clear singleton state.)
5. **Run:** `cd test/compliance && .venv/bin/python run_all.py`
   (single suite, e.g. `.venv/bin/python s9_2_1_compliance.py`). If the venv is missing, recreate it — §3c.

That's the whole bench. Details, troubleshooting, and the pending mock-ACK item are below.

---

## 1. Hardware needed

- **MSPM0G3507 LaunchPad** (LP-MSPM0G3507) flashed with this `saleae_hil_compliance` firmware.
- **Saleae Logic** analyzer (original Logic or Logic Pro) + the **Logic 2** desktop app.
- A few jumper wires and a common ground between the Saleae and the LaunchPad.
- USB cables: one for the LaunchPad (XDS110 USB-CDC UART + flashing), one for the Saleae.

## 2. Wiring

Connect the Saleae digital channels to the LaunchPad pins and tie grounds together:

| Saleae channel | LaunchPad pin | Signal                         | Used by                         |
|:--------------:|:-------------:|--------------------------------|---------------------------------|
| **D0 (ch 0)**  | **PB1**       | DCC main-track output          | all suites                      |
| **D1 (ch 1)**  | **PB3**       | Test trigger (DEBUG)           | triggered captures              |
| **D3 (ch 3)**  | **PB4**       | DCC service-track output       | S-9.2.3 service mode            |
| D2 (ch 2)*     | TBD*          | Mock-ACK loopback → PB12       | S-9.2.3 ACK test (not yet wired)|
| **GND**        | **GND**       | common ground                  | always                          |

These four (D0, D1, D3, GND) are the **current working setup** — wire all of them on
re-assembly. The channel-to-pin map lives at the top of `test/compliance/compliance_lib.py`
(`DIGITAL_CHANNEL`=0, `TRIGGER_CHANNEL`=1); the S-9.2.3 service channel (3) is set inside
`s9_2_3_compliance.py` (`SERVICE_CHANNEL`).

\* The mock-ACK loopback (D2 → a TBD GPIO → PB12) is **not yet wired** — that's tomorrow's
S-9.2.3 ACK work (see below and `documentation/ServiceModeOperationsMatrix.md`).

> Ground first. A missing common ground gives garbled or absent captures.

### Still TODO for the S-9.2.3 ACK test (not yet wired)

The service-track channel (ch3/PB4) is already in place and the s9_2_3 suite passes its
packet + parallel-track-timing checks. The remaining hardware item is the **mock-ACK loopback**
for the ACK-detection boundary tests:

1. **(already done) Service track on ch3/PB4.** The service-mode packets
   (≥20-bit preamble) come out here, separate from the main track. Wire e.g. **D3/ch 3 → PB4**
   and set the service-track channel in `compliance_lib.py`.

2. **Mock-ACK output GPIO — pick a pin and jumper it to PB12.** Two ACK pins are involved:
   - **PB12 — ACK sense input (fixed).** The firmware already reads it via
     `current_sense_read()`; the comparator/ACK signal lands here.
   - **Mock-ACK output GPIO — TBD, you must choose it.** A spare LaunchPad GPIO configured
     as an output, **jumpered to PB12**, that the HIL firmware pulses (via
     `SVC MOCKACK <width_us>`) to drive a controlled-width ACK back into the sense input.
     Add it to the HIL SysConfig.

3. **(Optional) Probe the mock-ACK pin — D2/ch 2.** Lets the suite independently measure the
   pulse width the firmware actually produced.

```
mock-ACK out (TBD GPIO) ──jumper──► PB12 (ACK sense in) ──► current_sense_read() ──► ack_sample()
service track (PB4) ─────────────► Saleae ch 3  (decode service-mode packets)
```

These are the open hardware items for the S-9.2.3 HIL work; everything else for that test is
specced in `documentation/ServiceModeOperationsMatrix.md` (“ACK Pulse-Width Verification”).

## 3. Software prerequisites

**a) Code Composer Studio** to build and flash this firmware (see §4).

**b) Saleae Logic 2** with the **Automation API enabled**:
- Logic 2 → Preferences → **Automation** → enable the API server.
- It listens on gRPC port **10430** (this is the automation port, *not* the MCP port 10530).
- Leave the Logic 2 app **running** while you run the suites.

**c) Python venv** for the harness (one-time), created under `test/compliance/.venv`:

```bash
cd test/compliance
python3 -m venv .venv
.venv/bin/pip install logic2-automation pyserial
```

## 4. Flash the firmware

1. Open the `saleae_hil_compliance` project in Code Composer Studio.
2. Build the **Debug** configuration (it uses the `dcc_lib` symlink to `src/dcc`).
3. Flash to the LaunchPad.

The firmware boots with the track powered **off**; the harness powers it on over UART.

## 5. Run the compliance tests

All commands run from `test/compliance/`. The DUT serial port is **auto-detected** among
`/dev/cu.usbmodem*` at **230400 baud** — no need to specify it normally.

**Run everything:**
```bash
cd test/compliance
.venv/bin/python run_all.py
```

**Run a single spec suite:**
```bash
.venv/bin/python s9_2_1_compliance.py      # S-9.2.1 extended packet formats
.venv/bin/python s9_1_compliance.py        # S-9.1 electrical/timing
.venv/bin/python s9_2_compliance.py        # S-9.2 baseline packets
.venv/bin/python s9_3_2_compliance.py      # S-9.3.2 RailCom
```

Each suite:
1. opens the DUT UART and powers the track on,
2. sends commands and captures the DCC wire via the Saleae,
3. decodes and checks each packet against the spec,
4. prints a pass/fail summary and writes an HTML report.

## 6. Results

- Console prints per-check `PASS` / `FAIL` / `n/a` and an overall summary.
- A timestamped HTML report is written to **`test/compliance/reports/*.html`**
  (combined report for `run_all.py`, per-suite for a standalone run).

## 7. Useful config knobs

Top of `test/compliance/compliance_lib.py`:

| Setting            | Default        | Meaning                                            |
|--------------------|----------------|----------------------------------------------------|
| `DIGITAL_CHANNEL`  | `0`            | Saleae channel on DCC out (PB1)                    |
| `TRIGGER_CHANNEL`  | `1`            | Saleae channel on the test trigger (PB3)           |
| `AUTOMATION_PORT`  | `10430`        | Logic 2 automation gRPC port                       |
| `SERIAL_PORT`      | `""`           | `""` = auto-detect; or set `/dev/cu.usbmodemXXXX`  |
| `SERIAL_BAUD`      | `230400`       | DUT UART baud                                       |
| `SAMPLE_RATE_HZ`   | `24_000_000`   | Saleae digital sample rate                          |

## 8. Driving the DUT by hand (optional)

The firmware accepts the same UART command set as the normal command-station example, plus
the HIL-only `TRIG` (arm the PB3 trigger). Open the DUT port in any serial terminal at
230400 8N1 and type `HELP`. Examples:

```
POWER ON
SPEED 3 64 FWD 128
SVC ENTER
SVC DIRECT READ 8
SVC DETECT
TRIG                     # HIL-only: pulse PB3 on the next non-idle packet
```

## 9. Troubleshooting

| Symptom                                   | Likely cause / fix                                             |
|-------------------------------------------|---------------------------------------------------------------|
| `Missing dependency` on run               | venv not set up — see §3c                                      |
| Cannot connect to automation / port 10430 | Logic 2 not running, or Automation API not enabled (§3b)       |
| `could not find the DUT UART`             | LaunchPad not enumerated; check USB, or set `SERIAL_PORT`      |
| Empty / garbled capture                   | no common ground, or probes on wrong pins (§2)                 |
| Triggered suites time out                 | trigger probe not on PB3, or firmware not pulsing (try `TRIG`) |

---

For the service-mode (S-9.2.3) HIL plan, including the mock-ACK loopback and the
`s9_2_3_compliance.py` boundary suite, see the **ACK Pulse-Width Verification** section of
`documentation/ServiceModeOperationsMatrix.md`.
