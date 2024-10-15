//
// Created by Jannik on 29.06.2024.
//
#include "ahci.h"
#include "../memmgr.h"
#include "../timer.h"
#include "../idt.h"
#include "../../libc/include/kernel/list.h"

#include <stdio.h>
#include "../../libc/include/string.h"
#include "../terminal.h"

list_t hard_drives;
sata_device_t* sataDevice;
HBA_MEM* hba;

void ahci_interrupt_handler(regs_t* regs) {
  printf("GOT AHCI INTERRUPT\n");
}

static void ahci_cache_remove_entry(disk_cache* cache, int i, disk_cache_entry_t* entry) {
  list_remove_by_index(&cache->entries, i);

  cache->total_size -= entry->size;
}

static void ahci_cache_add_entry(disk_cache* cache, disk_cache_entry_t* entry) {
  list_insert(&cache->entries, entry);

  cache->total_size += entry->size;
}

static void ahci_cache_evict(struct SATADevice* device) {
  disk_cache* cache = device->diskCache;

  while (cache->total_size > DISK_CACHE_MAX_SIZE && cache->entries.head) {
    disk_cache_entry_t* to_evict = cache->entries.head->value;

    if (to_evict->dirty) {
      // Write back dirty data to the actual file
      io_request_t* ioRequest = kcalloc(1, sizeof(io_request_t));
      ioRequest->type = IO_WRITE;
      ioRequest->count = to_evict->size;
      ioRequest->offset = to_evict->offset;
      ioRequest->buffer = to_evict->data;

      if(!ahci_send_command(device, ioRequest, ATA_CMD_WRITE_DMA_EXT)) {
        return;
      }

      kfree(ioRequest);
    }
    ahci_cache_remove_entry(cache, 0, to_evict);

    kfree(to_evict->data);
    kfree(to_evict);
  }
}

static void ahci_cache_flush(struct SATADevice* device) {
  disk_cache* cache = device->diskCache;

  while (cache->entries.head) {
    disk_cache_entry_t* to_evict = cache->entries.head->value;

    if (to_evict->dirty) {
      // Write back dirty data to the actual file
      io_request_t* ioRequest = kcalloc(1, sizeof(io_request_t));
      ioRequest->type = IO_WRITE;
      ioRequest->count = to_evict->size;
      ioRequest->offset = to_evict->offset;
      ioRequest->buffer = to_evict->data;

      if(device->deviceType == DRIVE_TYPE_SATA_HDD) {
        ahci_send_command(device, ioRequest, ATA_CMD_WRITE_DMA_EXT);
      } else {
        atapi_send_command(device, ioRequest, ATAPI_CMD_WRITE);
      }

      kfree(ioRequest);
    }
    ahci_cache_remove_entry(cache, 0, to_evict);

    kfree(to_evict->data);
    kfree(to_evict);
  }
}

