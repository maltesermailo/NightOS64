//
// Created by Jannik on 29.06.2024.
//

#ifndef NIGHTOS_AHCI_H
#define NIGHTOS_AHCI_H

#include <stdint.h>

typedef enum
{
    FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
    FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
    FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
    FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
    FIS_TYPE_DATA		= 0x46,	// Data FIS - bidirectional
    FIS_TYPE_BIST		= 0x58,	// BIST activate FIS - bidirectional
    FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
    FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
} FIS_TYPE;

typedef struct tagFIS_REG_H2D
{
    // DWORD 0
    uint8_t  fis_type;	// FIS_TYPE_REG_H2D

    uint8_t  pmport:4;	// Port multiplier
    uint8_t  rsv0:3;		// Reserved
    uint8_t  c:1;		// 1: Command, 0: Control

    uint8_t  command;	// Command register
    uint8_t  featurel;	// Feature register, 7:0

    // DWORD 1
    uint8_t  lba0;		// LBA low register, 7:0
    uint8_t  lba1;		// LBA mid register, 15:8
    uint8_t  lba2;		// LBA high register, 23:16
    uint8_t  device;		// Device register

    // DWORD 2
    uint8_t  lba3;		// LBA register, 31:24
    uint8_t  lba4;		// LBA register, 39:32
    uint8_t  lba5;		// LBA register, 47:40
    uint8_t  featureh;	// Feature register, 15:8

    // DWORD 3
    uint8_t  countl;		// Count register, 7:0
    uint8_t  counth;		// Count register, 15:8
    uint8_t  icc;		// Isochronous command completion
    uint8_t  control;	// Control register

    // DWORD 4
    uint8_t  rsv1[4];	// Reserved
} FIS_REG_H2D;

typedef struct tagFIS_REG_D2H
{
    // DWORD 0
    uint8_t  fis_type;    // FIS_TYPE_REG_D2H

    uint8_t  pmport:4;    // Port multiplier
    uint8_t  rsv0:2;      // Reserved
    uint8_t  i:1;         // Interrupt bit
    uint8_t  rsv1:1;      // Reserved

    uint8_t  status;      // Status register
    uint8_t  error;       // Error register

    // DWORD 1
    uint8_t  lba0;        // LBA low register, 7:0
    uint8_t  lba1;        // LBA mid register, 15:8
    uint8_t  lba2;        // LBA high register, 23:16
    uint8_t  device;      // Device register

    // DWORD 2
    uint8_t  lba3;        // LBA register, 31:24
    uint8_t  lba4;        // LBA register, 39:32
    uint8_t  lba5;        // LBA register, 47:40
    uint8_t  rsv2;        // Reserved

    // DWORD 3
    uint8_t  countl;      // Count register, 7:0
    uint8_t  counth;      // Count register, 15:8
    uint8_t  rsv3[2];     // Reserved

    // DWORD 4
    uint8_t  rsv4[4];     // Reserved
} FIS_REG_D2H;

typedef struct tagFIS_DATA
{
    // DWORD 0
    uint8_t  fis_type;	// FIS_TYPE_DATA

    uint8_t  pmport:4;	// Port multiplier
    uint8_t  rsv0:4;		// Reserved

    uint8_t  rsv1[2];	// Reserved

    // DWORD 1 ~ N
    uint32_t data[1];	// Payload
} FIS_DATA;

