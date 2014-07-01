/*
 *   dm9000.c: Version 1.2 03/18/2003
 *   
 *         A Davicom DM9000 ISA NIC fast Ethernet driver for Linux.
 * 	Copyright (C) 1997  Sten Wang
 * 
 * 	This program is free software; you can redistribute it and/or
 * 	modify it under the terms of the GNU General Public License
 * 	as published by the Free Software Foundation; either version 2
 * 	of the License, or (at your option) any later version.
 * 
 * 	This program is distributed in the hope that it will be useful,
 * 	but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * 	GNU General Public License for more details.
 * 
 *   (C)Copyright 1997-1998 DAVICOM Semiconductor,Inc. All Rights Reserved.
 * 
 * V0.11	06/20/2001	REG_0A bit3=1, default enable BP with DA match
 * 	06/22/2001 	Support DM9801 progrmming	
 * 	 	 	E3: R25 = ((R24 + NF) & 0x00ff) | 0xf000
 * 		 	E4: R25 = ((R24 + NF) & 0x00ff) | 0xc200
 * 		     		R17 = (R17 & 0xfff0) | NF + 3
 * 		 	E5: R25 = ((R24 + NF - 3) & 0x00ff) | 0xc200
 * 		     		R17 = (R17 & 0xfff0) | NF
 * 				
 * v1.00               	modify by simon 2001.9.5
 *                         change for kernel 2.4.x    
 * 			
 * v1.1   11/09/2001      	fix force mode bug             
 * 
 * v1.2   03/18/2003       Weilun Huang <weilun_huang@davicom.com.tw>: 
 * 			Fixed phy reset.
 * 			Added tx/rx 32 bit mode.
 * 			Cleaned up for kernel merge.
 *
 *        03/03/2004    Sascha Hauer <saschahauer@web.de>
 *                      Port to 2.6 kernel
 *                      
 *	  24-Sep-2004   Ben Dooks <ben@simtec.co.uk>
 *			Cleanup of code to remove ifdefs
 *			Allowed platform device data to influence access width
 *			Reformatting areas of code

 */

#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/dm9000.h>

#include <asm/delay.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "dm9000x.h"

#include <asm-arm/arch/irqs.h>
#include <asm-arm/arch/hardware.h>
#include <asm-arm/arch/regs-gpio.h> 

/* Board/System/Debug information/definition ---------------- */

static void *bwscon;
static void *gpfcon;
static void *extint0;
static void *intmsk;
#define BWSCON           (0x48000000)
#define GPFCON           (0x56000050)
#define EXTINT0           (0x56000088)
#define INTMSK           (0x4A000008)

#define DM9000_REG00		0x00
#define DM9000_REG05		0x30	/* SKIP_CRC/SKIP_LONG */
#define DM9000_REG08		0x27
#define DM9000_REG09		0x38
#define DM9000_REG0A		0xff
#define DM9000_REGFF		0x83	/* IMR */

#define DM9000_PHY		0x40	/* PHY address 0x01 */

#define TRUE			1
#define FALSE			0

#define CARDNAME "dm9000"
#define PFX CARDNAME ": "

#define DMFE_TIMER_WUT  jiffies+(HZ*2)	/* timer wakeup time : 2 second */
#define DMFE_TX_TIMEOUT (HZ*2)	/* tx packet time-out time 1.5 s" */

#define DMFE_DEBUG 0

#if DMFE_DEBUG > 2
#define PRINTK3(args...)  printk(CARDNAME ": " args)
#else
#define PRINTK3(args...)  do { } while(0)
#endif

#if DMFE_DEBUG > 1
#define PRINTK2(args...)  printk(CARDNAME ": " args)
#else
#define PRINTK2(args...)  do { } while(0)
#endif

#if DMFE_DEBUG > 0
#define PRINTK1(args...)  printk(CARDNAME ": " args)
#define PRINTK(args...)   printk(CARDNAME ": " args)
#else
#define PRINTK1(args...)  do { } while(0)
#define PRINTK(args...)   printk(KERN_DEBUG args)
#endif

enum DM9000_PHY_mode {
	DM9000_10MHD = 0,
	DM9000_100MHD = 1,
	DM9000_10MFD = 4,
	DM9000_100MFD = 5,
	DM9000_AUTO = 8,
	DM9000_1M_HPNA = 0x10
};

/* Structure/enum declaration ------------------------------- */
typedef struct board_info {

	u32 runt_length_counter;	/* counter: RX length < 64byte */
	u32 long_length_counter;	/* counter: RX length > 1514byte */
	u32 reset_counter;	/* counter: RESET */
	u32 reset_tx_timeout;	/* RESET caused by TX Timeout */
	u32 reset_rx_status;	/* RESET caused by RX Statsus wrong */

	void __iomem *io_addr;	/* Register I/O base address */
	void __iomem *io_data;	/* Data I/O address */
	u16 irq;		/* IRQ */

	u16 tx_pkt_cnt;
	u16 queue_pkt_len;
	u16 queue_start_addr;
	u16 dbug_cnt;
	u8 reg0, reg5, reg8, reg9, rega;	/* registers saved */
	u8 op_mode;		/* PHY operation mode */
	u8 io_mode;		/* 0:word, 2:byte */
	u8 phy_addr;
	u8 device_wait_reset;	/* device state */

	void (*inblk)(void __iomem *port, void *data, int length);
	void (*outblk)(void __iomem *port, void *data, int length);
	void (*dumpblk)(void __iomem *port, int length);

	struct resource	*addr_res;   /* resources found */
	struct resource *data_res;
	struct resource	*addr_req;   /* resources requested */
	struct resource *data_req;
	struct resource *irq_res;

	struct timer_list timer;
	struct net_device_stats stats;
	unsigned char srom[128];
	spinlock_t lock;

	struct mii_if_info mii;
	u32 msg_enable;

} board_info_t;

