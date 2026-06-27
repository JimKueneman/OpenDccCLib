# PDF Regeneration Guide

> **Verified against commit `c4dc61e` (2026-04-21), written 2026-06-22.**
> This document explains how to regenerate the five OpenDccCLib PDFs and — more
> importantly — **what to pull from the current source** so each rebuild reflects
> the latest code. It describes the *general* content and structure of each
> document, not the exact prose. Treat the code as the source of truth; if this
> guide and the code disagree, the code wins.

The previous PDFs were produced by a hand-authored generator whose content was
hardcoded and drifted out of sync with the library. That generator was removed.
When regenerating, **re-derive every factual table and code reference from the
files listed below** instead of copying the old text.

---

## 1. The five documents

| File | Type | Audience | One-line purpose |
|---|---|---|---|
| `QuickStartGuide_CommandStation.pdf` | Quick start | New users | Get a command station running on the reference board in minutes |
| `QuickStartGuide_Decoder.pdf` | Quick start | New users | Get a decoder running on the reference board in minutes |
| `DeveloperGuide_CommandStation.pdf` | Developer guide | Integrators / porters | Full explanation of the CS-side stack and how to port it |
| `DeveloperGuide_Decoder.pdf` | Developer guide | Integrators / porters | Full explanation of the decoder-side stack and how to port it |
| `OpenDccCLib_Brochure.pdf` | Marketing one-pager | Evaluators | Sell the library: features, coverage, getting started |

All five are **generated artifacts** — keep them git-ignored (see `.gitignore`).

---

## 2. Build mechanism (tool-agnostic)

The content of these guides is **mostly hand-authored narrative** (tutorials,
wiring, concepts) that does not exist in the source and cannot be auto-extracted.
The recommended path is therefore:

1. **Author the guide bodies as Markdown** (or whatever source format the chosen
   tool consumes), kept in this folder alongside the other living docs.
2. **Convert to PDF at release time** (e.g. Pandoc, or a fresh small generator).
   Page setup used previously, if you want to match the look: US Letter, 1" left/
   right margins, ~0.8" top/bottom (brochure ~0.6"/0.7"); per-page footer
   `OpenDccCLib | <document title>` on the left and `Page N` on the right; the
   brochure uses a centered two-line footer instead of page numbers.
3. **Do not regenerate the API reference by hand** — that comes from Doxygen
   (`Doxyfile` + `../src/mainpage.h`), which reads the headers directly.

The visual style is described in §4 so the look can be reproduced in any tool.

---

## 3. Sources of truth — pull these from the code on every rebuild

This is the heart of the guide. Each content category below names the file(s) to
read for current values. If the code changed, these are what changed with it.