typedef struct tagFIS_PIO_SETUP
{
    // DWORD 0
    uint8_t  fis_type;	// FIS_TYPE_PIO_SETUP

    uint8_t  pmport:4;	// Port multiplier
    uint8_t  rsv0:1;		// Reserved
    uint8_t  d:1;		// Data transfer direction, 1 - device to host
    uint8_t  i:1;		// Interrupt bit
    uint8_t  rsv1:1;

    uint8_t  status;		// Status register
    uint8_t  error;		// Error register

    // DWORD 1
    uint8_t  lba0;		// LBA low register, 7:0
    uint8_t  lba1;		// LBA mid register, 15:8
    uint8_t  lba2;		// LBA high register, 23:16
    uint8_t  device;		// Device register

    // DWORD 2
    uint8_t  lba3;		// LBA register, 31:24
    uint8_t  lba4;		// LBA register, 39:32
    uint8_t  lba5;		// LBA register, 47:40
    uint8_t  rsv2;		// Reserved

    // DWORD 3
    uint8_t  countl;		// Count register, 7:0
    uint8_t  counth;		// Count register, 15:8
    uint8_t  rsv3;		// Reserved
    uint8_t  e_status;	// New value of status register

    // DWORD 4
    uint16_t tc;		// Transfer count
    uint8_t  rsv4[2];	// Reserved
} FIS_PIO_SETUP;

typedef struct tagFIS_DMA_SETUP
{
    // DWORD 0
    uint8_t  fis_type;	// FIS_TYPE_DMA_SETUP

    uint8_t  pmport:4;	// Port multiplier
    uint8_t  rsv0:1;		// Reserved
    uint8_t  d:1;		// Data transfer direction, 1 - device to host
    uint8_t  i:1;		// Interrupt bit
    uint8_t  a:1;            // Auto-activate. Specifies if DMA Activate FIS is needed

    uint8_t  rsved[2];       // Reserved

    //DWORD 1&2

    uint64_t DMAbufferID;    // DMA Buffer Identifier. Used to Identify DMA buffer in host memory.
    // SATA Spec says host specific and not in Spec. Trying AHCI spec might work.

    //DWORD 3
    uint32_t rsvd;           //More reserved

    //DWORD 4
    uint32_t DMAbufOffset;   //Byte offset into buffer. First 2 bits must be 0

    //DWORD 5
    uint32_t TransferCount;  //Number of bytes to transfer. Bit 0 must be 0

    //DWORD 6
    uint32_t resvd;          //Reserved

} FIS_DMA_SETUP;

typedef uint16_t FIS_DEV_BITS;

typedef volatile struct tagHBA_FIS
{
    // 0x00
    FIS_DMA_SETUP	dsfis;		// DMA Setup FIS
    uint8_t         pad0[4];

    // 0x20
    FIS_PIO_SETUP	psfis;		// PIO Setup FIS
    uint8_t         pad1[12];

    // 0x40
    FIS_REG_D2H	rfis;		// Register – Device to Host FIS
    uint8_t         pad2[4];

    // 0x58
    FIS_DEV_BITS	sdbfis;		// Set Device Bit FIS

    // 0x60
    uint8_t         ufis[64];

    // 0xA0
    uint8_t   	rsv[0x100-0xA0];
} HBA_FIS;

typedef struct tagHBA_CMD_HEADER
{
    // DW0
    uint8_t  cfl:5;		// Command FIS length in DWORDS, 2 ~ 16
    uint8_t  a:1;		// ATAPI
    uint8_t  w:1;		// Write, 1: H2D, 0: D2H
    uint8_t  p:1;		// Prefetchable

    uint8_t  r:1;		// Reset
    uint8_t  b:1;		// BIST
    uint8_t  c:1;		// Clear busy upon R_OK
    uint8_t  rsv0:1;		// Reserved
    uint8_t  pmp:4;		// Port multiplier port

    uint16_t prdtl;		// Physical region descriptor table length in entries

    // DW1
    volatile
    uint32_t prdbc;		// Physical region descriptor byte count transferred

    // DW2, 3
    uint32_t ctba;		// Command table descriptor base address
    uint32_t ctbau;		// Command table descriptor base address upper 32 bits

    // DW4 - 7
    uint32_t rsv1[4];	// Reserved
} HBA_CMD_HEADER;

