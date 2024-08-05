//
// Created by Jannik on 28.07.2024.
//
#include "../include/kernel/ring_buffer.h"
#include "../../kernel/proc/process.h"
#include "../../mlibc/abis/linux/errno.h"
#include <stdlib.h>

circular_buffer_t* ring_buffer_create(int size) {
    circular_buffer_t* buffer = calloc(1, sizeof(circular_buffer_t));
    buffer->buffer = calloc(1, size);
    buffer->max = size;
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

        if(cb->tail > cb->max) {
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
    while(written == 0) {
        spin_lock(&cb->lock);

        while(ring_buffer_available(cb) > 0 && written < size) {
            buffer[written] = cb->buffer[cb->tail];
            cb->tail = (cb->tail + 1) % cb->max;
            written++;
        }

        if(written == 0) {
            spin_unlock(&cb->lock);
            //This mutex is nothing more than a simple wait queue.
            //If the mutex isnt acquired, it acquires it but doesnt block.
            //If it is acquired, it wont block, but will block on mutex_wait
            mutex_acquire_if_free(&cb->wait_queue_write);
            mutex_wait(&cb->wait_queue_write);
            spin_lock(&cb->lock);

            return -ERESTART; //RESTART SYSCALL
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
        //Readable part is in front of writeable

        return (buffer->max - buffer->head) + buffer->tail - 1;
    } else {
        //Readable part is in front of writeable
        //Basically since
        //|      TAIL      HEAD   |
        //We're removing this part
        //|          XXXXXXHEAD   |

        return buffer->tail - buffer->head - 1;
    }
}