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

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/sizes.h>

#include "pxamci.h"

#ifdef CONFIG_MMC_DEBUG
#define DBG(x...)	printk(KERN_DEBUG x)
#else
#define DBG(x...)	do { } while (0)
#endif

struct pxamci_host {
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
	pxa_dma_desc		*sg_cpu;

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

static void pxamci_stop_clock(struct pxamci_host *host)
{
	if (readl(host->base + MMC_STAT) & STAT_CLK_EN) {
		unsigned long flags;
		unsigned int v;

		writel(STOP_CLOCK, host->base + MMC_STRPCL);

		/*
		 * Wait for the "clock has stopped" interrupt.
		 * We need to unmask the interrupt to receive
		 * the notification.  Sigh.
		 */
		spin_lock_irqsave(&host->lock, flags);
		writel(host->imask & ~CLK_IS_OFF, host->base + MMC_I_MASK);
		do {
			v = readl(host->base + MMC_I_REG);
		} while (!(v & CLK_IS_OFF));
		writel(host->imask, host->base + MMC_I_MASK);
		spin_unlock_irqrestore(&host->lock, flags);
	}
}

static void pxamci_enable_irq(struct pxamci_host *host, unsigned int mask)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	host->imask &= ~mask;
	writel(host->imask, host->base + MMC_I_MASK);
	spin_unlock_irqrestore(&host->lock, flags);
}

static void pxamci_disable_irq(struct pxamci_host *host, unsigned int mask)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	host->imask |= mask;
	writel(host->imask, host->base + MMC_I_MASK);
	spin_unlock_irqrestore(&host->lock, flags);
}

static void pxamci_setup_data(struct pxamci_host *host, struct mmc_data *data)
{
	unsigned int nob = data->blocks;
	unsigned int timeout, size;
	dma_addr_t dma;
	u32 dcmd;
	int i;

	host->data = data;

	if (data->flags & MMC_DATA_STREAM)
		nob = 0xffff;

	writel(nob, host->base + MMC_NOB);
	writel(1 << data->blksz_bits, host->base + MMC_BLKLEN);

	timeout = ns_to_clocks(data->timeout_ns) + data->timeout_clks;
	timeout = (timeout + 255)>>8;
	if(timeout>=65536)
		timeout = 65535;
	writel(timeout, host->base + MMC_RDTO);	//modified by hzh
//	writel((timeout + 255) / 256, host->base + MMC_RDTO);

	if (data->flags & MMC_DATA_READ) {
		host->dma_dir = DMA_FROM_DEVICE;
		dcmd = DCMD_INCTRGADDR | DCMD_FLOWTRG;
		DRCMRTXMMC = 0;
		DRCMRRXMMC = host->dma | DRCMR_MAPVLD;
	} else {
		host->dma_dir = DMA_TO_DEVICE;
		dcmd = DCMD_INCSRCADDR | DCMD_FLOWSRC;
		DRCMRRXMMC = 0;
		DRCMRTXMMC = host->dma | DRCMR_MAPVLD;
	}

	dcmd |= DCMD_BURST32 | DCMD_WIDTH1;

	host->dma_size = data->blocks << data->blksz_bits;
	host->dma_buf = dma_map_single(mmc_dev(host->mmc), data->rq->buffer+data->offset,
				       host->dma_size, host->dma_dir);
	//add by hzh, +data->offset
	for (i = 0, size = host->dma_size, dma = host->dma_buf; size; i++) {
		u32 len = size;

		if (len > DCMD_LENGTH)
			len = 0x1000;

		if (data->flags & MMC_DATA_READ) {
			host->sg_cpu[i].dsadr = host->res->start + MMC_RXFIFO;
			host->sg_cpu[i].dtadr = dma;
		} else {
			host->sg_cpu[i].dsadr = dma;
			host->sg_cpu[i].dtadr = host->res->start + MMC_TXFIFO;
		}
		host->sg_cpu[i].dcmd = dcmd | len;

		dma += len;
		size -= len;

		if (size) {
			host->sg_cpu[i].ddadr = host->sg_dma + (i + 1) *
						 sizeof(pxa_dma_desc);
		} else {
			host->sg_cpu[i].ddadr = DDADR_STOP;
		}
	}
	wmb();

	DDADR(host->dma) = host->sg_dma;
	DCSR(host->dma) = DCSR_RUN;
}

