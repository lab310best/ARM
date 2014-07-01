#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

#define DEVICE_NAME	"leds"
#define LED_MAJOR 231

static unsigned long led_table [] = {
       S3C2410_GPF3,
       S3C2410_GPF4,
	S3C2410_GPF5,
	S3C2410_GPF6,
	S3C2410_GPF7,
	
};

static unsigned int led_cfg_table [] = {
	S3C2410_GPF3_OUTP,
	S3C2410_GPF4_OUTP,
	S3C2410_GPF5_OUTP,
	S3C2410_GPF6_OUTP,
	S3C2410_GPF7_OUTP,
	
};

static int s3c2440_leds_ioctl(
	struct inode *inode, 
	struct file *file, 
	unsigned int cmd, 
	unsigned long arg)
{
	switch(cmd) {
	case 0:
	case 1:
		if (arg > 4) {
			return -EINVAL;
		}
		s3c2410_gpio_setpin(led_table[arg], !cmd);
		return 0;
	default:
		return -EINVAL;
	}
}

static struct file_operations s3c2440_leds_fops = {
	.owner	=	THIS_MODULE,
	.ioctl	=	s3c2440_leds_ioctl,
};

static int __init s3c2440_leds_init(void)
{
	int ret;
	int i;

	ret = register_chrdev(LED_MAJOR, DEVICE_NAME, &s3c2440_leds_fops);
	if (ret < 0) {
	  printk(DEVICE_NAME " can't register major number\n");
	  return ret;
	}

	devfs_mk_cdev(MKDEV(LED_MAJOR, 0), S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP, DEVICE_NAME);
	
	for (i = 0; i < 5; i++) {
		s3c2410_gpio_cfgpin(led_table[i], led_cfg_table[i]);
//		printk("led%d_con:0x%x\n",i,s3c2410_gpio_getcfg(led_table[i]));
		s3c2410_gpio_setpin(led_table[i], 1);
	}

	printk(DEVICE_NAME " initialized\n");
	return 0;
}

static void __exit s3c2440_leds_exit(void)
{
	devfs_remove(DEVICE_NAME);
	unregister_chrdev(LED_MAJOR, DEVICE_NAME);
}

module_init(s3c2440_leds_init);
module_exit(s3c2440_leds_exit);
