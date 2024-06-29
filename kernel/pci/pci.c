//
// Created by Jannik on 05.04.2024.
//

#include "pci.h"
#include <stddef.h>
#include <string.h>
#include "../terminal.h"
#include "../arch/amd64/io.h"
#include "../memmgr.h"
#include "ahci.h"

static pci_device_t pciDevices[65536];
static uint8_t* pciBase;

/////////////////////////////LEGACY CODE/////////////////////////////////////////////

uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;

    // Create configuration address
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
                         (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    // Write out the address
    outl(0xCF8, address);
    tmp = (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
    return tmp;
}

void pciConfigWriteDword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;

    // Create configuration address
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
                         (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    // Write out the address
    outl(0xCF8, address);
    outl(0xCFC, value);
}

void pciConfigWriteWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;

    // Create configuration address
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
                         (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    // Write out the address
    outl(0xCF8, address);
    outw(0xCFC, value);
}

void pciConfigWriteByte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;

    // Create configuration address
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
                         (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    // Write out the address
    outl(0xCF8, address);
    outb(0xCFC, value);
}

bool pciCheckVendor(uint8_t bus, uint8_t slot) {
    uint16_t vendor;
    /* Try and read the first configuration register. Since there are no
     * vendors that == 0xFFFF, it must be a non-existent device. */
    return (vendor = pciConfigReadWord(bus, slot, 0, 0)) != 0xFFFF;
}

uint16_t getVendorID(uint8_t bus, uint8_t device, uint8_t function) {
    return pciConfigReadWord(bus, device, function, 0);
}

uint8_t getHeaderType(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t out;

    out = pciConfigReadWord(bus, device, function, 0x3C);

    //First 8 bits are header type
    return (uint8_t)(out & 0xFF);
}

uint8_t getBaseClass(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t out;

    out = pciConfigReadWord(bus, device, function, 0x28);

    //Second 8 bits are base class
    return (uint8_t)((out >> 8) & 0xFF);
}

uint8_t getSubClass(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t out;

    out = pciConfigReadWord(bus, device, function, 0x28);

    //First 8 bits are sub class
    return (uint8_t)(out & 0xFF);
}

void checkFunction(uint8_t bus, uint8_t device, uint8_t function) {
    uint8_t baseClass;
    uint8_t subClass;
    uint8_t vendorId;

    baseClass = getBaseClass(bus, device, function);
    subClass = getSubClass(bus, device, function);
    vendorId = getVendorID(bus, device, function);
    if ((baseClass == 0x6) && (subClass == 0x4)) {
        //For simplicity, this will not be implemented.
        //secondaryBus = getSecondaryBus(bus, device, function);
        //checkBus(secondaryBus);
    }

    pciDevices[256 * bus + 32 * device + function].type = baseClass;
    pciDevices[256 * bus + 32 * device + function].subclass = subClass;
    pciDevices[256 * bus + 32 * device + function].bus = bus;
    pciDevices[256 * bus + 32 * device + function].slot = device;
    pciDevices[256 * bus + 32 * device + function].function = function;
    pciDevices[256 * bus + 32 * device + function].vendorId = vendorId;
}

void checkDevice(uint8_t bus, uint8_t device) {
    uint8_t function = 0;

    uint16_t vendorId = getVendorID(bus, device, function);
    if(vendorId == 0xFFFF) return;
    checkFunction(bus, device, function);

    uint8_t headerType = getHeaderType(bus, device, function);

    if((headerType & 0x80) != 0) {
        for(function = 1; function < 8; function++) {
            if(getVendorID(bus, device, function) != 0xFFFF) {
                checkFunction(bus, device, function);
            }
        }
    }
}

void checkBus(uint8_t bus) {
    uint8_t device;

    for(device = 0; device < 32; device++) {
        checkDevice(bus, device);
    }
}

void pci_probe_legacy() {
    //Probe PCI
    uint8_t function;
    uint8_t bus;
    uint8_t headerType;

    headerType = getHeaderType(0, 0, 0);
    if ((headerType & 0x80) == 0) {
        printf("[PCI] Found Single PCI Host controller\n");
        // Single PCI host controller
        checkBus(0);
    } else {
        // Multiple PCI host controllers
        for (function = 0; function < 8; function++) {
            if (getVendorID(0, 0, function) != 0xFFFF) break;
            bus = function;
            checkBus(bus);
        }
    }
}

/////////////////////////END LEGACY CODE/////////////////////////////////////////////

pci_device_t pci_find_first_by_type(uint8_t type, uint8_t subtype) {
    for(int i = 0; i < 65536; i++) {
        if(pciDevices[i].type != type) {
            continue;
        }

        if(pciDevices[i].subclass != subtype) {
            continue;
        }

        return pciDevices[i];
    }
}

bool doChecksum(ACPISDTHeader *tableHeader)
{
    unsigned char sum = 0;

    for (int i = 0; i < tableHeader->Length; i++)
    {
        sum += ((char *) tableHeader)[i];
    }

    return sum == 0;
}

bool doChecksumXSDP(XSDP_t *tableHeader)
{
    unsigned char sum = 0;

    for (int i = 0; i < sizeof(RSDP_t); i++)
    {
        sum += ((char *) tableHeader)[i];
    }

    if(sum != 0) {
        return false;
    }

    sum = 0;

    for (int i = sizeof(RSDP_t); i < sizeof(XSDP_t); i++)
    {
        sum += ((char *) tableHeader)[i];
    }

    return sum == 0;
}

bool doChecksumRSDP(RSDP_t *tableHeader)
{
    unsigned char sum = 0;


    for (int i = 0; i < sizeof(RSDP_t); i++)
    {
        sum += ((char *) tableHeader)[i];
    }

    return sum == 0;
}

uint8_t pcieConfigReadByte(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
    volatile uint16_t* address = (volatile uint16_t *) ((((bus * 256) + (slot * 8) + func) * 4096) + offset);
    address = (uint16_t*) ((uint64_t)address + (uint64_t)pciBase);

    return (*address);
}

uint16_t pcieConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
    volatile uint16_t* address = (volatile uint16_t *) ((((bus * 256) + (slot * 8) + func) * 4096) + offset);
    address = (uint16_t*) ((uint64_t)address + (uint64_t)pciBase);

    return (*address);
}

