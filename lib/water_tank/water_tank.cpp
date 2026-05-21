// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Water tank logic — implementation
// ============================================================
#include "water_tank.h"

static constexpr float PI_F = 3.14159265358979323846f;
// Note: named PI_F to avoid collision with Arduino's PI macro
// when this file is compiled as part of the ESP32 build

float distanceToHeight(float dist_m, float tankHeight_m) {
    float h = tankHeight_m - dist_m;
    return h < 0.0f ? 0.0f : h;  // clamp: sensor noise can push dist > H
}

float heightToVolume(float height_m, float radius_m) {
    return PI_F * radius_m * radius_m * height_m * 1000.0f;
}

bool isWaterLow(float height_m, float minLevel_m) {
    return height_m <= minLevel_m;  // <= so threshold itself triggers the flag
}