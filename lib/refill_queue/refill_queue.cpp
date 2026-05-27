// ============================================================
//  IoT Chemical Dosing System — Hospital
//  Refill queue — implementation
//
//  Circular FIFO buffer. Duplicates rejected at enqueue.
//  Tank IDs are 1-based (1 to QUEUE_MAX_SIZE).
// ============================================================
#include "refill_queue.h"

void queueInit(RefillQueue& q) {
    q.head = 0;
    q.tail = 0;
    q.count = 0;
    for (uint8_t i = 0; i < QUEUE_MAX_SIZE; i++) {
        q.items[i] = 0;
    }
}

bool enqueue(RefillQueue& q, uint8_t tankId) {
    // Reject invalid tank IDs
    if (tankId == 0 || tankId > QUEUE_MAX_SIZE) return false;

    // Reject if full
    if (queueIsFull(q)) return false;

    // Reject duplicates — tank already waiting
    if (queueContains(q, tankId)) return false;

    q.items[q.tail] = tankId;
    q.tail = (q.tail + 1) % QUEUE_MAX_SIZE;  // circular wrap
    q.count++;
    return true;
}

uint8_t dequeue(RefillQueue& q) {
    if (queueIsEmpty(q)) return 0;

    uint8_t tankId = q.items[q.head];
    q.items[q.head] = 0;
    q.head = (q.head + 1) % QUEUE_MAX_SIZE;  // circular wrap
    q.count--;
    return tankId;
}

uint8_t peek(const RefillQueue& q) {
    if (queueIsEmpty(q)) return 0;
    return q.items[q.head];
}

bool queueIsEmpty(const RefillQueue& q) {
    return q.count == 0;
}

bool queueIsFull(const RefillQueue& q) {
    return q.count >= QUEUE_MAX_SIZE;
}

uint8_t queueSize(const RefillQueue& q) {
    return q.count;
}

bool queueContains(const RefillQueue& q, uint8_t tankId) {
    for (uint8_t i = 0; i < q.count; i++) {
        uint8_t idx = (q.head + i) % QUEUE_MAX_SIZE;
        if (q.items[idx] == tankId) return true;
    }
    return false;
}