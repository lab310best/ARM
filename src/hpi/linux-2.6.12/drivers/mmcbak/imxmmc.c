/*
 *  linux/drivers/mmc/imxmmc.c - Motorola i.MX MMCI driver
 *
 *  Copyright (C) 2004 Sascha Hauer, Pengutronix <sascha@saschahauer.de>
 *
 *  derived from pxamci.c by Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
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
#include <linux/mmc/card.h>
#include <linux/mmc/protocol.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/sizes.h>

#include "imxmmc.h"

#define DRIVER_NAME "IMXMMC"

#ifdef CONFIG_MMC_DEBUG
#define DBG(x...)	printk(KERN_DEBUG x)
#else
#define DBG(x...)	do { } while (0)
#endif

#ifdef CONFIG_MMC_DEBUG
static void dump_cmd(struct mmc_command *cmd)
{
	printk("CMD: opcode: %d ",cmd->opcode);
	printk("arg: 0x%08x ",cmd->arg);
	printk("flags: 0x%08x\n",cmd->flags);
}

static void dump_status(int sts)
{
	printk("status: ");
	if (sts & STATUS_CARD_PRESENCE ) printk("CARD_PRESENCE |");
	if (sts & STATUS_SDIO_INT_ACTIVE ) printk("SDIO_INT_ACTIVE |");
	if (sts & STATUS_END_CMD_RESP ) printk("END_CMD_RESP |");
	if (sts & STATUS_WRITE_OP_DONE ) printk("WRITE_OP_DONE |");
	if (sts & STATUS_DATA_TRANS_DONE ) printk("DATA_TRANS_DONE |");
	if (sts & STATUS_CARD_BUS_CLK_RUN ) printk("CARD_BUS_CLK_RUN |");
	if (sts & STATUS_APPL_BUFF_FF ) printk("APPL_BUFF_FF |");
	if (sts & STATUS_APPL_BUFF_FE ) printk("APPL_BUFF_FE |");
	if (sts & STATUS_RESP_CRC_ERR ) printk("RESP_CRC_ERR |");
	if (sts & STATUS_CRC_READ_ERR ) printk("CRC_READ_ERR |");
	if (sts & STATUS_CRC_WRITE_ERR ) printk("CRC_WRITE_ERR |");
	if (sts & STATUS_TIME_OUT_RESP ) printk("TIME_OUT_RESP |");
	if (sts & STATUS_TIME_OUT_READ ) printk("TIME_OUT_READ |");
	printk("\n");
}
#endif

struct imxmci_host {
	struct mmc_host		*mmc;
	spinlock_t		lock;
	struct resource		*res;
	void			*base;
	int			irq;
	int			dma;
	unsigned int		clkrt;
	unsigned int		cmdat;
	unsigned int		imask;
	unsigned int		power_mode;

	struct mmc_request	*req;
	struct mmc_command	*cmd;
	struct mmc_data		*data;

	dma_addr_t		sg_dma;
	struct imx_dma_desc	*sg_cpu;

	dma_addr_t		dma_buf;
	unsigned int		dma_size;
	unsigned int		dma_dir;
};

/*
 * The base MMC clock rate
 */
#define CLOCKRATE	20000000

static inline unsigned int ns_to_clocks(unsigned int ns)
{
	return (ns * (CLOCKRATE / 1000000) + 999) / 1000;
}

static void imxmci_stop_clock(struct imxmci_host *host)
{
	int i;
	while(1) {
	        MMC_STR_STP_CLK |= STR_STP_CLK_STOP_CLK;
		i = 0;
        	while( (MMC_STATUS & STATUS_CARD_BUS_CLK_RUN) && (i++ < 100));
		
		if ( !(MMC_STATUS & STATUS_CARD_BUS_CLK_RUN) )
			break;
	}
}

static void imxmci_start_clock(struct imxmci_host *host)
{
	int i;
	while(1) {
	        MMC_STR_STP_CLK |= STR_STP_CLK_START_CLK;
		i = 0;
        	while( !(MMC_STATUS & STATUS_CARD_BUS_CLK_RUN) && (i++ < 100));
		
		if ( MMC_STATUS & STATUS_CARD_BUS_CLK_RUN )
			break;
	}
}

