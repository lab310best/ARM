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
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/protocol.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/sizes.h>

#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-irq.h>
#include <asm/arch/regs-clock.h>
#include <asm/arch/regs-sdi.h>
#include <asm/arch/regs-dma.h>

#ifdef CONFIG_MMC_DEBUG
#define DBG(x...)	printk(KERN_DEBUG x)
#else
#define DBG(x...)	do { } while (0)
#endif

#define	FIFO_TYPEA	(0<<4)
#define	FIFO_TYPEB	(1<<4)
#define	FIFO_TYPE	FIFO_TYPEA

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

	dma_addr_t		dma_buf;
	unsigned int		dma_size;
	unsigned int		dma_dir;
};

/*
 * The base MMC clock rate
 */
#define CLOCKRATE	25000000

static inline unsigned int ns_to_clocks(unsigned int ns)
{
	return (ns * (CLOCKRATE / 1000000) + 999) / 1000;
}

static inline void pxamci_stop_clock(struct pxamci_host *host)
{
	writel(readl(S3C2410_SDICON) & ~1, S3C2410_SDICON);
}

static inline void pxamci_enable_irq(struct pxamci_host *host, unsigned int mask)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	host->imask |= mask;
	writel(host->imask, S3C2410_SDIIMSK);
	spin_unlock_irqrestore(&host->lock, flags);
}

static inline void pxamci_disable_irq(struct pxamci_host *host, unsigned int mask)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	host->imask &= ~mask;
	writel(host->imask, S3C2410_SDIIMSK);
	spin_unlock_irqrestore(&host->lock, flags);
}

#define	SDI_RX_DMA_VAL  ((1UL<<31)+(0<<30)+(1<<29)+(0<<28)+(0<<27)+(2<<24)+(1<<23)+(1<<22)+(2<<20))
//handshake, sync PCLK, TC int, single tx, single service, SDI, H/W request, 
//auto-reload off, word, transmit size/4
#define	SDI_TX_DMA_VAL  ((1UL<<31)+(0<<30)+(1<<29)+(0<<28)+(0<<27)+(2<<24)+(1<<23)+(1<<22)+(2<<20))
//handshake, sync PCLK, TC int, single tx, single service, SDI, H/W request, 
//auto-reload off, word, transmit size/4

static void pxamci_setup_data(struct pxamci_host *host, struct mmc_data *data)
{
	unsigned int nob = data->blocks;
	unsigned int timeout;
	u32 dcon;

	host->data = data;
	
	//reset fifo, after dma start, data from memory to fifo, so
	//can't reset fifo before data transfer end
	//if dma size > transmit size, after all data transmitted,
	//fifo can also be filled with data in the memory,!!!
	//and then the dma process will not be completed 
	//if dma size > transmit size + fifo size(64)
	writel(FIFO_TYPE|(1<<1)|1, S3C2410_SDICON);
	
	//clear dat status
	writel(0x7fc,  S3C2410_SDIDSTA);

	dcon = nob | (1 << 17);	//block mode
	
	if (data->flags & MMC_DATA_STREAM) {
		dcon &= ~(1 << 17);		//stream mode
		dcon |= 1 << 14;		//stop by force
	}
	
	if (data->flags & MMC_DATA_4BITS)
		dcon |= 1 << 16;		//4bits mode
		
//	if (nob > 1)
//		dcon |= 1 << 14;		//stop by force
	
	writel(1 << data->blksz_bits, S3C2410_SDIBSIZE);

//	timeout = ns_to_clocks(data->timeout_ns) + data->timeout_clks;
//	timeout = (timeout + 255)>>8;
//	if(timeout>=65536)
		timeout = 65535;
	writel(timeout, S3C2410_SDIDTIMER);

	host->dma_size = nob << data->blksz_bits;
	
	if (data->flags & MMC_DATA_READ) {
		dcon |= (1 << 19) | (2 << 12);	//receive start after command sent
		host->dma_dir = DMA_FROM_DEVICE;
		
		host->dma_buf = dma_map_single(mmc_dev(host->mmc), data->rq->buffer+data->offset,
									       host->dma_size, host->dma_dir);
		//add by hzh, +data->offset
		
		writel(S3C2410_PA_SDI+0x3c, S3C2410_DISRC0);	//SDIDAT
		writel(3, S3C2410_DISRCC0);		//APB, fix
		writel(host->dma_buf, S3C2410_DIDST0);				//memory address
		writel(0, S3C2410_DIDSTC0);		//AHB, inc
		writel(SDI_RX_DMA_VAL|(host->dma_size>>2), S3C2410_DCON0);
	    writel(2, S3C2410_DMTRIG0);		//no-stop, DMA2 channel on, no-sw trigger
	} else {
		dcon |= (1 << 20) | (3 << 12);	//transmit start after resp receive
		host->dma_dir = DMA_TO_DEVICE;
		
		host->dma_buf = dma_map_single(mmc_dev(host->mmc), data->rq->buffer+data->offset,
									       host->dma_size, host->dma_dir);
		//add by hzh, +data->offset
		
		writel(host->dma_buf, S3C2410_DISRC0);				//memory address
		writel(0, S3C2410_DISRCC0);		//AHB, inc
		writel(S3C2410_PA_SDI+0x3c, S3C2410_DIDST0);	//SDIDAT
		writel(3, S3C2410_DIDSTC0);		//APB, fix
		writel(SDI_TX_DMA_VAL|(host->dma_size>>2), S3C2410_DCON0);
	    writel(2, S3C2410_DMTRIG0);		//no-stop, DMA2 channel on, no-sw trigger
	}
	
    writel(dcon|(1<<15), S3C2410_SDIDCON);	//enable DMA and start receive or transmit
}