| Content in the PDFs | Pull from | Notes |
|---|---|---|
| Role / service-mode **feature flags** and their dependency rules | `src/dcc/dcc_config.h` (the `#if defined(...)` validation block) | Currently: `DCC_COMPILE_COMMAND_STATION`, `_DECODER`, `_ACCESSORY_DECODER`, and `_SERVICE_MODE_{DIRECT,PAGED,REGISTER,ADDRESS}` |
| **User-configurable constants** (buffer sizes, ACK tuning, function count) and their defaults | `src/dcc/dcc_types.h` (validation) + the `dcc_user_config.h` templates (`templates/typical/`, `test/user_config/typical/`, and each app's own) | e.g. `USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT`, `..._MAX_LOCOS`, `..._RAILCOM_BUFFER_DEPTH`, `..._SERVICE_MODE_RETRIES`, `..._ACK_THRESHOLD_MA`, `..._ACK_MIN/MAX_DURATION_US`, `..._DECODER_MAX_FUNCTIONS` |
| **`dcc_config_t` struct** fields: required hardware drivers, NULL-optional drivers, callbacks | `src/dcc/dcc_config.h` (`dcc_config_t`, `dcc_output_hw_t`, `dcc_railcom_hw_t`) | This is the single most drift-prone area. Re-read the struct in full. |
| **Lifecycle / ISR entry points** | `src/dcc/dcc_config.h` (the `DccConfig_*` externs) | e.g. `DccConfig_initialize/run`, `_58us_timer_isr`, `_railcom_oneshot_timer_isr`, `_100ms_timer_tick`, `_decoder_edge_isr` |
| **Public application API** (the function tables in the dev guides) | `src/dcc/dcc_application_command_station_main_track.h`, `..._service_track.h`, `..._packet.h`, `dcc_application_decoder_cv.h`, `dcc_application_decoder_railcom.h`, `dcc_application_accessory_decoder_railcom.h` | List the actual exported `Dcc*` functions; do not invent or carry over removed ones |
| **Protocol / timing constants** (58µs, zero-bit, preamble, RailCom windows, decoder threshold) | `src/dcc/dcc_defines.h` | e.g. `DCC_ONE_BIT_HALF_PERIOD_US`, `DCC_ZERO_BIT_HALF_PERIOD_US`, `DCC_ZERO_BIT_MAX_TOTAL_DURATION_US`, the `DCC_RAILCOM_*` window values, `DCC_DECODER_HALF_BIT_MAX_US` |
| **Module list & responsibilities** (file-tree and "what each module does" sections) | `documentation/ARCHITECTURE.md` §6 + actual files in `src/dcc/` | ARCHITECTURE.md is kept current; mirror it rather than re-deriving |
| **Service-mode result enum** and **speed-mode enum** | `src/dcc/dcc_types.h` | The dev-guide "result" table must match the actual enum members |
| **Reference-board pin assignments** (wiring tables) | The application projects: `applications/ti_theia/mspm03507_launchpad/command_station/` and `.../decoder/` (SysConfig / `ti_msp_dl_config`, and the `.c` entry points) | **Verify every pin** — the old PDFs disagreed with the loopback wiring (see §6) |
| **Example `main()` / main-loop shape** | The app entry files: `command_station/command_station.c`, `decoder/decoder.c` | Copy the real loop, not a paraphrase |
| **UART CLI command list** (QSG command tables) | `command_station/uart_command_parser.c/.h`, `decoder/decoder_command_parser.c/.h` | Commands change as the demo evolves |
| **Test suite list & counts** | `src/dcc/*_Test.cxx` + `src/dcc/CMakeLists.txt`; run the suite for live counts | Currently 23 test binaries; report counts only if freshly measured |
| **Coverage numbers** (brochure / dev-guide stats) | Build the tests and read the gcovr report under `test/build/gcovr/` | Don't quote stale figures; measure at rebuild time |
| **Status / known limitations / pending features** | `documentation/compliance/ComplianceOverview.md` | If a guide describes a feature, confirm it's actually "implemented" there (e.g. RailCom cutout retiming is pending) |
| **License / author / repo URL** | repo `LICENSE`/file headers, `README.md` | |

---

## 4. Shared visual style (so any tool can match the look)

- **Fonts:** Helvetica family throughout; Courier for code.
- **Palette:** dark navy `#2C3E5A` (titles/H1), steel blue `#3B6FA0` (subtitles/H2/H3, table header bg), light blue `#E8F0FE` (note-box bg), `#F5F7FA` (alt table rows), `#F5F5F5` (code bg), `#333333` (body), `#999999` (footer).
- **Type scale:** title 28, subtitle 14, H1 20, H2 14, H3 11, body 10, code 8, note 9 (italic).
- **Elements:** title page → table of contents → one section per page-break; styled tables with a colored header row and alternating row shading; blue italic "note" callout boxes; gray code blocks.

---

## 5. Per-document recipes (general content + what to pull)

For each, the **outline is a starting skeleton** — adjust to match the code. The
"pull" column points back to §3.

### 5.1 Quick Start Guide — Command Station
- **Goal:** zero-to-running on the reference board.
- **Outline:** What is DCC? → What you need (hardware + **pin table** + software) → Project setup / file structure → `dcc_user_config.h` walkthrough → Build & flash + **main loop** → UART command interface (**command table** + a quick test transcript) → What's next (RailCom, current sense, porting, pointer to dev guide).
- **Pull:** pin table (app SysConfig), `dcc_user_config.h` flags/constants, main-loop from `command_station.c`, command list from `uart_command_parser.*`.

### 5.2 Quick Start Guide — Decoder
- **Goal:** zero-to-running decoder on the reference board.
- **Outline:** What is a decoder? → What you need (**pin table**) → Project setup / file structure → `dcc_user_config.h` (decoder role + function count) → Build & flash + **main loop with edge-buffer drain** → How the decoder works (edge capture → bit decode → packet decode → address match) → Customizing callbacks (**callback table** + sample handlers) → What's next (RailCom responses, persistent CV storage, porting).
- **Pull:** pin table (decoder app), main loop from `decoder.c`, callback list from `dcc_config.h` (decoder section), bit/packet behavior from `dcc_bit_decoder.*` / `dcc_packet_decoder.*` and `dcc_defines.h` thresholds.

### 5.3 Developer Guide — Command Station
- **Goal:** complete CS-side reference + porting.
- **Outline:** Intro (what it does / platform support) → DCC protocol concepts (encoding, packet format, **address types**, **instruction types**) → Project file structure → `dcc_user_config.h` in depth (**service-mode params table**) → Initialization (**`dcc_config_t`**, **`dcc_output_hw_t`**, **`dcc_railcom_hw_t`** tables, setup/main loop) → ISR architecture (58µs shared timer, RailCom one-shot, 100ms tick) → Implementing the drivers (**driver contract table**) → Bit encoder → Scheduler (**priority table**, duplicate combining, auto-refresh) → Service mode (direct/paged/register/address, ACK detection) → RailCom overview (**cutout-phase table**) → Callbacks (**result enum table**) → **Application API reference** (main-track + service-track tables) → Porting → Unit testing (**suite table**, build commands) → Troubleshooting.
- **Pull:** every bolded table from §3 (config struct, drivers, constants, API headers, defines, result enum, test list). Cross-check the **scheduler priority enum** and **service-mode result enum** against `dcc_types.h`.

### 5.4 Developer Guide — Decoder
- **Goal:** complete decoder-side reference + porting.
- **Outline:** Intro → Decoder concepts (signal reception, **bit classification table**, packet structure, **address-matching CV table**, **CV reference table**) → Project file structure → `dcc_user_config.h` → Initialization (**decoder `dcc_config_t` fields**, edge ring-buffer, setup/main loop) → ISR architecture (GPIO edge, 100ms tick) → Drivers (platform, **CV storage**, ACK pulse) → Bit decoder → Packet decoder (**state-machine table**) → CV storage & address matching (decoder lock) → Service mode (decoder side) → RailCom responses overview → Failsafe → Callbacks (**callback table with signatures**) → Porting → Unit testing → Troubleshooting.
- **Pull:** CV defaults and address logic from `dcc_cv_storage.*` and `dcc_defines.h`; decoder callbacks **with their real signatures** from `dcc_config.h` (note callbacks carry the `service_mode` flag); bit-decode threshold from `dcc_defines.h`; test list from `*_Test.cxx`.

### 5.5 Brochure
- **Goal:** one-page (≈3-page) sell sheet.
- **Outline:** What is DCC? → What is OpenDccCLib? → Key features → **Protocol coverage table** → Demo platforms / getting started → Architecture highlights → **Unit-test coverage stats** → Getting started steps → Documentation & repository.
- **Pull:** feature list from `ARCHITECTURE.md` + `ComplianceOverview.md` (only claim what's actually implemented); coverage stats and test-suite count **measured at build time**; module coverage table from `ARCHITECTURE.md` §6; version/license/URL from the repo.

---

## 6. Accuracy checklist — verify these every rebuild

The retired PDFs had baked-in errors. Confirm each against current source before
publishing:

1. **RailCom cutout timing.** Old text said the H-bridge tristates at "T_CS = 88µs."
   Per spec, T_CS is 26–32µs; 88µs is roughly T_TS1. The current code still uses an
   88µs delay (a known pending fix — see `ComplianceOverview.md`). State whatever
   `dcc_defines.h` actually defines **and** flag it as the in-progress retiming, or
   omit precise numbers until that lands. Don't reprint "T_CS = 88µs" as correct.
2. **Module names.** Old file trees referenced `dcc_packet_encoder.h/c`. The
   role-first module is `dcc_application_command_station_packet`. (Both currently
   exist — see `ComplianceOverview.md` on the pending old-module retirement.) Use the names in
   `ARCHITECTURE.md` §6.
3. **API table accuracy.** The old CS dev-guide "Main Track Operations" table mixed
   packet builders (`_load_*`, on `DccApplicationCommandStationPacket`) with
   main-track ops (`power_on/off`, `send_packet`, `add_to_auto_refresh`, …). Pull
   the real split from the two headers; don't conflate.
4. **Callback names & signatures.** Decoder callbacks use the `_command` suffix
   (e.g. `on_emergency_stop_command`, `on_binary_state_short_command`) and the CV
   callbacks take a `service_mode` flag. Copy signatures verbatim from
   `dcc_config.h`.
5. **Pin tables.** The QSGs, the decoder guide, and the loopback `README.md` did not
   agree on pin assignments. Treat the application project's SysConfig as
   authoritative and make all docs match it.
6. **Counts & stats.** "23 test suites / N source files / ~lines / coverage %" — only
   publish numbers you measured on the current tree at rebuild time.

---

*Companion docs: [ARCHITECTURE.md](ARCHITECTURE.md) (as-built design),
[ComplianceOverview.md](compliance/ComplianceOverview.md) (what's implemented vs pending), and Doxygen
(`Doxyfile`) for the API reference.*
