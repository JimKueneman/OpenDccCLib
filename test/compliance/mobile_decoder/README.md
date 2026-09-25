# Mobile-decoder HIL suites

Compliance suites whose **device-under-test is a mobile decoder**, stimulated by the certified
**waveform player** (`../saleae_hil_waveform_player`). Two-board rig: the player emits the DCC
stimulus into the decoder; the decoder decodes it and **reports back over its UART** (`RECV …`
lines — the functional-decode oracle) and raises its ACK pin for the Saleae. See
[`HIL_SETUP.md`](HIL_SETUP.md) for wiring, status and the run guide.

```
  host --UART--> player ==DCC(PB1)==> decoder --UART(RECV lines)--> host (compares to what was sent)
```

## What's here

| Item | What it is |
|---|---|
| `saleae_hil_compliance/` | the **decoder DUT firmware** (CCS project, `DCC_COMPILE_DECODER`) — **built and flashed**. GPIO edge-capture → `DccConfig_decoder_edge_isr` → library → `RECV` report-back over UART. Commands: `ADDR <n> <SHORT\|LONG\|ACC\|ACCE>` (writes CV1 / CV17-18 / CV513+521 and the CV29 / CV541 bits into the app's RAM store, then `DccConfig_reload_address_cvs()`), `ACK …`, `CLEAR`, `STATUS`, `HELP`. Open `saleae_hil_compliance.theia-workspace` in CCS. |
| `dcc_encode.py` | the **independent, spec-derived instruction encoder** — imports nothing from the library. Multifunction (short / long / broadcast address; 14-28-128-step speed and both e-stops; FG1, FG2, F13-F68; consist set / set-reversed / set-0; binary state; analog; POM CV write / verify / bit), accessory (basic in decoder-address and output-address form, extended, 2.4.3 CV access) and service mode (reset; direct verify / write / bit; register and paged 3-byte forms). Every builder cites its S-9.2 / S-9.2.1 / S-9.2.3 section. |
| `decoder_smoke.py` | the 30-second **loop probe** — one hardcoded speed packet, one `RECV` match, no Saleae, no report. Run it first after re-wiring or re-flashing. |
| `s9_1_compliance.py` | **timing acceptance** — spec-valid bit timing / preamble is decoded, out-of-spec is rejected (player marginal timing). |
| `s9_2_compliance.py` | **packet accept / reject** — idle ignored, reset accepted, broadcast e-stop, corrupt XOR rejected, own vs foreign address. |
| `s9_2_1_compliance.py` | **instruction decode** — one wire-proven decode per instruction type (speed modes, function groups, F13+, POM CV ops, consist, binary state, analog, short / long addressing) plus the consist-addressing and accessory-filter rows below. |
| `s9_2_2_compliance.py` | **CV behavioural effects** — CV1 moves the address, CV15/16 lock, CV8 factory reset, CV29 decode + notify, CV31/32 indexed access, and the CV21 / CV22 consist-function rows below. |
| `s9_2_3_compliance.py` | **service-mode ACK pulse** — 3 resets + Direct VERIFY (20-bit preamble); Saleae D1 measures `ACK_OUT`: one 6 ms ± 1 ms pulse per matching verify, silence on mismatch or without resets. |
| `s9_2_4_compliance.py` | **packet-timeout fail-safe** — CV11 silence trips `FAILSAFE_ENTER` on time, packets clear it, CV11 = 0 disables; plus the re-arm rules below. |
| `compliance_lib.py`, `wfplayer.py` | symlinks → the shared Saleae / report lib and the player's host driver |
| `reports/` | generated HTML reports (gitignored) |

## Added 2026-09-25

The library gained CV19 consist addressing, CV21 / CV22 consist-function enables, the
accessory address filter / accessory-type guard and `DccConfig_reload_address_cvs()`; the
suites now cover them on silicon:

| Suite | `@compliance` | What it proves |
|---|---|---|
| `s9_2_1_compliance.py` | `DCC-S9.2.1-DEC-016` | consist set 5 / set 5 reversed / set 0 write CV19 = 5 / 133 / 0 (`RECV CV_WRITE`) and report `RECV CONSIST` |
| | `DCC-S9.2.1-DEC-017` | consist address 5 answers speed, e-stop and — for a reversed unit — reports `dir=REV` to a forward packet; address 6 and a cleared consist stay silent |
| | `DCC-S9.2.1-DEC-018` | `ADDR 7 ACC` / `ACCE`: basic board 7 and extended 7 decode (`RECV ACC` / `RECV ACCE`), board / extended 8 are silent |
| | `DCC-S9.2.1-DEC-019` | an accessory-configured decoder ignores a speed packet to short 7 and the multifunction broadcast e-stop; `ADDR 3 SHORT` brings it back |
| `s9_2_2_compliance.py` | `DCC-S9.2.2-DEC-006` | with CV19 = 5: FG1 to 5 delivers nothing at CV21 = 0, exactly F1 + F3 at CV21 = 0x05; FG2b delivers exactly F9 + F12 at CV22 = 0x24; FG1 to the own address is unaffected |
| | `DCC-S9.2.2-DEC-007` | CV22 bit 0: FL on the consist address is delivered after a forward speed, not after a reverse one |
| `s9_2_4_compliance.py` | `DCC-S9.2.4-DEC-001` | the trip time is now **asserted** (CV11 time ± 0.3 s, was only recorded); a continuous stream to address 99 does not re-arm the timer (trips on time), a continuous broadcast stream exits fail-safe and holds it out |

The `DEC-018` / `DEC-019` tids are new rows for `documentation/compliance/compliance.data.js`.

## Running

```bash
cd test/compliance/mobile_decoder
../.venv/bin/python decoder_smoke.py           # loop alive?  (auto-discovers both ports)
../.venv/bin/python s9_2_1_compliance.py       # any single suite -> reports/mobile_decoder_<spec>.html
```

`PLAYER_PORT` / `DECODER_PORT` override auto-discovery. `s9_2_3_compliance.py` also needs the Saleae
(Logic 2 Automation API on 10430). After a firmware rebuild / reflash, run the smoke test first.

Still open: **RailCom-Tx** (S-9.3.2-DEC) — see the firmware-side gaps in
[`HIL_SETUP.md`](HIL_SETUP.md). Role-dir convention: [`../README.md`](../README.md).