static void pxamci_start_cmd(struct pxamci_host *host, struct mmc_command *cmd, unsigned int cmdat)
{
	WARN_ON(host->cmd != NULL);
	host->cmd = cmd;

	if (cmd->flags & MMC_RSP_BUSY)
		cmdat |= CMDAT_BUSY;

	switch (cmd->flags & (MMC_RSP_MASK | MMC_RSP_CRC)) {
	case MMC_RSP_SHORT | MMC_RSP_CRC:
		cmdat |= CMDAT_RESP_SHORT;
		break;
	case MMC_RSP_SHORT:
		cmdat |= CMDAT_RESP_R3;
		break;
	case MMC_RSP_LONG | MMC_RSP_CRC:
		cmdat |= CMDAT_RESP_R2;
		break;
	default:
		break;
	}

	writel(cmd->opcode, host->base + MMC_CMD);
	writel(cmd->arg >> 16, host->base + MMC_ARGH);
	writel(cmd->arg & 0xffff, host->base + MMC_ARGL);
	writel(cmdat, host->base + MMC_CMDAT);
	writel(host->clkrt, host->base + MMC_CLKRT);

	writel(START_CLOCK, host->base + MMC_STRPCL);

	pxamci_enable_irq(host, END_CMD_RES);
}

static void pxamci_finish_request(struct pxamci_host *host, struct mmc_request *req)
{
	DBG("PXAMCI: request done\n");
	host->req = NULL;
	host->cmd = NULL;
	host->data = NULL;
	mmc_request_done(host->mmc, req);
}

static int pxamci_cmd_done(struct pxamci_host *host, unsigned int stat)
{
	struct mmc_command *cmd = host->cmd;
	int i;
	u32 v;

	if (!cmd)
		return 0;

	host->cmd = NULL;

	/*
	 * Did I mention this is Sick.  We always need to
	 * discard the upper 8 bits of the first 16-bit word.
	 */
	v = readl(host->base + MMC_RES) & 0xffff;
	for (i = 0; i < 4; i++) {
		u32 w1 = readl(host->base + MMC_RES) & 0xffff;
		u32 w2 = readl(host->base + MMC_RES) & 0xffff;
		cmd->resp[i] = v << 24 | w1 << 8 | w2 >> 8;
		v = w2;
	}

	if (stat & STAT_TIME_OUT_RESPONSE) {
		cmd->error = MMC_ERR_TIMEOUT;
	} else if (stat & STAT_RES_CRC_ERR && cmd->flags & MMC_RSP_CRC) {
		cmd->error = MMC_ERR_BADCRC;
	}

	pxamci_disable_irq(host, END_CMD_RES);
	if (host->data && cmd->error == MMC_ERR_NONE) {
		pxamci_enable_irq(host, DATA_TRAN_DONE);
	} else {
		pxamci_finish_request(host, host->req);
	}

	return 1;
}

static int pxamci_data_done(struct pxamci_host *host, unsigned int stat)
{
	struct mmc_data *data = host->data;

	if (!data)
		return 0;

	DCSR(host->dma) = 0;
	dma_unmap_single(mmc_dev(host->mmc), host->dma_buf, host->dma_size,
			 host->dma_dir);

	if (stat & STAT_READ_TIME_OUT)
		data->error = MMC_ERR_TIMEOUT;
	else if (stat & (STAT_CRC_READ_ERROR|STAT_CRC_WRITE_ERROR))
		data->error = MMC_ERR_BADCRC;

	data->bytes_xfered = (data->blocks - readl(host->base + MMC_NOB))
			       << data->blksz_bits;

	pxamci_disable_irq(host, DATA_TRAN_DONE);

	host->data = NULL;
	if (host->req->stop && data->error == MMC_ERR_NONE) {
		pxamci_stop_clock(host);
		pxamci_start_cmd(host, host->req->stop, 0);
	} else {
		pxamci_finish_request(host, host->req);
	}

	return 1;
}

static irqreturn_t pxamci_irq(int irq, void *devid, struct pt_regs *regs)
{
	struct pxamci_host *host = devid;
	unsigned int ireg;
	int handled = 0;

	ireg = readl(host->base + MMC_I_REG);

	DBG("PXAMCI: irq %08x\n", ireg);

	if (ireg) {
		unsigned stat = readl(host->base + MMC_STAT);

		DBG("PXAMCI: stat %08x\n", stat);

		if (ireg & END_CMD_RES)
			handled |= pxamci_cmd_done(host, stat);
		if (ireg & DATA_TRAN_DONE)
			handled |= pxamci_data_done(host, stat);
	}

	return IRQ_RETVAL(handled);
}

