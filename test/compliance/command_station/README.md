# Command-station HIL suites

Compliance suites whose **device-under-test is a command station** — an MSPM0G3507 LaunchPad
running the `saleae_hil_compliance` firmware. The suites drive the DUT over UART and
wire-verify its DCC output on a Saleae against the NMRA specs.

What's in this folder:

| Item | What it is |
|---|---|
| `s9_*_compliance.py`, `library_compliance.py` | the spec suites (each exposes `run()`) |
| `run_all.py` | runs every suite into one combined HTML report |
| `compliance_lib.py` | **symlink** → `../compliance_lib.py` (shared Saleae/serial/decode); also makes `reports/` land here |
| `reports/` | generated HTML reports (auto-created, gitignored) |
| `HIL_SETUP.md` | bench wiring (pins, Saleae channels, jumpers) + how to run |
| `saleae_hil_compliance/` | the CS **DUT firmware** (CCS project) |
| `saleae_hil_compliance.theia-workspace` | open this in CCS to load the firmware project |

Run from `test/compliance/`:
```bash
.venv/bin/python command_station/run_all.py            # all suites → one report
.venv/bin/python command_station/s9_2_1_compliance.py  # a single suite
```
See [`HIL_SETUP.md`](HIL_SETUP.md) for the bench and [`../README.md`](../README.md) for the
role-dir convention.
