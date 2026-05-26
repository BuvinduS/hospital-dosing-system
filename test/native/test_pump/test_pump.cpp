// ============================================================
//  Unit tests — Chemical pump logic
//  Phase 2b
//
//  Tests: chemTargetVolume(), isPumpFailure(),
//         isRatioCorrect(), isChemTargetReached()
//  Run with: pio test -e native
// ============================================================
#include <unity.h>
#include "pump.h"

// 1:50 dilution — 1 part chem per 50 parts water
// ratio = 1/51 ≈ 0.01961
static constexpr float RATIO = 1.0f / 51.0f;
static constexpr float RATIO_TOL = 0.05f;
static constexpr float WATER_TARGET = 0.588f;  // ~588mL water for 600mL total
static constexpr unsigned long GRACE = 3000UL;

// ============================================================
//  chemTargetVolume()
// ============================================================

void test_chem_target_reasonable_for_600mL_cycle() {
    // 600mL total at 1:50 → ~11.76mL chemical
    float chem = chemTargetVolume(WATER_TARGET, RATIO);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 11.76f / 1000.0f, chem);
}

void test_chem_target_zero_for_zero_water() {
    float chem = chemTargetVolume(0.0f, RATIO);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, chem);
}

void test_chem_target_zero_for_invalid_ratio_zero() {
    float chem = chemTargetVolume(WATER_TARGET, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, chem);
}

void test_chem_target_zero_for_invalid_ratio_one() {
    float chem = chemTargetVolume(WATER_TARGET, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, chem);
}

void test_chem_target_scales_with_water_volume() {
    float chem_half = chemTargetVolume(WATER_TARGET / 2.0f, RATIO);
    float chem_full = chemTargetVolume(WATER_TARGET, RATIO);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, chem_full / 2.0f, chem_half);
}

// ============================================================
//  isPumpFailure()
// ============================================================

void test_no_failure_within_grace_no_flow() {
    TEST_ASSERT_FALSE(isPumpFailure(0.0f, GRACE - 1, GRACE));
}

void test_no_failure_within_grace_with_flow() {
    TEST_ASSERT_FALSE(isPumpFailure(0.005f, GRACE - 1, GRACE));
}

void test_no_failure_after_grace_with_flow() {
    TEST_ASSERT_FALSE(isPumpFailure(0.005f, GRACE + 1, GRACE));
}

void test_pump_failure_after_grace_no_flow() {
    TEST_ASSERT_TRUE(isPumpFailure(0.0f, GRACE + 1, GRACE));
}

void test_pump_failure_at_exact_grace_no_flow() {
    TEST_ASSERT_TRUE(isPumpFailure(0.0f, GRACE, GRACE));
}

// ============================================================
//  isRatioCorrect()
// ============================================================

void test_ratio_correct_at_exact_target() {
    // 588mL water + 11.76mL chem = 1:50 exactly
    TEST_ASSERT_TRUE(isRatioCorrect(0.588f, 0.01176f, RATIO, RATIO_TOL));
}

void test_ratio_correct_within_tolerance() {
    // Slightly off but within 5%
    TEST_ASSERT_TRUE(isRatioCorrect(0.588f, 0.012f, RATIO, RATIO_TOL));
}

void test_ratio_incorrect_too_much_chem() {
    // Way too much chemical — dangerous concentration
    TEST_ASSERT_FALSE(isRatioCorrect(0.588f, 0.05f, RATIO, RATIO_TOL));
}

void test_ratio_incorrect_too_little_chem() {
    // 0.1mL chemical in 588mL water — far too dilute
    TEST_ASSERT_FALSE(isRatioCorrect(0.588f, 0.0001f, RATIO, RATIO_TOL));
}

void test_ratio_returns_false_for_zero_total() {
    TEST_ASSERT_FALSE(isRatioCorrect(0.0f, 0.0f, RATIO, RATIO_TOL));
}

// ============================================================
//  isChemTargetReached()
// ============================================================

