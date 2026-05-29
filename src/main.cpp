// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Phase 4a: Alarm system — RGB LED, buzzer, OLED display
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
#include "refill_queue.h"
#include "alarm_manager.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ILI9341.h>

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

// ── Run time state — Refill queue ──────────────────────────────────────────────
static RefillQueue refillQueue;
static uint8_t currentTankId = 0;  // tank currently being dosed 0 = none
static uint8_t lastCompletedId = 0;  // tank that just finished — pending notification

// ── Run time state — Alarm state ───────────────────────────────────────────────
static AlarmSeverity currentSeverity = AlarmSeverity::NONE;
static bool          buzzerOn = false;
static unsigned long lastBeepTime_ms = 0;
static unsigned long buzzerOnTime_ms = 0;

// ── OLED ─────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1   // no reset pin
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  &Wire, OLED_RESET);
static bool oledAvailable = false;

// ── Tank node simulation ──────────────────────────────────────
// In production: separate ESP32 per tank, communicates via MQTT
// In simulation: buttons on same ESP32, calls handler directly
static volatile bool tank1Requested = false;
static volatile bool tank2Requested = false;

// ── Report timing ─────────────────────────────────────────────
static unsigned long lastReport = 0;

// ── Debounce timestamps ───────────────────────────────────────
static volatile unsigned long lastChemDebounce = 0;
static volatile unsigned long lastWaterDebounce = 0;
static volatile unsigned long lastCycleChemDebounce = 0;
static volatile unsigned long lastTank1Debounce = 0;
static volatile unsigned long lastTank2Debounce = 0;

// ── Interrupt pulse counters ──────────────────────────────────
volatile long pendingChemPulses = 0;  // chemical tank stock tracking
volatile long pendingWaterPulses = 0;  // water dosing cycle
volatile long pendingCycleChemPulses = 0;  // chemical dosing cycle

// ── Usage log ─────────────────────────────────────────────────
struct RefillRecord {
  uint8_t       tankId;
  unsigned long timestamp_s;      // seconds since boot
  float         waterDispensed_L;
  float         chemDispensed_L;
  bool          ratioOk;
  DosingFault   fault;
  unsigned long duration_s;
  unsigned long cycleStart_s;     // when cycle started
};

static RefillRecord usageLog[MAX_LOG_RECORDS];
static uint8_t      logCount = 0;
static bool         logFull = false;

static unsigned long cycleStart_s = 0;

// ── TFT ───────────────────────────────────────────────────────
static Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
static bool tftAvailable = false;

// ── ILI9341 colour constants ──────────────────────────────────
#define TFT_BLACK   0x0000
#define TFT_WHITE   0xFFFF
#define TFT_RED     0xF800
#define TFT_GREEN   0x07E0
#define TFT_BLUE    0x001F
#define TFT_YELLOW  0xFFE0
#define TFT_CYAN    0x07FF
#define TFT_MAGENTA 0xF81F
#define TFT_GREY    0x8410

// ── Report button ─────────────────────────────────────────────
static volatile bool reportRequested = false;
static volatile unsigned long lastReportBtnDebounce = 0;

// ── Manual Reset ──────────────────────────────────
static volatile bool faultResetRequested = false;

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

// Tank node simulation ISRs
// In production: MQTT publish from tank node triggers this
// In simulation: button press triggers directly
void IRAM_ATTR onTank1Request() {
  unsigned long now = millis();
  if (now - lastTank1Debounce >= TANK_DEBOUNCE_MS) {
    tank1Requested = true;
    lastTank1Debounce = now;
  }
}

void IRAM_ATTR onTank2Request() {
  unsigned long now = millis();
  if (now - lastTank2Debounce >= TANK_DEBOUNCE_MS) {
    tank2Requested = true;
    lastTank2Debounce = now;
  }
}

void IRAM_ATTR onFaultReset() {
  faultResetRequested = true;
}

