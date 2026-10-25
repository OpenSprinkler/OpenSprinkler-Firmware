/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2023-2024 NanjingQinhengMicroelectronics CO LTD
 * SPDX-FileContributor: 2024 Sergey Kharenko
 * SPDX-FileContributor: 2024-2025 Espressif Systems (Shanghai) CO LTD
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
 * Register definition
 */

#define CH390_NCR       0x00   // Network control reg
#define NCR_WAKEEN      (1<<6) // Enable wakeup function
#define NCR_FDX         (1<<3) // Duplex mode of the internal PHY
#define NCR_LBK_MAC     (1<<1) // MAC loop-back
#define NCR_RST         (1<<0) // Software reset

#define CH390_NSR       0x01   // Network status reg
#define NSR_SPEED       (1<<7) // Speed of internal PHY
#define NSR_LINKST      (1<<6) // Link status of internal PHY
#define NSR_WAKEST      (1<<5) // Wakeup event status
#define NSR_TX2END      (1<<3) // Tx packet B complete status
#define NSR_TX1END      (1<<2) // Tx packet A complete status
#define NSR_RXOV        (1<<1) // Rx fifo overflow
#define NSR_RXRDY       (1<<0)

#define CH390_TCR       0x02   // Transmit control reg
#define TCR_TJDIS       (1<<6) // Transmit jabber timer
#define TCR_PAD_DIS2    (1<<4) // PAD appends for packet B
#define TCR_CRC_DIS2    (1<<3) // CRC appends for packet B
#define TCR_PAD_DIS1    (1<<2) // PAD appends for packet A
#define TCR_CRC_DIS1    (1<<1) // CRC appends for packet A
#define TCR_TXREQ       (1<<0) // Tx request

#define CH390_TSRA      0x03   // Transmit status reg A
#define CH390_TSRB      0x04   // Transmit status reg B
#define TSR_TJTO        (1<<7) // Transmit jabber time out
#define TSR_LC          (1<<6) // Loss of carrier
#define TSR_NC          (1<<5) // No carrier
#define TSR_LCOL        (1<<4) // Late collision
#define TSR_COL         (1<<3) // Collision packet
#define TSR_EC          (1<<2) // Excessive collision

#define CH390_RCR       0x05   // Receive control reg
#define RCR_DEFAULT     0x00   // Default settings
#define RCR_WTDIS       (1<<6) // Disable 2048 bytes watch dog
#define RCR_DIS_CRC     (1<<4) // Discard CRC error packet
#define RCR_ALL         (1<<3) // Pass all multicast
#define RCR_RUNT        (1<<2) // Pass runt packet
#define RCR_PRMSC       (1<<1) // Promiscuous mode
#define RCR_RXEN        (1<<0) // Enable RX

#define CH390_RSR       0x06   // Receive status reg
#define RSR_RF          (1<<7) // Rnt frame
#define RSR_MF          (1<<6) // Multicast frame
#define RSR_LCS         (1<<5) // Late collision seen
#define RSR_RWTO        (1<<4) // Receive watchdog time-out
#define RSR_PLE         (1<<3) // Physical layer error
#define RSR_AE          (1<<2) // Alignment error
#define RSR_CE          (1<<1) // CRC error
#define RSR_FOE         (1<<0) // FIFO overflow error

//Receive status error mask(default)
#define RSR_ERR_MASK    (RSR_RF | RSR_LCS | RSR_RWTO | RSR_PLE | \
                        RSR_AE | RSR_CE | RSR_FOE)

#define CH390_ROCR      0x07   // Receive overflow count reg
#define CH390_BPTR      0x08   // Back pressure threshold reg
#define CH390_FCTR      0x09   // Flow control threshold reg
#define FCTR_HWOT(overflow_th)   (( overflow_th & 0xf ) << 4)
#define FCTR_LWOT(overflow_th)   ( overflow_th & 0xf )

#define CH390_FCR       0x0A   // Transmit/Receive flow control reg
#define FCR_FLOW_ENABLE (0x39) // Enable Flow Control