static void imxmci_softreset(void)
{
	/* reset sequence */
	MMC_STR_STP_CLK = 0x8;
	MMC_STR_STP_CLK = 0xD;
	MMC_STR_STP_CLK = 0x5;
	MMC_STR_STP_CLK = 0x5;
	MMC_STR_STP_CLK = 0x5;
	MMC_STR_STP_CLK = 0x5;
	MMC_STR_STP_CLK = 0x5;
	MMC_STR_STP_CLK = 0x5;
	MMC_STR_STP_CLK = 0x5;
	MMC_STR_STP_CLK = 0x5;
	
	MMC_RES_TO = 0xff;
	MMC_BLK_LEN = 512;
	MMC_NOB = 1;
}

static void imxmci_setup_data(struct imxmci_host *host, struct mmc_data *data)
{
	unsigned int nob = data->blocks;
	unsigned int ccr = CCR_REN;

	if (data->flags & MMC_DATA_STREAM)
		nob = 0xffff;

	host->data = data;

	MMC_NOB = nob;
	MMC_BLK_LEN = 1 << data->blksz_bits;

	if (data->flags & MMC_DATA_READ) {
		host->dma_dir = DMA_FROM_DEVICE;
		ccr |= CCR_SMOD_FIFO | CCR_SSIZ_16 | CCR_DSIZ_32;
	} else {
		host->dma_dir = DMA_TO_DEVICE;
		ccr |= CCR_DMOD_FIFO | CCR_SSIZ_32 | CCR_DSIZ_16;
	}

	host->dma_size = data->blocks << data->blksz_bits;
	host->dma_buf = dma_map_single(mmc_dev(host->mmc), data->rq->buffer,
				       host->dma_size, host->dma_dir);

	if (data->flags & MMC_DATA_READ) {
		SAR(host->dma) = host->res->start + MMC_BUFFER_ACCESS_OFS;
		DAR(host->dma) = host->dma_buf;
	} else {
		SAR(host->dma) = host->dma_buf;
		DAR(host->dma) = host->res->start + MMC_BUFFER_ACCESS_OFS;
	}

	if( host->mmc->card_selected->type && MMC_TYPE_SD )
		BLR(host->dma) = 0x00;
	else
		BLR(host->dma) = 0x08;
		
	CNTR(host->dma) = host->dma_size;
	CCR(host->dma) = ccr;
	RSSR(host->dma) = DMA_REQ_SDHC;

	/* finally start DMA engine */
	CCR(host->dma) |= CCR_CEN;
}

static void imxmci_start_cmd(struct imxmci_host *host, struct mmc_command *cmd, unsigned int cmdat)
{
	WARN_ON(host->cmd != NULL);
	host->cmd = cmd;

	if (cmd->flags & MMC_RSP_BUSY)
		cmdat |= CMD_DAT_CONT_BUSY;

	switch (cmd->flags & (MMC_RSP_MASK | MMC_RSP_CRC)) {
	case MMC_RSP_SHORT | MMC_RSP_CRC:
		cmdat |= CMD_DAT_CONT_RESPONSE_FORMAT_R1;
		break;
	case MMC_RSP_SHORT:
		cmdat |= CMD_DAT_CONT_RESPONSE_FORMAT_R3;
		break;
	case MMC_RSP_LONG | MMC_RSP_CRC:
		cmdat |= CMD_DAT_CONT_RESPONSE_FORMAT_R2;
		break;
	default:
		break;
	}

        if ( cmd->opcode == MMC_GO_IDLE_STATE )
                cmdat |= CMD_DAT_CONT_INIT; /* This command needs init */

	MMC_CMD = cmd->opcode;
	MMC_ARGH = cmd->arg >> 16;
	MMC_ARGL = cmd->arg & 0xffff;
	MMC_CMD_DAT_CONT = cmdat;

	/* disable card detect interrupt during command */
	host->imask |= INT_MASK_AUTO_CARD_DETECT;
	MMC_INT_MASK = host->imask;

	imxmci_start_clock(host);
}

