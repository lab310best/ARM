/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Copyright (c) 2004 Arnaud Patard <arnaud.patard@rtp-net.org>
 * iPAQ H1940 touchscreen support
 *
 * ChangeLog
 *
 * 2004-09-05: Herbert Pötzl <herbert@13thfloor.at>
 *	- added clock (de-)allocation code
 *
 * 2005-03-06: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - h1940_ -> s3c2410 (this driver is now also used on the n30
 *        machines :P)
 *      - Debug messages are now enabled with the config option
 *        TOUCHSCREEN_S3C2410_DEBUG
 *      - Changed the way the value are read
 *      - Input subsystem should now work
 *      - Use ioremap and readl/writel
 *
 * 2005-03-23: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - Make use of some undocumented features of the touchscreen
 *        controller
 *
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/devfs_fs_kernel.h>   //fla


#define SUBSRCPND (0x4A000018)
#define INTSUBMSK (0x4A00001C)
#define INTMSK    (0x4A000008)
static unsigned long subsrcpnd,intsubmsk,intmsk;

#include <asm/arch/regs-adc.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/s3c2410_ts.h>
#include <asm/hardware/clock.h>
#include <asm/arch/regs-irq.h>
/* For ts.dev.id.version */
#define S3C2410TSVERSION	0x0101
//#define CONFIG_TOUCHSCREEN_S3C2410_DEBUG 1//fla
#define WAIT4INT(x)  (((x)<<8) | \
		     S3C2410_ADCTSC_YM_SEN | S3C2410_ADCTSC_YP_SEN | S3C2410_ADCTSC_XP_SEN | \
		     S3C2410_ADCTSC_XY_PST(3))

#define AUTOPST	     (S3C2410_ADCTSC_YM_SEN | S3C2410_ADCTSC_YP_SEN | S3C2410_ADCTSC_XP_SEN | \
		     S3C2410_ADCTSC_AUTO_PST | S3C2410_ADCTSC_XY_PST(0))

#define DEBUG_LVL    KERN_DEBUG

MODULE_AUTHOR("Arnaud Patard <arnaud.patard@rtp-net.org>");
MODULE_DESCRIPTION("s3c2410 touchscreen driver");
MODULE_LICENSE("GPL");

/*
 * Definitions & global arrays.
 */


static char *s3c2410ts_name = "s3c2410 TouchScreen";

/*
 * Per-touchscreen data.
 */

struct s3c2410ts {
	struct input_dev dev;
	long xp;
	long yp;
	int count;
	int shift;
	char phys[32];  
};

static struct s3c2410ts ts;
static void __iomem *base_addr;

static inline void s3c2410_ts_connect(void)
{
	s3c2410_gpio_cfgpin(S3C2410_GPG12, S3C2410_GPG12_XMON);
	s3c2410_gpio_cfgpin(S3C2410_GPG13, S3C2410_GPG13_nXPON);
	s3c2410_gpio_cfgpin(S3C2410_GPG14, S3C2410_GPG14_YMON);
	s3c2410_gpio_cfgpin(S3C2410_GPG15, S3C2410_GPG15_nYPON);
}

static void touch_timer_fire(unsigned long data)
{
  	unsigned long data0;
  	unsigned long data1;
	int updown;

  	data0 = readl(base_addr+S3C2410_ADCDAT0);
  	data1 = readl(base_addr+S3C2410_ADCDAT1);

 	updown = (!(data0 & S3C2410_ADCDAT0_UPDOWN)) && (!(data1 & S3C2410_ADCDAT0_UPDOWN));

 	if (updown) {
 		if (ts.count != 0) {
 			ts.xp >>= ts.shift;
 			ts.yp >>= ts.shift;

#ifdef CONFIG_TOUCHSCREEN_S3C2410_DEBUG
 			{
 				struct timeval tv;
 				do_gettimeofday(&tv);
 				printk(DEBUG_LVL "T: %06d, X: %03ld, Y: %03ld\n", (int)tv.tv_usec, ts.xp, ts.yp);
 			}
#endif

 			input_report_abs(&ts.dev, ABS_X, ts.xp);
 			input_report_abs(&ts.dev, ABS_Y, ts.yp);

 			input_report_key(&ts.dev, BTN_TOUCH, 1);
 			input_report_abs(&ts.dev, ABS_PRESSURE, 1);
 			input_sync(&ts.dev);
 		}

 		ts.xp = 0;
 		ts.yp = 0;
 		ts.count = 0;

 		writel(S3C2410_ADCTSC_PULL_UP_DISABLE | AUTOPST, base_addr+S3C2410_ADCTSC);
 		writel(readl(base_addr+S3C2410_ADCCON) | S3C2410_ADCCON_ENABLE_START, base_addr+S3C2410_ADCCON);
 	} else {
 		ts.count = 0;

 		input_report_key(&ts.dev, BTN_TOUCH, 0);
 		input_report_abs(&ts.dev, ABS_PRESSURE, 0);
 		input_sync(&ts.dev);

 		writel(WAIT4INT(0), base_addr+S3C2410_ADCTSC);
 	}
}

