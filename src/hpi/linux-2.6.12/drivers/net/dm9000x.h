/*
 * dm9000 Ethernet
 */

#ifndef _DM9000X_H_
#define _DM9000X_H_

#define DM9000_ID		0x90000A46

/* although the registers are 16 bit, they are 32-bit aligned.
 */

#define DMFE_NCR             0x00
#define DMFE_NSR             0x01
#define DMFE_TCR             0x02
#define DMFE_TSR1            0x03
#define DMFE_TSR2            0x04
#define DMFE_RCR             0x05
#define DMFE_RSR             0x06
#define DMFE_ROCR            0x07
#define DMFE_BPTR            0x08
#define DMFE_FCTR            0x09
#define DMFE_FCR             0x0A
#define DMFE_EPCR            0x0B
#define DMFE_EPAR            0x0C
#define DMFE_EPDRL           0x0D
#define DMFE_EPDRH           0x0E
#define DMFE_WCR             0x0F

#define DMFE_PAR             0x10
#define DMFE_MAR             0x16

#define DMFE_GPCR	     0x1e
#define DMFE_GPR             0x1f
#define DMFE_TRPAL           0x22
#define DMFE_TRPAH           0x23
#define DMFE_RWPAL           0x24
#define DMFE_RWPAH           0x25

#define DMFE_VIDL            0x28
#define DMFE_VIDH            0x29
#define DMFE_PIDL            0x2A
#define DMFE_PIDH            0x2B

#define DMFE_CHIPR           0x2C
#define DMFE_SMCR            0x2F

#define DMFE_MRCMDX          0xF0
#define DMFE_MRCMD           0xF2
#define DMFE_MRRL            0xF4
#define DMFE_MRRH            0xF5
#define DMFE_MWCMDX          0xF6
#define DMFE_MWCMD           0xF8
#define DMFE_MWRL            0xFA
#define DMFE_MWRH            0xFB
#define DMFE_TXPLL           0xFC
#define DMFE_TXPLH           0xFD
#define DMFE_ISR             0xFE
#define DMFE_IMR             0xFF

#define NCR_EXT_PHY         (1<<7)
#define NCR_WAKEEN          (1<<6)
#define NCR_FCOL            (1<<4)
#define NCR_FDX             (1<<3)
#define NCR_LBK             (3<<1)
#define NCR_RST	            (1<<0)

#define NSR_SPEED           (1<<7)
#define NSR_LINKST          (1<<6)
#define NSR_WAKEST          (1<<5)
#define NSR_TX2END          (1<<3)
#define NSR_TX1END          (1<<2)
#define NSR_RXOV            (1<<1)

#define TCR_TJDIS           (1<<6)
#define TCR_EXCECM          (1<<5)
#define TCR_PAD_DIS2        (1<<4)
#define TCR_CRC_DIS2        (1<<3)
#define TCR_PAD_DIS1        (1<<2)
#define TCR_CRC_DIS1        (1<<1)
#define TCR_TXREQ           (1<<0)

#define TSR_TJTO            (1<<7)
#define TSR_LC              (1<<6)
#define TSR_NC              (1<<5)
#define TSR_LCOL            (1<<4)
#define TSR_COL             (1<<3)
#define TSR_EC              (1<<2)

#define RCR_WTDIS           (1<<6)
#define RCR_DIS_LONG        (1<<5)
#define RCR_DIS_CRC         (1<<4)
#define RCR_ALL	            (1<<3)
#define RCR_RUNT            (1<<2)
#define RCR_PRMSC           (1<<1)
#define RCR_RXEN            (1<<0)

#define RSR_RF              (1<<7)
#define RSR_MF              (1<<6)
#define RSR_LCS             (1<<5)
#define RSR_RWTO            (1<<4)
#define RSR_PLE             (1<<3)
#define RSR_AE              (1<<2)
#define RSR_CE              (1<<1)
#define RSR_FOE             (1<<0)

#define FCTR_HWOT(ot)	(( ot & 0xf ) << 4 )
#define FCTR_LWOT(ot)	( ot & 0xf )

#define IMR_PAR             (1<<7)
#define IMR_ROOM            (1<<3)
#define IMR_ROM             (1<<2)
#define IMR_PTM             (1<<1)
#define IMR_PRM             (1<<0)

#define ISR_ROOS            (1<<3)
#define ISR_ROS             (1<<2)
#define ISR_PTS             (1<<1)
#define ISR_PRS             (1<<0)
#define ISR_CLR_STATUS      (ISR_ROOS | ISR_ROS | ISR_PTS | ISR_PRS)

#define EPCR_REEP           (1<<5)
#define EPCR_WEP            (1<<4)
#define EPCR_EPOS           (1<<3)
#define EPCR_ERPRR          (1<<2)
#define EPCR_ERPRW          (1<<1)
#define EPCR_ERRE           (1<<0)

#define DM9000_PKT_RDY		0x01	/* Packet ready to receive */
#define DM9000_PKT_MAX		1536	/* Received packet max size */

#endif /* _DM9000X_H_ */

