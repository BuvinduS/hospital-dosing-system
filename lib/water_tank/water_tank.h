// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Water tank logic — declarations
//  No hardware dependencies — safe to include in unit tests
// ============================================================
#pragma once

// Returns water column height in metres: h = H - d
// Clamped to [0, tankHeight_m] — never returns negative
float distanceToHeight(float dist_m, float tankHeight_m);

// Returns volume in litres: V = pi * r^2 * h * 1000
float heightToVolume(float height_m, float radius_m);

// Returns true when height is below the minimum safe level
bool isWaterLow(float height_m, float minLevel_m);