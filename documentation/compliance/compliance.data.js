// OpenDccCLib — NMRA DCC compliance database.
// Single source of truth. index.html renders it; validate_compliance.py checks it
// against the code. Payload below is strict JSON so both consumers can parse it —
// edit values, keep it valid JSON (double quotes, no trailing commas).
//
// Per-feature shape:
//   tid        stable test id, frozen once assigned (DCC-<spec>-<role>-<NNN>)
//   role       cs | dec | acc | hw
//   ref        { spec, cite, origin ("released"|"draft"), draftDelta {change}|null }
//              origin = which revision we implemented to; draftDelta = how a draft
//              revision changes a released requirement. The revision DATE is not stored
//              per-feature -- it is looked up once in meta.revisions[spec][origin].
//   supported/gtest/hil  { state, note }   state drives the badge
//   detail     { impl, gtest, hil }        prose fallback (used when refs.* is empty)
//   refs       { symbols:[str], tests:[{name,file,desc}], hilChecks:[{label,file,desc}] }
//              symbols are grep-checked against src/dcc by the validator; tests/hilChecks
//              render as drop-downs and are the targets of @compliance <tid> tags.
// meta.revisions { "<spec>": {released, draft} }  single home for revision dates.
window.COMPLIANCE =
{
  "meta": {
    "title": "OpenDccCLib — NMRA DCC compliance",
    "sourceCommit": "c1c6b71",
    "validated": "2026-06-26",
    "roles": {
      "cs": "Command station",
      "dec": "Mobile decoder",
      "acc": "Accessory decoder",
      "hw": "Hardware / app driver"
    },
    "roleColor": {
      "cs": "#185fa5",
      "dec": "#0f6e56",
      "acc": "#854f0b",
      "hw": "#5f5e5a"
    },
    "hilRig": {
      "cs": "CS HIL",
      "dec": "Decoder HIL",
      "acc": "Accessory HIL",
      "hw": "—"
    },
    "legend": {
      "supported": {
        "ok": "implemented",
        "partial": "partial",
        "no": "not implemented",
        "na": "out of library scope"
      },
      "test": {
        "ok": "covered",
        "partial": "partial",
        "no": "none",
        "planned": "planned (rig not built)",
        "na": "n/a"
      }
    },
    "revisions": {
      "S-9.1": {
        "released": "2025-04-30",
        "draft": null
      },
      "S-9.2": {
        "released": "2004-07",
        "draft": null
      },
      "S-9.2.1": {
        "released": "2025-01-24",
        "draft": "2025-11-15"
      },
      "S-9.2.1.1": {
        "released": "2022-05-15",
        "draft": "2024-06-23"
      },
      "S-9.2.2": {
        "released": "2012-07",
        "draft": "2026-02-11"
      },
      "S-9.2.3": {
        "released": "2012-07",
        "draft": null
      },
      "S-9.2.4": {
        "released": "2025-07-03",
        "draft": null
      },
      "S-9.3.2": {
        "released": "2012-07",
        "draft": "2026-04-10"
      },
      "Library": {
        "released": null,
        "draft": null
      },
      "S-9.1.1.6": {
        "released": null,
        "draft": "2026-02-15"
      },
      "S-9.4.x": {
        "released": null,
        "draft": "2026-01-29"
      },
      "S-9": {
        "released": "2025-01",
        "draft": null
      },
      "S-9.1.1": {
        "released": "2021-01-31",
        "draft": null
      },
      "S-9.1.1.1": {
        "released": "2020-11-13",
        "draft": null
      },
      "S-9.1.1.2": {
        "released": "2020-11-13",
        "draft": null
      },
      "S-9.1.1.3": {
        "released": "2024-05-06",
        "draft": null
      },
      "S-9.1.1.4": {
        "released": "2021-07-07",
        "draft": null
      },
      "S-9.1.1.5": {
        "released": "2020-12-06",
        "draft": null
      },
      "S-9.1.2": {
        "released": "2021-06-09",
        "draft": null
      },
      "S-9.1.3": {
        "released": null,
        "draft": "2026-05-05"
      }
    }
  },
  "specs": [
    {
      "id": "s9",
      "spec": "S-9",
      "title": "Electrical (general)",
      "features": [
        {
          "tid": "DCC-S9-HW-001",
          "feature": "Peak voltage limits by scale",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9",
            "cite": "Section 4.2, Table 4.2",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Components must withstand peak voltage by scale: HO and larger <=27V, HOn3 to N <=24V, smaller than N <=12V (DC or DCC); AC motors operate at 27 VAC max. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9-HW-002",
          "feature": "Power delivery and superimposed-signal rules",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9",
            "cite": "Section 4 (Power)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Full-throttle voltage at rails/motor must supply sufficient current at max anticipated load; high-frequency superimposed voltage must not interfere with operation; harmonic-rich sources must not exceed rated motor current. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9-HW-003",
          "feature": "Coupler and wheelset insulation",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9",
            "cite": "Sections 6-7",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Metallic couplers (powered and non-powered) must be insulated from the rails; wheelsets must be insulated to prevent undue rail-to-rail conductance. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_1",
      "spec": "S-9.1",
      "title": "Electrical / signal",
      "features": [
        {
          "tid": "DCC-S9.1-CS-001",
          "feature": "One-bit half-period 58 µs (55–61)",
          "role": "cs",
          "ref": {
            "spec": "S-9.1",
            "cite": "Section 2, Table 2.1 (bit timing)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "Emits 58 µs half-bits for a one-bit.",
            "gtest": "",
            "hil": "55–61 µs on wire."
          },
          "refs": {
            "symbols": [
              "DCC_ONE_BIT_HALF_PERIOD_US"
            ],
            "tests": [
              {
                "name": "DccDefines.bit_timing_constants",
                "file": "dcc_config_Test.cxx",
                "desc": "Asserts the constant DCC_ONE_BIT_HALF_PERIOD_US == 58 (with the zero-bit and max-total constants)."
              },
              {
                "name": "DccBitEncoder.tick_preamble_is_all_one_bits",
                "file": "dcc_bit_encoder_Test.cxx",
                "desc": "Ticks the encoder through the preamble and checks every emitted bit is a one-bit (58 µs halves)."
              }
            ],
            "hilChecks": [
              {
                "label": "ONE half-bit 55-61 us",
                "file": "s9_1_compliance.py",
                "desc": "Measures every captured one-bit half-period on the wire is within 55–61 µs (nominal 58)."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.1-CS-002",
          "feature": "Zero-bit half ≥95 µs (nom 100); total ≤12000 µs",
          "role": "cs",
          "ref": {
            "spec": "S-9.1",
            "cite": "Section 2, Table 2.1 (bit timing)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "DCC_ZERO_BIT_HALF_PERIOD_US=100, DCC_ZERO_BIT_MAX_TOTAL_DURATION_US=12000.",
            "gtest": "",
            "hil": "≥95, ≤12000 µs."
          },
          "refs": {
            "symbols": [
              "DCC_ZERO_BIT_HALF_PERIOD_US",
              "DCC_ZERO_BIT_MAX_TOTAL_DURATION_US"
            ],
            "tests": [
              {
                "name": "DccDefines.bit_timing_constants",
                "file": "dcc_config_Test.cxx",
                "desc": "Asserts DCC_ZERO_BIT_HALF_PERIOD_US == 100 and DCC_ZERO_BIT_MAX_TOTAL_DURATION_US == 12000."
              },
              {
                "name": "DccBitEncoder.tick_zero_bit_takes_four_ticks",
                "file": "dcc_bit_encoder_Test.cxx",
                "desc": "Verifies a zero-bit spans four 58 µs half-ticks — the stretched-zero encoding."
              }
            ],
            "hilChecks": [
              {
                "label": "ZERO half-bit >= 95 us",
                "file": "s9_1_compliance.py",
                "desc": "Measures every captured zero-bit half-period is ≥95 µs."
              },
              {
                "label": "ZERO total bit <= 12000 us",
                "file": "s9_1_compliance.py",
                "desc": "Sums each zero-bit's two halves and checks the total ≤12000 µs (stretched-zero limit)."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.1-CS-003",
          "feature": "Preamble ≥14 (transmit)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section 5 (preamble; CS sends ≥14)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "CS sends 16 preamble bits (DCC_PREAMBLE_BITS_OPS=16) — ≥14 spec min, +margin for the RailCom cutout. Sends 16 (+margin for cutout).",
            "gtest": "",
            "hil": "≥14 ones."
          },
          "refs": {
            "symbols": [
              "DCC_PREAMBLE_BITS_OPS"
            ],
            "tests": [
              {
                "name": "DccDefines.preamble_constants",
                "file": "dcc_config_Test.cxx",
                "desc": "Asserts DCC_PREAMBLE_BITS_OPS == 16 (and service == 20, decoder-min == 10)."
              },
              {
                "name": "DccBitEncoder.tick_preamble_is_all_one_bits",
                "file": "dcc_bit_encoder_Test.cxx",
                "desc": "Confirms the encoder emits a run of one-bits for the preamble."
              }
            ],
            "hilChecks": [
              {
                "label": "preamble >= 14 ones (CS transmit)",
                "file": "s9_1_compliance.py",
                "desc": "Counts the preamble run per packet and checks the minimum is ≥14 ones (idle-inflated first packet excluded)."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.1-CS-004",
          "feature": "Intra-bit half symmetry ≤3 µs",
          "role": "cs",
          "ref": {
            "spec": "S-9.1",
            "cite": "Section 2 (half-bit symmetry)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "nobs",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "Symmetric by construction — the same timer period drives both halves.",
            "gtest": "No dedicated host assertion; it is a wire-timing property. Intra-bit symmetry is a wire-timing property.",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": [
              {
                "label": "intra-bit symmetry <= 3 us",
                "file": "s9_1_compliance.py",
                "desc": "Checks each bit's two captured halves differ by ≤3 µs."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.1-HW-001",
          "feature": "Signal voltage / levels",
          "role": "hw",
          "ref": {
            "spec": "S-9.1",
            "cite": "Section 2 (track signal voltage)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Out of library scope — set by the application's H-bridge driver hardware. App H-bridge driver.",
            "gtest": "Not a library concern.",
            "hil": "Needs analog capture."
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": [
              {
                "label": "signal voltage levels (>=8.5 V out)",
                "file": "s9_1_compliance.py",
                "desc": "Declared n/a (rep.na) — not measurable on a digital logic analyzer; needs a scope/meter."
              }
            ]
          },
          "verify": "hw"
        },
        {
          "tid": "DCC-S9.1-DEC-001",
          "feature": "One-bit 55–61 µs accept (full range)",
          "role": "dec",
          "ref": {
            "spec": "S-9.1",
            "cite": "Section 2, Table 2.1 (decoder timing acceptance)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Bit decoder accepts the full one-bit window (dcc_bit_decoder.c edge timing).",
            "gtest": "",
            "hil": "Decoder HIL: source DCC at 55 / 58 / 61 µs into the decoder DUT, confirm decode. Rig not built. Needs decoder rig."
          },
          "refs": {
            "symbols": [],
            "tests": [
              {
                "name": "DccBitDecoder.too_long_period_resets",
                "file": "dcc_bit_decoder_Test.cxx",
                "desc": "Feeds an over-long half-period and confirms the decoder resets rather than mis-decoding (timing upper bound)."
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1-DEC-002",
          "feature": "Preamble ≥10 (accept)",
          "role": "dec",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section 5 (preamble; decoder accepts ≥10)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "DCC_PREAMBLE_BITS_DECODER_MIN=10.",
            "gtest": "",
            "hil": "Decoder HIL: feed 9 vs 10 preamble bits, confirm reject / accept. Rig not built. Needs decoder rig."
          },
          "refs": {
            "symbols": [
              "DCC_PREAMBLE_BITS_DECODER_MIN"
            ],
            "tests": [
              {
                "name": "DccBitDecoder.preamble_exactly_10_produces_packet",
                "file": "dcc_bit_decoder_Test.cxx",
                "desc": "Feeds exactly 10 preamble ones and confirms the decoder accepts and produces a packet."
              },
              {
                "name": "DccBitDecoder.preamble_too_short_no_packet",
                "file": "dcc_bit_decoder_Test.cxx",
                "desc": "Feeds 9 preamble ones and confirms no packet is produced (below the ≥10 minimum)."
              },
              {
                "name": "DccDefines.preamble_constants",
                "file": "dcc_config_Test.cxx",
                "desc": "Pins the decoder minimum-preamble constant (DCC_PREAMBLE_BITS_DECODER_MIN == 10)."
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1-DEC-003",
          "feature": "Asymmetric / stretched zero accept",
          "role": "dec",
          "ref": {
            "spec": "S-9.1",
            "cite": "Section 2 (stretched-zero acceptance)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Accepts stretched / asymmetric zero halves (DCC_ZERO_BIT_HALF_PERIOD_DECODER_MIN_US=90).",
            "gtest": "",
            "hil": "Decoder HIL: source stretched-zero waveforms, confirm decode. Rig not built. Needs decoder rig."
          },
          "refs": {
            "symbols": [
              "DCC_ZERO_BIT_HALF_PERIOD_DECODER_MIN_US"
            ],
            "tests": [
              {
                "name": "DccBitDecoder.asymmetric_zero_accepted",
                "file": "dcc_bit_decoder_Test.cxx",
                "desc": "Feeds a zero-bit with asymmetric / stretched halves and confirms the decoder still accepts it."
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1-ACC-001",
          "feature": "Bit-decode front-end (shared with mobile decoder)",
          "role": "acc",
          "ref": {
            "spec": "S-9.1",
            "cite": "Section 2 (shared decode front-end)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Accessory decoders reuse the dcc_bit_decoder front-end; S-9.1 behaviour is identical to the mobile decoder. Shared.",
            "gtest": "Shared.",
            "hil": "Accessory HIL rig not built. Accessory-specific compliance (packets, SRQ / RailCom) begins at S-9.2.1."
          },
          "refs": {
            "symbols": [],
            "tests": [
              {
                "name": "DccBitDecoder.asymmetric_zero_accepted",
                "file": "dcc_bit_decoder_Test.cxx",
                "desc": "Shared front-end test — same decoder path serves accessory decoders."
              }
            ],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_1_1",
      "spec": "S-9.1.1",
      "title": "Decoder interfaces",
      "features": [
        {
          "tid": "DCC-S9.1.1-HW-001",
          "feature": "Standard wire color code",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1",
            "cite": "Section 3.1, Table 3.1",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Wire colors: RED=right rail, BLACK=left rail, ORANGE=motor(+), GRAY=motor(-), WHITE=front light, YELLOW=rear light, GREEN=Aux1, BROWN/VIOLET=Aux2, BLUE=common(+), BLACK-WHITE stripe=common(-)/ground; outputs Aux3-6 have no assigned color. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1-HW-002",
          "feature": "Direction convention and per-pin ratings",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1",
            "cite": "Sections 3.1, 1.1",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Motor+/(orange) and motor-/(gray) wired so the loco moves forward on a forward packet with CV29 bit0=0; table power ratings are per individual pin and do not account for total loco draw. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1-HW-003",
          "feature": "Conformance and dummy plug",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1",
            "cite": "Sections 1.4, 1.1",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "All stated mechanical/electrical values must be met; unimplemented connections remain unconnected; when no decoder is installed a dummy plug must allow DC operation and room must remain to fit a decoder. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_1_1_1",
      "spec": "S-9.1.1.1",
      "title": "Six and eight pin interface",
      "features": [
        {
          "tid": "DCC-S9.1.1.1-HW-001",
          "feature": "Mechanical (Small 6-pin / Medium 8-pin)",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.1",
            "cite": "Section 2, Table 2.1",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Small: 6 pins 1x6, 0.050 inch pitch, N scale or larger. Medium: 8 pins 2x4, 0.100 inch pitch, HO or larger. Female part in vehicle; pin 1 identified; Small/Medium pictogram marking required. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.1-HW-002",
          "feature": "Electrical ratings",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.1",
            "cite": "Table 2.1",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Small (6-pin): 0.50A continuous / 0.75A peak per pin. Medium (8-pin): 1.50A continuous / 3.00A peak per pin. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.1-HW-003",
          "feature": "Pinout",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.1",
            "cite": "Section 3.1, Tables 3.1/3.2",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "6-pin: 1 Motor+, 2 Motor-, 3 Right Rail, 4 Left Rail, 5 Front Light, 6 Rear Light. 8-pin: 1 Motor+, 2 Rear Light, 3 Aux1, 4 Left Rail, 5 Motor-, 6 Front Light, 7 Common V+, 8 Right Rail. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.1-HW-004",
          "feature": "Wiring rules / scope",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.1",
            "cite": "Sections 3.1, 1.1",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "No motor-to-rail/frame connection and no pin3-to-pin7 connection on the loco side; Aux1 must be diode-protected if polarity-sensitive; not recommended for new designs, non-sound decoders only. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_1_1_2",
      "spec": "S-9.1.1.2",
      "title": "JST-9 interface",
      "features": [
        {
          "tid": "DCC-S9.1.1.2-HW-001",
          "feature": "Mechanical",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.2",
            "cite": "Section 2",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "9-pin single-row connector, approximately 1.50 mm pitch and 15.00 mm length, pin 1 identified. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.2-HW-002",
          "feature": "Electrical rating",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.2",
            "cite": "Section 3.1",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Rated 50 VDC and 1.0 A maximum on each pin. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.2-HW-003",
          "feature": "Pinout",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.2",
            "cite": "Section 3.1, Table 3.1",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "1 Aux1/green, 2 Right Rail/red, 3 Motor+/orange, 4 Common V+/blue, 5 Front Light/white, 6 Rear Light/yellow, 7 Motor-/gray, 8 Left Rail/black, 9 Aux2/violet or brown. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.2-HW-004",
          "feature": "Wiring and conformance",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.2",
            "cite": "Sections 3.2, 1.4",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Because field installation is the most frequent use, the wire harness must follow the S-9.1.1 color code; all stated values must be met and unimplemented pins remain unconnected. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_1_1_3",
      "spec": "S-9.1.1.3",
      "title": "21MTC interface",
      "features": [
        {
          "tid": "DCC-S9.1.1.3-HW-001",
          "feature": "Mechanical",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.3",
            "cite": "Section 2",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "22-pin two-row strip on 1.27 mm grid with pin 11 omitted for orientation keying; gold-plated contacts 1A max; decoder max 30 x 15.5 x 6.5 mm; Compact and Rotated install variants; 21MTC logo marking required; direct plug-in (no cable). Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.3-HW-002",
          "feature": "Pinout",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.3",
            "cite": "Section 3.1, Table 2",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Motor (18/19), Track (21/22), GND (20), V+ (16), F0r/F0f (7/8), AUX1-8, speaker LS A/B (9/10), train bus ZBCLK/ZBDTA (5/6), sensor inputs (1/2), Vcc (12); pin 11 = orientation index. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.3-HW-003",
          "feature": "Electrical",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.3",
            "cite": "Section 3.1.2, Table 3",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Power outputs open-drain to GND with load from V+, 100mA; logic-level outputs TTL/LVTTL max 0.5mA (off <=0.4V / on >=2.4V); speaker impedance 4-8 ohm; train-bus pins via max 470 ohm series resistor; per-output current manufacturer-specified. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.3-HW-004",
          "feature": "Operation without decoder",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.3",
            "cite": "Section 4",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "A jumper plug must connect Track1->Motor1 and Track2->Motor2 (and F0f/F0r to track for lighting); V+ supplied via two diodes from the track connections. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_1_1_4",
      "spec": "S-9.1.1.4",
      "title": "PluX interface",
      "features": [
        {
          "tid": "DCC-S9.1.1.4-HW-001",
          "feature": "Mechanical",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.4",
            "cite": "Section 2",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "16- or 22-pin double-row sockets on 1.27 mm grid with pin 11 omitted for keying; gold-plated contacts 1A max; PluX22 decoder max 30x16x6mm, PluX16 uses pins 3-18 only; Compact and Tall vehicle variants; direct plug-in. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.4-HW-002",
          "feature": "Pinout",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.4",
            "cite": "Section 3.1, Table 2",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "PluX22: GPIO/C (1), Aux3 (2), train bus clk/data (3/4), GND (5), Cap+ (6), F0f (7), Motor1 (8), V+ (9), Motor2 (10), index (11), Track1 (12), F0r (13), Track2 (14), speaker A/B (15/17), AUX1/AUX2/AUX4-7. PluX16 omits 1,2 and 19-22. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.4-HW-003",
          "feature": "Electrical",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.4",
            "cite": "Section 3.2",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "GPIO TTL-compatible (input ~100k, output 0.5mA max); train-bus pins via max 470 ohm series; speaker 4-8 ohm; Cap+ switchable carrying at most track voltage to V+; V+ supplied via two diodes. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.4-HW-004",
          "feature": "Operation without decoder / SUSI use",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.4",
            "cite": "Sections 4, 5",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Jumper plug connects Track1->Motor1 and Track2->Motor2; as a SUSI interface only four signals are used (GND, V+, train bus clock, train bus data) with track connections unused. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_1_1_5",
      "spec": "S-9.1.1.5",
      "title": "Next18 / Next18-S interface",
      "features": [
        {
          "tid": "DCC-S9.1.1.5-HW-001",
          "feature": "Mechanical",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.5",
            "cite": "Section 2",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "18-pin connector, 0.50 mm pitch, symmetric layout; Next18 install volume 15x9.5x2.9mm, Next18-S (with sound) 25x9.5x4.1mm; encapsulated socket in vehicle, plug on decoder. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.5-HW-002",
          "feature": "Electrical",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.5",
            "cite": "Sections 2.3, 3",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "0.5A per contact (1.0A on doubled track/V+/GND contacts); insulation 1000 Mohm at 500VDC; contact resistance 50 mohm; open-drain outputs 100mA; logic TTL 0.5mA; speaker 4-8 ohm; train bus 470 ohm series + >=15k pull-up. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.5-HW-003",
          "feature": "Pinout",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.5",
            "cite": "Sections 3.1/3.2",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Next18: Track (1/9/10/18), Motor+/- (2/11), GND (5/14), V+ (6/15), F0F/F0R (8/17), AUX1-6, train bus AUX3/AUX4 (4/13). Next18-S replaces AUX5/AUX6 (pins 16/7) with Speaker A/B. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.5-HW-004",
          "feature": "Operation without decoder / SUSI use",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.5",
            "cite": "Sections 3.5, 3.6",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "A dummy/bridge plug connects track to motor and direction lights; as a SUSI interface only four signals are used (GND, V+, train bus clock pin 4, train bus data pin 13). Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_1_1_6",
      "spec": "S-9.1.1.6",
      "title": "E24 decoder interface",
      "features": [
        {
          "tid": "DCC-S9.1.1.6-HW-001",
          "feature": "Mechanical (28-pin, unkeyed)",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.6",
            "cite": "Section 2 (Draft B)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "28-pin connector (24 contacts + 4 guide pins for pickup), Molex 505270/505070, 0.35 mm pitch; decoder max 19.5 x 8.4 x 2.6 mm. The interface is symmetric and NOT keyed, so the vehicle must add external features to prevent wrong-orientation insertion. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.6-HW-002",
          "feature": "Electrical",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.6",
            "cite": "Section 3, Table 2 (Draft B)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Max 50V AC/DC; 0.3A per pin and 3.0A per guide pin; motor and GND doubled (0.6A); function outputs open-drain to GND 100mA; logic-level outputs TTL/LVTTL 0.5mA; speaker 4-8 ohm; insulation 100 Mohm at 250VDC. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.6-HW-003",
          "feature": "Pinout",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.6",
            "cite": "Section 3.1, Table 3 (Draft B)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Track power on the guide pins (left X1A/B, right X2A/B); Motor (4/5/6/7), GND (3/22), V+ (17), Cap+ (8), F0f/F0r (9/10), AUX1-AUX12, speaker LS_A/LS_B (2/1), Vcc (18), train bus TBCLK/TBDATA (23/24). Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.1.6-HW-004",
          "feature": "Requirements and use cases",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.1.6",
            "cite": "Sections 3.3, 3.4 (Draft B)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Minimum function outputs F0_f, F0_r, AUX_1, AUX_2; a jumper plug enables operation without a decoder; the interface may be used function-only (decoder generates the service-mode ACK internally) and as a SUSI interface using four signals (GND, V+, TBCLK, TBDATA). Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_1_2",
      "spec": "S-9.1.2",
      "title": "DCC Power Station Interface",
      "features": [
        {
          "tid": "DCC-S9.1.2-HW-001",
          "feature": "Full Scale signal levels",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.2",
            "cite": "Sections 2.2, Tables 3-4",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Full Scale option: command station differential output >= +/-8.5V into 100 ohm and <= +/-24V open circuit (current limited <=1A, short-protected); power station accepts >= +/-7V, withstands +/-27V, rejects < +/-1.5V. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.2-HW-002",
          "feature": "Driver/Receiver (RS-422/485) levels",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.2",
            "cite": "Section 2.3, Tables 5-8",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Driver/Receiver option uses TIA/EIA-422 (>= +/-2V into 100 ohm) or TIA/EIA-485 (>= +/-1.5V into 60 ohm) differential signalling with defined current limits and receiver input thresholds (>= +/-200mV). Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.2-HW-003",
          "feature": "Distortion limits",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.2",
            "cite": "Sections 2.1.2-2.1.3, Tables 1-2",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Apart from producing the S-9.3.2 cutout, a power station must not distort the signal beyond Ton/Toff <=5us and |Toff-Ton| <=2us; a repeater (only one per segment) must stay within 1us and 0.5us. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.2-HW-004",
          "feature": "Fail-safe and non-floating output",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.2",
            "cite": "Sections 2.1.5, 2.1.1",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "A power station must disable its output if no valid DCC packet is received for >30ms (configurable for multiprotocol use); when unpowered or disabled, the two outputs must not float relative to each other. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.2-HW-005",
          "feature": "Physical medium and topology",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.2",
            "cite": "Sections 3, 4",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "A single >=26 AWG pair (twisted/coax) with no special termination (Driver/Receiver adds a reference ground); only one signal source per interface, connected as tree/star/daisy-chain, never in a loop. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_1_3",
      "spec": "S-9.1.3",
      "title": "Decoder Power Modes (draft)",
      "features": [
        {
          "tid": "DCC-S9.1.3-HW-001",
          "feature": "Three power modes",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.3",
            "cite": "Section 2.1",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Defines Standard (normal operation, outputs enabled), Low power (minimum draw, all outputs off except ACK, energy storage disabled, < 250mA per S-9.2.3 so the ACK is recognizable), and High power (full draw including energy-storage charging). The same modes apply to Train Bus modules. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.3-HW-002",
          "feature": "Mode transitions",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.3",
            "cite": "Sections 2.2.1-2.2.3",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Default is Standard or Low (per config); a decoder switches to Low power immediately on receiving two or more reset packets (disabling energy storage and informing Train Bus modules), and may switch to High power on a packet addressed to it or a broadcast. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.1.3-HW-003",
          "feature": "Inrush and mode validity",
          "role": "hw",
          "verify": "hw",
          "ref": {
            "spec": "S-9.1.3",
            "cite": "Sections 2.2.3-2.2.4",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "An accessory decoder with significant draw must limit inrush via a random delay before entering High power; a power mode stays valid until the next switching event, and for decoders with energy storage it persists through short signal interruptions covered by the storage device. Hardware standard.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_2",
      "spec": "S-9.2",
      "title": "Packet format (baseline)",
      "features": [
        {
          "tid": "DCC-S9.2-CS-001",
          "feature": "Emit idle packet FF 00 FF",
          "role": "cs",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section C (Idle Packet)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "DccApplicationCommandStationPacket_load_idle fills data[0..2]=0xFF,0x00,0xFF.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_idle"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.idle_packet_bytes",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "Asserts data bytes are FF 00 FF, byte_count 3"
              }
            ],
            "hilChecks": [
              {
                "label": "idle packet = FF 00 FF",
                "file": "s9_2_compliance.py",
                "desc": "Confirms a decoded packet equals FF 00 FF on the wire"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2-DEC-001",
          "feature": "Ignore idle packet (no callback)",
          "role": "dec",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section C (Idle Packet)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "DccPacketDecoder_process_packet returns early when data[0]==0xFF.",
            "gtest": "",
            "hil": "Requires a decoder-side HIL rig that does not exist yet. Decoder HIL rig not built."
          },
          "refs": {
            "symbols": [
              "DccPacketDecoder_process_packet"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.idle_packet_ignored",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "Asserts idle packet fires no decoder callbacks"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2-CS-002",
          "feature": "Emit reset packet 00 00 00",
          "role": "cs",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section C (Reset Packet)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "DccApplicationCommandStationPacket_load_reset writes all three bytes to 0x00.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_reset",
              "DCC_RESET_BYTE"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.reset_packet_bytes",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "Asserts data bytes are 00 00 00"
              }
            ],
            "hilChecks": [
              {
                "label": "reset packet = 00 00 00",
                "file": "s9_2_compliance.py",
                "desc": "Driven RESET yields 00 00 00 on the wire"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2-DEC-002",
          "feature": "Accept reset packet",
          "role": "dec",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section C (Reset Packet)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "process_packet detects reset and returns without dispatching an instruction.",
            "gtest": "",
            "hil": "Requires a decoder-side HIL rig that does not exist yet. Decoder HIL rig not built."
          },
          "refs": {
            "symbols": [
              "DccPacketDecoder_process_packet"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.broadcast_reset_ignored",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "Asserts reset packet fires no operating callbacks"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2-CS-003",
          "feature": "Emit broadcast emergency stop (S=1)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section B (broadcast stop)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_estop_all with panic emits 00 51 51.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_estop_all"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.estop_all_packet",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "Asserts panic estop emits broadcast with S=1"
              }
            ],
            "hilChecks": [
              {
                "label": "broadcast stop = baseline 01DC000S (emergency S=1)",
                "file": "s9_2_compliance.py",
                "desc": "Driven ESTOP yields 00 51 51"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2-CS-004",
          "feature": "Emit broadcast controlled stop (S=0)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section B (broadcast stop)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_estop_all without panic emits 00 50 50 (S=0).",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_estop_all"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.estop_all_controlled",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "Asserts non-panic stop emits S=0"
              }
            ],
            "hilChecks": [
              {
                "label": "broadcast controlled stop = baseline 01DC000S (S=0)",
                "file": "s9_2_compliance.py",
                "desc": "Driven STOP yields 00 50 50"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2-DEC-003",
          "feature": "Accept broadcast stop / e-stop",
          "role": "dec",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section B (broadcast stop)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "process_packet maps broadcast e-stop to on_emergency_stop_command.",
            "gtest": "",
            "hil": "Requires a decoder-side HIL rig that does not exist yet. Decoder HIL rig not built."
          },
          "refs": {
            "symbols": [
              "DccPacketDecoder_process_packet"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.broadcast_packet_accepted_by_any_short_address",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "Asserts a broadcast packet is accepted regardless of short address"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2-CS-005",
          "feature": "Packet framing + preamble",
          "role": "cs",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section A (preamble, start/end bits)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "Every load_* sets preamble_bits; bit/start/end framing applied by the bit encoder.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DCC_PREAMBLE_BITS_OPS"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.idle_packet_preamble",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "Asserts preamble_bits set to ops preamble length"
              }
            ],
            "hilChecks": [
              {
                "label": "packet structure (addr + data* + error byte)",
                "file": "s9_2_compliance.py",
                "desc": "Decoded packets have address + data + error byte structure"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2-CS-006",
          "feature": "Append XOR error byte",
          "role": "cs",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section B (error detection byte)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "_append_xor XORs all data bytes and appends the result.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_idle"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.idle_packet_xor_valid",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "Asserts error byte is the XOR of preceding bytes"
              }
            ],
            "hilChecks": [
              {
                "label": "error-detection byte = XOR of preceding bytes",
                "file": "s9_2_compliance.py",
                "desc": "Every decoded packet XORs to zero"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2-DEC-004",
          "feature": "Validate XOR; reject corrupt packets",
          "role": "dec",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section B (error detection byte)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "_validate_xor recomputes XOR; process_packet drops on mismatch.",
            "gtest": "",
            "hil": "Requires a decoder-side HIL rig that does not exist yet. Decoder HIL rig not built."
          },
          "refs": {
            "symbols": [
              "DccPacketDecoder_process_packet"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.bad_xor_ignored",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "Asserts a wrong error byte fires no callbacks"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2-DEC-005",
          "feature": "Address ranges and matching (short/long/broadcast)",
          "role": "dec",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section B (address partitions)",
            "origin": "released",
            "draftDelta": {
              "change": "The released S-9.2 reserves only addresses 0 (broadcast), 254 (11111110) and 255 (11111111, idle), treating the whole 232-254 range as undifferentiated 'Reserved for Future Use'. The draft (section 2.1) narrows that reserved band to 232-252 and carves out 253-254 as a dedicated partition for 'Advanced Extended Packet Formats', delegating their meaning to S-9.2.1.1 (Logon auto-registration / Data Space). This is a real allocation change, not just notation: address-matching logic must route 253/254 to the Logon/Data-Space handler (also the RailCom service/programming addresses) rather than treating them as generic reserved, leaving 255 as idle and 232-252 still reserved."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "process_packet parses short/long/reserved and dispatches only on match/broadcast.",
            "gtest": "",
            "hil": "Requires a decoder-side HIL rig that does not exist yet. Decoder HIL rig not built."
          },
          "refs": {
            "symbols": [
              "DccPacketDecoder_process_packet"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.matching_address_dispatched",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "Asserts a matching-address packet is dispatched"
              },
              {
                "name": "DccPacketDecoder.wrong_address_ignored",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "Asserts a non-matching short address is ignored"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2-CS-007",
          "feature": "Idle keep-alive (<=30 ms packet floor)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section C (Frequency of Packet Transmission)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "The scheduler loads the next packet the instant the encoder goes idle -- one-shot, then refresh round-robin, then an idle packet if nothing is queued (DccScheduler_run, dcc_scheduler.c). Transmission is therefore continuous back-to-back with no gaps, so the <=30 ms start-to-start floor is always met (and exceeded ~3-5x). The anti-fallback intent (keep auto-conversion decoders in digital mode) is satisfied as long as the app drives the run loop. Idle-fill, no gaps.",
            "gtest": "DccScheduler.run_sends_idle_when_empty proves the scheduler loads an idle packet when nothing is queued, so the rail never goes empty; the wall-clock 30 ms timing itself is verified on the wire (HIL).",
            "hil": "Measures the max packet start-to-start gap stays <= 30 ms on the wire. Wire-verified."
          },
          "refs": {
            "symbols": [
              "DccScheduler_run",
              "DccApplicationCommandStationPacket_load_idle"
            ],
            "tests": [
              {
                "name": "DccScheduler.run_sends_idle_when_empty",
                "file": "dcc_scheduler_Test.cxx",
                "desc": "With an empty queue, DccScheduler_run loads an idle packet -- proves the rail never goes empty (no DCC gaps)."
              }
            ],
            "hilChecks": [
              {
                "label": "refresh: a packet at least every 30 ms",
                "file": "s9_2_compliance.py",
                "desc": "Measures max packet start-to-start gap stays <= 30 ms"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2-CS-008",
          "feature": "Min 5 ms same-address spacing (112-127)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2",
            "cite": "Section C (Frequency of Packet Transmission), footnote 11",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "Enforced in DccScheduler_run: before loading a one-shot or refresh packet whose first byte is 0x70-0x7F (short address 112-127, which aliases the service-mode 0111CRRR command) and equals the last-loaded address byte, the scheduler loads an idle packet instead and retries the real packet next cycle. The intervening idle (~5.8 ms) provides the >=5 ms end-to-start gap required by S-9.2 Section C footnote 11. Long/accessory addresses (first byte 0xC0-0xFF / 0x80-0xBF) never alias and are unaffected. Idle spacer.",
            "gtest": "run_inserts_idle_spacer_between_same_short_addr_112_127 proves the spacer is inserted, repeat_count is preserved, and the spacer does not fire on_packet_sent; run_no_spacer_for_short_addr_below_112 and run_no_spacer_for_long_address confirm the guard is scoped to short addresses 112-127.",
            "hil": "Driven HIL check in s9_2_compliance.py: a single short loco at 115 in the refresh cycle, captured on the wire -- 7 packets to 115 each separated by an idle spacer, min same-address start-to-start 14.39 ms (>=5 ms); control addr 100 (<112) sent back-to-back. Bench-verified PASS."
          },
          "refs": {
            "symbols": [
              "DccScheduler_run",
              "_aliases_service_mode"
            ],
            "tests": [
              {
                "name": "DccScheduler.run_inserts_idle_spacer_between_same_short_addr_112_127",
                "file": "dcc_scheduler_Test.cxx",
                "desc": "A second same short-address (115) packet within 5 ms is replaced by an idle spacer; the real packet is retried next cycle (repeat_count preserved) and the spacer does not fire on_packet_sent."
              },
              {
                "name": "DccScheduler.run_no_spacer_for_short_addr_below_112",
                "file": "dcc_scheduler_Test.cxx",
                "desc": "Address 100 (0x64) does not alias a service-mode byte, so consecutive same-address packets are sent with no spacer."
              },
              {
                "name": "DccScheduler.run_no_spacer_for_long_address",
                "file": "dcc_scheduler_Test.cxx",
                "desc": "A long-address packet (first byte 0xC0-0xFF) never aliases, so no spacer is inserted."
              }
            ],
            "hilChecks": [
              {
                "label": "single short loco 115: idle spacer + >=5 ms same-address gap (control: addr 100 back-to-back)",
                "file": "s9_2_compliance.py",
                "desc": "Drives a single short loco at 115 and captures the wire; asserts the idle spacer is present (no two 115 packets adjacent) and same-address start-to-start >= 5 ms. Control addr 100 (<112) is sent back-to-back."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2-CS-009",
          "feature": "Address-0 analog loco control (zero-stretching)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2",
            "cite": "Address 0 / analog (RP-9.2.1; not in baseline S-9.2)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "no",
            "note": "unsupported (legacy, RP-9.2.1)"
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Running one true DC locomotive on DCC rails by stretching the '0' half-bits to create a net DC offset (address-0 / analog operation, RP-9.2.1; not part of baseline S-9.2). Intentionally NOT implemented: the DC bias overheats analog motors (current flows even at standstill), only one analog loco can be controlled, it degrades the signal for sound/keep-alive decoders, and most decoders ship with CV29 analog conversion disabled. A conformant command station need not provide it. (Distinct from the S-9.2.1 Analog Function Group, which IS supported.)",
            "gtest": "Intentionally not implemented; nothing to test.",
            "hil": "Intentionally not implemented; nothing to verify."
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_2_1",
      "spec": "S-9.2.1",
      "title": "Extended packets (multifunction)",
      "features": [
        {
          "tid": "DCC-S9.2.1-CS-001",
          "feature": "Encode 128-step speed (00111111 DSSSSSSS)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.2.1 (128 Speed Step Control)",
            "origin": "released",
            "draftDelta": {
              "change": "The released spec writes 128-step Advanced Operations as 00111111 followed by DSSSSSSS (bit7 = direction, bits6-0 = speed: 0=stop, 1=emergency stop, 2-127 = steps 1-126). The draft (section 2.3.2.1) keeps the identical instruction byte and bit assignment but describes the data byte generically as DDDDDDDD, noting bit7 is direction and the speed MSB is bit6. This is a notation harmonization only (RailCommunity RCN-212 style); the on-wire bits, the 126-step range, and the stop/e-stop encodings are unchanged, so an encoder needs no functional change."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_speed_128 emits 0x3F then DSSSSSSS plus XOR.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_speed_128"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.speed128_short_addr_forward",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts 0x3F + DSSSSSSS bytes"
              }
            ],
            "hilChecks": [
              {
                "label": "128-step speed 64 fwd",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte wire check of 128-step speed"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-001",
          "feature": "Decode 128-step speed",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.2.1 (128 Speed Step Control)",
            "origin": "released",
            "draftDelta": {
              "change": "Same as the encoder side: the released spec defines 128-step decode as 00111111 DSSSSSSS (bit7 direction, bits6-0 speed; 0=stop, 1=e-stop, 2-127=steps 1-126), and the draft (section 2.3.2.1) merely restates the data byte as DDDDDDDD while keeping bit7 as direction and bit6 as the speed MSB. This is documentation-notation harmonization with RCN-212 only; the decoded fields, ranges, and stop/e-stop values are bit-for-bit identical, so a decoder requires no functional change."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder recognizes 0x3F adv-ops byte and fires speed callback.",
            "gtest": "",
            "hil": "Decoder-side HIL requires a rig that does not exist. Needs decoder-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_ADV_OPS_128_SPEED"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.speed_128_forward",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded forward 128-step speed"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-002",
          "feature": "Encode 28-step speed (01DCSSSS)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.2.1 (28 Speed Step Control)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_speed_28 maps speed through the interleaved 01DCSSSS encoding.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_speed_28"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.speed28_step2_forward",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts interleaved C-bit/SSSS"
              }
            ],
            "hilChecks": [
              {
                "label": "28-step speed 11 fwd (c-bit odd)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check of 28-step"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-002",
          "feature": "Decode 28-step speed",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.2.1 (28 Speed Step Control)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder reverses the interleaved 28-step encoding.",
            "gtest": "",
            "hil": "Decoder-side HIL requires a rig that does not exist. Needs decoder-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_INST_SPEED_FORWARD"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.speed_28_forward_step_5",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded 28-step forward step 5"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-003",
          "feature": "Encode 14-step speed with FL bit",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.2.1 (14 Speed Step Control)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_speed_14 builds 01DCSSSS with C=headlight.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_speed_14"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.speed14_speed5_reverse_headlight_on",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts 14-step reverse w/ headlight"
              }
            ],
            "hilChecks": [
              {
                "label": "14-step max (SSSS=15, FL on)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check of 14-step + FL"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-003",
          "feature": "Decode 14-step speed",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.2.1 (14 Speed Step Control)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder decodes 01DCSSSS as 14-step when configured.",
            "gtest": "",
            "hil": "Decoder-side HIL requires a rig that does not exist. Needs decoder-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_INST_SPEED_FORWARD"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.speed_14_step",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded 14-step speed"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-004",
          "feature": "Encode function group 1 (FL,F1-F4)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.4 (Function Group One)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_func_group_1 ORs 5 function bits into 0x80.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_func_group_1"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.func_group1_all_on",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts 0x9F for all on"
              }
            ],
            "hilChecks": [
              {
                "label": "func group 1: FL on",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check of group 1"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-004",
          "feature": "Decode function group 1",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.4 (Function Group One)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder extracts FL/F1-F4 from 100DDDDD.",
            "gtest": "",
            "hil": "Decoder-side HIL requires a rig that does not exist. Needs decoder-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_INST_FUNC_GROUP_1"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.func_group1_fl_on_f1_f2_on",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded FL/F1/F2 states"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-005",
          "feature": "Encode function group 2a (F5-F8)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.5 (Function Group Two, S=1)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_func_group_2a ORs F5-F8 into 0xB0.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_func_group_2a"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.func_group2a_all_on",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts 0xBF"
              }
            ],
            "hilChecks": [
              {
                "label": "func group 2: F5 on (1011)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check of group 2a"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-005",
          "feature": "Decode function group 2a",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.5 (Function Group Two, S=1)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder dispatches F5-F8 from 1011DDDD.",
            "gtest": "",
            "hil": "Decoder-side HIL requires a rig that does not exist. Needs decoder-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_INST_FUNC_GROUP_2A"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.func_group2a_f5_f6_on",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded F5/F6"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-006",
          "feature": "Encode function group 2b (F9-F12)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.5 (Function Group Two, S=0)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_func_group_2b ORs F9-F12 into 0xA0.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_func_group_2b"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.func_group2b_all_on",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts 0xAF"
              }
            ],
            "hilChecks": [
              {
                "label": "func group 2: F9 on (1010)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check of group 2b"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-006",
          "feature": "Decode function group 2b",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.5 (Function Group Two, S=0)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder dispatches F9-F12 from 1010DDDD.",
            "gtest": "",
            "hil": "Decoder-side HIL requires a rig that does not exist. Needs decoder-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_INST_FUNC_GROUP_2B"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.func_group2b_f9_on",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded F9"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-007",
          "feature": "Encode function expansion F13-F68",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.6 (Feature Expansion, F13-F68)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_func_f13_f20..f61_f68 emit expansion byte + 8 bits.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_func_f13_f20",
              "DccApplicationCommandStationPacket_load_func_f61_f68"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.func_f13_f20_short_addr",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts 0xDE + bits"
              },
              {
                "name": "DccPacketEncoder.func_f61_f68_instruction_byte",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts 0xDC expansion byte"
              }
            ],
            "hilChecks": [
              {
                "label": "F13-F20 expansion (11011110)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check"
              },
              {
                "label": "F61-F68 expansion (11011100)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check of high group"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-007",
          "feature": "Decode function expansion F13-F68",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.6 (Feature Expansion, F13-F68)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder recognizes 0xD8-0xDF and dispatches the F13-F68 group.",
            "gtest": "",
            "hil": "Decoder-side HIL requires a rig that does not exist. Needs decoder-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_FEAT_F13_F20"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.func_f13_f20",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded F13-F20"
              },
              {
                "name": "DccPacketDecoder.func_f61_f68",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded F61-F68"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-008",
          "feature": "Encode basic accessory packet",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.4 (Basic Accessory Packet)",
            "origin": "released",
            "draftDelta": {
              "change": "The released basic-accessory second byte is 1AAACDDD / 1AAADAAR: a leading 1, three inverted high address bits, an activate bit, and an output field. The draft (section 2.4.1, footnote 12) formally adopts 1AAADAAR to harmonize with RCN-213: bit3 'D' activates/deactivates, bit0 'R' selects which output of the pair (R=0 diverging/stop, R=1 normal/proceed), bits4-6 hold the inverted high address bits with bits1-2 the two LSBs. The on-wire positions match current practice — old C becomes D, the old DDD output field collapses to the single R output-select bit — so an encoder built to the modern 1AAADAAR layout needs no functional change."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_accessory_basic builds two bytes with inverted upper-3 address bits.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_accessory_basic"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.accessory_basic_addr5_output2_activate",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts activate byte layout"
              },
              {
                "name": "DccPacketEncoder.accessory_basic_high_address",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts inverted high address bits"
              }
            ],
            "hilChecks": [
              {
                "label": "accessory basic (inverted high addr bits)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check"
              },
              {
                "label": "accessory basic board 511 (9-bit max)",
                "file": "s9_2_1_compliance.py",
                "desc": "9-bit max boundary"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-008",
          "feature": "Decode basic accessory packet",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.4 (Basic Accessory Packet)",
            "origin": "released",
            "draftDelta": {
              "change": "As with the encoder, the draft (section 2.4.1, footnote 12) restates the basic-accessory second byte as 1AAADAAR (RCN-213 harmonization), with D=activate (bit3) and R=output-of-pair select (bit0). Beyond notation, the draft adds a decoder-relevant detail: Table 2.2 defines two user-address-to-A10..A0 mapping conventions, 'Linear' (required for new designs) and 'Non-Linear' (legacy), differing in how the address rolls over past user-address 252. The decoder's on-wire decode is unchanged (all A10..A0 are valid); the linear/non-linear choice only affects how a UI presents addresses, so default to Linear."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder reconstructs address from inverted bits and fires accessory callback.",
            "gtest": "",
            "hil": "Accessory-side HIL requires a rig that does not exist. Needs accessory-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_ACCESSORY_BASIC_PREFIX"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.basic_accessory",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded basic accessory"
              },
              {
                "name": "DccPacketDecoder.output_address_mode_basic_accessory",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts output-address-mode decode"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-009",
          "feature": "Encode extended accessory packet with aspect byte",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.4.2 (Extended Accessory Control)",
            "origin": "released",
            "draftDelta": {
              "change": "The released extended-accessory packet is 10AAAAAA 0AAA0AA1 DDDDDDDD with the third byte carrying an 8-bit signal aspect (00000000 = absolute stop). The draft (section 2.4.2) adds a new time-based interpretation of the third byte as RZZZZZZZ: ZZZZZZZ specifies an output on-time at 100 ms resolution (0 = off, 1111111 = on continuously until the next command) and bit7 'R' selects which output of a pair. This is a genuine functional addition: an encoder targeting timed outputs must emit a 0-127 duration (x100 ms) with the R bit rather than only an aspect code; the aspect-control path is unchanged."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_accessory_extended builds two address bytes plus an aspect byte.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_accessory_extended"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.accessory_extended_addr0_aspect5",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts aspect/address layout"
              }
            ],
            "hilChecks": [
              {
                "label": "accessory extended (inverted high addr bits)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check"
              },
              {
                "label": "accessory extended board 511 (9-bit max)",
                "file": "s9_2_1_compliance.py",
                "desc": "9-bit max boundary"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-009",
          "feature": "Decode extended accessory packet",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.4.2 (Extended Accessory Control)",
            "origin": "released",
            "draftDelta": {
              "change": "Mirroring the encoder, the released spec decodes the extended-accessory third byte solely as an 8-bit aspect (0 = absolute stop). The draft (section 2.4.2) adds a second decode mode where the third byte is RZZZZZZZ: bit7 'R' is the output-pair selector and ZZZZZZZ is a timed on-duration at 100 ms resolution (0 = off, 127 = on indefinitely). A decoder supporting timed outputs must distinguish aspect-data from timed-output usage and apply the 100 ms-per-count timer; aspect-only decoders are unaffected on the wire."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder reconstructs the extended address and fires the aspect callback.",
            "gtest": "",
            "hil": "Accessory-side HIL requires a rig that does not exist. Needs accessory-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_ACCESSORY_EXTENDED_PREFIX"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.extended_accessory",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded extended accessory aspect"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-010",
          "feature": "Encode accessory basic stop",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.4 (Basic Accessory deactivate)",
            "origin": "released",
            "draftDelta": {
              "change": "The released spec deactivates a basic accessory by sending the packet with the activate bit cleared. The draft (section 2.4.1) restates this under the harmonized 1AAADAAR notation where deactivation is simply D=0 (bit3=0), R still selecting the output; it also notes that if the on-duration is within the CV515-518 timed window no explicit deactivation is required. This is RCN-213 notation harmonization (the 'Table 36' framing is the RCN cross-reference, not a new format); the deactivate packet is bit-identical to current practice, so the encoder is functionally unchanged."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_accessory_basic_stop builds basic byte with C=0 (deactivate).",
            "gtest": "",
            "hil": "s9_2_1 exact-byte vector: ACC <b> <p> OFF on the wire matches the independent deactivate encoder (D=0). (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_accessory_basic_stop"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.accessory_basic_stop_valid",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts C=0 deactivate byte"
              }
            ],
            "hilChecks": [
              {
                "label": "basic accessory deactivate/stop byte (D=0)",
                "file": "s9_2_1_compliance.py",
                "desc": "ACC OFF wire bytes equal the independent accessory_basic(activate=False) encoder."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-011",
          "feature": "Encode accessory extended stop (aspect 0)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.4.2 (Extended Accessory all-stop)",
            "origin": "released",
            "draftDelta": {
              "change": "The released spec stops an extended accessory by sending aspect 0. The draft (section 2.4.2) confirms a third-byte value of 00000000 indicates the absolute-stop aspect, and in timed-output mode ZZZZZZZ=0 likewise switches the output off. This is a notation/harmonization restatement aligned with RailCommunity (the 'Table 36' reference is the RCN cross-map); the on-wire encoding of all-stop is unchanged, so the encoder is functionally identical."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_accessory_extended_stop emits aspect byte 0x00.",
            "gtest": "",
            "hil": "s9_2_1 exact-byte vector: ACCE <addr> 0 emits the aspect-0 all-stop packet, matching the independent extended-accessory encoder. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_accessory_extended_stop"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.accessory_extended_stop_valid",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts aspect=0 all-stop"
              }
            ],
            "hilChecks": [
              {
                "label": "extended accessory all-stop (aspect 0)",
                "file": "s9_2_1_compliance.py",
                "desc": "ACCE 0 wire bytes equal accessory_extended(aspect=0)."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-012",
          "feature": "Encode accessory NOP (0AAA1AAT)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.4.6 (Accessory NOP Packet)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_accessory_nop builds two bytes with NOP marker bit3=1 and T basic/extended. Matrix said unimplemented; function exists.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_accessory_nop"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.accessory_nop_addr1_basic",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts NOP basic T=0"
              },
              {
                "name": "DccPacketEncoder.accessory_nop_addr1_extended",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts NOP extended T=1"
              }
            ],
            "hilChecks": [
              {
                "label": "accessory NOP basic (0AAA1AAT, T=0)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check of NOP basic"
              },
              {
                "label": "accessory NOP extended (T=1)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check of NOP extended"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-013",
          "feature": "Encode CV ops-mode (POM) write",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.7.3 (CV Access Long Form, write)",
            "origin": "released",
            "draftDelta": {
              "change": "The released CV Access Long Form (POM) write is 1110CCVV VVVVVVVV DDDDDDDD with CC selecting the operation (01=verify, 11=write, 10=bit-manip) and a 111FDBBB bit-manip byte. The draft (section 2.3.7.3) renames the 2-bit operation selector from CC to GG (its generic 'instruction sub-type' field) and keeps F as the bit-manip flag; a write is GG=11 and requires two identical packets before commit. This is purely a notation change harmonizing field names — the instruction byte, the 10-bit CV address (CV1 = '00 00000000'), the values, and the data layout are bit-for-bit identical, so a POM-write encoder is unchanged."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_cv_write_pom emits 1110 11 + CV-low + data.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_cv_write_pom"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.cv_write_ops_short_addr",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts write instruction and CV bytes"
              }
            ],
            "hilChecks": [
              {
                "label": "CV-POM write CV1=8 (1110 11)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check of POM write"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-014",
          "feature": "Encode CV ops-mode (POM) verify",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.7.3 (CV Access Long Form, verify)",
            "origin": "released",
            "draftDelta": {
              "change": "The released spec encodes a POM CV verify as Long Form with CC=01. The draft (section 2.3.7.3) restates this as GG=01 'Verify byte': the decoder compares the addressed CV against DDDDDDDD and, if equal and enabled, returns the value via the ops-mode/RailCom response. Only the field label changed (CC to GG) as cross-standard harmonization; the 1110GGVV VVVVVVVV DDDDDDDD bits and verify semantics are identical, so the encoder needs no functional change."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_cv_verify_pom emits CC=01 verify.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_cv_verify_pom"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.cv_verify_ops_short_addr",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts verify instruction"
              }
            ],
            "hilChecks": [
              {
                "label": "CV-POM verify CV1=8 (1110 01)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check of POM verify"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-015",
          "feature": "Encode CV ops-mode (POM) bit manipulation",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.7.3 (CV bit manipulation)",
            "origin": "released",
            "draftDelta": {
              "change": "The released POM bit-manipulation uses Long Form CC=10 with data byte 111FDBBB (F=1 write / 0 verify, D=bit value, BBB=bit position 0-7). The draft (section 2.3.7.3) restates this as GG=10 with the same 111FDBBB byte; a write-bit requires two identical packets before commit. This is a CC-to-GG notation harmonization only — the instruction byte, the F flag, and the DBBB fields are bit-identical to the released form (note this F differs from the K flag the S-9.2.3 service-mode form uses for the same field), so the encoder is functionally unchanged."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_cv_bit_pom emits CC=10 plus 111CDBBB.",
            "gtest": "",
            "hil": "s9_2_1 exact-byte vector: CV BIT emits the 1110 10 instruction + 111KDBBB data byte, matching the independent POM bit encoder. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_cv_bit_pom"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.cv_bit_ops_write_bit3_high",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts bit-write byte"
              }
            ],
            "hilChecks": [
              {
                "label": "CV-POM bit-manipulation packet (1110 10)",
                "file": "s9_2_1_compliance.py",
                "desc": "CV BIT wire bytes equal cv_bit_pom()."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-010",
          "feature": "Decode CV ops-mode write/verify/bit",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.7.3 (CV Access Long Form)",
            "origin": "released",
            "draftDelta": {
              "change": "On decode, the released CV Access Long Form (1110CCVV VVVVVVVV DDDDDDDD; CC=01 verify / 11 write / 10 bit-manip; bit-manip byte 111FDBBB) is restated by the draft (section 2.3.7.3) using GG for the operation selector and retaining F for the bit-manip flag. This is a cross-standard notation harmonization only; the decoded operation values, the 10-bit CV address (+1 for CV number), and the 111FDBBB layout are bit-for-bit identical, so a decoder requires no functional change."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder dispatches 1110CCDD CV access to write/verify/bit callbacks.",
            "gtest": "",
            "hil": "Decoder-side HIL requires a rig that does not exist. Needs decoder-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_CV_LONG_WRITE"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.cv_write_ops_mode",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts ops-mode CV write dispatched"
              },
              {
                "name": "DccPacketDecoder.cv_bit_write",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts CV bit write applied"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-ACC-001",
          "feature": "Encode basic accessory CV access",
          "role": "acc",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.4 (Basic Accessory CV Access)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_accessory_basic_cv_* build a 5-byte accessory CV instruction.",
            "gtest": "",
            "hil": "s9_2_1 exact-byte vectors: ACC CV WRITE/VERIFY/BIT emit the 5-byte basic accessory CV instruction, matching the independent encoder. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_accessory_basic_cv_write"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.acc_basic_cv_write_addr5_pair2_cv1",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts basic accessory CV write payload"
              }
            ],
            "hilChecks": [
              {
                "label": "basic accessory CV write/verify/bit",
                "file": "s9_2_1_compliance.py",
                "desc": "ACC CV wire bytes equal accessory_basic_cv()."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-ACC-002",
          "feature": "Encode extended accessory CV access",
          "role": "acc",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.4.2 (Extended Accessory CV Access)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_accessory_extended_cv_* build a 5-byte extended accessory CV instruction.",
            "gtest": "",
            "hil": "s9_2_1 exact-byte vectors: ACCE CV WRITE/VERIFY/BIT emit the 5-byte extended accessory CV instruction, matching the independent encoder. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_accessory_extended_cv_write"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.acc_extended_cv_write_addr0_cv1",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts extended accessory CV write payload"
              }
            ],
            "hilChecks": [
              {
                "label": "extended accessory CV write/verify/bit",
                "file": "s9_2_1_compliance.py",
                "desc": "ACCE CV wire bytes equal accessory_extended_cv()."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-016",
          "feature": "XPOM (extended POM) encoder",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.7.4 (Extended CV Access / XPOM)",
            "origin": "released",
            "draftDelta": {
              "change": "The released reference only sketches XPOM as a '1110GGSS form carrying a 24-bit indexed CV address' for up to four contiguous CVs. The draft (section 2.3.7.4) fully specifies it: byte 1110GGSS (GG: 01=read bytes, 11=write bytes, 10=write bits; SS = a sequence number used only for S-9.3.2 block reads, else 00) then a 24-bit index where byte1=CV31, byte2=CV32, byte3=the Long-Form low CV-address byte, then up to four optional data bytes (write-bits uses a 1111-DBBB fifth byte). XPOM intentionally exceeds the 6-byte packet maximum and therefore requires two identical packets before any write is committed, with all four CV values reported back via RailCom. An implementer must encode the full 24-bit index, gate writes on two-identical-packet receipt, and tolerate oversized packets."
            }
          },
          "supported": {
            "state": "no",
            "note": ""
          },
          "gtest": {
            "state": "no",
            "note": ""
          },
          "hil": {
            "state": "no",
            "note": ""
          },
          "detail": {
            "impl": "Only DCC_RAILCOM_ID_XPOM_8..11 exist; no command-station XPOM builder. Only RailCom response IDs defined, no encoder.",
            "gtest": "No XPOM encoder to test. Encoder not implemented yet.",
            "hil": "No XPOM encoder to verify. Encoder not implemented yet."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_ID_XPOM_8"
            ],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-017",
          "feature": "Encode consist set",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.1.4 (Consist Control / Set)",
            "origin": "released",
            "draftDelta": {
              "change": "The released spec encodes consist setup as 0001001C 0AAAAAAA, where the single C bit sets direction normal (0) or reversed (1) and AAAAAAA is the 1-127 consist address. The draft (section 2.3.1.4) re-notates the instruction as 0001TTTT with a 4-bit field: TTTT=0010 sets the consist normal (CV19 bit7=0), TTTT=0011 sets it reversed (CV19 bit7=1), others reserved. Because old C=0 yields 00010010 and C=1 yields 00010011, the new TTTT=0010/0011 encodings are bit-for-bit identical — notation only — so a 'consist set' encoder is unchanged."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_consist_set emits 0x12/0x13 + 7-bit consist address.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_consist_set"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.consist_set_normal",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts normal-direction consist set"
              }
            ],
            "hilChecks": [
              {
                "label": "consist set addr 5 normal",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check of consist set"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-018",
          "feature": "Encode consist clear",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.1.4 (Consist Control / Clear)",
            "origin": "released",
            "draftDelta": {
              "change": "The released spec clears a consist by sending the consist-setup instruction with address 0000000, deactivating the consist and resetting CV19. The draft (section 2.3.1.4) keeps that behavior under the new TTTT notation: TTTT=0010 or 0011 with the second-byte address = 0000000 deactivates the consist (restoring bi-directional settings per CVs 26-28). The on-wire clear packet is identical to the released form; only the field naming (C to TTTT) changed, so the 'consist clear' encoder needs no functional change."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_consist_clear delegates to load_consist_set with address 0.",
            "gtest": "",
            "hil": "s9_2_1 exact-byte vector: CONSIST CLEAR emits the consist-set-address-0 packet, matching the independent encoder. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_consist_clear"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.consist_clear",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts clear encodes consist addr 0"
              }
            ],
            "hilChecks": [
              {
                "label": "consist clear (set consist addr 0)",
                "file": "s9_2_1_compliance.py",
                "desc": "CONSIST CLEAR wire bytes equal consist_clear()."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-011",
          "feature": "Decode consist set/clear",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.1.4 (Consist Control)",
            "origin": "released",
            "draftDelta": {
              "change": "The released decode treats consist setup/clear as 0001001C 0AAAAAAA (C = direction, address 0 = clear). The draft (section 2.3.1.4) re-notates the instruction byte as 0001TTTT, decoding TTTT=0010 as set-normal and TTTT=0011 as set-reversed, with a zero address meaning deactivate. Since 0010/0011 map exactly onto old C=0/C=1, this is notation harmonization only; the decoded direction, address range, and clear-on-zero semantics are bit-identical, so the decoder is unchanged."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder dispatches consist control to the consist callback.",
            "gtest": "",
            "hil": "Decoder-side HIL requires a rig that does not exist. Needs decoder-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_CONSIST_SET_NORMAL"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.consist_set_normal",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded consist set"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-019",
          "feature": "Encode binary state short",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.6.1 (Binary State Short Form)",
            "origin": "released",
            "draftDelta": {
              "change": "In the project's prior filing, Binary State Control was attributed to S-9.2.1.1. The draft places the Binary State Short Form squarely in S-9.2.1 as Feature Expansion sub-instruction GGGGG=11101 (section 2.3.6.4): 11011101 0 DLLLLLLL, where LLLLLLL is the state number 1-127, D the on/off value, and LLLLLLL=0 broadcasts to all 127 short-form states. Footnote 5 adds that states 1-15 are reserved for NMRA bi-directional communication (S-9.3.2). On-wire the bytes are unchanged; the practical impact is to avoid emitting application data on states 1-15 and to treat L=0 as the all-states broadcast."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_binary_state_short emits 0xDD + DLLLLLLL.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_binary_state_short"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.binary_state_short_activate",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts active bit + state"
              }
            ],
            "hilChecks": [
              {
                "label": "binary state short (11011101)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check"
              },
              {
                "label": "binary state NOT auto-refreshed",
                "file": "s9_2_1_compliance.py",
                "desc": "verifies no auto-refresh of binary state"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-012",
          "feature": "Decode binary state short",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.6.1 (Binary State Short Form)",
            "origin": "released",
            "draftDelta": {
              "change": "The Binary State Short Form decode is confirmed by the draft to live in S-9.2.1 as Feature Expansion GGGGG=11101 (section 2.3.6.4), packet 11011101 0 DLLLLLLL: LLLLLLL (1-127) the state number, D the value, LLLLLLL=0 the broadcast over all 127 short-form states. Footnote 5 reserves states 1-15 for bi-directional communication (S-9.3.2). The bit layout is unchanged; the decoder change is the section relocation plus awareness that states 1-15 carry RailCom reservation semantics."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder recognizes 0xDD and dispatches binary state short.",
            "gtest": "",
            "hil": "Decoder-side HIL requires a rig that does not exist. Needs decoder-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_FEAT_BINARY_STATE_SHORT"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.binary_state_short",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded binary state short"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-020",
          "feature": "Encode binary state long",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.6.1 (Binary State Long Form)",
            "origin": "released",
            "draftDelta": {
              "change": "The draft places the Binary State Long Form in S-9.2.1 as Feature Expansion GGGGG=00000 (section 2.3.6.1), a three-byte instruction 11000000 0 DLLLLLLL 0 HHHHHHHH addressing 1-32767 states: LLLLLLL the low 7 bits, HHHHHHHH the high 8, D (bit7 of the second data byte) the on/off value; address 0 broadcasts to all 32767 states. Footnote 4 reserves states 1-15 for bi-directional communication, and the spec requires states 1-127 be sent via the short form. The 32767-state encoding is unchanged on the wire; the impact is the relocation, the use-short-form-for-1-127 rule, and avoiding states 1-15."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_binary_state_long emits feature byte + low/high state bytes.",
            "gtest": "",
            "hil": "s9_2_1 exact-byte vector: BSL emits 0xC0 (11000000) + DLLLLLLL + HHHHHHHH per S-9.2.1 §2.3.6.1. This independent encoder caught a real library bug — DCC_FEAT_BINARY_STATE_LONG was 0xDC (colliding with F61-F68); fixed in dcc_defines.h + dcc_packet_decoder.c (host gTests pass). (Authored; reflash firmware for the wire validation — library fix applied 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_binary_state_long"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.binary_state_long_activate",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts low/high state bytes"
              }
            ],
            "hilChecks": [
              {
                "label": "binary state long opcode 0xC0 + state bytes",
                "file": "s9_2_1_compliance.py",
                "desc": "BSL wire bytes equal the independent binary_state_long() encoder (0xC0 long-form opcode)."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-013",
          "feature": "Decode binary state long",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.6.1 (Binary State Long Form)",
            "origin": "released",
            "draftDelta": {
              "change": "The Binary State Long Form decode is confirmed by the draft in S-9.2.1 as Feature Expansion GGGGG=00000 (section 2.3.6.1): 11000000 0 DLLLLLLL 0 HHHHHHHH, state = HHHHHHHH:LLLLLLL (1-32767), D = on/off, address 0 = broadcast over all states. Footnote 4 reserves states 1-15 for bi-directional communication. The decoded fields and range are bit-identical to prior practice; the change is the relocation into S-9.2.1 plus the RailCom reservation note for states 1-15."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder recognizes the long-form feature byte and dispatches the 15-bit state.",
            "gtest": "",
            "hil": "Decoder-side HIL requires a rig that does not exist. Needs decoder-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_FEAT_BINARY_STATE_LONG"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.binary_state_long",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded binary state long"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-021",
          "feature": "Encode analog function group (00111101)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.2.3 (Analog Function Group)",
            "origin": "released",
            "draftDelta": {
              "change": "An earlier project note carried an erratum giving the Analog Function Group instruction byte as 11111101. The draft (section 2.3.2.3) defines it correctly as Advanced Operations sub-instruction GGGGG=11101, i.e. byte 00111101, followed by VVVVVVVV (output; 00000001 = Volume Control, others reserved) and DDDDDDDD (analog value), and confirms it lives in S-9.2.1 and must not control speed. The corrected top two bits (00, not 11) are an on-wire correction relative to the erroneous note; an encoder built to the correct value is already right."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_analog_function emits 0x3D + output + value.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_analog_function"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.analog_function_volume",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts 0x3D + output + value"
              }
            ],
            "hilChecks": [
              {
                "label": "analog function (00111101)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-014",
          "feature": "Decode analog function group",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.2.3 (Analog Function Group)",
            "origin": "released",
            "draftDelta": {
              "change": "Decode side of the analog-function correction: an earlier project note carried an erratum giving the Analog Function Group instruction byte as 11111101. The draft (section 2.3.2.3) confirms it is Advanced Operations sub-instruction GGGGG=11101, i.e. byte 00111101, followed by VVVVVVVV (output; 00000001 = Volume Control, others reserved) and DDDDDDDD (analog value), residing in S-9.2.1 and not for speed control. The corrected top two bits (00, not 11) are an on-wire correction versus the erroneous note; a decoder built to the correct 00111101 value needs no change."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder recognizes 0x3D and dispatches the analog function callback.",
            "gtest": "",
            "hil": "Decoder-side HIL requires a rig that does not exist. Needs decoder-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_ADV_OPS_ANALOG_FUNCTION"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.analog_function",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts decoded analog function"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-022",
          "feature": "Encode system-time broadcast",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.6.3 (System Time)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_system_time broadcasts addr 0 + 0xC2 + 16-bit ms MSB-first. Matrix said unimplemented; function exists.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_system_time"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.system_time_zero",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts 00 C2 00 00"
              },
              {
                "name": "DccPacketEncoder.system_time_max_value",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts FF FF max"
              }
            ],
            "hilChecks": [
              {
                "label": "system time ms=65535 (FF FF, 16-bit max)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte boundary check"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-023",
          "feature": "Encode model-time broadcast",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.6.2 (Time and Date / Time)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_model_time broadcasts addr 0 + 0xC1 + time fields. Matrix said unimplemented; function exists.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_model_time"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.model_time_representative",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts time-command field packing"
              }
            ],
            "hilChecks": [
              {
                "label": "model time 14:30 Wed accel 8",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte check of model time"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-024",
          "feature": "Encode model-date broadcast",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.6.2 (Time and Date / Date)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "load_model_date broadcasts addr 0 + 0xC1 + 12-bit year fields. Matrix said unimplemented; function exists.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_model_date"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.model_date_representative",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts date field packing"
              }
            ],
            "hilChecks": [
              {
                "label": "model date max fields (12-bit year)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte boundary check"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-CS-025",
          "feature": "Short vs long locomotive addressing",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.1 (Multi-Function addressing)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "_encode_address emits 1-byte short or 2-byte 0xC0|high/low long address.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DccApplicationCommandStationPacket_load_speed_128"
            ],
            "tests": [
              {
                "name": "DccPacketEncoder.speed128_long_addr",
                "file": "dcc_application_command_station_packet_Test.cxx",
                "desc": "asserts 2-byte long-address"
              }
            ],
            "hilChecks": [
              {
                "label": "long address (11AAAAAA AAAAAAAA)",
                "file": "s9_2_1_compliance.py",
                "desc": "exact-byte wire check of long address"
              },
              {
                "label": "short address max (7F)",
                "file": "s9_2_1_compliance.py",
                "desc": "boundary check of short-address max"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.1-DEC-015",
          "feature": "Accept short and long addressing",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1",
            "cite": "Section 2.3.1 (Multi-Function addressing)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder matches its short or long address before dispatching multifunction instructions.",
            "gtest": "",
            "hil": "Decoder-side HIL requires a rig that does not exist. Needs decoder-HIL rig (none exists)."
          },
          "refs": {
            "symbols": [
              "DCC_ADV_OPS_128_SPEED"
            ],
            "tests": [
              {
                "name": "DccPacketDecoder.long_address_speed_128",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts long-address packet accepted"
              },
              {
                "name": "DccPacketDecoder.long_addr_packet_to_short_decoder_ignored",
                "file": "dcc_packet_decoder_Test.cxx",
                "desc": "asserts long packet ignored by short decoder"
              }
            ],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_2_1_1",
      "spec": "S-9.2.1.1",
      "title": "Logon / data spaces",
      "features": [
        {
          "tid": "DCC-S9.2.1.1-DEC-001",
          "feature": "Decoder logon auto-registration (partitions 253/254)",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1.1",
            "cite": "Automatic Logon (partitions 253/254 auto-registration)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: S-9.2.1.1 (2022) defines Logon auto-registration over the 253/254 address partitions, letting a System discover a decoder by its 44-bit Unique ID and assign a session DCC address with no manual addressing. The draft fully fleshes out the four-step procedure: Enumeration (System broadcasts 'Logon Enable' carrying a 16-bit CID and 8-bit Session ID at least every 300 ms; unselected decoders reply in channels 1+2 with their 12-bit Manufacturer ID + 32-bit Unique ID, collisions resolved by per-type back-offs keyed off the GG group field: 00=ALL, 01=LOCO, 10=ACC, 11=NOW), Confirmation via Select/ReadShortInfo to the DID, Assignment via 'Logon Assign', and optional Configuration Discovery. CV28 bit7 must be set or the decoder ignores all 254-partition commands 0xE0-0xFF; on restart a decoder rejoins Selected only if the CID matches and the Session ID is unchanged or incremented by fewer than four. Implementer impact: a command station must emit periodic Logon Enable, hash a stable CID, manage Session IDs across reboots, and parse the channel-1+2 feedback; decoders implement the Unselected/Selected state machine and back-off timing."
            }
          },
          "supported": {
            "state": "no",
            "note": ""
          },
          "gtest": {
            "state": "no",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Not implemented; only the RailCom Logon Enable datagram ID is defined with no logic. No logon state machine; only DCC_RAILCOM_ID_LOGON_ENABLE define with no consuming logic.",
            "gtest": "No tests exist for decoder logon. No host tests cover logon.",
            "hil": "No HIL suite exists. No HIL suite for S-9.2.1.1."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_ID_LOGON_ENABLE"
            ],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1.1-DEC-002",
          "feature": "CRC-8 plus XOR for packets > 6 bytes",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1.1",
            "cite": "Packet integrity (CRC-8 in addition to XOR for long packets)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: ordinary S-9.2 packets use a single XOR error-detection byte. The draft (section 2.1) defines a dual-checksum scheme for the 253/254 partitions: if total packet length including the XOR byte is 6 bytes or fewer, normal XOR-only framing applies; if it exceeds 6 bytes, an extra CRC-8 byte is inserted immediately before the trailing XOR byte and the decoder must validate BOTH. The CRC-8 is the Dallas/Maxim 1-Wire polynomial x^8 + x^5 + x^4 + 1, init 0, not inverted, so the receiver's result over the whole message including the CRC-8 byte (excluding XOR) is 0; the same CRC-8 is reused for feedback unless a Data Space number overrides the seed. 7-byte packets are forbidden entirely on 253/254 (Systems shall not send, decoders shall ignore) because 7 is the ambiguous boundary between the two regimes; max packet is 32 bytes. Implementer impact: need a 1-Wire CRC-8 routine, select framing by length, and hard-skip the 7-byte case."
            }
          },
          "supported": {
            "state": "no",
            "note": ""
          },
          "gtest": {
            "state": "no",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Not implemented; packet building uses only the XOR error byte. No CRC-8 routine or polynomial anywhere in src/dcc.",
            "gtest": "No tests exist for CRC-8. No CRC-8 tests.",
            "hil": "No HIL suite exists. No HIL suite for S-9.2.1.1."
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.1.1-DEC-003",
          "feature": "Data Spaces (ReadBlock / WriteBlock)",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.1.1",
            "cite": "Data Space commands (ReadBlock / WriteBlock)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: bulk configuration was only reachable via per-CV POM/XPOM. The draft (section 7) defines Data Spaces — numbered byte arrays for bulk transfer — with four defined spaces: 0 Capabilities (RO), 1 Data Space Info (RO bitmap of supported spaces), 2 Short GUI (name/symbol/function metadata), 3 Configuration Variables (the indexed CV space); 4-255 reserved. WriteBlock (253-partition command 0xFC) carries a 1-byte space number, 3-byte big-endian offset, then payload, acked in channel 2; completion is reported via ID13 feedback (1CCCCCCC: 0x81 success, 0x82 permanent error, 0x83 retryable/busy, 0x80 in-progress), with progress at least every 500 ms and a 700 ms-silence lost-write timeout, plus an 'Addressed Continue' (0x80-prefixed) to resume. ReadBlock (0xFE) and ReadBackground (0xFD) take the same space-number+offset(+length) args; ReadBlock streams chunks via 254-partition Get Data Start/Continue while ReadBackground rides ID13/ID14. For reads the CRC-8 seed is the Data Space number. Implementer impact: a Variable-Length-Feedback header, space-seeded CRC-8, offset/length/continue state machine, and the progress/timeout contract — WriteBlock is Mandatory for the Bulk-Write class, the read variants optional."
            }
          },
          "supported": {
            "state": "no",
            "note": ""
          },
          "gtest": {
            "state": "no",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Not implemented; no block-transfer or data-space code present. No ReadBlock/WriteBlock or data-space logic in src/dcc.",
            "gtest": "No tests exist for data spaces. No data-space tests.",
            "hil": "No HIL suite exists. No HIL suite for S-9.2.1.1."
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_2_2",
      "spec": "S-9.2.2",
      "title": "Configuration variables",
      "features": [
        {
          "tid": "DCC-S9.2.2-DEC-001",
          "feature": "CV read/write via callbacks",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.2",
            "cite": "Configuration Variable access (read/write through persistent storage)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "nobs",
            "note": ""
          },
          "detail": {
            "impl": "DccCvStorage_read/write delegate to injected callbacks with null guards.",
            "gtest": "",
            "hil": "CV-storage logic is host-tested; on-wire access covered under S-9.2.1. CV access on the wire rides POM under S-9.2.1; storage logic is host-only."
          },
          "refs": {
            "symbols": [
              "DccCvStorage_read",
              "DccCvStorage_write"
            ],
            "tests": [
              {
                "name": "DccCvStorage.read_returns_stored_value",
                "file": "dcc_cv_storage_Test.cxx",
                "desc": "asserts read returns the stored value"
              },
              {
                "name": "DccCvStorage.write_stores_value_when_unlocked",
                "file": "dcc_cv_storage_Test.cxx",
                "desc": "asserts write persists when unlocked"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.2-DEC-002",
          "feature": "Decoder lock (CV15/CV16)",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.2",
            "cite": "Decoder Lock (CV15 index / CV16 lock)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: S-9.2.2 defines a Decoder Lock where CV15 holds a key and CV16 a compare value, isolating one of several decoders sharing an address (e.g. motor vs sound) so only the matching decoder accepts CV writes. The draft tightens this normatively: apply a write only when CV15 == CV16, and a value of 0 in CV16 disables the lock entirely (every CV writable). It also makes explicit that the lock never applies to CV15/CV16 themselves — writes that change CV15 or CV16 are always allowed. Implementer impact: gate CV writes on (CV16==0 || CV15==CV16), but always exempt CV15/CV16 so an operator can never lock out the unlock mechanism."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "nobs",
            "note": ""
          },
          "detail": {
            "impl": "is_locked compares CV15 to CV16; write blocks all CVs except 15/16 (and CV8=8) while locked.",
            "gtest": "",
            "hil": "Lock logic is host-only."
          },
          "refs": {
            "symbols": [
              "DccCvStorage_is_locked"
            ],
            "tests": [
              {
                "name": "DccCvStorage.is_locked_when_cv15_ne_cv16",
                "file": "dcc_cv_storage_Test.cxx",
                "desc": "asserts is_locked true when CV15 != CV16"
              },
              {
                "name": "DccCvStorage.write_blocked_when_locked",
                "file": "dcc_cv_storage_Test.cxx",
                "desc": "asserts a normal CV write is rejected while locked"
              },
              {
                "name": "DccCvStorage.unlock_then_write_succeeds",
                "file": "dcc_cv_storage_Test.cxx",
                "desc": "asserts unlocking then writing succeeds"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.2-DEC-003",
          "feature": "Factory reset (CV8) to defaults",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.2",
            "cite": "CV8 Manufacturer ID (write value 8 resets CVs to factory defaults)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "partial",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "nobs",
            "note": ""
          },
          "detail": {
            "impl": "write special-cases CV8==8 to bypass the lock and forwards it to cv_write; no in-library default restoration. Only a write-permission exception; no reset-to-defaults logic in the library.",
            "gtest": "",
            "hil": "Host-only."
          },
          "refs": {
            "symbols": [
              "DccCvStorage_write",
              "DCC_CV_MANUFACTURER_ID"
            ],
            "tests": [
              {
                "name": "DccCvStorage.factory_reset_allowed_when_locked",
                "file": "dcc_cv_storage_Test.cxx",
                "desc": "asserts writing 8 to CV8 is forwarded even while locked"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.2-DEC-004",
          "feature": "CV29 configuration decode",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.2",
            "cite": "CV29 Configuration (bit 0 direction, bit 1 speed-steps, bit 5 extended address)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: CV29 is a bit-flag register selecting direction, FL location, power-source conversion, RailCom, speed table, and long/short address. The draft's Table 4 nails down the disputed mid bits and adds a high bit: bit1 'FL Location' (0 = bit4 of Speed-and-Direction controls FL, 1 = bit4 of Function Group One controls FL); bit2 'Power Source Conversion' (0 = NMRA Digital Only, 1 = enabled, target in CV12); bit5 = use extended address (CV17/18); bit6 reserved; and the new bit7 'Accessory Decoder Control Type' (0 = Multifunction, 1 = Accessory). When bit7 = 1 the meaning of bits 0-6 switches to the accessory layout (CV541/Table 11). Implementer impact: bit7 selects which CV29 bit-map applies, so decode must branch on it, and a bit must not be settable if the feature is unsupported."
            }
          },
          "supported": {
            "state": "partial",
            "note": ""
          },
          "gtest": {
            "state": "no",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "_update_address_cv_cache caches direction/speed-step/extended-address from CV29; other bits are ignored. Only bits 0/1/5 acted on; bits 2 (analog), 3 (RailCom), 4 (speed table) not acted on.",
            "gtest": "No host test covers CV29 bit interpretation. No host test exercises CV29 bit interpretation.",
            "hil": "Address/speed-mode behavior tested on the wire elsewhere."
          },
          "refs": {
            "symbols": [
              "DCC_CV29_DIRECTION_BIT",
              "DCC_CV29_SPEED_STEPS_BIT",
              "DCC_CV29_EXTENDED_ADDRESS_BIT"
            ],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.2-DEC-005",
          "feature": "Indexed CVs (CV31/CV32 paging)",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.2",
            "cite": "Indexed area (CV31/CV32 select a page over CVs 257-512)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: CV31 (high) and CV32 (low) form a page pointer for indexed CVs 257-512, low pages reserved for NMRA. The draft adds a concrete page-allocation map (Table 5) and widens the scheme: CV31 may be 0x10-0xFF (0x00-0x0F reserved = 4096 reserved pages), giving 61,440 indexed pages of 256 bytes. Within CV31=0 the banks are fixed windows: CV32=0 -> CVs 1-256, 1 -> 257-512 (otherwise inaccessible), 2 -> 513-768, 3 -> 769-1024 (SUSI from CV897), 40-43 -> function assignment per RCN-227, 254 -> read-only functionality bitmap, 255 -> RailCom page per RCN-217; CV31=1 is RailComPlus loco info, CV31=2 holds the 256 data-space pages per RCN-218. Implementer impact: indexed access becomes a first-class addressing window (CV1-1024 reachable even on 3-digit-only command stations), and a decoder must reject unimplemented pages/features rather than alias them."
            }
          },
          "supported": {
            "state": "no",
            "note": ""
          },
          "gtest": {
            "state": "no",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Only #define constants for CV31/CV32; no page-selected indexing. CV31/CV32 are #defines only; no paging logic.",
            "gtest": "No indexed-CV behavior exists to test. No tests for indexed CV paging.",
            "hil": "Not implemented."
          },
          "refs": {
            "symbols": [
              "DCC_CV_INDEX_HIGH",
              "DCC_CV_INDEX_LOW"
            ],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.2-DEC-006",
          "feature": "Accessory decoder CVs (513/521/541)",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.2",
            "cite": "Accessory Decoder CVs (CV513/521 address, CV541 config)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: accessory-decoder CVs lived in 513-1024 (CV513=address LSB, CV521=address MSB, CV541=config), with 1-512 reserved. The draft (section 1.3.3) relocates the canonical accessory CV definitions down into 1-512 (many shipped decoders already used low CVs and some command stations cannot reach above 512), leaving 513-1024 as optional, and fills the gaps: adds CV15-16 (decoder lock), CV17-18 (Mirrored Address, so the decoder is also reachable via a mobile address when CV29[541] bit7=0), and a full CV29[541] bit table (Table 11: bit3 RailCom, bit5 Basic vs Extended, bit6 Addressing Method, bit7 control type). It adds the Table 9 user-address-to-packet mapping: bit6=0 'Decoder-Address' stores the raw 9-bit address; bit6=1 'Output-Address' stores Output = User-3, split CV1 = Output mod 256, CV9 = Output div 256, where Linear makes Output == User except at 2048 and Non-Linear applies a 256 (mod 2048) offset. Implementer impact: support the lower CV image, branch address encode/decode on CV29[541] bit6, and apply the -3 offset and linear/non-linear convention."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "no",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "_update_accessory_address parses CV541 mode bits and combines CV513/CV521 into the cached address.",
            "gtest": "No host test exercises accessory CV513/521/541 decode. No host test for accessory CV decode.",
            "hil": "Accessory CV access on the wire covered under S-9.2.1."
          },
          "refs": {
            "symbols": [
              "DCC_CV_ACC_ADDRESS_LSB",
              "DCC_CV_ACC_ADDRESS_MSB",
              "DCC_CV_ACC_CONFIG"
            ],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_2_3",
      "spec": "S-9.2.3",
      "title": "Service mode",
      "features": [
        {
          "tid": "DCC-S9.2.3-CS-001",
          "feature": "ACK accepted at exact minimum sample count (85 samples / 5 ms)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section D (acknowledgment, 6 ms +/- 1 ms window)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "DccServiceModeCommon_ack_sample counts consecutive above-threshold samples and accepts a run >= MIN on the falling edge. DCC_ACK_MIN_SAMPLES = (5000/58)-1 = 85.",
            "gtest": "",
            "hil": "4930 us = 85 samples."
          },
          "refs": {
            "symbols": [
              "DccServiceModeCommon_ack_sample",
              "DCC_ACK_MIN_SAMPLES"
            ],
            "tests": [
              {
                "name": "DccServiceModeCommon.ack_exact_min_samples_detected",
                "file": "dcc_service_mode_common_Test.cxx",
                "desc": "asserts a run of exactly MIN samples is detected as a valid ACK"
              }
            ],
            "hilChecks": [
              {
                "label": "MIN (85 samples / 4930us)",
                "file": "s9_2_3_compliance.py",
                "desc": "injects a real 4930 us pulse and expects ACK DETECTED"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-002",
          "feature": "ACK rejected one sample below minimum (84 samples)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section D (acknowledgment window)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "A run shorter than DCC_ACK_MIN_SAMPLES on the falling edge is not accepted.",
            "gtest": "",
            "hil": "4872 us = 84 samples."
          },
          "refs": {
            "symbols": [
              "DccServiceModeCommon_ack_sample",
              "DCC_ACK_MIN_SAMPLES"
            ],
            "tests": [
              {
                "name": "DccServiceModeCommon.ack_one_below_min_samples_not_detected",
                "file": "dcc_service_mode_common_Test.cxx",
                "desc": "asserts a run of MIN-1 samples is rejected"
              }
            ],
            "hilChecks": [
              {
                "label": "MIN-1 (84 samples / 4872us)",
                "file": "s9_2_3_compliance.py",
                "desc": "injects a 4872 us pulse and expects NO ACK"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-003",
          "feature": "ACK accepted within maximum window (120 samples / 7 ms)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section D (acknowledgment window)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "A run whose length lands within [MIN, MAX] inclusive on the falling edge is accepted. DCC_ACK_MAX_SAMPLES = 7000/58 = 120.",
            "gtest": "",
            "hil": "6960 us = 120 samples = MAX."
          },
          "refs": {
            "symbols": [
              "DccServiceModeCommon_ack_sample",
              "DCC_ACK_MAX_SAMPLES"
            ],
            "tests": [
              {
                "name": "DccServiceModeCommon.ack_within_max_window_detected",
                "file": "dcc_service_mode_common_Test.cxx",
                "desc": "asserts a run inside the max window is detected"
              }
            ],
            "hilChecks": [
              {
                "label": "MAX (120 samples / 6960us)",
                "file": "s9_2_3_compliance.py",
                "desc": "injects a 6960 us pulse and expects ACK DETECTED"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-004",
          "feature": "ACK rejected beyond max / over-current (121+ samples)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section D (acknowledgment window)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "A run exceeding DCC_ACK_MAX_SAMPLES, or one ever flagged over-current, is rejected.",
            "gtest": "",
            "hil": "7018 us = 121 samples overrun."
          },
          "refs": {
            "symbols": [
              "DccServiceModeCommon_ack_sample",
              "DCC_ACK_MAX_SAMPLES"
            ],
            "tests": [
              {
                "name": "DccServiceModeCommon.ack_beyond_max_window_not_detected",
                "file": "dcc_service_mode_common_Test.cxx",
                "desc": "asserts a run past the max window is rejected as overrun"
              }
            ],
            "hilChecks": [
              {
                "label": "MAX+1 (121 samples / 7018us, overrun)",
                "file": "s9_2_3_compliance.py",
                "desc": "injects a 7018 us pulse and expects NO ACK (overrun)"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-005",
          "feature": "Interrupted current pulse resets the ACK width counter",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section D (acknowledgment pulse measurement)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "ack_sample treats a low run longer than the dropout tolerance as a real falling edge and restarts the count.",
            "gtest": "",
            "hil": "Mock-ACK loopback injects an INTERRUPTED pulse (3000 us high, ~232 us low, 3000 us high) via SVC MOCKACK <us> GLITCH <gap>. Each sub-pulse is below the MIN ACK width, so the verdict NO ACK (cross-checked against two sub-pulses on D4) proves the counter reset on the gap instead of accumulating ~6000 us. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccServiceModeCommon_ack_sample",
              "DCC_ACK_DROPOUT_SAMPLES"
            ],
            "tests": [
              {
                "name": "DccServiceModeCommon.interrupted_pulse_resets_counter",
                "file": "dcc_service_mode_common_Test.cxx",
                "desc": "asserts a deep low gap resets the width counter"
              }
            ],
            "hilChecks": [
              {
                "label": "interrupted mock-ACK pulse -> NO ACK (counter reset)",
                "file": "s9_2_3_compliance.py",
                "desc": "glitched pulse (3000us/gap/3000us): NO-ACK verdict + two sub-pulses on D4 prove the width counter resets on the gap rather than summing to an in-window 6000us."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-006",
          "feature": "ACK sampled only in COMMAND scan window",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section D (begin scanning at Packet End bit of second packet)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "ack_sample ignores current until the scan window opens and after the command phase, so pre/post-reset current never changes the result. Opens after DCC_SERVICE_MODE_ACK_BLANK_PACKETS.",
            "gtest": "",
            "hil": "In-window vs EARLY."
          },
          "refs": {
            "symbols": [
              "DccServiceModeCommon_ack_sample",
              "DCC_SERVICE_MODE_ACK_BLANK_PACKETS"
            ],
            "tests": [
              {
                "name": "DccServiceModeCommon.ack_during_pre_reset_ignored",
                "file": "dcc_service_mode_common_Test.cxx",
                "desc": "asserts current during pre-reset is ignored"
              },
              {
                "name": "DccServiceModeCommon.ack_during_post_reset_does_not_change_result",
                "file": "dcc_service_mode_common_Test.cxx",
                "desc": "asserts current during post-reset does not flip the result"
              }
            ],
            "hilChecks": [
              {
                "label": "EARLY",
                "file": "s9_2_3_compliance.py",
                "desc": "injects an in-window vs EARLY pulse and verifies only the in-window one is honored"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-007",
          "feature": "Pre-operation reset = 3 packets",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (packet sequence, pre-op resets)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "The common sequencer sends the configured pre-reset count of reset packets before the command phase.",
            "gtest": "",
            "hil": "_count_resets >= 3."
          },
          "refs": {
            "symbols": [
              "DccServiceModeCommon_run",
              "DCC_SERVICE_MODE_RESET_PRE_COUNT"
            ],
            "tests": [
              {
                "name": "DccServiceModeCommon.pre_reset_sends_correct_count",
                "file": "dcc_service_mode_common_Test.cxx",
                "desc": "asserts exactly 3 pre-reset packets are emitted"
              }
            ],
            "hilChecks": [
              {
                "label": "_count_resets",
                "file": "s9_2_3_compliance.py",
                "desc": "counts reset packets on the service wire and asserts >= 3"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-008",
          "feature": "Post-operation reset = 6 packets",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (packet sequence, post-op resets)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "The common sequencer sends the configured post-reset count after the operation completes.",
            "gtest": "",
            "hil": "s9_2_3: the final reset run before the service track idles is 6 packets — the RESET_POST block. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccServiceModeCommon_run",
              "DCC_SERVICE_MODE_RESET_POST_COUNT"
            ],
            "tests": [
              {
                "name": "DccServiceModeCommon.post_reset_sends_correct_packet_count",
                "file": "dcc_service_mode_common_Test.cxx",
                "desc": "asserts exactly 6 post-reset packets are emitted"
              }
            ],
            "hilChecks": [
              {
                "label": "post-operation RESET_POST = 6 packets (wire)",
                "file": "s9_2_3_compliance.py",
                "desc": "trailing reset run before idle = 6."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-009",
          "feature": "ACK terminates command phase early",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (Decoder-Recovery-Time ends early on ACK)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "When a valid ACK is detected the sequencer ends the command/recovery phase without sending the remaining repeats.",
            "gtest": "",
            "hil": "Wall-time differential on the real ACK path (mock decoder): a Direct read whose every bit ACKs (CV=0xFF) completes faster than one with no ACK (CV=0x00) — identical packet count, the only difference is each per-bit recovery cut short by the ACK. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccServiceModeCommon_ack_sample",
              "DccServiceModeCommon_run"
            ],
            "tests": [
              {
                "name": "DccServiceModeCommon.ack_detected_terminates_command_phase_early",
                "file": "dcc_service_mode_common_Test.cxx",
                "desc": "asserts command phase ends early once ACK is detected"
              }
            ],
            "hilChecks": [
              {
                "label": "ACK cuts recovery short (read timing differential)",
                "file": "s9_2_3_compliance.py",
                "desc": "all-ACK Direct read (CV=0xFF) finishes measurably faster than the no-ACK read (CV=0x00); same packets, only recovery differs."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-010",
          "feature": "Service-mode packets use long (>=20-bit) preamble",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (long preamble for service mode)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "nobs",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "All service-mode command and reset packets are emitted with the long-preamble service-mode framing.",
            "gtest": "Preamble length is a wire property not asserted at host layer. Wire-timing property.",
            "hil": "_check_command on all modes."
          },
          "refs": {
            "symbols": [
              "DccServiceModeCommon_run"
            ],
            "tests": [],
            "hilChecks": [
              {
                "label": "_check_command",
                "file": "s9_2_3_compliance.py",
                "desc": "decodes service packets across all modes and verifies the >=20-bit preamble"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-011",
          "feature": "Register VERIFY uses 7 command repeats",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (Physical Register verify repeats)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "DccServiceModeRegister verify passes command_repeat=7 with zero recovery. DCC_SERVICE_MODE_REGISTER_VERIFY_REPEAT = 7.",
            "gtest": "",
            "hil": "Test_register min_repeat=7."
          },
          "refs": {
            "symbols": [
              "DccServiceModeRegister_verify",
              "DCC_SERVICE_MODE_REGISTER_VERIFY_REPEAT"
            ],
            "tests": [
              {
                "name": "DccServiceModeRegister.verify_uses_seven_repeats_zero_recovery",
                "file": "dcc_service_mode_register_Test.cxx",
                "desc": "asserts register verify issues 7 command repeats and 0 recovery"
              }
            ],
            "hilChecks": [
              {
                "label": "verify reg1=0",
                "file": "s9_2_3_compliance.py",
                "desc": "checks register verify command repeats >= 7 on the wire"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-012",
          "feature": "Register 1 / CV#1 write uses long 10-packet recovery",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (Register 1 special recovery time)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "Register writes to register 1 and Address-Only CV#1 writes pass the long 10-packet recovery count. DCC_SERVICE_MODE_RECOVERY_COUNT_LONG = 10.",
            "gtest": "",
            "hil": "s9_2_3: no-decoder REGISTER-1 write — between-command reset runs are 13 (= RECOVERY_LONG 10 + PRE 3), so register-1/CV#1 uses the 10-packet long recovery. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccServiceModeRegister_write",
              "DCC_SERVICE_MODE_RECOVERY_COUNT_LONG"
            ],
            "tests": [
              {
                "name": "DccServiceModeRegister.write_register1_uses_long_recovery",
                "file": "dcc_service_mode_register_Test.cxx",
                "desc": "asserts a write to register 1 uses the 10-packet recovery"
              },
              {
                "name": "DccServiceModeAddress.write_passes_write_flag_and_long_recovery",
                "file": "dcc_service_mode_address_Test.cxx",
                "desc": "asserts Address-Only CV#1 write uses long recovery"
              }
            ],
            "hilChecks": [
              {
                "label": "register-1 long recovery = 10 packets (wire)",
                "file": "s9_2_3_compliance.py",
                "desc": "between-command reset runs = 13 (RECOVERY_LONG 10 + PRE 3); derived recovery = 10."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-013",
          "feature": "Standard write recovery = 6 packets",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (standard Decoder-Recovery-Time)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "Register writes to registers other than 1 pass the standard 6-packet recovery count.",
            "gtest": "",
            "hil": "s9_2_3: no-decoder DIRECT write — the reset run between consecutive write-byte command groups on ch3 is 9 (= RECOVERY 6 + PRE 3), so the Decoder-Recovery-Time is 6 packets. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccServiceModeRegister_write",
              "DCC_SERVICE_MODE_RECOVERY_COUNT"
            ],
            "tests": [
              {
                "name": "DccServiceModeRegister.write_register2_uses_standard_recovery",
                "file": "dcc_service_mode_register_Test.cxx",
                "desc": "asserts a write to register 2 uses the standard 6-packet recovery"
              }
            ],
            "hilChecks": [
              {
                "label": "standard write recovery = 6 packets (wire)",
                "file": "s9_2_3_compliance.py",
                "desc": "between-command reset runs = 9 (RECOVERY 6 + PRE 3); derived recovery = 6."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-014",
          "feature": "Direct mode exact bytes (write/verify byte and bit)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (Direct mode packet 0111CCAA)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "DccServiceModeDirect emits the 4-byte Direct packet with correct CC bits for write byte (0x7C), verify byte (0x74), and 111KDBBB bit byte.",
            "gtest": "",
            "hil": "Test_direct 3 instr types."
          },
          "refs": {
            "symbols": [
              "DccServiceModeDirect_write_byte",
              "DccServiceModeDirect_verify_byte",
              "DccServiceModeDirect_write_bit",
              "DccServiceModeDirect_verify_bit"
            ],
            "tests": [
              {
                "name": "DccServiceModeDirect.write_byte_cv1_value_0x55",
                "file": "dcc_service_mode_direct_Test.cxx",
                "desc": "asserts exact write-byte packet bytes for CV1=0x55"
              },
              {
                "name": "DccServiceModeDirect.verify_bit_cv1_bit5_high",
                "file": "dcc_service_mode_direct_Test.cxx",
                "desc": "asserts exact bit-verify byte for CV1 bit5=1"
              }
            ],
            "hilChecks": [
              {
                "label": "write-byte CV3=5 (CC=11)",
                "file": "s9_2_3_compliance.py",
                "desc": "decodes Direct write/verify/bit instruction types on the wire"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-015",
          "feature": "Direct mode 10-bit CV addressing (CV 1..1024)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (Direct mode 10-bit CV address)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "DccServiceModeDirect packs the 10-bit CV address across byte0 AA and byte1 AAAAAAAA. CV# = address + 1.",
            "gtest": "",
            "hil": "Test_direct CV#1/#3/#1023."
          },
          "refs": {
            "symbols": [
              "DccServiceModeDirect_write_byte"
            ],
            "tests": [
              {
                "name": "DccServiceModeDirect.write_byte_cv1024_value_0xAA",
                "file": "dcc_service_mode_direct_Test.cxx",
                "desc": "asserts CV1024 splits correctly into the 10-bit address field"
              }
            ],
            "hilChecks": [
              {
                "label": "write-byte CV1023 (AA=11)",
                "file": "s9_2_3_compliance.py",
                "desc": "decodes high-CV Direct addressing on the wire"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-016",
          "feature": "Register/Address-Only commands prepend a page-preset",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (page-preset before Register/Address access)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "Register and Address modes run a page-preset chain before emitting the command. Page register -> page 1 (7D 01 7C).",
            "gtest": "",
            "hil": "Test_register, test_address."
          },
          "refs": {
            "symbols": [
              "DccServiceModeRegister_write",
              "DccServiceModeAddress_write"
            ],
            "tests": [
              {
                "name": "DccServiceModeRegister.write_starts_with_page_preset",
                "file": "dcc_service_mode_register_Test.cxx",
                "desc": "asserts register write begins with the page-preset packet"
              },
              {
                "name": "DccServiceModeAddress.write_starts_with_page_preset",
                "file": "dcc_service_mode_address_Test.cxx",
                "desc": "asserts Address-Only write begins with the page-preset packet"
              }
            ],
            "hilChecks": [
              {
                "label": "write reg1=5",
                "file": "s9_2_3_compliance.py",
                "desc": "decodes the register command sequence including the page-preset"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-017",
          "feature": "Page-preset NO_ACK still proceeds to command",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section D (page-select not gated by ACK)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "The page-preset completes by ACK or packet count and never gates the command step. ACK optional per spec line 215.",
            "gtest": "",
            "hil": "With no decoder attached the page-preset gets NO ACK, yet a register write on the service track (ch3) still shows the page-preset packet followed by the command packet — the page-select is not gated by ACK. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccServiceModeRegister_write",
              "DccServiceModePaged_write"
            ],
            "tests": [
              {
                "name": "DccServiceModeRegister.command_proceeds_even_if_preset_no_ack",
                "file": "dcc_service_mode_register_Test.cxx",
                "desc": "asserts the register command proceeds when the page-preset gets no ACK"
              },
              {
                "name": "DccServiceModePaged.write_page_select_no_ack_still_proceeds",
                "file": "dcc_service_mode_paged_Test.cxx",
                "desc": "asserts paged data step proceeds when the page-select gets no ACK"
              }
            ],
            "hilChecks": [
              {
                "label": "page-preset (un-ACKed) is followed by the command on the wire",
                "file": "s9_2_3_compliance.py",
                "desc": "register write with no decoder: page-preset packet then the command packet both on ch3, proving the preset is not ACK-gated."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-018",
          "feature": "Paged mode page-write then data-register command",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (Paged mode Paging then Data Register)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "DccServiceModePaged emits the page-select packet then a data-register access packet computed from the CV number. CV# = (page-1)*4 + data_register + 1.",
            "gtest": "",
            "hil": "Test_paged."
          },
          "refs": {
            "symbols": [
              "DccServiceModePaged_write",
              "DccServiceModePaged_verify"
            ],
            "tests": [
              {
                "name": "DccServiceModePaged.write_cv1_page_select_packet",
                "file": "dcc_service_mode_paged_Test.cxx",
                "desc": "asserts the page-select packet bytes for CV1"
              },
              {
                "name": "DccServiceModePaged.write_cv1_data_access_after_page_select_success",
                "file": "dcc_service_mode_paged_Test.cxx",
                "desc": "asserts the data-register access packet follows a successful page-select"
              }
            ],
            "hilChecks": [
              {
                "label": "data-register write CV1=42",
                "file": "s9_2_3_compliance.py",
                "desc": "decodes the paged page-write plus data-register command"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-019",
          "feature": "Register-to-CV mapping (Mobile and Accessory)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (Register-to-CV mapping table)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "nobs",
            "note": ""
          },
          "detail": {
            "impl": "DccServiceModeTaskRegister maps a CV number to its physical register using separate Mobile and Accessory tables. Decoder_type per-call.",
            "gtest": "",
            "hil": "The CV-to-register mapping is internal task-layer logic; only the resulting register command byte reaches the wire (covered by the register-mode command vectors). The mapping table itself is host-tested, not separately wire-observable."
          },
          "refs": {
            "symbols": [
              "DccServiceModeTaskRegister_write_cv",
              "DccServiceModeTaskRegister_read_cv"
            ],
            "tests": [
              {
                "name": "DccServiceModeTaskRegister.mobile_cv1_maps_to_register_1",
                "file": "dcc_service_mode_task_register_Test.cxx",
                "desc": "asserts Mobile CV1 maps to register 1"
              },
              {
                "name": "DccServiceModeTaskRegister.accessory_cv520_maps_to_register_8",
                "file": "dcc_service_mode_task_register_Test.cxx",
                "desc": "asserts Accessory CV520 maps to register 8"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-020",
          "feature": "Decoder factory reset writes 8 to Register 8 (7F 08 77)",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (Decoder Factory Reset)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "DccServiceModeTaskRegister_factory_reset issues a register-8 value-8 write following the register sequence. 01111111 00001000 01110111.",
            "gtest": "",
            "hil": "Test_register factory reset."
          },
          "refs": {
            "symbols": [
              "DccServiceModeTaskRegister_factory_reset"
            ],
            "tests": [
              {
                "name": "DccServiceModeTaskRegister.factory_reset_writes_8_to_register_8",
                "file": "dcc_service_mode_task_register_Test.cxx",
                "desc": "asserts factory reset writes value 8 to register 8"
              }
            ],
            "hilChecks": [
              {
                "label": "factory reset",
                "file": "s9_2_3_compliance.py",
                "desc": "decodes the register-8 value-8 factory-reset command"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-021",
          "feature": "read_cv iterates verify; ACK returns the found value",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (read via Verify, iterate to find value)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "The task modules iterate verify 0..N and return the value at the first ACK.",
            "gtest": "",
            "hil": "Direct read on the service track (ch3) with the mock decoder holding CV8=0x5A: the per-bit verify packets (verify_bit cv,b,1 for b=0..7) appear on the wire — the iteration is externally visible — and the read returns 0x5A through the real ACK path. (Register/address byte-search iteration stays host-only: the mock decoder only ACKs Direct-format verifies.) (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccServiceModeTaskRegister_read_cv",
              "DccServiceModeTaskAddress_read"
            ],
            "tests": [
              {
                "name": "DccServiceModeTaskRegister.read_cv_ack_on_value_42_returns_42",
                "file": "dcc_service_mode_task_register_Test.cxx",
                "desc": "asserts iteration stops and returns 42 when the decoder ACKs that value"
              },
              {
                "name": "DccServiceModeTaskAddress.read_ack_on_address_3_returns_3",
                "file": "dcc_service_mode_task_address_Test.cxx",
                "desc": "asserts address read returns 3 on ACK"
              }
            ],
            "hilChecks": [
              {
                "label": "Direct read: per-bit verify iteration visible + ACK returns the value",
                "file": "s9_2_3_compliance.py",
                "desc": "mock holds CV8=0x5A; >=3 of the 8 verify-bit packets seen on ch3 and the op returns 0x5A via the real ACK path."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-022",
          "feature": "write_cv performs write then verify read-back",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (write then verify)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "The task write_cv runs a WRITE phase then a VERIFY phase against the same value.",
            "gtest": "",
            "hil": "Direct write on the service track (ch3): the write-byte packet's first appearance precedes the verify-byte read-back packet's (timestamp ordering from the capture), and the op completes SUCCESS through the real ACK path (mock accepts the write). (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccServiceModeTaskRegister_write_cv",
              "DccServiceModeTaskAddress_write"
            ],
            "tests": [
              {
                "name": "DccServiceModeTaskRegister.write_cv_verifies_after_write_complete",
                "file": "dcc_service_mode_task_register_Test.cxx",
                "desc": "asserts a verify is issued after the write completes"
              }
            ],
            "hilChecks": [
              {
                "label": "write-byte precedes verify-byte on the wire (phase order)",
                "file": "s9_2_3_compliance.py",
                "desc": "Direct write CV8=51: write-byte packet timestamped before the verify-byte read-back; op returns SUCCESS via the ACK path."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-023",
          "feature": "Bit ops in non-Direct modes use read-modify-write",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (bit access via byte read-modify-write)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "nobs",
            "note": ""
          },
          "detail": {
            "impl": "Non-Direct write_bit reads the byte, modifies the bit, writes back, and verifies.",
            "gtest": "",
            "hil": "Read-modify-write is task-layer; not measured on the wire. Task-layer logic, host-only."
          },
          "refs": {
            "symbols": [
              "DccServiceModeTaskRegister_write_bit",
              "DccServiceModeTaskAddress_write_bit"
            ],
            "tests": [
              {
                "name": "DccServiceModeTaskRegister.write_bit_scans_then_writes_with_bit_set",
                "file": "dcc_service_mode_task_register_Test.cxx",
                "desc": "asserts register write_bit scans then writes back with the bit set"
              },
              {
                "name": "DccServiceModeTaskAddress.write_bit_clears_bit",
                "file": "dcc_service_mode_task_address_Test.cxx",
                "desc": "asserts address write_bit clears the target bit"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-024",
          "feature": "Read-back / write-back value correctness against a held CV",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (read via Verify, value correctness)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "partial",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "The mock decoder holds a CV value and ACKs only on a matching verify, so read returns the stored value and write-then-read round-trips.",
            "gtest": "Host tests assert iteration/verify logic but not a full held-value round-trip. Host tests verify per-step logic, not end-to-end held value.",
            "hil": "Mock decoder SVC MOCKCV."
          },
          "refs": {
            "symbols": [
              "DccServiceModeTaskDirect_read_cv",
              "DccServiceModeTaskDirect_write_cv"
            ],
            "tests": [
              {
                "name": "DccServiceModeTaskRegister.read_cv_ack_on_value_42_returns_42",
                "file": "dcc_service_mode_task_register_Test.cxx",
                "desc": "asserts read returns the value the decoder ACKs (per-step proxy)"
              }
            ],
            "hilChecks": [
              {
                "label": "SVC MOCKCV",
                "file": "s9_2_3_compliance.py",
                "desc": "holds a CV value on a mock decoder and checks read-back and write-back correctness"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-025",
          "feature": "Parallel main + service track timing within tolerance",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (service mode bit timing) with S-9.1 half-bit tolerance",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "nobs",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "Both track pin toggles fire at the top of the shared 58us ISR before state processing, so task overhead cannot stretch either channel. Shared 58us ISR, toggle-first design.",
            "gtest": "Half-bit timing is a wire property not assertable at host. Wire-timing property.",
            "hil": "Check_track_timing every op."
          },
          "refs": {
            "symbols": [
              "DccConfig_58us_timer_isr"
            ],
            "tests": [],
            "hilChecks": [
              {
                "label": "check_track_timing",
                "file": "s9_2_3_compliance.py",
                "desc": "captures main and service tracks simultaneously during each op and verifies half-bit periods on both"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.2.3-CS-026",
          "feature": "Address-Only mode accesses CV#1 (7-bit) only",
          "role": "cs",
          "ref": {
            "spec": "S-9.2.3",
            "cite": "Section E (Address-Only mode CV#1 only)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "DccServiceModeAddress emits the Address-Only packet writing/verifying only the 7-bit CV#1 and rejects out-of-range. Address range 1..127.",
            "gtest": "",
            "hil": "Test_address."
          },
          "refs": {
            "symbols": [
              "DccServiceModeAddress_write",
              "DccServiceModeAddress_verify"
            ],
            "tests": [
              {
                "name": "DccServiceModeAddress.write_address_127",
                "file": "dcc_service_mode_address_Test.cxx",
                "desc": "asserts the Address-Only write packet for max address 127"
              },
              {
                "name": "DccServiceModeAddress.write_address_128_rejected",
                "file": "dcc_service_mode_address_Test.cxx",
                "desc": "asserts addresses beyond 7-bit range are rejected"
              }
            ],
            "hilChecks": [
              {
                "label": "write CV#1 addr=5",
                "file": "s9_2_3_compliance.py",
                "desc": "decodes the Address-Only CV#1 command on the wire"
              }
            ]
          }
        }
      ]
    },
    {
      "id": "s9_2_4",
      "spec": "S-9.2.4",
      "title": "Fail-safe",
      "features": [
        {
          "tid": "DCC-S9.2.4-DEC-001",
          "feature": "Packet-timeout fail-safe (CV11) stops device",
          "role": "dec",
          "ref": {
            "spec": "S-9.2.4",
            "cite": "Fail-safe (CV11 Packet Time-out, controlled stop on loss of packets)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "no",
            "note": ""
          },
          "gtest": {
            "state": "no",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Unimplemented: on_failsafe_* callbacks are declared but never called, and CV11 is defined only. Only dead callback declarations and an unreferenced DCC_CV_PACKET_TIMEOUT define; no timer or stop logic.",
            "gtest": "No gtest coverage; the on_failsafe_* callbacks have zero call sites. No tests exercise failsafe; symbols never invoked.",
            "hil": "No HIL coverage; nothing to drive. Feature unimplemented."
          },
          "refs": {
            "symbols": [
              "DCC_CV_PACKET_TIMEOUT"
            ],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_3_2",
      "spec": "S-9.3.2",
      "title": "RailCom / bi-directional communication",
      "features": [
        {
          "tid": "DCC-S9.3.2-CS-001",
          "feature": "Cutout start window T_CS (~26 us from end-bit)",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 3.2 (cutout timing, T_CS)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: the 2012 S-9.3.2 already requires the cutout to begin 26-32 us after the zero crossing of the last DCC end bit, but states it in prose. The draft (2026-04-10) formalizes this as parameter T_CS in Table 1 (Message timing parameters), 'Cut-out Start', Min 26 us / Max 32 us, explicitly measured from the end-bit zero crossing; the cutout device must drop no more than 10 mV at the 34 mA max loop current. Implementer impact: firmware can assert the cutout strobe against a single tabulated window (26-32 us) rather than an inferred value, and HIL timing keys off the end-bit zero crossing as t=0."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "DELAY state (26 us default) tristates the H-bridge to begin the cutout.",
            "gtest": "",
            "hil": "On-wire envelope."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CUTOUT_START_DELAY_US"
            ],
            "tests": [
              {
                "name": "DccRailcomCutout.begin_starts_delay_timer",
                "file": "dcc_railcom_cutout_Test.cxx",
                "desc": "begin arms the DELAY one-shot timer that opens the cutout"
              }
            ],
            "hilChecks": [
              {
                "label": "T_CS 26-32 us",
                "file": "s9_3_2_compliance.py",
                "desc": "measures end-bit edge to cutout-active rising edge against 26-32 us band"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-002",
          "feature": "Cutout end window T_CE (~454 us from end-bit)",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 3.2 (cutout timing, T_CE)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: the cutout must terminate ~454 us after the end bit, expressed only in prose in 2012. The draft Table 1 formalizes this as T_CE, 'Cut-out End', Min 454 us / Max 488 us, measured from the end-bit zero crossing; the booster must restore a valid DCC signal immediately after T_CE. A footnote notes a max-length cutout overlaps about five nominal 116 us one-bits, but since the command station/booster controls both, a compliant implementation resolves it. Implementer impact: the cutout-end edge has a defined 454-488 us band and the booster must resume normal drive right after, so verification can assert both the closing edge and prompt restoration."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "CH2 state expiry (cumulative 454 us) restores the H-bridge and ends the cutout.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CH2_WINDOW_US"
            ],
            "tests": [
              {
                "name": "DccRailcomCutout.per_state_timer_periods_are_correct",
                "file": "dcc_railcom_cutout_Test.cxx",
                "desc": "asserts each state period so cumulative T_CE lands at 454 us"
              }
            ],
            "hilChecks": [
              {
                "label": "T_CE 454-488 us",
                "file": "s9_3_2_compliance.py",
                "desc": "measures end-bit edge to cutout-active falling edge against 454-488 us band"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-003",
          "feature": "Cutout window duration T_CE-T_CS within tolerance",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 3.2 (Table 1 cutout window)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: per-channel cutout-window durations were described approximately, derived from the overall ~450 us cutout. The draft Table 1 replaces the approximations with precise per-channel windows: Channel 1 occupies T_TS1=80 us to T_TC1=177 us (97 us; 16 bits need 64 us at 250 kbit/s), and Channel 2 occupies T_TS2=193 us to T_TC2=454 us (261 us; up to 48 bits need ~192 us). The full cutout spans T_CS (26-32 us) to T_CE (454-488 us). Implementer impact: detectors can validate each channel's bytes fall inside its exact window, and the 97/261 us budgets bound how many code words each channel can carry."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "partial",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "Sum of DELAY..CH2 periods yields the 422-462 us active cutout window.",
            "gtest": "Host asserts individual state periods; the derived window is checked on the wire. Window implied by per-state period tests.",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CUTOUT_START_DELAY_US"
            ],
            "tests": [
              {
                "name": "DccRailcomCutout.per_state_timer_periods_are_correct",
                "file": "dcc_railcom_cutout_Test.cxx",
                "desc": "per-state periods sum to the 422-462 us window"
              }
            ],
            "hilChecks": [
              {
                "label": "window T_CE-T_CS",
                "file": "s9_3_2_compliance.py",
                "desc": "derived window 422-462 us"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-004",
          "feature": "One cutout per packet",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 3.1 (one cutout per packet)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "partial",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "Cutout is armed once per transmitted packet via the scheduler dispatch hook.",
            "gtest": "No host count-invariant test; HIL matches cutout count to packet count. Re-arm covered, not the per-packet count invariant.",
            "hil": ""
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CUTOUT_START_DELAY_US"
            ],
            "tests": [
              {
                "name": "DccRailcomCutout.second_cutout_after_first_succeeds",
                "file": "dcc_railcom_cutout_Test.cxx",
                "desc": "a fresh cutout can be armed after the previous one completes"
              }
            ],
            "hilChecks": [
              {
                "label": "one cutout window per packet",
                "file": "s9_3_2_compliance.py",
                "desc": "matches cutout window count to decoded packet count"
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-005",
          "feature": "Full 5-state cutout sequence and event ordering",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 3.2 (cutout phases / channel windows)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: the 2012 standard describes a cutout with Channel 1 then Channel 2 but without a single tabulated timeline. The draft (section 2.5 + Table 1) lays out the full phased sequence against one t=0 (end-bit zero crossing): cutout opens at T_CS (26-32 us); a settling gap precedes Channel 1, which runs T_TS1=80 us to T_TC1=177 us (2 bytes); an inter-channel gap follows; Channel 2 runs T_TS2=193 us to T_TC2=454 us (up to 6 bytes); cutout closes at T_CE (454-488 us). Each byte is start bit + 8 code-word bits + stop bit, no parity, at 250 kbit/s. Implementer impact: the ordered start/end events give a deterministic state machine for generating/decoding the cutout, so an analyzer can flag any byte starting outside its phase markers."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "5-state machine DELAY->SETTLING->CH1->GAP->CH2 toggles UART Rx enable/disable and begin/end-cutout hooks in order.",
            "gtest": "",
            "hil": "The channel-window open/close hooks are mirrored to the RAILCOM_RX_WINDOW probe pin (ch5); combined with the PB2 cutout strobe (T_CS/T_CE) the Saleae times all five state boundaries and the suite asserts T_CS<T_TS1<T_TC1<T_TS2<T_CE on every cutout. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_UART_RX_DELAY_US",
              "DCC_RAILCOM_CH1_WINDOW_US",
              "DCC_RAILCOM_CH1_CH2_GAP_US"
            ],
            "tests": [
              {
                "name": "DccRailcomCutout.full_sequence_state_transitions_and_actions",
                "file": "dcc_railcom_cutout_Test.cxx",
                "desc": "verifies each transition fires the correct H-bridge/UART action"
              },
              {
                "name": "DccRailcomCutout.full_sequence_event_order",
                "file": "dcc_railcom_cutout_Test.cxx",
                "desc": "verifies the begin/Rx/end event order across the sequence"
              }
            ],
            "hilChecks": [
              {
                "label": "5-state event order on the wire (PB2 + RAILCOM_RX_WINDOW mirror)",
                "file": "s9_3_2_compliance.py",
                "desc": "mirror pin marks Ch1/Ch2 window open/close; suite asserts T_CS<T_TS1<T_TC1<T_TS2<T_CE per cutout."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-006",
          "feature": "Per-state default periods (26/54/97/16/261 us)",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 3.2 (default phase durations)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: per-phase default periods were treated as approximate (~26/54/97/16/261 us style derivations). The draft Table 1 supplies authoritative per-channel boundary values measured from the end-bit zero crossing: Start Ch1 T_TS1=80 us, End Ch1 T_TC1=177 us, Start Ch2 T_TS2=193 us, End Ch2 T_TC2=454 us, bounded by T_CS (26-32) and T_CE (454-488). These yield a 97 us Channel 1 window and a 261 us Channel 2 window at the nominal 250 kbit/s (4 us/bit) +/-2% rate. Implementer impact: firmware schedules each channel against fixed absolute offsets rather than relative default periods, and tooling can assert the exact 80/177/193/454 us marks."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "Default per-state durations DELAY 26, SETTLING 54, CH1 97, GAP 16, CH2 261.",
            "gtest": "",
            "hil": "From the RAILCOM_RX_WINDOW mirror edges (ch5) the suite computes each interior period and asserts SETTLING~54, CH1~97, GAP~16, CH2~263 us (the firmware-configured values; SETTLING/CH1/GAP are the spec defaults, DELAY/CH2 pre-adjusted for bench latency as the T_CS/T_CE checks already are). (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CH2_WINDOW_US"
            ],
            "tests": [
              {
                "name": "DccRailcomCutout.per_state_timer_periods_are_correct",
                "file": "dcc_railcom_cutout_Test.cxx",
                "desc": "each state programs the correct default timer period"
              }
            ],
            "hilChecks": [
              {
                "label": "per-state periods from the mirror (SETTLING/CH1/GAP/CH2)",
                "file": "s9_3_2_compliance.py",
                "desc": "interior periods derived from RAILCOM_RX_WINDOW edges checked against 54/97/16/263 us within tolerance."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-007",
          "feature": "User-configurable cutout timing (0=spec default)",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 3.2 (configurable cutout timing)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "Per-state periods are sourced from dcc_config_t, with 0 selecting the spec default.",
            "gtest": "",
            "hil": "Reconfigured at runtime over UART (RAILCOM TIMING ...): setting CH1=150 us shows ~150 on the PB18 mirror, and CH1=0 falls back to the 97 us spec default — proving custom values reach the wire and 0 selects the default. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CUTOUT_START_DELAY_US"
            ],
            "tests": [
              {
                "name": "DccRailcomCutout.initialize_stores_timing_fields",
                "file": "dcc_railcom_cutout_Test.cxx",
                "desc": "initialize captures configured timing fields"
              },
              {
                "name": "DccRailcomCutout.custom_timings_drive_each_state_period",
                "file": "dcc_railcom_cutout_Test.cxx",
                "desc": "custom config values drive each state's timer period"
              }
            ],
            "hilChecks": [
              {
                "label": "custom CH1 reaches the wire; 0 selects the default",
                "file": "s9_3_2_compliance.py",
                "desc": "RAILCOM TIMING sets CH1=150us (measured ~150 on PB18) then CH1=0 (measured ~97, the spec default)."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-008",
          "feature": "Cancel mid-cutout restores H-bridge",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 3.1 (cutout abort / H-bridge restore)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "cancel ends the cutout and restores the H-bridge from any state past DELAY.",
            "gtest": "",
            "hil": "RAILCOM CANCEL arms a one-shot cancel that the firmware fires early (SETTLING/CH1) in the next cutout; on PB2 exactly one cutout is truncated to a short pulse while the rest stay ~440 us — the H-bridge is restored mid-cutout. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CH1_WINDOW_US"
            ],
            "tests": [
              {
                "name": "DccRailcomCutout.cancel_during_settling_ends_cutout",
                "file": "dcc_railcom_cutout_Test.cxx",
                "desc": "cancel in SETTLING restores H-bridge"
              },
              {
                "name": "DccRailcomCutout.cancel_during_ch1_ends_cutout",
                "file": "dcc_railcom_cutout_Test.cxx",
                "desc": "cancel in CH1 restores H-bridge"
              }
            ],
            "hilChecks": [
              {
                "label": "one cutout truncated by a mid-cutout cancel (PB2)",
                "file": "s9_3_2_compliance.py",
                "desc": "RAILCOM CANCEL produces exactly one short PB2 pulse among ~440us cutouts, confirming the H-bridge is restored early."
              }
            ]
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-009",
          "feature": "4/8 codec encode (64 values + boundaries)",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 2.5 (4/8 code, encoding)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "nobs",
            "note": ""
          },
          "detail": {
            "impl": "encode_byte maps a 6-bit value to a fixed-weight 8-bit 4/8 code word.",
            "gtest": "",
            "hil": "Codeword content not decoded on the wire; pure logic proven on host. Not wire-observable."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CH1_MAX_BYTES"
            ],
            "tests": [
              {
                "name": "DccRailcomEncoder.encode_byte_value_0x00",
                "file": "dcc_railcom_encoder_Test.cxx",
                "desc": "encodes low boundary value 0x00"
              },
              {
                "name": "DccRailcomEncoder.round_trip_all_values",
                "file": "dcc_railcom_encoder_Test.cxx",
                "desc": "encode/decode round-trips all 64 values"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-010",
          "feature": "4/8 codec decode + invalid-codeword rejection",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 2.5 (4/8 code, decoding)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: the 2012 4-of-8 code table defined 64 data code words plus ACK (00001111 and 11110000) and BUSY (11100001), and marked 11000011 and 10000111 as 'not approved for use'. The draft (section 3, Table 2) keeps the two ACK words but adds an optional NACK control code word 00111100 ('Command or CV is not supported' / 'NO'), and explicitly drops BUSY: 11100001 is now Reserved and 'no longer supported', its function absorbed by NACK; 11000011 and 10000111 remain Reserved. Implementer impact: decoders/detectors must treat 00111100 as NACK and reject 11100001/11000011/10000111 rather than acting on BUSY; every valid byte still carries exactly four 1s and four 0s for single-bit-error detection."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "nobs",
            "note": ""
          },
          "detail": {
            "impl": "decode_byte inverts the 4/8 mapping and rejects invalid words.",
            "gtest": "",
            "hil": "Wire datagram content not decoded by the HIL suite. Not wire-observable."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_DECODE_INVALID"
            ],
            "tests": [
              {
                "name": "DccRailcomDecoder.decode_byte_value_0x00_codeword_0xAC",
                "file": "dcc_railcom_decoder_Test.cxx",
                "desc": "decodes valid code word 0xAC back to 0x00"
              },
              {
                "name": "DccRailcomDecoder.decode_byte_invalid_0x00",
                "file": "dcc_railcom_decoder_Test.cxx",
                "desc": "rejects an invalid code word"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-011",
          "feature": "ACK special code word 0xF0",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 2.5 (ACK code word)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "nobs",
            "note": ""
          },
          "detail": {
            "impl": "ACK is transmitted raw as 0xF0.",
            "gtest": "",
            "hil": "Not measured on the wire. Not wire-observable."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CODE_WORD_ACK"
            ],
            "tests": [
              {
                "name": "DccRailcomEncoder.send_code_word_ack_raw",
                "file": "dcc_railcom_encoder_Test.cxx",
                "desc": "emits ACK 0xF0 raw"
              },
              {
                "name": "DccRailcomDecoder.decode_byte_ack_0xF0",
                "file": "dcc_railcom_decoder_Test.cxx",
                "desc": "decodes 0xF0 as ACK"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-012",
          "feature": "NACK special code word 0x3C",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 2.5 (NACK code word)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "nobs",
            "note": ""
          },
          "detail": {
            "impl": "NACK is transmitted raw as 0x3C.",
            "gtest": "",
            "hil": "Not measured on the wire. Not wire-observable."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CODE_WORD_NACK"
            ],
            "tests": [
              {
                "name": "DccRailcomEncoder.send_code_word_nack_raw",
                "file": "dcc_railcom_encoder_Test.cxx",
                "desc": "emits NACK 0x3C raw"
              },
              {
                "name": "DccRailcomDecoder.decode_byte_nack_0x3C",
                "file": "dcc_railcom_decoder_Test.cxx",
                "desc": "decodes 0x3C as NACK"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-013",
          "feature": "Channel 1 two-byte datagram",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 2.6 (Channel 1 datagram)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "nobs",
            "note": ""
          },
          "detail": {
            "impl": "Ch1 is a 2-byte datagram carrying a 12-bit payload.",
            "gtest": "",
            "hil": "Channel content not decoded on the wire. Not wire-observable."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CH1_MAX_BYTES"
            ],
            "tests": [
              {
                "name": "DccRailcomDecoder.ch1_valid_2_bytes",
                "file": "dcc_railcom_decoder_Test.cxx",
                "desc": "assembles a valid 2-byte Ch1 datagram"
              },
              {
                "name": "DccRailcomEncoder.send_ch1_basic",
                "file": "dcc_railcom_encoder_Test.cxx",
                "desc": "encodes a basic Ch1 datagram"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-014",
          "feature": "Channel 2 multi-byte datagram",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 2.6 (Channel 2 datagram)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "nobs",
            "note": ""
          },
          "detail": {
            "impl": "Ch2 is a multi-byte (4+) datagram.",
            "gtest": "",
            "hil": "Channel content not decoded on the wire. Not wire-observable."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CH2_MAX_BYTES"
            ],
            "tests": [
              {
                "name": "DccRailcomDecoder.ch2_valid_4_bytes",
                "file": "dcc_railcom_decoder_Test.cxx",
                "desc": "assembles a valid 4-byte Ch2 datagram"
              },
              {
                "name": "DccRailcomEncoder.send_ch2_multiple_data_bytes",
                "file": "dcc_railcom_encoder_Test.cxx",
                "desc": "encodes a Ch2 datagram with multiple data bytes"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-CS-015",
          "feature": "Receive datagram buffer (depth-4 ring)",
          "role": "cs",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 2.6 (datagram reception)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "nobs",
            "note": ""
          },
          "detail": {
            "impl": "Decoder buffers received datagrams in a depth-4 ring, dropping the oldest on overflow.",
            "gtest": "",
            "hil": "Buffer behavior is host-side logic. Not wire-observable."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CH2_MAX_BYTES"
            ],
            "tests": [
              {
                "name": "DccRailcomDecoder.read_returns_buffered_datagram",
                "file": "dcc_railcom_decoder_Test.cxx",
                "desc": "read returns a previously buffered datagram"
              },
              {
                "name": "DccRailcomDecoder.buffer_overflow_drops_oldest",
                "file": "dcc_railcom_decoder_Test.cxx",
                "desc": "overflow drops the oldest buffered datagram"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-DEC-001",
          "feature": "Address feedback ADR1/ADR2 alternation",
          "role": "dec",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 4 (address feedback, ID 1/ID 2)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: decoders feed back their address in Channel 1 by alternating two datagrams, but the 2012 framing of the three address types was thin. The draft (section 6.2.1, Table 19) fully specifies the Basic Location Service: after every addressed mobile command, all non-addressed mobile decoders send one Channel 1 address datagram, alternating ADR1 (identifier 1) and ADR2 (identifier 2) over two successive cutouts, with exact bit layouts for a 7-bit base address (CV1), a 7-bit consist address (CV19, R = consist direction), and a 14-bit extended address (CV17/18). An optional Info1 datagram (identifier 3) can be interleaved (bit4 request-to-be-addressed, bit3 consist member, bit2 moving/standing, bit1 direction, bit0 rerailing direction). Implementer impact: alternate ADR1/ADR2 per address type and optionally add Info1; CV28 bit0 disables Channel 1 responses, CV28 bit2 enables dynamic replies to reduce collisions."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Address feedback alternates ADR1 (ID 1) and ADR2 (ID 2); host-tested only, since dcc_config.c leaves the encoder uart_write NULL so nothing transmits in a real build.",
            "gtest": "",
            "hil": "RailCom Tx integration is a prerequisite for on-wire coverage. Decoder Tx not wired to hardware."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_ID_ADR1_HIGH",
              "DCC_RAILCOM_ID_ADR2_LOW"
            ],
            "tests": [
              {
                "name": "DccApplicationDecoderRailcom.send_address_feedback_alternates",
                "file": "dcc_application_decoder_railcom_Test.cxx",
                "desc": "successive calls alternate ADR1/ADR2"
              },
              {
                "name": "DccApplicationDecoderRailcom.send_address_feedback_first_call_sends_adr1",
                "file": "dcc_application_decoder_railcom_Test.cxx",
                "desc": "first call emits ADR1"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-DEC-002",
          "feature": "POM response (ID 0)",
          "role": "dec",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 4 (POM response, ID 0)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: in the 2012 table, identifier 0 in Channel 2 carries the POM CV-readback/acknowledge response. The draft (section 4.7, Table 6) re-tabulates the full mobile Channel 2 identifier set and keeps POM as ID 0 (a 12-bit datagram for POM acknowledgement / CV verification). It clarifies the response protocol — e.g. a POM write to an unsupported CV first returns an ACK for the successful transaction, then a NACK for the unidentified CV, and ACK+NACK may be combined in one Channel 2 transmission. Only identifiers 0, 1, 2 are mandatory; 3-14 optional. Implementer impact: POM readback stays on ID 0 but must follow the tightened ACK-then-NACK ordering and the mandatory-vs-optional identifier rules."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "POM read result is returned as an ID-0 datagram; host-tested only, as uart_write is NULL so nothing transmits in a real build.",
            "gtest": "",
            "hil": "Requires a decoder-side Tx path that does not yet exist. Decoder Tx not wired to hardware."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_ID_POM"
            ],
            "tests": [
              {
                "name": "DccApplicationDecoderRailcom.send_pom_response_delegates",
                "file": "dcc_application_decoder_railcom_Test.cxx",
                "desc": "POM response delegates to the encoder as ID 0"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-DEC-003",
          "feature": "Dynamic data response (ID 7)",
          "role": "dec",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 4 (dynamic data, ID 7)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: identifier 7 (DYN) carried optional dynamic data, but 2012 defined only a handful of dynamic variables. The draft (sections 4.7/4.11, Tables 6 and 25) keeps DYN as ID 7 (an 18-bit datagram, sent up to 2x) and expands the Dynamic Variable set to 64 DVs, mapped to indexed CVs 321-384 and selected by a 6-bit sub-index. Defined DVs now include 0 = scale speed up to 255 km/h, 1 = speed overage above 255, 3 = RailCom version (e.g. 0x15 = v1.5), 4 = change flags, 7 = reception statistics (percent faulty packets), 8-19 = container fill levels 1-12, 20 = mobile address, 21 = warnings/alarms, 26 = temperature (-50 C offset), 27 = east/west direction; 2,5,6,22-25,28-63 reserved. Implementer impact: a decoder can report rich telemetry via DYN sub-index and a command station can request any of 64 DVs; DV20 is an alternate address-feedback path and DV21 encodes alarm vs warning plus the offending sub-index."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Dynamic data is emitted as an ID-7 datagram; host-tested only, as uart_write is NULL so nothing transmits in a real build.",
            "gtest": "",
            "hil": "Decoder-side Tx integration is the prerequisite. Decoder Tx not wired to hardware."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_ID_DYN"
            ],
            "tests": [
              {
                "name": "DccApplicationDecoderRailcom.send_dynamic_data_delegates",
                "file": "dcc_application_decoder_railcom_Test.cxx",
                "desc": "dynamic data delegates to the encoder as ID 7"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-DEC-004",
          "feature": "Track-search response (ID 1/2/14)",
          "role": "dec",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 4 (track-search, ID 1/2/14)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: 2012 had no formal Track Search location service. The draft (section 4.13 / 6.2.3, Table 22) adds Track Search to quickly identify a just-powered or re-railed locomotive. For 30 seconds after power-up, the decoder responds to a broadcast binary-state-control short-form instruction to address 0 (bytes 1101-1101 0000-0010, i.e. reserved binary state XF2 off) by sending three datagrams together in Channel 2: ADR1 (ID 1) and ADR2 (ID 2) carrying the active address (per Table 19), plus a Time datagram (ID 14) giving seconds since power-up so the command station can timestamp power-up. Note ID 1/2 are reused here in Channel 2 (normally Channel 1). Implementer impact: command stations issue the XF2-off broadcast within 30 s of energizing the loco, and decoders emit the ADR1+ADR2+Time triple in a single Channel 2 burst."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Track-search feedback via ID 1/2/14; host-tested only, since uart_write is NULL so nothing transmits in a real build.",
            "gtest": "",
            "hil": "Requires decoder-side Tx path. Decoder Tx not wired to hardware."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_ID_TIME"
            ],
            "tests": [
              {
                "name": "DccApplicationDecoderRailcom.send_track_search_delegates",
                "file": "dcc_application_decoder_railcom_Test.cxx",
                "desc": "track-search delegates to the encoder"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-DEC-005",
          "feature": "CV auto-transfer response (ID 12)",
          "role": "dec",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 4 (CV auto-transfer, ID 12)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "CV auto-transfer as an ID-12 datagram; host-tested only, as uart_write is NULL so nothing transmits in a real build.",
            "gtest": "",
            "hil": "Requires decoder-side Tx path. Decoder Tx not wired to hardware."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_ID_CV_AUTO"
            ],
            "tests": [
              {
                "name": "DccApplicationDecoderRailcom.send_cv_auto_transfer_delegates",
                "file": "dcc_application_decoder_railcom_Test.cxx",
                "desc": "CV auto-transfer delegates to the encoder as ID 12"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-DEC-006",
          "feature": "ACK / NACK as raw code words",
          "role": "dec",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 4 (ACK/NACK raw code words)",
            "origin": "released",
            "draftDelta": {
              "change": "Released baseline: the 2012 4-of-8 table provided ACK (00001111 / 11110000) and BUSY (11100001) as raw special code words; a NACK meaning existed in prose but the optional NACK code-word handling was loose. The draft (section 3, Table 2) cleans this up: ACK remains (both words), an optional NACK is assigned the raw code word 00111100 ('Command or CV is not supported' / 'NO'), and BUSY 11100001 is dropped (now Reserved, no longer supported) with its function folded into NACK; 11000011 and 10000111 stay Reserved. Implementer impact: a decoder transmits 00111100 for NACK and never the old BUSY pattern; a detector decodes 00111100 as NACK, treats 11100001 as reserved/ignored, and may receive a combined ACK-then-NACK in one Channel 2 transmission."
            }
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Decoder ACK/NACK are sent as raw code words; host-tested only, as uart_write is NULL so nothing transmits in a real build.",
            "gtest": "",
            "hil": "Requires decoder-side Tx path. Decoder Tx not wired to hardware."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CODE_WORD_ACK",
              "DCC_RAILCOM_CODE_WORD_NACK"
            ],
            "tests": [
              {
                "name": "DccApplicationDecoderRailcom.send_ack_sends_raw_code_word",
                "file": "dcc_application_decoder_railcom_Test.cxx",
                "desc": "ACK is emitted as a raw code word"
              },
              {
                "name": "DccApplicationDecoderRailcom.send_nack_sends_raw_code_word",
                "file": "dcc_application_decoder_railcom_Test.cxx",
                "desc": "NACK is emitted as a raw code word"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-DEC-007",
          "feature": "Raw datagram passthrough with byte-count clamp",
          "role": "dec",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 4 (raw datagram passthrough)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Raw passthrough forwards caller bytes and clamps the count; host-tested only, as uart_write is NULL so nothing transmits in a real build.",
            "gtest": "",
            "hil": "Requires decoder-side Tx path. Decoder Tx not wired to hardware."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_CH2_MAX_BYTES"
            ],
            "tests": [
              {
                "name": "DccApplicationDecoderRailcom.send_raw_delegates",
                "file": "dcc_application_decoder_railcom_Test.cxx",
                "desc": "raw response delegates the supplied bytes"
              },
              {
                "name": "DccApplicationDecoderRailcom.send_raw_clamps_count",
                "file": "dcc_application_decoder_railcom_Test.cxx",
                "desc": "raw response clamps an oversized byte count"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-ACC-001",
          "feature": "Accessory SRQ state machine (IDLE/PENDING/RESPONDING)",
          "role": "acc",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 5 (accessory SRQ state machine)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Accessory SRQ machine transitions IDLE->PENDING->RESPONDING; host-tested only, as uart_write is NULL so nothing transmits in a real build.",
            "gtest": "",
            "hil": "Requires an accessory-side Tx path that does not yet exist. Decoder Tx not wired to hardware."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_ID_ADR1_HIGH"
            ],
            "tests": [
              {
                "name": "AccDecoderRailcomTest.send_srq_sets_pending_state",
                "file": "dcc_application_accessory_decoder_railcom_Test.cxx",
                "desc": "SRQ moves state IDLE->PENDING"
              },
              {
                "name": "AccDecoderRailcomTest.on_stop_command_transitions_pending_to_responding",
                "file": "dcc_application_accessory_decoder_railcom_Test.cxx",
                "desc": "stop command moves PENDING->RESPONDING"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-ACC-002",
          "feature": "Full-address SRQ, basic vs extended format",
          "role": "acc",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 5 (full-address SRQ)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "SRQ carries the full address and the Ch1 payload differs for basic vs extended; host-tested only, as uart_write is NULL so nothing transmits in a real build.",
            "gtest": "",
            "hil": "Requires accessory-side Tx path. Decoder Tx not wired to hardware."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_ID_EXT"
            ],
            "tests": [
              {
                "name": "AccDecoderRailcomTest.on_cutout_basic_and_extended_differ_for_same_address",
                "file": "dcc_application_accessory_decoder_railcom_Test.cxx",
                "desc": "basic and extended Ch1 payloads differ for the same address"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-ACC-003",
          "feature": "Status 1 report (ID 4) aspect + flags",
          "role": "acc",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 5 (Status 1, ID 4)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Status 1 (ID 4) packs aspect plus command-match and setpoint flags; host-tested only, as uart_write is NULL so nothing transmits in a real build.",
            "gtest": "",
            "hil": "Requires accessory-side Tx path. Decoder Tx not wired to hardware."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_ID_EXT"
            ],
            "tests": [
              {
                "name": "AccDecoderRailcomTest.send_status_packs_aspect_only",
                "file": "dcc_application_accessory_decoder_railcom_Test.cxx",
                "desc": "Status 1 packs aspect with no flags"
              },
              {
                "name": "AccDecoderRailcomTest.send_status_packs_all_flags",
                "file": "dcc_application_accessory_decoder_railcom_Test.cxx",
                "desc": "Status 1 packs aspect with all flags"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-ACC-004",
          "feature": "Status 4 report (ID 3) extended",
          "role": "acc",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 5 (Status 4, ID 3)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Status 4 (ID 3) emits an extended 2-byte status payload; host-tested only, as uart_write is NULL so nothing transmits in a real build.",
            "gtest": "",
            "hil": "Requires accessory-side Tx path. Decoder Tx not wired to hardware."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_ID_EXT"
            ],
            "tests": [
              {
                "name": "AccDecoderRailcomTest.send_status_extended_packs_two_datagrams",
                "file": "dcc_application_accessory_decoder_railcom_Test.cxx",
                "desc": "extended status packs two datagrams"
              },
              {
                "name": "AccDecoderRailcomTest.send_status_4_delegates",
                "file": "dcc_application_accessory_decoder_railcom_Test.cxx",
                "desc": "Status 4 delegates as ID 3"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-ACC-005",
          "feature": "Time report (ID 5)",
          "role": "acc",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 5 (Time report, ID 5)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Time report (ID 5) encodes a time value with resolution selector; host-tested only, as uart_write is NULL so nothing transmits in a real build.",
            "gtest": "",
            "hil": "Requires accessory-side Tx path. Decoder Tx not wired to hardware."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_ID_TIME"
            ],
            "tests": [
              {
                "name": "AccDecoderRailcomTest.send_time_report_with_resolution",
                "file": "dcc_application_accessory_decoder_railcom_Test.cxx",
                "desc": "time report encodes value with resolution flag"
              }
            ],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.3.2-ACC-006",
          "feature": "Error report (ID 6)",
          "role": "acc",
          "ref": {
            "spec": "S-9.3.2",
            "cite": "Section 5 (Error report, ID 6)",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "planned",
            "note": ""
          },
          "detail": {
            "impl": "Error report (ID 6) encodes an error code with optional additional data; host-tested only, as uart_write is NULL so nothing transmits in a real build.",
            "gtest": "",
            "hil": "Requires accessory-side Tx path. Decoder Tx not wired to hardware."
          },
          "refs": {
            "symbols": [
              "DCC_RAILCOM_ID_LOGON_ENABLE"
            ],
            "tests": [
              {
                "name": "AccDecoderRailcomTest.send_error_report_with_additional",
                "file": "dcc_application_accessory_decoder_railcom_Test.cxx",
                "desc": "error report encodes code plus additional data"
              }
            ],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "s9_4",
      "spec": "S-9.4.x",
      "title": "SUSI Bus",
      "features": [
        {
          "tid": "DCC-S9.4.X-HOST-001",
          "feature": "Electrical interface (S-9.4.1)",
          "role": "cs",
          "verify": "other",
          "ref": {
            "spec": "S-9.4.x",
            "cite": "S-9.4.1 Section 3",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "4-wire interface (Ground, Data, Clock, V+), up to 3 modules in parallel; 470 ohm series resistors on Clock and Data at both ends, Data pulled up >=15k to Vcc; 5V and 3.3V logic levels allowed (a 3.3V-capable module may be marked SUSI3); reverse-polarity protection diode recommended. Separate library.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.4.X-HOST-002",
          "feature": "Protocol and timing (S-9.4.1)",
          "role": "cs",
          "verify": "other",
          "ref": {
            "spec": "S-9.4.x",
            "cite": "S-9.4.1 Section 4",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Synchronous clocked transfer: clock idle low, data set on the rising edge and read on the falling edge, LSB first, 2 (occasionally 3) byte packets; clock half-period >=10us, bit <=500us, bytes within 7ms; a module resets/resyncs 8ms after a byte and confirms with a 1-2ms acknowledge. Separate library.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.4.X-HOST-003",
          "feature": "Command set and CV manipulation (S-9.4.1)",
          "role": "cs",
          "verify": "other",
          "ref": {
            "spec": "S-9.4.x",
            "cite": "S-9.4.1 Section 5",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "Two-byte operating commands derived from DCC (function groups, speed, direct, analog, trigger, module address/control) repeated at least every 200ms; three-byte CV commands (check 0x77, bit 0x7B, write 0x7F) address CVs 897-1024, with a module-reset exception writing CV8. Separate library.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.4.X-DEC-001",
          "feature": "Configuration variables (S-9.4.2)",
          "role": "dec",
          "verify": "other",
          "ref": {
            "spec": "S-9.4.x",
            "cite": "S-9.4.2 Section 2",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "SUSI CVs occupy 897-1024; each of up to 3 modules gets a 40-CV range (900-939 / 940-979 / 980-1019) selected by CV897 module number; ID CVs (manufacturer/version/SUSI version), CV1020 status byte (WAIT/SLOW/HOLD/STOP), and CV1021 banking expanding each module to 256 banks (with banks 248-253 reserved for RCN-218 config data). Separate library.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.4.X-HOST-004",
          "feature": "Bidirectional extension (S-9.4.3)",
          "role": "cs",
          "verify": "other",
          "ref": {
            "spec": "S-9.4.x",
            "cite": "S-9.4.3",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "SUSI-BiDi polls up to 3 registered modules at least every 100ms over the Data line; a module signals data with an ACK then transmits over 32 clocks; supports whole-CV-bank reads with a CRC-8 checksum (polynomial x8+x5+x4+1), host call commands (0x01, 0x0C-0x0F), and module response messages (0x80-0x8F). Separate library.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        },
        {
          "tid": "DCC-S9.4.X-HOST-005",
          "feature": "SIO shift-register train bus (S-9.4.4)",
          "role": "cs",
          "verify": "other",
          "ref": {
            "spec": "S-9.4.x",
            "cite": "S-9.4.4",
            "origin": "draft",
            "draftDelta": null
          },
          "supported": {
            "state": "na",
            "note": ""
          },
          "gtest": {
            "state": "na",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "An alternative use of the Train Bus (Data/Clock) drives shift-register function outputs without a programmed module (not usable simultaneously with SUSI): data clocked on the rising edge at 400 kHz-4 MHz, max 16 bits, highest function bit first, latched by interrupting the clock for >=30us. Separate library.",
            "gtest": "",
            "hil": ""
          },
          "refs": {
            "symbols": [],
            "tests": [],
            "hilChecks": []
          }
        }
      ]
    },
    {
      "id": "xcut",
      "spec": "Library",
      "title": "Scheduler / bit engine (cross-cutting)",
      "features": [
        {
          "tid": "DCC-Library-CS-001",
          "feature": "Scheduler priority selection (one-shot vs refresh)",
          "role": "cs",
          "ref": {
            "spec": "Library",
            "cite": "Library design (scheduler priority)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "DccScheduler_run selects the highest-priority one-shot, then falls back to refresh round-robin, then idle. One-shot lowest priority first, then refresh round-robin, else idle.",
            "gtest": "",
            "hil": "With a SPEED slot auto-refreshing, a one-shot broadcast ESTOP (priority ESTOP, non-refresh) is fired; the main-track capture (ch0) shows BOTH the continuing refresh packets and the one-shot ESTOP — the scheduler selects and interleaves the one-shot by priority without starving the refresh slot. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccScheduler_run",
              "DccScheduler_insert"
            ],
            "tests": [
              {
                "name": "DccScheduler.estop_has_higher_priority_than_speed",
                "file": "dcc_scheduler_Test.cxx",
                "desc": "lower priority value (e-stop) dispatched before a speed packet"
              },
              {
                "name": "DccScheduler.one_shot_takes_priority_over_refresh",
                "file": "dcc_scheduler_Test.cxx",
                "desc": "a pending one-shot is sent before any auto-refresh slot"
              }
            ],
            "hilChecks": [
              {
                "label": "one-shot ESTOP interleaved with refresh on the wire",
                "file": "library_compliance.py",
                "desc": "SPEED slot keeps streaming while a fired one-shot ESTOP also appears on ch0 — priority selection emits the one-shot without starving refresh."
              }
            ]
          }
        },
        {
          "tid": "DCC-Library-CS-002",
          "feature": "Auto-refresh round-robin of speed/function state",
          "role": "cs",
          "ref": {
            "spec": "Library",
            "cite": "Library design (auto-refresh)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "_select_refresh walks slots from refresh_index, returning the next active auto_refresh slot and advancing for fairness.",
            "gtest": "",
            "hil": "Three SPEED slots (addrs 3/4/5) on auto-refresh: one main-track capture (ch0) shows every address transmitted, repeatedly — the scheduler cycles round-robin through all active slots rather than starving any. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccScheduler_run",
              "DccScheduler_insert"
            ],
            "tests": [
              {
                "name": "DccScheduler.auto_refresh_keeps_sending",
                "file": "dcc_scheduler_Test.cxx",
                "desc": "an auto-refresh slot is retransmitted indefinitely"
              },
              {
                "name": "DccScheduler.auto_refresh_round_robin",
                "file": "dcc_scheduler_Test.cxx",
                "desc": "multiple refresh slots rotate fairly"
              }
            ],
            "hilChecks": [
              {
                "label": "all 3 refresh slots cycle on the wire",
                "file": "library_compliance.py",
                "desc": "addrs 3/4/5 each appear >=2x in one capture, proving round-robin retransmission of every active slot."
              }
            ]
          }
        },
        {
          "tid": "DCC-Library-CS-003",
          "feature": "Duplicate combining by (address, tag)",
          "role": "cs",
          "ref": {
            "spec": "Library",
            "cite": "Library design (duplicate combining)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "ok",
            "note": ""
          },
          "detail": {
            "impl": "DccScheduler_insert overwrites an existing (address, tag) slot instead of allocating a new one.",
            "gtest": "",
            "hil": "Two SPEED commands to the SAME address (10 then 20): the main-track capture (ch0) shows exactly ONE distinct packet for that address, now carrying the latest value, with the earlier value's packet gone — the (address, tag) slot was combined, not duplicated. (Validated on the bench 2026-06-26.)"
          },
          "refs": {
            "symbols": [
              "DccScheduler_insert"
            ],
            "tests": [
              {
                "name": "DccScheduler.duplicate_combining_overwrites_packet",
                "file": "dcc_scheduler_Test.cxx",
                "desc": "re-inserting same address+tag overwrites without a new slot"
              },
              {
                "name": "DccScheduler.different_tags_do_not_combine",
                "file": "dcc_scheduler_Test.cxx",
                "desc": "same address different tag occupies a separate slot"
              }
            ],
            "hilChecks": [
              {
                "label": "second SPEED overwrites the slot (one packet, latest value)",
                "file": "library_compliance.py",
                "desc": "after SPEED addr 10 then 20, ch0 carries exactly one distinct addr packet (the latest); the old value is absent — combined, not duplicated."
              }
            ]
          }
        },
        {
          "tid": "DCC-Library-CS-004",
          "feature": "Bit-encoder single-active-packet buffering",
          "role": "cs",
          "ref": {
            "spec": "Library",
            "cite": "Library design (bit encoder buffering)",
            "origin": "released",
            "draftDelta": null
          },
          "supported": {
            "state": "ok",
            "note": ""
          },
          "gtest": {
            "state": "ok",
            "note": ""
          },
          "hil": {
            "state": "na",
            "note": ""
          },
          "detail": {
            "impl": "DccBitEncoder_load_packet copies into the single active_packet and sets packet_loaded, cleared at completion. One active_packet + packet_loaded flag.",
            "gtest": "",
            "hil": "Wire-level framing validated by the s9_1 timing harness. Bit timing checked indirectly by s9_1."
          },
          "refs": {
            "symbols": [
              "DccBitEncoder_load_packet",
              "DccBitEncoder_is_idle"
            ],
            "tests": [
              {
                "name": "DccBitEncoder.tick_consecutive_packets",
                "file": "dcc_bit_encoder_Test.cxx",
                "desc": "a second loaded packet transmits after the first completes"
              },
              {
                "name": "DccBitEncoder.tick_packet_complete_without_railcom",
                "file": "dcc_bit_encoder_Test.cxx",
                "desc": "on_packet_complete fires and packet_loaded clears at end of packet"
              }
            ],
            "hilChecks": []
          }
        }
      ]
    }
  ]
};
