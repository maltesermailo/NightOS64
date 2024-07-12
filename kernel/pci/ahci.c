//
// Created by Jannik on 29.06.2024.
//
#include "ahci.h"
#include "../memmgr.h"
#include "../timer.h"
#include "../idt.h"
#include "../../libc/include/kernel/list.h"

#include <stdio.h>

list_t hard_drives;
sata_device_t* sataDevice;
HBA_MEM* hba;

void ahci_interrupt_handler(regs_t* regs) {

}

int get_next_free(int portNumber) {
    HBA_PORT* port = &hba->ports[portNumber];

    uint32_t slots = (port->sact | port->ci); //Combined SACT and Command Issued register
    for(int i = 0; i < 32; i++) {
        if((slots & 1) == 0)
            return i;

        slots >>= 1;
    }

    return -1;
}

void ahci_send_command(io_request_t* ioRequest, int ataCommand) {
    HBA_PORT* port = &hba->ports[sataDevice->port];

    int cmdIndex = get_next_free(sataDevice->port);

    if(cmdIndex == -1) {
        return;
    }

    uintptr_t ptrCmdHeader = ((uintptr_t)port->clbu) << 32 | port->clb;
    HBA_CMD_HEADER* cmdHeader = (HBA_CMD_HEADER*)ptrCmdHeader;
    cmdHeader += cmdIndex;

    HBA_CMD_TBL* cmdTable = (HBA_CMD_TBL*)(((uintptr_t)cmdHeader->ctbau) << 32 | cmdHeader->ctba);
    FIS_REG_H2D* commandFIS = (FIS_REG_H2D*)(cmdTable->cfis);

    cmdHeader->cfl = sizeof(FIS_REG_D2H) / sizeof(uint32_t);
    cmdHeader->prdtl = 1;
    cmdHeader->w = ioRequest->type;

    commandFIS->fis_type = FIS_TYPE_REG_H2D; //To Device
    commandFIS->command = ataCommand; //Command
    commandFIS->c = 1; //Command Type
    commandFIS->lba0 = (uint8_t)ioRequest->offset;
    commandFIS->lba1 = (uint8_t)(ioRequest->offset>>8);
    commandFIS->lba2 = (uint8_t)(ioRequest->offset>>16);
    commandFIS->device = 1<<6;  // LBA mode
    commandFIS->lba3 = (uint8_t)(ioRequest->offset>>24);
    commandFIS->lba4 = (uint8_t)(ioRequest->offset>>32);
    commandFIS->lba5 = (uint8_t)(ioRequest->offset>>40);
    commandFIS->countl = ioRequest->count & 0xFF;
    commandFIS->counth = (ioRequest->count >> 8) & 0xFF;

    cmdTable->prdt_entry[0].dba = (uintptr_t)ioRequest->buffer;
    cmdTable->prdt_entry[0].dbau = ((uintptr_t)ioRequest->buffer >> 32);
    cmdTable->prdt_entry[0].dbc = ioRequest->count;

    port->ci |= 1 << cmdIndex;

    while(true) {
        if((port->ci & (1 << cmdIndex)) == 0) {
            break;
        }

        //Command finished, packet in buffer
        return;
    }
}

int ahci_read(file_node_t* node, char* buf, size_t offset, size_t length) {
    struct SATADevice* thisDevice = (struct SATADevice*)node->fs;

    io_request_t* ioRequest = calloc(1, sizeof(io_request_t));
    ioRequest->type = IO_READ;
    ioRequest->count = length;
    ioRequest->offset = offset;
    ioRequest->buffer = calloc(1, length);

    ahci_send_command(ioRequest, ATA_CMD_READ_DMA_EXT);

    memcpy(buf, ioRequest->buffer, length);

    free(ioRequest->buffer);
    free(ioRequest);

    return length;
}

file_node_t* create_ahci_device(struct SATADevice* sataDevice) {
    file_node_t* node = calloc(1, sizeof(file_node_t));
    node->type = FILE_TYPE_BLOCK_DEVICE;
    node->fs = sataDevice;
    node->size = sataDevice->size;
    snprintf(node->name, 16, "hd%d", sataDevice->port);
    node->refcount = 0;
    node->file_ops.read = ahci_read;

    return node;
}