static void pxamci_request(struct mmc_host *mmc, struct mmc_request *req)
{
	struct pxamci_host *host = mmc_priv(mmc);
	unsigned int cmdat;

	WARN_ON(host->req != NULL);

	host->req = req;

	pxamci_stop_clock(host);

	cmdat = host->cmdat;
	host->cmdat &= ~CMDAT_INIT;

	if (req->data) {
		pxamci_setup_data(host, req->data);

		cmdat &= ~CMDAT_BUSY;
		cmdat |= CMDAT_DATAEN | CMDAT_DMAEN;
		if (req->data->flags & MMC_DATA_WRITE)
			cmdat |= CMDAT_WRITE;

		if (req->data->flags & MMC_DATA_STREAM)
			cmdat |= CMDAT_STREAM;
	}

	pxamci_start_cmd(host, req->cmd, cmdat);
}

static int pxamci_get_clkrt(unsigned int clk)
{
	int i;

	for(i=0; i<6; i++)
		if(clk>=(CLOCKRATE>>i))
			break;
	return i;
}

static void pxamci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct pxamci_host *host = mmc_priv(mmc);

	DBG("pxamci_set_ios: clock %u power %u vdd %u.%02u\n",
	    ios->clock, ios->power_mode, ios->vdd / 100,
	    ios->vdd % 100);

	if (ios->clock) {
		//unsigned int clk = CLOCKRATE / ios->clock;
		//if (CLOCKRATE / clk > ios->clock)
		//	clk <<= 1;
		//host->clkrt = fls(clk) - 1;	//modified by hzh, fls fail???(in asm/bitops.h)
		host->clkrt = pxamci_get_clkrt(ios->clock);

		/*
		 * we write clkrt on the next command
		 */
	} else if (readl(host->base + MMC_STAT) & STAT_CLK_EN) {
		/*
		 * Ensure that the clock is off.
		 */
		writel(STOP_CLOCK, host->base + MMC_STRPCL);
	}

	if (host->power_mode != ios->power_mode) {
		host->power_mode = ios->power_mode;

		/*
		 * power control?  none on the lubbock.
		 */

		if (ios->power_mode == MMC_POWER_ON)
			host->cmdat |= CMDAT_INIT;
	}

	DBG("pxamci_set_ios: clkrt = %x cmdat = %x\n",
	    host->clkrt, host->cmdat);
}

static struct mmc_host_ops pxamci_ops = {
	.request	= pxamci_request,
	.set_ios	= pxamci_set_ios,
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

static void pxamci_dma_irq(int dma, void *devid, struct pt_regs *regs)
{
	printk(KERN_ERR "DMA%d: IRQ???\n", dma);
	DCSR(dma) = DCSR_STARTINTR|DCSR_ENDINTR|DCSR_BUSERR;
}

static u8  cd_sta;
static u8  cd_lst;
static u16 cd_cnt;

static struct timer_list cd_timer;
static struct work_struct card_hotplug_task;

static void card_insert(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mmc_host *mmc;
	struct pxamci_host *host = NULL;
	struct resource *r;
	int ret, irq;

	r = platform_device_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_device_irq(pdev, 0);
	if (!r || irq == NO_IRQ)
		return;

	r = request_mem_region(r->start, SZ_4K, "PXAMCI");
	if (!r)
		return;

	mmc = mmc_alloc_host(sizeof(struct pxamci_host), dev);
	if (!mmc) {
		ret = -ENOMEM;
		goto out;
	}

	mmc->ops = &pxamci_ops;
	mmc->f_min = 312500;
	mmc->f_max = 20000000;
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
	host->imask = TXFIFO_WR_REQ|RXFIFO_RD_REQ|CLK_IS_OFF|STOP_CMD|
		      END_CMD_RES|PRG_DONE|DATA_TRAN_DONE;

	host->base = ioremap(r->start, SZ_4K);
	if (!host->base) {
		ret = -ENOMEM;
		goto out;
	}

	/*
	 * Ensure that the host controller is shut down, and setup
	 * with our defaults.
	 */
	pxamci_stop_clock(host);
	writel(0, host->base + MMC_SPI);
	writel(64, host->base + MMC_RESTO);

#ifdef CONFIG_PREEMPT
#error Not Preempt-safe
#endif
	pxa_gpio_mode(GPIO6_MMCCLK_MD);
	pxa_gpio_mode(GPIO8_MMCCS0_MD);
	CKEN |= CKEN12_MMC;

	host->dma = pxa_request_dma("PXAMCI", DMA_PRIO_LOW, pxamci_dma_irq, host);
	if (host->dma < 0) {
		ret = -EBUSY;
		goto out;
	}

	ret = request_irq(host->irq, pxamci_irq, 0, "PXAMCI", host);
	if (ret)
		goto out;

	dev_set_drvdata(dev, mmc);	//modified by hzh, host -> mmc

	mmc_add_host(mmc);

	return;

 out:
	if (host) {
		if (host->dma >= 0)
			pxa_free_dma(host->dma);
		if (host->base)
			iounmap(host->base);
		if (host->sg_cpu)
			dma_free_coherent(dev, PAGE_SIZE, host->sg_cpu, host->sg_dma);
	}
	if (mmc)
		mmc_free_host(mmc);
	release_resource(r);
}

static void card_remove(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);

