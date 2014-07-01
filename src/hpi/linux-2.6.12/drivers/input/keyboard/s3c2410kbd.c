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
 * Copyright (c) 2005 Arnaud Patard <arnaud.patard@rtp-net.org>
 * Samsung S3C2410 keyboard support
 *
 * Based on various pxa ipaq drivers.
 * 
 * ChangeLog
 *
 * 2005-06-21: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - Initial version
 *
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/arch/regs-gpio.h>

/* For id.version */
#define S3C2410KBDVERSION	0x0001
#define DRV_NAME		"s3c2410-kbd"

MODULE_AUTHOR("Arnaud Patard <arnaud.patard@rtp-net.org>");
MODULE_DESCRIPTION("s3c2410 keyboard driver");
MODULE_LICENSE("GPL");

struct s3c2410kbd_button {
	int irq;
	int pin;
	int pin_setting;
	int keycode;
	char *name;
};

/* To be moved later to a better place */
static struct s3c2410kbd_button s3c2410kbd_buttons[] = {
	{  IRQ_EINT0,  S3C2410_GPF0,   S3C2410_GPF0_EINT0,    KEY_POWER,	      "Power" },
	{  IRQ_EINT6,  S3C2410_GPF6,   S3C2410_GPF6_EINT6,    KEY_ENTER, 	     "Select" },
	{  IRQ_EINT7,  S3C2410_GPF7,   S3C2410_GPF7_EINT7,   KEY_RECORD,	     "Record" },
	{  IRQ_EINT8,  S3C2410_GPG0,   S3C2410_GPG0_EINT8, KEY_CALENDAR,	   "Calendar" },
	{ IRQ_EINT10,  S3C2410_GPG2,  S3C2410_GPG2_EINT10,   KEY_COFFEE,	   "Contacts" }, /* TODO: find a better key :P */
	{ IRQ_EINT11,  S3C2410_GPG3,  S3C2410_GPG3_EINT11,     KEY_MAIL,	       "Mail" },
	{ IRQ_EINT14,  S3C2410_GPG6,  S3C2410_GPG6_EINT14,     KEY_LEFT,	 "Left_arrow" },
	{ IRQ_EINT15,  S3C2410_GPG7,  S3C2410_GPG7_EINT15, KEY_HOMEPAGE,	       "Home" },
	{ IRQ_EINT16,  S3C2410_GPG8,  S3C2410_GPG8_EINT16,    KEY_RIGHT,	"Right_arrow" },
	{ IRQ_EINT17,  S3C2410_GPG9,  S3C2410_GPG9_EINT17,       KEY_UP,	   "Up_arrow" },
	{ IRQ_EINT18, S3C2410_GPG10, S3C2410_GPG10_EINT18,     KEY_DOWN,	 "Down_arrow" },
};

struct s3c2410kbd {
	struct input_dev	dev;
	spinlock_t		lock;
	int 			count;
	int			shift;	
	char			phys[32];
};

static struct s3c2410kbd kbd;

static irqreturn_t s3c2410kbd_keyevent(int irq, void *dev_id, struct pt_regs *regs)
{
	struct s3c2410kbd_button *button = (struct s3c2410kbd_button *)dev_id;
	int down;

	if (!button)
		return IRQ_HANDLED;

	down = !(s3c2410_gpio_getpin(button->pin));
	
	printk(KERN_DEBUG "%s button %s\n",button->name, down ? "pressed" : "released");

	input_report_key(&kbd.dev, button->keycode, down);
	input_sync(&kbd.dev);
	
	return IRQ_HANDLED;
}

static int __init s3c2410kbd_probe(struct device *dev)
{
	int i;

	/* Initialise input stuff */
	memset(&kbd, 0, sizeof(struct s3c2410kbd));
	init_input_dev(&kbd.dev);
	kbd.dev.evbit[0] = BIT(EV_KEY);
	sprintf(kbd.phys, "input/s3c2410_kbd0");

	kbd.dev.private = &kbd;
	kbd.dev.name = DRV_NAME;
	kbd.dev.phys = kbd.phys;
	kbd.dev.id.bustype = BUS_HOST;
	kbd.dev.id.vendor = 0xDEAD;
	kbd.dev.id.product = 0xBEEF;
	kbd.dev.id.version = S3C2410KBDVERSION;


	for (i = 0; i < ARRAY_SIZE (s3c2410kbd_buttons); i++) {
		set_bit(s3c2410kbd_buttons[i].keycode, kbd.dev.keybit);
		s3c2410_gpio_cfgpin(s3c2410kbd_buttons[i].pin,s3c2410kbd_buttons[i].pin_setting);
		request_irq (s3c2410kbd_buttons[i].irq, s3c2410kbd_keyevent,\
				SA_SAMPLE_RANDOM, s3c2410kbd_buttons[i].name, &s3c2410kbd_buttons[i]);
		set_irq_type(s3c2410kbd_buttons[i].irq, IRQT_BOTHEDGE);
	}
	
	printk(KERN_INFO "%s successfully loaded\n", DRV_NAME);

	/* All went ok, so register to the input system */
	input_register_device(&kbd.dev);

	return 0;
}

static int s3c2410kbd_remove(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE (s3c2410kbd_buttons); i++) {
		disable_irq(s3c2410kbd_buttons[i].irq);
		free_irq(s3c2410kbd_buttons[i].irq,&kbd.dev);
	}

	input_unregister_device(&kbd.dev);

	return 0;
}


static struct device_driver s3c2410kbd_driver = {
       .name           = DRV_NAME,
       .bus            = &platform_bus_type,
       .probe          = s3c2410kbd_probe,
       .remove         = s3c2410kbd_remove,
};


int __init s3c2410kbd_init(void)
{
	return driver_register(&s3c2410kbd_driver);
}

void __exit s3c2410kbd_exit(void)
{
	driver_unregister(&s3c2410kbd_driver);
}

module_init(s3c2410kbd_init);
module_exit(s3c2410kbd_exit);