/* Global variable declaration ----------------------------- */
static int dmfe_debug = 0;

/* For module input parameter */
static int debug = 0;
static int mode = DM9000_AUTO;
static int media_mode = DM9000_AUTO;
static u8 reg5 = DM9000_REG05;
static u8 reg8 = DM9000_REG08;
static u8 reg9 = DM9000_REG09;
static u8 rega = DM9000_REG0A;
static u8 nfloor = 0;

#ifdef CONFIG_MX1_SCB9328
#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>
#include <asm/arch/scb9328.h>
#else
static u8 irqline = 3;
#endif

/* function declaration ------------------------------------- */
static int dmfe_probe(struct device *);
static int dmfe_open(struct net_device *);
static int dmfe_start_xmit(struct sk_buff *, struct net_device *);
static int dmfe_stop(struct net_device *);
static int dmfe_do_ioctl(struct net_device *, struct ifreq *, int);


static void dmfe_timer(unsigned long);
static void dmfe_init_dm9000(struct net_device *);

static struct net_device_stats *dmfe_get_stats(struct net_device *);

static irqreturn_t dmfe_interrupt(int, void *, struct pt_regs *);

static int dmfe_phy_read(struct net_device *dev, int phyaddr_unsused, int reg);
static void dmfe_phy_write(struct net_device *dev, int phyaddr_unused, int reg,
			   int value);
static u16 read_srom_word(board_info_t *, int);
static void dmfe_rx(struct net_device *);
static void dm9000_hash_table(struct net_device *);

//#define DM9000_PROGRAM_EEPROM
#ifdef DM9000_PROGRAM_EEPROM
static void program_eeprom(board_info_t * db);
#endif
/* DM9000 network board routine ---------------------------- */

static void
dmfe_reset(board_info_t * db)
{
	PRINTK1("dm9000x: resetting\n");
	/* RESET device */
	writeb(DMFE_NCR, db->io_addr);
	udelay(200);		/* delay 100us */
	writeb(NCR_RST, db->io_data);
	udelay(200);		/* delay 100us */
}

/*
 *   Read a byte from I/O port
 */
static u8
ior(board_info_t * db, int reg)
{
	writeb(reg, db->io_addr);
	return readb(db->io_data);
}

/*
 *   Write a byte to I/O port
 */

static void
iow(board_info_t * db, int reg, int value)
{
	writeb(reg, db->io_addr);
	writeb(value, db->io_data);
}

/* routines for sending block to chip */

static void dm9000_outblk_8bit(void __iomem *reg, void *data, int count)
{
	writesb(reg, data, count);
}

static void dm9000_outblk_16bit(void __iomem *reg, void *data, int count)
{
	writesw(reg, data, (count+1) >> 1);
}

static void dm9000_outblk_32bit(void __iomem *reg, void *data, int count)
{
	writesl(reg, data, (count+3) >> 2);
}

/* input block from chip to memory */

static void dm9000_inblk_8bit(void __iomem *reg, void *data, int count)
{
	readsb(reg, data, count+1);
}


static void dm9000_inblk_16bit(void __iomem *reg, void *data, int count)
{
	readsw(reg, data, (count+1) >> 1);
}

static void dm9000_inblk_32bit(void __iomem *reg, void *data, int count)
{
	readsl(reg, data, (count+3) >> 2);
}

/* dump block from chip to null */

static void dm9000_dumpblk_8bit(void __iomem *reg, int count)
{
	int i;
	int tmp;

	for (i = 0; i < count; i++)
		tmp = readb(reg);
}

static void dm9000_dumpblk_16bit(void __iomem *reg, int count)
{
	int i;
	int tmp;

	count = (count + 1) >> 1;

	for (i = 0; i < count; i++)
		tmp = readw(reg);
}

static void dm9000_dumpblk_32bit(void __iomem *reg, int count)
{
	int i;
	int tmp;

	count = (count + 3) >> 2;

	for (i = 0; i < count; i++)
		tmp = readl(reg);
}

/* dmfe_set_io
 *
 * select the specified set of io routines to use with the
 * device
*/

