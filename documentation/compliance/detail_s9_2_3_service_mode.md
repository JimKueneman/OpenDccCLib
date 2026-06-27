# Service Mode Operations Matrix

> Source: NMRA S-9.2.3 (July 2012). All four modes are defined in §E.
> "Read" in service mode is always accomplished via **Verify** — the CS sends a verify
> packet and an ACK pulse from the decoder means the value matches. A true read of an
> unknown value requires iterating verify operations (bit-by-bit or byte-by-byte).

---

## Operations Supported per Mode

| Mode             | Write Byte | Read Byte | Bit Write | Bit Read | CV Scope |
|------------------|:----------:|:---------:|:---------:|:--------:|----------|
| **Direct**       | ✅ native — 1 op | ✅ native — exactly **8 ops** (one `verify_bit` per bit) | ✅ native — 1 op | ✅ native — 1 op | CV 1–1023 |
| **Paged**        | ✅ native — 1 op | ✅ iterate `verify_byte` 0→255 — up to **255 ops** | ✅ read-modify-write — up to **256 ops** | ✅ read byte, extract bit — up to **255 ops** | CV 1–1024 (via page register) |
| **Register**     | ✅ native — 1 op | ✅ iterate `verify_byte` 0→255 — up to **255 ops** | ✅ read-modify-write — up to **256 ops** | ✅ read byte, extract bit — up to **255 ops** | 8 registers → specific CVs |
| **Address-Only** | ✅ native — 1 op | ✅ iterate `verify_byte` 0→127 — up to **127 ops** (CV#1 is 7-bit) | ✅ read-modify-write — up to **128 ops** | ✅ read byte, extract bit — up to **127 ops** | CV 1 only (short address) |

---

## Mode Notes (from §E and §F)

**Direct** — 4-byte packet `long-preamble 0 0111CCAA 0 AAAAAAAA 0 DDDDDDDD 0 EEEEEEEE 1`.
AA+AAAAAAAA = 10-bit CV address (CV# = address + 1). CC selects operation type.
Bit manipulation uses a special data byte `111KDBBB` (K=write/verify, D=value, BBB=bit position).
Required on all conformant Command Stations since 1-Aug-2002.

**Paged** — 3-byte packet `long-preamble 0 0111CRRR 0 DDDDDDDD 0 EEEEEEEE 1`.
Registers 1–4 (RRR=000–011) are Data Registers pointing to CVs on the current page.
Register 6 (RRR=101) is the Paging Register. Page preset required before each operation.
CV formula: CV# = (page−1)×4 + data_register + 1.

**Register (Physical Register)** — Same packet format as Paged (`0111CRRR`).
8 fixed registers (RRR=000–111). Page preset required before operations.
Recovery time after writing Register 1 is 10 packets (same as Address-Only mode).

Register-to-CV mapping differs between Mobile and Accessory decoders (§E, page 5):

| Register | RRR | Mobile Decoder CV | Accessory Decoder CV |
|:--------:|:---:|-------------------|----------------------|
| 1 | 000 | Address (CV #1) | Lower Address (CV #513) |
| 2 | 001 | Start Voltage (CV #2) | Undefined — see Mfg. documentation |
| 3 | 010 | Acceleration (CV #3) | Undefined — see Mfg. documentation |
| 4 | 011 | Deceleration (CV #4) | Undefined — see Mfg. documentation |
| 5 | 100 | Basic Config Register (CV #29) | Undefined — see Mfg. documentation |
| 6 | 101 | (Reserved for Page Register) | Undefined — see Mfg. documentation |
| 7 | 110 | Version Number (CV #7) | Version Number (CV #7) |
| 8 | 111 | Manufacturer ID (CV #8) | Manufacturer ID (CV #520) |

The register task module must know whether it is addressing a Mobile or Accessory decoder
to correctly map register numbers to CV numbers. Register 1 recovery time (10 packets)
applies to Mobile decoders writing the address CV; for Accessory decoders, Register 1
writes CV#513 and the same 10-packet recovery applies.

**Address-Only** — 3-byte packet `long-preamble 0 0111C000 0 0DDDDDDD 0 EEEEEEEE 1`.
Only accesses CV #1 (7-bit short address). Requires Page-Preset-Instruction sequence first
(5+ writes to Page Register → 6+ reset packets). Writing CV#1 via any method must also
clear the extended-addressing bit (CV#29 bit 5) and the consist address (CV#19).

---

## Packet Sequence Requirements (§E)

| Mode | Pre-op resets | Page preset | Pre-CV resets | Command repeats | Recovery (write) | Post-op resets |
|------|:------------:|:-----------:|:-------------:|:---------------:|:----------------:|:--------------:|
| Direct | ≥3 | — | — | ≥5 | ≥6 | ≥1 |
| Paged | ≥3 | 5+ page writes + 6+ resets | ≥3 | ≥5 | ≥6 | ≥1 |
| Register | ≥3 | 5+ page writes + 6+ resets | ≥3 | ≥5 (≥7 verify) | ≥6 (≥10 if Reg 1) | ≥1 |
| Address-Only | ≥3 | 5+ page writes + 6+ resets | ≥3 | ≥5 | ≥10 | ≥1 |

All three non-Direct modes have a **second block of ≥3 reset packets** between the page preset
recovery and the actual CV access operation. The Pre-CV resets column captures this.
Register and Address-Only modes additionally allow an optional power-off/on cycle between
the page preset recovery and the pre-CV resets (footnote 3 — see Spec Constraints below).
Paged mode does NOT allow a power cycle at this point.

### Implementation status — these sequences are enforced in the library

As of the service-mode sequence work, the primitives produce the full §E sequences above:

- **Register** and **Address-Only** prepend a **page-preset** (write page register → page 1,
  `7D 01 7C`) before the command — a two-step chain mirroring Paged (`begin_operation`
  page-preset → on complete → command). Previously they emitted the command directly.
- **Register VERIFY** uses **7** command packets (`DCC_SERVICE_MODE_REGISTER_VERIFY_REPEAT`);
  a write to **Register 1** (and to **CV #1** in Address-Only) uses the **10**-packet recovery
  (`DCC_SERVICE_MODE_RECOVERY_COUNT_LONG`). `begin_operation` gained a per-op `command_repeat`
  parameter to support this (recovery was already per-op).
- **Paged** proceeds from the page-select to the data step **unconditionally** — the page-write
  phase completes by ACK *or* by packet count, and ACK is optional (§D), so requiring the
  page-select to ACK would break paged on decoders that never assert one (S-9.2.3 line 215).
- The preset/page-select's own ACK result does **not** gate the command step in any mode.

Every one of these sequences is verified at both layers; the **Test Coverage Matrix** below
lists the exact host (`*_Test.cxx`) and HIL (`s9_2_3_compliance.py`) test per requirement.
The bench wiring for the HIL checks is in
[HIL_SETUP.md](../../test/compliance/ti_theia/saleae_hil_compliance/HIL_SETUP.md).

---

## Test Coverage Matrix

> WHAT each spec requirement is verified by, and WHERE. The *how the bench is wired* —
> pins, Saleae channels, jumpers, UART commands — lives in
> [HIL_SETUP.md](../../test/compliance/ti_theia/saleae_hil_compliance/HIL_SETUP.md), not here.
>
> **Host** = `src/dcc/*_Test.cxx` (gTest, mocked drivers). **HIL** =
> `test/compliance/s9_2_3_compliance.py` (Saleae, on-wire). A blank cell means "not covered
> at that layer," not "untested."

### ACK detection (§D — 6 ms ±1 ms window)

| Requirement | Host (gTest) | HIL (s9_2_3) |
|---|---|---|
| ACK accepted at exact min sample count (85) | `common.ack_exact_min_samples_detected` | `ack_width_tests` (4930 µs = MIN) |
| ACK rejected one below min (84) | `common.ack_one_below_min_samples_not_detected` | `ack_width_tests` (4872 µs) |
| ACK accepted within max window | `common.ack_within_max_window_detected` | `ack_width_tests` (6960 µs = MAX) |
| ACK rejected over max / over-current | `common.ack_beyond_max_window_not_detected` | `ack_width_tests` (7018 µs) |
| Interrupted pulse resets the width counter | `common.interrupted_pulse_resets_counter` | — |
| ACK sampled only in COMMAND state (scan window) | `common.ack_during_pre_reset_ignored`, `common.ack_during_post_reset_does_not_change_result` | `ack_width_tests` (in-window vs EARLY) |
| Injected pulse width measured independently | — | `ack_width_tests` (D4 tap, `_measure_high_us`) |

### Packet sequence (§E)

| Requirement | Host (gTest) | HIL (s9_2_3) |
|---|---|---|
| Pre-reset = 3 packets | `common.pre_reset_sends_correct_count` | `_count_resets ≥ 3` |
| Post-reset = 6 packets | `common.post_reset_sends_correct_packet_count` | — |
| ACK terminates command phase early | `common.ack_detected_terminates_command_phase_early` | — |
| ≥20-bit preamble (service packets) | — | `_check_command` (all modes) |
| Register VERIFY = 7 command repeats | `register.verify_uses_seven_repeats_zero_recovery` | `test_register` (`min_repeat`) |
| Register 1 / CV#1 write = 10-packet recovery | `register.write_register1_uses_long_recovery`, `address.write_passes_write_flag_and_long_recovery` | — |
| Standard write recovery = 6 packets | `register.write_register2_uses_standard_recovery` | — |

### Per-mode encoding & behavior

| Requirement | Host (gTest) | HIL (s9_2_3) |
|---|---|---|
| Direct exact bytes (write 0x7C / verify 0x74 / bit) | `direct.write_byte_cv1_value_0x55`, `direct.verify_byte_cv1_value_0x55`, `direct.write_bit_cv1_bit3_high`, `direct.verify_bit_cv1_bit5_high` | `test_direct` (3 instr types) |
| Direct 10-bit CV addressing | `direct.write_byte_cv1024_value_0xAA` | `test_direct` (CV#1/#3/#1023) |
| Page-preset before Register/Address command | `register.write_starts_with_page_preset`, `address.write_starts_with_page_preset` | `test_register`, `test_address` |
| Page-preset NO_ACK still proceeds | `register.command_proceeds_even_if_preset_no_ack`, `paged.write_page_select_no_ack_still_proceeds` | — |
| Paged page-write + data-register command | `paged.write_cv1_page_select_packet`, `paged.write_cv1_data_access_after_page_select_success` | `test_paged` |
| Register↔CV mapping (Mobile + Accessory) | `task_register` mapping tests (CV1→Reg1 … CV520→Reg8) | — |
| Factory reset = value 8 → Register 8 (`7F 08 77`) | `task_register.factory_reset_writes_8_to_register_8` | `test_register` |

### Task orchestrator (iteration / read-modify-write / verify)

| Requirement | Host (gTest) | HIL (s9_2_3) |
|---|---|---|
| read_cv iterates; ACK returns the found value | `task_register.read_cv_ack_on_value_42_returns_42`, `task_address.read_ack_on_address_3_returns_3` | — |
| write_cv = write → verify | `task_register.write_cv_verifies_after_write_complete`, `task_address.write_verifies_after_write_complete` | — |
| Bit op = read-modify-write | `task_register.write_bit_scans_then_writes_with_bit_set`, `task_address.write_bit_clears_bit` | — |
| Read-back / write-back value correctness | — | `test_readback` (mock decoder, `SVC MOCKCV`) |
| Parallel-track timing (main + service) | — | `check_track_timing` / `_check_timing_both` (every op) |

---

## Conformance Groups (§F)

| Programmer Type | Modes Required |
|-----------------|----------------|
| Address-Only | Address-Only only |
| Register-Mode CV | Address-Only + Physical Register |
| Paged CV | Address-Only + Physical Register + Paged |
| Direct CV | Address-Only + Physical Register + Direct |
| Universal CV | All four modes |

Effective 1-Aug-2002: all conformant Command Stations must implement **Direct Mode**.

---

## Task Orchestrator Architecture

### Module Structure

One task module per service mode type — independent compile-gating, focused interface
structs, and isolated test files. Each module sits above its corresponding primitive module
and below the application layer.

```
dcc_service_mode_task_direct.c/.h     → DCC_COMPILE_SERVICE_MODE_DIRECT
dcc_service_mode_task_paged.c/.h      → DCC_COMPILE_SERVICE_MODE_PAGED
dcc_service_mode_task_register.c/.h   → DCC_COMPILE_SERVICE_MODE_REGISTER
dcc_service_mode_task_address.c/.h    → DCC_COMPILE_SERVICE_MODE_ADDRESS
dcc_service_mode_task_detect.c/.h     → DCC_COMPILE_COMMAND_STATION
```

**Why per-mode, not consolidated:**
- Each module is 4–5 functions with a small focused interface struct and mock in tests.
- Test files (`_Test.cxx`) mock only that mode's function pointers — a consolidated module
  would require mocking all four modes even when testing one.
- Independently compile-gated — unused modes compile out entirely.
- Mirrors the existing primitive module structure directly below in the stack.

### Singleton — No Context Pointer

All task modules are singletons. State is held in a static internal variable; no context
pointer is passed to any API function.

**Hardware justification:** the shared 58 µs DCC timer already drives the main track and
service track in parallel. A second programming track would risk missing ISR deadlines.
There is therefore only ever one service track — multi-instance support has no use case
and context passing would add noise to every call site for no benefit.

Matches the existing application module pattern (`DccApplicationCommandStationServiceTrack_*`
functions take no context).

### HIL Timing Requirement — Parallel Track Operation

The HIL compliance suite for service mode **must** run both the main DCC track output and the
service track output simultaneously and verify that neither drops a bit or stretches a
half-bit period beyond spec tolerance. (Channel/pin assignments are in
[HIL_SETUP.md](../../test/compliance/ti_theia/saleae_hil_compliance/HIL_SETUP.md).)

**What to verify:**
- Main track continues producing correctly timed DCC packets (preamble, bits, error byte)
  with no glitches while service mode operations run on the programming track.
- Service track produces correctly timed service mode packets (≥20-bit preamble, correct
  bit encoding) with no glitches while the main track runs.
- No ISR deadline is missed — half-bit periods on both channels stay within DCC timing
  tolerance (±6 µs for a 58 µs half-bit).

**Why this matters:** the shared 58 µs timer drives both tracks from one ISR. Any task
module code that runs too long in the ACK callback or state advance path will stretch the
next half-bit period on both tracks. The HIL rig can catch this directly by measuring
both channels simultaneously on the Saleae.

**Existing architecture that protects timing (`dcc_config.c:813`):** both track pin
toggles are pulled to the very top of `DccConfig_58us_timer_isr()` before any state
machine processing. Main track (PB1) and service track (PB4) GPIO toggles fire
back-to-back immediately on ISR entry; the heavier `DccBitEncoder_tick_isr()` calls for
both encoders follow after. Bit timing on the wire is set by the GPIO write, not by
subsequent state processing — so task module callback overhead cannot cause bit-period
jitter on either channel.

**Why "continuous DCC, no software blanking" is what makes this safe.** Both tracks run
off the *one* 58 µs timer/ISR, and the encoder **never stops** — it ticks the bit cadence
continuously. RailCom is implemented as a **driver-side cutout strobe** (the H-bridge output
is gated externally via the begin/end-cutout hooks and a *separate* one-shot timer), **not**
by blanking or pausing the bitstream in software. The timing engine is therefore never
paused for a cutout. This matters directly for service mode: because the service track
shares that same 58 µs ISR, *anything that perturbed the shared timer would corrupt
service-mode bit timing too*. If RailCom were done the old way — blanking/pausing the DCC in
software to carve out the cutout — that disruption would propagate onto the service track
exactly when it needs clean ≥20-bit-preamble verify packets and a stable ACK-sample window.
The continuous-encoder + toggle-first-ISR design decouples both tracks from cutout events
and from each other's state-machine load. This is why the main track holds ~58.0 µs ± ~0.05 µs
half-bits even while a service-mode operation runs (measured on the HIL rig).

**The parallel-track timing check guards this property** (see the **Test Coverage Matrix**).
`s9_2_3_compliance.py` captures the main and service tracks *simultaneously* during every
service-mode operation and runs `check_track_timing()` on both — so a future change that
reintroduces software blanking (or otherwise stretches/drops a bit on either track) would
**fail the suite** instead of silently corrupting service-mode timing.

### ACK Pulse-Width Verification (6 ms ±1 ms window, S-9.2.3 §D)

ACK detection lives entirely in `dcc_service_mode_common` (`ack_sample`): the 58 µs ISR
feeds it the `current_sense_read()` level (the application's comparator output on the ACK
sense pin), and it counts consecutive above-threshold samples to validate the pulse width
against the configured window. Window constants (typical config):

- `DCC_ACK_MIN_SAMPLES = (USER_DEFINED_DCC_ACK_MIN_DURATION_US / 58) − 1` = `(5000/58) − 1` = **85**
- `DCC_ACK_MAX_SAMPLES = USER_DEFINED_DCC_ACK_MAX_DURATION_US / 58` = `7000/58` = **120**

A run is accepted as an ACK only when, on the falling edge, its length is in
`[MIN, MAX]` (inclusive) and was never flagged over-current.

**Scan-window blanking (S-9.2.3 line 55).** `ack_sample` ignores current until the ACK scan
window opens — set once more than `DCC_SERVICE_MODE_ACK_BLANK_PACKETS` (2) command packets
have been sent. This masks the first two command packets, where a decoder switching into
service-mode code can glitch its motor-drive pins and put a transient on the same
current-sense line the ACK uses. It is lossless for a conformant decoder, which per §E cannot
ACK until it has received two identical packets anyway (a two-sided contract: decoder
won't ACK early, programmer won't look early). The window opens on the packet-complete event,
so it is packet-accurate; the `−1` in `MIN_SAMPLES` absorbs the one-sample edge quantization,
so an ACK that legitimately begins right at the 2nd packet's end is not clipped.

**Dropout filter (noisy loads).** Because the ACK *is* the decoder pulsing its motor, the
sensed current is noisy — commutation/PWM ripple can drop the comparator below threshold for
a sample or two mid-pulse. `ack_sample` bridges up to `DCC_ACK_DROPOUT_SAMPLES` consecutive
low samples into the run (measuring the elevated-current *span*, not strict-high time); only a
longer low run is a real falling edge. Tunable via **`USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US`**
(default 116 µs ≈ 2 samples; `0` = strict consecutive-high, the original behavior). Keep it a
small fraction of the 6 ms ACK or it defeats the width discrimination.

**Coverage** is in the **Test Coverage Matrix** (host width-counter / dropout-filter /
window-blanking tests, and the HIL `ack_width_tests` vectors). The host tests verify the
*logic*; the HIL suite injects a **real electrical pulse** (a firmware mock-ACK looped back
on-board) and checks both the UART verdict **and** the Saleae-measured pulse width against
these boundary vectors — widths chosen so `width_us / 58` lands on an exact sample count:

| width_us | samples (=width/58) | expected verdict   |
|:--------:|:-------------------:|:------------------:|
| 4872     | 84 (< MIN 85)       | NO ACK             |
| 4930     | 85 (= MIN)          | ACK DETECTED       |
| 6000     | 103                 | ACK DETECTED       |
| 6960     | 120 (= MAX)         | ACK DETECTED       |
| 7018     | 121 (> MAX 120)     | NO ACK (overrun)   |

This makes ACK-width an on-hardware check, not a synthetic value injected behind the driver.
The loopback wiring (the PB24→PB9 jumper, the D4 tap, and the `SVC MOCKACK` command) is
documented in [HIL_SETUP.md](../../test/compliance/ti_theia/saleae_hil_compliance/HIL_SETUP.md).

### API per Module

Every task module exposes the same four operations plus one ACK entry point.
Implementation and cost differ by mode:

| Operation | Direct ops | Non-Direct ops | Notes |
|-----------|:----------:|:--------------:|-------|
| `write_cv` | **2** (write + verify) | **2** (write + verify) | Value is known after write — one targeted verify, never iterate |
| `read_cv` | **8** (one `verify_bit` per bit) | **up to 255** (iterate `verify_byte` 0→N) | Value unknown — must iterate for non-Direct |
| `write_bit` | **2** (write_bit + verify_bit) | **up to 257** (read byte + modify + write + verify) | Non-Direct: read-modify-write; read step must iterate |
| `read_bit` | **1** (`verify_bit`) | **up to 255** (read byte, extract bit in software) | Non-Direct: full byte read required to get bit |

**Callback types:**

```c
// CV operations: value is the CV byte or bit value found/validated
typedef void (*dcc_service_mode_task_on_complete_callback_t)(dcc_service_mode_result_t result, uint8_t value);

// Detect operation: supported_modes is a bitmask of ALL supported modes
typedef void (*dcc_service_mode_task_on_detect_callback_t)(dcc_service_mode_result_t result, uint8_t supported_modes);

typedef void (*dcc_service_mode_task_on_progress_callback_t)(dcc_task_phase_enum phase, uint8_t current_step, uint8_t estimated_steps);
```

`on_complete` value is always meaningful: for reads it is the CV/bit value found; for writes
it is the value read back during validation.

`on_detect` returns a bitmask of every supported mode, not a single "best" mode, using the
`DCC_SERVICE_MODE_SUPPORTED_*` flags:

```c
#define DCC_SERVICE_MODE_SUPPORTED_DIRECT   (1u << 0)
#define DCC_SERVICE_MODE_SUPPORTED_PAGED    (1u << 1)
#define DCC_SERVICE_MODE_SUPPORTED_REGISTER (1u << 2)
#define DCC_SERVICE_MODE_SUPPORTED_ADDRESS  (1u << 3)
```

`supported_modes == 0` means no mode was detected (and `result` is `DCC_SERVICE_MODE_NO_ACK`);
otherwise `result` is `DCC_SERVICE_MODE_SUCCESS`. The application picks whichever supported
mode it prefers and selects the matching task module.

`on_progress` carries a phase so the application can map to its own strings or UI state:

```c
typedef enum {
    DCC_TASK_PHASE_READ,     // byte or bit iteration to find unknown value
    DCC_TASK_PHASE_WRITE,    // writing value to the decoder
    DCC_TASK_PHASE_VERIFY    // read-back validation after write
} dcc_task_phase_enum;
```

`estimated_steps` is the worst-case ceiling — the operation ends early via `on_complete`
the moment ACK is received. All operations accept `on_progress` as nullable; callers
pass `NULL` when progress reporting is not needed.

Phase sequence per operation:

| Operation | Phase sequence | Notes |
|-----------|---------------|-------|
| `read_cv` | READ | Ends at first ACK |
| `write_cv` | WRITE → VERIFY | Always 2 steps — value known after write |
| `read_bit` | READ | Ends at first ACK |
| `write_bit` Direct | WRITE → VERIFY | Always 2 steps |
| `write_bit` non-Direct | READ → WRITE → VERIFY | READ phase can be up to 255 steps |

**Interface structs** (all wired by `dcc_config.c`, all function pointers only):

```c
// --- Direct ---
typedef struct {

    bool (*verify_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value);
    bool (*verify_byte)(uint16_t cv_number, uint8_t value);
    bool (*write_byte)(uint16_t cv_number, uint8_t value);
    bool (*write_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value);
    bool (*is_idle)(void);
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_direct_t;

// --- Paged ---
typedef struct {

    bool (*paged_verify)(uint16_t cv_number, uint8_t value);
    bool (*paged_write)(uint16_t cv_number, uint8_t value);
    bool (*is_idle)(void);
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_paged_t;

// --- Register ---
typedef struct {

    bool (*register_verify)(uint8_t register_number, uint8_t value);
    bool (*register_write)(uint8_t register_number, uint8_t value);
    bool (*is_idle)(void);
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_register_t;

// --- Address-Only ---
typedef struct {

    bool (*address_verify)(uint8_t address);
    bool (*address_write)(uint8_t address);
    bool (*is_idle)(void);
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_address_t;

// --- Detect ---
typedef struct {

    bool (*direct_verify_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value);
    bool (*paged_verify)(uint16_t cv_number, uint8_t value);
    bool (*register_verify)(uint8_t register_number, uint8_t value);
    bool (*address_verify)(uint8_t address);
    bool (*is_idle)(void);
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_detect_t;
```

Detect needs `address_verify` because Address-Only is probed for real (it accesses only
CV#1 and cannot be inferred from the CV#8 probes used by the other three modes).

**Public API** (Direct shown; Paged and Address-Only follow same shape without `decoder_type`;
Register adds `decoder_type` per-call since decoder type can change between operations):

```c
// Direct
void DccServiceModeTaskDirect_initialize(const interface_dcc_service_mode_task_direct_t *interface);
bool DccServiceModeTaskDirect_read_cv(uint16_t cv, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskDirect_write_cv(uint16_t cv, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskDirect_read_bit(uint16_t cv, uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskDirect_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
void DccServiceModeTaskDirect_on_primitive_complete(dcc_service_mode_result_t result);

// Paged — same shape as Direct
void DccServiceModeTaskPaged_initialize(const interface_dcc_service_mode_task_paged_t *interface);
bool DccServiceModeTaskPaged_read_cv(uint16_t cv, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskPaged_write_cv(uint16_t cv, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskPaged_read_bit(uint16_t cv, uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskPaged_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
void DccServiceModeTaskPaged_on_primitive_complete(dcc_service_mode_result_t result);

// Register — decoder_type per-call (user may switch between loco and accessory decoder)
void DccServiceModeTaskRegister_initialize(const interface_dcc_service_mode_task_register_t *interface);
bool DccServiceModeTaskRegister_read_cv(uint16_t cv, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskRegister_write_cv(uint16_t cv, uint8_t value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskRegister_read_bit(uint16_t cv, uint8_t bit, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskRegister_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
void DccServiceModeTaskRegister_on_primitive_complete(dcc_service_mode_result_t result);
bool DccServiceModeTaskRegister_factory_reset(dcc_service_mode_task_on_complete_callback_t on_complete);

// Address-Only — CV#1 only; decoder handles CV#29/CV#19 side effects automatically
void DccServiceModeTaskAddress_initialize(const interface_dcc_service_mode_task_address_t *interface);
bool DccServiceModeTaskAddress_read(dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskAddress_write(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskAddress_read_bit(uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
bool DccServiceModeTaskAddress_write_bit(uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);
void DccServiceModeTaskAddress_on_primitive_complete(dcc_service_mode_result_t result);

// Detect — own callback type: result is a supported-modes bitmask, not a CV byte
void DccServiceModeTaskDetect_initialize(const interface_dcc_service_mode_task_detect_t *interface);
bool DccServiceModeTaskDetect_detect_mode(dcc_service_mode_task_on_detect_callback_t on_detect);
void DccServiceModeTaskDetect_on_primitive_complete(dcc_service_mode_result_t result);
```

**ACK detection — internal, not the task's concern.** ACK detection lives in the
`dcc_service_mode_common` module, not in the task layer or the application. The application's
driver supplies only a raw level via `current_sense_read()` (e.g. a comparator output on a
GPIO pin reported as above-threshold/zero). The 58 µs ISR feeds that into
`DccServiceModeCommon_ack_sample()`, which counts consecutive above-threshold samples to
measure the pulse width and validate it against the S-9.2.3 §D window (5–7 ms, with
over-current rejection). The outcome is folded into the primitive's result code:
`DCC_SERVICE_MODE_SUCCESS` = valid ACK, otherwise no ACK. The split is: **hardware owns
amplitude/threshold, the common module owns width/windowing, the task owns sequencing.**

**`on_primitive_complete` design:** `dcc_config.c` wires each primitive's `on_complete` slot
to `DccServiceModeTask<Mode>_on_primitive_complete(result)`. The primitive fires this once its
full operation (command + recovery) is finished. The task derives the ACK directly from the
result — `ack = (result == DCC_SERVICE_MODE_SUCCESS)` — and advances its state machine. There
is **no** `on_ack` entry point: the task never needs the application to time or report the
pulse, because the common module already did.

Event sequence for each primitive step:
```
task calls interface->verify_bit(cv, bit, 1)
  → primitive (via common) sends command packets
  → 58µs ISR feeds current_sense_read() to common.ack_sample() the whole time
  → common measures the pulse width, sets result = SUCCESS (ACK) / NO_ACK
  → primitive finishes recovery → on_primitive_complete(result) fires
  → task: ack = (result == SUCCESS) → advances state
```

> Note: `on_start_ack_scan` remains in the task interface structs but is currently unused —
> the common module gates ACK sampling itself by its COMMAND/RECOVERY state, so the task has
> no need to tell the application "start scanning now." It is a candidate for removal.

**`decoder_type` rationale (Register mode only):** the user may program a locomotive and
then switch to an accessory decoder on the same programming track. Fixing decoder type at
init time would require re-initialization on every switch. Passing it per-call makes the
decoder type explicit and eliminates hidden session state.

### Internal State Machine Shape

The same state machine pattern applies to all five modules:

```
IDLE
  ↓ operation called (read_cv / write_cv / read_bit / write_bit)
PHASE_EXECUTE      ← send packet via interface, wait for on_primitive_complete()
  ↓ on_primitive_complete(result) received  (ack = result == SUCCESS)
PHASE_ADVANCE      ← record result, decide: next iteration OR move to next phase OR done
  ↓ next iteration
PHASE_EXECUTE      ← (loop back — next bit / next byte value)
  ↓ done / phase complete
PHASE_COMPLETE     ← call on_complete(result, value), return to IDLE
```

What differs per mode is what EXECUTE sends and how ADVANCE decides:

| Operation | Mode | EXECUTE sends | ADVANCE logic |
|-----------|------|--------------|---------------|
| `read_cv` | Direct | `verify_bit(cv, bit, 1)` | ACK→bit=1, no ACK→bit=0; bit++ until 8 done |
| `read_cv` | Paged/Reg/Addr | `verify(cv, current_value)` | ACK→found; no ACK→value++ until 255/127 |
| `write_cv` | All | `write(cv, value)` then `verify(cv, value)` | Write phase→Verify phase→Complete |
| `read_bit` | Direct | `verify_bit(cv, bit, 1)` | ACK→bit=1; no ACK→bit=0; Complete |
| `read_bit` | Paged/Reg/Addr | `verify(cv, 0)` iterate | Same as read_cv; extract bit from result |
| `write_bit` | Direct | `write_bit(cv, bit, val)` then `verify_bit` | Write→Verify→Complete |
| `write_bit` | Paged/Reg/Addr | read byte first (iterate), modify bit, write, verify | Read phase→Modify→Write phase→Verify phase→Complete |

### Detection Module

`dcc_service_mode_task_detect` determines **all** supported service modes and returns them
as a `DCC_SERVICE_MODE_SUPPORTED_*` bitmask via `on_detect`. It runs four stages in order:

1. **Direct** — the spec-defined method (S-9.2.3 p.3 lines 94-99): two `verify_bit` on CV#8
   bit 7 (try value=0, then value=1); ACK on either → Direct supported. If supported, the
   remaining 7 bits of CV#8 are read so the **byte value is now known**.
2. **Paged** — `paged_verify` on CV#8. If the value is already known (from the Direct read,
   or a prior scan), this is a **single value-verify**; otherwise it is a 0→255 scan that
   *learns* the value for the stages that follow.
3. **Register** — same as Paged, against register 8 (= CV#8 Manufacturer ID), reusing the
   known value when available.
4. **Address-Only** — a separate 0→127 scan of CV#1. It cannot reuse the CV#8 value (CV#1 is
   a different CV), and is **not** assumed to be universally supported — S-9.2.3 §F mandates
   Address-Only for *command stations*, not for every *decoder*.

Value reuse is the key cost optimization: only the **first** mode to reveal CV#8's byte value
pays a full scan (worst case 256); every later CV#8-family mode then costs a single probe.
A Direct-capable decoder (the common case) costs ~9 probes for the Direct read + 1 each for
Paged and Register. If no stage acknowledges, `supported_modes == 0` and `result` is
`DCC_SERVICE_MODE_NO_ACK`.

Detection cost is explicit — the application calls `DccServiceModeTaskDetect_detect_mode(on_detect)`
and knows it is paying that cost. The library never runs detection silently inside another
operation.

### API Design Rationale

- **Mode-specific calls, not a unified `read_cv`** — auto-detection inside `read_cv` would
  hide a potentially multi-second probe sequence behind a simple-looking call. The caller
  picks the mode; if they do not know the decoder type, they call `detect_mode` first.
- **All four modes support all four operations** — bit read/write in non-Direct modes is
  implemented by the task layer as read-modify-write (read byte, modify bit in software,
  write byte back). The packet-level primitive modules have no bit support for these modes.
- **Progress and cancellation** — any operation that iterates (read_cv in non-Direct, any
  bit op in non-Direct) can run for seconds. Progress callbacks and a cancel path are
  required in the task module interface structs.

### Library Design Goal

This library targets **Universal CV Programmer** capability (all four modes). The spec
permits tiered conformance (§F) but as a library design requirement all four modes are
supported so applications can program any decoder, including pre-2002 decoders with no
Direct mode path.

**Address-Only** writes to CV#1 must also enforce CV#29 bit 5 clear and CV#19 clear —
the task module must handle this as a compound write sequence, not a single packet.

---

## Spec Constraints and Special Cases (S-9.2.3)

### Fundamental Decoder Execution Requirement

**Two identical packets before execution (§E, line 64-65):**
"A Digital Decoder must not execute any Verify or Write operations unless it is in service
mode and has received **two identical** Service Mode packets without any intervening valid
packets." This is why command repeat counts are ≥5 (≥7 for Register mode verify) and why
the ACK scanning window opens at the Packet End bit of the **second** service mode packet.
The task orchestrator must send at least 2 identical command packets before expecting
execution or ACK.

### Timing Constraints

**20ms decoder exit timeout (§C, line 30):**
The decoder exits service mode if no valid reset or service mode packet is received within
20ms. The task orchestrator must never allow a gap of ≥20ms between packets. No callback,
state-advance code, or application handler invoked during a task may block longer than
~20ms or the decoder silently drops out of service mode mid-operation.

**Power-On Cycle (§E, line 75-80):**
After applying power to the programming track, the CS must transmit ≥20 valid packets
before initiating any service mode operation. If current draw exceeds 250mA sustained for
>100ms at any point during power-on, this is an over-current fault condition.

**100mA idle current limit after power-up (§E, line 81):**
After the power-up sequence, a decoder with all outputs turned off (lamps, amplifiers, etc.)
shall not draw more than 100mA of current except when processing an acknowledgment. Current
above this baseline during normal operation indicates a decoder fault.

**Operations mode re-entry protection (§C, line 31-33):**
After exiting service mode, the decoder only re-enters operations mode on a valid operations
mode packet NOT identical to a service mode packet. Service mode instruction packets use
short addresses 112–127 decimal — these cannot accidentally trigger operations mode re-entry.

### ACK Timing and Constraints

**Verify ACK = "shall" / Write ACK = "may" (§E, all modes):**
For all four modes, the spec is unambiguous: a VERIFY operation **shall** generate an ACK
if values match; a WRITE operation **may** generate an ACK upon completion. Write ACK is
never guaranteed. The task module must handle both the ACK and no-ACK cases for writes —
never assume a write succeeded or failed based solely on the presence or absence of ACK.

**ACK scanning window (§D, line 55-57):**
The programmer must begin scanning for ACK at the Packet End bit of the **second** service
mode instruction packet — this follows directly from the two-identical-packets requirement.
Scanning continues through all required instruction repeats and through the end of
Decoder-Recovery-Time for writes. The `on_start_ack_scan` callback in the interface struct
signals the application to begin monitoring at this exact point.

**Write ACK is delayed (§D, line 54-55):**
For write operations, the decoder must not issue ACK until all affected non-volatile storage
has been fully written. The ACK pulse may therefore arrive significantly later than the
packet transmission. The task module must not time out ACK scanning prematurely on writes.

**Decoder Recovery Time ends early on ACK (§E, line 83-84):**
"Command Station/Programmer shall send the same service mode write packets or reset packets
during the Decoder-Recovery-Time until the specified packet time has been met **or** until
the command station/programmer has received a valid acknowledgment." Recovery can be cut
short if ACK arrives — the task module does not need to send the full recovery packet count.

**CS must not stop sending packets early (§D, line 58-59):**
The CS may not stop sending packets to the programming track (which would cut power to the
decoder) until the end of Decoder-Recovery-Time, even if ACK is received before that point.

### Paged Mode Register Rules

**Page Register not reset by Reset packets (§E, line 270-272):**
"If the decoder maintains an internal Page Register copy that is only valid when power is
applied, it should NOT be initialized when a Reset packet is received." The task orchestrator
can therefore set the Page Register once at the start of a paged session and run multiple
operations without re-sending the page preset. The page register persists across reset
packets within the same power session.

**Registers 5, 7 and 8 unaffected by Page Register (§E, line 251):**
"Registers 5, 7 and 8 are unaffected by the contents of the Page Register value." CV#29
(Register 5), CV#7 (Register 7), and CV#8 (Register 8 mobile / CV#520 accessory) are always
accessible at their fixed register addresses regardless of the current page setting.

**Paged decoder detection (§E, line 205):**
"If a decoder does not implement Paged CV addressing, it must not respond to Paged CV
programming commands when the page register has a value greater than 1." The detect module
can probe for Paged support by setting page > 1 and issuing a verify — no ACK = Paged not
supported.

**Compatibility: power cycle after page preset (§E footnote 3):**
Some older decoders require a power off/on cycle after the page preset sequence and before
the CV access. This optional power cycle appears in the Register and Address-Only packet
sequences but is NOT allowed during a paged operation.

### Special Task Operations

**Hard-Reset-Cycle vs Decoder Factory Reset — two distinct operations:**

*Hard-Reset-Cycle (§E, line 90):* a hard reset (RP-9.2.1) followed by 10 idle or reset
packets. Returns the decoder to its **initial predefined state** immediately. Used when the
CS wants to reset the decoder's operating state without touching stored CVs.

*Decoder Factory Reset (§E, line 280-291):* a Physical Register write of value 8 to
Register 8. Packet: `long-preamble 0 01111111 0 00001000 0 01110111 1`. Instructs the
decoder to restore **factory default CVs** — but this does NOT happen immediately. The
actual CV restoration occurs across each subsequent power-on cycle until complete. The
decoder places 255 in CV#8 as a "restore in progress" indicator until all CVs are restored.
Follows Physical Register packet sequence. Should be exposed as:
`DccServiceModeTaskRegister_factory_reset(on_complete, on_progress)`

**Page Register cleanup after Paged programming (§E, line 275):**
After any paged CV programming session, the task module must reset the Page Register
(Register 6) to value 1 before exiting service mode. This ensures compatibility with
older Command Stations/Programmers that assume the Page Register defaults to 1.

**Register 1 special recovery time (§E, line 182-183):**
Writing to Physical Register 1 (CV#1 / address) requires 10 recovery packets, not the
standard 6. The register task module must detect writes to Register 1 and use the
extended recovery count.

### Legacy and Optional Instructions (Appendices)

**Appendix A — Address Query Instruction:**
Legacy instruction for older decoders that cannot respond to CV#1 verify operations.
Format: `long-preamble 0 AAAAAAAA 0 11111001 0 EEEEEEEE 1`. Address range 1–111 only.
Spec recommends: attempt Direct or Paged verify of CV#1 first; invoke Address Query only
on failure. Relevant to the detect module's fallback strategy for very old decoders.

**Appendix B — Service Mode Decoder Lock (optional):**
Locks all decoders on the programming track except one specified short address, allowing
programming of a single decoder without physical track isolation. A DCC product need not
implement this but if it does must implement it completely.
Format: `long-preamble 0 00000000 0 11111001 0 0aaaaaaa 0 EEEEEEEE 1` (4-byte packet).
Unlock: decoder receives any valid DCC packet other than a service mode packet or a
Decoder Lock packet with a non-matching address. If targeted at Universal CV Programmer
capability, this should be a task: `DccServiceModeTaskDetect_lock_decoder(address)`.
