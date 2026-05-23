// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Phase 2a: Solenoid valve + water flow meter + dosing cycle
//
//  Hardware: ESP32-S3-DevKitC-1
//  Simulator: Wokwi (VS Code extension)
//
//  Sensors:
//    Water tank   — HC-SR04 ultrasonic (GPIO5 TRIG, GPIO17 ECHO)
//    Chem tank    — Push button flow meter sim (GPIO4)
//                   Potentiometer load cell sim (GPIO6 ADC)
//  Actuators:
//    Solenoid     — LED on GPIO7 (green = open)
//    Water flow   — Push button on GPIO8 (interrupt)
//  Indicators:
//    Fault LED    — GPIO15 (red)
//    OK LED       — GPIO16 (green)
//
//  All physical constants and pin definitions in include/config.h
// ============================================================
#include <Arduino.h>
#include "water_tank.h"
#include "chem_tank.h"
#include "solenoid.h"
#include "config.h"

// ── Runtime state — Phase 1 ───────────────────────────────────
static float  chemRemaining_L = CHEM_INITIAL_VOL_L;
static long   totalChemPulses = 0;

// ── Runtime state — Phase 2a ──────────────────────────────────
static DosingState dosingState = DosingState::IDLE;
static DosingFault dosingFault = DosingFault::NONE;
static float  waterDispensed_L = 0.0f;   // accumulated this cycle
static float  closingFlow_L = 0.0f;   // flow accumulated after close cmd
static unsigned long dosingStart_ms = 0;
static unsigned long closingStart_ms = 0;

// Phase 2a temporary trigger — replaced by MQTT queue in Phase 4
static volatile bool refillRequested = false;

// ── Report timing ─────────────────────────────────────────────
static unsigned long lastReport = 0;

// ── Debounce timestamps ───────────────────────────────────────
static volatile unsigned long lastChemDebounce = 0;
static volatile unsigned long lastWaterDebounce = 0;

// ── Interrupt pulse counters ──────────────────────────────────
volatile long pendingChemPulses = 0;
volatile long pendingWaterPulses = 0;

// ============================================================
//  ISRs
// ============================================================
void IRAM_ATTR onChemPulse() {
  unsigned long now = millis();
  if (now - lastChemDebounce >= DEBOUNCE_MS) {
    pendingChemPulses++;
    lastChemDebounce = now;
  }
}

// Water flow meter ISR — only counts when valve is open (DOSING or CLOSING)
// In CLOSING state pulses count toward stuck valve detection
void IRAM_ATTR onWaterPulse() {
  unsigned long now = millis();
  if (now - lastWaterDebounce >= DEBOUNCE_MS) {
    pendingWaterPulses++;
    lastWaterDebounce = now;
  }
}

// ============================================================
//  Solenoid control
// ============================================================
void openSolenoid() {
  digitalWrite(SOLENOID_PIN, HIGH);  // LED on = valve open
  Serial.println("  [VALVE] OPEN");
}

void closeSolenoid() {
  digitalWrite(SOLENOID_PIN, LOW);   // LED off = valve closed
  Serial.println("  [VALVE] CLOSE");
}

// ============================================================
//  Process pending pulses from ISRs
// ============================================================
void processPendingPulses() {
  // ── Chemical pulses ───────────────────────────────────────
  if (pendingChemPulses > 0) {
    noInterrupts();
    long pulses = pendingChemPulses;
    pendingChemPulses = 0;
    interrupts();

    totalChemPulses += pulses;
    float dispensed = pulseToVolume(pulses, ML_PER_PULSE);
    chemRemaining_L = updateRemaining(chemRemaining_L, dispensed);

    Serial.print("  [CHEM PULSE] #"); Serial.print(totalChemPulses);
    Serial.print("  +"); Serial.print(dispensed * 1000.0f, 1);
    Serial.print(" mL  remaining: "); Serial.print(chemRemaining_L, 3);
    Serial.println(" L");
  }

  // ── Water pulses ──────────────────────────────────────────
  if (pendingWaterPulses > 0) {
    noInterrupts();
    long pulses = pendingWaterPulses;
    pendingWaterPulses = 0;
    interrupts();

    float vol = pulseToVolume(pulses, ML_PER_PULSE);

    if (dosingState == DosingState::DOSING) {
      waterDispensed_L += vol;
      Serial.print("  [WATER PULSE] +"); Serial.print(vol * 1000.0f, 1);
      Serial.print(" mL  dispensed: "); Serial.print(waterDispensed_L * 1000.0f, 1);
      Serial.print(" mL / "); Serial.print(DISPENSE_TARGET_L * 1000.0f, 0);
      Serial.println(" mL");
    }
    else if (dosingState == DosingState::CLOSING) {
      closingFlow_L += vol;
      Serial.print("  [WATER PULSE] flow after close: ");
      Serial.print(closingFlow_L * 1000.0f, 1); Serial.println(" mL");
    }
    // Pulses outside DOSING/CLOSING states are ignored
  }
}

