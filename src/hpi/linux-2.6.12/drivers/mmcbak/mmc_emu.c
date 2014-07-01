/*
 *  linux/drivers/mmc/pxa.c - PXA MMCI driver
 *
 *  Copyright (C) 2003 Russell King, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This hardware is really sick.  No way to clear interrupts.  Have
 *  to turn off the clock whenever we touch the device.  Yuck!
 *
 *	1 and 3 byte data transfers not supported
 *	max block length up to 1023
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/dma-mapping.h>
#include <linux/mmc/host.h>
#include <linux/mmc/protocol.h>

#ifdef CONFIG_MMC_DEBUG
#define DBG(x...)	printk(KERN_DEBUG x)
#else
#define DBG(x...)	do { } while (0)
#endif

struct emu_disk_para {
	u32 sect_size;
	u32 sect_count;
	u32 blk_size;
	u32 ro;
};

struct attach_bdops {
	int (*open)(void *);//(struct inode *, struct file *);
	int (*release)(void *);//(struct inode *, struct file *);
	int (*ioctl)(void *, unsigned int, unsigned long);//(struct inode *, struct file *,	unsigned int, unsigned long);
	ssize_t (*read)(void *, char *, unsigned long);
	ssize_t (*write)(void *, char *, unsigned long);
	int (*do_transfer)(void *, const struct request *);
	int (*proc_rd_entry)(char *);
};

struct pxamci_host {
	struct mmc_host		*mmc;
	spinlock_t		lock;
	unsigned int		power_mode;

	struct mmc_request	*req;
	struct mmc_command	*cmd;
	struct mmc_data		*data;
	
	int state;
	int rca;
	void *disk;
	struct attach_bdops *disk_ops;
	struct emu_disk_para *disk_para;
	struct device *dev;
};

#define	EMU_RCA		host->rca

static void emu_request(struct mmc_host *mmc, struct mmc_request *req)
{
	struct pxamci_host *host = mmc_priv(mmc);
	unsigned int blks, i;
	
	struct mmc_command *cmd = req->cmd;
	struct mmc_data *data = req->data;
	
	switch (cmd->opcode) {
	//class 2	
	case MMC_READ_SINGLE_BLOCK:		//17
		if(host->disk_ops->read(host->disk, 
								data->rq->buffer+data->offset, 
								cmd->arg>>data->blksz_bits))
			data->error = MMC_ERR_BADCRC;
		break;
		
	case MMC_READ_MULTIPLE_BLOCK:	//18
		for(i=0; i<data->blocks; i++)
			if(host->disk_ops->read(host->disk, 
									data->rq->buffer+(i<<data->blksz_bits), 
									cmd->arg>>data->blksz_bits)) {
				data->error = MMC_ERR_TIMEOUT;
				break;
			}
		break;
		
	//class 4	
	case MMC_WRITE_BLOCK:			//24
		if(host->disk_ops->write(host->disk, 
							data->rq->buffer+data->offset, 
							cmd->arg>>data->blksz_bits))
		data->error = MMC_ERR_TIMEOUT;
		break;
		
	case MMC_WRITE_MULTIPLE_BLOCK:	//25
		break;
		
	//class 1
	case MMC_GO_IDLE_STATE:			//0
		host->state = 0;
		break;						//no error. no data, no resp
		
	case MMC_SEND_OP_COND:			//1
		cmd->error = MMC_ERR_TIMEOUT;
		break;						//emulate SD mode, so no resp
		
	case MMC_ALL_SEND_CID:			//2
		if(!host->state)
			goto	set_card_cid;			
		else
			cmd->error = MMC_ERR_TIMEOUT;
		break;
		
	case MMC_SET_RELATIVE_ADDR:		//3
		EMU_RCA = cmd->arg>>16;
		cmd->resp[0] = EMU_RCA<<16;	//return relative address
		host->state = 1;		
		break;
		
	case MMC_SET_DSR:				//4
		break;
		
	case MMC_SELECT_CARD:			//7
		if(cmd->arg) {
			if(cmd->arg==EMU_RCA<<16)
				cmd->resp[0] = 0x800;	//ready state
			else
				cmd->error = MMC_ERR_TIMEOUT;
		}
		break;
		
	case MMC_SEND_CSD:				//9
		if(cmd->arg!=EMU_RCA<<16) {
			cmd->error = MMC_ERR_TIMEOUT;
			break;
		}
		blks = (host->disk_para->sect_size*host->disk_para->sect_count)>>16;	//64K blocks
		cmd->resp[0] = (0<<30)|(0x12<<24)|(1<<19)|(1<<16)|(1<<8)|(6<<3)|2;
		cmd->resp[1] = (0xff<<20)|(9<<16)|(0<<15)|(0<<14)|(0<<13)|(1<<12)|(blks>>2);
		cmd->resp[2] = (blks<<30)|(1<<27)|(1<<24)|(1<<21)|(1<<18)|(5<<15)|(1<<14)|(32<<7)|1;
		cmd->resp[3] = (1<<31)|(0<<29)|(0<<26)|(9<<22)|(0<<21)|(0<<16)|(0<<12)|(0<<8)|1;
		break;
		
	case MMC_SEND_CID:				//10
		if(cmd->arg!=EMU_RCA<<16) {
			cmd->error = MMC_ERR_TIMEOUT;
			break;
		}
set_card_cid:
		cmd->resp[0] = (0xab<<24)|(0xcdef<<8)|'a';	//Manu ID, OEM ID and Prod Name
		cmd->resp[1] = ('n'<<24)|('t'<<16)|('i'<<8)|'s';
		cmd->resp[2] = 0x34|(0x56<<8)|(0x78<<16)|(0x5a<<24);
		cmd->resp[3] = 1|(10<<8)|(4<<12)|(0<<20)|(0x12<<24);
		break;		//2004/10, SN = 0x12345678, Prod Ver = 0x5a

	case MMC_READ_DAT_UNTIL_STOP:	//11
		break;
		
	case MMC_STOP_TRANSMISSION:		//12
		break;
		
	case MMC_SEND_STATUS:			//13
		if(cmd->arg!=EMU_RCA<<16)
			cmd->error = MMC_ERR_TIMEOUT;
		else
			cmd->resp[0] = 1 << 8;	//ready for data
		break;		
		
	case MMC_GO_INACTIVE_STATE:		//14
		break;
		
	//class 2
	case MMC_SET_BLOCKLEN:			//16
		if(cmd->arg!=512)
			cmd->error = MMC_ERR_TIMEOUT;
		break;
		
	//class 3
	case MMC_WRITE_DAT_UNTIL_STOP:	//20
		break;
		
	//class 4
	case MMC_SET_BLOCK_COUNT:		//23
		break;
		
	case MMC_PROGRAM_CID:			//26
		cmd->error = MMC_ERR_TIMEOUT;
		break;
		
	case MMC_PROGRAM_CSD:			//27
		cmd->error = MMC_ERR_TIMEOUT;
		break;
		
	//class 6
	case MMC_SET_WRITE_PROT:		//28
		break;
		
	case MMC_CLR_WRITE_PROT:		//29
		break;
		
	case MMC_SEND_WRITE_PROT:		//30
		cmd->resp[0] = 0;	//return the 32 groups write protect status from
		break;				//the given address
		
	//class 5
	case MMC_ERASE_GROUP_START:		//35
		break;
		
	case MMC_ERASE_GROUP_END:		//36
		break;
		
	case MMC_ERASE:					//37
		break;
		
	//class 9
	case MMC_FAST_IO:				//39
		cmd->error = MMC_ERR_TIMEOUT;
		break;		//don't support
		
	case MMC_GO_IRQ_STATE:			//40
		cmd->error = MMC_ERR_TIMEOUT;
		break;		//don't support
		
	//class 7
	case MMC_LOCK_UNLOCK:			//42
		break;
		
	//class 8
	case MMC_APP_CMD:				//55
		if(cmd->arg==EMU_RCA<<16||!cmd->arg)
			cmd->resp[0] = cmd->arg;
		else
			cmd->error = MMC_ERR_TIMEOUT;
		break;
		
	case MMC_GEN_CMD:				//56
		break;
		
	//SD card specific
	case SD_SEND_OP_COND:			//41
		if(!cmd->arg)				//SD card use this command
			cmd->resp[0] = 0x80ff8000;				//2.7~3.6V
		else
			cmd->resp[0] = cmd->arg | 0x80000000;	//set ready
		break;
		
	case SET_BUS_WIDTH:				//6
		break;
		
	default:
		cmd->error = MMC_ERR_TIMEOUT;
	}
	
	host->req = NULL;
	host->cmd = NULL;
	host->data = NULL;
	mmc_request_done(host->mmc, req);
}

static void emu_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
}

static struct mmc_host_ops mmc_emu_ops = {
	.request	= emu_request,
	.set_ios	= emu_set_ios,
};

void *register_mmc_emu_disk(void *disk, 
						   struct attach_bdops *disk_ops, 
						   struct emu_disk_para *disk_para, 
						   struct device *dev)
{
	struct mmc_host *mmc;
	struct pxamci_host *host = NULL;

	//printk("dev=%p, dma_mask=%08x\n", dev, *dev->dma_mask&0xffffffff);

	mmc = mmc_alloc_host(sizeof(struct pxamci_host), dev);
	if (!mmc) {
		printk(KERN_ERR "fail to alloc mmc host!\n");
		return NULL;
	}
	//the dev initialized in mmc_alloc_host, can't clear after that!!!
	//memset(mmc, 0, sizeof(struct pxamci_host));	//cleard in mmc_alloc_host
	//printk("mmc=%p, mmc->dev=%p\n", mmc, mmc->dev);

	mmc->ops = &mmc_emu_ops;
	mmc->f_min = 312500;
	mmc->f_max = 20000000;
	mmc->ocr_avail = MMC_VDD_32_33;

	host = mmc_priv(mmc);
	host->mmc = mmc;
	host->disk = disk;
	host->disk_ops = disk_ops;
	host->disk_para = disk_para;
	host->rca = 0x10;
	host->dev = dev;

	spin_lock_init(&host->lock);

#ifdef CONFIG_PREEMPT
#error Not Preempt-safe
#endif

	mmc_add_host(mmc);

	return mmc;
}

int unregister_mmc_emu_disk(void *disk)
{
	struct mmc_host *mmc = (struct mmc_host *)disk;

	if (mmc) {
		mmc_remove_host(mmc);
		mmc_free_host(mmc);
	}
	return 0;
}

EXPORT_SYMBOL(register_mmc_emu_disk);
EXPORT_SYMBOL(unregister_mmc_emu_disk);
