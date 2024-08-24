//
// Created by Jannik on 20.08.2024.
//
#include "server.h"

drv_lib_func functions[7] = {
        [DRV_LIB_MMIO_MAP]     = 0,
        [DRV_LIB_PCIE_READ08]  = 0,
        [DRV_LIB_PCIE_READ16]  = 0,
        [DRV_LIB_PCIE_READ32]  = 0,
        [DRV_LIB_PCIE_WRITE08] = 0,
        [DRV_LIB_PCIE_WRITE16] = 0,
        [DRV_LIB_PCIE_WRITE32] = 0
};


int srvctl(enum srv_op operation, int flags, void* message) {
    switch(operation) {
        case SRV_DRV_LIB: {
            srv_drv_lib_args_t* args = (srv_drv_lib_args_t*)message;

            if(args->method > DRV_LIB_FUNCTIONS) {
                return -1;
            }

            //functions[args->method]();
            break;
        }
    }

    return 0;
}