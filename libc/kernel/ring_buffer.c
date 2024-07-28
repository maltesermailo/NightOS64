//
// Created by Jannik on 28.07.2024.
//
#include "../include/kernel/ring_buffer.h"
#include <stdlib.h>

circular_buffer_t* ring_buffer_create(int size) {
    circular_buffer_t* buffer = calloc(1, sizeof(circular_buffer_t));
    buffer->buffer = calloc(1, size);
    buffer->max = size;

    return buffer;
}

void ring_buffer_destroy(circular_buffer_t* buffer) {
    free(buffer->buffer);
    free(buffer);
}

int ring_buffer_write(circular_buffer_t* circularBuffer, int size, uint8_t* data) {
    return 0;
}

int ring_buffer_read(circular_buffer_t* circularBuffer, int size, uint8_t* buffer) {
    return 0;
}

int ring_buffer_available(circular_buffer_t* buffer) {
    return 0;
}

int ring_buffer_writeable(circular_buffer_t* buffer) {
    if(buffer->head == buffer->tail) {
        return buffer->max;
    }

}