static disk_cache_entry_t* ahci_cache_find(disk_cache* cache, size_t offset, size_t size) {
  for (list_entry_t* list_entry = cache->entries.head; list_entry != NULL; list_entry = list_entry->next) {
    disk_cache_entry_t* entry = (disk_cache_entry_t*) list_entry->value;
    if (entry->offset <= offset) {
      if(entry->offset + entry->size < offset + size) {
        uint8_t *old_data = entry->data;

        int64_t size_variance =
            ((entry->offset + entry->size) - (offset + size));

        if (size_variance > 0) {
          uint8_t *new_data = calloc(1, entry->size + size_variance);

          memcpy(new_data, old_data, entry->size);
          entry->data = new_data;
          entry->size = entry->size + size_variance;
          free(old_data);
        }
      }
      return entry;
    }
  }
  return NULL;
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

bool ahci_send_command(struct SATADevice* ataDevice, io_request_t* ioRequest, int ataCommand) {
    HBA_PORT* port = &hba->ports[ataDevice->port];

    uint64_t offset = ioRequest->offset / 512; //Convert to LBA

    int cmdIndex = get_next_free(ataDevice->port);

    if(cmdIndex == -1) {
        return false;
    }

    void* ptrCmdHeader = memmgr_get_from_physical(((uintptr_t)port->clbu) << 32 | port->clb);
    HBA_CMD_HEADER* cmdHeader = (HBA_CMD_HEADER*)ptrCmdHeader;
    cmdHeader += cmdIndex;

    HBA_CMD_TBL* cmdTable = (HBA_CMD_TBL*) memmgr_get_from_physical((((uintptr_t)cmdHeader->ctbau) << 32 | cmdHeader->ctba));
    FIS_REG_H2D* commandFIS = (FIS_REG_H2D*)(cmdTable->cfis);
    memset(commandFIS, 0, 5 * 4);

    cmdHeader->cfl = sizeof(FIS_REG_D2H) / sizeof(uint32_t);
    cmdHeader->prdtl = 1;
    cmdHeader->w = 0;
    cmdHeader->prdbc = 0;
    cmdHeader->p = 1;
    cmdHeader->c = 1;

    commandFIS->fis_type = FIS_TYPE_REG_H2D; //To Device
    commandFIS->command = ataCommand; //Command
    commandFIS->c = 1; //Command Type
    commandFIS->featureh = 0;
    commandFIS->featurel = 0;
    commandFIS->lba0 = (uint8_t)offset;
    commandFIS->lba1 = (uint8_t)(offset>>8);
    commandFIS->lba2 = (uint8_t)(offset>>16);
    commandFIS->device = ataCommand != ATA_CMD_IDENTIFY ? 1<<6 : 0;  // LBA mode
    commandFIS->lba3 = (uint8_t)(offset>>24);
    commandFIS->lba4 = (uint8_t)(offset>>32);
    commandFIS->lba5 = (uint8_t)(offset>>40);
    commandFIS->countl = ataCommand != ATA_CMD_IDENTIFY ? (ioRequest->count / 512) & 0xFF : 0;
    commandFIS->counth = ataCommand != ATA_CMD_IDENTIFY ? ((ioRequest->count / 512) >> 8) & 0xFF : 0;

    //TODO: Allow buffers larger than 8k
    uintptr_t bufferPhys = (uintptr_t) memmgr_get_page_physical((uintptr_t) ioRequest->buffer);

    if(ioRequest->count <= 4096 && ioRequest->count > 0) {
        cmdTable->prdt_entry[0].dba = (uintptr_t)bufferPhys;
        cmdTable->prdt_entry[0].dbau = ((uintptr_t)bufferPhys >> 32);
        cmdTable->prdt_entry[0].dbc = (ioRequest->count - 1) | 1;
    } else if (ioRequest->count > 4096) {
        //PRDT supports 8K byte reads per PRDT, but since we are using paging and our physical pages might be scattered, we do 4K per PRDT. Easier to manage.
        int countOfPRDTs = ioRequest->count / 4096;

        cmdHeader->prdtl = countOfPRDTs;

        if(countOfPRDTs > 8) {
            printf("WARNING: ATA Read tried to read more than 32 KB!");
            //Not supported
            return false;
        }

        int remaining = ioRequest->count;

        for(int i = 0; i < countOfPRDTs; i++) {
            int count = remaining > 4096 ? 4096 : remaining;

            cmdTable->prdt_entry[0].dba = (uintptr_t)memmgr_get_page_physical(
                    (uintptr_t) (ioRequest->buffer + i * 4096));
            cmdTable->prdt_entry[0].dbau = ((uintptr_t)memmgr_get_page_physical(
                    (uintptr_t) (ioRequest->buffer + i * 4096)) >> 32);
            cmdTable->prdt_entry[0].dbc = (count - 1) | 1;

            remaining -= count;
        }
    }

    printf("Command FIS:\n");
    printf("  FIS Type: 0x%x\n", commandFIS->fis_type);
    printf("  PM Port: %d\n", commandFIS->pmport);
    printf("  Command: 0x%x\n", commandFIS->command);
    printf("  Features: 0x%x\n", commandFIS->featurel | (commandFIS->featureh << 8));
    printf("  LBA: 0x%x\n", (unsigned long)commandFIS->lba0 |
                                 ((unsigned long)commandFIS->lba1 << 8) |
                                 ((unsigned long)commandFIS->lba2 << 16) |
                                 ((unsigned long)commandFIS->lba3 << 24) |
                                 ((unsigned long)commandFIS->lba4 << 32) |
                                 ((unsigned long)commandFIS->lba5 << 40));
    printf("  Device: 0x%x\n", commandFIS->device);
    printf("  Control: 0x%x\n", commandFIS->control);

    printf("Command Header:\n");
    printf("  Flags: 0x%x\n", *(uint16_t*)cmdHeader);
    printf("  PRDTL: %d\n", cmdHeader->prdtl);
    printf("  PRDBC: %d\n", cmdHeader->prdbc);

    unsigned long spin = 0;

    uint32_t tfd = port->tfd;
    if (tfd & ATA_SR_ERR) {
      // Clear error by reading the error register
      uint8_t error = (tfd >> 8) & 0xFF;
      printf("Clearing pre-existing error: 0x%x\n", error);
    }

    while ((port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)) && spin < 1000000)
    {
        spin++;
    }
    if (spin == 1000000)
    {
        printf("Port is hung\n");
        return false;
    }

    asm volatile ("mfence" ::: "memory");

    printf("Port status before command:\n");
    printf("  TFD: 0x%x\n", port->tfd);
    printf("  SSTS: 0x%x\n", port->ssts);
    printf("  SCTL: 0x%x\n", port->sctl);
    printf("  SERR: 0x%x\n", port->serr);

    port->is = ~0;

    port->ci |= 1 << cmdIndex;

    asm volatile ("mfence" ::: "memory");

    printf("Port status after command:\n");
    printf("  TFD: 0x%x\n", port->tfd);
    printf("  SSTS: 0x%x\n", port->ssts);
    printf("  SCTL: 0x%x\n", port->sctl);
    printf("  SERR: 0x%x\n", port->serr);

    while(true) {
        if((port->ci & (1 << cmdIndex)) == 0) {
            break;
        }

        if(port->is & HBA_PxIS_TFES) {
            printf("READ DISK ERROR");
            panic();

            return false;
        }
    }

    if(port->tfd & (ATA_SR_ERR | ATA_SR_DF)) {
        printf("ATA command failed. Cmd: 0x%x, Status: 0x%x, Error: 0x%x\n", ataCommand, port->tfd & 0xFF, (port->tfd >> 8) & 0xFF);
        return false;
    }

    printf("ATA: %d bytes transferred \n", cmdHeader->prdbc);

    return true;
}

