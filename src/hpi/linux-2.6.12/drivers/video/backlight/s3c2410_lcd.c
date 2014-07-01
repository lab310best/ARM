/*
 * linux/drivers/video/backlight/s3c2410_lcd.c
 * Copyright (c) Arnaud Patard <arnaud.patard@rtp-net.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	    S3C2410 LCD Controller Backlight Driver
 *
 * ChangeLog
 *
 * 2005-04-07: Arnaud Patard <arnaud.patard@rtp-net.org>
 *   - Renamed to s3c2410_lcd.c as it include also lcd power controls
 *
 * 2005-03-17: Arnaud Patard <arnaud.patard@rtp-net.org>
 *   - First version
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/fb.h>
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
#include <linux/backlight.h>
#endif
#ifdef CONFIG_LCD_CLASS_DEVICE
#include <linux/lcd.h>
#endif
#include <asm/arch/s3c2410_lcd.h>
#include <asm/arch/regs-gpio.h>

struct s3c2410bl_devs {
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	struct backlight_device *bl;
#endif
#ifdef CONFIG_LCD_CLASS_DEVICE
	struct lcd_device	*lcd;
#endif
};

#ifdef CONFIG_LCD_CLASS_DEVICE
static int s3c2410bl_get_lcd_power(struct lcd_device *lcd)
{
	struct s3c2410_bl_mach_info *info;

	info = (struct s3c2410_bl_mach_info *)class_get_devdata(&lcd->class_dev);

	if (info)
		return info->lcd_power_value;

	return 0;
}

static int s3c2410bl_set_lcd_power(struct lcd_device *lcd, int power)
{
	struct s3c2410_bl_mach_info *info;

	info = (struct s3c2410_bl_mach_info *)class_get_devdata(&lcd->class_dev);

	if (info && info->lcd_power) {
		info->lcd_power_value=power;
		switch(power) {
			case FB_BLANK_NORMAL:
			case FB_BLANK_POWERDOWN:
				info->lcd_power(0);
				break;
			default:
			case FB_BLANK_VSYNC_SUSPEND:
			case FB_BLANK_HSYNC_SUSPEND:
			case FB_BLANK_UNBLANK:
				info->lcd_power(1);
				break;
		}
	}

	return 0;
}
#endif
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
static int s3c2410bl_get_bl_power(struct backlight_device *bl)
{
	struct s3c2410_bl_mach_info *info;

	info = (struct s3c2410_bl_mach_info *)class_get_devdata(&bl->class_dev);

	if (info)
		return info->backlight_power_value;

	return 0;
}

static int s3c2410bl_set_bl_power(struct backlight_device *bl, int power)
{
	struct s3c2410_bl_mach_info *info;

	info = (struct s3c2410_bl_mach_info *)class_get_devdata(&bl->class_dev);

	power = !!power;

	if (info && info->backlight_power) {
		info->backlight_power_value=power;
		switch(power) {
			case FB_BLANK_NORMAL:
			case FB_BLANK_VSYNC_SUSPEND:
			case FB_BLANK_HSYNC_SUSPEND:
			case FB_BLANK_POWERDOWN:
				info->backlight_power(0);
				break;
			default:
			case FB_BLANK_UNBLANK:
				info->backlight_power(1);
				break;
		}
	}

	return 0;
}

static int s3c2410bl_get_bl_brightness(struct backlight_device *bl)
{
	struct s3c2410_bl_mach_info *info;

	info = (struct s3c2410_bl_mach_info *)class_get_devdata(&bl->class_dev);

	if(info)
		return info->brightness_value;

	return 0;
}

static int s3c2410bl_set_bl_brightness(struct backlight_device *bl, int brightness)
{
	struct s3c2410_bl_mach_info *info;

	info = (struct s3c2410_bl_mach_info *)class_get_devdata(&bl->class_dev);

	if (info && info->set_brightness) {
		if ( (brightness <= info->backlight_max) && brightness )
		{
			info->brightness_value=brightness;
			info->set_brightness(brightness);
		}
		else
			return -1;
	}

	return 0;
}
#endif

static int is_s3c2410fb(struct fb_info *info)
{
	return (!strcmp(info->fix.id,"s3c2410fb"));
}

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
static struct backlight_properties s3c2410bl_props = {
	.owner    	= THIS_MODULE,
	.get_power	= s3c2410bl_get_bl_power,
	.set_power	= s3c2410bl_set_bl_power,
	.get_brightness = s3c2410bl_get_bl_brightness,
	.set_brightness	= s3c2410bl_set_bl_brightness,
	.check_fb 	= is_s3c2410fb
};
#endif
#ifdef CONFIG_LCD_CLASS_DEVICE
static struct lcd_properties s3c2410lcd_props = {
	.owner		= THIS_MODULE,
	.get_power	= s3c2410bl_get_lcd_power,
	.set_power	= s3c2410bl_set_lcd_power,
	.check_fb	= is_s3c2410fb
};
#endif

static int __init s3c2410bl_probe(struct device *dev)
{
	struct s3c2410bl_devs *devs;
	struct s3c2410_bl_mach_info *info;

	info = ( struct s3c2410_bl_mach_info *)dev->platform_data;

	/* Register the backlight device */
	if (!info) {
		printk(KERN_ERR "Hm... too bad : no platform data for bl\n");
	}
	else {
		s3c2410bl_props.max_brightness = info->backlight_max;
	}

	devs = (struct s3c2410bl_devs *)kmalloc(sizeof(*devs),GFP_KERNEL);
	if (!devs) {
		return -ENOMEM;
	}

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	devs->bl = backlight_device_register ("s3c2410-bl",info, &s3c2410bl_props);

	if (IS_ERR (devs->bl)) {
		kfree(devs);
		return PTR_ERR (devs->bl);
	}
