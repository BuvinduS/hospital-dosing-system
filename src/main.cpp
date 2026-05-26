// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Phase 2b: Simultaneous water + chemical dosing
//
//  Hardware: ESP32-S3-DevKitC-1
//  Simulator: Wokwi (VS Code extension)
//
//  Sensors:
//    Water tank   — HC-SR04 ultrasonic (GPIO5 TRIG, GPIO17 ECHO)
//    Chem tank    — Push button flow meter sim (GPIO4)
//                   Potentiometer load cell sim (GPIO6 ADC)
//  Actuators:
//    Solenoid     — LED on GPIO7  (magenta = open)
//    Water flow   — Push button on GPIO8
//    Chem pump    — LED on GPIO10 (blue = running)
//    Chem flow    — Push button on GPIO11
//  Indicators:
//    Fault LED    — GPIO15 (red)
//    OK LED       — GPIO16 (green)
//
//  Refill trigger — Push button on GPIO9 (temporary — Phase 4: MQTT)
//  All physical constants and pin definitions in include/config.h
// ============================================================
#include <Arduino.h>
#include "water_tank.h"
#include "chem_tank.h"
#include "solenoid.h"
#include "pump.h"
#include "config.h"

// ── Runtime state — tanks ─────────────────────────────────────
static float chemRemaining_L = CHEM_INITIAL_VOL_L;
static long  totalChemPulses = 0;

// ── Runtime state — dosing cycle ──────────────────────────────
static DosingState dosingState = DosingState::IDLE;
static DosingFault dosingFault = DosingFault::NONE;
static float waterDispensed_L = 0.0f;
static float chemDispensed_L = 0.0f;
static float chemCycleTarget_L = 0.0f;  // computed at cycle start
static float closingFlow_L = 0.0f;
static unsigned long dosingStart_ms = 0;
static unsigned long toppingUpStart_ms = 0;
static unsigned long closingStart_ms = 0;
static bool ratioOk = false;
static float chemCycleActualTarget_L = 0.0f;
static bool chemPumpStopped = false;  // true when pump stopped mid-cycle

// ── Temporary refill trigger ──────────────────────────────────
// Phase 4: replaced by MQTT queue
static volatile bool refillRequested = false;

// ── Report timing ─────────────────────────────────────────────
static unsigned long lastReport = 0;

// ── Debounce timestamps ───────────────────────────────────────
static volatile unsigned long lastChemDebounce = 0;
static volatile unsigned long lastWaterDebounce = 0;
static volatile unsigned long lastCycleChemDebounce = 0;

// ── Interrupt pulse counters ──────────────────────────────────
volatile long pendingChemPulses = 0;  // chemical tank stock tracking
volatile long pendingWaterPulses = 0;  // water dosing cycle
volatile long pendingCycleChemPulses = 0;  // chemical dosing cycle

// ============================================================
//  ISRs
// ============================================================

// Chemical tank stock tracking (GPIO4)
void IRAM_ATTR onChemStockPulse() {
  unsigned long now = millis();
  if (now - lastChemDebounce >= DEBOUNCE_MS) {
    pendingChemPulses++;
    lastChemDebounce = now;
  }
}

// Water flow meter — dosing cycle (GPIO8)
void IRAM_ATTR onWaterPulse() {
  unsigned long now = millis();
  if (now - lastWaterDebounce >= DEBOUNCE_MS) {
    pendingWaterPulses++;
    lastWaterDebounce = now;
  }
}

// Chemical flow meter — dosing cycle (GPIO11)
void IRAM_ATTR onCycleChemPulse() {
  unsigned long now = millis();
  if (now - lastCycleChemDebounce >= DEBOUNCE_MS) {
    pendingCycleChemPulses++;
    lastCycleChemDebounce = now;
  }
}

// ============================================================
//  Actuator control
// ============================================================
void openSolenoid() {
  digitalWrite(SOLENOID_PIN, HIGH);
  Serial.println("  [VALVE] OPEN");
}

void closeSolenoid() {
  digitalWrite(SOLENOID_PIN, LOW);
  Serial.println("  [VALVE] CLOSE");
}

