//
// Created by Jannik on 29.06.2024.
//
#include "ahci.h"
#include "../memmgr.h"
#include "../timer.h"
#include "../idt.h"

#include <stdio.h>


void ahci_interrupt_handler(regs_t* regs) {

}

void ahci_setup(void* abar, uint16_t interruptVector) {
    abar = memmgr_map_mmio((uintptr_t)abar, 0x2000, true); //ABAR needs a full page for each port and about 512 bytes for the HBA mem

    HBA_MEM* mem = (HBA_MEM*) abar;

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
    //mem->ghc |= HBA_MEM_GHC_AE;
    //mem->ghc |= HBA_MEM_GHC_HR;
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
    }
}