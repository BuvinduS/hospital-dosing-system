// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Phase 1e: Water tank + Chemical tank sensing
//
//  Hardware: ESP32-S3-DevKitC-1
//  Simulator: Wokwi (VS Code extension)
//
//  Sensors:
//    Water tank   — HC-SR04 ultrasonic (GPIO5 TRIG, GPIO17 ECHO)
//    Chem tank    — Push button flow meter sim (GPIO4)
//                   Potentiometer load cell sim (GPIO6 ADC)
//  Indicators:
//    Fault LED    — GPIO15 (red)
//    OK LED       — GPIO16 (green)
// ============================================================
#include <Arduino.h>
#include "water_tank.h"
#include "chem_tank.h"

// ── Pin definitions ──────────────────────────────────────────
#define TRIG_PIN 5
#define ECHO_PIN 17
#define FLOWPULSE_PIN   4    // push button — simulates flow meter pulses
#define LOADCELL_PIN    6    // potentiometer — simulates load cell ADC
#define LED_FAULT       15
#define LED_OK          16

// ── Physical constants ───────────────────────────────────────
// TODO: move to include/config.h in Phase 1f
// ── Water tank constants ─────────────────────────────────────
static constexpr float SPEED_OF_SOUND_M_PER_US = 0.000343f; // 343 m/s → m/µs
static constexpr float TANK_HEIGHT_M = 10.19f;  // metres — derived from V=2000L, d=50cm
static constexpr float TANK_RADIUS_M = 0.25f;   // metres — diameter 50cm as per spec
static constexpr float TANK_MIN_LEVEL_M = 0.10f;

// ── Chemical tank constants ───────────────────────────────────
static constexpr float CHEM_INITIAL_VOL_L = 10.0f;
static constexpr float CHEM_LOW_THRESHOLD_L = 0.50f;
static constexpr float CHEM_DENSITY_KG_L = 1.05f;
static constexpr float MISMATCH_TOLERANCE_L = 0.20f;
static constexpr float ML_PER_PULSE = 5.0f;
static constexpr float LOADCELL_MAX_KG = 12.0f;
static constexpr int   ADC_MAX = 4095;

// ── Runtime state ─────────────────────────────────────────────
static float  chemRemaining_L = CHEM_INITIAL_VOL_L;
static long   totalPulses = 0;

// ── Report timing ─────────────────────────────────────────────
static unsigned long lastReport = 0;
static constexpr unsigned long REPORT_MS = 1000;

// ============================================================

// ── Interrupt-driven button handling ─────────────────────────
volatile long pendingPulses = 0;

// IRAM_ATTR is a compiler macro used primarily in ESP32 (and ESP8266) development frameworks to force specific functions or variables into Internal RAM (IRAM). 
// This is crucial for interrupt service routines (ISRs) because they need to execute quickly and reliably, without the latency that can come from fetching code from flash memory. 
//By placing an ISR in IRAM, you ensure that it can be executed with minimal delay, which is essential for handling time-sensitive tasks in embedded systems.
void IRAM_ATTR onButtonPress() {
  pendingPulses++;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(FLOWPULSE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOWPULSE_PIN), onButtonPress, FALLING);
  pinMode(LED_FAULT, OUTPUT);
  pinMode(LED_OK, OUTPUT);

  digitalWrite(TRIG_PIN, LOW);

  Serial.println("============================================================");
  Serial.println(" Hospital Dosing System — Phase 1e");
  Serial.println(" Water tank + Chemical tank sensing");
  Serial.println("=============================================");
  Serial.println(" [Tip] Move HC-SR04 slider to change water level");
  Serial.println(" [Tip] Press button to inject flow pulses");
  Serial.println(" [Tip] Turn potentiometer to adjust load cell");
  Serial.println("============================================================");
}

// ============================================================
//  Hardware read — HC-SR04
// ============================================================
float readDistanceMetres() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration_us = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (duration_us == 0) {
    Serial.println("[FAULT] No echo — sensor timeout or out of range");
    delay(1000);
    return -1.0f;  // sentinel value for timeout
  }

  return duration_us * SPEED_OF_SOUND_M_PER_US / 2.0f;
}

