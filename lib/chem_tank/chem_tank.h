// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Chemical tank logic — declarations
//  No hardware dependencies — safe to include in unit tests
// ============================================================
# pragma once

// ── Volume tracking ──────────────────────────────────────────

// Convert pulse count to millilitres
// ml_per_pulse is a calibration constant for the specific flow meter
float pulseToVolume(long pulseCount, float ml_per_pulse);

// Update the chemical level by subtracting the dispensed volume from the remaining volume
float updateRemaining(float current_remaining_L, float dispensed_L);

// ── Load cell ──────────────────────────────────────────

// Converts raw ADC reading to mass in kg
// adc_max is the ADC resolution (4095 for 12-bit)
// scale_max_kg is the load cell full scale rating
float adcToMass(int adc_raw, int adc_max, float scale_max_kg);

float massToVolume(float mass_kg, float density_kg_per_L);

// ── Cross check ──────────────────────────────────────────

// Returns true when the difference between flow estimate and
// load cell estimate exceeds the tolerance threshold
bool isMismatch(float flow_estimate_L, float loadcell_estimate_L, float tolerance_L);

// ── Level Check and Alerts ──────────────────────────────────────────

bool isChemLow(float remainingVolume_L, float threshold_L);