# Contributing Guide
 
Thank you for contributing to the IoT Chemical Dosing System. This document defines the standards for all contributions — code, documentation, and simulation verification. Following these rules keeps the project clean and traceable as it grows across many phases.
 
---
 
## Table of Contents
 
- [Branching Strategy](#branching-strategy)
- [Commit Message Format](#commit-message-format)
- [Code Style](#code-style)
- [Simulation Verification Checklist](#simulation-verification-checklist)
- [Pull Request Process](#pull-request-process)
- [Documentation Standards](#documentation-standards)
---
 
## Branching Strategy
 
### Branch Structure
 
```
main          ← stable, simulation-verified builds only
develop       ← integration branch — all phase branches merge here first
phase/Nx-*    ← individual development phase branches
fix/*         ← bug fixes against develop or main
docs/*        ← documentation-only changes
```
 
### Rules
 
- **Never commit directly to `main`** — all changes arrive via pull request
- **Never commit directly to `develop`** — always branch off it
- Each phase gets its own branch, named exactly as listed in the README phase table
- A phase branch is only merged after its simulation verification checklist passes in full
- One branch = one phase. Do not combine multiple phases into one branch
### Creating a Phase Branch
 
```bash
git checkout develop
git pull origin develop
git checkout -b phase/1b-water-math
```
 
### Merging Back
 
```bash
# After simulation verification passes:
git checkout develop
git merge --no-ff phase/1b-water-math -m "merge: phase 1b water tank math complete"
git push origin develop
```
 
Use `--no-ff` (no fast-forward) so the merge commit is always recorded. This keeps the history readable.
 
---
 
## Commit Message Format
 
Every commit message follows this structure:
 
```
<type>(<scope>): <short summary>
 
[optional body — wrap at 72 characters]
 
[optional footer — e.g. Closes #12]
```
 
### Types
 
| Type | When to use |
|---|---|
| `feat` | New functionality added |
| `fix` | Bug or fault correction |
| `sim` | Simulation-related changes (diagram.json, wokwi.toml) |
| `docs` | Documentation only — no code changes |
| `refactor` | Code restructured without behaviour change |
| `test` | Test additions or changes |
| `chore` | Build config, .gitignore, platformio.ini changes |
| `wip` | Work in progress — do not merge in this state |
 
### Scope
 
The scope is the phase or module affected. Examples: `1a`, `water-tank`, `fault-manager`, `mqtt`, `readme`.
 
### Examples
 
```
feat(1b): add water height and volume calculation from ultrasonic
 
Implements h = H - d and V = pi*r^2*h.
Tank constants defined in config.h.
Serial output shows distance, height, and volume each second.
```
 
```
fix(1a): add pulseIn timeout to prevent loop blocking
 
Without a 30ms timeout, pulseIn blocked indefinitely when
no echo was received, silencing serial output entirely.
```
 
```
sim(1b): update diagram.json to use GPIO5 and GPIO17
 
GPIO38 conflicts with onboard RGB LED on ESP32-S3-DevKitC-1.
Moved TRIG to GPIO5, ECHO to GPIO17.
```
 
```
chore: add $serialMonitor connections to diagram.json
 
Required for Wokwi VS Code extension to route Serial output
to the Wokwi terminal tab.
```
 
### Rules
 
- Summary line: 72 characters maximum, imperative mood ("add", not "added")
- No full stop at the end of the summary line
- Body explains *why*, not *what* — the diff shows what
- Reference issue numbers in the footer when applicable
---
 
## Code Style
 
### General
 
- Language: C++11 (Arduino framework on ESP32)
- Indentation: 2 spaces — no tabs
- Line length: 100 characters maximum
- All files must begin with a block comment describing purpose, phase, and author
### File Header Template
 
```cpp
// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Phase 1b: Water tank height and volume calculation
//
//  Board:     ESP32-S3-DevKitC-1
//  Simulator: Wokwi (VS Code extension)
//  Author:    Your Name
//  Date:      YYYY-MM-DD
// ============================================================
```
 
### Naming Conventions
 
| Construct | Convention | Example |
|---|---|---|
| Constants | `UPPER_SNAKE_CASE` | `TANK_HEIGHT_M` |
| Global variables | `camelCase` with type prefix | `floatChemVolRemaining` |
| Functions | `camelCase` | `readWaterTank()` |
| Structs | `PascalCase` | `WaterReading` |
| Pins | `UPPER_SNAKE_CASE` with `_PIN` suffix | `TRIG_PIN` |
 
### Physical Constants
 
All physical constants (tank dimensions, thresholds, pin numbers) must be defined in `include/config.h`, not hardcoded in `.cpp` files. This makes it easy to reconfigure for different tank sizes or pin assignments without hunting through source files.
 
```cpp
// include/config.h
#pragma once
 
// ── Water tank ───────────────────────────────────────────────
const float TANK_HEIGHT_M     = 1.60f;
const float TANK_RADIUS_M     = 0.632f;
const float TANK_MIN_LEVEL_M  = 0.10f;
 
// ── Pins ─────────────────────────────────────────────────────
#define TRIG_PIN   5
#define ECHO_PIN  17
```
 
### Comments
 
- Comment *why*, not *what*
- Every function needs a one-line description comment above it
- Non-obvious formulas must cite the source or show the derivation
---
 
## Simulation Verification Checklist
 
Before merging any phase branch, every item in the relevant checklist must be confirmed working in the Wokwi simulation. Record the result as a comment in the pull request.
 
### All Phases — Pre-flight
 
- [ ] `PlatformIO: Build` completes with zero errors and zero warnings
- [ ] `firmware.bin` and `firmware.elf` exist in `.pio/build/esp32-s3-devkitc-1/`
- [ ] Wokwi simulation starts without error
- [ ] Serial output appears in the Wokwi terminal tab within 3 seconds of simulation start
### Phase 1a — HC-SR04 Raw Distance
 
- [ ] Serial prints distance in cm every second
- [ ] Distance value changes when the sensor slider is moved
- [ ] "No echo" message appears when slider is at maximum (out of range)
- [ ] No blocking / frozen output at any slider position
### Phase 1b — Water Tank Math
 
- [ ] Height formula: distance 0 cm → height = H (full tank)
- [ ] Height formula: distance = H → height = 0 (empty tank)
- [ ] Volume at full: approximately 2 000 L
- [ ] Volume at empty: 0 L
- [ ] Low-level fault flag triggers when height < 10 cm
- [ ] Fault LED illuminates when flag is set
### Phase 1c — Chemical Flow Meter
 
- [ ] Pulse count increments correctly on each button press
- [ ] Accumulated volume increases by the correct mL per pulse
- [ ] Total dispensed volume prints correctly to serial
- [ ] Volume never goes below zero
### Phase 1d — Load Cell (Potentiometer)
 
- [ ] ADC reads full range (0–4095) across potentiometer travel
- [ ] Mass calculation maps correctly: pot at 0% → 0 kg, pot at 100% → 12 kg
- [ ] Volume estimate from mass is correct: mass / density
- [ ] Value updates every second in serial output
### Phase 1e — Cross-check and Mismatch
 
- [ ] No mismatch flag when flow estimate and load cell agree within 200 mL
- [ ] Mismatch flag triggers when pot is adjusted to disagree by more than 200 mL
- [ ] Mismatch LED and serial message both appear on fault
- [ ] Mismatch clears when values are brought back into agreement
### Phase 1f — Unified Dashboard
 
- [ ] All sensor readings appear in a single report per second
- [ ] Report format is clean and consistent
- [ ] All fault flags visible in the report
- [ ] Status line shows OK or FAULT correctly
---
 
## Pull Request Process
 
1. Ensure all simulation verification checklist items for the phase are checked
2. Update `CHANGELOG.md` with a brief entry under `[Unreleased]`
3. Update the phase table in `README.md` — change status from ⏳ to ✅
4. Open a pull request from your phase branch into `develop`
5. Title format: `Phase 1b: Water tank height and volume calculation`
6. In the PR description, paste the simulation verification checklist with all items checked
7. Assign yourself as the author
8. Merge using **Squash and merge** for single-commit phases, or **Merge commit** for multi-commit phases where history is worth preserving
---
 
## Documentation Standards
 
- All documentation lives in `docs/` except `README.md`, `CONTRIBUTING.md`, and `CHANGELOG.md` which are in the project root
- Use Markdown for all documentation
- Code blocks must specify the language (` ```cpp `, ` ```json `, ` ```bash `)
- All formulas must be written in both plain-text inline form and a readable block form
- Wiring changes must be reflected in `docs/wiring/` before the phase branch is merged