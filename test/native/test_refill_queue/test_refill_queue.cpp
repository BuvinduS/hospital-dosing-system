// ============================================================
//  Unit tests — Refill queue
//  Phase 3a
//
//  Tests: queueInit(), enqueue(), dequeue(), peek(),
//         queueIsEmpty(), queueIsFull(), queueSize(),
//         queueContains()
//  Run with: pio test -e native
// ============================================================
#include <unity.h>
#include "refill_queue.h"

static RefillQueue q;

// ============================================================
//  Helpers
// ============================================================
void fillQueue() {
    // Fill queue to capacity with tank IDs 1..QUEUE_MAX_SIZE
    for (uint8_t i = 1; i <= QUEUE_MAX_SIZE; i++) {
        enqueue(q, i);
    }
}

// ============================================================
//  queueInit()
// ============================================================

void test_init_queue_is_empty() {
    queueInit(q);
    TEST_ASSERT_TRUE(queueIsEmpty(q));
}

void test_init_size_is_zero() {
    queueInit(q);
    TEST_ASSERT_EQUAL_UINT8(0, queueSize(q));
}

void test_init_peek_returns_zero() {
    queueInit(q);
    TEST_ASSERT_EQUAL_UINT8(0, peek(q));
}

void test_init_dequeue_returns_zero() {
    queueInit(q);
    TEST_ASSERT_EQUAL_UINT8(0, dequeue(q));
}

// ============================================================
//  enqueue()
// ============================================================

void test_enqueue_single_item_succeeds() {
    queueInit(q);
    TEST_ASSERT_TRUE(enqueue(q, 1));
}

void test_enqueue_increases_size() {
    queueInit(q);
    enqueue(q, 1);
    TEST_ASSERT_EQUAL_UINT8(1, queueSize(q));
}

void test_enqueue_two_items_size_two() {
    queueInit(q);
    enqueue(q, 1);
    enqueue(q, 2);
    TEST_ASSERT_EQUAL_UINT8(2, queueSize(q));
}

void test_enqueue_duplicate_rejected() {
    queueInit(q);
    enqueue(q, 1);
    TEST_ASSERT_FALSE(enqueue(q, 1));
}

void test_enqueue_duplicate_does_not_increase_size() {
    queueInit(q);
    enqueue(q, 1);
    enqueue(q, 1);
    TEST_ASSERT_EQUAL_UINT8(1, queueSize(q));
}

void test_enqueue_invalid_id_zero_rejected() {
    queueInit(q);
    TEST_ASSERT_FALSE(enqueue(q, 0));
}

void test_enqueue_invalid_id_over_max_rejected() {
    queueInit(q);
    TEST_ASSERT_FALSE(enqueue(q, QUEUE_MAX_SIZE + 1));
}

void test_enqueue_when_full_rejected() {
    queueInit(q);
    fillQueue();
    TEST_ASSERT_FALSE(enqueue(q, QUEUE_MAX_SIZE));
}

void test_enqueue_all_valid_ids_succeeds() {
    queueInit(q);
    for (uint8_t i = 1; i <= QUEUE_MAX_SIZE; i++) {
        TEST_ASSERT_TRUE(enqueue(q, i));
    }
}

// ============================================================
//  dequeue()
// ============================================================

void test_dequeue_returns_correct_item() {
    queueInit(q);
    enqueue(q, 3);
    TEST_ASSERT_EQUAL_UINT8(3, dequeue(q));
}

void test_dequeue_fifo_order() {
    queueInit(q);
    enqueue(q, 1);
    enqueue(q, 2);
    enqueue(q, 3);
    TEST_ASSERT_EQUAL_UINT8(1, dequeue(q));
    TEST_ASSERT_EQUAL_UINT8(2, dequeue(q));
    TEST_ASSERT_EQUAL_UINT8(3, dequeue(q));
}

void test_dequeue_reduces_size() {
    queueInit(q);
    enqueue(q, 1);
    enqueue(q, 2);
    dequeue(q);
    TEST_ASSERT_EQUAL_UINT8(1, queueSize(q));
}

void test_dequeue_empty_returns_zero() {
    queueInit(q);
    TEST_ASSERT_EQUAL_UINT8(0, dequeue(q));
}

