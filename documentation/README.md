# OpenDccCLib Documentation

This folder is the documentation root. Start here — it maps every document and
tells you which one is authoritative for what.

> **Last updated 2026-06-22** — compliance-deviation fixes + documentation consolidation
> (baseline commit `c4dc61e`). The living documents below each carry their own status note;
> if it predates the latest commit, treat the content as possibly stale and confirm against the source.

## Living documents (kept current)

| Document | Purpose |
|---|---|
| [ARCHITECTURE.md](ARCHITECTURE.md) | **As-built** design: modules, feature flags, interface-struct DI, execution contexts, scheduler/bit-encoder. The source of truth for how the library is structured today. |
| [ComplianceOverview.md](compliance/ComplianceOverview.md) | **Compliance narrative & design memory:** snapshot, per-feature NMRA spec (released + draft) → implementation summary, the backlog (released-vs-draft tagged), known defects, recently resolved, plus the service-mode and RailCom design appendices. *Machine-checked per-feature status lives in `compliance/compliance.data.js`, rendered by `index.html` — that is the status source of truth.* |
| [PDF_Regeneration_Guide.md](PDF_Regeneration_Guide.md) | How to regenerate the guide/brochure PDFs and what to pull from the current source so a rebuild stays accurate. |

## Reference material (stable, external truth)

| Document | Purpose |
|---|---|
| [specs/DCC_Spec_Reference.md](specs/DCC_Spec_Reference.md) | Distilled notes from the **released** NMRA S-9.x standards. |
| [compliance/index.html](compliance/index.html) | Compliance dashboard + `compliance.data.js` database: per-requirement status, tests, and **draft-vs-released provenance** — the home for draft-revision tracking. |
| [specs/*.pdf](specs/) | Original NMRA standard PDFs (input source documents). |
| [style_guides/StyleGuide_v4.md](style_guides/StyleGuide_v4.md) | C source style guide. |
| [style_guides/Doxygen_StyleGuide_v2_0.md](style_guides/Doxygen_StyleGuide_v2_0.md) | Doxygen comment conventions. |

## Test & validation docs

| Document | Purpose |
|---|---|
| [../test/README.md](../test/README.md) | How to build and run the host unit-test suite (CMake + GoogleTest + gcovr). |
| [../applications/ti_theia/mspm03507_launchpad/loopback_test/README.md](../applications/ti_theia/mspm03507_launchpad/loopback_test/README.md) | Hardware-in-the-loop loopback test setup (two MSPM0 boards). |
| [../applications/ti_theia/mspm03507_launchpad/loopback_test/dcc_loopback_test_plan.md](../applications/ti_theia/mspm03507_launchpad/loopback_test/dcc_loopback_test_plan.md) | Detailed loopback test plan. |

## Historical (frozen — do not treat as current)

| Document | Purpose |
|---|---|
| [archive/OpenDccCLib_Requirements.md](archive/OpenDccCLib_Requirements.md) | Original design intent / requirements. Superseded by [ARCHITECTURE.md](ARCHITECTURE.md); module and API names here predate the role-first refactor. |
| [archive/application_api_refactor.md](archive/application_api_refactor.md) | The 10-phase application-API refactor plan, now largely executed. Kept as the record of what shipped. Any still-pending items live in [ComplianceOverview.md](compliance/ComplianceOverview.md). |
| [archive/compliance_deviation_fixes.md](archive/compliance_deviation_fixes.md) | Executed plan for the six "implemented but wrong" deviation fixes (cutout timing, SRQ, ACK, datagram IDs, speed-restriction removal, double-buffer doc). The deferred decoder-RailCom-Tx follow-on lives in [ComplianceOverview.md](compliance/ComplianceOverview.md). |

## Guides & generated artifacts

The Quick Start / Developer guide PDFs and the brochure
(`QuickStartGuide_*.pdf`, `DeveloperGuide_*.pdf`, `OpenDccCLib_Brochure.pdf`) are
**committed deliverables**, kept in sync with the library. They are produced
out-of-band by a local reportlab generator that is intentionally **not** tracked
in the repo; see [PDF_Regeneration_Guide.md](PDF_Regeneration_Guide.md) for how to
regenerate them and what to pull from the current source.

API reference is produced by **Doxygen** from the source headers (see `Doxyfile`
and `../src/mainpage.h`); its output (`documentation/html/`) is git-ignored and
regenerated locally as needed.

Narrative documentation lives as Markdown in this folder. Structural facts have a
single source of truth in [ARCHITECTURE.md](ARCHITECTURE.md) and
[ComplianceOverview.md](compliance/ComplianceOverview.md) — link to them rather than restating module names, struct
fields, or timing values.
