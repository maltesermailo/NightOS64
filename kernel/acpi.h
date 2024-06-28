//
// Created by Jannik on 28.06.2024.
//

#ifndef NIGHTOS_ACPI_H
#define NIGHTOS_ACPI_H

#include <stdint.h>

typedef struct __attribute__ ((packed)) {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
} RSDP_t;

typedef struct  __attribute__ ((packed)) {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;      // deprecated since version 2.0

    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t reserved[3];
} XSDP_t;

typedef struct __attribute__ ((packed, aligned(4))) {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} ACPISDTHeader;

typedef struct __attribute__ ((packed, aligned(4))) {
    ACPISDTHeader h;
    uint64_t PointerToOtherSDT[];
} XSDT;

typedef struct __attribute__ ((packed, aligned(4))) {
    ACPISDTHeader h;
    uint32_t PointerToOtherSDT[];
} RSDT;

typedef struct __attribute__ ((packed, aligned(4))) {
    ACPISDTHeader h;
    uint64_t Reserved;
    struct CSBA {
        uint64_t base;
        uint16_t segment;
        uint8_t startBus;
        uint8_t endBus;
        uint32_t reserved;
    }  csba[];
} MCFG;

#endif //NIGHTOS_ACPI_H
