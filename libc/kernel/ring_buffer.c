//
// Created by Jannik on 28.07.2024.
//
#include "../include/kernel/ring_buffer.h"
#include "../../kernel/proc/process.h"
#include "../../mlibc/abis/linux/errno.h"
#include <stdlib.h>
#include <stdio.h>

circular_buffer_t* ring_buffer_create(int size, bool blockingWrite) {
    circular_buffer_t* buffer = calloc(1, sizeof(circular_buffer_t));
    buffer->buffer = calloc(1, size);
    buffer->max = size;
    buffer->blockingWrite = blockingWrite;
    mutex_init(&buffer->wait_queue_read);
    mutex_init(&buffer->wait_queue_write);

    return buffer;
}

void ring_buffer_destroy(circular_buffer_t* buffer) {
    free(buffer->buffer);
    free(buffer);
}

int ring_buffer_writeable(circular_buffer_t* buffer) {
    if(buffer->head == buffer->tail) {
        return buffer->max - 1;
    }

    if(buffer->tail < buffer->head) {
        //Readable part is in front of writeable, therefore we can write till the end of readable part

        return buffer->head - buffer->tail - 1;
    } else {
        //Writeable part is in front of readable
        //Basically since
        //|      HEAD      TAIL   |
        //We're removing this part
        //|                    XXX|
        //Then we add everything before our head as the last remaining
        //This gives us a cool loop to work with

        return (buffer->max - buffer->tail) + buffer->head - 1;
    }
}

int ring_buffer_write(circular_buffer_t* cb, int size, uint8_t* data) {
    while(ring_buffer_writeable(cb) < size) {
        if(!cb->blockingWrite) {
            return 0;
        }

        //This mutex is nothing more than a simple wait queue.
        //If the mutex isnt acquired, it acquires it but doesnt block.
        //If it is acquired, it wont block, but will block on mutex_wait
        mutex_acquire_if_free(&cb->wait_queue_write);
        mutex_wait(&cb->wait_queue_write);
    }

    size_t written = 0;
    while(written < size) {
        spin_lock(&cb->lock);

        cb->buffer[cb->tail] = data[written];
        cb->tail++;

        if(cb->tail >= cb->max) {
            cb->tail = 0;
        }

        written++;
        spin_unlock(&cb->lock);
    }

    //Finished with writing, wake up sleepers on read
    if(mutex_acquire_if_free(&cb->wait_queue_read)) {
        return written;
    }

    mutex_release(&cb->wait_queue_read);

    return written;
}

int ring_buffer_read(circular_buffer_t* cb, int size, uint8_t* buffer) {
    size_t written = 0;
    printf("Awaiting size %d\n", size);
    while(written == 0) {
        spin_lock(&cb->lock);

        while(ring_buffer_available(cb) > 0 && written < size) {
            buffer[written] = cb->buffer[cb->head];
            cb->head = (cb->head + 1) % cb->max;
            written++;
        }

        if(written == 0) {
            spin_unlock(&cb->lock);
            //This mutex is nothing more than a simple wait queue.
            //If the mutex isnt acquired, it acquires it but doesnt block.
            //If it is acquired, it wont block, but will block on mutex_wait
            mutex_acquire_if_free(&cb->wait_queue_read);
            mutex_wait(&cb->wait_queue_read);
            spin_lock(&cb->lock);

            printf("Awaiting size %d\n", size);
            printf("available %d\n", ring_buffer_available(cb));

            while(ring_buffer_available(cb) > 0 && written < size) {
                buffer[written] = cb->buffer[cb->head];
                cb->head = (cb->head + 1) % cb->max;

                printf("Read one\n");

                written++;
            }

            if(written == 0) {
                spin_unlock(&cb->lock);
                return written;
            }
        }

        spin_unlock(&cb->lock);
    }

    //Finished with reading, wake up sleepers on write
    if(mutex_acquire_if_free(&cb->wait_queue_write)) {
        return written;
    }

    mutex_release(&cb->wait_queue_write);

    return written;
}

int ring_buffer_available(circular_buffer_t* buffer) {
    if(buffer->head == buffer->tail) {
        return 0;
    }

    if(buffer->tail < buffer->head) {
        //Writeable part is in front of readable

        return (buffer->max - buffer->head) + buffer->tail;
    } else {
        //Readable part is in front of writeable
        //Basically since
        //|      TAIL      HEAD   |
        //We're removing this part
        //|          XXXXXXHEAD   |

        return buffer->tail - buffer->head;
    }
}

int ring_buffer_pop(circular_buffer_t* cb) {
    spin_lock(&cb->lock);

    if (ring_buffer_available(cb) == 0) {
        spin_unlock(&cb->lock);
        return 0;  // Buffer is empty
    }

    // Move the tail back by one
    cb->tail = (cb->tail - 1 + cb->max) % cb->max;

    spin_unlock(&cb->lock);

    // Wake up any waiting writers
    if (!mutex_acquire_if_free(&cb->wait_queue_write)) {
        mutex_release(&cb->wait_queue_write);
    }

    return 1;  // Successfully popped
}

int ring_buffer_peek(circular_buffer_t* cb, int offset, uint8_t* data) {
    spin_lock(&cb->lock);

    if (offset >= ring_buffer_available(cb)) {
        spin_unlock(&cb->lock);
        return 0;  // Not enough data
    }

    int index = (cb->head + offset) % cb->max;
    *data = cb->buffer[index];

    spin_unlock(&cb->lock);
    return 1;  // Successfully peeked
}

int ring_buffer_read_last(circular_buffer_t* cb, uint8_t* data) {
    spin_lock(&cb->lock);

    if (ring_buffer_available(cb) == 0) {
        spin_unlock(&cb->lock);
        return 0;  // Buffer is empty
    }

    int last_index = (cb->tail - 1 + cb->max) % cb->max;
    *data = cb->buffer[last_index];

    spin_unlock(&cb->lock);
    return 1;  // Successfully read last element
}