uint32_t pcieConfigReadDWord(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
    volatile uint16_t* address = (volatile uint16_t *) ((((bus * 256) + (slot * 8) + func) * 4096) + offset);
    address = (uint16_t*) ((uint64_t)address + (uint64_t)pciBase);

    return (*address);
}

void pcieConfigWriteByte(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint8_t value) {
    volatile uint16_t* address = (volatile uint16_t *) ((((bus * 256) + (slot * 8) + func) * 4096) + offset);
    address = (uint16_t*) ((uint64_t)address + (uint64_t)pciBase);

    *address = value;
}

void pcieConfigWriteWord(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint16_t value) {
    volatile uint16_t* address = (volatile uint16_t *) ((((bus * 256) + (slot * 8) + func) * 4096) + offset);
    address = (uint16_t*) ((uint64_t)address + (uint64_t)pciBase);

    *address = value;
}

void pcieConfigWriteDWord(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint32_t value) {
    volatile uint16_t* address = (volatile uint16_t *) ((((bus * 256) + (slot * 8) + func) * 4096) + offset);
    address = (uint16_t*) ((uint64_t)address + (uint64_t)pciBase);

    *address = value;
}

uint16_t pcieGetVendorID(uint8_t bus, uint8_t device, uint8_t function) {
    return pcieConfigReadWord(bus, device, function, 0);
}

uint8_t pcieGetHeaderType(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t out;

    out = pcieConfigReadWord(bus, device, function, 0xe);

    //First 8 bits are header type
    return (uint8_t)(out & 0xFF);
}

uint8_t pcieGetBaseClass(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t out;

    out = pcieConfigReadWord(bus, device, function, 0xa);

    //Second 8 bits are base class
    return (uint8_t)((out >> 8) & 0xFF);
}

uint8_t pcieGetSubClass(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t out;

    out = pcieConfigReadWord(bus, device, function, 0xa);

    //First 8 bits are sub class
    return (uint8_t)(out & 0xFF);
}

uint8_t pcieGetProgIf(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t out;

    out = pcieConfigReadWord(bus, device, function, 0x8);

    //First 8 bits are sub class
    return (uint8_t)((out >> 8) & 0xFF);
}

uint8_t pcieBusGetPrimary(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t out;

    out = pcieConfigReadWord(bus, device, function, 0x18);

    //First 8 bits are primary bus
    return (uint8_t)(out & 0xFF);
}

uint8_t pcieBusGetSecondary(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t out;

    out = pcieConfigReadWord(bus, device, function, 0x18);

    //Second 8 bits are base class
    return (uint8_t)((out >> 8) & 0xFF);
}

