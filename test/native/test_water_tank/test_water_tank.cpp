// ============================================================
//  Unit tests — Water tank logic
//  Phase 1b
//
//  Tests: distanceToHeight(), heightToVolume(), isWaterLow()
//  Run with: pio test -e native
// ============================================================
#include <unity.h>
#include "water_tank.h"

// ── Constants matching current spec ──────────────────────────
static constexpr float H = 10.19f;  // tank height (m) — derived from V=2000L, d=50cm
static constexpr float R = 0.25f;   // tank radius (m) — diameter 50cm per spec
static constexpr float HMIN = 0.10f;   // low level threshold (m)

// ============================================================
//  distanceToHeight()
// ============================================================

void test_empty_tank_distance_gives_full_height() {
    // Sensor reads 0 cm — water is at the very top
    float h = distanceToHeight(0.0f, H);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, H, h);
}

void test_full_tank_distance_gives_zero_height() {
    // Sensor reads full tank height — water is at the bottom
    float h = distanceToHeight(H, H);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, h);
}

void test_midpoint_distance_gives_half_height() {
    float h = distanceToHeight(H / 2.0f, H);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, H / 2.0f, h);
}

void test_height_clamped_when_distance_exceeds_tank_height() {
    // Sensor noise can push distance slightly beyond H
    // Result must never be negative
    float h = distanceToHeight(H + 0.5f, H);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, h);
}

void test_height_clamped_at_zero_not_negative() {
    float h = distanceToHeight(H + 2.0f, H);
    TEST_ASSERT_TRUE(h >= 0.0f);
}

// ============================================================
//  heightToVolume()
// ============================================================

void test_zero_height_gives_zero_volume() {
    float v = heightToVolume(0.0f, R);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v);
}

void test_full_height_gives_approx_2000L() {
    // V = pi * 0.25^2 * 10.19 * 1000 = ~2001 L
    float v = heightToVolume(H, R);
    TEST_ASSERT_FLOAT_WITHIN(10.0f, 2001.0f, v);
}

void test_half_height_gives_half_volume() {
    // Volume scales linearly with height for a cylinder
    float full = heightToVolume(H, R);
    float half = heightToVolume(H / 2.0f, R);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, full / 2.0f, half);
}

void test_quarter_height_gives_quarter_volume() {
    float full = heightToVolume(H, R);
    float quarter = heightToVolume(H / 4.0f, R);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, full / 4.0f, quarter);
}

void test_volume_never_negative() {
    // Even with clamped zero height, volume must be >= 0
    float v = heightToVolume(0.0f, R);
    TEST_ASSERT_TRUE(v >= 0.0f);
}

// ============================================================
//  isWaterLow()
// ============================================================

void test_low_flag_clear_when_well_above_threshold() {
    TEST_ASSERT_FALSE(isWaterLow(5.0f, HMIN));
}

void test_low_flag_clear_just_above_threshold() {
    TEST_ASSERT_FALSE(isWaterLow(HMIN + 0.001f, HMIN));
}

void test_low_flag_triggers_at_exact_threshold() {
    // <= boundary: at exactly HMIN the flag must be set
    TEST_ASSERT_TRUE(isWaterLow(HMIN, HMIN));
}

void test_low_flag_triggers_below_threshold() {
    TEST_ASSERT_TRUE(isWaterLow(0.05f, HMIN));
}

void test_low_flag_triggers_at_zero_height() {
    TEST_ASSERT_TRUE(isWaterLow(0.0f, HMIN));
}

// ============================================================
//  Integration — distanceToHeight into heightToVolume
// ============================================================

void test_pipeline_full_tank_end_to_end() {
    // Distance = 0 (sensor right at water surface) → should give ~2000 L
    float h = distanceToHeight(0.0f, H);
    float v = heightToVolume(h, R);
    TEST_ASSERT_FLOAT_WITHIN(10.0f, 2001.0f, v);
}

void test_pipeline_empty_tank_end_to_end() {
    // Distance = H (water at floor) → should give 0 L
    float h = distanceToHeight(H, H);
    float v = heightToVolume(h, R);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v);
}

void test_pipeline_over_range_clamped_end_to_end() {
    // Distance beyond tank height → clamped → 0 L, not negative
    float h = distanceToHeight(H + 1.0f, H);
    float v = heightToVolume(h, R);
    TEST_ASSERT_TRUE(v >= 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v);
}

// ============================================================
//  Runner
// ============================================================

void setUp() {}  // runs before each test — nothing needed yet
void tearDown() {}  // runs after each test — nothing needed yet

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // distanceToHeight
    RUN_TEST(test_empty_tank_distance_gives_full_height);
    RUN_TEST(test_full_tank_distance_gives_zero_height);
    RUN_TEST(test_midpoint_distance_gives_half_height);
    RUN_TEST(test_height_clamped_when_distance_exceeds_tank_height);
    RUN_TEST(test_height_clamped_at_zero_not_negative);

    // heightToVolume
    RUN_TEST(test_zero_height_gives_zero_volume);
    RUN_TEST(test_full_height_gives_approx_2000L);
    RUN_TEST(test_half_height_gives_half_volume);
    RUN_TEST(test_quarter_height_gives_quarter_volume);
    RUN_TEST(test_volume_never_negative);

    // isWaterLow
    RUN_TEST(test_low_flag_clear_when_well_above_threshold);
    RUN_TEST(test_low_flag_clear_just_above_threshold);
    RUN_TEST(test_low_flag_triggers_at_exact_threshold);
    RUN_TEST(test_low_flag_triggers_below_threshold);
    RUN_TEST(test_low_flag_triggers_at_zero_height);

    // End-to-end pipeline
    RUN_TEST(test_pipeline_full_tank_end_to_end);
    RUN_TEST(test_pipeline_empty_tank_end_to_end);
    RUN_TEST(test_pipeline_over_range_clamped_end_to_end);

    return UNITY_END();
}