# TODO

## Code Quality
- [x] Rename `_iface` to `_interface` throughout the library — violates style guide (no abbreviated names)

## Bug Fixes
- [ ] POM CV callbacks fire `AckPulseDriver_fire()` — ACK must only fire during service mode, not POM. Library calls same `on_cv_write`/`on_cv_verify`/`on_cv_bit` callbacks for both POM and service mode with no way for the application to distinguish them.
