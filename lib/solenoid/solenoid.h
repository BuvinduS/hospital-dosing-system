// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Solenoid valve and water dosing logic — declarations
//  No hardware dependencies — safe to include in unit tests
// ============================================================
#pragma once

// ── Dosing state machine ──────────────────────────────────────
enum class DosingState {
    IDLE,
    DOSING,
    TOPPING_UP,  // water done but chem still needs more — topping up
    CLOSING,    // valve just closed — verifying flow has stopped
    COMPLETE,
    FAULT
};

// ── Fault codes ───────────────────────────────────────────────
enum class DosingFault {
    NONE,
    DRY_TANK,         // water level too low before dosing started
    BLOCKED_VALVE,    // no flow detected within grace period after open
    STUCK_VALVE,      // flow continues after close command
    PUMP_FAILURE,
    TIMEOUT           // target not reached within allowed time
};

// ── Logic functions ───────────────────────────────────────────

// Returns true when dispensed volume meets or exceeds target
bool isTargetReached(float dispensed_L, float target_L);

// Returns true when elapsed time exceeds the allowed dosing window
bool isDosingTimeout(unsigned long elapsed_ms, unsigned long timeout_ms);

// Returns true when no flow detected within grace period after open
// Indicates a blocked valve or empty supply
bool isBlockedValve(float dispensed_L, unsigned long elapsed_ms,
    unsigned long grace_ms);

// Returns true when flow continues after close command
// Indicates a stuck-open valve
bool isStuckOpen(float flow_since_close_L, float tolerance_L);

// Returns true when water level is sufficient to begin dosing
bool isSufficientWater(float current_level_m, float min_level_m);

// Returns a human-readable fault description
const char* faultToString(DosingFault fault);

// Returns a human-readable state description
const char* stateToString(DosingState state);