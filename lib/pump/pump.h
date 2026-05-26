// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Chemical pump logic — declarations
//  No hardware dependencies — safe to include in unit tests
// ============================================================
#pragma once

// Returns required chemical volume from water volume and dilution ratio
// ratio = V_chem / (V_chem + V_water) e.g. 1/51 for 1:50 dilution
float chemTargetVolume(float water_target_L, float ratio);

// Returns true when chemical pump has failed to deliver flow
// within the grace period after being commanded on
bool isPumpFailure(float chem_dispensed_L, unsigned long elapsed_ms,
    unsigned long grace_ms);

// tolerance is a fraction of target_ratio, not an absolute value
// e.g. 0.10 means actual ratio must be within 10% of target ratio
bool isRatioCorrect(float water_dispensed_L, float chem_dispensed_L, float target_ratio, float tolerance);

// Returns true when chemical target volume has been reached
bool isChemTargetReached(float chem_dispensed_L, float chem_target_L);