# Changelog

All notable changes to this project are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versions correspond to completed development phases, not semantic releases.

---

## [Unreleased]

Planned for next commit:
- Phase 2a: Solenoid valve control with water flow meter feedback
- `lib/solenoid/` module — valve open/close logic, flow verification
- `test/native/test_solenoid/` unit tests

---

## [Phase 1f] — Config Consolidation and Serial Dashboard

### Added
- `include/config.h` — single source of truth for all compile-time
  constants: pin definitions, tank dimensions, thresholds, calibration
  values, and timing constants
- `DEBOUNCE_MS` constant in `config.h` — replaces magic number hardcoded
  in ISR

### Changed
- `src/main.cpp` — all `static constexpr` constants removed, replaced
  with `#include "config.h"`
- Serial dashboard reformatted — fixed ASCII separator lines, timestamp
  on every cycle header, all sensor reads grouped before any printing,
  fault summary computed before output, water and chemical readings each
  on their own indented sub-line
- File header comment updated from Phase 1e to Phase 1f

### Simulation Verified
- `pio test -e native`: 52 Tests 0 Failures 0 Ignored
- Dashboard format stable and correctly aligned in Wokwi terminal
- All sensor interactions confirmed: HC-SR04 slider, push button
  pulses, potentiometer load cell, fault LED and OK LED

---

## [Phase 1e] — Chemical Tank Sensing and Interrupt-Driven Pulse Counting

### Added
- `lib/chem_tank/chem_tank.h` and `lib/chem_tank/chem_tank.cpp` —
  chemical tank logic module with six functions: `pulseToVolume()`,
  `updateRemaining()`, `adcToMass()`, `massToVolume()`, `isMismatch()`,
  `isChemLow()`
- `test/native/test_chem_tank/test_chem_tank.cpp` — 34 unit tests
  covering all functions, boundary conditions, clamping, dispense cycle
  simulation, and leak detection scenario
- Interrupt-driven pulse counting via `attachInterrupt()` and
  `IRAM_ATTR` ISR — replaces polling approach which missed pulses
  during `pulseIn()` blocking
- ISR-level debounce using `millis()` timestamp comparison (50ms window)
- `volatile` qualifiers on `pendingPulses` and `lastDebounce` for
  ISR-safe shared variable access
- Atomic pulse drain in `processPendingPulses()` using `noInterrupts()`
  / `interrupts()` guard
- Load cell simulated via potentiometer on GPIO6 (ADC)
- Flow meter simulated via push button on GPIO4 (interrupt on FALLING
  edge)
- Mismatch detection cross-checks flow estimate against load cell
  estimate with 200mL tolerance

### Changed
- Phases 1c and 1d implemented within this phase — flow meter
  accumulation and load cell ADC reading built and verified together
  with cross-check logic
- `main.cpp` updated with `readLoadCellVolume()` and
  `processPendingPulses()` hardware interface functions
- TODO comment updated from Phase 1b to Phase 1f for `config.h`
  migration

### Simulation Verified
- `pio test -e native`: 52 Tests 0 Failures 0 Ignored
- HC-SR04 slider updates water height and volume correctly
- Button press registers as single pulse with interrupt debounce
- Potentiometer adjusts load cell estimate in real time
- Mismatch fault triggers when flow estimate and load cell
  diverge > 200mL
- Mismatch clears when potentiometer adjusted back into agreement
- Fault LED and OK LED respond correctly to system status

---

## [Phase 1b] — Water Tank Logic and Unit Tests

### Added
- `lib/water_tank/water_tank.h` — declarations for water tank logic
  functions
- `lib/water_tank/water_tank.cpp` — implementations:
  `distanceToHeight()`, `heightToVolume()`, `isWaterLow()`
- `test/native/test_water_tank/test_water_tank.cpp` — 18 unit tests
  covering all functions, boundary conditions, clamping, and
  end-to-end pipeline

### Changed
- Project module structure moved to `lib/` — each logic module now
  lives in `lib/<module>/` so PlatformIO compiles it under both ESP32
  and native environments without extra configuration
- `include/water_tank.h` removed — replaced by
  `lib/water_tank/water_tank.h`
- `src/water_tank.cpp` removed — replaced by
  `lib/water_tank/water_tank.cpp`
- `platformio.ini` — removed `build_src_filter` (no longer needed
  with `lib/`)
- Tank constants updated to match spec: `H = 10.19m`, `r = 0.25m`
  (diameter 50cm, volume ~2001L)

### Fixed
- `distanceToHeight()` now clamps result to 0 when sensor distance
  exceeds tank height — prevents negative height and volume values
  from sensor noise
- `isWaterLow()` uses `<=` boundary — flag triggers at exactly the
  threshold, not only below it

### Simulation Verified
- `pio test -e native`: 18 Tests 0 Failures 0 Ignored

---

## [Phase 1a] — HC-SR04 Raw Distance Reading

### Added
- Initial project structure: `src/main.cpp`, `platformio.ini`,
  `wokwi.toml`, `diagram.json`
- HC-SR04 ultrasonic sensor wired to ESP32-S3-DevKitC-1 on GPIO5
  (TRIG) and GPIO17 (ECHO)
- Raw distance reading in cm printed to serial every 1000ms
- `pulseIn` timeout of 30ms to prevent loop blocking on missing echo
- "No echo" diagnostic message when timeout occurs
- `$serialMonitor` connections in `diagram.json` for Wokwi VS Code
  serial output routing
- `.gitignore` configured for PlatformIO and Wokwi local build
  artifacts
- `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md` added

### Fixed
- Moved ECHO pin from GPIO38 (conflicts with onboard RGB LED on
  ESP32-S3-DevKitC-1) to GPIO17
- Replaced `.elf`-only `wokwi.toml` with correct `firmware.bin` +
  `firmware.elf` pair
- Removed invalid `[serial] enabled = true` from `wokwi.toml`

### Simulation Verified
- Distance reads correctly and updates when HC-SR04 slider is moved
- Serial output visible in Wokwi terminal tab in VS Code
- No blocking at any slider position


*Older entries will appear here as phases are completed.*