void IRAM_ATTR onReportRequest() {
  unsigned long now = millis();
  if (now - lastReportBtnDebounce >= TANK_DEBOUNCE_MS) {
    reportRequested = true;
    lastReportBtnDebounce = now;
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

void logRefillRecord(uint8_t tankId, float waterL, float chemL,
  bool ratioOk, DosingFault fault,
  unsigned long startSec) {
  if (logCount >= MAX_LOG_RECORDS) {
    // Overwrite oldest — circular
    for (uint8_t i = 0; i < MAX_LOG_RECORDS - 1; i++) {
      usageLog[i] = usageLog[i + 1];
    }
    logCount = MAX_LOG_RECORDS - 1;
    logFull = true;
  }
  RefillRecord r;
  r.tankId = tankId;
  r.timestamp_s = millis() / 1000;
  r.waterDispensed_L = waterL;
  r.chemDispensed_L = chemL;
  r.ratioOk = ratioOk;
  r.fault = fault;
  r.duration_s = (millis() / 1000) - startSec;
  r.cycleStart_s = startSec;
  usageLog[logCount++] = r;
}

// ============================================================
//  Dosing state machine
// ============================================================
void startDosing(float current_level_m) {
  cycleStart_s = millis() / 1000;
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
      logRefillRecord(currentTankId,
        waterDispensed_L, chemDispensed_L,
        false, DosingFault::BLOCKED_VALVE,
        cycleStart_s);
      dosingState = DosingState::FAULT;
      Serial.println("  [DOSE] FAULT — blocked valve");
      break;
    }

    // Pump failure check
    if (isPumpFailure(chemDispensed_L, elapsed, PUMP_GRACE_MS)) {
      closeSolenoid();
      stopPump();
      dosingFault = DosingFault::PUMP_FAILURE;
      logRefillRecord(currentTankId,
        waterDispensed_L, chemDispensed_L,
        false, DosingFault::BLOCKED_VALVE,
        cycleStart_s);
      dosingState = DosingState::FAULT;
      Serial.println("  [DOSE] FAULT — pump failure");
      break;
    }

    // Dosing timeout
    if (isDosingTimeout(elapsed, DOSING_TIMEOUT_MS)) {
      closeSolenoid();
      stopPump();
      dosingFault = DosingFault::TIMEOUT;
      logRefillRecord(currentTankId,
        waterDispensed_L, chemDispensed_L,
        false, DosingFault::BLOCKED_VALVE,
        cycleStart_s);
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
      logRefillRecord(currentTankId,
        waterDispensed_L, chemDispensed_L,
        false, DosingFault::BLOCKED_VALVE,
        cycleStart_s);
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
      logRefillRecord(currentTankId,
        waterDispensed_L, chemDispensed_L,
        false, DosingFault::BLOCKED_VALVE,
        cycleStart_s);
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
    lastCompletedId = currentTankId;  // ← record who just finished
    logRefillRecord(currentTankId, waterDispensed_L, chemDispensed_L, ratioOk, DosingFault::NONE, cycleStart_s);
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

// ============================================================
//  RGB LED control - must be defined before setup() 
// ============================================================
void setRGB(bool r, bool g, bool b) {
  digitalWrite(RGB_RED_PIN, r ? LOW : HIGH);  // inverted — common anode
  digitalWrite(RGB_GREEN_PIN, g ? LOW : HIGH);
  digitalWrite(RGB_BLUE_PIN, b ? LOW : HIGH);
}

void updateRGB(AlarmSeverity severity) {
  switch (severity) {
  case AlarmSeverity::NONE:     setRGB(0, 1, 0); break;  // green
  case AlarmSeverity::INFO:     setRGB(0, 0, 1); break;  // blue
  case AlarmSeverity::WARNING:  setRGB(1, 1, 0); break;  // amber
  case AlarmSeverity::CRITICAL: setRGB(1, 0, 0); break;  // red
  }
}

// ============================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  // ── Output pins first — no interrupts needed ──────────────
  pinMode(TRIG_PIN, OUTPUT); digitalWrite(TRIG_PIN, LOW);
  pinMode(SOLENOID_PIN, OUTPUT); digitalWrite(SOLENOID_PIN, LOW);
  pinMode(PUMP_PIN, OUTPUT); digitalWrite(PUMP_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);
  setRGB(0, 1, 0);

  // ── Input pins ────────────────────────────────────────────
  pinMode(ECHO_PIN, INPUT);
  pinMode(FLOWPULSE_PIN, INPUT_PULLUP);
  pinMode(WATER_FLOW_PIN, INPUT_PULLUP);
  pinMode(CHEM_FLOW_PIN, INPUT_PULLUP);
  pinMode(LOADCELL_PIN, INPUT);
  pinMode(TANK1_BTN_PIN, INPUT_PULLUP);
  pinMode(TANK2_BTN_PIN, INPUT_PULLUP);
  pinMode(FAULT_RESET_PIN, INPUT_PULLUP);
  pinMode(REPORT_BTN_PIN, INPUT_PULLUP);

  // ── OLED ──────────────────────────────────────────────────
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    oledAvailable = true;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Hospital Dosing");
    display.println("System v1.0");
    display.println("Initialising...");
    display.display();
  }
  else {
    Serial.println("[WARN] OLED not found");
  }

  // ── TFT ───────────────────────────────────────────────────
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 100);
  tft.println("Hospital Dosing");
  tft.setTextSize(1);
  tft.setCursor(10, 130);
  tft.println("Initialising...");
  tftAvailable = true;

  // ── Queue ─────────────────────────────────────────────────
  queueInit(refillQueue);

  // ── Attach ALL interrupts last — after displays initialised ──
  // This prevents spurious ISR fires during display init delays
  attachInterrupt(digitalPinToInterrupt(FLOWPULSE_PIN),
    onChemStockPulse, FALLING);
  attachInterrupt(digitalPinToInterrupt(WATER_FLOW_PIN),
    onWaterPulse, FALLING);
  attachInterrupt(digitalPinToInterrupt(CHEM_FLOW_PIN),
    onCycleChemPulse, FALLING);
  attachInterrupt(digitalPinToInterrupt(TANK1_BTN_PIN),
    onTank1Request, FALLING);
  attachInterrupt(digitalPinToInterrupt(TANK2_BTN_PIN),
    onTank2Request, FALLING);
  attachInterrupt(digitalPinToInterrupt(FAULT_RESET_PIN),
    onFaultReset, FALLING);
  attachInterrupt(digitalPinToInterrupt(REPORT_BTN_PIN),
    onReportRequest, FALLING);

  Serial.println("+--------------------------------------------------+");
  Serial.println("| Hospital Dosing System — Phase 4b                |");
  Serial.println("| Alarm + Display + Usage Logging                  |");
  Serial.println("+--------------------------------------------------+");
  Serial.println("| [GPIO4 ] btn green  — chem stock pulse           |");
  Serial.println("| [GPIO8 ] btn blue   — water flow pulse           |");
  Serial.println("| [GPIO11] btn yellow — chem flow pulse            |");
  Serial.println("| [GPIO7 ] LED magenta— solenoid valve             |");
  Serial.println("| [GPIO21] LED cyan   — chemical pump              |");
  Serial.println("| [GPIO12] btn white  — Tank 1 low level sensor    |");
  Serial.println("| [GPIO13] btn white  — Tank 2 low level sensor    |");
  Serial.println("| [GPIO14] btn orange — fault reset                |");
  Serial.println("| [GPIO18] btn purple — print report               |");
  Serial.println("+--------------------------------------------------+");
}