typedef volatile struct tagHBA_PORT
{
    uint32_t clb;		// 0x00, command list base address, 1K-byte aligned
    uint32_t clbu;		// 0x04, command list base address upper 32 bits
    uint32_t fb;		// 0x08, FIS base address, 256-byte aligned
    uint32_t fbu;		// 0x0C, FIS base address upper 32 bits
    uint32_t is;		// 0x10, interrupt status
    uint32_t ie;		// 0x14, interrupt enable
    uint32_t cmd;		// 0x18, command and status
    uint32_t rsv0;		// 0x1C, Reserved
    uint32_t tfd;		// 0x20, task file data
    uint32_t sig;		// 0x24, signature
    uint32_t ssts;		// 0x28, SATA status (SCR0:SStatus)
    uint32_t sctl;		// 0x2C, SATA control (SCR2:SControl)
    uint32_t serr;		// 0x30, SATA error (SCR1:SError)
    uint32_t sact;		// 0x34, SATA active (SCR3:SActive)
    uint32_t ci;		// 0x38, command issue
    uint32_t sntf;		// 0x3C, SATA notification (SCR4:SNotification)
    uint32_t fbs;		// 0x40, FIS-based switch control
    uint32_t rsv1[11];	// 0x44 ~ 0x6F, Reserved
    uint32_t vendor[4];	// 0x70 ~ 0x7F, vendor specific
} HBA_PORT;

typedef volatile struct tagHBA_MEM_CAP {
    uint32_t S64A : 1; //Whether 64-bit addresses are supported
    uint32_t SNCQ : 1; //Supports native command queueing
    uint32_t SSNTF: 1; //Supports Snotification Register
    uint32_t SMPS : 1; //Supports Mechanical Presence Switch
    uint32_t SSS  : 1; //Supports Staggered Spin-Up
    uint32_t SALP : 1; //Supports Aggressive Link Power Management
    uint32_t SAL  : 1; //Supports Activity LED
    uint32_t SCLO : 1; //Supports Command List Override
    uint32_t ISS  : 4; //Interface Speed Support; 0x1 = Gen1(1.5 Gpbs), 0x2 = Gen2 (3 Gbps), 0x3 = Gen3 (6 Gbps)
    uint32_t res0 : 1;
    uint32_t SAM  : 1; //Supports only AHCI mode
    uint32_t SPM  : 1; //Supports Port Multiplier
    uint32_t FBSS : 1; //Supports FIS-based switching
    uint32_t PMD  : 1; //PIO Multiple DRQ Block
    uint32_t SSC  : 1; //Slumber State capable
    uint32_t PSC  : 1; //Partial state capable;
    uint32_t NCS  : 5; //Number of command slots per port
    uint32_t CCCS : 1; //Command completeion coalescing supported
    uint32_t EMS  : 1; //Enclosure Managment Supported
    uint32_t SXS  : 1; //Supports External SATA
    uint32_t NP   : 5; //Number of ports
} HBA_MEM_CAP;

typedef volatile struct tagHBA_MEM_GHC {
    uint32_t AE   : 1; //AHCI Enable bit
    uint32_t res0 : 28; //Reserved
    uint32_t MSRM : 1; //MSI Revert to Single (read only)
    uint32_t IE   : 1; //Interrupt Enable
    uint32_t HR   : 1; //HBA Software Reset
} HBA_MEM_GHC;

typedef volatile struct tagHBA_MEM_CCC_CTL {
    uint32_t tv   : 16; //Timeout value
    uint32_t cc   : 8;  //Command completions
    uint32_t intr : 5; //Interrupt
    uint32_t res0 : 2; //Reserved
    uint32_t en   : 1; //Enable
} HBA_MEM_CCC_CTL;

typedef volatile struct tagHBA_MEM_EM_LOC {
    uint32_t off  : 16; //Offset
    uint32_t bs   : 16; //Buffer size
} HBA_MEM_EM_LOC;