static void imxmci_finish_request(struct imxmci_host *host, struct mmc_request *req)
{
	/* reenable card detect interrupt */
	host->imask &= ~INT_MASK_AUTO_CARD_DETECT;
	MMC_INT_MASK = host->imask;

	host->req = NULL;
	host->cmd = NULL;
	host->data = NULL;
	mmc_request_done(host->mmc, req);
}

static int imxmci_cmd_done(struct imxmci_host *host, unsigned int stat)
{
	struct mmc_command *cmd = host->cmd;
	int i;
	u32 a,b,c;
	
	if (!cmd)
		return 0;

	host->cmd = NULL;

	if (stat & STATUS_TIME_OUT_RESP) {
		DBG("%s: CMD TIMEOUT\n",DRIVER_NAME);
		cmd->error = MMC_ERR_TIMEOUT;
	} else if (stat & STATUS_RESP_CRC_ERR && cmd->flags & MMC_RSP_CRC) {
		printk("%s: cmd crc error\n",DRIVER_NAME);
		cmd->error = MMC_ERR_BADCRC;
	}

	switch (cmd->flags & (MMC_RSP_MASK | MMC_RSP_CRC)) {
	case MMC_RSP_SHORT | MMC_RSP_CRC:
		a = MMC_RES_FIFO & 0xffff;
		b = MMC_RES_FIFO & 0xffff;
		c = MMC_RES_FIFO & 0xffff;
		cmd->resp[0] = a<<24 | b<<8 | c>>8;
		break;
	case MMC_RSP_SHORT:
		a = MMC_RES_FIFO & 0xffff;
		b = MMC_RES_FIFO & 0xffff;
		c = MMC_RES_FIFO & 0xffff;
		cmd->resp[0] = a<<24 | b<<8 | c>>8;
		break;
	case MMC_RSP_LONG | MMC_RSP_CRC:
		for (i = 0; i < 4; i++) {
			u32 a = MMC_RES_FIFO & 0xffff;
			u32 b = MMC_RES_FIFO & 0xffff;
			cmd->resp[i] = a<<16 | b;
		}
		break;
	default:
		break;
	}

	DBG("%s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",DRIVER_NAME,cmd->resp[0],
	        cmd->resp[1],cmd->resp[2],cmd->resp[3]);


	if (host->data && cmd->error == MMC_ERR_NONE) {
		/* nothing */
	} else {
		imxmci_stop_clock(host);
		imxmci_finish_request(host, host->req);
	}

	return 1;
}

static int imxmci_data_done(struct imxmci_host *host, unsigned int stat)
{
	struct mmc_data *data = host->data;

	CCR(host->dma) &= ~CCR_CEN;

	if (!data)
		return 0;

	dma_unmap_single(mmc_dev(host->mmc), host->dma_buf, host->dma_size,
			 host->dma_dir);

	if ( MMC_STATUS & STATUS_ERR_MASK ) {
		DBG("%s: request failed. status: 0x%08x\n",DRIVER_NAME,MMC_STATUS);
	}

	host->data = NULL;
	data->bytes_xfered = host->dma_size;

	if (host->req->stop && data->error == MMC_ERR_NONE) {
		imxmci_stop_clock(host);
		imxmci_start_cmd(host, host->req->stop, 0);
	} else {
		imxmci_finish_request(host, host->req);
	}

	return 1;
}

static irqreturn_t imxmci_irq(int irq, void *devid, struct pt_regs *regs)
{
	struct imxmci_host *host = devid;
	unsigned int status = 0;
	int handled = 0,timeout = 10000;
	ulong flags;
	
	spin_lock_irqsave(host->lock,flags);

	status = MMC_STATUS;
	
	while( !handled && --timeout ) {
		if( !(host->imask & INT_MASK_AUTO_CARD_DETECT) ) {
			DBG("%s: Card inserted / removed\n",DRIVER_NAME);
			mmc_detect_change(host->mmc);
			handled = 1;
		}
	
		if(status & STATUS_END_CMD_RESP) {
			imxmci_cmd_done(host, status);
			handled = 1;
		}

		status = MMC_STATUS;
	}
	
	if(!timeout) {
		printk("%s: fake interrupt\n",DRIVER_NAME);
		handled = 1;
	}

	MMC_INT_MASK = host->imask;

	spin_unlock_irqrestore(host->lock, flags);

	return IRQ_RETVAL(handled);
}