	dev_set_drvdata(dev, NULL);
	
//	del_timer_sync(&cd_timer);

	if (mmc) {
		struct pxamci_host *host = mmc_priv(mmc);

		mmc_remove_host(mmc);

		pxamci_stop_clock(host);
		writel(TXFIFO_WR_REQ|RXFIFO_RD_REQ|CLK_IS_OFF|STOP_CMD|
		       END_CMD_RES|PRG_DONE|DATA_TRAN_DONE,
		       host->base + MMC_I_MASK);

		free_irq(host->irq, host);
		pxa_free_dma(host->dma);
		iounmap(host->base);
		dma_free_coherent(dev, PAGE_SIZE, host->sg_cpu, host->sg_dma);

		release_resource(host->res);

		mmc_free_host(mmc);
	}
}

static void card_hotplug(void *dummy)
{
	if(cd_sta)	//insert
		card_insert(dummy);
	else		//remove
		card_remove(dummy);
}

static void cd_func(unsigned long data)
{
	u32 r, i;
	
	r = GPLR0 & (1<<25);
	for(i=0; i<5; i++)
		if(r!=(GPLR0 & (1<<25))) {
			cd_lst = -1;
			mod_timer(&cd_timer, jiffies+HZ/100);
			return;
		}

	r = r?0:1;
	cd_cnt++;
	if(r!=cd_lst) {
		cd_lst = r;
		cd_cnt = 0;
	}
	
	if(cd_cnt<20)
		mod_timer(&cd_timer, jiffies+HZ/100);
	else {
		del_timer_sync(&cd_timer);
		if(cd_sta!=r) {
			cd_sta = r;
			schedule_work(&card_hotplug_task);
		}
		//enable irq after card add/remove
		enable_irq(IRQ_GPIO(25));
	}
}

static inline void cd_start(void)
{
	cd_lst = -1;
	
	init_timer(&cd_timer);
	cd_timer.function = cd_func;
//	cd_timer.data = NULL;
	cd_timer.expires = jiffies+(cd_sta?HZ:(HZ>>2));	//insert delay 250ms, remove delay 1s
	add_timer(&cd_timer);
}

static irqreturn_t cd_isr(int irq, void *devid, struct pt_regs *regs)
{
	disable_irq(IRQ_GPIO(25));
	cd_start();
	return IRQ_HANDLED;
}

static int pxamci_probe(struct device *dev)
{
	INIT_WORK(&card_hotplug_task, card_hotplug, dev);
		
	set_irq_type(IRQ_GPIO(25), IRQT_BOTHEDGE);
	cli();
//	disable_irq(IRQ_GPIO(25));
	if(request_irq(IRQ_GPIO(25), cd_isr, 0, "SD/MMC-CD", dev)) {
//		enable_irq(IRQ_GPIO(25));
		printk("fail to get irq for card-detect!\n");
	} else
		disable_irq(IRQ_GPIO(25));
	sti();
	cd_start();
	
	return 0;
}

static int pxamci_remove(struct device *dev)
{
	free_irq(IRQ_GPIO(25), dev);
	return 0;
}

#ifdef CONFIG_PM
static int pxamci_suspend(struct device *dev, u32 state, u32 level)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	int ret = 0;

	if (mmc && level == SUSPEND_DISABLE)
		ret = mmc_suspend_host(mmc, state);

	return ret;
}

static int pxamci_resume(struct device *dev, u32 level)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	int ret = 0;

	if (mmc && level == RESUME_ENABLE)
		ret = mmc_resume_host(mmc);

	return ret;
}
#endif

static struct device_driver pxamci_driver = {
	.name		= "pxa2xx-mci",
	.bus		= &platform_bus_type,
	.probe		= pxamci_probe,
	.remove		= pxamci_remove,
#ifdef CONFIG_PM
	.suspend	= pxamci_suspend,
	.resume		= pxamci_resume,
#endif
};

static int __init pxamci_init(void)
{
	return driver_register(&pxamci_driver);
}

static void __exit pxamci_exit(void)
{
	driver_unregister(&pxamci_driver);
}

module_init(pxamci_init);
module_exit(pxamci_exit);

MODULE_DESCRIPTION("PXA Multimedia Card Interface Driver");
MODULE_LICENSE("GPL");