void test_dequeue_all_items_queue_empty() {
    queueInit(q);
    enqueue(q, 1);
    enqueue(q, 2);
    dequeue(q);
    dequeue(q);
    TEST_ASSERT_TRUE(queueIsEmpty(q));
}

void test_dequeue_allows_reenqueue_of_same_id() {
    // Once dequeued, the same tank ID can be enqueued again
    queueInit(q);
    enqueue(q, 1);
    dequeue(q);
    TEST_ASSERT_TRUE(enqueue(q, 1));
}

// ============================================================
//  peek()
// ============================================================

void test_peek_returns_front_item() {
    queueInit(q);
    enqueue(q, 5);
    TEST_ASSERT_EQUAL_UINT8(5, peek(q));
}

void test_peek_does_not_remove_item() {
    queueInit(q);
    enqueue(q, 5);
    peek(q);
    TEST_ASSERT_EQUAL_UINT8(1, queueSize(q));
}

void test_peek_empty_returns_zero() {
    queueInit(q);
    TEST_ASSERT_EQUAL_UINT8(0, peek(q));
}

void test_peek_returns_first_not_second() {
    queueInit(q);
    enqueue(q, 2);
    enqueue(q, 7);
    TEST_ASSERT_EQUAL_UINT8(2, peek(q));
}

// ============================================================
//  queueIsEmpty() and queueIsFull()
// ============================================================

void test_empty_after_init() {
    queueInit(q);
    TEST_ASSERT_TRUE(queueIsEmpty(q));
}

void test_not_empty_after_enqueue() {
    queueInit(q);
    enqueue(q, 1);
    TEST_ASSERT_FALSE(queueIsEmpty(q));
}

void test_empty_after_enqueue_and_dequeue() {
    queueInit(q);
    enqueue(q, 1);
    dequeue(q);
    TEST_ASSERT_TRUE(queueIsEmpty(q));
}

void test_not_full_after_init() {
    queueInit(q);
    TEST_ASSERT_FALSE(queueIsFull(q));
}

void test_full_after_max_enqueues() {
    queueInit(q);
    fillQueue();
    TEST_ASSERT_TRUE(queueIsFull(q));
}

void test_not_full_after_one_dequeue_from_full() {
    queueInit(q);
    fillQueue();
    dequeue(q);
    TEST_ASSERT_FALSE(queueIsFull(q));
}

// ============================================================
//  queueContains()
// ============================================================

void test_contains_item_in_queue() {
    queueInit(q);
    enqueue(q, 4);
    TEST_ASSERT_TRUE(queueContains(q, 4));
}

void test_not_contains_item_not_in_queue() {
    queueInit(q);
    enqueue(q, 4);
    TEST_ASSERT_FALSE(queueContains(q, 5));
}

void test_not_contains_after_dequeue() {
    queueInit(q);
    enqueue(q, 4);
    dequeue(q);
    TEST_ASSERT_FALSE(queueContains(q, 4));
}

void test_contains_works_with_multiple_items() {
    queueInit(q);
    enqueue(q, 1);
    enqueue(q, 2);
    enqueue(q, 3);
    TEST_ASSERT_TRUE(queueContains(q, 2));
    TEST_ASSERT_FALSE(queueContains(q, 4));
}

void test_not_contains_on_empty_queue() {
    queueInit(q);
    TEST_ASSERT_FALSE(queueContains(q, 1));
}

// ============================================================
//  Integration — FIFO ordering and duplicate handling
// ============================================================

void test_fifo_two_tanks_request_simultaneously() {
    // Tank 2 requests first, then Tank 1 — Tank 2 served first
    queueInit(q);
    enqueue(q, 2);
    enqueue(q, 1);
    TEST_ASSERT_EQUAL_UINT8(2, dequeue(q));
    TEST_ASSERT_EQUAL_UINT8(1, dequeue(q));
}

void test_duplicate_request_while_serving() {
    // Tank 1 being served (dequeued), requests again — should be
    // accepted back into queue since it was removed
    queueInit(q);
    enqueue(q, 1);
    enqueue(q, 2);
    dequeue(q);              // Tank 1 now being served
    TEST_ASSERT_TRUE(enqueue(q, 1));  // Tank 1 low again — re-enqueue
    TEST_ASSERT_EQUAL_UINT8(2, dequeue(q));  // Tank 2 served next
    TEST_ASSERT_EQUAL_UINT8(1, dequeue(q));  // Tank 1 served after
}