static void imxmci_request(struct mmc_host *mmc, struct mmc_request *req)
{
	struct imxmci_host *host = mmc_priv(mmc);
	unsigned int cmdat;

	WARN_ON(host->req != NULL);
	
	host->req = req;

	cmdat = 0;

	if (req->data) {
		imxmci_setup_data(host, req->data);
		
		cmdat |= CMD_DAT_CONT_DATA_ENABLE;

		if (req->data->flags & MMC_DATA_WRITE)
			cmdat |= CMD_DAT_CONT_WRITE;

		if (req->data->flags & MMC_DATA_STREAM) {
			cmdat |= CMD_DAT_CONT_STREAM_BLOCK;
		}
	
		if ( host->mmc->card_selected->type & MMC_TYPE_SD )
			cmdat |= CMD_DAT_CONT_BUS_WIDTH_4;
	}

	imxmci_start_cmd(host, req->cmd, cmdat);
}

#define CLK_RATE 19200000

static void imxmci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct imxmci_host *host = mmc_priv(mmc);
	int prescaler;

	DBG("%d: clock %u power %u vdd %u.%02u\n", DRIVER_NAME
	    ios->clock, ios->power_mode, ios->vdd / 100,
	    ios->vdd % 100);

        if ( ios->clock ) {
                prescaler = 5; /* 96MHz / 5 = 19.2 MHz */
                unsigned int clk;

                for(clk=0; clk<8; clk++) {
                        int x;
                        x = CLK_RATE / (1<<clk);
                        if( x <= ios->clock)
                                break;
                }

                MMC_STR_STP_CLK |= STR_STP_CLK_ENABLE; /* enable controller */

                imxmci_stop_clock(host);
                MMC_CLK_RATE = (prescaler<<3) | clk;
                imxmci_start_clock(host);

                DBG("%s:MMC_CLK_RATE: 0x%08x\n",DRIVER_NAME,MMC_CLK_RATE);
        } else {
                imxmci_stop_clock(host);
        }
}

static struct mmc_host_ops imxmci_ops = {
	.request	= imxmci_request,
	.set_ios	= imxmci_set_ios,
};

static struct resource *platform_device_resource(struct platform_device *dev, unsigned int mask, int nr)
{
	int i;

	for (i = 0; i < dev->num_resources; i++)
		if (dev->resource[i].flags == mask && nr-- == 0)
			return &dev->resource[i];
	return NULL;
}

static int platform_device_irq(struct platform_device *dev, int nr)
{
	int i;

	for (i = 0; i < dev->num_resources; i++)
		if (dev->resource[i].flags == IORESOURCE_IRQ && nr-- == 0)
			return dev->resource[i].start;
	return NO_IRQ;
}

static void imxmci_dma_irq(int dma, void *devid, struct pt_regs *regs)
{
	struct imxmci_host *host = devid;
	u32 status;

	DISR |= 1<<dma;

	do {
		status = MMC_STATUS;
	} while( !(status & STATUS_DATA_TRANS_DONE) );

	imxmci_data_done(host, status);
	MMC_INT_MASK = host->imask;
}

