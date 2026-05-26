// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Chemical pump logic — implementation
// ============================================================
#include "pump.h"

// Dilution ratio: 1:50 means 1 part chemical per 50 parts water
// ratio = 1/51 (chemical fraction of total volume)
// e.g. for 600mL total: V_chem = 600 * (1/51) ≈ 11.76mL
float chemTargetVolume(float water_target_L, float ratio) {
    if (ratio <= 0.0f || ratio >= 1.0f) return 0.0f;
    // water_target is the water fraction, derive total then chem
    // V_water = total * (1 - ratio)
    // total   = V_water / (1 - ratio)
    // V_chem  = total * ratio
    float total = water_target_L / (1.0f - ratio);
    return total * ratio;
}

// Returns true when no chemical flow detected after grace period
bool isPumpFailure(float chem_dispensed_L, unsigned long elapsed_ms,
    unsigned long grace_ms) {
    if (elapsed_ms < grace_ms) return false;
    return chem_dispensed_L <= 0.0f;
}

// Checks actual ratio against target within tolerance
// actual_ratio = V_chem / (V_chem + V_water)
bool isRatioCorrect(float water_dispensed_L, float chem_dispensed_L,
    float target_ratio, float tolerance) {
    float total = water_dispensed_L + chem_dispensed_L;
    if (total <= 0.0f)            return false;
    if (chem_dispensed_L <= 0.0f) return false;
    float actual_ratio = chem_dispensed_L / total;
    // Use relative tolerance — tolerance is a fraction of the target ratio
    // e.g. tolerance=0.10 means actual must be within 10% of target ratio
    float delta = actual_ratio - target_ratio;
    if (delta < 0.0f) delta = -delta;
    return delta <= (target_ratio * tolerance);
}

bool isChemTargetReached(float chem_dispensed_L, float chem_target_L) {
    return chem_dispensed_L >= chem_target_L;
}