#endif
#ifdef CONFIG_LCD_CLASS_DEVICE
	devs->lcd = lcd_device_register("s3c2410-lcd",info,&s3c2410lcd_props);

	if (IS_ERR (devs->bl)) {
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
		backlight_device_unregister(devs->bl);
#endif
		kfree(devs);
		return PTR_ERR (devs->lcd);
	}
#endif
	dev_set_drvdata(dev, devs);

	/* Set power */
	s3c2410bl_set_bl_power(devs->bl,1);

	/* Set default brightness */
	s3c2410bl_set_bl_brightness(devs->bl,info->backlight_default);

	printk(KERN_ERR "s3c2410 Backlight Driver Initialized.\n");
	return 0;
}

static int s3c2410bl_remove(struct device *dev)
{
	struct s3c2410bl_devs *devs = dev->driver_data;

	if (devs) {
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
		if (devs->bl) {
			backlight_device_unregister(devs->bl);
			s3c2410bl_set_bl_brightness(devs->bl,0);
		}
#endif
#ifdef CONFIG_LCD_CLASS_DEVICE
		if (devs->lcd) {
			lcd_device_unregister(devs->lcd);
		}
#endif
		kfree(devs);
	}
	
	printk("s3c2410 Backlight Driver Unloaded\n");

	return 0;

}

static struct device_driver s3c2410bl_driver = {
	.name		= "s3c2410-bl",
	.bus		= &platform_bus_type,
	.probe		= s3c2410bl_probe,
	.remove		= s3c2410bl_remove,
};



static int __init s3c2410bl_init(void)
{
	return driver_register(&s3c2410bl_driver);
}

static void __exit s3c2410fb_cleanup(void)
{
	driver_unregister(&s3c2410bl_driver);
}

module_init(s3c2410bl_init);
module_exit(s3c2410fb_cleanup);

MODULE_AUTHOR("Arnaud Patard <arnaud.patard@rtp-net.org>");
MODULE_DESCRIPTION("s3c2410 Backlight Driver");
MODULE_LICENSE("GPLv2");