void startPump() {
  digitalWrite(PUMP_PIN, HIGH);
  Serial.println("  [PUMP ] ON");
}

void stopPump() {
  digitalWrite(PUMP_PIN, LOW);
  Serial.println("  [PUMP ] OFF");
}

// ============================================================
//  Process pending pulses
// ============================================================
void processPendingPulses() {
  // ── Chemical tank stock (GPIO4) ───────────────────────────
  if (pendingChemPulses > 0) {
    noInterrupts();
    long pulses = pendingChemPulses;
    pendingChemPulses = 0;
    interrupts();

    totalChemPulses += pulses;
    float dispensed = pulseToVolume(pulses, ML_PER_PULSE);
    chemRemaining_L = updateRemaining(chemRemaining_L, dispensed);

    Serial.print("  [CHEM STOCK] #"); Serial.print(totalChemPulses);
    Serial.print("  remaining: "); Serial.print(chemRemaining_L, 3);
    Serial.println(" L");
  }

  // ── Water dosing (GPIO8) ──────────────────────────────────
  if (pendingWaterPulses > 0) {
    noInterrupts();
    long pulses = pendingWaterPulses;
    pendingWaterPulses = 0;
    interrupts();

    float vol = pulseToVolume(pulses, ML_PER_PULSE);

    if (dosingState == DosingState::DOSING) {
      waterDispensed_L += vol;
      Serial.print("  [WATER] +"); Serial.print(vol * 1000.0f, 1);
      Serial.print(" mL  total: "); Serial.print(waterDispensed_L * 1000.0f, 1);
      Serial.print(" / "); Serial.print(DISPENSE_TARGET_L * 1000.0f, 0);
      Serial.println(" mL");
    }
    else if (dosingState == DosingState::CLOSING) {
      closingFlow_L += vol;
    }
  }

  // ── Chemical dosing (GPIO11) ──────────────────────────────
  if (pendingCycleChemPulses > 0) {
    noInterrupts();
    long pulses = pendingCycleChemPulses;
    pendingCycleChemPulses = 0;
    interrupts();

    float vol = pulseToVolume(pulses, ML_PER_PULSE);

    if (dosingState == DosingState::DOSING ||
      dosingState == DosingState::TOPPING_UP) {
      chemDispensed_L += vol;
      // Also subtract from stock
      chemRemaining_L = updateRemaining(chemRemaining_L, vol);
      Serial.print("  [CHEM ] +"); Serial.print(vol * 1000.0f, 1);
      Serial.print(" mL  total: "); Serial.print(chemDispensed_L * 1000.0f, 1);
      Serial.print(" / "); Serial.print(chemCycleTarget_L * 1000.0f, 1);
      Serial.println(" mL");
    }
  }
}

// ============================================================
//  Dosing state machine
// ============================================================
void startDosing(float current_level_m) {
  chemPumpStopped = false;
  if (dosingState != DosingState::IDLE) return;

  if (!isSufficientWater(current_level_m, TANK_MIN_LEVEL_M)) {
    dosingFault = DosingFault::DRY_TANK;
    dosingState = DosingState::FAULT;
    Serial.println("  [DOSE] PRE-CHECK FAILED — dry tank");
    return;
  }

  // Compute chemical target for this cycle
  chemCycleTarget_L = chemTargetVolume(DISPENSE_TARGET_L, DILUTION_RATIO);
  chemCycleActualTarget_L = chemCycleTarget_L;  // for reporting at end of cycle
  waterDispensed_L = 0.0f;
  chemDispensed_L = 0.0f;
  closingFlow_L = 0.0f;
  ratioOk = false;
  dosingFault = DosingFault::NONE;
  dosingStart_ms = millis();
  dosingState = DosingState::DOSING;

  openSolenoid();
  startPump();

  Serial.print("  [DOSE] START — water target: ");
  Serial.print(DISPENSE_TARGET_L * 1000.0f, 0);
  Serial.print(" mL  chem target: ");
  Serial.print(chemCycleTarget_L * 1000.0f, 1);
  Serial.println(" mL");
}

