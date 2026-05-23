// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Solenoid valve and water dosing logic — implementation
// ============================================================
#include "solenoid.h"

bool isTargetReached(float dispensed_L, float target_L) {
    return dispensed_L >= target_L;
}

bool isDosingTimeout(unsigned long elapsed_ms, unsigned long timeout_ms) {
    return elapsed_ms >= timeout_ms;
}

bool isBlockedValve(float dispensed_L, unsigned long elapsed_ms,
    unsigned long grace_ms) {
    // If grace period has elapsed and no flow has been measured — blocked
    if (elapsed_ms < grace_ms) return false;  // still within grace period
    return dispensed_L <= 0.0f;
}

bool isStuckOpen(float flow_since_close_L, float tolerance_L) {
    return flow_since_close_L > tolerance_L;
}

bool isSufficientWater(float current_level_m, float min_level_m) {
    return current_level_m > min_level_m;
}

const char* faultToString(DosingFault fault) {
    switch (fault) {
    case DosingFault::NONE:          return "NONE";
    case DosingFault::DRY_TANK:      return "DRY TANK";
    case DosingFault::BLOCKED_VALVE: return "BLOCKED VALVE";
    case DosingFault::STUCK_VALVE:   return "STUCK VALVE";
    case DosingFault::TIMEOUT:       return "TIMEOUT";
    default:                         return "UNKNOWN";
    }
}

const char* stateToString(DosingState state) {
    switch (state) {
    case DosingState::IDLE:     return "IDLE";
    case DosingState::DOSING:   return "DOSING";
    case DosingState::CLOSING:  return "CLOSING";
    case DosingState::COMPLETE: return "COMPLETE";
    case DosingState::FAULT:    return "FAULT";
    default:                    return "UNKNOWN";
    }
}