void ahci_setup(void* abar, uint16_t interruptVector) {
    abar = memmgr_map_mmio((uintptr_t)abar, 0x2000, true); //ABAR needs a full page for each port and about 512 bytes for the HBA mem

    HBA_MEM* mem = (HBA_MEM*) abar;
    hba = mem;

    if((mem->cap2 & HBA_MEM_CAP2_BOH) == 1) {
        //BIOS/OS Handoff supported
        mem->bohc |= HBA_MEM_BOHC_OOS;
        //Now BIOS should take over and start the handoff, we wait 25 ms now
        ksleep(25);

        int sleepCount = 0;

        while((mem->bohc & HBA_MEM_BOHC_BB) == HBA_MEM_BOHC_BB) {
            ksleep(2000);
            sleepCount++;

            if(sleepCount > 4) {
                printf("BIOS took longer than 8 seconds to cleanup HBA");
                panic();

                break;
            }
        }
    }

    uint32_t oldPI = mem->pi;

    //Reset HBA
    mem->ghc.AE = 1;
    mem->ghc.HR = 1;

    int sleepCount = 0;
    while(mem->ghc.HR == 1) {
        ksleep(1000);

        sleepCount++;

        if(sleepCount > 2) {
            printf("HBA took longer than 2 seconds to reset HBA\n");

            uint32_t ghc = *(uint32_t*)&mem->ghc;
            printf("ABAR: %x\n", abar);
            printf("GHC: %d\n", ghc);

            panic();

            break;
        }
    }

    mem->ghc.AE = 1;
    mem->pi = oldPI;
    mem->ghc.IE = 1;

    printf("HBA interrupt on line %d\n", interruptVector);

    pic_enableInterrupt(interruptVector);
    irq_install_handler(interruptVector, ahci_interrupt_handler);

    if(mem->cap.S64A == 0) {
        printf("HBA doesn't support 64-bit DMA!!!\n");
        panic();
    }

    uint32_t numPorts = mem->cap.NP;

    printf("HBA PI %d, Num Ports: %d\n", mem->pi, numPorts);

    for(int i = 0; i < numPorts+1; i++) {
        bool isPortActive = (mem->pi >> i) & 1;

        if(!isPortActive) {
            continue;
        }

        HBA_CMD_HEADER* commandList = malloc(sizeof(HBA_CMD_HEADER) * 32);
        for(int j = 0; j < 32; j++) {
            commandList += j;

            HBA_CMD_TBL* cmd_tbl = malloc(sizeof(HBA_CMD_TBL));

            commandList->ctba = ((uintptr_t)cmd_tbl) & 0xffffffff;
            commandList->ctbau = ((uintptr_t)cmd_tbl) >> 32;
        }

        HBA_FIS* receivedFIS = malloc(sizeof(HBA_FIS));

        mem->ports[i].clb = ((uintptr_t)commandList) & 0xffffffff;
        mem->ports[i].clbu = ((uintptr_t)commandList) >> 32;

        mem->ports[i].fb = ((uintptr_t )receivedFIS) & 0xffffffff;
        mem->ports[i].fbu = ((uintptr_t)receivedFIS) >> 32;

        while(mem->ports[i].cmd & HBA_PxCMD_CR);

        mem->ports[i].cmd |= HBA_PxCMD_FRE;
        mem->ports[i].cmd |= HBA_PxCMD_ST;

        uint8_t ipm = (mem->ports[i].ssts >> 8) & 0x0F;
        uint8_t det = mem->ports[i].ssts & 0x0F;

        printf("STATUS: %d, SIGNATURE: %d, IPM: %d, DET: %d\n", mem->ports[i].ssts, mem->ports[i].sig, ipm, det);

        if(mem->ports[i].sig == SATA_SIG_ATA) {
            sataDevice = calloc(1, sizeof(sata_device_t));
            io_request_t ioRequest;
            ioRequest.buffer = malloc(512);
            ioRequest.count = 512;

            ahci_send_command(&ioRequest, ATA_CMD_IDENTIFY);

            //TODO: Parse Identify Packet
            ata_device_info_t* deviceInfo = (ata_device_info_t*)ioRequest.buffer;

            printf("Available space: %d bytes\n", deviceInfo->capacitySectors * 512);

            file_node_t* node = create_ahci_device(sataDevice);

            char* path = calloc(32, sizeof(char));
            snprintf(path, 32, "/%s/%s", "dev", node->name);

            mount_directly(path, node);
            printf("Mounted hdd at %s\n", path);
        }
    }
}