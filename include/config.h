// ============================================================
//  IoT Chemical Dosing System — Hospital
//  System configuration — all physical constants and pin definitions
//
//  This is the single source of truth for all compile-time
//  constants. Change tank dimensions, thresholds, pin assignments,
//  and calibration values here only.
//
//  Rules:
//    - No hardware calls in this file
//    - No #include <Arduino.h> — must be safe for native builds
//    - Runtime state variables stay in main.cpp
// ============================================================
#pragma once

// ── Pin definitions ──────────────────────────────────────────
#define TRIG_PIN        5     // HC-SR04 trigger
#define ECHO_PIN        17    // HC-SR04 echo
#define FLOWPULSE_PIN   4     // flow meter pulse input (button in simulation)
#define LOADCELL_PIN    6     // load cell ADC input (potentiometer in simulation)
#define LED_FAULT       15    // red fault indicator LED
#define LED_OK          16    // green status LED

// ── Water tank ───────────────────────────────────────────────
// Geometry derived from spec: V ≈ 2000L, diameter = 50cm
// h = V / (pi * r^2) = 2.0 / (pi * 0.25^2) = 10.19m
constexpr float SPEED_OF_SOUND_M_PER_US = 0.000343f; // 343 m/s expressed in m/µs
constexpr float TANK_HEIGHT_M = 10.19f;    // total internal height (m)
constexpr float TANK_RADIUS_M = 0.25f;     // internal radius (m) — diameter 50cm
constexpr float TANK_MIN_LEVEL_M = 0.10f;     // low level alarm threshold (m)

// ── Chemical tank ─────────────────────────────────────────────
constexpr float CHEM_INITIAL_VOL_L = 10.0f;   // starting volume (L)
constexpr float CHEM_LOW_THRESHOLD_L = 0.50f;   // low stock alarm threshold (L)
constexpr float CHEM_DENSITY_KG_L = 1.05f;   // concentrate density (kg/L)
constexpr float MISMATCH_TOLERANCE_L = 0.20f;   // max flow vs load cell delta (L)
constexpr float ML_PER_PULSE = 5.0f;    // flow meter calibration (mL per pulse)
constexpr float LOADCELL_MAX_KG = 12.0f;   // load cell full scale rating (kg)
constexpr int   ADC_MAX = 4095;     // 12-bit ADC maximum value

// ── Timing ────────────────────────────────────────────────────
constexpr unsigned long REPORT_MS = 1000;    // serial report interval (ms)
constexpr unsigned long DEBOUNCE_MS = 50;      // ISR debounce window (ms)