static void pxamci_start_cmd(struct pxamci_host *host, struct mmc_command *cmd, unsigned int cmdat)
{
	int imsk;
	
	WARN_ON(host->cmd != NULL);
	host->cmd = cmd;
	
	//clear cmd status
	writel(0x1e00, S3C2410_SDICSTA);
	
	cmdat = 0x140;

	if (cmd->flags & MMC_RSP_MASK) {
		imsk = I_RspEnd | I_CmdTout;
		cmdat |= 1<<9;			//wait resp
		if (cmd->flags & MMC_RSP_LONG)
			cmdat |= 1<<10;		//long resp
	} else
		imsk = I_CmdSent;
	
//	printk(KERN_DEBUG "%08x\n", cmd->opcode);
	//set clock rate
	writel(host->clkrt*2+1, S3C2410_SDIPRE); //fla    host->clkrt-->host->clkrt  x 2 + 1 
	//Type B, don't reset FIFO, enable clock
	writel(FIFO_TYPE|1, S3C2410_SDICON);
	//command arguments
	writel(cmd->arg, S3C2410_SDICARG);
	//start command
	writel(cmdat|cmd->opcode, S3C2410_SDICCON);
	
	pxamci_enable_irq(host, imsk);
}

static void pxamci_finish_request(struct pxamci_host *host, struct mmc_request *req)
{
	pxamci_stop_clock(host);
	
	DBG("PXAMCI: request done\n");
	host->req = NULL;
	host->cmd = NULL;
	host->data = NULL;
	mmc_request_done(host->mmc, req);
}

