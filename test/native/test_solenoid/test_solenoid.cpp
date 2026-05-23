// ============================================================
//  Unit tests — Solenoid valve and dosing logic
//  Phase 2a
//
//  Tests: isTargetReached(), isDosingTimeout(), isBlockedValve(),
//         isStuckOpen(), isSufficientWater()
//  Run with: pio test -e native
// ============================================================
#include <unity.h>
#include "solenoid.h"

// ── Constants matching config.h ───────────────────────────────
static constexpr float    TARGET_L = 0.60f;
static constexpr float    STUCK_TOL_L = 0.01f;
static constexpr float    MIN_LEVEL_M = 0.10f;
static constexpr unsigned long TIMEOUT_MS = 30000UL;
static constexpr unsigned long GRACE_MS = 5000UL;

// ============================================================
//  isTargetReached()
// ============================================================

void test_target_not_reached_when_dispensed_zero() {
    TEST_ASSERT_FALSE(isTargetReached(0.0f, TARGET_L));
}

void test_target_not_reached_when_dispensed_below() {
    TEST_ASSERT_FALSE(isTargetReached(0.30f, TARGET_L));
}

void test_target_not_reached_just_below() {
    TEST_ASSERT_FALSE(isTargetReached(0.599f, TARGET_L));
}

void test_target_reached_at_exact_value() {
    TEST_ASSERT_TRUE(isTargetReached(0.60f, TARGET_L));
}

void test_target_reached_above_value() {
    TEST_ASSERT_TRUE(isTargetReached(0.65f, TARGET_L));
}

// ============================================================
//  isDosingTimeout()
// ============================================================

void test_no_timeout_at_zero_elapsed() {
    TEST_ASSERT_FALSE(isDosingTimeout(0UL, TIMEOUT_MS));
}

void test_no_timeout_before_limit() {
    TEST_ASSERT_FALSE(isDosingTimeout(15000UL, TIMEOUT_MS));
}

void test_no_timeout_just_before_limit() {
    TEST_ASSERT_FALSE(isDosingTimeout(TIMEOUT_MS - 1, TIMEOUT_MS));
}

void test_timeout_at_exact_limit() {
    TEST_ASSERT_TRUE(isDosingTimeout(TIMEOUT_MS, TIMEOUT_MS));
}

void test_timeout_after_limit() {
    TEST_ASSERT_TRUE(isDosingTimeout(TIMEOUT_MS + 1000, TIMEOUT_MS));
}

// ============================================================
//  isBlockedValve()
// ============================================================

void test_not_blocked_within_grace_period_no_flow() {
    // No flow but still within grace period — not blocked yet
    TEST_ASSERT_FALSE(isBlockedValve(0.0f, GRACE_MS - 1, GRACE_MS));
}

void test_not_blocked_within_grace_period_with_flow() {
    TEST_ASSERT_FALSE(isBlockedValve(0.05f, GRACE_MS - 1, GRACE_MS));
}

void test_not_blocked_after_grace_with_flow() {
    // Grace period elapsed but flow is present — not blocked
    TEST_ASSERT_FALSE(isBlockedValve(0.05f, GRACE_MS + 1, GRACE_MS));
}

void test_blocked_after_grace_with_no_flow() {
    // Grace period elapsed and still no flow — blocked valve
    TEST_ASSERT_TRUE(isBlockedValve(0.0f, GRACE_MS + 1, GRACE_MS));
}

void test_blocked_at_exact_grace_boundary_no_flow() {
    TEST_ASSERT_TRUE(isBlockedValve(0.0f, GRACE_MS, GRACE_MS));
}

// ============================================================
//  isStuckOpen()
// ============================================================

void test_not_stuck_when_no_flow_after_close() {
    TEST_ASSERT_FALSE(isStuckOpen(0.0f, STUCK_TOL_L));
}

void test_not_stuck_within_tolerance() {
    TEST_ASSERT_FALSE(isStuckOpen(0.009f, STUCK_TOL_L));
}

