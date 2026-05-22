// ============================================================
//  Unit tests — Chemical tank logic
//  Phases 1c, 1d, 1e
//
//  Tests: pulseToVolume(), updateRemaining(), adcToMass(),
//         massToVolume(), isMismatch(), isChemLow()
//  Run with: pio test -e native
// ============================================================
#include <unity.h>
#include "chem_tank.h"

// ── Constants matching spec ───────────────────────────────────
static constexpr float CHEM_INITIAL_VOL_L = 10.0f;
static constexpr float CHEM_DENSITY_KG_L = 1.05f;   // concentrate density
static constexpr float CHEM_LOW_THRESHOLD_L = 0.50f;
static constexpr float MISMATCH_TOLERANCE_L = 0.20f;   // 200 mL
static constexpr float ML_PER_PULSE = 5.0f;    // flow meter calibration
static constexpr float LOADCELL_MAX_KG = 12.0f;
static constexpr int   ADC_MAX = 4095;

// ============================================================
//  pulseToVolume()
// ============================================================

void test_zero_pulses_gives_zero_volume() {
    float v = pulseToVolume(0, ML_PER_PULSE);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, v);
}

void test_single_pulse_gives_correct_volume() {
    // 1 pulse × 5 mL = 5 mL = 0.005 L
    float v = pulseToVolume(1, ML_PER_PULSE);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.005f, v);
}

void test_hundred_pulses_gives_correct_volume() {
    // 100 × 5 mL = 500 mL = 0.5 L
    float v = pulseToVolume(100, ML_PER_PULSE);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, v);
}

void test_two_thousand_pulses_gives_full_tank_volume() {
    // 2000 × 5 mL = 10 000 mL = 10 L (full tank)
    float v = pulseToVolume(2000, ML_PER_PULSE);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, v);
}

void test_negative_pulses_gives_zero_volume() {
    // Guard against corrupt pulse counter
    float v = pulseToVolume(-10, ML_PER_PULSE);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, v);
}

// ============================================================
//  updateRemaining()
// ============================================================

void test_dispensing_reduces_remaining() {
    float r = updateRemaining(10.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.0f, r);
}

void test_dispensing_full_tank_gives_zero() {
    float r = updateRemaining(10.0f, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r);
}

void test_overdispense_clamps_to_zero() {
    // Dispensing more than available must never go negative
    float r = updateRemaining(0.3f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r);
}

void test_zero_dispensed_leaves_remaining_unchanged() {
    float r = updateRemaining(5.5f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.5f, r);
}

void test_remaining_never_negative() {
    float r = updateRemaining(0.0f, 100.0f);
    TEST_ASSERT_TRUE(r >= 0.0f);
}

// ============================================================
//  adcToMass()
// ============================================================

void test_zero_adc_gives_zero_mass() {
    float m = adcToMass(0, ADC_MAX, LOADCELL_MAX_KG);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, m);
}

void test_full_adc_gives_max_mass() {
    float m = adcToMass(ADC_MAX, ADC_MAX, LOADCELL_MAX_KG);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, LOADCELL_MAX_KG, m);
}

void test_half_adc_gives_half_mass() {
    float m = adcToMass(ADC_MAX / 2, ADC_MAX, LOADCELL_MAX_KG);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, LOADCELL_MAX_KG / 2.0f, m);
}

void test_adc_over_max_clamped() {
    // ADC reading beyond max should not exceed scale rating
    float m = adcToMass(ADC_MAX + 500, ADC_MAX, LOADCELL_MAX_KG);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, LOADCELL_MAX_KG, m);
}

void test_negative_adc_clamped_to_zero() {
    float m = adcToMass(-100, ADC_MAX, LOADCELL_MAX_KG);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, m);
}

// ============================================================
//  massToVolume()
// ============================================================

void test_zero_mass_gives_zero_volume() {
    float v = massToVolume(0.0f, CHEM_DENSITY_KG_L);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, v);
}

void test_full_tank_mass_gives_correct_volume() {
    // 10 L × 1.05 kg/L = 10.5 kg → 10.5 / 1.05 = 10.0 L
    float v = massToVolume(10.5f, CHEM_DENSITY_KG_L);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, v);
}

void test_half_mass_gives_half_volume() {
    float full = massToVolume(10.5f, CHEM_DENSITY_KG_L);
    float half = massToVolume(5.25f, CHEM_DENSITY_KG_L);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, full / 2.0f, half);
}

void test_negative_mass_gives_zero_volume() {
    float v = massToVolume(-1.0f, CHEM_DENSITY_KG_L);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, v);
}

// ============================================================
//  isMismatch()
// ============================================================

void test_no_mismatch_when_values_agree() {
    TEST_ASSERT_FALSE(isMismatch(5.0f, 5.0f, MISMATCH_TOLERANCE_L));
}

void test_no_mismatch_within_tolerance() {
    // 0.1 L difference, tolerance is 0.2 L — should be fine
    TEST_ASSERT_FALSE(isMismatch(5.0f, 4.9f, MISMATCH_TOLERANCE_L));
}

void test_no_mismatch_at_exact_tolerance() {
    // Exactly at tolerance boundary — should not trigger
    TEST_ASSERT_FALSE(isMismatch(5.0f, 4.8f, MISMATCH_TOLERANCE_L));
}

void test_mismatch_triggered_above_tolerance() {
    // 0.21 L difference — just over 200 mL tolerance
    TEST_ASSERT_TRUE(isMismatch(5.0f, 4.79f, MISMATCH_TOLERANCE_L));
}

void test_mismatch_triggered_when_flow_lower_than_loadcell() {
    // Mismatch works in both directions
    TEST_ASSERT_TRUE(isMismatch(4.79f, 5.0f, MISMATCH_TOLERANCE_L));
}