static void dmfe_set_io(struct board_info *db, int byte_width) 
{
	/* use the size of the data resource to work out what IO
	 * routines we want to use
	 */

	switch (byte_width) {
	case 1:
		db->dumpblk = dm9000_dumpblk_8bit;
		db->outblk  = dm9000_outblk_8bit;
		db->inblk   = dm9000_inblk_8bit;
		break;

	case 2:
		db->dumpblk = dm9000_dumpblk_16bit;
		db->outblk  = dm9000_outblk_16bit;
		db->inblk   = dm9000_inblk_16bit;
		break;
			
	case 3:
		printk(KERN_ERR PFX ": 3 byte IO, falling back to 16bit\n");
		db->dumpblk = dm9000_dumpblk_16bit;
		db->outblk  = dm9000_outblk_16bit;
		db->inblk   = dm9000_inblk_16bit;
		break;			

	case 4:
	default:
		db->dumpblk = dm9000_dumpblk_32bit;
		db->outblk  = dm9000_outblk_32bit;
		db->inblk   = dm9000_inblk_32bit;
		break;
	}
}


/* dmfe_release_board
 *
 * release a board, and any mapped resources
*/

static void
dmfe_release_board(struct platform_device *pdev, struct board_info *db)
{
	if (db->data_res == NULL) {
		if (db->addr_res != NULL)
			release_mem_region((unsigned long)db->io_addr, 4);
		return;
	}

	/* unmap our resources */
	
	iounmap(db->io_addr);
	iounmap(db->io_data);

	/* release the resources */

	if (db->data_req != NULL) {
		release_resource(db->data_req);
		kfree(db->data_req);
	}

	if (db->addr_res != NULL) {
		release_resource(db->data_req);
		kfree(db->addr_req);
	}
}

#define res_size(_r) (((_r)->end - (_r)->start) + 1)