void test_not_stuck_at_exact_tolerance() {
    TEST_ASSERT_FALSE(isStuckOpen(STUCK_TOL_L, STUCK_TOL_L));
}

void test_stuck_above_tolerance() {
    TEST_ASSERT_TRUE(isStuckOpen(0.011f, STUCK_TOL_L));
}

void test_stuck_large_flow_after_close() {
    // Obvious stuck valve — significant flow after close
    TEST_ASSERT_TRUE(isStuckOpen(0.10f, STUCK_TOL_L));
}

// ============================================================
//  isSufficientWater()
// ============================================================

void test_sufficient_water_well_above_minimum() {
    TEST_ASSERT_TRUE(isSufficientWater(5.0f, MIN_LEVEL_M));
}

void test_sufficient_water_just_above_minimum() {
    TEST_ASSERT_TRUE(isSufficientWater(MIN_LEVEL_M + 0.001f, MIN_LEVEL_M));
}

void test_insufficient_water_at_minimum() {
    // At exactly minimum — not sufficient, need more than minimum
    TEST_ASSERT_FALSE(isSufficientWater(MIN_LEVEL_M, MIN_LEVEL_M));
}

void test_insufficient_water_below_minimum() {
    TEST_ASSERT_FALSE(isSufficientWater(0.05f, MIN_LEVEL_M));
}

void test_insufficient_water_at_zero() {
    TEST_ASSERT_FALSE(isSufficientWater(0.0f, MIN_LEVEL_M));
}

// ============================================================
//  faultToString() and stateToString()
// ============================================================

void test_fault_none_string() {
    TEST_ASSERT_EQUAL_STRING("NONE", faultToString(DosingFault::NONE));
}

void test_fault_dry_tank_string() {
    TEST_ASSERT_EQUAL_STRING("DRY TANK", faultToString(DosingFault::DRY_TANK));
}

void test_fault_blocked_valve_string() {
    TEST_ASSERT_EQUAL_STRING("BLOCKED VALVE",
        faultToString(DosingFault::BLOCKED_VALVE));
}

void test_fault_stuck_valve_string() {
    TEST_ASSERT_EQUAL_STRING("STUCK VALVE",
        faultToString(DosingFault::STUCK_VALVE));
}

void test_fault_timeout_string() {
    TEST_ASSERT_EQUAL_STRING("TIMEOUT", faultToString(DosingFault::TIMEOUT));
}

void test_state_idle_string() {
    TEST_ASSERT_EQUAL_STRING("IDLE", stateToString(DosingState::IDLE));
}

void test_state_dosing_string() {
    TEST_ASSERT_EQUAL_STRING("DOSING", stateToString(DosingState::DOSING));
}

void test_state_closing_string() {
    TEST_ASSERT_EQUAL_STRING("CLOSING", stateToString(DosingState::CLOSING));
}

void test_state_complete_string() {
    TEST_ASSERT_EQUAL_STRING("COMPLETE", stateToString(DosingState::COMPLETE));
}

void test_state_fault_string() {
    TEST_ASSERT_EQUAL_STRING("FAULT", stateToString(DosingState::FAULT));
}

// ============================================================
//  Integration — full dosing cycle simulation
// ============================================================

void test_dosing_cycle_completes_normally() {
    // Simulate: valve opens, flow accumulates, target reached
    float dispensed = 0.0f;
    float target = TARGET_L;

    // Simulate 120 pulses at 5mL each = 600mL = 0.6L
    dispensed = 0.60f;
    unsigned long elapsed = 10000UL;  // 10 seconds — well within timeout

    TEST_ASSERT_FALSE(isBlockedValve(dispensed, elapsed, GRACE_MS));
    TEST_ASSERT_FALSE(isDosingTimeout(elapsed, TIMEOUT_MS));
    TEST_ASSERT_TRUE(isTargetReached(dispensed, target));
}