// ============================================================
//  Tank node simulation
//  In production: onMqttMessage() handles incoming MQTT publishes
//  In simulation: ISR flags polled here, queue updated directly
// ============================================================
void processTankRequests() {
  if (tank1Requested) {
    tank1Requested = false;
    if (currentTankId == TANK1_ID) {
      Serial.println("  [TANK 1] Already being served — request ignored");
    }
    else if (enqueue(refillQueue, TANK1_ID)) {
      Serial.print("  [TANK 1] Low level detected — queued. Queue size: ");
      Serial.println(queueSize(refillQueue));
    }
    else if (queueContains(refillQueue, TANK1_ID)) {
      Serial.println("  [TANK 1] Already in queue — request ignored");
    }
    else {
      Serial.println("  [TANK 1] Queue full — request rejected");
    }
  }

  if (tank2Requested) {
    tank2Requested = false;
    if (currentTankId == TANK2_ID) {
      Serial.println("  [TANK 2] Already being served — request ignored");
    }
    else if (enqueue(refillQueue, TANK2_ID)) {
      Serial.print("  [TANK 2] Low level detected — queued. Queue size: ");
      Serial.println(queueSize(refillQueue));
    }
    else if (queueContains(refillQueue, TANK2_ID)) {
      Serial.println("  [TANK 2] Already in queue — request ignored");
    }
    else {
      Serial.println("  [TANK 2] Queue full — request rejected");
    }
  }
}

