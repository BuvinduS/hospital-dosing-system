# Changelog
 
All notable changes to this project are documented in this file.
 
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versions correspond to completed development phases, not semantic releases.
 
---
 
## [Unreleased]
 
Planned for next commit:
- Phase 1b: Water tank height and volume calculation from ultrasonic distance
---
 
## [Phase 1a] — HC-SR04 Raw Distance Reading
 
### Added
- Initial project structure: `src/main.cpp`, `platformio.ini`, `wokwi.toml`, `diagram.json`
- HC-SR04 ultrasonic sensor wired to ESP32-S3-DevKitC-1 on GPIO5 (TRIG) and GPIO17 (ECHO)
- Raw distance reading in cm printed to serial every 1 000 ms
- `pulseIn` timeout of 30 ms to prevent loop blocking on missing echo
- "No echo" diagnostic message when timeout occurs
- `$serialMonitor` connections in `diagram.json` for Wokwi VS Code serial output routing
- `.gitignore` configured for PlatformIO and Wokwi local build artifacts
- `README.md` with full system overview, architecture, phase table, and getting started guide
- `CONTRIBUTING.md` with branching rules, commit format, code style, and verification checklists
- `CHANGELOG.md` (this file)
### Fixed
- Moved ECHO pin from GPIO38 (conflicts with onboard RGB LED on ESP32-S3-DevKitC-1) to GPIO17
- Replaced `.elf`-only `wokwi.toml` with correct `firmware.bin` + `firmware.elf` pair
- Removed invalid `[serial] enabled = true` from `wokwi.toml` — not supported by local extension
### Simulation Verified
- Distance reads correctly and updates when HC-SR04 slider is moved in Wokwi
- Serial output visible in Wokwi terminal tab in VS Code
- No blocking at any slider position
---
 
*Older entries will appear here as phases are completed.*