/*
  Search DM9000 board, allocate space and register it
*/
static int
dmfe_probe(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dm9000_plat_data *pdata = pdev->dev.platform_data;
	struct board_info *db;	/* Point a board information structure */
	struct net_device *ndev;
	unsigned long base;
	int ret = 0;
	int iosize;
	int i;
	u32 id_val;
	unsigned char ne_def_eth_mac_addr[]={0x00,0x12,0x34,0x56,0x80,0x49};

	bwscon=ioremap_nocache(BWSCON,0x0000004);
	gpfcon=ioremap_nocache(GPFCON,0x0000004);
	extint0=ioremap_nocache(EXTINT0,0x0000004);
	intmsk=ioremap_nocache(INTMSK,0x0000004);
	               
	writel(readl(bwscon)|0xc0000,bwscon);
	writel( (readl(gpfcon) & ~(0x3 << 14)) | (0x2 << 14), gpfcon); 
	writel( readl(gpfcon) | (0x1 << 7), gpfcon); // Disable pull-up
	writel( (readl(extint0) & ~(0xf << 28)) | (0x4 << 28), extint0); //rising edge
	writel( (readl(intmsk))  & ~0x80, intmsk);   

	printk(KERN_INFO "%s Ethernet Driver\n", CARDNAME);

	/* Init network device */
	ndev = alloc_etherdev(sizeof (struct board_info));
	if (!ndev) {
		printk("%s: could not allocate device.\n", CARDNAME);
		return -ENOMEM;
	}

	SET_MODULE_OWNER(ndev);
	SET_NETDEV_DEV(ndev, dev);

	PRINTK2("dmfe_probe()");

	/* setup board info structure */
	db = (struct board_info *) ndev->priv;
	memset(db, 0, sizeof (*db));

	if (pdev->num_resources < 2) {
		ret = -ENODEV;
		goto out;
	}

	switch (pdev->num_resources) {
	case 2:
		base = pdev->resource[0].start;

		if (!request_mem_region(base, 4, ndev->name)) {
			ret = -EBUSY;
			goto out;
		}

		ndev->base_addr = base;
		ndev->irq = pdev->resource[1].start;		
		db->io_addr = (void *)base;
		db->io_data = (void *)(base + 4);

#ifdef CONFIG_MX1_SCB9328
		db->outblk  = dm9000_outblk_16bit;
		db->inblk   = dm9000_inblk_16bit;
		db->dumpblk = dm9000_dumpblk_16bit;
#endif
		break;

	case 3:
		db->addr_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		db->data_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		db->irq_res  = platform_get_resource(pdev, IORESOURCE_IRQ, 0);

		if (db->addr_res == NULL || db->data_res == NULL) {
			printk(KERN_ERR PFX "insufficient resources\n");
			ret = -ENOENT;
			goto out;
		}

		i = res_size(db->addr_res);
		db->addr_req = request_mem_region(db->addr_res->start, i,
						  pdev->name);

		if (db->addr_req == NULL) {
			printk(KERN_ERR PFX "cannot claim address reg area\n");
			ret = -EIO;
			goto out;
		}

		db->io_addr = ioremap(db->addr_res->start, i);

		if (db->io_addr == NULL) {
			printk(KERN_ERR "failed to ioremap address reg\n");
			ret = -EINVAL;
			goto out;
		}

		iosize = res_size(db->data_res);
		db->data_req = request_mem_region(db->data_res->start, iosize,
						  pdev->name);

		if (db->data_req == NULL) {
			printk(KERN_ERR PFX "cannot claim data reg area\n");
			ret = -EIO;
			goto out;
		}

		db->io_data = ioremap(db->data_res->start, iosize);

		if (db->io_data == NULL) {
			printk(KERN_ERR "failed to ioremap data reg\n");
			ret = -EINVAL;
			goto out;
		}

		/* fill in parameters for net-dev structure */

		ndev->base_addr = (unsigned long)db->io_addr;
		ndev->irq	= db->irq_res->start;

		/* ensure at least we have a default set of IO routines */
		dmfe_set_io(db, iosize);

		/* check to see if anything is being over-ridden */
		if (pdata != NULL) {
			/* check to see if the driver wants to over-ride the
			 * default IO width */

			if (pdata->flags & DM9000_PLATF_8BITONLY)
				dmfe_set_io(db, 1);

			if (pdata->flags & DM9000_PLATF_16BITONLY)
				dmfe_set_io(db, 2);

			if (pdata->flags & DM9000_PLATF_32BITONLY)
				dmfe_set_io(db, 4);

			/* check to see if there are any IO routine
			 * over-rides */

			if (pdata->inblk != NULL)
				db->inblk = pdata->inblk;

			if (pdata->outblk != NULL)
				db->outblk = pdata->outblk;

			if (pdata->dumpblk != NULL)
				db->dumpblk = pdata->dumpblk;
		}
	}


	dmfe_reset(db);

	/* try two times, DM9000 sometimes gets the first read wrong */
	for (i = 0; i < 2; i++) {
		id_val  = ior(db, DMFE_VIDL);
		id_val |= (u32)ior(db, DMFE_VIDH) << 8;
		id_val |= (u32)ior(db, DMFE_PIDL) << 16;
		id_val |= (u32)ior(db, DMFE_PIDH) << 24;

		if (id_val == DM9000_ID)
			break;
		printk("%s: read wrong id 0x%08x\n", CARDNAME, id_val);
	}

	if (id_val != DM9000_ID) {
		printk("%s: wrong id: 0x%08x\n", CARDNAME, id_val);
		goto release;
	}

	/* from this point we assume that we have found a DM9000 */

	/* driver system function */
	ether_setup(ndev);

	ndev->open		 = &dmfe_open;
	ndev->hard_start_xmit    = &dmfe_start_xmit;
	ndev->stop		 = &dmfe_stop;
	ndev->get_stats		 = &dmfe_get_stats;
	ndev->set_multicast_list = &dm9000_hash_table;
	ndev->do_ioctl		 = &dmfe_do_ioctl;

#ifdef DM9000_PROGRAM_EEPROM
	program_eeprom(db);
#endif
	db->msg_enable       = NETIF_MSG_LINK;
	db->mii.phy_id_mask  = 0x1f;
	db->mii.reg_num_mask = 0x1f;
	db->mii.force_media  = 0;
	db->mii.full_duplex  = 0;
	db->mii.dev	     = ndev;
	db->mii.mdio_read    = dmfe_phy_read;
	db->mii.mdio_write   = dmfe_phy_write;

	/* Read SROM content */
	for (i = 0; i < 64; i++)
		((u16 *) db->srom)[i] = read_srom_word(db, i);

	/* Set Node Address */
	for (i = 0; i < 6; i++)
		ndev->dev_addr[i] = ne_def_eth_mac_addr[i];

	if (!is_valid_ether_addr(ndev->dev_addr))
		printk("%s: Invalid ethernet MAC address.  Please "
		       "set using ifconfig\n", ndev->name);

	dev_set_drvdata(dev, ndev);
	ret = register_netdev(ndev);

	if (ret == 0) {
		printk("%s: dm9000 at %p,%p IRQ %d MAC: ",
		       ndev->name,  db->io_addr, db->io_data, ndev->irq);
		for (i = 0; i < 5; i++)
			printk("%02x:", ndev->dev_addr[i]);
		printk("%02x\n", ndev->dev_addr[5]);
	}
	return 0;

 release:
 out:
	printk("%s: not found (%d).\n", CARDNAME, ret);

	dmfe_release_board(pdev, db);
	kfree(ndev);

	return ret;
}

/*
 *  Open the interface.
 *  The interface is opened whenever "ifconfig" actives it.
 */
static int
dmfe_open(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;

	PRINTK2("entering dmfe_open\n");

	if (request_irq(dev->irq, &dmfe_interrupt, SA_SHIRQ, dev->name, dev))
		return -EAGAIN;
	/* Disable all interrupts */
	iow(db, DMFE_IMR, IMR_PAR);

#ifdef CONFIG_MX1_SCB9328
	set_irq_type(SCB9328_ETH_IRQ, __IRQT_LOWLVL);
#endif

	/* Initialize DM9000 board */
	dmfe_reset(db);
	dmfe_init_dm9000(dev);

	/* Init driver variable */
	db->dbug_cnt = 0;
	db->runt_length_counter = 0;
	db->long_length_counter = 0;
	db->reset_counter = 0;
	db->device_wait_reset = 0;

	/* set and active a timer process */
	init_timer(&db->timer);
	db->timer.expires  = DMFE_TIMER_WUT * 2;
	db->timer.data     = (unsigned long) dev;
	db->timer.function = &dmfe_timer;
	add_timer(&db->timer);

	mii_check_media(&db->mii, netif_msg_link(db), 1);
	netif_start_queue(dev);
	/* Re-enable interrupt mask */
	iow(db, DMFE_IMR, IMR_PAR | IMR_PTM | IMR_PRM);

	return 0;
}