void test_dosing_cycle_blocked_valve_scenario() {
    // Simulate: valve opens, no flow for 6 seconds — blocked
    float dispensed = 0.0f;
    unsigned long elapsed = 6000UL;  // beyond 5s grace

    TEST_ASSERT_TRUE(isBlockedValve(dispensed, elapsed, GRACE_MS));
    TEST_ASSERT_FALSE(isTargetReached(dispensed, TARGET_L));
}

void test_dosing_cycle_timeout_scenario() {
    // Simulate: flow too slow, timeout expires before target reached
    float dispensed = 0.30f;      // only half dispensed
    unsigned long elapsed = 31000UL; // beyond 30s timeout

    TEST_ASSERT_FALSE(isTargetReached(dispensed, TARGET_L));
    TEST_ASSERT_TRUE(isDosingTimeout(elapsed, TIMEOUT_MS));
}

void test_dosing_cycle_stuck_valve_after_close() {
    // Simulate: valve closed but 50mL still flowed through
    float flow_after_close = 0.05f;
    TEST_ASSERT_TRUE(isStuckOpen(flow_after_close, STUCK_TOL_L));
}

void test_dosing_cycle_dry_tank_pre_check() {
    // Simulate: water level at 5cm — below 10cm minimum
    float current_level = 0.05f;
    TEST_ASSERT_FALSE(isSufficientWater(current_level, MIN_LEVEL_M));
}

// ============================================================
//  Runner
// ============================================================

void setUp() {}
void tearDown() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // isTargetReached
    RUN_TEST(test_target_not_reached_when_dispensed_zero);
    RUN_TEST(test_target_not_reached_when_dispensed_below);
    RUN_TEST(test_target_not_reached_just_below);
    RUN_TEST(test_target_reached_at_exact_value);
    RUN_TEST(test_target_reached_above_value);

    // isDosingTimeout
    RUN_TEST(test_no_timeout_at_zero_elapsed);
    RUN_TEST(test_no_timeout_before_limit);
    RUN_TEST(test_no_timeout_just_before_limit);
    RUN_TEST(test_timeout_at_exact_limit);
    RUN_TEST(test_timeout_after_limit);

    // isBlockedValve
    RUN_TEST(test_not_blocked_within_grace_period_no_flow);
    RUN_TEST(test_not_blocked_within_grace_period_with_flow);
    RUN_TEST(test_not_blocked_after_grace_with_flow);
    RUN_TEST(test_blocked_after_grace_with_no_flow);
    RUN_TEST(test_blocked_at_exact_grace_boundary_no_flow);

    // isStuckOpen
    RUN_TEST(test_not_stuck_when_no_flow_after_close);
    RUN_TEST(test_not_stuck_within_tolerance);
    RUN_TEST(test_not_stuck_at_exact_tolerance);
    RUN_TEST(test_stuck_above_tolerance);
    RUN_TEST(test_stuck_large_flow_after_close);

    // isSufficientWater
    RUN_TEST(test_sufficient_water_well_above_minimum);
    RUN_TEST(test_sufficient_water_just_above_minimum);
    RUN_TEST(test_insufficient_water_at_minimum);
    RUN_TEST(test_insufficient_water_below_minimum);
    RUN_TEST(test_insufficient_water_at_zero);

    // string helpers
    RUN_TEST(test_fault_none_string);
    RUN_TEST(test_fault_dry_tank_string);
    RUN_TEST(test_fault_blocked_valve_string);
    RUN_TEST(test_fault_stuck_valve_string);
    RUN_TEST(test_fault_timeout_string);
    RUN_TEST(test_state_idle_string);
    RUN_TEST(test_state_dosing_string);
    RUN_TEST(test_state_closing_string);
    RUN_TEST(test_state_complete_string);
    RUN_TEST(test_state_fault_string);

    // integration
    RUN_TEST(test_dosing_cycle_completes_normally);
    RUN_TEST(test_dosing_cycle_blocked_valve_scenario);
    RUN_TEST(test_dosing_cycle_timeout_scenario);
    RUN_TEST(test_dosing_cycle_stuck_valve_after_close);
    RUN_TEST(test_dosing_cycle_dry_tank_pre_check);

    return UNITY_END();
}