// ============================================================
//  Hardware read — potentiometer as load cell
// ============================================================
float readLoadCellVolume() {
  int   raw = analogRead(LOADCELL_PIN);
  float mass = adcToMass(raw, ADC_MAX, LOADCELL_MAX_KG);
  return massToVolume(mass, CHEM_DENSITY_KG_L);
}

// ── Process any pending pulses from interrupt ─────────────────
void processPendingPulses() {
  if (pendingPulses > 0) {
    // Atomically grab and clear pending count
    noInterrupts();
    long pulses = pendingPulses;
    pendingPulses = 0;
    interrupts();

    totalPulses += pulses;
    float dispensed = pulseToVolume(pulses, ML_PER_PULSE);
    chemRemaining_L = updateRemaining(chemRemaining_L, dispensed);

    Serial.print("  [PULSE] #");
    Serial.print(totalPulses);
    Serial.print("  dispensed +");
    Serial.print(ML_PER_PULSE * pulses, 1);
    Serial.print(" mL  remaining: ");
    Serial.print(chemRemaining_L, 3);
    Serial.println(" L");
  }
}

// ============================================================
//  Update fault LEDs
// ============================================================
void updateLEDs(bool anyFault) {
  digitalWrite(LED_FAULT, anyFault ? HIGH : LOW);
  digitalWrite(LED_OK, anyFault ? LOW : HIGH);
}

void loop() {
  processPendingPulses();

  if (millis() - lastReport >= REPORT_MS) {
    lastReport = millis();

    // ── Water tank ────────────────────────────────────────
    float dist_m = readDistanceMetres();

    bool  waterFault = false;
    float height_m = 0.0f;
    float volume_L = 0.0f;
    bool  waterLow = false;

    if (dist_m < 0.0f) {
      waterFault = true;
      Serial.println("[WATER] FAULT — ultrasonic timeout");
    }
    else {
      height_m = distanceToHeight(dist_m, TANK_HEIGHT_M);
      volume_L = heightToVolume(height_m, TANK_RADIUS_M);
      waterLow = isWaterLow(height_m, TANK_MIN_LEVEL_M);

      Serial.print("[WATER] dist=");
      Serial.print(dist_m * 100.0f, 1);
      Serial.print(" cm  h=");
      Serial.print(height_m * 100.0f, 1);
      Serial.print(" cm  V=");
      Serial.print(volume_L, 1);
      Serial.print(" L");
      if (waterLow) Serial.print("  *** LOW ***");
      Serial.println();
    }

    // ── Chemical tank — flow meter estimate ───────────────
    Serial.print("[CHEM ] flow_est=");
    Serial.print(chemRemaining_L, 3);
    Serial.print(" L  pulses=");
    Serial.print(totalPulses);
    bool chemLow = isChemLow(chemRemaining_L, CHEM_LOW_THRESHOLD_L);
    if (chemLow) Serial.print("  *** LOW ***");
    Serial.println();

    // ── Chemical tank — load cell estimate ────────────────
    float lcVol_L = readLoadCellVolume();

    // ── Chemical tank — mismatch detection ─────────────── 
    bool  mismatch = isMismatch(chemRemaining_L, lcVol_L, MISMATCH_TOLERANCE_L);

    Serial.print("[CHEM ] loadcell_est=");
    Serial.print(lcVol_L, 3);
    Serial.print(" L");

    if (mismatch) Serial.print("  *** MISMATCH - check for potential leak/spill ***");
    Serial.println();

    // ── Status summary ───────────────────────────────────────
    bool anyFault = waterFault || waterLow || chemLow || mismatch;
    Serial.print("[STATUS] ");
    Serial.println(anyFault ? "FAULT" : "OK");
    Serial.println("============================================================");

    // Update LEDs at end of cycle
    updateLEDs(anyFault);
  }
}