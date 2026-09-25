# OpenDccCLib

A portable C library for building NMRA DCC command stations and decoders on any processor — microcontrollers, desktop PCs, or anything in between.

DCC (Digital Command Control) is the NMRA standard for controlling model railroad locomotives and accessories: a command station sends addressed speed, function and programming commands over the rails as a modulated square wave, and decoders act on them. OpenDccCLib implements the protocol stack for both ends in plain C — no dynamic memory, no OS, no external dependencies — so it can run on anything with a C compiler and a timer.

## Features

- Command station and decoder in one library; compile-time role flags select what you need, and a booster or repeater can be both
- Packet builders for speed (14/28/128 step), functions F0–F68, basic and extended accessories, consist, binary state, analog, time and date, and operations-mode CV access, each carrying its repeat count
- Scheduler with priorities, duplicate combining, and two-tier paced auto-refresh with a starvation bound
- Full service mode: direct, paged, register and address programming, a mode-detect probe, and ACK detection with configurable thresholds
- RailCom: command-station cutout state machine and Channel 1/2 receive; decoder-side reply engine with bit-banged 4/8 transmit
- Decoder: bit decoding from edge timing, packet decoding with address matching for locomotive, consist (CV 19, CV 21/22) and accessory addresses, CV storage with decoder lock and factory reset, S-9.2.4 fail-safe
- No dynamic memory allocation — every buffer is statically sized at compile time
- No OS or RTOS required
- Dependency injection — the library reaches hardware only through function pointers you supply
- Proven on the wire: a Saleae hardware-in-the-loop bench checks the shipped firmware against the NMRA standards down to microsecond timing

## Platform-agnostic by design

The library core has no hardware dependencies. A command station needs one periodic timer and a GPIO per track output; a decoder needs one edge interrupt and a microsecond timestamp. The platform below is the one that ships with working example projects. Adapting the library to a new platform means rewriting a small set of driver callbacks — the protocol code is unchanged.

## Platforms with working examples

| Platform | Role | IDE / Toolchain |
|---|---|---|
| TI MSPM0G3507 LaunchPad | Command station | Code Composer Studio Theia |
| TI MSPM0G3507 LaunchPad | Decoder | Code Composer Studio Theia |

## Getting started

The fastest path is the command-station example. Import the project into Code Composer Studio, build, flash, open a 230400-baud terminal and type `POWER ON`.

- [Quick Start Guide — Command Station](documentation/QuickStartGuide_CommandStation.pdf) and [Quick Start Guide — Decoder](documentation/QuickStartGuide_Decoder.pdf): step-by-step LaunchPad walkthroughs.
- [Developer Guide — Command Station](documentation/DeveloperGuide_CommandStation.pdf) and [Developer Guide — Decoder](documentation/DeveloperGuide_Decoder.pdf): the config struct, every module, porting, and testing.
- [Architecture](documentation/ARCHITECTURE.md) (also as [PDF](documentation/ARCHITECTURE.pdf)): the as-built design — modules, feature flags, dependency injection, execution contexts.
- [Compliance](documentation/compliance/ComplianceOverview.md): per-feature status against the NMRA S-9.x standards, with the machine-checked [dashboard](https://jimkueneman.github.io/OpenDccCLib/documentation/compliance/index.html) behind it.
- API reference: [https://jimkueneman.github.io/OpenDccCLib/documentation/help/html/](https://jimkueneman.github.io/OpenDccCLib/documentation/help/html/) — Doxygen, generated from the headers. In a clone, open `documentation/help/html/index.html`.
- [Brochure](documentation/OpenDccCLib_Brochure.pdf): a one-page overview.

## Repository layout

```
src/                              main library source
  dcc/                            the DCC protocol engine, one module per concern:
                                  bit encoder and scheduler, packet builders, service mode,
                                  RailCom cutout/receive/transmit, bit and packet decoders,
                                  CV storage, fail-safe, and the dcc_config wiring module
  test/                           shared GoogleTest main used by every unit test
  mainpage.h                      Doxygen main page

applications/                     ready-to-run example projects
  ti_theia/mspm03507_launchpad/
    command_station/              command station with a UART command line
    decoder/                      decoder that logs every decoded command over UART
    loopback_test/                two-board loopback test

test/                             unit, gate and compliance tests
  CMakeLists.txt, Makefile        GoogleTest build for src/dcc (see test/README.md)
  user_config/                    per-role configurations compiled on every run
  compliance/                     Saleae hardware-in-the-loop bench
    command_station/              S-9.1, S-9.2, S-9.2.1, S-9.2.3, S-9.3.2 and scheduler suites
    mobile_decoder/               decoder-side suites and the bench firmware
    accessory_decoder/            bench setup notes
    saleae_hil_waveform_player/   waveform playback firmware for decoder tests

templates/                        user config template for new projects
  typical/                        dcc_user_config.h

documentation/                    guides (PDF sources in guides/), Doxygen configuration and
                                  output (help/), compliance database and dashboard,
                                  NMRA specification notes and PDFs (specs/), style guides
```

## Building and running the tests

```
cd test && make
```

Each test executable runs as part of the build, serially, and gcovr writes a coverage report under `test/build/gcovr/`. The build also compiles the library under every single-role configuration in `test/user_config/`, so a missing feature-flag guard fails as a compile or link error. See [test/README.md](test/README.md) for the prerequisites.

The hardware-in-the-loop suites need the bench described in `test/compliance/command_station/HIL_SETUP.md`.

## License

BSD 2-Clause. See individual source file headers for the full license text.

## Author

Jim Kueneman
