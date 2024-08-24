//
// Created by Jannik on 02.08.2024.
//

#ifndef NIGHTOS_SERVER_H
#define NIGHTOS_SERVER_H

#define DRV_LIB_MMIO_MAP     0
#define DRV_LIB_PCIE_READ08  1
#define DRV_LIB_PCIE_READ16  2
#define DRV_LIB_PCIE_READ32  3
#define DRV_LIB_PCIE_WRITE08 4
#define DRV_LIB_PCIE_WRITE16 5
#define DRV_LIB_PCIE_WRITE32 6

#define DRV_LIB_FUNCTIONS    7

typedef int (*drv_lib_func)(long,long,long,long,long);

enum srv_op {
    SRV_SEND = 1,
    SRV_RECEIVE,
    SRV_CALL,
    SRV_REGISTER,
    SRV_UNREGISTER,
    SRV_AUTH, //this operation is specifically for the server to authenticate requests against the security framework
    SRV_DRV_LIB, //this operation is for servers to call driver library functions(ex. Map MMIO, access pci config space etc.)
};

typedef struct ServerDriverLibArguments {
    int method;
    int size;

    unsigned long args[0];
} srv_drv_lib_args_t;

#define SRV_FLAG_NOBLOCKING 1<<0

int srvctl(enum srv_op operation, int flags, void* message);

#endif //NIGHTOS_SERVER_H