void test_queue_wraps_around_correctly() {
    // Fill, empty halfway, fill again — tests circular buffer wrap
    queueInit(q);
    for (uint8_t i = 1; i <= 10; i++) enqueue(q, i);
    for (uint8_t i = 1; i <= 10; i++) dequeue(q);
    // Queue now empty, head and tail have wrapped
    for (uint8_t i = 1; i <= 10; i++) enqueue(q, i);
    TEST_ASSERT_EQUAL_UINT8(10, queueSize(q));
    TEST_ASSERT_EQUAL_UINT8(1, peek(q));
}

void test_sequential_service_three_tanks() {
    // Tanks 3, 1, 2 request in that order — served in that order
    queueInit(q);
    enqueue(q, 3);
    enqueue(q, 1);
    enqueue(q, 2);
    TEST_ASSERT_EQUAL_UINT8(3, dequeue(q));
    TEST_ASSERT_EQUAL_UINT8(1, dequeue(q));
    TEST_ASSERT_EQUAL_UINT8(2, dequeue(q));
    TEST_ASSERT_TRUE(queueIsEmpty(q));
}

void test_full_queue_rejects_new_request() {
    // All 20 slots taken — new request rejected
    queueInit(q);
    fillQueue();
    TEST_ASSERT_FALSE(enqueue(q, 1));  // already in queue — duplicate
}

// ============================================================
//  Runner
// ============================================================

void setUp() { queueInit(q); }  // fresh queue before each test
void tearDown() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // queueInit
    RUN_TEST(test_init_queue_is_empty);
    RUN_TEST(test_init_size_is_zero);
    RUN_TEST(test_init_peek_returns_zero);
    RUN_TEST(test_init_dequeue_returns_zero);

    // enqueue
    RUN_TEST(test_enqueue_single_item_succeeds);
    RUN_TEST(test_enqueue_increases_size);
    RUN_TEST(test_enqueue_two_items_size_two);
    RUN_TEST(test_enqueue_duplicate_rejected);
    RUN_TEST(test_enqueue_duplicate_does_not_increase_size);
    RUN_TEST(test_enqueue_invalid_id_zero_rejected);
    RUN_TEST(test_enqueue_invalid_id_over_max_rejected);
    RUN_TEST(test_enqueue_when_full_rejected);
    RUN_TEST(test_enqueue_all_valid_ids_succeeds);

    // dequeue
    RUN_TEST(test_dequeue_returns_correct_item);
    RUN_TEST(test_dequeue_fifo_order);
    RUN_TEST(test_dequeue_reduces_size);
    RUN_TEST(test_dequeue_empty_returns_zero);
    RUN_TEST(test_dequeue_all_items_queue_empty);
    RUN_TEST(test_dequeue_allows_reenqueue_of_same_id);

    // peek
    RUN_TEST(test_peek_returns_front_item);
    RUN_TEST(test_peek_does_not_remove_item);
    RUN_TEST(test_peek_empty_returns_zero);
    RUN_TEST(test_peek_returns_first_not_second);

    // queueIsEmpty and queueIsFull
    RUN_TEST(test_empty_after_init);
    RUN_TEST(test_not_empty_after_enqueue);
    RUN_TEST(test_empty_after_enqueue_and_dequeue);
    RUN_TEST(test_not_full_after_init);
    RUN_TEST(test_full_after_max_enqueues);
    RUN_TEST(test_not_full_after_one_dequeue_from_full);

    // queueContains
    RUN_TEST(test_contains_item_in_queue);
    RUN_TEST(test_not_contains_item_not_in_queue);
    RUN_TEST(test_not_contains_after_dequeue);
    RUN_TEST(test_contains_works_with_multiple_items);
    RUN_TEST(test_not_contains_on_empty_queue);

    // integration
    RUN_TEST(test_fifo_two_tanks_request_simultaneously);
    RUN_TEST(test_duplicate_request_while_serving);
    RUN_TEST(test_queue_wraps_around_correctly);
    RUN_TEST(test_sequential_service_three_tanks);
    RUN_TEST(test_full_queue_rejects_new_request);

    return UNITY_END();
}