bool atapi_send_command(struct SATADevice* ataDevice, io_request_t* ioRequest, int atapiCommand) {
    HBA_PORT* port = &hba->ports[ataDevice->port];

    uint64_t offset = ioRequest->offset / 2048; //Convert to LBA

    int cmdIndex = get_next_free(ataDevice->port);

    if(cmdIndex == -1) {
        return false;
    }

    void* ptrCmdHeader = memmgr_get_from_physical(((uintptr_t)port->clbu) << 32 | port->clb);
    HBA_CMD_HEADER* cmdHeader = (HBA_CMD_HEADER*)ptrCmdHeader;
    cmdHeader += cmdIndex;

    HBA_CMD_TBL* cmdTable = (HBA_CMD_TBL*) memmgr_get_from_physical((((uintptr_t)cmdHeader->ctbau) << 32 | cmdHeader->ctba));
    FIS_REG_H2D* commandFIS = (FIS_REG_H2D*)(cmdTable->cfis);
    memset(commandFIS, 0, 5 * 4);

    cmdHeader->cfl = sizeof(FIS_REG_D2H) / sizeof(uint32_t);
    cmdHeader->prdtl = 1;
    cmdHeader->w = ioRequest->type == IO_WRITE;
    cmdHeader->a = 1; //ATAPI command

    commandFIS->fis_type = FIS_TYPE_REG_H2D; //To Device
    commandFIS->command = ATA_CMD_PACKET; //Command
    commandFIS->c = 1; //Command Type
    commandFIS->device = 0;  // Select Master
    commandFIS->featurel = 1;

    uint8_t atapi_packet[12] = {0};

    if(atapiCommand == ATAPI_CMD_INQUIRY) {
        atapi_packet[0] = atapiCommand;
        atapi_packet[4] = 36;
    } else {
        atapi_packet[0] = atapiCommand;
        atapi_packet[2] = (offset >> 24) & 0xFF;
        atapi_packet[3] = (offset >> 16) & 0xFF;
        atapi_packet[4] = (offset >> 8) & 0xFF;
        atapi_packet[5] = offset & 0xFF;
        atapi_packet[7] = (ioRequest->count / 2048) >> 8;
        atapi_packet[8] = (ioRequest->count / 2048) & 0xFF;
    }

    // Copy ATAPI packet to command table
    memcpy(cmdTable->acmd, atapi_packet, 12);

    //TODO: Allow buffers larger than 8k
    uintptr_t bufferPhys = (uintptr_t) memmgr_get_page_physical((uintptr_t) ioRequest->buffer);

    if(ioRequest->count < 4096) {
        cmdTable->prdt_entry[0].dba = (uintptr_t)bufferPhys;
        cmdTable->prdt_entry[0].dbau = ((uintptr_t)bufferPhys >> 32);
        cmdTable->prdt_entry[0].dbc = (ioRequest->count - 1) | 1;
    } else {
        //PRDT supports 8K byte reads per PRDT, but since we are using paging and our physical pages might be scattered, we do 4K per PRDT. Easier to manage.
        int countOfPRDTs = ioRequest->count / 4096;

        cmdHeader->prdtl = countOfPRDTs;

        if(countOfPRDTs > 8) {
            printf("WARNING: ATA Read tried to read more than 32 KB!");
            //Not supported
            return false;
        }

        int remaining = ioRequest->count;

        for(int i = 0; i < countOfPRDTs; i++) {
            int count = remaining > 4096 ? 4096 : remaining;

            cmdTable->prdt_entry[0].dba = (uintptr_t)memmgr_get_page_physical(
                    (uintptr_t) (ioRequest->buffer + i * 4096));
            cmdTable->prdt_entry[0].dbau = ((uintptr_t)memmgr_get_page_physical(
                    (uintptr_t) (ioRequest->buffer + i * 4096)) >> 32);
            cmdTable->prdt_entry[0].dbc = (count - 1) | 1;

            remaining -= count;
        }
    }

    unsigned long spin = 0;

    while ((port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)) && spin < 1000000)
    {
        spin++;
    }
    if (spin == 1000000)
    {
        printf("Port is hung\n");
        return false;
    }

    port->is = ~0;

    port->ci |= 1 << cmdIndex;

    while(true) {
        if((port->ci & (1 << cmdIndex)) == 0) {
            break;
        }

        if(port->is & HBA_PxIS_TFES) {
            printf("READ DISK ERROR");
            panic();

            return false;
        }
    }

    if(port->tfd & (ATA_SR_ERR | ATA_SR_DF)) {
        printf("ATAPI command failed. Status: 0x%x, Error: 0x%x\n", atapiCommand, port->tfd & 0xFF, (port->tfd >> 8) & 0xFF);
        return false;
    }

    return true;
}

