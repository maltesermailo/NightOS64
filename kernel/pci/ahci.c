//
// Created by Jannik on 29.06.2024.
//
#include "ahci.h"
#include "../memmgr.h"

void ahci_setup(void* abar) {
    memmgr_map_mmio((uintptr_t)abar, 0x2000, true); //ABAR needs a full page for each port and about 512 bytes for the HBA mem

    HBA_MEM* mem = (HBA_MEM*) abar;
    
}