void updateDosingStateMachine() {
  unsigned long now = millis();

  switch (dosingState) {

  case DosingState::IDLE:
    break;

  case DosingState::DOSING: {
    unsigned long elapsed = now - dosingStart_ms;

    // Blocked valve check
    if (isBlockedValve(waterDispensed_L, elapsed, BLOCKED_GRACE_MS)) {
      closeSolenoid();
      stopPump();
      dosingFault = DosingFault::BLOCKED_VALVE;
      dosingState = DosingState::FAULT;
      Serial.println("  [DOSE] FAULT — blocked valve");
      break;
    }

    // Pump failure check
    if (isPumpFailure(chemDispensed_L, elapsed, PUMP_GRACE_MS)) {
      closeSolenoid();
      stopPump();
      dosingFault = DosingFault::PUMP_FAILURE;
      dosingState = DosingState::FAULT;
      Serial.println("  [DOSE] FAULT — pump failure");
      break;
    }

    // Dosing timeout
    if (isDosingTimeout(elapsed, DOSING_TIMEOUT_MS)) {
      closeSolenoid();
      stopPump();
      dosingFault = DosingFault::TIMEOUT;
      dosingState = DosingState::FAULT;
      Serial.println("  [DOSE] FAULT — timeout");
      break;
    }

    // ── Chemical finished before water ────────────────────────
    if (!chemPumpStopped &&
      isChemTargetReached(chemDispensed_L, chemCycleTarget_L)) {
      stopPump();
      chemPumpStopped = true;  // ← prevents re-entry
      Serial.println("  [DOSE] Chem target reached — pump stopped, water continuing");
    }

    // ── Water finished ────────────────────────────────────────
    if (isTargetReached(waterDispensed_L, DISPENSE_TARGET_L)) {
      closeSolenoid();
      Serial.println("  [DOSE] Water target reached");

      if (chemPumpStopped ||
        isChemTargetReached(chemDispensed_L, chemCycleTarget_L)) {
        closingFlow_L = 0.0f;
        closingStart_ms = now;
        dosingState = DosingState::CLOSING;
        Serial.println("  [DOSE] Both complete — verifying valves...");
      }
      else {
        toppingUpStart_ms = now;
        dosingState = DosingState::TOPPING_UP;
        Serial.print("  [DOSE] TOPPING UP — chem remaining: ");
        Serial.print((chemCycleTarget_L - chemDispensed_L) * 1000.0f, 1);
        Serial.println(" mL");
      }
    }
    break;
  }

  case DosingState::TOPPING_UP: {
    unsigned long elapsed = now - toppingUpStart_ms;

    // Pump failure during top-up
    if (isPumpFailure(chemDispensed_L, elapsed + PUMP_GRACE_MS,
      PUMP_GRACE_MS)) {
      stopPump();
      dosingFault = DosingFault::PUMP_FAILURE;
      dosingState = DosingState::FAULT;
      Serial.println("  [DOSE] FAULT — pump failure during top-up");
      break;
    }

    // Top-up timeout
    if (isDosingTimeout(elapsed, TOPPING_UP_TIMEOUT_MS)) {
      stopPump();
      dosingFault = DosingFault::TIMEOUT;
      dosingState = DosingState::FAULT;
      Serial.println("  [DOSE] FAULT — top-up timeout");
      break;
    }

    // Chemical target reached
    if (isChemTargetReached(chemDispensed_L, chemCycleTarget_L)) {
      stopPump();
      closingFlow_L = 0.0f;
      closingStart_ms = now;
      dosingState = DosingState::CLOSING;
      Serial.println("  [DOSE] Chem target reached — verifying valves...");
    }
    break;
  }

  case DosingState::CLOSING: {
    unsigned long elapsed = now - closingStart_ms;

    // Stuck valve check
    if (isStuckOpen(closingFlow_L, STUCK_VALVE_TOL_L)) {
      dosingFault = DosingFault::STUCK_VALVE;
      dosingState = DosingState::FAULT;
      Serial.println("  [DOSE] FAULT — stuck valve");
      break;
    }

    if (elapsed >= CLOSING_VERIFY_MS) {
      // Verify ratio before marking complete
      ratioOk = isRatioCorrect(waterDispensed_L, chemDispensed_L,
        DILUTION_RATIO, RATIO_TOLERANCE);
      dosingState = DosingState::COMPLETE;
      Serial.print("  [DOSE] COMPLETE — ratio ");
      Serial.println(ratioOk ? "OK" : "WARNING: out of tolerance");
    }
    break;
  }

  case DosingState::COMPLETE:
    dosingState = DosingState::IDLE;
    Serial.println("  [DOSE] IDLE — ready for next request");
    break;

  case DosingState::FAULT:
    // Held until manual reset — Phase 4: MQTT acknowledgement
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

void updateLEDs(bool anyFault) {
  digitalWrite(LED_FAULT, anyFault ? HIGH : LOW);
  digitalWrite(LED_OK, !anyFault ? HIGH : LOW);
}

// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  pinMode(FLOWPULSE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOWPULSE_PIN),
    onChemStockPulse, FALLING);

  pinMode(SOLENOID_PIN, OUTPUT);
  digitalWrite(SOLENOID_PIN, LOW);

  pinMode(WATER_FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WATER_FLOW_PIN),
    onWaterPulse, FALLING);

  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

  pinMode(CHEM_FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CHEM_FLOW_PIN),
    onCycleChemPulse, FALLING);

  pinMode(LOADCELL_PIN, INPUT);
  pinMode(LED_FAULT, OUTPUT);
  pinMode(LED_OK, OUTPUT);

  pinMode(REFILL_REQUEST_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(REFILL_REQUEST_PIN),
    []() { refillRequested = true; }, FALLING);

  Serial.println("+--------------------------------------------------+");
  Serial.println("| Hospital Dosing System — Phase 2b                |");
  Serial.println("| Simultaneous water + chemical dosing             |");
  Serial.println("+--------------------------------------------------+");
  Serial.println("| [GPIO4 ] btn green  — chem stock pulse           |");
  Serial.println("| [GPIO8 ] btn blue   — water flow pulse           |");
  Serial.println("| [GPIO9 ] btn red    — request refill (temp)      |");
  Serial.println("| [GPIO11] btn yellow — chem flow pulse            |");
  Serial.println("| [GPIO7 ] LED magenta— solenoid valve             |");
  Serial.println("| [GPIO10] LED blue   — chemical pump              |");
  Serial.println("+--------------------------------------------------+");
}