/* 
 * Set PHY operating mode
 */
static void
set_PHY_mode(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;
	u16 phy_reg4 = 0x01e1, phy_reg0 = 0x1000;

	if (!(db->op_mode & DM9000_AUTO)) {

		switch (db->op_mode) {
		case DM9000_10MHD:
			phy_reg4 = ADVERTISE_10HALF | ADVERTISE_CSMA;
			phy_reg0 = 0;
			break;
		case DM9000_10MFD:
			phy_reg4 = ADVERTISE_10FULL | ADVERTISE_CSMA;
			phy_reg0 = BMCR_ANENABLE | BMCR_FULLDPLX;
			break;
		case DM9000_100MHD:
			phy_reg4 = ADVERTISE_100HALF | ADVERTISE_CSMA;
			phy_reg0 = BMCR_SPEED100;
			break;
		case DM9000_100MFD:
			phy_reg4 = ADVERTISE_100FULL | ADVERTISE_CSMA;
			phy_reg0 =
			    BMCR_SPEED100 | BMCR_ANENABLE | BMCR_FULLDPLX;
			break;
		}
		dmfe_phy_write(dev, 0, MII_ADVERTISE, phy_reg4);	/* Set PHY media mode */
		dmfe_phy_write(dev, 0, MII_BMCR, phy_reg0);	/*  Tmp */
	}

	iow(db, DMFE_GPCR, 0x01);	/* Let GPIO0 output */
	iow(db, DMFE_GPR, 0x00);	/* Enable PHY */
}

/* 
 * Initilize dm9000 board
 */
static void
dmfe_init_dm9000(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;

	PRINTK1("entering %s\n",__FUNCTION__);

	/* I/O mode */
	db->io_mode = ior(db, DMFE_ISR) >> 6;	/* ISR bit7:6 keeps I/O mode */

	/* GPIO0 on pre-activate PHY */
	iow(db, DMFE_GPR, 0x00);	/*REG_1F bit0 activate phyxcer */

	/* Set PHY */
	db->op_mode = media_mode;
	set_PHY_mode(dev);

	/* Init needed register value */
	db->reg0 = DM9000_REG00;

	/* User passed argument */
	db->reg5 = reg5;
	db->reg8 = reg8;
	db->reg9 = reg9;
	db->rega = rega;

	/* Program operating register */
	iow(db, DMFE_NCR, db->reg0);
	iow(db, DMFE_TCR, 0);	        /* TX Polling clear */
	iow(db, DMFE_BPTR, 0x3f);	    /* Less 3Kb, 200us */
	iow(db, DMFE_FCTR, db->reg9);	/* Flow Control : High/Low Water */
	iow(db, DMFE_FCR, db->rega);	/* Flow Control */
	iow(db, DMFE_SMCR, 0);	        /* Special Mode */
	iow(db, DMFE_NSR, 0x2c);	    /* clear TX status */
	iow(db, DMFE_ISR, ISR_CLR_STATUS); /* Clear interrupt status */

	/* Set address filter table */
	dm9000_hash_table(dev);

	/* Activate DM9000 */
	iow(db, DMFE_RCR, db->reg5 | 1);	/* RX enable */
	iow(db, DMFE_IMR, DM9000_REGFF);	/* Enable TX/RX interrupt mask */

	/* Init Driver variable */
	db->tx_pkt_cnt = 0;
	db->queue_pkt_len = 0;
	dev->trans_start = 0;
	spin_lock_init(&db->lock);
}

/*
 *  Hardware start transmission.
 *  Send a packet to media from the upper layer.
 */
static int
dmfe_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;

	PRINTK3("dmfe_start_xmit\n");

	if (db->tx_pkt_cnt > 1)
		return 1;

	netif_stop_queue(dev);

	/* Disable all interrupts */
	iow(db, DMFE_IMR, IMR_PAR);

	/* Move data to DM9000 TX RAM */
	writeb(DMFE_MWCMD, db->io_addr);

	(db->outblk)(db->io_data, skb->data, skb->len);

	/* TX control: First packet immediately send, second packet queue */
	if (db->tx_pkt_cnt == 0) {

		/* First Packet */
		db->tx_pkt_cnt++;

		/* Set TX length to DM9000 */
		iow(db, DMFE_TXPLL, skb->len & 0xff);
		iow(db, DMFE_TXPLH, (skb->len >> 8) & 0xff);

		/* Issue TX polling command */
		iow(db, DMFE_TCR, TCR_TXREQ);	/* Cleared after TX complete */

		dev->trans_start = jiffies;	/* saved the time stamp */

	} else {
		/* Second packet */
		db->tx_pkt_cnt++;
		db->queue_pkt_len = skb->len;
	}

	/* free this SKB */
	dev_kfree_skb(skb);

	/* Re-enable resource check */
	if (db->tx_pkt_cnt == 1)
		netif_wake_queue(dev);

	/* Re-enable interrupt */
	iow(db, DMFE_IMR, IMR_PAR | IMR_PTM | IMR_PRM);

	return 0;
}

