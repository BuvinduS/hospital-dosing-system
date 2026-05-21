// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Phase 1b: Water tank sensing — main entry point
//
//  Hardware: ESP32-S3-DevKitC-1 + HC-SR04
//  Simulator: Wokwi (VS Code extension)
// ============================================================
#include <Arduino.h>
#include "water_tank.h"

// ── Pin definitions ──────────────────────────────────────────
#define TRIG_PIN 5
#define ECHO_PIN 17

// ── Physical constants ───────────────────────────────────────
// TODO: move to include/config.h in Phase 1b
static constexpr float SPEED_OF_SOUND_M_PER_US = 0.000343f; // 343 m/s → m/µs
static constexpr float TANK_HEIGHT_M = 10.19f;  // metres — derived from V=2000L, d=50cm
static constexpr float TANK_RADIUS_M = 0.25f;   // metres — diameter 50cm as per spec
static constexpr float TANK_MIN_LEVEL_M = 0.10f;

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  Serial.println("Water tank sensing — ready");
}

void loop() {
  // ── Trigger ultrasonic pulse ───────────────────────────────
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // ── Measure echo ──────────────────────────────────────────
  // pulseIn returns duration in MICROSECONDS, 0 on timeout
  long duration_us = pulseIn(ECHO_PIN, HIGH, 30000UL);

  if (duration_us == 0) {
    Serial.println("[FAULT] No echo — sensor timeout or out of range");
    delay(1000);
    return;
  }

  // ── Distance conversion ────────────────────────────────────
  // dist = (duration_µs × speed_m/µs) / 2   (round trip → halve)
  float dist_m = duration_us * SPEED_OF_SOUND_M_PER_US / 2.0f;

  // ── Logic functions from water_tank.cpp ───────────────────
  float height_m = distanceToHeight(dist_m, TANK_HEIGHT_M);
  float volume_L = heightToVolume(height_m, TANK_RADIUS_M);
  bool  low = isWaterLow(height_m, TANK_MIN_LEVEL_M);

  // ── Serial report ─────────────────────────────────────────
  Serial.print("[WATER] dist=");   Serial.print(dist_m * 100.0f, 1); Serial.print(" cm");
  Serial.print("  h=");            Serial.print(height_m * 100.0f, 1); Serial.print(" cm");
  Serial.print("  V=");            Serial.print(volume_L, 1); Serial.print(" L");
  if (low) Serial.print("  *** LOW LEVEL ***");
  Serial.println();

  delay(1000);
}