void test_chem_target_not_reached_at_zero() {
    TEST_ASSERT_FALSE(isChemTargetReached(0.0f, 0.01176f));
}

void test_chem_target_not_reached_below() {
    TEST_ASSERT_FALSE(isChemTargetReached(0.005f, 0.01176f));
}

void test_chem_target_reached_at_exact() {
    TEST_ASSERT_TRUE(isChemTargetReached(0.01176f, 0.01176f));
}

void test_chem_target_reached_above() {
    TEST_ASSERT_TRUE(isChemTargetReached(0.015f, 0.01176f));
}

// ============================================================
//  Integration — full simultaneous dosing cycle
// ============================================================

void test_simultaneous_dosing_correct_ratio() {
    // Simulate: water and chem dispensed simultaneously
    // Water hits target first, chem tops up, ratio checked
    float water_dispensed = WATER_TARGET;
    float chem_target = chemTargetVolume(WATER_TARGET, RATIO);
    float chem_dispensed = chem_target;  // pump delivered correctly

    TEST_ASSERT_TRUE(isChemTargetReached(chem_dispensed, chem_target));
    TEST_ASSERT_TRUE(isRatioCorrect(water_dispensed, chem_dispensed,
        RATIO, RATIO_TOL));
}

void test_simultaneous_dosing_pump_failure_scenario() {
    // Pump never delivered — ratio check fails
    float water_dispensed = WATER_TARGET;
    float chem_dispensed = 0.0f;
    float chem_target = chemTargetVolume(WATER_TARGET, RATIO);

    TEST_ASSERT_FALSE(isChemTargetReached(chem_dispensed, chem_target));
    TEST_ASSERT_TRUE(isPumpFailure(chem_dispensed, GRACE + 1, GRACE));
    TEST_ASSERT_FALSE(isRatioCorrect(water_dispensed, chem_dispensed,
        RATIO, RATIO_TOL));
}

void test_topping_up_scenario() {
    // Water done, chem still needs more — topping up
    float water_dispensed = WATER_TARGET;
    float chem_target = chemTargetVolume(WATER_TARGET, RATIO);
    float chem_so_far = chem_target * 0.5f;  // halfway through

    TEST_ASSERT_FALSE(isChemTargetReached(chem_so_far, chem_target));
    // Water solenoid should be closed, pump still running
}

// ============================================================
//  Runner
// ============================================================

void setUp() {}
void tearDown() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_chem_target_reasonable_for_600mL_cycle);
    RUN_TEST(test_chem_target_zero_for_zero_water);
    RUN_TEST(test_chem_target_zero_for_invalid_ratio_zero);
    RUN_TEST(test_chem_target_zero_for_invalid_ratio_one);
    RUN_TEST(test_chem_target_scales_with_water_volume);

    RUN_TEST(test_no_failure_within_grace_no_flow);
    RUN_TEST(test_no_failure_within_grace_with_flow);
    RUN_TEST(test_no_failure_after_grace_with_flow);
    RUN_TEST(test_pump_failure_after_grace_no_flow);
    RUN_TEST(test_pump_failure_at_exact_grace_no_flow);

    RUN_TEST(test_ratio_correct_at_exact_target);
    RUN_TEST(test_ratio_correct_within_tolerance);
    RUN_TEST(test_ratio_incorrect_too_much_chem);
    RUN_TEST(test_ratio_incorrect_too_little_chem);
    RUN_TEST(test_ratio_returns_false_for_zero_total);

    RUN_TEST(test_chem_target_not_reached_at_zero);
    RUN_TEST(test_chem_target_not_reached_below);
    RUN_TEST(test_chem_target_reached_at_exact);
    RUN_TEST(test_chem_target_reached_above);

    RUN_TEST(test_simultaneous_dosing_correct_ratio);
    RUN_TEST(test_simultaneous_dosing_pump_failure_scenario);
    RUN_TEST(test_topping_up_scenario);

    return UNITY_END();
}