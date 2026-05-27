// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Alarm manager — declarations
//  No hardware dependencies — safe to include in unit tests
// ============================================================
#pragma once

enum class AlarmSeverity {
    NONE,      // system OK — silent, green LED
    INFO,      // dosing active — silent, blue LED
    WARNING,   // slow beep — amber LED
    CRITICAL   // rapid beep — red LED, OLED fault message
};

// Returns the highest severity from individual fault flags
AlarmSeverity evaluateSeverity(
    bool waterFault,
    bool waterLow,
    bool chemLow,
    bool mismatch,
    bool dosingFault,
    bool dosingActive
);

// Returns true if it is time to toggle the buzzer on
// based on severity and elapsed time
bool shouldBeepNow(AlarmSeverity severity,
    unsigned long now_ms,
    unsigned long lastBeep_ms,
    bool buzzerOn);

// Returns the beep interval in ms for a given severity
unsigned long beepInterval(AlarmSeverity severity);

// Returns human-readable severity string
const char* severityToString(AlarmSeverity severity);