// ============================================================
//  Queue dispatch — serves next tank when system is idle
// ============================================================
void processRefillQueue() {
  // Only runs when dosing system is idle
  if (dosingState != DosingState::IDLE) return;

  // Step 1 — handle completion notification first
  if (lastCompletedId != 0) {
    Serial.print("  [QUEUE] Tank ");
    Serial.print(lastCompletedId);
    Serial.println(" refill complete — notifying node");
    lastCompletedId = 0;
    return;  // wait one full loop before dispatching next
  }

  // Step 2 — nothing to dispatch
  if (queueIsEmpty(refillQueue)) return;

  // Step 3 — dispatch next tank
  currentTankId = dequeue(refillQueue);

  Serial.print("  [QUEUE] Serving tank ");
  Serial.print(currentTankId);
  Serial.print(" — ");
  Serial.print(queueSize(refillQueue));
  Serial.println(" remaining in queue");

  float dist_m = readDistanceMetres();
  float height_m = (dist_m < 0.0f) ? 0.0f :
    distanceToHeight(dist_m, TANK_HEIGHT_M);
  startDosing(height_m);
}

// ============================================================
//  Buzzer control
// ============================================================
void updateBuzzer(AlarmSeverity severity, unsigned long now) {
  if (severity == AlarmSeverity::NONE ||
    severity == AlarmSeverity::INFO) {
    // Silent — make sure buzzer is off
    if (buzzerOn) {
      digitalWrite(BUZZER_PIN, LOW);
      buzzerOn = false;
    }
    return;
  }

  if (buzzerOn) {
    // Buzzer currently on — check if duration expired
    if (now - buzzerOnTime_ms >= BEEP_DURATION_MS) {
      digitalWrite(BUZZER_PIN, LOW);
      buzzerOn = false;
      lastBeepTime_ms = now;
    }
  }
  else {
    // Buzzer off — check if interval elapsed
    if (shouldBeepNow(severity, now, lastBeepTime_ms, false)) {
      digitalWrite(BUZZER_PIN, HIGH);
      buzzerOn = true;
      buzzerOnTime_ms = now;
    }
  }
}