void checkFunctionPCIe(uint8_t bus, uint8_t device, uint8_t function) {
    uint8_t baseClass;
    uint8_t subClass;
    uint8_t vendorId;
    uint8_t progIf;

    baseClass = pcieGetBaseClass(bus, device, function);
    subClass = pcieGetSubClass(bus, device, function);
    vendorId = pcieGetVendorID(bus, device, function);
    progIf = pcieGetProgIf(bus, device, function);
    if ((baseClass == 0x6) && (subClass == 0x4)) {
        printf("test\n");
        uint8_t primaryBus;
        uint8_t secondaryBus;
        secondaryBus = pcieBusGetSecondary(bus, device, function);
        primaryBus = pcieBusGetPrimary(bus, device, function);

        printf("[PCIe] Found PCI-to-PCI Bridge at primary bus%d, secondary bus %d\n", primaryBus, secondaryBus);
        for(int device = 0; device < 32; device++) {
            checkDevice(secondaryBus, device);
        }
    }

    printf("[PCI] Found PCI device %d, %d, %d, %d at %d,%d,%d\n", baseClass, subClass, progIf, vendorId, bus, device, function);

    pciDevices[256 * bus + 32 * device + function].type = baseClass;
    pciDevices[256 * bus + 32 * device + function].subclass = subClass;
    pciDevices[256 * bus + 32 * device + function].bus = bus;
    pciDevices[256 * bus + 32 * device + function].slot = device;
    pciDevices[256 * bus + 32 * device + function].function = function;
    pciDevices[256 * bus + 32 * device + function].vendorId = vendorId;
}

void checkDevicePCIe(uint8_t bus, uint8_t device) {
    uint8_t function = 0;

    uint16_t vendorId = pcieGetVendorID(bus, device, function);
    if(vendorId == 0xFFFF) return;
    checkFunctionPCIe(bus, device, function);

    uint8_t headerType = pcieGetHeaderType(bus, device, function);

    if((headerType & 0x80) != 0) {
        for(function = 1; function < 8; function++) {
            if(getVendorID(bus, device, function) != 0xFFFF) {
                checkFunctionPCIe(bus, device, function);
            }
        }
    }
}

void pci_init(RSDP_t* rsdp) {
    if(!doChecksumRSDP(rsdp)) {
        printf("Warning: ACPI XSDP wrong checksum");

        return;
    }

    RSDT* rsdt = (RSDT*) memmgr_get_from_physical(rsdp->RsdtAddress);
    if(!doChecksum(&rsdt->h)) {
        printf("Warning: ACPI XSDT wrong checksum");

        return;
    }

    uintptr_t entries = (rsdt->h.Length - sizeof(rsdt->h)) / 4;
    for(uintptr_t i = 0; i < entries; i++) {
        ACPISDTHeader *h = (ACPISDTHeader *) memmgr_get_from_physical(rsdt->PointerToOtherSDT[i]);
        if (!strncmp(h->Signature, "MCFG", 4)) {
            MCFG* mcfg = (MCFG*) memmgr_get_from_physical(rsdt->PointerToOtherSDT[i]);

            if(!doChecksum(&mcfg->h)) {
                printf("Warning: ACPI MCFG wrong checksum");

                return;
            }

            uintptr_t mcfgEntries = (mcfg->h.Length - 44) / 16;
            for(uintptr_t j = 0; j < mcfgEntries; j++) {
                printf("PCIe base address: %x\n", mcfg->csba[j].base);

                pciBase = (uint8_t *) memmgr_get_from_physical(mcfg->csba[j].base);
            }
        }
    }

    for(int i = 0; i < 65536; i++) {
        //Set all PCI devices as unclassified and therefore not interested
        pciDevices[i].type = 0xFF;
    }

    //Probe PCI
    uint8_t function;
    uint8_t bus;
    uint8_t headerType;

    headerType = pcieGetHeaderType(0, 0, 0);
    if ((headerType & 0x80) == 0) {
        printf("[PCI] Found Single PCI Host controller\n");
        // Single PCI host controller
        for(int device = 0; device < 32; device++) {
            checkDevicePCIe(0, device);
        }
    } else {
        // Multiple PCI host controllers
        for (function = 0; function < 8; function++) {
            if (getVendorID(0, 0, function) != 0xFFFF) break;
            bus = function;
            for(int device = 0; device < 32; device++) {
                checkDevicePCIe(bus, device);
            }
        }
    }

    //Setup PCI devices
    for(int i = 0; i < 65536; i++) {
        struct PCIDevice device = pciDevices[i];

        //Mass storage
        if(device.type == 1) {
            //SATA
            if(device.subclass == 6) {
                //AHCI 1.0
                if(device.progif == 1) {
                    uint16_t command = pcieConfigReadWord(device.bus, device.deviceId, device.function, 0x4);
                    command |= PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MEMORY_SPACE;
                    command &= ~PCI_COMMAND_INTERRUPT_DISABLE;

                    pcieConfigWriteWord(device.bus, device.deviceId, device.function, 0x4, command);

                    uint32_t abar = pcieConfigReadDWord(device.bus, device.deviceId, device.function, 0x24);
                    ahci_setup((void *) abar);
                }
            }
        }
    }
}