static int pxamci_cmd_done(struct pxamci_host *host, unsigned int stat)
{
	struct mmc_command *cmd = host->cmd;

	if (!cmd)
		return 0;

	host->cmd = NULL;

	if(cmd->flags & MMC_RSP_MASK) {
		if(stat & CS_CmdTout)
			cmd->error = MMC_ERR_TIMEOUT;
		else if(stat & CS_RspCrc && cmd->flags & MMC_RSP_CRC) {
			//in s3c2410, cmd 2, cmd9 always crc fail,bug???
			if(cmd->opcode!=MMC_ALL_SEND_CID&&cmd->opcode!=MMC_SEND_CSD)
				cmd->error = MMC_ERR_BADCRC;
		}

		cmd->resp[0] = readl(S3C2410_SDIRSP0);
		cmd->resp[1] = readl(S3C2410_SDIRSP1);
		cmd->resp[2] = readl(S3C2410_SDIRSP2);
		cmd->resp[3] = readl(S3C2410_SDIRSP3);
	}
		
	pxamci_disable_irq(host, I_RspEnd | I_CmdTout | I_CmdSent);
	
	if (host->data) {
		if(cmd->error == MMC_ERR_NONE) {
			pxamci_enable_irq(host, I_DatFin | I_DatTout);
			/*if(cmd->opcode==0x18) {
				int i=3;
				while(i--)
					printk("%x,%x,%x,%x,%x,%x:%x,%x,%x\n", 
					readl(S3C2410_DISRC0),
					readl(S3C2410_DISRCC0), 
					readl(S3C2410_DIDST0), 
					readl(S3C2410_DIDSTC0), 
					readl(S3C2410_DCON0), 
					readl(S3C2410_DSTAT0), 
					readl(S3C2410_SDIDCON), 
					readl(S3C2410_SDIDSTA), 
					readl(S3C2410_SDIDCNT));
			
			}*/
			return 1;
		} else {	//if error occured, stop dma
			u32 v;
			
			writel(4, S3C2410_DMTRIG0);		//stop dma ch 0
			//v = (readl(S3C2410_SDIDCON) & ~0xb000) | 0x4000;  fla mask
			v = ((readl(S3C2410_SDIDCON) & ~0xb000)) & (~0x4000); //set bit14 to 0 (0:dataready 1:datastart)
			writel(v, S3C2410_SDIDCON);		//clear DMA bit and stop transfer
		}
	}

	pxamci_finish_request(host, host->req);

	return 1;
}

