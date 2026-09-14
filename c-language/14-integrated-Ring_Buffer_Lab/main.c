#include "ring_buffer.h"

#include <stdio.h>

static unsigned int g_failed_tests;

static void check_true(const char *name, bool actual)
{
    printf("%-42s %s\n", name, actual ? "PASS" : "FAIL");
    if (!actual)
    {
        g_failed_tests++;
    }
}

static void check_size(const char *name, size_t actual, size_t expected)
{
    bool passed = actual == expected;
    printf("%-42s %s (actual=%zu expected=%zu)\n",
           name,
           passed ? "PASS" : "FAIL",
           actual,
           expected);
    if (!passed)
    {
        g_failed_tests++;
    }
}

static void check_byte(const char *name, uint8_t actual, uint8_t expected)
{
    bool passed = actual == expected;
    printf("%-42s %s (actual=%u expected=%u)\n",
           name,
           passed ? "PASS" : "FAIL",
           (unsigned int)actual,
           (unsigned int)expected);
    if (!passed)
    {
        g_failed_tests++;
    }
}

static void test_empty_and_null_inputs(void)
{
    RingBuffer buffer;
    uint8_t value = 0u;

    ring_buffer_init(&buffer);
    check_true("new buffer is empty", ring_buffer_is_empty(&buffer));
    check_true("new buffer is not full", !ring_buffer_is_full(&buffer));
    check_size("new buffer size is zero", ring_buffer_size(&buffer), 0u);
    check_true("pop from empty buffer fails", !ring_buffer_pop(&buffer, &value));
    check_true("push with NULL buffer fails", !ring_buffer_push(NULL, 1u));
    check_true("pop with NULL buffer fails", !ring_buffer_pop(NULL, &value));
    check_true("pop with NULL output fails", !ring_buffer_pop(&buffer, NULL));
    check_true("NULL buffer is treated as empty", ring_buffer_is_empty(NULL));
    check_true("NULL buffer is not full", !ring_buffer_is_full(NULL));
    check_size("NULL buffer size is zero", ring_buffer_size(NULL), 0u);
}

static void test_fifo_and_full_boundary(void)
{
    RingBuffer buffer;
    uint8_t value = 0u;
    size_t i;

    ring_buffer_init(&buffer);
    for (i = 0u; i < RING_BUFFER_CAPACITY; i++)
    {
        check_true("push into available slot succeeds", ring_buffer_push(&buffer, (uint8_t)(i + 1u)));
    }

    check_true("buffer reports full at capacity", ring_buffer_is_full(&buffer));
    check_size("full buffer size equals capacity",
               ring_buffer_size(&buffer),
               RING_BUFFER_CAPACITY);
    check_true("push into full buffer fails", !ring_buffer_push(&buffer, 99u));

    for (i = 0u; i < RING_BUFFER_CAPACITY; i++)
    {
        bool popped = ring_buffer_pop(&buffer, &value);
        check_true("pop from full buffer succeeds", popped);
        if (popped)
        {
            check_byte("FIFO order is preserved", value, (uint8_t)(i + 1u));
        }
    }

    check_true("buffer is empty after all pops", ring_buffer_is_empty(&buffer));
}

static void test_wrap_around(void)
{
    RingBuffer buffer;
    uint8_t value = 0u;
    size_t i;

    ring_buffer_init(&buffer);
    for (i = 0u; i < 5u; i++)
    {
        check_true("seed push succeeds", ring_buffer_push(&buffer, (uint8_t)(10u + i)));
    }
    for (i = 0u; i < 3u; i++)
    {
        check_true("seed pop succeeds", ring_buffer_pop(&buffer, &value));
        check_byte("seed values are FIFO", value, (uint8_t)(10u + i));
    }
    for (i = 0u; i < 6u; i++)
    {
        check_true("wrap-around push succeeds", ring_buffer_push(&buffer, (uint8_t)(20u + i)));
    }

    check_true("buffer is full after wrapped writes", ring_buffer_is_full(&buffer));
    for (i = 3u; i < 5u; i++)
    {
        check_true("remaining old values stay first", ring_buffer_pop(&buffer, &value));
        check_byte("old values remain in order", value, (uint8_t)(10u + i));
    }
    for (i = 0u; i < 6u; i++)
    {
        check_true("wrapped values pop successfully", ring_buffer_pop(&buffer, &value));
        check_byte("wrapped values remain in order", value, (uint8_t)(20u + i));
    }
    check_true("wrapped buffer is empty", ring_buffer_is_empty(&buffer));
}

int main(void)
{
    printf("Fixed-length ring buffer capacity: %u\n", RING_BUFFER_CAPACITY);
    test_empty_and_null_inputs();
    test_fifo_and_full_boundary();
    test_wrap_around();

    if (g_failed_tests == 0u)
    {
        printf("ALL TESTS PASSED\n");
        return 0;
    }

    printf("FAILED TESTS: %u\n", g_failed_tests);
    return 1;
}
