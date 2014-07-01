

#include <linux/config.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/arch-s3c2410/regs-gpio.h>
#include <asm/arch-s3c2410/regs-irq.h>
#include <asm/arch-s3c2410/regs-gpio.h>
#if 1// defined(CONFIG_ARCH_FS2410) || defined(CONFIG_ARCH_FS2440)
/*
 * Set up a hw structure for a specified data port, control port and IRQ.
 * This should follow whatever the default interface uses.
 */

static __inline__ void
cf_ide_init_hwif_ports(hw_regs_t *hw, int data_port, int ctrl_port, int *irq)
{
	unsigned long reg;
	int i;
	int regincr = 1;
	
	regincr = 1 << 1;

	memset(hw, 0, sizeof(*hw));

	 	reg = (unsigned long)data_port;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		hw->io_ports[i] = reg;
		reg += regincr;
	}
	
	hw->io_ports[IDE_CONTROL_OFFSET] = (unsigned long) ctrl_port;
	
	if (irq)
		*irq = 0;
 }


/*
 * This registers the standard ports for this architecture with the IDE
 * driver.
 */

#define S3C2410_CF_PWR_CTL	(GPIO_MODE_OUT | GPIO_PULLUP_DIS | GPIO_B10)
#define S3C2410_CF_BASE		0x28000000	//nGCS5
#define S3C2410_CF_CTL_BASE	0x28800000	//A23 high
#define	S3C2410_CF_IRQ		IRQ_EINT3      //
#define S3C2410_IDE_BASE	0x10000000	//nGCS2
#define	S3C2410_IDE_IRQ		IRQ_EINT1

static __inline__ void
ide_init_default_hwifs(void)
{
	hw_regs_t hw_cf, hw_ide;
	int cf_base, cf_ctl_base;
	int ret,ide_base;
	unsigned long extint3;
#if 0//no cf
		//turn on GPB10 ,,zzm 2006-09-15
		*(volatile unsigned *)S3C2410_GPBCON |= 1<<20;
	  *(volatile unsigned *)S3C2410_GPBDAT |= 0x600;
	  printk(KERN_ERR "now begin cf_ide_init_default_hwifs()\n");
	/* map physical adress */
       
	cf_base     = (unsigned long) ioremap(S3C2410_CF_BASE, 0x10000);
	cf_ctl_base = (unsigned long) ioremap(S3C2410_CF_CTL_BASE, 0x10000);
	if(!cf_base||!cf_ctl_base) {
		printk("ioremap CF card failed\n");
		if(cf_base)
			iounmap((void *)cf_base);
		if(cf_ctl_base)
			iounmap((void *)cf_ctl_base);
	} else {
		// init the interface
		cf_ide_init_hwif_ports(&hw_cf, cf_base, cf_ctl_base+0xc, NULL);
		hw_cf.irq = S3C2410_CF_IRQ;
//		set_irq_type(hw_cf.irq, IRQT_RISING);//IRQT_RISING IRQT_FALLING IRQT_BOTHEDGE IRQT_LOW IRQT_HIGH
		//add by fla,initial cf interrupt Eint3
                __raw_writel((__raw_readl(S3C2410_GPFCON)|(1<<7))&(~(1<<6)),S3C2410_GPFCON);

                __raw_writel(__raw_readl(S3C2410_INTMSK)&(~(1<<3)),S3C2410_INTMSK);
                
                __raw_writel(__raw_readl(S3C2410_INTMSK)&(~(1<<1)),S3C2410_INTMSK);
               
                
                 __raw_writel(0x4040,S3C2410_EXTINT0);
                
                //*(volatile unsigned *)S3C2410_EXTINT0 |= 0x4041;
               
              
		printk(KERN_INFO "have initialized interrupt for cf:EINT3\n");
		printk(KERN_ERR "fla2612:INTMSK=%08x\nGPFCON=%08x\nEXTINT0=%08x\nGPBCON=%08x\nGPBDAT=%08x\n",__raw_readl(S3C2410_INTMSK),__raw_readl(S3C2410_GPFCON),
		__raw_readl(S3C2410_EXTINT0),__raw_readl(S3C2410_GPBCON),__raw_readl(S3C2410_GPBDAT));
		
		//*(volatile unsigned *)S3C2410_EXTINT0 |= 0x4040 ;
//printk(KERN_INFO "S3C2410_EXTINT0 0x%x\n", S3C2410_EXTINT0);
		ret = ide_register_hw(&hw_cf, NULL);
		printk(KERN_INFO "-1:no 0:ok CF_register_hw return %d\n", ret);
	}

	/* map physical adress */
#endif
	ide_base = (unsigned long) ioremap(S3C2410_IDE_BASE, 0x10000);
	if(!ide_base)
		printk("ioremap IDE failed\n");
	else {
// init the interface
		cf_ide_init_hwif_ports(&hw_ide, ide_base+0x20, ide_base+0x1c, NULL);
		hw_ide.irq = S3C2410_IDE_IRQ;
//		set_irq_type(hw_ide.irq, IRQT_RISING);
		//add by zzm
		*(volatile unsigned *)S3C2410_EXTINT0 |= 0x4040 ;
		//init EINT1/GPF1 fla
		// __raw_writel((__raw_readl(S3C2410_GPFCON)|(1<<3))&(~(1<<2)),S3C2410_GPFCON);                 
          //set EINT1 enable  fla
          //__raw_writel(__raw_readl(S3C2410_INTMSK)&(~(1<<1)),S3C2410_INTMSK);

		// printk(KERN_INFO "have initialize interrupt for ide:EINT1"
		//          "fla2612:INTMSK=%08x\nGPFCON=%08x\nEXTINT0=%08x",__raw_readl(S3C2410_INTMSK),__raw_readl(S3C2410_GPFCON),__raw_readl(S3C2410_EXTINT0));
		ret = ide_register_hw(&hw_ide, NULL);
		printk(KERN_INFO "-1:no 0:ok ide_register_hw return %d\n", ret);
	}

}

#endif
//EXPORT_SYMBOL(ide_init_default_hwifs);
