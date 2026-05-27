// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Refill queue — declarations
//  No hardware dependencies — safe to include in unit tests
//
//  FIFO queue for dispensing tank refill requests.
//  Duplicate tank IDs are rejected.
//  Maximum capacity: QUEUE_MAX_SIZE tanks.
// ============================================================
#pragma once

#include <stdint.h>

static constexpr uint8_t QUEUE_MAX_SIZE = 20;

// Queue entry — holds a tank ID (1-based, 1 to QUEUE_MAX_SIZE)
struct RefillQueue {
    uint8_t  items[QUEUE_MAX_SIZE];
    uint8_t  head;   // index of next item to dequeue
    uint8_t  tail;   // index of next empty slot
    uint8_t  count;  // number of items currently in queue
};

// ── Queue lifecycle ───────────────────────────────────────────

// Initialise queue to empty state — call before first use
void queueInit(RefillQueue& q);

// ── Queue operations ──────────────────────────────────────────

// Add a tank ID to the back of the queue
// Returns true if added, false if full or duplicate
bool enqueue(RefillQueue& q, uint8_t tankId);

// Remove and return the front tank ID
// Returns 0 if queue is empty (0 is not a valid tank ID)
uint8_t dequeue(RefillQueue& q);

// Return the front tank ID without removing it
// Returns 0 if queue is empty
uint8_t peek(const RefillQueue& q);

// ── Queue state ───────────────────────────────────────────────

// Returns true if queue has no items
bool queueIsEmpty(const RefillQueue& q);

// Returns true if queue has reached maximum capacity
bool queueIsFull(const RefillQueue& q);

// Returns number of items currently in queue
uint8_t queueSize(const RefillQueue& q);

// Returns true if tankId is already present in queue
bool queueContains(const RefillQueue& q, uint8_t tankId);