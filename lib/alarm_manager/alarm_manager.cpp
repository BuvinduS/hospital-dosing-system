// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Alarm manager — implementation
// ============================================================
#include "alarm_manager.h"

AlarmSeverity evaluateSeverity(
    bool waterFault,
    bool waterLow,
    bool chemLow,
    bool mismatch,
    bool dosingFault,
    bool dosingActive
) {
    // Critical — hard faults that stop operation
    if (waterFault || dosingFault) return AlarmSeverity::CRITICAL;

    // Warning — degraded but operational
    if (waterLow || chemLow || mismatch) return AlarmSeverity::WARNING;

    // Info — normal dosing in progress
    if (dosingActive) return AlarmSeverity::INFO;

    // All clear
    return AlarmSeverity::NONE;
}

bool shouldBeepNow(AlarmSeverity severity, unsigned long now_ms, unsigned long lastBeep_ms, bool buzzerOn) {
    if (severity == AlarmSeverity::NONE || severity == AlarmSeverity::INFO) return false;

    unsigned long interval = beepInterval(severity);
    unsigned long elapsed = now_ms - lastBeep_ms;

    if (!buzzerOn) {
        // Buzzer is off — turn on if interval has elapsed
        return elapsed >= interval;
    }
    else {
        // Buzzer is on — keep on for BEEP_DURATION then off
        // This is handled in main.cpp with the duration check
        return false;
    }
}

unsigned long beepInterval(AlarmSeverity severity) {
    switch (severity) {
    case AlarmSeverity::WARNING:  return 3000UL;
    case AlarmSeverity::CRITICAL: return 300UL;
    default:                      return 0UL;
    }
}

const char* severityToString(AlarmSeverity severity) {
    switch (severity) {
    case AlarmSeverity::NONE:     return "OK";
    case AlarmSeverity::INFO:     return "DOSING";
    case AlarmSeverity::WARNING:  return "WARNING";
    case AlarmSeverity::CRITICAL: return "CRITICAL";
    default:                      return "UNKNOWN";
    }
}