static void
dmfe_shutdown(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;

	/* RESET devie */
	dmfe_phy_write(dev, 0, MII_BMCR, BMCR_RESET);	/* PHY RESET */
	iow(db, DMFE_GPR, 0x01);	/* Power-Down PHY */
	iow(db, DMFE_IMR, IMR_PAR);	/* Disable all interrupt */
	iow(db, DMFE_RCR, 0x00);	/* Disable RX */
}

/*
  Stop the interface.
  The interface is stopped when it is brought.
*/
static int
dmfe_stop(struct net_device *ndev)
{
	board_info_t *db = (board_info_t *) ndev->priv;

	PRINTK1("entering %s\n",__FUNCTION__);

	/* deleted timer */
	del_timer(&db->timer);

	netif_stop_queue(ndev);
	netif_carrier_off(ndev);

	/* free interrupt */
	free_irq(ndev->irq, ndev);

	dmfe_shutdown(ndev);

	return 0;
}

/*
  DM9000 interrupt handler
  receive the packet to upper layer, free the transmitted packet
*/

void
dmfe_tx_done(struct net_device *dev, board_info_t * db)
{
	int tx_status = ior(db, DMFE_NSR);	/* Got TX status */

	if (tx_status & 0xc) {
		/* One packet sent complete */
		db->tx_pkt_cnt--;
		dev->trans_start = 0;
		db->stats.tx_packets++;

		/* Queue packet check & send */
		if (db->tx_pkt_cnt > 0) {
			iow(db, DMFE_TXPLL, db->queue_pkt_len & 0xff);
			iow(db, DMFE_TXPLH, (db->queue_pkt_len >> 8) & 0xff);
			iow(db, DMFE_TCR, TCR_TXREQ);
			dev->trans_start = jiffies;
		}
		netif_wake_queue(dev);
	}
}

static irqreturn_t
dmfe_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = dev_id;
	board_info_t *db;
	int int_status;
	u8 reg_save;
//	scb9328_ledon(4);
	PRINTK3("entering %s\n",__FUNCTION__);

	if (!dev) {
		PRINTK1("dmfe_interrupt() without DEVICE arg\n");
		return IRQ_HANDLED;
	}
   
	/* A real interrupt coming */
	db = (board_info_t *) dev->priv;
	spin_lock(&db->lock);

	/* Save previous register address */
	reg_save = readb(db->io_addr);

	/* Disable all interrupts */
	iow(db, DMFE_IMR, IMR_PAR);

	/* Got DM9000 interrupt status */
	int_status = ior(db, DMFE_ISR);	/* Got ISR */
	iow(db, DMFE_ISR, int_status);	/* Clear ISR status */

	/* Received the coming packet */
	if (int_status & ISR_PRS)
		dmfe_rx(dev);

	/* Trnasmit Interrupt check */
	if (int_status & ISR_PTS)
		dmfe_tx_done(dev, db);

	/* Re-enable interrupt mask */
	iow(db, DMFE_IMR, IMR_PAR | IMR_PTM | IMR_PRM);

	/* Restore previous register address */
	writeb(reg_save, db->io_addr);

	spin_unlock(&db->lock);

//	scb9328_ledoff(4);
	return IRQ_HANDLED;
}

/*
 *  Get statistics from driver.
 */
static struct net_device_stats *
dmfe_get_stats(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;
	return &db->stats;
}

/*
 *  Process the upper socket ioctl command
 */
static int
dmfe_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	PRINTK1("entering %s\n",__FUNCTION__);
	return 0;
}

/*
 *  A periodic timer routine
 *  Dynamic media sense, allocated Rx buffer...
 */
static void
dmfe_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *) data;
	board_info_t *db = (board_info_t *) dev->priv;
	u8 reg_save;

	PRINTK3("dmfe_timer()\n");

	/* Save previous register address */
	reg_save = readb(db->io_addr);

	/* TX timeout check */
	if (dev->trans_start
	    && ((jiffies - dev->trans_start) > DMFE_TX_TIMEOUT)) {
		//printk("tx timeout\n");
		db->device_wait_reset = 1;
		db->reset_tx_timeout++;
	}

	/* DM9000 dynamic RESET check and do */
	if (db->device_wait_reset) {
		netif_stop_queue(dev);
		db->reset_counter++;
		db->device_wait_reset = 0;
		dev->trans_start = 0;
		dmfe_reset(db);
		dmfe_init_dm9000(dev);
		netif_wake_queue(dev);
	}

	mii_check_media(&db->mii, netif_msg_link(db), 0);

	/* Restore previous register address */
	writeb(reg_save, db->io_addr);

	/* Set timer again */
	db->timer.expires = DMFE_TIMER_WUT;
	add_timer(&db->timer);
}

#if 0
/* dump a packet to screen */
static void
dump_packet(unsigned char *buf, int len)
{
	int i = 0;
	printk("\n------------------------\n");
	while (i < len) {
		printk("%02x ", buf[i]);
		i++;
		if (!(i % 8))
			printk("\n");
	}

	printk("----------------------\n");
}
#endif