int ahci_read(file_node_t* node, char* buf, size_t offset, size_t length) {
    struct SATADevice* thisDevice = (struct SATADevice*)node->fs;

    disk_cache_entry_t* entry = ahci_cache_find(thisDevice->diskCache, offset, length);

    if (entry) {
      // Cache hit
      memcpy(buf, entry->data + (offset - entry->offset), length);
      return length;
    }

    // Cache miss
    entry = kmalloc(sizeof(disk_cache_entry_t));
    entry->offset = offset;
    entry->size = length;
    entry->data = calloc(1, length);
    entry->dirty = false;

    io_request_t* ioRequest = calloc(1, sizeof(io_request_t));
    ioRequest->type = IO_READ;
    ioRequest->count = length;
    ioRequest->offset = offset;
    ioRequest->buffer = entry->data;

    if(thisDevice->deviceType == DRIVE_TYPE_SATA_HDD) {
      if(!ahci_send_command(thisDevice, ioRequest, ATA_CMD_READ_DMA_EXT)) {
        return -1;
      }
    } else if (thisDevice->deviceType == DRIVE_TYPE_OPTICAL) {
      if(!atapi_send_command(thisDevice, ioRequest, ATAPI_CMD_READ)) {
        return -1;
      }
    }

    memcpy(buf, ioRequest->buffer, length);

    ahci_cache_add_entry(thisDevice->diskCache, entry);
    ahci_cache_evict(thisDevice);

    free(ioRequest);

    return length;
}

int ahci_write(file_node_t* node, char* buf, size_t offset, size_t length) {
    struct SATADevice* thisDevice = (struct SATADevice*)node->fs;

    disk_cache_entry_t* entry = ahci_cache_find(thisDevice->diskCache, offset, length);

    if (entry) {
      // Cache hit, update existing entry
      memcpy(entry->data + (offset - entry->offset), buf, length);
      entry->dirty = true;
    } else {
      // Cache miss, create new entry
      entry = kmalloc(sizeof(disk_cache_entry_t));
      entry->offset = offset;
      entry->size = length;
      entry->data = kmalloc(length);
      memcpy(entry->data, buf, length);
      entry->dirty = true;
      ahci_cache_add_entry(thisDevice->diskCache, entry);
      ahci_cache_evict(thisDevice);
    }

    return length;
}