static struct timer_list touch_timer =
		TIMER_INITIALIZER(touch_timer_fire, 0, 0);

static irqreturn_t stylus_updown(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long data0;
	unsigned long data1;
	int updown;
  printk(KERN_ERR "hello,in stylus_updow !!!!!!\n");
	data0 = readl(base_addr+S3C2410_ADCDAT0);
	data1 = readl(base_addr+S3C2410_ADCDAT1);

	updown = (!(data0 & S3C2410_ADCDAT0_UPDOWN)) && (!(data1 & S3C2410_ADCDAT0_UPDOWN));

	/* TODO we should never get an interrupt with updown set while
	 * the timer is running, but maybe we ought to verify that the
	 * timer isn't running anyways. */

	if (updown)
		touch_timer_fire(0);

	return IRQ_HANDLED;
}


static irqreturn_t stylus_action(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long data0;
	unsigned long data1;
   printk(KERN_ERR "hello,in stylus_ACTION!!!!!!!!!!!\n");
	data0 = readl(base_addr+S3C2410_ADCDAT0);
	data1 = readl(base_addr+S3C2410_ADCDAT1);

	ts.xp += data0 & S3C2410_ADCDAT0_XPDATA_MASK;
	ts.yp += data1 & S3C2410_ADCDAT1_YPDATA_MASK;
	ts.count++;

        if (ts.count < (1<<ts.shift)) {
		writel(S3C2410_ADCTSC_PULL_UP_DISABLE | AUTOPST, base_addr+S3C2410_ADCTSC);
		writel(readl(base_addr+S3C2410_ADCCON) | S3C2410_ADCCON_ENABLE_START, base_addr+S3C2410_ADCCON);
	} else {
		mod_timer(&touch_timer, jiffies+1);
		writel(WAIT4INT(1), base_addr+S3C2410_ADCTSC);
	}

	return IRQ_HANDLED;
}

static struct clk	*adc_clock;

/*
 * The functions for inserting/removing us as a module.
 */

