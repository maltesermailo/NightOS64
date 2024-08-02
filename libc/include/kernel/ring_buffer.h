//
// Created by Jannik on 27.07.2024.
//

#ifndef NIGHTOS_RING_BUFFER_H
#define NIGHTOS_RING_BUFFER_H
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "mutex.h"
typedef atomic_flag spin_t;

typedef struct circular_buffer {
    uint8_t* buffer;
    uint64_t head;
    uint64_t tail;
    uint64_t max;

    spin_t lock;
    mutex_t wait_queue_write;
    mutex_t wait_queue_read;
} circular_buffer_t;

/***
 * Creates a ring buffer with the specified buffer size
 * @param size the size of the buffer
 * @return a new ring buffer
 */
circular_buffer_t* ring_buffer_create(int size);
/***
 * Deletes the ring buffer, freeing its internal buffer
 * @param circularBuffer the ring buffer
 */
void ring_buffer_destroy(circular_buffer_t* circularBuffer);

/**
 * Writes to the ring buffer, discarding everything that would overflow the buffer
 * @param circularBuffer the ring buffer
 * @param size size of data
 * @param data the data
 * @return amount of data written
 */
int ring_buffer_write(circular_buffer_t* circularBuffer, int size, uint8_t* data);

/**
 * Read from the ring buffer, increasing the head
 * @param circularBuffer the ring buffer
 * @param size the size to read
 * @param buffer the buffer to read to
 * @return amount of data read
 */
int ring_buffer_read(circular_buffer_t* circularBuffer, int size, uint8_t* buffer);

/**
 * Available space in the ring buffer before overflowing
 * @param buffer the buffer
 * @return the amount left
 */
int ring_buffer_available(circular_buffer_t* buffer);

void ring_buffer_discard_readable(circular_buffer_t* buffer);

#endif //NIGHTOS_RING_BUFFER_H
