#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RING_BUFFER_CAPACITY 8u

typedef struct
{
    uint8_t data[RING_BUFFER_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
} RingBuffer;

void ring_buffer_init(RingBuffer *buffer);
bool ring_buffer_is_empty(const RingBuffer *buffer);
bool ring_buffer_is_full(const RingBuffer *buffer);
size_t ring_buffer_size(const RingBuffer *buffer);
bool ring_buffer_push(RingBuffer *buffer, uint8_t value);
bool ring_buffer_pop(RingBuffer *buffer, uint8_t *value);

#endif