// ============================================================
void loop() {
  processPendingPulses();
  updateDosingStateMachine();

  // ── Temporary refill trigger ──────────────────────────────
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

    // ── Fault summary ─────────────────────────────────────────
    bool anyFault = waterFault || waterLow || chemLow || mismatch
      || (dosingState == DosingState::FAULT);

    // ── Serial dashboard ──────────────────────────────────────
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
    Serial.print("stock="); Serial.print(chemRemaining_L, 3); Serial.print("L  ");
    Serial.print("lc=");    Serial.print(lcVol_L, 3); Serial.print("L  ");
    Serial.print("pulses="); Serial.println(totalChemPulses);
    Serial.print("|         ");
    if (chemLow)  Serial.println("*** CHEM LOW ***");
    else if (mismatch) Serial.println("*** MISMATCH — check for leak/spill ***");
    else               Serial.println("OK");

    // Dosing cycle
    Serial.print("| [DOSE ] ");
    Serial.print("state="); Serial.print(stateToString(dosingState));
    Serial.print("  W="); Serial.print(waterDispensed_L * 1000.0f, 1); Serial.print("mL");
    Serial.print("  C="); Serial.print(chemDispensed_L * 1000.0f, 1); Serial.println("mL");
    Serial.print("|         ");
    Serial.print("W_target="); Serial.print(DISPENSE_TARGET_L * 1000.0f, 0);
    Serial.print("  C_target=");
    Serial.print(chemCycleActualTarget_L * 1000.0f, 1);
    Serial.println("mL");
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