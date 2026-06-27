# HIL compliance suites — layout

Hardware-in-the-loop compliance suites, organized **by device role**. A single NMRA spec
(e.g. S-9.2.1) has features for more than one role — the command station *encodes* a packet,
the mobile decoder *decodes* it — so the suites are separated by role, not just by spec.

```
test/compliance/
  compliance_lib.py        shared: Saleae capture, serial/DUT control, DCC wire-decode
  command_station/         command-station suites + its DUT firmware
    run_all.py  s9_1_…  s9_2_…  s9_2_1_…  s9_2_3_…  s9_3_2_…  library_…
    compliance_lib.py      → symlink to ../compliance_lib.py (so suites import it same-dir)
    reports/               generated HTML reports (per role; auto-created, gitignored)
    saleae_hil_compliance/ CS DUT firmware (CCS project)
  mobile_decoder/          decoder suites + its DUT firmware (future)
  accessory_decoder/       accessory suites + its DUT firmware (future)
  saleae_hil_waveform_player/  shared stimulus instrument — CCS project + host driver
```

## Conventions

- **Role dir = role.** Suites keep spec-based names (`s9_2_1_compliance.py`); the **directory**
  disambiguates roles, so `command_station/s9_2_1_compliance.py` (CS encodes) and a future
  `mobile_decoder/s9_2_1_compliance.py` (decoder decodes) can coexist.
- **Shared library stays at root.** `compliance_lib.py` lives here and is **symlinked into each
  role dir** (same pattern as the `dcc_lib` symlink) so suites do a plain `import compliance_lib`
  with no path hacks. Add the symlink when a role dir gets its first suite:
  `ln -s ../compliance_lib.py <role>/compliance_lib.py`.
- **DB references use role-relative paths.** `compliance.data.js` `refs.hilChecks` files are
  `"command_station/s9_2_3_compliance.py"`, and `validate_compliance.py` discovers suites **recursively**
  (`rglob`) and keys them by path — so names never collide across roles.
- **Firmware lives with its role; the player is shared.** Each role's device-under-test firmware
  is a `saleae_hil_compliance/` CCS project **inside its role dir**, opened via the
  `saleae_hil_compliance.theia-workspace` sitting next to it — *same names in every role*
  (the dir disambiguates), so a new role's DUT is clone-and-tweak. The **waveform player** is the
  one shared instrument (it stimulates mobile *and* accessory DUTs), so it stands alone at the top
  level. Projects are independent — opened/flashed one at a time, no shared workspace.

## Running

From `test/compliance/` (see `command_station/HIL_SETUP.md` for the bench):

```bash
.venv/bin/python command_station/run_all.py            # all command-station suites → one report
.venv/bin/python command_station/s9_2_1_compliance.py  # a single suite
```
