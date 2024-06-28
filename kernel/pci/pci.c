//
// Created by Jannik on 05.04.2024.
//

#include "pci.h"
#include <stddef.h>
#include <string.h>
#include "../terminal.h"
#include "../arch/amd64/io.h"
#include "../memmgr.h"

static pci_device_t pciDevices[65536];

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

    uint8_t vendorId = getVendorID(bus, device, function);
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
                printf("PCIe base address: %d\n", mcfg->csba[j].base);
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