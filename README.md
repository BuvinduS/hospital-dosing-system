# IoT Automated Chemical Dosing System for Hospitals

[![Build](https://img.shields.io/badge/build-passing-brightgreen)]()
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)]()
[![Framework](https://img.shields.io/badge/framework-Arduino-teal)]()
[![Simulation](https://img.shields.io/badge/simulation-Wokwi-purple)]()
[![License](https://img.shields.io/badge/license-MIT-orange)]()
[![Phase](https://img.shields.io/badge/phase-1e%20complete-brightgreen)]()

An industrial-grade IoT system for automated dilution, dosing, and distribution of disinfectant chemicals in hospital environments. Designed for precision, safety, and fault tolerance using distributed ESP32 nodes communicating over MQTT.

---

## Table of Contents

- [System Overview](#system-overview)
- [Hardware Architecture](#hardware-architecture)
- [Software Architecture](#software-architecture)
- [Repository Structure](#repository-structure)
- [Development Phases](#development-phases)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Installation](#installation)
  - [Running the Simulation](#running-the-simulation)
- [Sensor Formulas and Design Decisions](#sensor-formulas-and-design-decisions)
- [Fault Detection](#fault-detection)
- [MQTT Communication](#mqtt-communication)
- [Contributing](#contributing)
- [Changelog](#changelog)
- [License](#license)

---

## System Overview

Hospitals use concentrated disinfectant chemicals that must be diluted with purified water to a precise concentration before distribution to dispensing points throughout the facility. This system automates that process end-to-end.

The system:

- Continuously monitors a 2 000 L purified water tank and a 10 L concentrated chemical tank
- Mixes water and chemical to a target dilution ratio using flow-meter-verified dosing
- Distributes the mixed solution through a shared pipeline to multiple 750 mL dispensing tanks
- Automatically refills dispensing tanks when liquid falls below the minimum threshold
- Handles multiple simultaneous refill requests using a first-come-first-served queue — only one tank is refilled at a time
- Detects and alarms on faults including dry tanks, leaks, blocked valves, pump failures, stuck valves, and sensor failures
- Logs all usage and refill events with timestamps

### Key Constraints

| Constraint | Detail |
|---|---|
| Dispensing tank sensors | Single low-level sensor per tank only |
| Water tank sensing | Non-contact ultrasonic — no sensor inside tank |
| Chemical tank sensing | No sensor inside container — flow meter + load cell only |
| Pump control | PWM speed does not guarantee flow rate — flow meters are mandatory |
| Refill concurrency | Exactly one dispensing tank refilled at a time |
| Fault tolerance | All failure modes must be detected and alarmed |

---

## Hardware Architecture

### Physical Components

| Component | Specification | Purpose |
|---|---|---|
| Water tank | 2 000 L cylindrical, H = 1.60 m, r = 0.632 m | Purified water supply |
| Chemical tank | 10 L concentrated disinfectant | Chemical supply |
| Ultrasonic sensor | HC-SR04 | Non-contact water level measurement |
| Load cell | 0–12 kg, HX711 ADC | Chemical tank mass verification |
| Water flow meter | YF-S201, pulse output | Volumetric water flow measurement |
| Chemical flow meter | YF-S201, pulse output | Volumetric chemical flow measurement |
| Solenoid valve | 12 V DC, normally closed | Water line control |
| Chemical pump | PWM-controlled peristaltic | Chemical dosing |
| Dispensing tank nodes | ESP32-S3 per tank | Local level sensing + MQTT |
| Central controller | ESP32-S3 | Dosing logic, queue, fault detection |
| Alarm module | Piezo buzzer + RGB LED | Audible and visual alerts |

### Distributed Node Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Physical Layer                        │
│  ┌─────────────┐    ┌──────────────┐                   │
│  │ Water tank  │    │ Chemical tank│                   │
│  │ HC-SR04     │    │ Flow meter   │                   │
│  │ ultrasonic  │    │ + Load cell  │                   │
│  └──────┬──────┘    └──────┬───────┘                   │
│         │                  │                            │
│  ┌──────▼──────────────────▼───────┐                   │
│  │    Mixing chamber + Solenoid    │                   │
│  │    valve + PWM pump             │                   │
│  └──────────────────┬──────────────┘                   │
│                     │ Distribution pipeline             │
│         ┌───────────┼───────────┐                      │
│    ┌────▼───┐  ┌────▼───┐  ┌───▼────┐                 │
│    │ Tank 1 │  │ Tank 2 │  │ Tank N │                 │
│    │ ESP32  │  │ ESP32  │  │ ESP32  │                 │
│    └────────┘  └────────┘  └────────┘                 │
└─────────────────────────────────────────────────────────┘
                         │ Wi-Fi / MQTT
┌────────────────────────▼────────────────────────────────┐
│              Central Controller (ESP32-S3)               │
│  Queue manager │ Fault detection │ Alarm │ Data logger  │
└─────────────────────────────────────────────────────────┘
```

---

## Software Architecture

### Single Entry Point, Modular Logic

The firmware has exactly one `main.cpp` — required by the Arduino framework. All other functionality is split into header/source pairs and `#include`-d into `main.cpp`. This is the key architectural rule:

- **`main.cpp`** — the only file that touches hardware (`pinMode`, `pulseIn`, `analogRead`, `digitalWrite`, `Serial`). It calls logic functions and passes values around.
- **`include/*.h`** — function declarations. No hardware calls, no `#include <Arduino.h>`.
- **`src/*.cpp`** — logic implementations. Pure math and state — testable on a PC without any hardware.
- **`include/config.h`** — all physical constants, thresholds, and pin definitions in one place.

```
main.cpp
  │
  ├── #include "config.h"          → all constants (TANK_HEIGHT_M, TRIG_PIN, ...)
  ├── #include "water_tank.h"      → distanceToHeight(), heightToVolume(), isWaterLow()
  ├── #include "chem_tank.h"       → pulseToVolume(), massToVolume(), isMismatch()
  ├── #include "fault_manager.h"   → evaluateFaults(), getFaultSeverity()
  ├── #include "refill_queue.h"    → enqueue(), dequeue(), isEmpty()
  ├── #include "alarm_manager.h"   → setAlarm(), clearAlarm()
  └── #include "mqtt_client.h"     → publishTopic(), handleMessage()
```

This boundary is what makes unit testing possible — the test files include the same headers and call the same logic functions, with no hardware involved at all.

### Firmware Modules

| Header | Source | Responsibility |
|---|---|---|
| `include/config.h` | — | All constants, thresholds, pin definitions |
| `include/water_tank.h` | `src/water_tank.cpp` | Height/volume formulas, low-level flag |
| `include/chem_tank.h` | `src/chem_tank.cpp` | Flow accumulation, mass-to-volume, mismatch |
| `include/fault_manager.h` | `src/fault_manager.cpp` | All fault condition evaluation and severity |
| `include/refill_queue.h` | `src/refill_queue.cpp` | FIFO queue, sequential dispatch logic |
| `include/alarm_manager.h` | `src/alarm_manager.cpp` | Alarm state, severity mapping |
| `include/mqtt_client.h` | `src/mqtt_client.cpp` | Publish/subscribe, topic formatting |
| `src/main.cpp` | — | Hardware calls, `setup()`, `loop()` |

### Communication Protocol

All nodes communicate via MQTT over Wi-Fi. Topic structure:

```
dosing/
├── tank/{id}/level_low          ← tank node publishes (QoS 1)
├── tank/{id}/status             ← tank node publishes (QoS 0)
├── refill/command/{id}          ← central controller publishes (QoS 1)
├── refill/complete/{id}         ← central controller publishes (QoS 1)
├── water_tank/level             ← central controller publishes (QoS 0)
├── chem_tank/volume             ← central controller publishes (QoS 0)
├── fault/{type}                 ← central controller publishes (QoS 2)
├── log/usage                    ← central controller publishes (QoS 1)
└── heartbeat/{id}               ← all nodes publish (QoS 0)
```

All payloads are JSON:
```json
{
  "node_id": 2,
  "timestamp": 1716800000,
  "value": 0.72,
  "unit": "L",
  "status": "low",
  "firmware": "1.0.0"
}
```

---

## Repository Structure

```
iot-chemical-dosing/
│
├── src/                          # Firmware source files
│   ├── main.cpp                  # ONLY file with hardware calls — setup() + loop()
│   ├── water_tank.cpp            # Water tank logic implementation
│   ├── chem_tank.cpp             # Chemical tank logic implementation
│   ├── fault_manager.cpp         # Fault detection logic
│   ├── refill_queue.cpp          # FIFO queue implementation
│   ├── alarm_manager.cpp         # Alarm state and output logic
│   └── mqtt_client.cpp           # MQTT publish/subscribe logic
│
├── include/                      # Header files — declarations only, no hardware
│   ├── config.h                  # All constants: tank dimensions, thresholds, pins
│   ├── water_tank.h              # distanceToHeight(), heightToVolume(), isWaterLow()
│   ├── chem_tank.h               # pulseToVolume(), massToVolume(), isMismatch()
│   ├── fault_manager.h           # evaluateFaults(), FaultType enum
│   ├── refill_queue.h            # enqueue(), dequeue(), isEmpty()
│   ├── alarm_manager.h           # setAlarm(), AlarmSeverity enum
│   └── mqtt_client.h             # publishTopic(), topic string constants
│
├── test/                         # Unit tests — PlatformIO + Unity framework
│   └── native/                   # Run on PC — no hardware required
│       ├── test_water_tank/
│       │   └── test_water_tank.cpp
│       ├── test_chem_tank/
│       │   └── test_chem_tank.cpp
│       ├── test_fault_manager/
│       │   └── test_fault_manager.cpp
│       └── test_refill_queue/
│           └── test_refill_queue.cpp
│
├── lib/                          # Project-local third-party libraries (PlatformIO)
│
├── docs/                         # Project documentation
│   ├── architecture.md           # Full system architecture description
│   ├── mqtt-topics.md            # MQTT topic reference
│   ├── fault-detection.md        # Fault logic specification
│   ├── testing.md                # Testing strategy and how to run tests
│   ├── wiring/                   # Pin mapping and wiring diagrams
│   │   └── phase1-wiring.md
│   └── formulas.md               # All sensor math with derivations
│
├── diagram.json                  # Wokwi circuit diagram
├── platformio.ini                # PlatformIO build + test environment config
├── wokwi.toml                    # Wokwi VS Code extension configuration
├── .gitignore                    # Git ignore rules
├── CHANGELOG.md                  # Version history
├── CONTRIBUTING.md               # Contribution guidelines
└── README.md                     # This file
```

---

## Development Phases

This project is built and verified in modular phases. Each phase is a separate Git branch, merged to `main` only after simulation is verified.

| Phase | Branch | Status | Description |
|---|---|---|---|
| 1a | `phase/1a-ultrasonic` | ✅ Complete | HC-SR04 raw distance reading, serial output confirmed |
| 1b | `phase/1b-water-math` | ✅ Complete | Height and volume formulas, low-level fault flag |
| 1c | `phase/1c-flow-meter` | ✅ Complete | Simulated flow pulse accumulation (push button) |
| 1d | `phase/1d-load-cell` | ✅ Complete | Simulated load cell via potentiometer ADC |
| 1e | `phase/1e-crosscheck` | ✅ Complete | Flow vs load cell mismatch detection |
| 1f | `phase/1f-dashboard` | ✅ Complete | Unified serial report, all Phase 1 sensors combined |
| 2a | `phase/2a-solenoid` | ✅ Complete | Solenoid valve control with flow meter feedback |
| 2b | `phase/2b-pump` | ✅ Complete | PWM pump control with chemical flow meter |
| 2c | `phase/2c-dosing-logic` | ✅ Complete | Target volume accumulation, ratio verification |
| 3a | `phase/3a-tank-node` | 🔄 Next | Dispensing tank ESP32 node, MQTT publish |
| 3b | `phase/3b-refill-cmd` | ⏳ Pending | Refill command subscriber, local valve |
| 4a | `phase/4a-central-ctrl` | ⏳ Pending | Central controller, queue, pre-checks |
| 4b | `phase/4b-mqtt-broker` | ⏳ Pending | Full MQTT integration, all nodes |
| 4c | `phase/4c-alarms` | ⏳ Pending | Alarm manager, RGB LED, buzzer |
| 4d | `phase/4d-logging` | ⏳ Pending | Usage logging, data reporting |

### Branching Strategy

```
main                  ← stable, simulation-verified only
└── develop           ← integration branch
    ├── phase/1a-...  ← individual phase branches
    ├── phase/1b-...
    └── ...
```

Never commit directly to `main`. Every phase branch is merged via pull request after passing simulation verification.

---

## Getting Started

### Prerequisites

| Tool | Version | Purpose |
|---|---|---|
| VS Code | Latest | IDE |
| PlatformIO IDE extension | Latest | ESP32 build toolchain and test runner |
| Wokwi VS Code extension | Latest | Local circuit simulation |
| Wokwi license | Free tier | Required for local simulation |
| Git | Any | Version control |

### `platformio.ini` Configuration

The project uses two environments — one for building/simulating on the ESP32-S3, one for running unit tests natively on your PC:

```ini
[env:esp32-s3-devkitc-1]
platform      = espressif32
board         = esp32-s3-devkitc-1
framework     = arduino
monitor_speed = 115200

[env:native]
platform    = native
test_filter = native/*
```

### Installation

**1. Clone the repository**

```bash
git clone https://github.com/your-username/iot-chemical-dosing.git
cd iot-chemical-dosing
```

**2. Open in VS Code**

```bash
code .
```

PlatformIO will automatically detect `platformio.ini` and download the required toolchain and libraries on first open. This may take a few minutes.

**3. Activate your Wokwi license**

Press `Ctrl+Shift+P` → `Wokwi: Request a New License` and follow the browser prompt. A free Wokwi account is sufficient.

### Running Unit Tests

Unit tests run natively on your PC — no board, no simulation, no build wait. They test all logic functions in isolation.

**Run all native tests:**
```bash
pio test -e native
```

**Run tests for a specific module:**
```bash
pio test -e native -f native/test_water_tank
```

**Expected output:**
```
test/native/test_water_tank/test_water_tank.cpp:28:test_full_tank_gives_zero_height  [PASSED]
test/native/test_water_tank/test_water_tank.cpp:34:test_zero_distance_gives_full_height [PASSED]
...
-----------------------
10 Tests 0 Failures 0 Ignored
OK
```

**Rule:** run `pio test -e native` before every Wokwi simulation run. Tests catch logic bugs in seconds. If tests pass and simulation fails, the bug is in the hardware interaction code in `main.cpp` — not in the logic.

### Running the Simulation

**Build the firmware first — this is required before every simulation run:**

```bash
# Option A — VS Code command palette
Ctrl+Shift+P → PlatformIO: Build

# Option B — PlatformIO CLI in terminal
pio run
```

Confirm the build succeeds:
```
.pio/build/esp32-s3-devkitc-1/firmware.bin   ← must exist
.pio/build/esp32-s3-devkitc-1/firmware.elf   ← must exist
```

**Start the simulation:**

```
Ctrl+Shift+P → Wokwi: Start Simulator
```

**View serial output:**

A terminal tab named "Wokwi" appears at the bottom of VS Code. Serial output from `Serial.print()` appears here. Keep the Wokwi diagram panel visible — if it is hidden, the simulation pauses and serial output stops.

**Interact with sensors:**

- Drag the HC-SR04 distance slider in the diagram to simulate changing water level
- (Phase 1c+) Press the green button to inject flow meter pulses
- (Phase 1d+) Turn the potentiometer to simulate load cell mass readings

### Expected Serial Output (Phase 1a)

```
HC-SR04 Ultrasonic Sensor Test — ready
----------------------------------------------
Distance: 40.0 cm
Distance: 40.0 cm
Distance: 65.3 cm      ← slider moved
Distance: 65.3 cm
```

---

## Sensor Formulas and Design Decisions

### Water Tank — Ultrasonic Level Sensing

The HC-SR04 is mounted at the top of the tank pointing downward. It measures the empty air gap between itself and the water surface.

**Water height:**
```
h = H - d
```
- `H` = total tank internal height (m) = 1.60 m
- `d` = measured distance from sensor to water surface (m)
- `h` = water column height (m)

**Volume:**
```
V = π × r² × h
```
- `r` = tank internal radius (m) = 0.632 m
- `V` = volume in m³, multiply by 1 000 for litres

At full capacity: `V = π × 0.632² × 1.60 × 1000 ≈ 2 004 L` ✓

**Why ultrasonic?**
- Non-contact — no sensor enters the tank, satisfying the constraint
- Maintenance friendly — sensor is external and accessible
- Unaffected by water chemistry or temperature in this range
- Works for both opaque liquids and clear water

### Chemical Tank — Hybrid Estimation

No sensor can be placed inside the chemical container. Two independent methods are used and cross-checked.

**Primary — flow meter accumulation:**
```
V_remaining = V_initial - Σ(V_dispensed)
```
Each flow meter pulse represents a calibrated volume. Pulses are counted in interrupt-driven ISRs to prevent missed counts at high flow rates.

**Secondary — load cell verification:**
```
V_load_cell = mass_reading / density
```
- Density of concentrate ≈ 1.05 kg/L
- Full container mass ≈ 10.5 kg, tare weight subtracted at startup

**Mismatch detection:**
```
|V_flow_estimate - V_load_cell| > ε  →  LEAK or SPILL fault
```
Tolerance `ε` = 200 mL (accounts for density variation and sensor noise).

---

## Fault Detection

| Fault | Detection Method | Trigger Condition | Severity | Response |
|---|---|---|---|---|
| Dry water tank | Ultrasonic + flow timeout | `h < h_min` AND `Q = 0` during open valve | Critical | Stop all ops, alarm |
| Chemical tank dry | Flow accumulation + load cell | `V_remaining < 0.5 L` | Warning | Pause queue, alarm |
| Leak / spill | Load cell vs flow meter delta | `|ΔV| > 200 mL` after dose | Warning | Log, manual inspect |
| Blocked valve | Flow meter during open command | Valve open, `Q < 0.1 L/min` for 5 s | Critical | Close, retry once |
| Stuck-open valve | Flow meter after close command | Valve closed, `Q > 0.05 L/min` | Critical | Alarm, block ops |
| Pump failure | Chemical flow meter during PWM | PWM > 0%, `Q_chem = 0` for 3 s | Critical | Stop dose, alarm |
| Sensor failure | Heartbeat timeout + range check | No heartbeat 30 s, or NaN value | Warning | Disable node, skip |
| Concentration error | Flow ratio check | `|R_actual - R_target| > 5%` | Warning | Log, discard if critical |

### Alarm Outputs

- **Red LED** — critical fault, all operations halted
- **Amber LED** — warning, operations continue if safe
- **Green LED** — system normal
- **Blue LED** — dosing cycle active
- **Buzzer** — short repeating beeps for warnings, continuous tone for critical faults

---

## MQTT Communication

See [`docs/mqtt-topics.md`](docs/mqtt-topics.md) for the full topic reference.

### Broker Configuration

| Parameter | Value |
|---|---|
| Broker | Mosquitto (local) or EMQX |
| Port | 1883 (unencrypted), 8883 (TLS) |
| Protocol | MQTT v3.1.1 |
| Keep-alive | 60 s |
| QoS for faults | 2 (exactly once) |
| QoS for telemetry | 0 (fire and forget) |

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the full contribution guide including branching rules, commit message format, and simulation verification checklist.

---

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for the full version history.

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

*Built as part of an IoT engineering project. Designed to production-grade standards for educational and simulation purposes.*