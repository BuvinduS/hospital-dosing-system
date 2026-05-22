// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Chemical tank logic — implementation
// ============================================================

#include "chem_tank.h"

float pulseToVolume(long pulseCount, float ml_per_pulse) {
    if (pulseCount < 0 || ml_per_pulse <= 0.0f) {
        return 0.0f;  // guard against invalid input
    }
    return (pulseCount * ml_per_pulse) / 1000.0f;  // convert to litres
}

float updateRemaining(float current_remaining_L, float dispensed_L) {
    float result = current_remaining_L - dispensed_L;
    return result < 0.0f ? 0.0f : result;
}

// Maps ADC reading linearly to mass
// Formula: mass = (adc_raw / adc_max) × scale_max_kg
float adcToMass(int adc_raw, int adc_max, float scale_max_kg) {
    if (adc_raw < 0)        adc_raw = 0;
    if (adc_raw > adc_max)  adc_raw = adc_max;
    return ((float)adc_raw / (float)adc_max) * scale_max_kg;
}

float massToVolume(float mass_kg, float density_kg_per_L) {
    if (density_kg_per_L <= 0.0f) return 0.0f;  // guard against division by zero
    if (mass_kg < 0.0f)           return 0.0f;
    return mass_kg / density_kg_per_L;
}

bool isMismatch(float flow_estimate_L, float loadcell_estimate_L, float tolerance_L) {
    float delta = flow_estimate_L - loadcell_estimate_L;
    if (delta < 0.0f) delta = -delta;  // absolute value without using <cmath>
    return delta > tolerance_L;
}

bool isChemLow(float remainingVolume_L, float threshold_L) {
    return remainingVolume_L <= threshold_L;
}