#define CH390_EPCR      0x0B   // EEPROM or PHY control reg
#define EPCR_REEP       (1<<5) // Reload EEPROM
#define EPCR_WEP        (1<<4) // Write EEPROM enable
#define EPCR_EPOS       (1<<3) // EEPROM or PHY operation select
#define EPCR_ERPRR      (1<<2) // EEPROM or PHY read command
#define EPCR_ERPRW      (1<<1) // EEPROM or PHY write command
#define EPCR_ERRE       (1<<0) // EEPROM or PHY access status

#define CH390_EPAR      0x0C   // EEPROM or PHY address reg

#define CH390_EPDRL     0x0D   // EEPROM or PHY low byte data reg
#define CH390_EPDRH     0x0E   // EEPROM or PHY high byte data reg

#define CH390_WCR       0x0F   // Wakeup control reg
#define WCR_LINKEN      (1<<5) // Link status change wakeup
#define WCR_SAMPLEEN    (1<<4) // Sample frame wakeup
#define WCR_MAGICEN     (1<<3) // Magic packet wakeup
#define WCR_LINKST      (1<<2) // Link status change event
#define WCR_SAMPLEST    (1<<1) // Sample frame event
#define WCR_MAGICST     (1<<0) // Magic packet event

#define CH390_PAR       0x10   // MAC address reg
#define CH390_MAR       0x16   // Multicast address reg
#define CH390_GPCR      0x1E   // General purpose control reg
#define CH390_GPR       0x1F   // General purpose reg
#define CH390_TRPAL     0x22   // Transmit read pointer low byte address reg
#define CH390_TRPAH     0x23   // Transmit read pointer high byte address reg
#define CH390_RWPAL     0x24   // Receive write pointer low byte address reg
#define CH390_RWPAH     0x25   // Receive write pointer high byte address reg
#define CH390_VIDL      0x28   // Vendor ID low byte reg
#define CH390_VIDH      0x29   // Vendor ID high byte reg
#define CH390_PIDL      0x2A   // Product ID low byte reg
#define CH390_PIDH      0x2B   // Product ID high byte reg
#define CH390_CHIPR     0x2C   // Chip reversion reg

#define CH390_TCR2      0x2D   // Transmit control reg II
#define TCR2_RLCP         (1<<6) // Retry Late Collision Packet

#define CH390_ATCR      0x30    // Auto-Transmit control reg
#define ATCR_AUTO_TX      (1<<7) // Auto-Transmit Control

#define CH390_TCSCR     0x31    // Transmit checksum and control reg
#define TCSCR_ALL         0x1F
#define TCSCR_IPv6TCPCSE  (1<<4) // IPv6 TCP checksum generation
#define TCSCR_IPv6UDPCSE  (1<<3) // IPv6 UDP checksum generation
#define TCSCR_UDPCSE      (1<<2) // UDP checksum generation
#define TCSCR_TCPCSE      (1<<1) // TCP checksum generation
#define TCSCR_IPCSE       (1<<0) // IP checksum generation

#define CH390_RCSCSR    0x32   // Receive checksum and control reg
#define RCSCSR_UDPS     (1<<7) // UDP checksum status
#define RCSCSR_TCPS     (1<<6) // TCP checksum status
#define RCSCSR_IPS      (1<<5) // IP checksum status
#define RCSCSR_UDPP     (1<<4) // UDP packet of current received packet
#define RCSCSR_TCPP     (1<<3) // TCP packet of current received packet
#define RCSCSR_IPP      (1<<2) // IP packet of current received packet
#define RCSCSR_RCSEN    (1<<1) // Receive checksum checking enable
#define RCSCSR_DCSE     (1<<0) // Discard checksum error packet

#define CH390_MPAR      0x33   // MII PHY address reg
#define CH390_SBCR      0x38   // SPI bus control reg

#define CH390_INTCR     0x39   // INT pin control reg
#define INCR_TYPE_OD    0x02   // Open drain output
#define INCR_TYPE_PP    0x00   // Push pull output
#define INCR_POL_L      0x01   // Low level positive
#define INCR_POL_H      0x00   // High level positive

#define CH390_ALNCR     0x4A   // SPI alignment error count reg
#define CH390_SCCR      0x50   // System clock control reg
#define CH390_RSCCR     0x51   // Recover system clock control reg

