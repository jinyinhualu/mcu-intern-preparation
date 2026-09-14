#include "ring_buffer.h"

static size_t next_index(size_t index)
{
    index++;
    if (index == RING_BUFFER_CAPACITY)
    {
        index = 0u;
    }

    return index;
}

void ring_buffer_init(RingBuffer *buffer)
{
    if (buffer == NULL)
    {
        return;
    }

    buffer->head = 0u;
    buffer->tail = 0u;
    buffer->count = 0u;
}

bool ring_buffer_is_empty(const RingBuffer *buffer)
{
    return buffer == NULL || buffer->count == 0u;
}

bool ring_buffer_is_full(const RingBuffer *buffer)
{
    return buffer != NULL && buffer->count == RING_BUFFER_CAPACITY;
}

size_t ring_buffer_size(const RingBuffer *buffer)
{
    return buffer == NULL ? 0u : buffer->count;
}

bool ring_buffer_push(RingBuffer *buffer, uint8_t value)
{
    if (buffer == NULL || ring_buffer_is_full(buffer))
    {
        return false;
    }

    buffer->data[buffer->head] = value;
    buffer->head = next_index(buffer->head);
    buffer->count++;
    return true;
}

bool ring_buffer_pop(RingBuffer *buffer, uint8_t *value)
{
    if (buffer == NULL || value == NULL || ring_buffer_is_empty(buffer))
    {
        return false;
    }

    *value = buffer->data[buffer->tail];
    buffer->tail = next_index(buffer->tail);
    buffer->count--;
    return true;
}
