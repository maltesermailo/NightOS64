//
// Created by Jannik on 05.04.2024.
//

#ifndef NIGHTOS_PCI_H
#define NIGHTOS_PCI_H

#define PCI_TYPE_MASS_STORAGE_CONTROLLER 0x1
#define PCI_TYPE_DISPLAY_CONTROLLER 0x3
#define PCI_TYPE_MEMORY_CONTROLLER 0x5
#define PCI_TYPE_BRIDGE 0x6
#define PCI_TYPE_SERIAL_BUS_CONTROLLER 0xC

#include <stdint.h>
#include <stdbool.h>
#include "../acpi.h"

typedef struct PCIDevice {
    uint8_t type;
    uint8_t subclass;

    int bus;
    int slot;
    int function;

    uint16_t vendorId;
    uint16_t deviceId;
} pci_device_t;

void pci_init(RSDP_t* rsdp);
pci_device_t pci_find_first_by_type(uint8_t type, uint8_t subtype);

#endif //NIGHTOS_PCI_H