int atapi_read(file_node_t* node, char* buf, size_t offset, size_t length) {
    struct SATADevice* thisDevice = (struct SATADevice*)node->fs;

    io_request_t* ioRequest = calloc(1, sizeof(io_request_t));
    ioRequest->type = IO_READ;
    ioRequest->count = length;
    ioRequest->offset = offset;
    ioRequest->buffer = calloc(1, length);

    if(!atapi_send_command(thisDevice, ioRequest, ATAPI_CMD_READ)) {
      return -1;
    }

    memcpy(buf, ioRequest->buffer, length);

    free(ioRequest->buffer);
    free(ioRequest);

    return length;
}


void ahci_close(file_node_t* node) {
  struct SATADevice* thisDevice = (struct SATADevice*)node->fs;

  ahci_cache_flush(thisDevice);
}

file_node_t* create_ahci_device(struct SATADevice* sataDevice) {
    file_node_t* node = calloc(1, sizeof(file_node_t));
    node->type = FILE_TYPE_BLOCK_DEVICE;
    node->fs = sataDevice;
    node->size = sataDevice->size;

    switch (sataDevice->deviceType) {
        case DRIVE_TYPE_SATA_HDD:
        case DRIVE_TYPE_SATA_SSD:
        case DRIVE_TYPE_ATA:
            snprintf(node->name, 16, "hd%d", sataDevice->port);
            node->file_ops.read = ahci_read;
            node->file_ops.write = ahci_write;
            node->file_ops.close = ahci_close;

            break;
        case DRIVE_TYPE_OPTICAL:
        case DRIVE_TYPE_REMOVABLE:
            snprintf(node->name, 16, "cd%d", sataDevice->port);
            node->file_ops.read = atapi_read;
            node->file_ops.write = atapi_write;
            node->file_ops.close = ahci_close;
            break;
        case DRIVE_TYPE_UNKNOWN:
            snprintf(node->name, 16, "unknown_drive%d", sataDevice->port);
            break;
    }
    node->ref_count = 0;

    return node;
}

