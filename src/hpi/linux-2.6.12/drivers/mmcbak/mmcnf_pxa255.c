#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/sizes.h>

struct reg_partition {
	char *name;
	u32 size;
	u16 rsv_blks;
	u8 ro_flag;
	u8 disk_flag;
};

struct nand_flash_itf {
	u32 data;
	u32 addr;
	u32 cmd;
	void (*nf_ce_ctl)(int ce);
	int (*nf_ready)(void);
	u16 addr_cycle;
} __attribute__((packed));

int register_nand_flash_itf(struct nand_flash_itf *, 
							u32, struct reg_partition *, 
							void(*open_release)(int), 
							struct device *);
int unregister_nand_flash_itf(struct nand_flash_itf *);

static struct reg_partition nf_partitions[] = {
	{
		name:		"boot",
		size:		SZ_256K,		
		ro_flag:	1,
		disk_flag:	0,
	},
	{
		name:		"kernel",
		size:		SZ_2M-SZ_256K,
		rsv_blks:	4,
		disk_flag:	1,
	},
	{
		name:		"rootfs",
		size:		SZ_16M-SZ_2M,
		rsv_blks:	16,
		disk_flag:	1,
	},
	{
		.name	   = "ext-fs",
		.size	   = (u32)-1,//NF_SIZE_FULL
		.rsv_blks  = 32,
		.disk_flag = 1,
	},
};

static void nf_ce_ctl(int ce)
{
	if(ce)
		GPSR1 = 1<<1;
	else
		GPCR1 = 1<<1;
}

static int nf_ready(void)
{
	udelay(10);
	return GPLR1 & (1<<22);
	//return 1;
}

static struct nand_flash_itf nf_itf = {
	nf_ce_ctl:	nf_ce_ctl,
	nf_ready:	nf_ready
};

static void nf_open_release(int open)
{
//	printk("mod usage %s", open?"open":"release");
	//if(open)
	//	MOD_INC_USE_COUNT;
	//else
	//	MOD_DEC_USE_COUNT;
//	printk("end\n");
}

static unsigned long nf_io;

#define	PXA255_NFBASE	0x04b00000

static struct resource pxanf_resources[] = {
	[0] = {
		.start	= PXA255_NFBASE,
		.end	= PXA255_NFBASE+SZ_1M-1,
		.flags	= IORESOURCE_MEM,
	},
};

static u64 pxanf_dmamask = 0xffffffffUL;

static struct platform_device pxanf_device = {
	.name		= "pxa2xx-nand",
	.id		= 0,
	.dev		= {
		.dma_mask = &pxanf_dmamask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(pxanf_resources),
	.resource	= pxanf_resources,
};

static int pxanf_probe(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *r;
	int err;
	
	r = &pdev->resource[0];
	if(!r)
		return -ENXIO;
	
	nf_io = (unsigned long)ioremap(r->start, r->end-r->start+1);
	if(!nf_io) {
		printk(KERN_ERR "fail to ioremap nand flash base address!\n");
		return -ENOMEM;
	}
	nf_itf.data = nf_io;
	nf_itf.addr = nf_io+0x40;
	nf_itf.cmd  = nf_io+0x80;
	
	pxa_gpio_mode(33|GPIO_OUT);	//CS
	pxa_gpio_mode(54|GPIO_IN);	//RDY/BUSY
	
	err = register_nand_flash_itf(&nf_itf, ARRAY_SIZE(nf_partitions), 
									nf_partitions, 
									nf_open_release, 
									dev);
	if(err<0) {
		nf_io = 0;
		printk(KERN_ERR "%s:fail to register nand flash interface! err=%d\n", __FUNCTION__, err);
		return -ENXIO; 
	}
	
	return 0;	
}

static int pxanf_remove(struct device *dev)
{
	int err;
	
	err = unregister_nand_flash_itf(&nf_itf);
	if(err<0)
		printk(KERN_ERR "%s:fail to unregister nand flash interface! err=%d\n", __FUNCTION__, err);
	else
		nf_io = 0;
	
	iounmap((void *)nf_io);
	nf_io = 0;
	
	return 0;
}

#ifdef CONFIG_PM
static int pxanf_suspend(struct device *dev, u32 state, u32 level)
{
}

static int pxanf_resume(struct device *dev, u32 level)
{
}
#endif

static struct device_driver pxanf_driver = {
	.name		= "pxa2xx-nand",
	.bus		= &platform_bus_type,
	.probe		= pxanf_probe,
	.remove		= pxanf_remove,
#ifdef CONFIG_PM
	.suspend	= pxamnf_suspend,
	.resume		= pxamnf_resume,
#endif
};

static __init int my_nfdrv_init(void)
{
	if(platform_device_register(&pxanf_device))
		return -ENOMEM;
	
	return driver_register(&pxanf_driver);
}

static void __exit my_nfdrv_exit(void)
{
	driver_unregister(&pxanf_driver);
	
	platform_device_unregister(&pxanf_device);
}

module_init(my_nfdrv_init);
module_exit(my_nfdrv_exit);

MODULE_AUTHOR("antiscle, hzhmei@hotmail.com");
MODULE_DESCRIPTION("PXA255 NAND FLASH Initialize");
MODULE_LICENSE("GPL");
