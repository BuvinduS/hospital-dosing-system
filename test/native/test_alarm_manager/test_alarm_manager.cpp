// ============================================================
//  Unit tests — Alarm manager logic
//  Phase 3b
//  Run with: pio test -e native
// ============================================================
#include <unity.h>
#include "alarm_manager.h"

// ============================================================
//  evaluateSeverity()
// ============================================================

void test_no_faults_gives_none() {
    TEST_ASSERT_EQUAL(AlarmSeverity::NONE,
        evaluateSeverity(false, false, false, false, false, false));
}

void test_dosing_active_gives_info() {
    TEST_ASSERT_EQUAL(AlarmSeverity::INFO,
        evaluateSeverity(false, false, false, false, false, true));
}

void test_water_low_gives_warning() {
    TEST_ASSERT_EQUAL(AlarmSeverity::WARNING,
        evaluateSeverity(false, true, false, false, false, false));
}

void test_chem_low_gives_warning() {
    TEST_ASSERT_EQUAL(AlarmSeverity::WARNING,
        evaluateSeverity(false, false, true, false, false, false));
}

void test_mismatch_gives_warning() {
    TEST_ASSERT_EQUAL(AlarmSeverity::WARNING,
        evaluateSeverity(false, false, false, true, false, false));
}

void test_water_fault_gives_critical() {
    TEST_ASSERT_EQUAL(AlarmSeverity::CRITICAL,
        evaluateSeverity(true, false, false, false, false, false));
}

void test_dosing_fault_gives_critical() {
    TEST_ASSERT_EQUAL(AlarmSeverity::CRITICAL,
        evaluateSeverity(false, false, false, false, true, false));
}

void test_critical_overrides_warning() {
    // Both water fault and chem low — critical wins
    TEST_ASSERT_EQUAL(AlarmSeverity::CRITICAL,
        evaluateSeverity(true, false, true, false, false, false));
}

void test_critical_overrides_info() {
    // Dosing active but water fault — critical wins
    TEST_ASSERT_EQUAL(AlarmSeverity::CRITICAL,
        evaluateSeverity(true, false, false, false, false, true));
}

void test_warning_overrides_info() {
    // Dosing active but chem low — warning wins
    TEST_ASSERT_EQUAL(AlarmSeverity::WARNING,
        evaluateSeverity(false, false, true, false, false, true));
}

void test_multiple_warnings_gives_warning() {
    TEST_ASSERT_EQUAL(AlarmSeverity::WARNING,
        evaluateSeverity(false, true, true, true, false, false));
}

// ============================================================
//  beepInterval()
// ============================================================

void test_beep_interval_none_is_zero() {
    TEST_ASSERT_EQUAL_UINT32(0UL, beepInterval(AlarmSeverity::NONE));
}

void test_beep_interval_info_is_zero() {
    TEST_ASSERT_EQUAL_UINT32(0UL, beepInterval(AlarmSeverity::INFO));
}

void test_beep_interval_warning_is_3000ms() {
    TEST_ASSERT_EQUAL_UINT32(3000UL, beepInterval(AlarmSeverity::WARNING));
}

void test_beep_interval_critical_is_300ms() {
    TEST_ASSERT_EQUAL_UINT32(300UL, beepInterval(AlarmSeverity::CRITICAL));
}

// ============================================================
//  shouldBeepNow()
// ============================================================

void test_no_beep_when_severity_none() {
    TEST_ASSERT_FALSE(shouldBeepNow(AlarmSeverity::NONE, 5000, 0, false));
}

void test_no_beep_when_severity_info() {
    TEST_ASSERT_FALSE(shouldBeepNow(AlarmSeverity::INFO, 5000, 0, false));
}

void test_warning_beep_triggers_after_interval() {
    // 3001ms elapsed since last beep — should trigger
    TEST_ASSERT_TRUE(shouldBeepNow(AlarmSeverity::WARNING, 3001, 0, false));
}

void test_warning_beep_does_not_trigger_before_interval() {
    // Only 1000ms elapsed — not yet
    TEST_ASSERT_FALSE(shouldBeepNow(AlarmSeverity::WARNING, 1000, 0, false));
}

void test_critical_beep_triggers_after_interval() {
    TEST_ASSERT_TRUE(shouldBeepNow(AlarmSeverity::CRITICAL, 301, 0, false));
}

void test_critical_beep_does_not_trigger_before_interval() {
    TEST_ASSERT_FALSE(shouldBeepNow(AlarmSeverity::CRITICAL, 100, 0, false));
}

void test_no_beep_when_buzzer_already_on() {
    // Buzzer already on — shouldBeepNow returns false
    // Duration management handled in main.cpp
    TEST_ASSERT_FALSE(shouldBeepNow(AlarmSeverity::CRITICAL, 5000, 0, true));
}

// ============================================================
//  severityToString()
// ============================================================

void test_severity_none_string() {
    TEST_ASSERT_EQUAL_STRING("OK", severityToString(AlarmSeverity::NONE));
}

void test_severity_info_string() {
    TEST_ASSERT_EQUAL_STRING("DOSING", severityToString(AlarmSeverity::INFO));
}

void test_severity_warning_string() {
    TEST_ASSERT_EQUAL_STRING("WARNING", severityToString(AlarmSeverity::WARNING));
}

void test_severity_critical_string() {
    TEST_ASSERT_EQUAL_STRING("CRITICAL", severityToString(AlarmSeverity::CRITICAL));
}

// ============================================================
//  Runner
// ============================================================

void setUp() {}
void tearDown() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // evaluateSeverity
    RUN_TEST(test_no_faults_gives_none);
    RUN_TEST(test_dosing_active_gives_info);
    RUN_TEST(test_water_low_gives_warning);
    RUN_TEST(test_chem_low_gives_warning);
    RUN_TEST(test_mismatch_gives_warning);
    RUN_TEST(test_water_fault_gives_critical);
    RUN_TEST(test_dosing_fault_gives_critical);
    RUN_TEST(test_critical_overrides_warning);
    RUN_TEST(test_critical_overrides_info);
    RUN_TEST(test_warning_overrides_info);
    RUN_TEST(test_multiple_warnings_gives_warning);

    // beepInterval
    RUN_TEST(test_beep_interval_none_is_zero);
    RUN_TEST(test_beep_interval_info_is_zero);
    RUN_TEST(test_beep_interval_warning_is_3000ms);
    RUN_TEST(test_beep_interval_critical_is_300ms);

    // shouldBeepNow
    RUN_TEST(test_no_beep_when_severity_none);
    RUN_TEST(test_no_beep_when_severity_info);
    RUN_TEST(test_warning_beep_triggers_after_interval);
    RUN_TEST(test_warning_beep_does_not_trigger_before_interval);
    RUN_TEST(test_critical_beep_triggers_after_interval);
    RUN_TEST(test_critical_beep_does_not_trigger_before_interval);
    RUN_TEST(test_no_beep_when_buzzer_already_on);

    // severityToString
    RUN_TEST(test_severity_none_string);
    RUN_TEST(test_severity_info_string);
    RUN_TEST(test_severity_warning_string);
    RUN_TEST(test_severity_critical_string);

    return UNITY_END();
}