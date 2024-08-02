//
// Created by Jannik on 02.08.2024.
//

#ifndef NIGHTOS_MESSAGE_H
#define NIGHTOS_MESSAGE_H

#include "process.h"
#include "../../libc/include/kernel/ring_buffer.h"

typedef struct Message {
    pid_t sender;
    pid_t receiver;

    unsigned int type;
    unsigned int size;
    void* data;
} message_t;

typedef struct MessageQueue {
    circular_buffer_t* buffer;

    unsigned int maxMessages; //Count of messages, defines size of buffer
    unsigned int messageSize; //Message size
} message_queue_t;

#endif //NIGHTOS_MESSAGE_H