struct dm9000_rxhdr {
	u16	RxStatus;
	u16	RxLen;
} __attribute__((__packed__));

/*
 *  Received a packet and pass to upper layer
 */
static void
dmfe_rx(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;
	struct dm9000_rxhdr rxhdr;
	struct sk_buff *skb;
	u8 rxbyte, *rdptr;
	int GoodPacket;
	int RxLen;

	/* Check packet ready or not */
	do {
		ior(db, DMFE_MRCMDX);	/* Dummy read */
		
		/* Get most updated data */
		rxbyte = readb(db->io_data);

		/* Status check: this byte must be 0 or 1 */
		if (rxbyte > DM9000_PKT_RDY) {
			printk("status check failed: %d\n", rxbyte);
			iow(db, DMFE_RCR, 0x00);	/* Stop Device */
			iow(db, DMFE_ISR, IMR_PAR);	/* Stop INT request */
			db->device_wait_reset = TRUE;
			db->reset_rx_status++;
			return;
		}

		if (rxbyte != DM9000_PKT_RDY)
			return;

		/* A packet ready now  & Get status/length */
		GoodPacket = TRUE;
		writeb(DMFE_MRCMD, db->io_addr);

		(db->inblk)(db->io_data, &rxhdr, sizeof(rxhdr));

		RxLen = rxhdr.RxLen;

		/* Packet Status check */
		if (RxLen < 0x40) {
			GoodPacket = FALSE;
			db->runt_length_counter++;
			PRINTK1("Bad Packet received (runt)\n");
		}

		if (RxLen > DM9000_PKT_MAX) {
			PRINTK1("RST: RX Len:%x\n", RxLen);
			db->device_wait_reset = TRUE;
			db->long_length_counter++;
		}

		if (rxhdr.RxStatus & 0xbf00) {
			GoodPacket = FALSE;
			if (rxhdr.RxStatus & 0x100) {
				PRINTK1("fifo error\n");
				db->stats.rx_fifo_errors++;
			}
			if (rxhdr.RxStatus & 0x200) {
				PRINTK1("crc error\n");
				db->stats.rx_crc_errors++;
			}
			if (rxhdr.RxStatus & 0x8000) {
				PRINTK1("length error\n");
				db->stats.rx_length_errors++;
			}
		}

		/* Move data from DM9000 */
		if (GoodPacket
		    && ((skb = dev_alloc_skb(RxLen + 4)) != NULL)) {
			skb->dev = dev;
			skb_reserve(skb, 2);
			rdptr = (u8 *) skb_put(skb, RxLen - 4);
			
			/* Read received packet from RX SRAM */
			
			(db->inblk)(db->io_data, rdptr, RxLen);

			/* Pass to upper layer */
			skb->protocol = eth_type_trans(skb, dev);
			netif_rx(skb);
			db->stats.rx_packets++;

		} else {
			/* need to dump the packet's data */
			
			(db->dumpblk)(db->io_data, RxLen);
		}
	} while (rxbyte == DM9000_PKT_RDY);
}

/*
 *  Read a word data from SROM
 */
static u16
read_srom_word(board_info_t * db, int offset)
{
	iow(db, DMFE_EPAR, offset);
	iow(db, DMFE_EPCR, EPCR_ERPRR);
	udelay(2000);		/* according to the datasheet 200us should be enough,
				   but it doesn't work */
	iow(db, DMFE_EPCR, 0x0);
	return (ior(db, DMFE_EPDRL) + (ior(db, DMFE_EPDRH) << 8));
}

#ifdef DM9000_PROGRAM_EEPROM
/*
 * Write a word data to SROM
 */
static void
write_srom_word(board_info_t * db, int offset, u16 val)
{
	iow(db, DMFE_EPAR, offset);
	iow(db, DMFE_EPDRH, ((val >> 8) & 0xff));
	iow(db, DMFE_EPDRL, (val & 0xff));
	iow(db, DMFE_EPCR, EPCR_WEP | EPCR_ERPRW);
	udelay(2000);		/* same shit */
	iow(db, DMFE_EPCR, 0);
}

/*
 * Only for development:
 * Here we write static data to the eeprom in case
 * we don't have valid content on a new board
 */
static void
program_eeprom(board_info_t * db)
{
	u16 eeprom[] = { 0x0c00, 0x007f, 0x1300,	/* MAC Address */
		0x0000,		/* Autoload: accept nothing */
		0x0a46, 0x9000,	/* Vendor / Product ID */
		0x0000,		/* pin control */
		0x0000,
	};			/* Wake-up mode control */
	int i;
	for (i = 0; i < 8; i++)
		write_srom_word(db, i, eeprom[i]);
}
#endif


/*
 *  Calculate the CRC valude of the Rx packet
 *  flag = 1 : return the reverse CRC (for the received packet CRC)
 *         0 : return the normal CRC (for Hash Table index)
 */

static unsigned long
cal_CRC(unsigned char *Data, unsigned int Len, u8 flag)
{

       u32 crc = ether_crc_le(Len, Data);

       if (flag)
               return ~crc;

       return crc;
}

/*
 *  Set DM9000 multicast address
 */