// ============================================================
//  Dosing state machine
// ============================================================
void startDosing(float current_level_m) {
  if (dosingState != DosingState::IDLE) return;

  if (!isSufficientWater(current_level_m, TANK_MIN_LEVEL_M)) {
    dosingFault = DosingFault::DRY_TANK;
    dosingState = DosingState::FAULT;
    Serial.println("  [DOSE] PRE-CHECK FAILED — dry tank");
    return;
  }

  // Pre-check passed — begin dosing
  waterDispensed_L = 0.0f;
  dosingStart_ms = millis();
  dosingFault = DosingFault::NONE;
  dosingState = DosingState::DOSING;
  openSolenoid();
  Serial.print("  [DOSE] START — target: ");
  Serial.print(DISPENSE_TARGET_L * 1000.0f, 0);
  Serial.println(" mL");
}

void updateDosingStateMachine() {
  unsigned long now = millis();

  switch (dosingState) {

  case DosingState::IDLE:
    // Nothing to do — waiting for refill request
    break;

  case DosingState::DOSING: {
    unsigned long elapsed = now - dosingStart_ms;

    // Check blocked valve — no flow after grace period
    if (isBlockedValve(waterDispensed_L, elapsed, BLOCKED_GRACE_MS)) {
      closeSolenoid();
      dosingFault = DosingFault::BLOCKED_VALVE;
      dosingState = DosingState::FAULT;
      Serial.println("  [DOSE] FAULT — blocked valve");
      break;
    }

    // Check timeout
    if (isDosingTimeout(elapsed, DOSING_TIMEOUT_MS)) {
      closeSolenoid();
      dosingFault = DosingFault::TIMEOUT;
      dosingState = DosingState::FAULT;
      Serial.println("  [DOSE] FAULT — timeout");
      break;
    }

    // Check target reached
    if (isTargetReached(waterDispensed_L, DISPENSE_TARGET_L)) {
      closeSolenoid();
      closingFlow_L = 0.0f;
      closingStart_ms = now;
      dosingState = DosingState::CLOSING;
      Serial.print("  [DOSE] TARGET REACHED — ");
      Serial.print(waterDispensed_L * 1000.0f, 1);
      Serial.println(" mL dispensed. Verifying valve closed...");
    }
    break;
  }

  case DosingState::CLOSING: {
    unsigned long elapsed = now - closingStart_ms;

    // Check stuck valve — flow continues after close
    if (isStuckOpen(closingFlow_L, STUCK_VALVE_TOL_L)) {
      dosingFault = DosingFault::STUCK_VALVE;
      dosingState = DosingState::FAULT;
      Serial.println("  [DOSE] FAULT — stuck valve detected");
      break;
    }

    // Verification window elapsed — valve confirmed closed
    if (elapsed >= CLOSING_VERIFY_MS) {
      dosingState = DosingState::COMPLETE;
      Serial.println("  [DOSE] COMPLETE — valve confirmed closed");
    }
    break;
  }

  case DosingState::COMPLETE:
    // Stay in COMPLETE until acknowledged — reset to IDLE
    dosingState = DosingState::IDLE;
    Serial.println("  [DOSE] IDLE — ready for next request");
    break;

  case DosingState::FAULT:
    // Stay in FAULT until manually reset
    // Phase 4: fault acknowledgement comes via MQTT
    break;
  }
}

// ============================================================
//  Hardware reads
// ============================================================
float readDistanceMetres() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration_us = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (duration_us == 0) return -1.0f;
  return duration_us * SPEED_OF_SOUND_M_PER_US / 2.0f;
}

float readLoadCellVolume() {
  int   raw = analogRead(LOADCELL_PIN);
  float mass = adcToMass(raw, ADC_MAX, LOADCELL_MAX_KG);
  return massToVolume(mass, CHEM_DENSITY_KG_L);
}

// ============================================================
//  LED indicators
// ============================================================
void updateLEDs(bool anyFault) {
  digitalWrite(LED_FAULT, anyFault ? HIGH : LOW);
  digitalWrite(LED_OK, !anyFault ? HIGH : LOW);
}

// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  // Water tank
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // Chemical flow meter
  pinMode(FLOWPULSE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOWPULSE_PIN),
    onChemPulse, FALLING);

  // Solenoid valve
  pinMode(SOLENOID_PIN, OUTPUT);
  digitalWrite(SOLENOID_PIN, LOW);  // start closed

  // Water flow meter
  pinMode(WATER_FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WATER_FLOW_PIN),
    onWaterPulse, FALLING);

  // Load cell ADC
  pinMode(LOADCELL_PIN, INPUT);

  // LEDs
  pinMode(LED_FAULT, OUTPUT);
  pinMode(LED_OK, OUTPUT);

  // Temporary refill request trigger
  pinMode(REFILL_REQUEST_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(REFILL_REQUEST_PIN), []() { refillRequested = true; }, FALLING);

  Serial.println("+--------------------------------------------------+");
  Serial.println("| Hospital Dosing System — Phase 2a                |");
  Serial.println("| Water tank + Chemical tank + Dosing cycle        |");
  Serial.println("+--------------------------------------------------+");
  Serial.println("| [Tip] HC-SR04 slider  — water level              |");
  Serial.println("| [Tip] Button GPIO4    — chemical flow pulses     |");
  Serial.println("| [Tip] Button GPIO8    — water flow pulses        |");
  Serial.println("| [Tip] Potentiometer   — load cell reading        |");
  Serial.println("| [Tip] Button GPIO9    — request refill (temp)    |");
  Serial.println("+--------------------------------------------------+");
}

// ============================================================
void loop() {
  processPendingPulses();
  updateDosingStateMachine();

  // ── Temporary refill trigger — Phase 2a only ──────────────
  // Phase 4: replaced by MQTT queue check
  if (refillRequested) {
    refillRequested = false;
    float dist_m = readDistanceMetres();
    float height_m = (dist_m < 0.0f) ? 0.0f :
      distanceToHeight(dist_m, TANK_HEIGHT_M);
    startDosing(height_m);
  }

  if (millis() - lastReport >= REPORT_MS) {
    lastReport = millis();

    // ── Sensor reads ────────────────────────────────────────
    float dist_m = readDistanceMetres();
    float lcVol_L = readLoadCellVolume();

    // ── Water tank ──────────────────────────────────────────
    bool  waterFault = (dist_m < 0.0f);
    float height_m = waterFault ? 0.0f : distanceToHeight(dist_m, TANK_HEIGHT_M);
    float volume_L = waterFault ? 0.0f : heightToVolume(height_m, TANK_RADIUS_M);
    bool  waterLow = waterFault ? false : isWaterLow(height_m, TANK_MIN_LEVEL_M);

    // ── Chemical tank ────────────────────────────────────────
    bool chemLow = isChemLow(chemRemaining_L, CHEM_LOW_THRESHOLD_L);
    bool mismatch = isMismatch(chemRemaining_L, lcVol_L, MISMATCH_TOLERANCE_L);

    // ── Fault summary ────────────────────────────────────────
    bool anyFault = waterFault || waterLow || chemLow || mismatch
      || (dosingState == DosingState::FAULT);

    // ── Serial dashboard ─────────────────────────────────────
    unsigned long t = millis() / 1000;
    Serial.println();
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
    Serial.print("pulses="); Serial.println(totalChemPulses);
    Serial.print("|         ");
    if (chemLow)  Serial.println("*** CHEM LOW ***");
    else if (mismatch) Serial.println("*** MISMATCH — check for leak/spill ***");
    else               Serial.println("OK");

    // Dosing state
    Serial.print("| [DOSE ] ");
    Serial.print("state="); Serial.print(stateToString(dosingState));
    Serial.print("  dispensed=");
    Serial.print(waterDispensed_L * 1000.0f, 1); Serial.print("mL");
    Serial.print("  target=");
    Serial.print(DISPENSE_TARGET_L * 1000.0f, 0); Serial.println("mL");
    if (dosingFault != DosingFault::NONE) {
      Serial.print("|         FAULT: ");
      Serial.println(faultToString(dosingFault));
    }

    // Status
    Serial.println("+--------------------------------------------------+");
    Serial.print("| [STATUS] ");
    Serial.println(anyFault ? "FAULT" : "OK");
    Serial.println("+--------------------------------------------------+");

    updateLEDs(anyFault);
  }
}