static int imxmci_probe(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mmc_host *mmc;
	struct imxmci_host *host = NULL;
	struct resource *r;
	int ret = 0, irq;

	printk("i.MX mmc driver\n",__FUNCTION__);

	r = platform_device_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_device_irq(pdev, 0);
	if (!r || irq == NO_IRQ)
		return -ENXIO;

	r = request_mem_region(r->start, 0x100, "IMXMCI");
	if (!r)
		return -EBUSY;

	mmc = mmc_alloc_host(sizeof(struct imxmci_host), dev);
	if (!mmc) {
		ret = -ENOMEM;
		goto out;
	}

	mmc->ops = &imxmci_ops;
	mmc->f_min = 150000;
	mmc->f_max = 19100000;
	mmc->ocr_avail = MMC_VDD_32_33;

	host = mmc_priv(mmc);
	host->mmc = mmc;
	host->dma = -1;

	host->sg_cpu = dma_alloc_coherent(dev, PAGE_SIZE, &host->sg_dma, GFP_KERNEL);
	if (!host->sg_cpu) {
		ret = -ENOMEM;
		goto out;
	}

	spin_lock_init(&host->lock);
	host->res = r;
	host->irq = irq;

	imx_gpio_mode(PB8_PF_SD_DAT0); /* FIXME: Which direction shall */
	imx_gpio_mode(PB9_PF_SD_DAT1); /* the data lines have? */
	imx_gpio_mode(PB10_PF_SD_DAT2);
	imx_gpio_mode(PB11_PF_SD_DAT3);
	imx_gpio_mode(PB12_PF_SD_CLK);
	imx_gpio_mode(PB13_PF_SD_CMD);

	imxmci_softreset();

	if ( MMC_REV_NO != 0x390 ) {
		printk("%s: wrong rev.no. 0x%08x. aborting.\n",
		                                    DRIVER_NAME,MMC_REV_NO);
		goto out;
	}

	MMC_READ_TO = 0x2db4; /* recommended in data sheet */

	host->imask = INT_MASK_BUF_READY | INT_MASK_DATA_TRAN | 
	              INT_MASK_WRITE_OP_DONE | INT_MASK_SDIO;
	MMC_INT_MASK = host->imask;

	
	host->dma = imx_request_dma(DRIVER_NAME, DMA_PRIO_LOW, imxmci_dma_irq, NULL, host);
	if (host->dma < 0) {
		ret = -EBUSY;
		goto out;
	}

	ret = request_irq(host->irq, imxmci_irq, 0, DRIVER_NAME, host);
	if (ret)
		goto out;

	dev_set_drvdata(dev, host);

	mmc_add_host(mmc);

	return 0;

 out:
	if (host) {
		if (host->dma >= 0)
			imx_free_dma(host->dma);
		if (host->base)
			iounmap(host->base);
		if (host->sg_cpu)
			dma_free_coherent(dev, PAGE_SIZE, host->sg_cpu, host->sg_dma);
	}
	if (mmc)
		mmc_free_host(mmc);
	release_resource(r);
	return ret;
}

static int imxmci_remove(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);

	dev_set_drvdata(dev, NULL);

	if (mmc) {
		struct imxmci_host *host = mmc_priv(mmc);

		mmc_remove_host(mmc);

		free_irq(host->irq, host);
		imx_free_dma(host->dma);
		iounmap(host->base);
		dma_free_coherent(dev, PAGE_SIZE, host->sg_cpu, host->sg_dma);

		release_resource(host->res);

		mmc_free_host(mmc);
	}
	return 0;
}

#ifdef CONFIG_PM
static int imxmci_suspend(struct device *dev, u32 state, u32 level)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	int ret = 0;

	if (mmc && level == SUSPEND_DISABLE)
		ret = mmc_suspend_host(mmc, state);

	return ret;
}

static int imxmci_resume(struct device *dev, u32 level)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	int ret = 0;

	if (mmc && level == RESUME_ENABLE)
		ret = mmc_resume_host(mmc);

	return ret;
}
#else
#define imxmci_suspend  NULL
#define imxmci_resume   NULL
#endif /* CONFIG_PM */

static struct device_driver imxmci_driver = {
	.name		= "imx-mmc",
	.bus		= &platform_bus_type,
	.probe		= imxmci_probe,
	.remove		= imxmci_remove,
	.suspend	= imxmci_suspend,
	.resume		= imxmci_resume,
};

static int __init imxmci_init(void)
{
	return driver_register(&imxmci_driver);
}

static void __exit imxmci_exit(void)
{
	driver_unregister(&imxmci_driver);
}

module_init(imxmci_init);
module_exit(imxmci_exit);

MODULE_DESCRIPTION("i.MX Multimedia Card Interface Driver");
MODULE_AUTHOR("Sascha Hauer");
MODULE_LICENSE("GPL");