void ahci_setup(void* abar, uint16_t interruptVector) {
    abar = memmgr_map_mmio((uintptr_t)abar, 0x2000, FLAG_UCMINUS, true); //ABAR needs a full page for each port and about 512 bytes for the HBA mem

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

        //mem->ports[i].cmd &= ~HBA_PxCMD_ST;
        //mem->ports[i].cmd &= ~HBA_PxCMD_FRE;

        //while(mem->ports[i].cmd & HBA_PxCMD_CR);

        HBA_CMD_HEADER* commandList = memmgr_map_mmio(kalloc_frame(), 4096, FLAG_UCMINUS, true);
        memset(commandList, 0, 4096);
        for(int j = 0; j < 32; j++) {
            HBA_CMD_HEADER* header = commandList + j;

            HBA_CMD_TBL* cmd_tbl = memmgr_map_mmio(kalloc_frame(), 4096, FLAG_UCMINUS, true);
            memset(cmd_tbl, 0, 4096);

            header->ctba = ((uintptr_t) memmgr_get_mmio_physical((uintptr_t)cmd_tbl)) & 0xffffffff;
            header->ctbau = ((uintptr_t)memmgr_get_mmio_physical((uintptr_t)cmd_tbl)) >> 32;
        }

        HBA_FIS* receivedFIS = memmgr_map_mmio(kalloc_frame(), 4096, FLAG_UCMINUS, true);
        memset(receivedFIS, 0, 4096);

        mem->ports[i].clb = ((uintptr_t) memmgr_get_mmio_physical((uintptr_t)commandList)) & 0xffffffff;
        mem->ports[i].clbu = ((uintptr_t)memmgr_get_mmio_physical((uintptr_t)commandList)) >> 32;

        mem->ports[i].fb = ((uintptr_t ) memmgr_get_mmio_physical((uintptr_t)receivedFIS)) & 0xffffffff;
        mem->ports[i].fbu = ((uintptr_t) memmgr_get_mmio_physical((uintptr_t)receivedFIS)) >> 32;

        while(mem->ports[i].cmd & HBA_PxCMD_CR);

        asm volatile ("mfence" ::: "memory");

        mem->ports[i].cmd |= HBA_PxCMD_FRE;
        mem->ports[i].cmd |= HBA_PxCMD_ST;

        asm volatile ("mfence" ::: "memory");

        uint8_t ipm = (mem->ports[i].ssts >> 8) & 0x0F;
        uint8_t det = mem->ports[i].ssts & 0x0F;

        printf("STATUS: %d, SIGNATURE: %d, IPM: %d, DET: %d\n", mem->ports[i].ssts, mem->ports[i].sig, ipm, det);

        if(mem->ports[i].sig == SATA_SIG_ATA) {
            sataDevice = calloc(1, sizeof(sata_device_t));
            sataDevice->port = i;
            sataDevice->diskCache = kcalloc(1, sizeof(disk_cache));

            char* buf = malloc(512 + 511);

            io_request_t ioRequest;
            ioRequest.buffer =
                (void *)(((uintptr_t)buf + 511 - 1) & ~(511 - 1));
            memset(ioRequest.buffer, 0, 512);
            ioRequest.count = 512;
            ioRequest.offset = 0;
            ioRequest.type = IO_READ;

            sataDevice->deviceType = DRIVE_TYPE_SATA_HDD;

            printf("Port setup:\n");
            printf("  CLB: 0x%x\n", (unsigned long)mem->ports[i].clb | ((unsigned long)mem->ports[i].clbu << 32));
            printf("  FB: 0x%x\n", (unsigned long)mem->ports[i].fb | ((unsigned long)mem->ports[i].fbu << 32));
            printf("  CMD: 0x%x\n", mem->ports[i].cmd);

            ahci_send_command(sataDevice, &ioRequest, ATA_CMD_IDENTIFY);

            //TODO: Parse Identify Packet
            ata_device_info_t* deviceInfo = (ata_device_info_t*)ioRequest.buffer;

            printf("Available space: %d bytes\n", ((uint64_t)deviceInfo->numUserAddressableSectors) * 512);

            file_node_t* node = create_ahci_device(sataDevice);

            char* path = calloc(32, sizeof(char));
            snprintf(path, 32, "/%s/%s", "dev", node->name);
            char* pathDup = strdup(path);

            sataDevice->node = node;
            sataDevice->port = i;
            sataDevice->size = deviceInfo->capacitySectors * 512;

            mount_directly(pathDup, node);
            free(pathDup);
            printf("Mounted hdd at %s\n", path);

            free(buf);
        }

        if(mem->ports[i].sig == SATA_SIG_ATAPI) {
            struct SATADevice* ataDevice = calloc(1, sizeof(sata_device_t));
            ataDevice->port = i;
            ataDevice->diskCache = kcalloc(1, sizeof(disk_cache));

            io_request_t ioRequest;
            char* buf = malloc(512 + 511);

            ioRequest.buffer =
                (void *)(((uintptr_t)buf + 511 - 1) & ~(511 - 1));
            memset(ioRequest.buffer, 0, 512);
            ioRequest.count = 512;
            ioRequest.offset = 0;

            if(!atapi_send_command(ataDevice, &ioRequest, ATAPI_CMD_INQUIRY)) {
                printf("[ATAPI] Couldn't send inquiry command to port %d.\n", ataDevice->port);
                free(ioRequest.buffer);
                continue;
            }

            //TODO: Parse Identify Packet
            atapi_device_info_t* deviceInfo = (atapi_device_info_t*)ioRequest.buffer;

            switch(INQUIRY_PERIPHERAL_DEVICE_TYPE(deviceInfo)) {
                case 0x05:
                    ataDevice->deviceType = DRIVE_TYPE_OPTICAL;
                    break;
                default:
                    ataDevice->deviceType = DRIVE_TYPE_ATA;
                    break;
            }

            free(ioRequest.buffer);
            ioRequest.buffer = malloc(sizeof(ATAPI_READ_CAPACITY_DATA));
            ioRequest.count = sizeof(ATAPI_READ_CAPACITY_DATA);

            if(!atapi_send_command(ataDevice, &ioRequest, ATAPI_CMD_READ_CAPACITY)) {
                printf("[ATAPI] Couldn't send read capacity command to port %d.\n", ataDevice->port);
                free(ioRequest.buffer);
                continue;
            }
            ATAPI_READ_CAPACITY_DATA* capacityData = (ATAPI_READ_CAPACITY_DATA*)ioRequest.buffer;
            capacityData->lba_last = __builtin_bswap32(capacityData->lba_last);
            capacityData->block_size = __builtin_bswap32(capacityData->block_size);

            file_node_t* node = create_ahci_device(ataDevice);

            char* path = calloc(32, sizeof(char));
            snprintf(path, 32, "/%s/%s", "dev", node->name);
            char* pathDup = strdup(path);

            ataDevice->node = node;
            ataDevice->size = (capacityData->lba_last+1) * capacityData->block_size;

            mount_directly(pathDup, node);
            free(pathDup);
            printf("Mounted ATAPI device at %s\n", path);
        }
    }
}