static int __init s3c2410ts_probe(struct device *dev)
{
	struct s3c2410_ts_mach_info *info;
  printk(KERN_ERR "enter s3c2410ts_probe()!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	info = ( struct s3c2410_ts_mach_info *)dev->platform_data;
	
	//-----------------------------------fla
	//devfs_mk_dir("touchscreen");
//	devfs_mk_cdev(MKDEV(100, 1), S_IFCHR|S_IRUGO|S_IWUSR, "touchscreen/%d", 0); //fla  TSRAW_MINOR -->1
	//-----------------------------------fla
 
 	

	if (!info)
	{
		//printk(KERN_ERR "Hm... too bad : no platform data for ts\n");
		return -EINVAL;
	}else printk(KERN_ERR "ok!have platform data for ts!\n");

#ifdef CONFIG_TOUCHSCREEN_S3C2410_DEBUG
	printk(DEBUG_LVL "Entering s3c2410ts_init\n");
#endif

	/*adc_clock = clk_get(NULL, "adc");
	if (!adc_clock) {
		//printk(KERN_ERR "failed to get adc clock source\n");
		return -ENOENT;
	}
	clk_use(adc_clock);
	clk_enable(adc_clock);
*/

#ifdef CONFIG_TOUCHSCREEN_S3C2410_DEBUG
	printk(DEBUG_LVL "got and enabled clock\n");
#endif

	base_addr=ioremap(S3C2410_PA_ADC,0x20); 
	printk(KERN_ERR "HI!!!ADCTSC1 = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC));
	if (base_addr == NULL) {
		//printk(KERN_ERR "Failed to remap register block\n");
		return -ENOMEM;
		printk(KERN_ERR "HI!!!ADCTSC2 = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC));
	}


	/* Configure GPIOs */
	//s3c2410_ts_connect();
	printk(KERN_ERR "HI!!!ADCTSC3 = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC));


	if ((info->presc&0xff) > 0)
		{writel(S3C2410_ADCCON_PRSCEN | S3C2410_ADCCON_PRSCVL(info->presc&0xFF),\
			     base_addr+S3C2410_ADCCON);
		printk(KERN_ERR "HI!!!ADCTSC4 = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC));}
	else
		{writel(0,base_addr+S3C2410_ADCCON);
	printk(KERN_ERR "HI!!!ADCTSC5 = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC));}


	/* Initialise registers */
	if ((info->delay&0xffff) > 0)
		{writel(info->delay & 0xffff,  base_addr+S3C2410_ADCDLY);
	printk(KERN_ERR "HI!!!ADCTSC6 = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC)); }

	writel(WAIT4INT(0), base_addr+S3C2410_ADCTSC); 
	printk(KERN_ERR "HI!!!ADCTSC7 = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC));
	//printk(KERN_ERR "line %d passed",__LINE__);
	
  __raw_writel(__raw_readl(S3C2410_GPGCON)|(0xff<<24), S3C2410_GPGCON);//fla
   printk(KERN_ERR "HI!!!ADCTSC8 = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC));
  //printk(KERN_ERR "line %d passed",__LINE__);

 __raw_writel(30000, base_addr+S3C2410_ADCDLY);	//added by hzh
  printk(KERN_ERR "HI!!!ADCTSC9 = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC));
//printk(KERN_ERR "line %d passed",__LINE__);

	/* Initialise input stuff */
	memset(&ts, 0, sizeof(struct s3c2410ts));
	init_input_dev(&ts.dev);
	printk(KERN_ERR "HI!!!ADCTSC10 = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC));
	ts.dev.evbit[0] = ts.dev.evbit[0] = BIT(EV_SYN) | BIT(EV_KEY) | BIT(EV_ABS);
	ts.dev.keybit[LONG(BTN_TOUCH)] = BIT(BTN_TOUCH);
	input_set_abs_params(&ts.dev, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(&ts.dev, ABS_Y, 0, 0x3FF, 0, 0);
	input_set_abs_params(&ts.dev, ABS_PRESSURE, 0, 1, 0, 0);

	sprintf(ts.phys, "ts0");

	ts.dev.private = &ts;
	ts.dev.name = s3c2410ts_name;
	ts.dev.phys = ts.phys;
	ts.dev.id.bustype = BUS_RS232;
	ts.dev.id.vendor = 0xDEAD;
	ts.dev.id.product = 0xBEEF;
	ts.dev.id.version = S3C2410TSVERSION;

	ts.shift = info->oversampling_shift;
  printk(KERN_ERR "HI!!!ADCTSC11 = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC));
	/* Get irqs */
	if (request_irq(IRQ_ADC, stylus_action, SA_SAMPLE_RANDOM,
		"s3c2410_action", &ts.dev)) {
		//printk(KERN_ERR "s3c2410_ts.c: Could not allocate ts IRQ_ADC !\n");
		iounmap(base_addr);
		return -EIO;
	}
	if (request_irq(IRQ_TC, stylus_updown, SA_SAMPLE_RANDOM,
			"s3c2410_action", &ts.dev)) {
		//printk(KERN_ERR "s3c2410_ts.c: Could not allocate ts IRQ_TC !\n");
		iounmap(base_addr);
		return -EIO;
	}

/*
printk(KERN_ERR "line %d passed!\n",__LINE__);
printk(KERN_ERR "look:ADCCON = %08x!!!!!!!!!!!!!\n",__raw_readl(S3C2410_ADCCON));
printk(KERN_ERR "line %d passed!\n",__LINE__);
  
printk(KERN_ERR "look:ADCTSC = %08x!!!!!!!!!!!!!\n",readl(S3C2410_ADCTSC));
printk(KERN_ERR "line %d passed!\n",__LINE__);
 
 printk(KERN_ERR "look:INTMSK = %08x!!!!!!!!!!!!!\n",readl(S3C2410_INTMSK));
printk(KERN_ERR "line %d passed!\n",__LINE__);
 
 printk(KERN_ERR "look:INTSUBMSK = %08x!!!!!!!!!!!!!\n",readl(S3C2410_INTSUBMSK));
printk(KERN_ERR "line %d passed!\n",__LINE__);
 
*/
 printk(KERN_ERR "%s successfully loaded!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", s3c2410ts_name);
 printk(KERN_ERR "HI!!!ADCTSC12 = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC));
	/* All went ok, so register to the input system */
	input_register_device(&ts.dev);
	printk(KERN_ERR "HI!!!ADCTSC13 = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC));

	return 0;
}

static int s3c2410ts_remove(struct device *dev)
{
	disable_irq(IRQ_ADC);
	disable_irq(IRQ_TC);
	free_irq(IRQ_TC,&ts.dev);
	free_irq(IRQ_ADC,&ts.dev);

	if (adc_clock) {
		clk_disable(adc_clock);
		clk_unuse(adc_clock);
		clk_put(adc_clock);
		adc_clock = NULL;
	}

	input_unregister_device(&ts.dev);
	iounmap(base_addr);

	return 0;
}

static struct device_driver s3c2410ts_driver = {
       .name           = "s3c2410-ts",
       .bus            = &platform_bus_type,
       .probe          = s3c2410ts_probe,
       .remove         = s3c2410ts_remove,
};


int __init s3c2410ts_init(void)
{    //unsigned long adctsc = ioremap(0x58000004,0x00000004);
    // printk(KERN_ERR "HI!!!ADCTSC = 0x%08x \n!!!",__raw_readl(base_addr+S3C2410_ADCTSC));
     return driver_register(&s3c2410ts_driver);
}

void __exit s3c2410ts_exit(void)
{
	driver_unregister(&s3c2410ts_driver);
}

module_init(s3c2410ts_init);
module_exit(s3c2410ts_exit);

/*
    Local variables:
        compile-command: "make ARCH=arm CROSS_COMPILE=/usr/local/arm/3.3.2/bin/arm-linux- -k -C ../../.."
        c-basic-offset: 8
    End:
*/