static int pxamci_data_done(struct pxamci_host *host, unsigned int stat)
{
	struct mmc_data *data = host->data;

	if (!data)
		return 0;
	
	writel(4, S3C2410_DMTRIG0);			//stop dma ch 0
	writel(readl(S3C2410_SDIDCON) & ~0x8000, S3C2410_SDIDCON);	//clear DMA bit
/*	
	DBG("%x,%x,%x,%x,%x,%x,%x,%x:%x,%x,%x\n", 
				readl(S3C2410_DISRC0),
				readl(S3C2410_DISRCC0), 
				readl(S3C2410_DIDST0), 
				readl(S3C2410_DIDSTC0), 
				readl(S3C2410_DCON0), 
				readl(S3C2410_DSTAT0), 
				readl(S3C2410_DCSRC0), 
				readl(S3C2410_DCDST0), 
				readl(S3C2410_SDIDCON), 
				readl(S3C2410_SDIDSTA), 
				readl(S3C2410_SDIDCNT));
*/
	dma_unmap_single(mmc_dev(host->mmc), host->dma_buf, host->dma_size,
			 host->dma_dir);

	if (stat & (DS_FFfail | DS_SBitErr))
		data->error = MMC_ERR_BADCRC;
	else if (stat & DS_DatTout)
		data->error = MMC_ERR_TIMEOUT;
	else if (stat & DS_CrcSta && data->flags & MMC_DATA_WRITE)
		data->error = MMC_ERR_BADCRC;
	else if (stat & DS_DatCrc && data->flags & MMC_DATA_READ)
		data->error = MMC_ERR_BADCRC;

	data->bytes_xfered = (data->blocks - ((readl(S3C2410_SDIDCNT) >> 12) & 0xfff))
			       << data->blksz_bits;

	pxamci_disable_irq(host, I_DatFin | I_DatTout);

	host->data = NULL;
	if (host->req->stop && data->error == MMC_ERR_NONE) {
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

	ireg = readl(S3C2410_SDICSTA);
	DBG("cmd irq %08x\n", ireg);
	ireg &= 0x1e00;
	if(ireg) {
		handled |= pxamci_cmd_done(host, ireg);
		writel(ireg, S3C2410_SDICSTA);	//clear status
	}

	ireg = readl(S3C2410_SDIDSTA);
	DBG("dat irq %08x\n", ireg);
	ireg &= 0x1f4;
	if (ireg) {
		handled |= pxamci_data_done(host, ireg);
		writel(ireg, S3C2410_SDIDSTA);	//clear status
	}
	
	//must clear parent int bit after clear status!!!
	writel(1<<21, S3C2410_SRCPND);	//clear srcpnd register
	writel(1<<21, S3C2410_INTPND);	//clear intpnd register

	return IRQ_RETVAL(handled);
}


//dma controller :when finish dma transfer ,send this irq 
static irqreturn_t pxamci_dma_irq(int irq, void *devid, struct pt_regs *regs)
{
	printk(KERN_ERR "pxamci_dma_irq(),now have finish a dma transfer!  line %d\n",__LINE__);
//	writel(4, S3C2410_DMTRIG0);
	writel(readl(S3C2410_SDIDCON) & ~0x8000, S3C2410_SDIDCON);	//clear DMA bit
	DBG("D\n");
	//pxamci_irq(irq, devid, regs);
	return IRQ_HANDLED;
}

static void pxamci_request(struct mmc_host *mmc, struct mmc_request *req)
{
	struct pxamci_host *host = mmc_priv(mmc);
	unsigned int cmdat;

	WARN_ON(host->req != NULL);

	host->req = req;

	pxamci_stop_clock(host);

	cmdat = host->cmdat;

	if (req->data) {
		pxamci_setup_data(host, req->data);
	}

	pxamci_start_cmd(host, req->cmd, cmdat);
}

static int pxamci_get_clkrt(unsigned int clk)
{
	int i;

	if(clk>=25000000)
		i = 0;
	else if(clk>=12500000)
		i = 1;
	else if(clk>=8000000)
		i = 2;
	else if(clk>=5000000)
		i = 4;
	else if(clk>=2500000)
		i = 9;
	else if(clk>=1000000)
		i = 24;
	else if(clk>=500000)
		i = 49;
	else
		i = 61;
	
	return i;
}

static void pxamci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct pxamci_host *host = mmc_priv(mmc);

	DBG("pxamci_set_ios: clock %u power %u vdd %u.%02u\n",
	    ios->clock, ios->power_mode, ios->vdd / 100,
	    ios->vdd % 100);

	if (ios->clock) {
	
		host->clkrt = pxamci_get_clkrt(ios->clock);
		
		//writel(host->clkrt, S3C2410_SDIPRE);

		/*
		 * we write clkrt on the next command
		 */
	} else {
		/*
		 * Ensure that the clock is off.
		 */
		pxamci_stop_clock(host);
	}

	if (host->power_mode != ios->power_mode) {
		host->power_mode = ios->power_mode;

		/*
		 * power control?  none on the lubbock.
		 */

		if (ios->power_mode == MMC_POWER_ON) {
			// Set INIT clock, about 400KHz
	//		writel(61, S3C2410_SDIPRE);
			// Type B, FIFO reset, clk enable
	//		writel(FIFO_TYPE|1, S3C2410_SDICON);
			// Wait 74SDCLK for MMC card
	//		udelay(200);
		}
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

static u8  cd_sta;
static u8  cd_lst;
static u16 cd_cnt;

static struct timer_list cd_timer;
static struct work_struct card_hotplug_task;

static void pxamci_release(struct device *dev);

static void card_insert(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mmc_host *mmc;
	struct pxamci_host *host = NULL;
	struct resource *r;
	int irq;
  printk(KERN_ERR "enter card_insert() line %d passed!\n",__LINE__);
	r = platform_device_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_device_irq(pdev, 0);
	if (!r || irq == NO_IRQ)
		{printk(KERN_ERR "line %d passed!\n",__LINE__);
	return;}
	printk(KERN_ERR "line %d passed!\n",__LINE__);
	mmc = mmc_alloc_host(sizeof(struct pxamci_host), dev);
	if (!mmc)
		return;
	
	mmc->ops = &pxamci_ops;
	mmc->f_min = 400000;
	mmc->f_max = CLOCKRATE>>1;
	mmc->ocr_avail = MMC_VDD_32_33;

	host = mmc_priv(mmc);
	host->mmc = mmc;
	host->dma = -1;

	spin_lock_init(&host->lock);
	host->res = r;
	host->irq = irq;
	host->imask = 0;
	
	host->base = (void *)S3C2410_VA_SDI;

	/*
	 * Ensure that the host controller is shut down, and setup
	 * with our defaults.
	 */
	pxamci_stop_clock(host);
		
//	writel(64, host->base + MMC_RESTO);

	//why pxa255 needn't release function???
	dev->release = pxamci_release;
	
//	if (request_irq(IRQ_SDI, pxamci_irq, 0, "PXAMCI", host))
	if (request_irq(IRQ_SDI, pxamci_irq, SA_INTERRUPT, "PXAMCI", host)) {
		printk("fail to get irq for SD/MMC card control!\n");
		return;
	}
	printk(KERN_ERR "success to get irq for SD/MMC card control!\n");  //fla
	if (request_irq(IRQ_DMA0, pxamci_dma_irq, SA_INTERRUPT, "PXAMCI-DMA", host)) {
		printk("fail to get irq for SD/MMC card DMA!\n");
		free_irq(IRQ_SDI, host);
		return;
	}
	printk(KERN_ERR "success  to get irq for SD/MMC card DMA!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	
//	writel(0, S3C2410_SDICARG);
	//start cmd
//	writel(0x140, S3C2410_SDICCON);
//	pxamci_enable_irq(host, 0x3ffff);//I_CmdSent);

	dev_set_drvdata(dev, mmc);	//modified by hzh, host -> mmc
	
	//if no CD line, it can only be done at initialize time
	mmc_add_host(mmc);
}

static void card_remove(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);

	dev_set_drvdata(dev, NULL);
	
//	del_timer_sync(&cd_timer);	//don't del_timer_sync here!!!

	if (mmc) {
		struct pxamci_host *host = mmc_priv(mmc);

		mmc_remove_host(mmc);

		pxamci_stop_clock(host);
		writel(0x3ffff, S3C2410_SDIIMSK);
		
		free_irq(IRQ_DMA0, host);
		free_irq(IRQ_SDI, host);
		
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
	u16 r, i;
	
	r = readl(S3C2410_GPGDAT)&(1<<10);
	for(i=0; i<5; i++)
		if(r!=(readl(S3C2410_GPGDAT)&(1<<10))) {
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
//		printk("2:%x,%x\n", readl(S3C2410_EINTMASK), readl(S3C2410_INTMSK));
//		printk(KERN_DEBUG "e %lu\n", jiffies);
		enable_irq(IRQ_EINT18);
//		//clear int status before enable it
//		writel(1<<18, S3C2410_EINTPEND);
//		//unmask IRQ_EINT18
//		writel(readl(S3C2410_EINTMASK)&~(1<<18), S3C2410_EINTMASK);
//		//unmask IRQ_EINT8t23
//		writel(readl(S3C2410_INTMSK)&~(1<<5), S3C2410_INTMSK);
	}
}

static inline void cd_start(void)
{
//	int i;
	
//	printk("1:%x,%x\n", readl(S3C2410_EINTMASK), readl(S3C2410_INTMSK));
	//to prevent vibration, it's must be in SA_INTERRUPT
	//and mask the irq when check card insert status!!!
	//mask IRQ_EINT18, needn't mask IRQ_EINT8t23
//	i  = readl(S3C2410_EINTMASK);
//	i |= 1<<18;
//	writel(i, S3C2410_EINTMASK);
//	//ack IRQ_EINT18
//	writel(1<<18, S3C2410_EINTPEND);
//	i = readl(S3C2410_EINTPEND)&~i;	
//	if(!(i&(0xffff<<8))) {
//		//ack IRQ_EINT8t23
//		writel(1<<5, S3C2410_SRCPND);
//		writel(1<<5, S3C2410_INTPND);
//	}

	cd_lst = -1;
	
	init_timer(&cd_timer);
	cd_timer.function = cd_func;
//	cd_timer.data = NULL;
	cd_timer.expires = jiffies+(cd_sta?HZ:(HZ>>2));	//insert delay 250ms, remove delay 1s
	add_timer(&cd_timer);
}

static irqreturn_t cd_isr(int irq, void *devid, struct pt_regs *regs)
{
	disable_irq(IRQ_EINT18);
//	printk(KERN_DEBUG "d %lu\n", jiffies);
	cd_start();
	return IRQ_HANDLED;
}

static int pxamci_probe(struct device *dev)
{
	//card detect, GPG10 eint18 and enable pull-up
	printk(KERN_ERR "hi today!!!!!!!!!!!!!!!!!,line %d passed.do probe mmc!\n",__LINE__);
	writel((readl(S3C2410_GPGCON)&~(3<<20))|(2<<20), S3C2410_GPGCON);
	writel(readl(S3C2410_GPGUP)&~(1<<10), S3C2410_GPGUP);
	//enable eint18 filter and both edge int
	writel(readl(S3C2410_EXTINT2)|(0xf<<8), S3C2410_EXTINT2);
	
	//set SDCLK,SDCMD,SDDAT0~SDDAT3
	writel((readl(S3C2410_GPECON)&~(0xfff<10))|(0xaaa<<10), S3C2410_GPECON);
	
	// Enable SDI Clock
	writel(readl(S3C2410_CLKCON)|(1<<9), S3C2410_CLKCON);
	// Set INIT clock, about 400KHz
	writel(123, S3C2410_SDIPRE);//61---123
	// Type B, FIFO reset, clk enable
	writel(FIFO_TYPE|(1<<1)|1, S3C2410_SDICON);
	// Wait 74SDCLK for MMC card
	udelay(200);

	//mask all sdi irq
	writel(0, S3C2410_SDIIMSK);
	//clear all status
	writel(0x1e00, S3C2410_SDICSTA);
	writel(0x7fc,  S3C2410_SDIDSTA);
	
#ifdef CONFIG_PREEMPT
#error Not Preempt-safe
#endif
	
	INIT_WORK(&card_hotplug_task, card_hotplug, dev);

	cli();
//	disable_irq(IRQ_EINT18);
	if (request_irq(IRQ_EINT18, cd_isr, SA_INTERRUPT, "SD/MMC-CD", NULL)) {
//		enable_irq(IRQ_EINT18);
		printk("fail to get irq for card-detect!\n");
	} else
		disable_irq(IRQ_EINT18);
	sti();	
	cd_start();

	return 0;
}

static int pxamci_remove(struct device *dev)
{
	card_remove(dev);
	
	free_irq(IRQ_EINT18, dev);
		
	writel(readl(S3C2410_CLKCON)&~(1<<9), S3C2410_CLKCON);

	return 0;
}

static void pxamci_release(struct device *dev)
{
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

static struct resource s3c2410_sdi_resources[] = {
	[0] = {
		.start	= S3C2410_VA_SDI,
		.end	= S3C2410_VA_SDI+0x100-1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_SDI,
		.end	= IRQ_SDI,
		.flags	= IORESOURCE_IRQ,
	},

};

static u64 s3c2410_sdi_dmamask = 0xffffffffUL;

static struct platform_device s3c2410_sdi_device = {
	.name		= "s3c2410-sdi",
	.id		= 0,
	.dev		= {
		.dma_mask = &s3c2410_sdi_dmamask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(s3c2410_sdi_resources),
	.resource	= s3c2410_sdi_resources,
};

static struct device_driver s3c2410_sdi_driver = {
	.name		= "s3c2410-sdi",
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
	if(platform_device_register(&s3c2410_sdi_device))
		return -ENOMEM;
		
	return driver_register(&s3c2410_sdi_driver);
}

static void __exit pxamci_exit(void)
{
	driver_unregister(&s3c2410_sdi_driver);
	
	platform_device_unregister(&s3c2410_sdi_device);
}

module_init(pxamci_init);
module_exit(pxamci_exit);

MODULE_DESCRIPTION("PXA Multimedia Card Interface Driver");
MODULE_LICENSE("GPL");
