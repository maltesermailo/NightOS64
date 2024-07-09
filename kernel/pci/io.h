//
// Created by Jannik on 07.07.2024.
//

#ifndef NIGHTOS_IO_H
#define NIGHTOS_IO_H

#include <stdint.h>
#include <stdlib.h>
#include "../fs/vfs.h"

typedef struct IORequest {
    enum io_type {IO_READ, IO_WRITE, SATA_COMMAND} type;
    void *buffer; //DMA buffer for read, Buffer for write and Command Data for SATA_COMMAND
    size_t count; //Size of buffer
    size_t offset;
    file_handle_t *file;
    int status;
    void *private_data;
} io_request_t;

#endif //NIGHTOS_IO_H