typedef volatile struct tagHBA_MEM_EM_CTL {
    uint32_t res0 : 4; //Reserved
    uint32_t pm   : 1; //Attrib: Port Multiplier Support
    uint32_t alhd : 1; //Attrib: Activity LED Hardware driven
    uint32_t xmt  : 1; //Attrib: Transmit only
    uint32_t smb  : 1; //Attrib:  Single Message Buffer
    uint32_t res1 : 4; //Reserved
    uint32_t sgpio: 1; //Supports SGPIO messages
    uint32_t ses2 : 1; //Supports SES2 messages
    uint32_t safte: 1; //Supports SAF-TE messages
    uint32_t led  : 1; //Supports LED Messages
    uint32_t res2 : 6; //Reserved
    uint32_t rst  : 1; //Control: Reset
    uint32_t tm   : 1; //Control:Transmit Message
    uint32_t res3 : 8; //Reserved
    uint32_t mr   : 1; //Message received
} HBA_MEM_EM_CTL;

typedef volatile struct tagHBA_MEM_CAP2 {
    uint32_t res0 : 26;
    uint32_t DESO : 1; //DevSleep Entrance from Slumber only
    uint32_t SADM : 1; //Supports Aggressive Device Sleep Management
    uint32_t SDS  : 1; //Supports Device Sleep
    uint32_t APST : 1; //Automatic Partial to Slumber Transitions
    uint32_t NVMP : 1; //NVMHCI Present
    uint32_t BOH  : 1; //BIOS/OS Handoff supported
} HBA_MEM_CAP2;

typedef volatile struct tagHBA_MEM_BOHC {
    uint32_t res0 : 27;
    uint32_t BB   : 1; //Supports Aggressive Device Sleep Management
    uint32_t OOC  : 1; //Supports Device Sleep
    uint32_t SOOE : 1; //Automatic Partial to Slumber Transitions
    uint32_t OOS  : 1; //NVMHCI Present
    uint32_t BOS  : 1; //BIOS/OS Handoff supported
} HBA_MEM_BOHC;

typedef volatile struct tagHBA_MEM
{
    // 0x00 - 0x2B, Generic Host Control
    HBA_MEM_CAP cap;		// 0x00, Host capability
    HBA_MEM_GHC ghc;		// 0x04, Global host control
    uint32_t is;		// 0x08, Interrupt status
    uint32_t pi;		// 0x0C, Port implemented
    uint16_t major_vs;	// 0x10, Version
    uint16_t minor_vs;
    HBA_MEM_CCC_CTL ccc_ctl;	// 0x14, Command completion coalescing control
    uint32_t ccc_pts;	// 0x18, Command completion coalescing ports
    HBA_MEM_EM_LOC em_loc;		// 0x1C, Enclosure management location
    HBA_MEM_EM_CTL em_ctl;		// 0x20, Enclosure management control
    HBA_MEM_CAP2 cap2;		// 0x24, Host capabilities extended
    HBA_MEM_BOHC bohc;		// 0x28, BIOS/OS handoff control and status

    // 0x2C - 0x9F, Reserved
    uint8_t  rsv[0xA0-0x2C];

    // 0xA0 - 0xFF, Vendor specific registers
    uint8_t  vendor[0x100-0xA0];

    // 0x100 - 0x10FF, Port control registers
    HBA_PORT	ports[1];	// 1 ~ 32
} HBA_MEM;


typedef struct tagHBA_PRDT_ENTRY
{
    uint32_t dba;		// Data base address
    uint32_t dbau;		// Data base address upper 32 bits
    uint32_t rsv0;		// Reserved

    // DW3
    uint32_t dbc:22;		// Byte count, 4M max
    uint32_t rsv1:9;		// Reserved
    uint32_t i:1;		// Interrupt on completion
} HBA_PRDT_ENTRY;

typedef struct tagHBA_CMD_TBL
{
    // 0x00
    uint8_t  cfis[64];	// Command FIS

    // 0x40
    uint8_t  acmd[16];	// ATAPI command, 12 or 16 bytes

    // 0x50
    uint8_t  rsv[48];	// Reserved

    // 0x80
    HBA_PRDT_ENTRY	prdt_entry[1];	// Physical region descriptor table entries, 0 ~ 65535
} HBA_CMD_TBL;

void ahci_setup(void* abar);
#endif //NIGHTOS_AHCI_H