// ============================================================
//  OLED display
// ============================================================
void updateDisplay(AlarmSeverity severity,
  float height_m, float volume_L, bool waterFault,
  float chemRemaining, bool chemLow, bool mismatch,
  DosingState state, float waterDisp, float chemDisp,
  float waterTarget, float chemTarget,
  uint8_t queueSz, uint8_t servingId,
  DosingFault fault) {
  if (!oledAvailable) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  // ── Critical fault screen ─────────────────────────────────
  if (severity == AlarmSeverity::CRITICAL) {
    display.setTextSize(2);
    display.println("!! FAULT !!");
    display.setTextSize(1);
    display.println();
    if (waterFault) {
      display.println("WATER SENSOR FAIL");
    }
    if (state == DosingState::FAULT) {
      display.print("DOSE: ");
      display.println(faultToString(fault));
    }
    display.println();
    display.println("Press RESET btn");
    display.display();
    return;
  }

  // ── Normal status screen ──────────────────────────────────
  // Line 1 — header
  display.setTextSize(1);
  display.println("HOSPITAL DOSING SYS");
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
  display.setCursor(0, 12);

  // Line 2 — water tank
  display.print("W:");
  if (waterFault) {
    display.println(" SENSOR ERR");
  }
  else {
    display.print(volume_L, 0); display.print("L ");
    display.println(waterFault ? "ERR" : "OK");
  }

  // Line 3 — chemical tank
  display.print("C:");
  display.print(chemRemaining, 2); display.print("L ");
  if (chemLow)      display.println("LOW");
  else if (mismatch) display.println("MISMATCH");
  else               display.println("OK");

  // Line 4 — dosing state
  display.print("Dose:");
  display.print(stateToString(state));
  if (state == DosingState::DOSING ||
    state == DosingState::TOPPING_UP) {
    display.print(" W:");
    display.print(waterDisp * 1000.0f, 0);
    display.print("/");
    display.print(waterTarget * 1000.0f, 0);
  }
  display.println();

  // Line 5 — queue
  display.print("Q:");
  display.print(queueSz);
  if (servingId != 0) {
    display.print(" Srv:T"); display.print(servingId);
  }
  else {
    display.print(" Srv:--");
  }
  display.println();

  // Line 6 — status
  display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
  display.setCursor(0, 56);
  display.print("STATUS: ");
  display.println(severityToString(severity));

  display.display();
}