#define CH390_RLENCR            0x52   // Receive data pack length control reg
#define RLENCR_RXLEN_EN         0x80   // Enable RX data pack length filter
#define RLENCR_RXLEN_DEFAULT    0x18   // Default MAX length of RX data(div by 64)

#define CH390_BCASTCR   0x53   // Receive broadcast control reg
#define CH390_INTCKCR   0x54   // INT pin clock output control reg

#define CH390_MPTRCR    0x55   // Memory pointer control reg
#define MPTRCR_RST_TX   (1<<1) // Reset TX Memory Pointer
#define MPTRCR_RST_RX   (1<<0) // Reset RX Memory Pointer

#define CH390_MLEDCR    0x57   // More LED control reg
#define CH390_MRCMDX    0x70   // Memory read command without address increment reg
// Memory read command without data pre-fetch and address increment reg
#define CH390_MRCMDX1   0x71
#define CH390_MRCMD     0x72   // Memory data read command with address increment reg
#define CH390_MRRL      0x74   // Memory read low byte address reg
#define CH390_MRRH      0x75   // Memory read high byte address reg
#define CH390_MWCMDX    0x76   // Memory write command without address increment reg
#define CH390_MWCMD     0x78   // Memory write command
#define CH390_MWRL      0x7A   // Memory write low byte address reg
#define CH390_MWRH      0x7B   // Memory write high byte address reg
#define CH390_TXPLL     0x7C   // Transmit pack low byte length reg
#define CH390_TXPLH     0x7D   // Transmit pack high byte length reg

#define CH390_ISR       0x7E   // Interrupt status reg
#define ISR_LNKCHG      (1<<5)  // Link status change
#define ISR_ROO         (1<<3)  // Receive overflow counter overflow
#define ISR_ROS         (1<<2)  // Receive overflow
#define ISR_PT          (1<<1)  // Packet transmitted
#define ISR_PR          (1<<0)  // Packet received
#define ISR_CLR_STATUS (ISR_LNKCHG | ISR_ROO | ISR_ROS | ISR_PT | ISR_PR)

#define CH390_IMR       0x7F    // Interrupt mask reg
#define IMR_NONE        0x00    // Disable all interrupt
#define IMR_ALL         0xFF    // Enable all interrupt
#define IMR_PAR         (1<<7)  // Pointer auto-return mode
#define IMR_LNKCHGI     (1<<5)  // Enable link status change interrupt
#define IMR_UDRUNI      (1<<4)  // Enable transmit under-run interrupt
#define IMR_ROOI        (1<<3)  // Enable receive overflow counter overflow interrupt
#define IMR_ROI         (1<<2)  // Enable receive overflow interrupt
#define IMR_PTI         (1<<1)  // Enable packet transmitted interrupt
#define IMR_PRI         (1<<0)  // Enable packet received interrupt

// SPI commands
#define OPC_REG_W       0x80  // Register Write
#define OPC_REG_R       0x00  // Register Read
#define OPC_MEM_DMY_R   0x70  // Memory Dummy Read
#define OPC_MEM_WRITE   0xF8  // Memory Write
#define OPC_MEM_READ    0x72  // Memory Read

#define CH390_SPI_RD    0
#define CH390_SPI_WR    1

// GPIO
#define CH390_GPIO1     0x02
#define CH390_GPIO2     0x04
#define CH390_GPIO3     0x08

// PHY registers
#define CH390_PHY          0x40
#define CH390_PHY_BMCR     0x00
#define CH390_PHY_BMSR     0x01
#define CH390_PHY_PHYID1   0x02
#define CH390_PHY_PHYID2   0x03
#define CH390_PHY_ANAR     0x04
#define CH390_PHY_ANLPAR   0x05
#define CH390_PHY_ANER     0x06
#define CH390_PHY_PAGE_SEL 0x1F

// Packet status
#define CH390_PKT_NONE              0x00    /* No packet received */
#define CH390_PKT_RDY               0x01    /* Packet ready to receive */
#define CH390_PKT_ERR               0xFE    /* Un-stable states mask */
#define CH390_PKT_ERR_WITH_RCSEN    0xE2    /* Un-stable states mask when RCSEN = 1 */

#ifdef __cplusplus
}
#endif