static void
dm9000_hash_table(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;
	struct dev_mc_list *mcptr = dev->mc_list;
	int mc_cnt = dev->mc_count;
	u32 hash_val;
	u16 i, oft, hash_table[4];

	PRINTK2("dm9000_hash_table()\n");

	for (i = 0, oft = 0x10; i < 6; i++, oft++)
		iow(db, oft, dev->dev_addr[i]);

	/* Clear Hash Table */
	for (i = 0; i < 4; i++)
		hash_table[i] = 0x0;

	/* broadcast address */
	hash_table[3] = 0x8000;

	/* the multicast address in Hash Table : 64 bits */
	for (i = 0; i < mc_cnt; i++, mcptr = mcptr->next) {
		hash_val = cal_CRC((char *) mcptr->dmi_addr, 6, 0) & 0x3f;
		hash_table[hash_val / 16] |= (u16) 1 << (hash_val % 16);
	}

	/* Write the hash table to MAC MD table */
	for (i = 0, oft = 0x16; i < 4; i++) {
		iow(db, oft++, hash_table[i] & 0xff);
		iow(db, oft++, (hash_table[i] >> 8) & 0xff);
	}
}


/*
 *   Read a word from phyxcer
 */
static int
dmfe_phy_read(struct net_device *dev, int phy_reg_unused, int reg)
{
	board_info_t *db = (board_info_t *) dev->priv;

	/* Fill the phyxcer register into REG_0C */
	iow(db, DMFE_EPAR, DM9000_PHY | reg);

	iow(db, DMFE_EPCR, 0xc);	/* Issue phyxcer read command */
	udelay(100);		/* Wait read complete */
	iow(db, DMFE_EPCR, 0x0);	/* Clear phyxcer read command */

	/* The read data keeps on REG_0D & REG_0E */
	return (ior(db, DMFE_EPDRH) << 8) | ior(db, DMFE_EPDRL);
}

/*
 *   Write a word to phyxcer
 */
static void
dmfe_phy_write(struct net_device *dev, int phyaddr_unused, int reg, int value)
{
	board_info_t *db = (board_info_t *) dev->priv;
	/* Fill the phyxcer register into REG_0C */
	iow(db, DMFE_EPAR, DM9000_PHY | reg);

	/* Fill the written data into REG_0D & REG_0E */
	iow(db, DMFE_EPDRL, (value & 0xff));
	iow(db, DMFE_EPDRH, ((value >> 8) & 0xff));

	iow(db, DMFE_EPCR, 0xa);	/* Issue phyxcer write command */
	udelay(500);		/* Wait write complete */
	iow(db, DMFE_EPCR, 0x0);	/* Clear phyxcer write command */
}

MODULE_PARM(debug, "i");
MODULE_PARM(mode, "i");
MODULE_PARM(reg5, "i");
MODULE_PARM(reg9, "i");
MODULE_PARM(rega, "i");
MODULE_PARM(nfloor, "i");

static int
dmfe_drv_suspend(struct device *dev, u32 state, u32 level)
{
	struct net_device *ndev = dev_get_drvdata(dev);

	if (ndev && level == SUSPEND_DISABLE) {
		if (netif_running(ndev)) {
			netif_device_detach(ndev);
			dmfe_shutdown(ndev);
		}
	}
	return 0;
}

static int
dmfe_drv_resume(struct device *dev, u32 level)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	board_info_t *db = (board_info_t *) ndev->priv;

	if (ndev && level == RESUME_ENABLE) {

		if (netif_running(ndev)) {
			dmfe_reset(db);
			dmfe_init_dm9000(ndev);

			netif_device_attach(ndev);
		}
	}
	return 0;
}

static int
dmfe_drv_remove(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *ndev = dev_get_drvdata(dev);

	dev_set_drvdata(dev, NULL);

	unregister_netdev(ndev);
	dmfe_release_board(pdev, (board_info_t *) ndev->priv);
	kfree(ndev);		/* free device structure */

	PRINTK1("clean_module() exit\n");

	return 0;
}

static struct device_driver dmfe_driver = {
	.name    = "dm9000",
	.bus     = &platform_bus_type,
	.probe   = dmfe_probe,
	.remove  = dmfe_drv_remove,
	.suspend = dmfe_drv_suspend,
	.resume  = dmfe_drv_resume,
};

static int __init
dmfe_init(void)
{
	if (debug)
		dmfe_debug = debug;	/* set debug flag */

	switch (mode) {
	case DM9000_10MHD:
	case DM9000_100MHD:
	case DM9000_10MFD:
	case DM9000_100MFD:
	case DM9000_1M_HPNA:
		media_mode = mode;
		break;
	default:
		media_mode = DM9000_AUTO;
	}

	nfloor = (nfloor > 15) ? 0 : nfloor;

	return driver_register(&dmfe_driver);	/* search board and register */
}

static void __exit
dmfe_cleanup(void)
{
	driver_unregister(&dmfe_driver);
}

module_init(dmfe_init);
module_exit(dmfe_cleanup);

MODULE_AUTHOR("Sascha Hauer, Ben Dooks");
MODULE_DESCRIPTION("Davicom DM9000 network driver");
MODULE_LICENSE("GPL");