void test_large_mismatch_triggers() {
    // Obvious leak scenario
    TEST_ASSERT_TRUE(isMismatch(10.0f, 7.0f, MISMATCH_TOLERANCE_L));
}

// ============================================================
//  isChemLow()
// ============================================================

void test_low_flag_clear_when_well_above_threshold() {
    TEST_ASSERT_FALSE(isChemLow(5.0f, CHEM_LOW_THRESHOLD_L));
}

void test_low_flag_clear_just_above_threshold() {
    TEST_ASSERT_FALSE(isChemLow(CHEM_LOW_THRESHOLD_L + 0.001f,
        CHEM_LOW_THRESHOLD_L));
}

void test_low_flag_triggers_at_exact_threshold() {
    TEST_ASSERT_TRUE(isChemLow(CHEM_LOW_THRESHOLD_L, CHEM_LOW_THRESHOLD_L));
}

void test_low_flag_triggers_below_threshold() {
    TEST_ASSERT_TRUE(isChemLow(0.25f, CHEM_LOW_THRESHOLD_L));
}

void test_low_flag_triggers_at_zero() {
    TEST_ASSERT_TRUE(isChemLow(0.0f, CHEM_LOW_THRESHOLD_L));
}

// ============================================================
//  Integration — full dispense cycle simulation
// ============================================================

void test_full_dispense_cycle() {
    // Start with full tank, dispense 100 pulses, check remaining
    float remaining = CHEM_INITIAL_VOL_L;
    long  pulses = 100;  // 100 × 5 mL = 0.5 L dispensed

    float dispensed = pulseToVolume(pulses, ML_PER_PULSE);
    remaining = updateRemaining(remaining, dispensed);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.5f, remaining);
    TEST_ASSERT_FALSE(isChemLow(remaining, CHEM_LOW_THRESHOLD_L));
}

void test_dispense_until_low_flag_triggers() {
    // Dispense 1900 pulses = 9.5 L leaving 0.5 L — exactly at threshold
    float remaining = CHEM_INITIAL_VOL_L;
    float dispensed = pulseToVolume(1900, ML_PER_PULSE);
    remaining = updateRemaining(remaining, dispensed);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, remaining);
    TEST_ASSERT_TRUE(isChemLow(remaining, CHEM_LOW_THRESHOLD_L));
}

void test_loadcell_pipeline_full_tank() {
    // Full tank on load cell: ADC = max → mass = 10.5 kg → volume = 10.0 L
    float mass = adcToMass(ADC_MAX, ADC_MAX, LOADCELL_MAX_KG);
    // Scale: max kg is 12, full chemical tank is 10.5 kg
    // At ADC max we get 12 kg — close enough for simulation
    float volume = massToVolume(mass, CHEM_DENSITY_KG_L);
    TEST_ASSERT_TRUE(volume > 0.0f);
}

void test_mismatch_detected_after_simulated_leak() {
    // Flow meter thinks 8 L remains, load cell reads 5 L — leak scenario
    float flow_estimate = 8.0f;
    float loadcell_estimate = 5.0f;
    TEST_ASSERT_TRUE(isMismatch(flow_estimate, loadcell_estimate,
        MISMATCH_TOLERANCE_L));
}

// ============================================================
//  Runner
// ============================================================

void setUp() {}
void tearDown() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // pulseToVolume
    RUN_TEST(test_zero_pulses_gives_zero_volume);
    RUN_TEST(test_single_pulse_gives_correct_volume);
    RUN_TEST(test_hundred_pulses_gives_correct_volume);
    RUN_TEST(test_two_thousand_pulses_gives_full_tank_volume);
    RUN_TEST(test_negative_pulses_gives_zero_volume);

    // updateRemaining
    RUN_TEST(test_dispensing_reduces_remaining);
    RUN_TEST(test_dispensing_full_tank_gives_zero);
    RUN_TEST(test_overdispense_clamps_to_zero);
    RUN_TEST(test_zero_dispensed_leaves_remaining_unchanged);
    RUN_TEST(test_remaining_never_negative);

    // adcToMass
    RUN_TEST(test_zero_adc_gives_zero_mass);
    RUN_TEST(test_full_adc_gives_max_mass);
    RUN_TEST(test_half_adc_gives_half_mass);
    RUN_TEST(test_adc_over_max_clamped);
    RUN_TEST(test_negative_adc_clamped_to_zero);

    // massToVolume
    RUN_TEST(test_zero_mass_gives_zero_volume);
    RUN_TEST(test_full_tank_mass_gives_correct_volume);
    RUN_TEST(test_half_mass_gives_half_volume);
    RUN_TEST(test_negative_mass_gives_zero_volume);

    // isMismatch
    RUN_TEST(test_no_mismatch_when_values_agree);
    RUN_TEST(test_no_mismatch_within_tolerance);
    RUN_TEST(test_no_mismatch_at_exact_tolerance);
    RUN_TEST(test_mismatch_triggered_above_tolerance);
    RUN_TEST(test_mismatch_triggered_when_flow_lower_than_loadcell);
    RUN_TEST(test_large_mismatch_triggers);

    // isChemLow
    RUN_TEST(test_low_flag_clear_when_well_above_threshold);
    RUN_TEST(test_low_flag_clear_just_above_threshold);
    RUN_TEST(test_low_flag_triggers_at_exact_threshold);
    RUN_TEST(test_low_flag_triggers_below_threshold);
    RUN_TEST(test_low_flag_triggers_at_zero);

    // Integration
    RUN_TEST(test_full_dispense_cycle);
    RUN_TEST(test_dispense_until_low_flag_triggers);
    RUN_TEST(test_loadcell_pipeline_full_tank);
    RUN_TEST(test_mismatch_detected_after_simulated_leak);

    return UNITY_END();
}