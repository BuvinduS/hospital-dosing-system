// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Phase 1f: Config consolidation and unified serial dashboard
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
//  Rules:
//    - All config constants in include/config.h
// ============================================================
#include <Arduino.h>
#include "water_tank.h"
#include "chem_tank.h"
#include "config.h"

// ── Runtime state ─────────────────────────────────────────────
static float  chemRemaining_L = CHEM_INITIAL_VOL_L;
static long   totalPulses = 0;

// ── Report timing ─────────────────────────────────────────────
static unsigned long lastReport = 0;

// ============================================================
// For debouncing
static volatile unsigned long  lastDebounce = 0;

// ── Interrupt-driven button handling ─────────────────────────
volatile long pendingPulses = 0;

// IRAM_ATTR is a compiler macro used primarily in ESP32 (and ESP8266) development frameworks to force specific functions or variables into Internal RAM (IRAM). 
// This is crucial for interrupt service routines (ISRs) because they need to execute quickly and reliably, without the latency that can come from fetching code from flash memory. 
//By placing an ISR in IRAM, you ensure that it can be executed with minimal delay, which is essential for handling time-sensitive tasks in embedded systems.
void IRAM_ATTR onButtonPress() {
  unsigned long now = millis();
  if (now - lastDebounce >= DEBOUNCE_MS) {   // 50ms debounce window
    pendingPulses++;
    lastDebounce = now;
  }
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

    // ── Sensor reads ──────────────────────────────────────
    float dist_m = readDistanceMetres();
    float lcVol_L = readLoadCellVolume();

    // ── Water tank calculations ───────────────────────────
    bool  waterFault = (dist_m < 0.0f);
    float height_m = waterFault ? 0.0f : distanceToHeight(dist_m, TANK_HEIGHT_M);
    float volume_L = waterFault ? 0.0f : heightToVolume(height_m, TANK_RADIUS_M);
    bool  waterLow = waterFault ? false : isWaterLow(height_m, TANK_MIN_LEVEL_M);

    // ── Chemical tank calculations ────────────────────────
    bool chemLow = isChemLow(chemRemaining_L, CHEM_LOW_THRESHOLD_L);
    bool mismatch = isMismatch(chemRemaining_L, lcVol_L, MISMATCH_TOLERANCE_L);

    // ── Fault summary ─────────────────────────────────────
    bool anyFault = waterFault || waterLow || chemLow || mismatch;

    // ── Serial dashboard ──────────────────────────────────
    unsigned long t = millis() / 1000;
    Serial.println("+--------------------------------------------------+");
    Serial.print("| T="); Serial.print(t); Serial.println("s");
    Serial.println("+--------------------------------------------------+");

    // Water tank
    Serial.print("| [WATER] ");
    if (waterFault) {
      Serial.println("FAULT — ultrasonic timeout");
    }
    else {
      Serial.print("dist="); Serial.print(dist_m * 100.0f, 1); Serial.print("cm  ");
      Serial.print("h=");    Serial.print(height_m * 100.0f, 1); Serial.print("cm  ");
      Serial.print("V=");    Serial.print(volume_L, 1); Serial.println("L");
      Serial.print("|         ");
      Serial.println(waterLow ? "*** WATER LOW ***" : "OK");
    }

    // Chemical tank
    Serial.print("| [CHEM ] ");
    Serial.print("flow="); Serial.print(chemRemaining_L, 3); Serial.print("L  ");
    Serial.print("lc=");   Serial.print(lcVol_L, 3); Serial.print("L  ");
    Serial.print("pulses="); Serial.println(totalPulses);
    Serial.print("|         ");
    if (chemLow)  Serial.println("*** CHEM LOW ***");
    else if (mismatch) Serial.println("*** MISMATCH — check for leak/spill ***");
    else               Serial.println("OK");

    // Status
    Serial.println("+--------------------------------------------------+");
    Serial.print("| [STATUS] ");
    Serial.println(anyFault ? "FAULT" : "OK");
    Serial.println("+--------------------------------------------------+");

    updateLEDs(anyFault);
  }
}