// ============================================================
//  TFT — print usage report
// ============================================================
void printReportTFT() {
  if (!tftAvailable) {
    Serial.println("[REPORT] TFT not available — printing to serial only");
    return;
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextWrap(false);

  // ── Header ────────────────────────────────────────────────
  tft.fillRect(0, 0, 320, 30, TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 8);
  tft.println("USAGE REPORT");

  // ── Session summary ────────────────────────────────────────
  uint8_t total = logCount;
  uint8_t successful = 0;
  uint8_t faulted = 0;
  float   totalWater = 0.0f;
  float   totalChem = 0.0f;

  // Per-tank accumulators (supports up to 10 tanks)
  uint8_t tankRefills[10] = { 0 };
  float   tankWater[10] = { 0 };
  float   tankChem[10] = { 0 };

  for (uint8_t i = 0; i < logCount; i++) {
    if (usageLog[i].fault == DosingFault::NONE) successful++;
    else faulted++;
    totalWater += usageLog[i].waterDispensed_L;
    totalChem += usageLog[i].chemDispensed_L;

    uint8_t tid = usageLog[i].tankId;
    if (tid >= 1 && tid <= 10) {
      tankRefills[tid - 1]++;
      tankWater[tid - 1] += usageLog[i].waterDispensed_L;
      tankChem[tid - 1] += usageLog[i].chemDispensed_L;
    }
  }

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  int y = 38;

  tft.setCursor(10, y); y += 14;
  tft.print("Session cycles : "); tft.println(total);

  tft.setTextColor(TFT_GREEN);
  tft.setCursor(10, y); y += 14;
  tft.print("Successful     : "); tft.println(successful);

  tft.setTextColor(faulted > 0 ? TFT_RED : TFT_GREEN);
  tft.setCursor(10, y); y += 14;
  tft.print("Faulted        : "); tft.println(faulted);

  tft.setTextColor(TFT_CYAN);
  tft.setCursor(10, y); y += 14;
  tft.print("Total water    : "); tft.print(totalWater, 3); tft.println(" L");

  tft.setCursor(10, y); y += 14;
  tft.print("Total chemical : "); tft.print(totalChem * 1000.0f, 1); tft.println(" mL");

  // ── Divider ───────────────────────────────────────────────
  y += 4;
  tft.drawLine(0, y, 319, y, TFT_GREY);
  y += 6;

  // ── Per-tank breakdown ────────────────────────────────────
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(10, y); y += 14;
  tft.println("By dispensing tank:");

  tft.setTextColor(TFT_WHITE);
  for (uint8_t t = 0; t < 10; t++) {
    if (tankRefills[t] == 0) continue;
    tft.setCursor(10, y); y += 14;
    tft.print("Tank "); tft.print(t + 1);
    tft.print(": "); tft.print(tankRefills[t]); tft.print(" refills");
    tft.print("  W:"); tft.print(tankWater[t], 2); tft.print("L");
    tft.print("  C:"); tft.print(tankChem[t] * 1000.0f, 0); tft.print("mL");
    if (y > 220) break;  // prevent overflow
  }

  // ── Divider ───────────────────────────────────────────────
  y += 4;
  tft.drawLine(0, y, 319, y, TFT_GREY);
  y += 6;

  // ── Recent cycles ─────────────────────────────────────────
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(10, y); y += 14;
  tft.println("Recent cycles:");

  tft.setTextSize(1);
  uint8_t start = logCount > 5 ? logCount - 5 : 0;
  for (uint8_t i = start; i < logCount; i++) {
    RefillRecord& r = usageLog[i];
    tft.setTextColor(r.fault == DosingFault::NONE ? TFT_GREEN : TFT_RED);
    tft.setCursor(10, y); y += 12;
    tft.print("T"); tft.print(r.tankId);
    tft.print(" @"); tft.print(r.timestamp_s); tft.print("s");
    tft.print(" W:"); tft.print(r.waterDispensed_L * 1000.0f, 0); tft.print("mL");
    tft.print(" C:"); tft.print(r.chemDispensed_L * 1000.0f, 0); tft.print("mL");
    if (r.fault != DosingFault::NONE) {
      tft.print(" "); tft.print(faultToString(r.fault));
    }
    else {
      tft.print(r.ratioOk ? " OK" : " RATIO!");
    }
    if (y > 300) break;
  }

  // ── Footer ────────────────────────────────────────────────
  tft.fillRect(0, 310, 320, 10, TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 311);
  tft.setTextSize(1);
  tft.print("Log: "); tft.print(logCount);
  tft.print("/"); tft.print(MAX_LOG_RECORDS);
  if (logFull) tft.print(" (oldest overwritten)");
}

// ============================================================
//  Serial report — printed alongside TFT
// ============================================================
void printReportSerial() {
  Serial.println();
  Serial.println("+--------------------------------------------------+");
  Serial.println("|              USAGE REPORT                        |");
  Serial.println("+--------------------------------------------------+");

  uint8_t total = logCount, successful = 0, faulted = 0;
  float   totalWater = 0.0f, totalChem = 0.0f;

  uint8_t tankRefills[10] = { 0 };
  float   tankWater[10] = { 0 };
  float   tankChem[10] = { 0 };

  for (uint8_t i = 0; i < logCount; i++) {
    if (usageLog[i].fault == DosingFault::NONE) successful++;
    else faulted++;
    totalWater += usageLog[i].waterDispensed_L;
    totalChem += usageLog[i].chemDispensed_L;
    uint8_t tid = usageLog[i].tankId;
    if (tid >= 1 && tid <= 10) {
      tankRefills[tid - 1]++;
      tankWater[tid - 1] += usageLog[i].waterDispensed_L;
      tankChem[tid - 1] += usageLog[i].chemDispensed_L;
    }
  }

  Serial.print("| Total cycles  : "); Serial.println(total);
  Serial.print("| Successful    : "); Serial.println(successful);
  Serial.print("| Faulted       : "); Serial.println(faulted);
  Serial.print("| Water used    : "); Serial.print(totalWater, 3);
  Serial.println(" L");
  Serial.print("| Chemical used : "); Serial.print(totalChem * 1000.0f, 1);
  Serial.println(" mL");
  Serial.println("+--------------------------------------------------+");
  Serial.println("| By tank:                                         |");
  for (uint8_t t = 0; t < 10; t++) {
    if (tankRefills[t] == 0) continue;
    Serial.print("| Tank "); Serial.print(t + 1);
    Serial.print(": "); Serial.print(tankRefills[t]); Serial.print(" refills");
    Serial.print("  W="); Serial.print(tankWater[t], 3); Serial.print("L");
    Serial.print("  C="); Serial.print(tankChem[t] * 1000.0f, 1); Serial.println("mL");
  }
  Serial.println("+--------------------------------------------------+");
  Serial.println("| Recent cycles (last 5):                          |");
  uint8_t start = logCount > 5 ? logCount - 5 : 0;
  for (uint8_t i = start; i < logCount; i++) {
    RefillRecord& r = usageLog[i];
    Serial.print("| T"); Serial.print(r.tankId);
    Serial.print(" @"); Serial.print(r.timestamp_s); Serial.print("s");
    Serial.print(" W:"); Serial.print(r.waterDispensed_L * 1000.0f, 0); Serial.print("mL");
    Serial.print(" C:"); Serial.print(r.chemDispensed_L * 1000.0f, 0); Serial.print("mL");
    Serial.print(" dur:"); Serial.print(r.duration_s); Serial.print("s");
    if (r.fault != DosingFault::NONE) {
      Serial.print(" FAULT:"); Serial.print(faultToString(r.fault));
    }
    else {
      Serial.print(r.ratioOk ? " OK" : " RATIO_WARN");
    }
    Serial.println();
  }
  Serial.println("+--------------------------------------------------+");
}

void loop() {
  processPendingPulses();
  updateDosingStateMachine();
  updateBuzzer(currentSeverity, millis());  // ← every loop, not just report

  if (faultResetRequested) {
    faultResetRequested = false;
    if (dosingState == DosingState::FAULT) {
      dosingState = DosingState::IDLE;
      dosingFault = DosingFault::NONE;
      currentTankId = 0;
      Serial.println("  [FAULT] Reset by operator — system IDLE");
    }
    else {
      Serial.println("  [RESET] No active fault — ignored");
    }
  }

  if (reportRequested) {
    reportRequested = false;
    printReportSerial();
    printReportTFT();
  }

  processTankRequests();
  processRefillQueue();

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

    // ── Alarm severity — evaluated here where all flags are in scope ──
    bool dosingActive = (dosingState == DosingState::DOSING ||
      dosingState == DosingState::TOPPING_UP ||
      dosingState == DosingState::CLOSING);
    bool dosingFaultActive = (dosingState == DosingState::FAULT);

    currentSeverity = evaluateSeverity(
      waterFault, waterLow, chemLow, mismatch,
      dosingFaultActive, dosingActive
    );

    // ── Fault summary for dashboard ───────────────────────────
    bool anyFault = waterFault || waterLow || chemLow || mismatch
      || dosingFaultActive;

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

    // Queue status
    Serial.print("| [QUEUE] ");
    Serial.print("size="); Serial.print(queueSize(refillQueue));
    Serial.print("  serving=");
    if (dosingState == DosingState::IDLE && queueIsEmpty(refillQueue)) {
      Serial.println("none");
    }
    else if (currentTankId == 0) {
      Serial.println("none");
    }
    else {
      Serial.print("Tank "); Serial.println(currentTankId);
    }

    // Status
    Serial.println("+--------------------------------------------------+");
    Serial.print("| [STATUS] ");
    Serial.println(anyFault ? "FAULT" : "OK");
    Serial.println("+--------------------------------------------------+");

    updateRGB(currentSeverity);
    // updateBuzzer already called above
    updateDisplay(currentSeverity, height_m, volume_L, waterFault, chemRemaining_L, chemLow, mismatch, dosingState, waterDispensed_L, chemDispensed_L,
      DISPENSE_TARGET_L, chemCycleActualTarget_L,
      queueSize(refillQueue), currentTankId,
      dosingFault);
  }
}