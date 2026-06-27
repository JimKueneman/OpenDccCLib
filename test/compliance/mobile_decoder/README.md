# Mobile-decoder HIL suites

Compliance suites whose **device-under-test is a mobile decoder**, stimulated by the
**waveform player** (`../saleae_hil_waveform_player`, a two-board rig — see its
`SPEC.md` / `IMPLEMENTATION.md`). The player emits the DCC stimulus; the decoder decodes it
and reports back / drives its RailCom-Tx and ACK pins, which the Saleae validates.

Empty for now — the player firmware is the prerequisite. When the first suite lands here:

- `ln -s ../compliance_lib.py compliance_lib.py` (Saleae capture / decode helpers).
- import the player's host driver `wfplayer.py` (from the player project) to compose stimuli.
- name suites by spec, e.g. `s9_1_compliance.py`, `s9_2_1_compliance.py` — the `mobile_decoder/`
  dir disambiguates them from the CS suites of the same spec.
- bring in the DUT firmware as `saleae_hil_compliance/` (+ its `.theia-workspace`) — cut-paste
  from `command_station/`, then modify for the decoder role (`DCC_COMPILE_DECODER`).

See [`../README.md`](../README.md) for the role-dir convention.
