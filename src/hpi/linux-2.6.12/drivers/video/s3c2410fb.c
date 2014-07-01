/*
 *  linux/drivers/video/pxafb.c
 *
 *  Copyright (C) 1999 Eric A. Thomas.
 *  Copyright (C) 2004 Jean-Frederic Clere.
 *  Copyright (C) 2004 Ian Campbell.
 *  Copyright (C) 2004 Jeff Lackey.
 *   Based on sa1100fb.c Copyright (C) 1999 Eric A. Thomas
 *  which in turn is
 *   Based on acornfb.c Copyright (C) Russell King.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	        Intel PXA250/210 LCD Controller Frame Buffer Driver
 *
 * Please direct your questions and comments on this driver to the following
 * email address:
 *
 *	linux-arm-kernel@lists.arm.linux.org.uk
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
/*#include <asm/arch/bitfield.h>*/
#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-lcd.h>
#include <asm/arch/regs-irq.h>
/*
 * Complain if VAR is out of range.
 */
#define DEBUG_VAR 1
#define	ClearPending(x)	{	\
			  __raw_writel((1 << (x)), S3C2410_SRCPND);	\
			  __raw_writel((1 << (x)), S3C2410_INTPND);	\
			}

#include "s3c2410fb.h"

/*
#define BSWP         (0)                  // Byte swap control
#define HWSWP        (1)                  // Half word swap control

#define LCD_XSIZE_TFT_320240 	320//(240)	
#define LCD_YSIZE_TFT_320240 	240//(320)

#define SCR_XSIZE_TFT_320240 	320//(240)
#define SCR_YSIZE_TFT_320240 	240//(320)

#define HOZVAL_TFT_320240       (LCD_XSIZE_TFT_320240 - 1)
#define LINEVAL_TFT_320240      (LCD_YSIZE_TFT_320240 - 1)

#define CLKVAL_TFT_320240	(6) 		// FCLK = 400MHz, HCLK = 100MHz, VCLK = HCLK / [(CLKVAL+1) * 2]  (CLKVAL >= 0)

#define VBPD_320240		(4)		//垂直同步信号的后肩
#define VFPD_320240		(4)		//垂直同步信号的前肩
#define VSPW_320240		(4)		//垂直同步信号的脉宽

#define HBPD_320240		(13)		//水平同步信号的后肩
#define HFPD_320240		(4)		//水平同步信号的前肩
#define HSPW_320240		(18)		//水平同步信号的脉宽

*/
struct pxafb_mach_info S3C2410_800X600_info = {
	.pixclock	= 174757,
	.xres		= 800,
	.yres		= 600,
	.bpp		= 16,
	.hsync_len	= 119,//95,
	.left_margin	= 55,//23,
	.right_margin	= 63,//39,
	.vsync_len	= 5,//1,
	.upper_margin	= 36,//10,
	.lower_margin	= 22,//31,
	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	.cmap_greyscale	= 0,
	.cmap_inverse	= 0,
	.cmap_static	= 0,
	.reg		= {
		.lcdcon1 = (1<<8)|(1<<7)|(3<<5)|(12<<1),
		.lcdcon2 = (22<<24)|(599<<14)|(36<<6)|5,
		.lcdcon3 = (63<<19)|(799<<8)|55,
		.lcdcon4 = (13<<8)|119,
		.lcdcon5 = (1<<11)|(1<<10)|(0<<9)|(0<<8)|(1<<3)|1,
	}
};

struct pxafb_mach_info S3C2410_640X480_info_vga = {
	.pixclock	= 341521,  //5000
	.xres		= 640,
	.yres		= 480,
	.bpp		= 16,
	.hsync_len	= 32,  //95,
	.left_margin	= 24,  //23,
	.right_margin	= 26,  //39,
	.vsync_len	= 2,   //1,
	.upper_margin	= 11,  //10,
	.lower_margin	= 1,   //31,
	.sync		= 0,   //FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	.cmap_greyscale	= 0,
	.cmap_inverse	= 0,
	.cmap_static	= 1,  //0,
	.reg		= {
		lcdcon1 : (1<<8)|(1<<7)|(3<<5)|(12<<1),
		lcdcon2 : (25<<24)|(479<<14)|(5<<6)|1,
		lcdcon3 : (67<<19)|(639<<8)|40,
		lcdcon4 : (13<<8)|31,
		lcdcon5 : (1<<11)|(0<<10)|(1<<9)|(1<<8)|(0<<7)|(0<<6)|(1<<3)|1,
	}
};
struct pxafb_mach_info S3C2410_640X480_info_tv = {
	.pixclock	= 5000,
	.xres		= 640,
	.yres		= 480,
	.bpp		= 16,
	.hsync_len	= 31,
	.left_margin	= 40,
	.right_margin	= 67,
	.vsync_len	= 1,
	.upper_margin	= 5,
	.lower_margin	= 25,
	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	.cmap_greyscale	= 0,
	.cmap_inverse	= 0,
	.cmap_static	= 0,
	.reg		= {
		lcdcon1 : (1<<8)|(1<<7)|(3<<5)|(12<<1),
		lcdcon2 : (26<<24)|(479<<14)|(115<<6)|1,
		lcdcon3 : (116<<19)|(639<<8)|80,
		lcdcon4 : (13<<8)|1,
		lcdcon5 : (1<<11)|(0<<10)|(1<<9)|(1<<8)|(0<<7)|(0<<6)|(1<<3)|1,
	}
};
/*
		lcdcon1 : (1<<8)|(1<<7)|(3<<5)|(12<<1),
		lcdcon2 : (26<<24)|(479<<14)|(115<<6)|1,
		lcdcon3 : (116<<19)|(639<<8)|80,
		lcdcon4 : (13<<8)|1,
		lcdcon5 : (1<<11)|(0<<10)|(1<<9)|(1<<8)|(0<<7)|(0<<6)|(1<<3)|1,
	}
};
*/
struct pxafb_mach_info S3C2410_640X480_info_shp = {
	.pixclock	= 174757,
	.xres		= 640,
	.yres		= 480,
	.bpp		= 16,
	.hsync_len	= 95,
	.left_margin	= 23,
	.right_margin	= 39,
	.vsync_len	= 1,
	.upper_margin	= 10,
	.lower_margin	= 31,
	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	.cmap_greyscale	= 0,
	.cmap_inverse	= 0,
	.cmap_static	= 0,
	.reg		= {
		lcdcon1 : (1<<8)|(1<<7)|(3<<5)|(12<<1),
		lcdcon2 : (31<<24)|(479<<14)|(10<<6)|1,
		lcdcon3 : (39<<19)|(639<<8)|23,
		lcdcon4 : (13<<8)|95,
		lcdcon5 : (1<<11)|(1<<10)|(0<<9)|(0<<8)|(0<<7)|(0<<6)|(1<<3)|1,
	}
};

struct pxafb_mach_info S3C2410_240X320_info = {
	.pixclock	= 270000,
	.xres		= 240,
	.yres		= 320,
	.bpp		= 16,
	.hsync_len	= 6,
	.left_margin	= 8,
	.right_margin	= 8,
	.vsync_len	= 4,
	.upper_margin	= 2,
	.lower_margin	= 2,
	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	.cmap_greyscale	= 0,
	.cmap_inverse	= 0,
	.cmap_static	= 0,
	.reg		= {
		lcdcon1 : (7<<8)|(3<<5)|(12<<1),
		lcdcon2 : (2<<24)|(319<<14)|(2<<6)|4,
		lcdcon3 : (8<<19)|(239<<8)|8,
		lcdcon4 : (13<<8)|6,
		lcdcon5 : (1<<11)|(0<<9)|(0<<8)|(0<<6)|(1),
	}
};

struct pxafb_mach_info S3C2410_800X480_info = {              //test 800x480 by bwx
	.pixclock	= 270000,
	.xres		= 800,
	.yres		= 480,
	.bpp		= 16,
	.hsync_len	= 8,
	.left_margin	= 5,
	.right_margin	= 15,
	.vsync_len	= 15,
	.upper_margin	= 3,
	.lower_margin	= 5,
	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	.cmap_greyscale	= 0,
	.cmap_inverse	= 0,
	.cmap_static	= 0,
	.reg		= {
		.lcdcon1 = (1<<8)|(3<<5)|(12<<1),
		.lcdcon2 = (25<<24) | (479<<14) | (5<<6) | (1),
		.lcdcon3 = (67<<19) | (799<<8) | (40),
		.lcdcon4 = (13<<8) | (31),
		.lcdcon5 = (1<<11) | (0<<10) | (1<<9) | (1<<8) | (0<<7) | (0<<6) | (1<<3)  |(0<<1) | (1),
	}
};


struct pxafb_mach_info S3C2410_320X240_info = {
	.pixclock	= 270000,
	.xres		= 320,
	.yres		= 240,
	.bpp		= 16,
	.hsync_len	= 8,
	.left_margin	= 5,
	.right_margin	= 15,
	.vsync_len	= 15,
	.upper_margin	= 3,
	.lower_margin	= 5,
	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	.cmap_greyscale	= 0,
	.cmap_inverse	= 0,
	.cmap_static	= 0,
	.reg		= {
		.lcdcon1 = (6<<8)|(0<<7)|(3<<5)|(12<<1),
		.lcdcon2 = (3<<24) | (239<<14) | (5<<6) | (15),
		.lcdcon3 = (58<<19) | (319<<8) | (15),
		.lcdcon4 = (13<<8) | (8),
		.lcdcon5 = (1<<11) | (0<<10) | (1<<9) | (1<<8) | (0<<7) | (0<<6) | (1<<3)  |(0<<1) | (1),
	}
};


struct pxafb_mach_info fs2410_info;
static int TE2440_fb_setup(char *options)
{
# ifdef CONFIG_FB_PXA_PARAMETERS
	strlcpy(g_options, options, sizeof(g_options));
# endif
	if(!strncmp("vga800", options, 6))
		fs2410_info = S3C2410_800X600_info;
	else if(!strncmp("vga640", options, 6))
		fs2410_info = S3C2410_640X480_info_vga;
	else if(!strncmp("tv640", options, 6))
		fs2410_info = S3C2410_640X480_info_tv;
	else if(!strncmp("shp640", options, 6))
		fs2410_info = S3C2410_640X480_info_shp;
	else if(!strncmp("shp240", options, 6))
		fs2410_info = S3C2410_240X320_info;
	else if(!strncmp("sam320", options, 6))
		fs2410_info = S3C2410_320X240_info;
	else if(!strncmp("qch800", options, 6))
		fs2410_info = S3C2410_800X480_info;
	else 
		fs2410_info = S3C2410_320X240_info;
	return 1;
}
__setup("display=", TE2440_fb_setup);
/*
static struct pxafb_mach_info fs2410_info = {
	.pixclock	= 270000,
#ifdef	CONFIG_FB_S3C2410_800X600
	.xres		= 800,
	.yres		= 600,
#elif defined CONFIG_FB_S3C2410_640X480
	.xres		= 640,
	.yres		= 480,
#elif defined CONFIG_FB_S3C2410_240X320_SHARP
	.xres		= 240,
	.yres		= 320,
#else
	.xres		= 320,
	.yres		= 240,
#endif
	.bpp		= 16,
	.hsync_len	= 18,//1,
	.left_margin	= 4,//3,
	.right_margin	= 13,//3,
	.vsync_len	= 4,//1,
	.upper_margin	= 4,//0,
	.lower_margin	= 4,//0,
	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	.cmap_greyscale	= 0,
	.cmap_inverse	= 0,
	.cmap_static	= 0,
#ifdef CONFIG_FB_S3C2410_800X600
	.reg		= {
		.lcdcon1 = (1<<8)|(1<<7)|(3<<5)|(12<<1),
		.lcdcon2 = (22<<24)|(599<<14)|(1<<6)|2,
		.lcdcon3 = (128<<19)|(799<<8)|24,
		.lcdcon4 = (13<<8)|72,
		.lcdcon5 = (1<<11)|(0<<10)|(0<<9)|(0<<8)|(0<<7)|(0<<6)|1,
	}
#elif defined CONFIG_FB_S3C2410_640X480
  #ifdef CONFIG_FB_S3C2410_640X480_SHARP
	.reg		= {
		lcdcon1 : (3<<8)|(1<<7)|(3<<5)|(12<<1),
		lcdcon2 : (32<<24)|(479<<14)|(8<<6)|4,
		lcdcon3 : (48<<19)|(639<<8)|18,
		lcdcon4 : (13<<8)|96,
		lcdcon5 : (1<<11)|(0<<10)|(0<<9)|(0<<8)|(0<<7)|(0<<6)|(1<<3)|1,
	}
  #else	//PRIME_VIEW
	.reg		= {
	//	.lcdcon1 = (3<<8)|(1<<7)|(3<<5)|(12<<1),
	//	.lcdcon2 = (32<<24)|(479<<14)|(9<<6)|1,
	//	.lcdcon3 = (47<<19)|(639<<8)|15,
	//	.lcdcon4 = (13<<8)|95,
	//	.lcdcon5 = (1<<11)|(0<<10)|(1<<9)|(1<<8)|(0<<7)|(0<<6)|(1<<3)|1,
		lcdcon1 : (2<<8)|(1<<7)|(3<<5)|(12<<1),
		lcdcon2 : (16<<24)|(479<<14)|(12<<6)|2,
		lcdcon3 : (48<<19)|(639<<8)|16,
		lcdcon4 : (13<<8)|96,
		lcdcon5 : (1<<11)|(0<<10)|(0<<9)|(0<<8)|(0<<7)|(0<<6)|(1<<3)|1,
	}
  #endif
#elif defined CONFIG_FB_S3C2410_240X320_SHARP
	.reg		= {
		lcdcon1 : (7<<8)|(3<<5)|(12<<1),
		lcdcon2 : (2<<24)|(319<<14)|(2<<6)|4,
		lcdcon3 : (8<<19)|(239<<8)|8,
		lcdcon4 : (13<<8)|6,
		lcdcon5 : (1<<11)|(0<<9)|(0<<8)|(0<<6)|(1),
	}
#elif	defined CONFIG_FB_S3C2410_240X320_SAMSUNG//Samsung 240X320 TFT LCD
	.reg		= {
		.lcdcon1 = (7<<8)|(0<<7)|(3<<5)|(12<<1),
		.lcdcon2 = (VBPD_320240<<24) | (LINEVAL_TFT_320240<<14) | (VFPD_320240<<6) | (VSPW_320240),
		.lcdcon3 = (HBPD_320240<<19) | (HOZVAL_TFT_320240<<8) | (HFPD_320240),
		.lcdcon4 = (13<<8) | (HSPW_320240),
		.lcdcon5 = (1<<11) | (1<<10) | (1<<9) | (1<<8) | (0<<7) | (0<<6) | (1<<3)  |(BSWP<<1) | (HWSWP),
	}
#else//Samsung 240X320 TFT LCD
	.reg		= {
		.lcdcon1 = (7<<8)|(0<<7)|(3<<5)|(12<<1),
		.lcdcon2 = 0xe4fc084,
		.lcdcon3 = 0x40ef08,
		.lcdcon4 = 0xd04,
		.lcdcon5 = (1<<11)|(1<<9)|(1<<8)|(1),
	}
#endif
};
*/
static void (*pxafb_backlight_power)(int);
static void (*pxafb_lcd_power)(int);

static int pxafb_activate_var(struct fb_var_screeninfo *var, struct pxafb_info *);
static void set_ctrlr_state(struct pxafb_info *fbi, u_int state);

#ifdef CONFIG_FB_PXA_PARAMETERS
#define PXAFB_OPTIONS_SIZE 256
static char g_options[PXAFB_OPTIONS_SIZE] __initdata = "";
#endif

static inline void pxafb_schedule_work(struct pxafb_info *fbi, u_int state)
{
	unsigned long flags;

	local_irq_save(flags);
	/*
	 * We need to handle two requests being made at the same time.
	 * There are two important cases:
	 *  1. When we are changing VT (C_REENABLE) while unblanking (C_ENABLE)
	 *     We must perform the unblanking, which will do our REENABLE for us.
	 *  2. When we are blanking, but immediately unblank before we have
	 *     blanked.  We do the "REENABLE" thing here as well, just to be sure.
	 */
	if (fbi->task_state == C_ENABLE && state == C_REENABLE)
		state = (u_int) -1;
	if (fbi->task_state == C_DISABLE && state == C_ENABLE)
		state = C_REENABLE;

	if (state != (u_int)-1) {
		fbi->task_state = state;
		schedule_work(&fbi->task);
	}
	local_irq_restore(flags);
}

static inline u_int chan_to_field(u_int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int
pxafb_setpalettereg(u_int regno, u_int red, u_int green, u_int blue,
		       u_int trans, struct fb_info *info)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	u_int val, ret = 1;

	if (regno < fbi->palette_size) {
		if (fbi->fb.var.grayscale) {
			val = ((blue >> 8) & 0x00ff);
		} else {
			val  = ((red   >>  0) & 0xf800);
			val |= ((green >>  5) & 0x07e0);
			val |= ((blue  >> 11) & 0x001f);
		}

		fbi->palette_cpu[regno] = val;
		ret = 0;
	}
	return ret;
}

static int
pxafb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
		   u_int trans, struct fb_info *info)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	unsigned int val;
	int ret = 1;

	/*
	 * If inverse mode was selected, invert all the colours
	 * rather than the register number.  The register number
	 * is what you poke into the framebuffer to produce the
	 * colour you requested.
	 */
	if (fbi->cmap_inverse) {
		red   = 0xffff - red;
		green = 0xffff - green;
		blue  = 0xffff - blue;
	}

	/*
	 * If greyscale is true, then we convert the RGB value
	 * to greyscale no matter what visual we are using.
	 */
	if (fbi->fb.var.grayscale)
		red = green = blue = (19595 * red + 38470 * green +
					7471 * blue) >> 16;

	switch (fbi->fb.fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/*
		 * 12 or 16-bit True Colour.  We encode the RGB value
		 * according to the RGB bitfield information.
		 */
		if (regno < 16) {
			u32 *pal = fbi->fb.pseudo_palette;

			val  = chan_to_field(red, &fbi->fb.var.red);
			val |= chan_to_field(green, &fbi->fb.var.green);
			val |= chan_to_field(blue, &fbi->fb.var.blue);

			pal[regno] = val;
			ret = 0;
		}
		break;

	case FB_VISUAL_STATIC_PSEUDOCOLOR:
	case FB_VISUAL_PSEUDOCOLOR:
		ret = pxafb_setpalettereg(regno, red, green, blue, trans, info);
		break;
	}

	return ret;
}

#ifdef CONFIG_CPU_FREQ
/*
 *  pxafb_display_dma_period()
 *    Calculate the minimum period (in picoseconds) between two DMA
 *    requests for the LCD controller.  If we hit this, it means we're
 *    doing nothing but LCD DMA.
 */
static unsigned int pxafb_display_dma_period(struct fb_var_screeninfo *var)
{
       /*
        * Period = pixclock * bits_per_byte * bytes_per_transfer
        *              / memory_bits_per_pixel;
        */
       return var->pixclock * 8 * 16 / var->bits_per_pixel;
}

extern unsigned int get_clk_frequency_khz(int info);
#endif

/*
 *  pxafb_check_var():
 *    Get the video params out of 'var'. If a value doesn't fit, round it up,
 *    if it's too big, return -EINVAL.
 *
 *    Round up in the following order: bits_per_pixel, xres,
 *    yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
 *    bitfields, horizontal timing, vertical timing.
 */
static int pxafb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;

	if (var->xres < MIN_XRES)
		var->xres = MIN_XRES;
	if (var->yres < MIN_YRES)
		var->yres = MIN_YRES;
	if (var->xres > fbi->max_xres)
		var->xres = fbi->max_xres;
	if (var->yres > fbi->max_yres)
		var->yres = fbi->max_yres;
	var->xres_virtual =
		max(var->xres_virtual, var->xres);
	var->yres_virtual =
		max(var->yres_virtual, var->yres);

        /*
	 * Setup the RGB parameters for this display.
	 *
	 * The pixel packing format is described on page 7-11 of the
	 * PXA2XX Developer's Manual.
         */
	if ( var->bits_per_pixel == 16 ) {
		var->red.offset   = 11; var->red.length   = 5;
		var->green.offset = 5;  var->green.length = 6;
		var->blue.offset  = 0;  var->blue.length  = 5;
		var->transp.offset = var->transp.length = 0;
	} else {
		var->red.offset = var->green.offset = var->blue.offset = var->transp.offset = 0;
		var->red.length   = 8;
		var->green.length = 8;
		var->blue.length  = 8;
		var->transp.length = 0;
	}

#ifdef CONFIG_CPU_FREQ
	DPRINTK("dma period = %d ps, clock = %d kHz\n",
		pxafb_display_dma_period(var),
		get_clk_frequency_khz(0));
#endif

	return 0;
}

static inline void pxafb_set_truecolor(u_int is_true_color)
{
	DPRINTK("true_color = %d\n", is_true_color);
	// do your machine-specific setup if needed
}

/*
 * pxafb_set_par():
 *	Set the user defined part of the display for the specified console
 */
static int pxafb_set_par(struct fb_info *info)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	struct fb_var_screeninfo *var = &info->var;
	unsigned long palette_mem_size;

	DPRINTK("set_par\n");

	if (var->bits_per_pixel == 16)
		fbi->fb.fix.visual = FB_VISUAL_TRUECOLOR;
	else if (!fbi->cmap_static)
		fbi->fb.fix.visual = FB_VISUAL_PSEUDOCOLOR;
	else {
		/*
		 * Some people have weird ideas about wanting static
		 * pseudocolor maps.  I suspect their user space
		 * applications are broken.
		 */
		fbi->fb.fix.visual = FB_VISUAL_STATIC_PSEUDOCOLOR;
	}

	fbi->fb.fix.line_length = var->xres_virtual *
				  var->bits_per_pixel / 8;
	if (var->bits_per_pixel == 16)
		fbi->palette_size = 0;
	else
		fbi->palette_size = var->bits_per_pixel == 1 ? 4 : 1 << var->bits_per_pixel;

	palette_mem_size = fbi->palette_size * sizeof(u16);

	DPRINTK("palette_mem_size = 0x%08lx\n", (u_long) palette_mem_size);

	fbi->palette_cpu = (u16 *)(fbi->map_cpu + PAGE_SIZE - palette_mem_size);
	fbi->palette_dma = fbi->map_dma + PAGE_SIZE - palette_mem_size;

	/*
	 * Set (any) board control register to handle new color depth
	 */
	pxafb_set_truecolor(fbi->fb.fix.visual == FB_VISUAL_TRUECOLOR);
	
	if (fbi->fb.var.bits_per_pixel == 16)
		fb_dealloc_cmap(&fbi->fb.cmap);
	else
		fb_alloc_cmap(&fbi->fb.cmap, 1<<fbi->fb.var.bits_per_pixel, 0);

	pxafb_activate_var(var, fbi);

	return 0;
}

/*
 * Formal definition of the VESA spec:
 *  On
 *  	This refers to the state of the display when it is in full operation
 *  Stand-By
 *  	This defines an optional operating state of minimal power reduction with
 *  	the shortest recovery time
 *  Suspend
 *  	This refers to a level of power management in which substantial power
 *  	reduction is achieved by the display.  The display can have a longer
 *  	recovery time from this state than from the Stand-by state
 *  Off
 *  	This indicates that the display is consuming the lowest level of power
 *  	and is non-operational. Recovery from this state may optionally require
 *  	the user to manually power on the monitor
 *
 *  Now, the fbdev driver adds an additional state, (blank), where they
 *  turn off the video (maybe by colormap tricks), but don't mess with the
 *  video itself: think of it semantically between on and Stand-By.
 *
 *  So here's what we should do in our fbdev blank routine:
 *
 *  	VESA_NO_BLANKING (mode 0)	Video on,  front/back light on
 *  	VESA_VSYNC_SUSPEND (mode 1)  	Video on,  front/back light off
 *  	VESA_HSYNC_SUSPEND (mode 2)  	Video on,  front/back light off
 *  	VESA_POWERDOWN (mode 3)		Video off, front/back light off
 *
 *  This will match the matrox implementation.
 */

/*
 * pxafb_blank():
 *	Blank the display by setting all palette values to zero.  Note, the
 * 	12 and 16 bpp modes don't really use the palette, so this will not
 *      blank the display in all modes.
 */
static int pxafb_blank(int blank, struct fb_info *info)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	int i;

	DPRINTK("pxafb_blank: blank=%d\n", blank);

	switch (blank) {
	case VESA_POWERDOWN:
	case VESA_VSYNC_SUSPEND:
	case VESA_HSYNC_SUSPEND:
		if (fbi->fb.fix.visual == FB_VISUAL_PSEUDOCOLOR ||
		    fbi->fb.fix.visual == FB_VISUAL_STATIC_PSEUDOCOLOR)
			for (i = 0; i < fbi->palette_size; i++)
				;//pxafb_setpalettereg(i, 0, 0, 0, 0, info);

		pxafb_schedule_work(fbi, C_DISABLE);
		//TODO if (pxafb_blank_helper) pxafb_blank_helper(blank);
		break;

	case VESA_NO_BLANKING:
		//TODO if (pxafb_blank_helper) pxafb_blank_helper(blank);
		if (fbi->fb.fix.visual == FB_VISUAL_PSEUDOCOLOR ||
		    fbi->fb.fix.visual == FB_VISUAL_STATIC_PSEUDOCOLOR)
			fb_set_cmap(&fbi->fb.cmap, info);
		pxafb_schedule_work(fbi, C_ENABLE);
	}
	return 0;
}

static int soft_cursor_dummy(struct fb_info *info, struct fb_cursor *cursor)
{
	return 0;
	}

static struct fb_ops pxafb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= pxafb_check_var,
	.fb_set_par	= pxafb_set_par,
	.fb_setcolreg	= pxafb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_blank	= pxafb_blank,
	.fb_cursor	= soft_cursor_dummy,
};

/*
 * Calculate the PCD value from the clock rate (in picoseconds).
 * We take account of the PPCR clock setting.
 * From PXA Developer's Manual:
 *
 *   PixelClock =      LCLK
 *                -------------
 *                2 ( PCD + 1 )
 *
 *   PCD =      LCLK
 *         ------------- - 1
 *         2(PixelClock)
 *
 * Where:
 *   LCLK = LCD/Memory Clock
 *   PCD = LCCR3[7:0]
 *
 * PixelClock here is in Hz while the pixclock argument given is the
 * period in picoseconds. Hence PixelClock = 1 / ( pixclock * 10^-12 )
 *
 * The function get_lclk_frequency_10khz returns LCLK in units of
 * 10khz. Calling the result of this function lclk gives us the
 * following
 *
 *    PCD = (lclk * 10^4 ) * ( pixclock * 10^-12 )
 *          -------------------------------------- - 1
 *                          2
 *
 * Factoring the 10^4 and 10^-12 out gives 10^-8 == 1 / 100000000 as used below.
 */
static inline unsigned int get_pcd(unsigned int pixclock)
{
	unsigned long long pcd;

	pcd = s3c2410_hclk/(((__raw_readl(S3C2410_LCDCON1)>>8)&0x3ff)*2+1);
	return (unsigned int)pcd;
}

/*
 * pxafb_activate_var():
 *	Configures LCD Controller based on entries in var parameter.  Settings are
 *	only written to the controller if changes were made.
 */
static int pxafb_activate_var(struct fb_var_screeninfo *var, struct pxafb_info *fbi)
{
	
	struct pxafb_lcd_reg new_regs;
	u_long flags;
	u_int /*lvines_per_panel, */ half_screen_size, yres/*, pcd = get_pcd(var->pixclock)*/;
	unsigned long VideoPhysicalTemp = fbi->screen_dma;

	DPRINTK("Configuring PXA LCD\n");

	DPRINTK("var: xres=%d hslen=%d lm=%d rm=%d\n",
		var->xres, var->hsync_len,
		var->left_margin, var->right_margin);
	DPRINTK("var: yres=%d vslen=%d um=%d bm=%d\n",
		var->yres, var->vsync_len,
		var->upper_margin, var->lower_margin);
	DPRINTK("var: pixclock=%d pcd=%d\n", var->pixclock, pcd);

#if DEBUG_VAR
	if (var->xres < 16        || var->xres > 1024)
		printk(KERN_ERR "%s: invalid xres %d\n",
			fbi->fb.fix.id, var->xres);
	switch(var->bits_per_pixel) {
	case 1:
	case 2:
	case 4:
	case 8:
	case 16:
		break;
	default:
		printk(KERN_ERR "%s: invalid bit depth %d\n",
		       fbi->fb.fix.id, var->bits_per_pixel);
		break;
	}
	if (var->hsync_len < 1    || var->hsync_len > 64)
		printk(KERN_ERR "%s: invalid hsync_len %d\n",
			fbi->fb.fix.id, var->hsync_len);
	if (var->left_margin < 1  || var->left_margin > 255)
		printk(KERN_ERR "%s: invalid left_margin %d\n",
			fbi->fb.fix.id, var->left_margin);
	if (var->right_margin < 1 || var->right_margin > 255)
		printk(KERN_ERR "%s: invalid right_margin %d\n",
			fbi->fb.fix.id, var->right_margin);
	if (var->yres < 1         || var->yres > 1024)
		printk(KERN_ERR "%s: invalid yres %d\n",
			fbi->fb.fix.id, var->yres);
	if (var->vsync_len < 1    || var->vsync_len > 64)
		printk(KERN_ERR "%s: invalid vsync_len %d\n",
			fbi->fb.fix.id, var->vsync_len);
	if (var->upper_margin < 0 || var->upper_margin > 255)
		printk(KERN_ERR "%s: invalid upper_margin %d\n",
			fbi->fb.fix.id, var->upper_margin);
	if (var->lower_margin < 0 || var->lower_margin > 255)
		printk(KERN_ERR "%s: invalid lower_margin %d\n",
			fbi->fb.fix.id, var->lower_margin);
#endif

	/* Update shadow copy atomically */
	local_irq_save(flags);

	new_regs.lcdcon1 = fbi->reg.lcdcon1 & ~S3C2410_LCDCON1_ENVID;

	new_regs.lcdcon2 = (fbi->reg.lcdcon2 & ~LCD2_LINEVAL_MSK) 
						| LCD2_LINEVAL(var->yres - 1);

	/* TFT LCD only ! */
	new_regs.lcdcon3 = (fbi->reg.lcdcon3 & ~LCD3_HOZVAL_MSK)
						| LCD3_HOZVAL(var->xres - 1);

	new_regs.lcdcon4 = fbi->reg.lcdcon4;
	new_regs.lcdcon5 = fbi->reg.lcdcon5;

	new_regs.lcdsaddr1 = 
		LCDADDR_BANK(((unsigned long)VideoPhysicalTemp >> 22))
		| LCDADDR_BASEU(((unsigned long)VideoPhysicalTemp >> 1));

	/* 16bpp */
	new_regs.lcdsaddr2 = LCDADDR_BASEL( 
		((unsigned long)VideoPhysicalTemp + (var->xres * 2 * (var->yres/*-1*/)))
		>> 1);

	new_regs.lcdsaddr3 = LCDADDR_OFFSET(0) | (LCDADDR_PAGE(var->xres) /*>> 1*/);

	yres = var->yres;
	/* if ( dual 锟斤拷 */
	//yres /= 2;

	half_screen_size = var->bits_per_pixel;
	half_screen_size = half_screen_size * var->xres * var->yres / 16;

	fbi->reg.lcdcon1 = new_regs.lcdcon1;
	fbi->reg.lcdcon2 = new_regs.lcdcon2;
	fbi->reg.lcdcon3 = new_regs.lcdcon3;
	fbi->reg.lcdcon4 = new_regs.lcdcon4;
	fbi->reg.lcdcon5 = new_regs.lcdcon5;
	fbi->reg.lcdsaddr1 = new_regs.lcdsaddr1;
	fbi->reg.lcdsaddr2 = new_regs.lcdsaddr2;
	fbi->reg.lcdsaddr3 = new_regs.lcdsaddr3;

	__raw_writel(fbi->reg.lcdcon1&~S3C2410_LCDCON1_ENVID, S3C2410_LCDCON1);
	__raw_writel(fbi->reg.lcdcon2, S3C2410_LCDCON2);
	__raw_writel(fbi->reg.lcdcon3, S3C2410_LCDCON3);
	__raw_writel(fbi->reg.lcdcon4, S3C2410_LCDCON4);
	__raw_writel(fbi->reg.lcdcon5, S3C2410_LCDCON5);
	__raw_writel(fbi->reg.lcdsaddr1, S3C2410_LCDSADDR1);
	__raw_writel(fbi->reg.lcdsaddr2, S3C2410_LCDSADDR2);
	__raw_writel(fbi->reg.lcdsaddr3, S3C2410_LCDSADDR3);

	//next code should not be used in TX06D18 LCD
	#if !defined (TX06D18_TFT_LCD )		//change by gongjun
		#if defined(CONFIG_S3C2410_SMDK) && !defined(CONFIG_SMDK_AIJI)
		    LCDLPCSEL = 0x2;
		#elif defined(CONFIG_S3C2410_SMDK) && defined(CONFIG_SMDK_AIJI) 
		    LCDLPCSEL = 0x7;
		#endif
	#endif
	
	__raw_writel(0, S3C2410_TPAL);
	__raw_writel(fbi->reg.lcdcon1|S3C2410_LCDCON1_ENVID, S3C2410_LCDCON1);

#if	1
	{
		DPRINTK("LCDCON1 0x%08x\n", __raw_readl(S3C2410_LCDCON1));
		DPRINTK("LCDCON2 0x%08x\n", __raw_readl(S3C2410_LCDCON2));
		DPRINTK("LCDCON3 0x%08x\n", __raw_readl(S3C2410_LCDCON3));
		DPRINTK("LCDCON4 0x%08x\n", __raw_readl(S3C2410_LCDCON4));
		DPRINTK("LCDCON5 0x%08x\n", __raw_readl(S3C2410_LCDCON5));
		DPRINTK("LCDSADDR1 0x%08x\n", __raw_readl(S3C2410_LCDSADDR1));
		DPRINTK("LCDSADDR2 0x%08x\n", __raw_readl(S3C2410_LCDSADDR2));
		DPRINTK("LCDSADDR3 0x%08x\n", __raw_readl(S3C2410_LCDSADDR3));
	}
#endif
	local_irq_restore(flags);

	return 0;
}

/*
 * NOTE!  The following functions are purely helpers for set_ctrlr_state.
 * Do not call them directly; set_ctrlr_state does the correct serialisation
 * to ensure that things happen in the right way 100% of time time.
 *	-- rmk
 */
static inline void __pxafb_backlight_power(struct pxafb_info *fbi, int on)
{
	DPRINTK("backlight o%s\n", on ? "n" : "ff");

 	if (pxafb_backlight_power)
 		pxafb_backlight_power(on);
}

static inline void __pxafb_lcd_power(struct pxafb_info *fbi, int on)
{
	DPRINTK("LCD power o%s\n", on ? "n" : "ff");

	if (pxafb_lcd_power)
		pxafb_lcd_power(on);
}

static void pxafb_setup_gpio(struct pxafb_info *fbi)
{
	DPRINTK("setup gpio\n");
	__raw_writel(0xaaaaaaaa, S3C2410_GPDCON);
	__raw_writel(7, S3C2410_LCDINTMSK);			// 3 by gjl MASK LCD Sub Interrupt
	__raw_writel(0, S3C2410_TPAL);				// Disable Temp Palette
	__raw_writel(0, S3C2410_LPCSEL);			// 0 by gjl Disable LPC3600
	__raw_writel(0, S3C2410_PRIORITY);			//0x7f add by gjl
}

static void pxafb_enable_controller(struct pxafb_info *fbi)
{/*
	DPRINTK("Enabling LCD controller\n");	
	DPRINTK("LCDCON1 0x%08x\n", __raw_readl(S3C2410_LCDCON1));
	DPRINTK("LCDCON2 0x%08x\n", __raw_readl(S3C2410_LCDCON2));
	DPRINTK("LCDCON3 0x%08x\n", __raw_readl(S3C2410_LCDCON3));
	DPRINTK("LCDCON4 0x%08x\n", __raw_readl(S3C2410_LCDCON4));
	DPRINTK("LCDCON5 0x%08x\n", __raw_readl(S3C2410_LCDCON5));
	DPRINTK("LCDSADDR1 0x%08x\n", __raw_readl(S3C2410_LCDSADDR1));
	DPRINTK("LCDSADDR2 0x%08x\n", __raw_readl(S3C2410_LCDSADDR2));
	DPRINTK("LCDSADDR3 0x%08x\n", __raw_readl(S3C2410_LCDSADDR3));*/

	__raw_writel(fbi->reg.lcdcon1&~S3C2410_LCDCON1_ENVID, S3C2410_LCDCON1);
	__raw_writel(fbi->reg.lcdcon2, S3C2410_LCDCON2);
	__raw_writel(fbi->reg.lcdcon3, S3C2410_LCDCON3);
	__raw_writel(fbi->reg.lcdcon4, S3C2410_LCDCON4);
	__raw_writel(fbi->reg.lcdcon5, S3C2410_LCDCON5);
	__raw_writel(fbi->reg.lcdsaddr1, S3C2410_LCDSADDR1);
	__raw_writel(fbi->reg.lcdsaddr2, S3C2410_LCDSADDR2);
	__raw_writel(fbi->reg.lcdsaddr3, S3C2410_LCDSADDR3);
	__raw_writel(fbi->reg.lcdcon1|S3C2410_LCDCON1_ENVID, S3C2410_LCDCON1);	
	
	DPRINTK("LCDCON1 0x%08x\n", __raw_readl(S3C2410_LCDCON1));
	DPRINTK("LCDCON2 0x%08x\n", __raw_readl(S3C2410_LCDCON2));
	DPRINTK("LCDCON3 0x%08x\n", __raw_readl(S3C2410_LCDCON3));
	DPRINTK("LCDCON4 0x%08x\n", __raw_readl(S3C2410_LCDCON4));
	DPRINTK("LCDCON5 0x%08x\n", __raw_readl(S3C2410_LCDCON5));
	DPRINTK("LCDSADDR1 0x%08x\n", __raw_readl(S3C2410_LCDSADDR1));
	DPRINTK("LCDSADDR2 0x%08x\n", __raw_readl(S3C2410_LCDSADDR2));
	DPRINTK("LCDSADDR3 0x%08x\n", __raw_readl(S3C2410_LCDSADDR3));
}

static void pxafb_disable_controller(struct pxafb_info *fbi)
{
/*	DECLARE_WAITQUEUE(wait, current);

	DPRINTK("Disabling LCD controller\n");

	add_wait_queue(&fbi->ctrlr_wait, &wait);
	set_current_state(TASK_UNINTERRUPTIBLE);

	LCSR = 0xffffffff;		// Clear LCD Status Register
	LCCR0 &= ~LCCR0_LDM;	// Enable LCD Disable Done Interrupt
	LCCR0 |= LCCR0_DIS;		// Disable LCD Controller

	schedule_timeout(20 * HZ / 1000);
	remove_wait_queue(&fbi->ctrlr_wait, &wait);*/
	__raw_writel(fbi->reg.lcdcon1&~S3C2410_LCDCON1_ENVID, S3C2410_LCDCON1);
}

/*
 * This function must be called from task context only, since it will
 * sleep when disabling the LCD controller, or if we get two contending
 * processes trying to alter state.
 */
static void set_ctrlr_state(struct pxafb_info *fbi, u_int state)
{
	u_int old_state;

	down(&fbi->ctrlr_sem);

	old_state = fbi->state;

	/*
	 * Hack around fbcon initialisation.
	 */
	if (old_state == C_STARTUP && state == C_REENABLE)
		state = C_ENABLE;

	switch (state) {
	case C_DISABLE_CLKCHANGE:
		/*
		 * Disable controller for clock change.  If the
		 * controller is already disabled, then do nothing.
		 */
		if (old_state != C_DISABLE && old_state != C_DISABLE_PM) {
			fbi->state = state;
			//TODO __pxafb_lcd_power(fbi, 0);
			pxafb_disable_controller(fbi);
		}
		break;

	case C_DISABLE_PM:
	case C_DISABLE:
		/*
		 * Disable controller
		 */
		if (old_state != C_DISABLE) {
			fbi->state = state;
			__pxafb_backlight_power(fbi, 0);
			__pxafb_lcd_power(fbi, 0);
			if (old_state != C_DISABLE_CLKCHANGE)
				pxafb_disable_controller(fbi);
		}
		break;

	case C_ENABLE_CLKCHANGE:
		/*
		 * Enable the controller after clock change.  Only
		 * do this if we were disabled for the clock change.
		 */
		if (old_state == C_DISABLE_CLKCHANGE) {
			fbi->state = C_ENABLE;
			pxafb_enable_controller(fbi);
			//TODO __pxafb_lcd_power(fbi, 1);
		}
		break;

	case C_REENABLE:
		/*
		 * Re-enable the controller only if it was already
		 * enabled.  This is so we reprogram the control
		 * registers.
		 */
		if (old_state == C_ENABLE) {
			pxafb_disable_controller(fbi);
			pxafb_setup_gpio(fbi);
			pxafb_enable_controller(fbi);
		}
		break;

	case C_ENABLE_PM:
		/*
		 * Re-enable the controller after PM.  This is not
		 * perfect - think about the case where we were doing
		 * a clock change, and we suspended half-way through.
		 */
		if (old_state != C_DISABLE_PM)
			break;
		/* fall through */

	case C_ENABLE:
		/*
		 * Power up the LCD screen, enable controller, and
		 * turn on the backlight.
		 */
		if (old_state != C_ENABLE) {
			fbi->state = C_ENABLE;
			pxafb_setup_gpio(fbi);
			pxafb_enable_controller(fbi);
			__pxafb_lcd_power(fbi, 1);
			__pxafb_backlight_power(fbi, 1);
		}
		break;
	}
	up(&fbi->ctrlr_sem);
}

/*
 * Our LCD controller task (which is called when we blank or unblank)
 * via keventd.
 */
static void pxafb_task(void *dummy)
{
	struct pxafb_info *fbi = dummy;
	u_int state = xchg(&fbi->task_state, -1);

	set_ctrlr_state(fbi, state);
}

#ifdef CONFIG_CPU_FREQ
/*
 * CPU clock speed change handler.  We need to adjust the LCD timing
 * parameters when the CPU clock is adjusted by the power management
 * subsystem.
 *
 * TODO: Determine why f->new != 10*get_lclk_frequency_10khz()
 */
static int
pxafb_freq_transition(struct notifier_block *nb, unsigned long val, void *data)
{
	struct pxafb_info *fbi = TO_INF(nb, freq_transition);
	//TODO struct cpufreq_freqs *f = data;
	u_int pcd;

	switch (val) {
	case CPUFREQ_PRECHANGE:
		set_ctrlr_state(fbi, C_DISABLE_CLKCHANGE);
		break;

	case CPUFREQ_POSTCHANGE:
		pcd = get_pcd(fbi->fb.var.pixclock);
		fbi->reg_lccr3 = (fbi->reg_lccr3 & ~0xff) | LCCR3_PixClkDiv(pcd);
		set_ctrlr_state(fbi, C_ENABLE_CLKCHANGE);
		break;
	}
	return 0;
}

static int
pxafb_freq_policy(struct notifier_block *nb, unsigned long val, void *data)
{
	struct pxafb_info *fbi = TO_INF(nb, freq_policy);
	struct fb_var_screeninfo *var = &fbi->fb.var;
	struct cpufreq_policy *policy = data;

	switch (val) {
	case CPUFREQ_ADJUST:
	case CPUFREQ_INCOMPATIBLE:
		printk(KERN_DEBUG "min dma period: %d ps, "
			"new clock %d kHz\n", pxafb_display_dma_period(var),
			policy->max);
		// TODO: fill in min/max values
		break;
#if 0
	case CPUFREQ_NOTIFY:
		printk(KERN_ERR "%s: got CPUFREQ_NOTIFY\n", __FUNCTION__);
		do {} while(0);
		/* todo: panic if min/max values aren't fulfilled
		 * [can't really happen unless there's a bug in the
		 * CPU policy verification process *
		 */
		break;
#endif
	}
	return 0;
}
#endif

#ifdef CONFIG_PM
/*
 * Power management hooks.  Note that we won't be called from IRQ context,
 * unlike the blank functions above, so we may sleep.
 */
static int pxafb_suspend(struct device *dev, u32 state, u32 level)
{
	struct pxafb_info *fbi = dev_get_drvdata(dev);

	if (level == SUSPEND_DISABLE || level == SUSPEND_POWER_DOWN)
		set_ctrlr_state(fbi, C_DISABLE_PM);
	return 0;
}

static int pxafb_resume(struct device *dev, u32 level)
{
	struct pxafb_info *fbi = dev_get_drvdata(dev);

	if (level == RESUME_ENABLE)
		set_ctrlr_state(fbi, C_ENABLE_PM);
	return 0;
}
#else
#define pxafb_suspend	NULL
#define pxafb_resume	NULL
#endif



void *consistent_alloc(int gfp, size_t size, dma_addr_t *dma_handle)
{
	struct page *page, *end, *free;
	unsigned long order;
	void *ret, *virt;

	if (in_interrupt())
		BUG();

	size = PAGE_ALIGN(size);
	order = get_order(size);

	page = alloc_pages(gfp, order);
	if (!page)
		goto no_page;

	/*
	 * We could do with a page_to_phys and page_to_bus here.
	 */
	virt = page_address(page);
	*dma_handle = virt_to_bus(virt);
	ret = __ioremap(virt_to_phys(virt), size, 0,0);
	if (!ret)
		goto no_remap;

#if 0 /* ioremap_does_flush_cache_all */
	/*
	 * we need to ensure that there are no cachelines in use, or
	 * worse dirty in this area.  Really, we don't need to do
	 * this since __ioremap does a flush_cache_all() anyway. --rmk
	 */
	invalidate_dcache_range(virt, virt + size);
#endif

	/*
	 * free wasted pages.  We skip the first page since we know
	 * that it will have count = 1 and won't require freeing.
	 * We also mark the pages in use as reserved so that
	 * remap_page_range works.
	 */
	page = virt_to_page(virt);
	free = page + (size >> PAGE_SHIFT);
	end  = page + (1 << order);

	for (; page < end; page++) {
		set_page_count(page, 1);
		if (page >= free)
			__free_page(page);
		else
			SetPageReserved(page);
	}
	return ret;

no_remap:
	__free_pages(page, order);
no_page:
	return NULL;
}
/*
 * pxafb_map_video_memory():
 *      Allocates the DRAM memory for the frame buffer.  This buffer is
 *	remapped into a non-cached, non-buffered, memory region to
 *      allow palette and pixel writes to occur without flushing the
 *      cache.  Once this area is remapped, all virtual memory
 *      access to the video memory should occur at the new region.
 */
static int __init pxafb_map_video_memory(struct pxafb_info *fbi)
{
	u_long palette_mem_size;

	/*
	 * We reserve one page for the palette, plus the size
	 * of the framebuffer.
	 */
	fbi->map_size = PAGE_ALIGN(fbi->fb.fix.smem_len + PAGE_SIZE);
	//changed by gjl  dma_alloc_writecombine   dma_alloc_coherent
	fbi->map_cpu = consistent_alloc(GFP_KERNEL, fbi->map_size,
	    			    &fbi->map_dma);  //fbi->dev, fbi->map_size,&fbi->map_dma, GFP_KERNEL);

	if (fbi->map_cpu) {
	printk("VA=0x%p, PA=0x%08x, size=0x%08x\n", fbi->map_cpu, fbi->map_dma, fbi->map_size);
		/* prevent initial garbage on screen */
		memset(fbi->map_cpu, 0, fbi->map_size);
		fbi->fb.screen_base = fbi->map_cpu + PAGE_SIZE;
		fbi->screen_dma = fbi->map_dma + PAGE_SIZE;
		/*
		 * FIXME: this is actually the wrong thing to place in
		 * smem_start.  But fbdev suffers from the problem that
		 * it needs an API which doesn't exist (in this case,
		 * dma_writecombine_mmap)
		 */
		fbi->fb.fix.smem_start = fbi->screen_dma;

		fbi->palette_size = fbi->fb.var.bits_per_pixel == 8 ? 256 : 16;

		palette_mem_size = fbi->palette_size * sizeof(u16);
		DPRINTK("palette_mem_size = 0x%08lx\n", (u_long) palette_mem_size);

		fbi->palette_cpu = (u16 *)(fbi->map_cpu + PAGE_SIZE - palette_mem_size);
		fbi->palette_dma = fbi->map_dma + PAGE_SIZE - palette_mem_size;
	}

	return fbi->map_cpu ? 0 : -ENOMEM;
}

static struct pxafb_info * __init pxafb_init_fbinfo(struct device *dev)
{
	struct pxafb_info *fbi;
	void *addr;
	struct pxafb_mach_info *inf = &fs2410_info;//dev->platform_data;

	/* Alloc the pxafb_info and pseudo_palette in one step */
	fbi = kmalloc(sizeof(struct pxafb_info) + sizeof(u32) * 16, GFP_KERNEL);
	if (!fbi)
		return NULL;

	memset(fbi, 0, sizeof(struct pxafb_info));
	fbi->dev = dev;

	strcpy(fbi->fb.fix.id, PXA_NAME);

	fbi->fb.fix.type	= FB_TYPE_PACKED_PIXELS;
	fbi->fb.fix.type_aux	= 0;
	fbi->fb.fix.xpanstep	= 0;
	fbi->fb.fix.ypanstep	= 0;
	fbi->fb.fix.ywrapstep	= 0;
	fbi->fb.fix.accel	= FB_ACCEL_NONE;

	fbi->fb.var.nonstd	= 0;
	fbi->fb.var.activate	= FB_ACTIVATE_NOW;
	fbi->fb.var.height	= -1;
	fbi->fb.var.width	= -1;
	fbi->fb.var.accel_flags	= 0;
	fbi->fb.var.vmode	= FB_VMODE_NONINTERLACED;

	fbi->fb.fbops		= &pxafb_ops;
	fbi->fb.flags		= FBINFO_FLAG_DEFAULT;
	fbi->fb.node		= -1;
	fbi->fb.currcon		= -1;

	addr = fbi;
	addr = addr + sizeof(struct pxafb_info);
	fbi->fb.pseudo_palette	= addr;

	fbi->max_xres			= inf->xres;
	fbi->fb.var.xres		= inf->xres;
	fbi->fb.var.xres_virtual	= inf->xres;
	fbi->max_yres			= inf->yres;
	fbi->fb.var.yres		= inf->yres;
	fbi->fb.var.yres_virtual	= inf->yres;
	fbi->max_bpp			= inf->bpp;
	fbi->fb.var.bits_per_pixel	= inf->bpp;
	fbi->fb.var.pixclock		= inf->pixclock;
	fbi->fb.var.hsync_len		= inf->hsync_len;
	fbi->fb.var.left_margin		= inf->left_margin;
	fbi->fb.var.right_margin	= inf->right_margin;
	fbi->fb.var.vsync_len		= inf->vsync_len;
	fbi->fb.var.upper_margin	= inf->upper_margin;
	fbi->fb.var.lower_margin	= inf->lower_margin;
	fbi->fb.var.sync		= inf->sync;
	fbi->fb.var.grayscale		= inf->cmap_greyscale;
	fbi->cmap_inverse		= inf->cmap_inverse;
	fbi->cmap_static		= inf->cmap_static;
	fbi->reg.lcdcon1		= inf->reg.lcdcon1;
	fbi->reg.lcdcon2		= inf->reg.lcdcon2;
	fbi->reg.lcdcon3		= inf->reg.lcdcon3;
	fbi->reg.lcdcon4		= inf->reg.lcdcon4;
	fbi->reg.lcdcon5		= inf->reg.lcdcon5;
	fbi->state			= C_STARTUP;
	fbi->task_state			= (u_char)-1;
	fbi->fb.fix.smem_len		= fbi->max_xres * fbi->max_yres *
					  fbi->max_bpp / 8;

	init_waitqueue_head(&fbi->ctrlr_wait);
	INIT_WORK(&fbi->task, pxafb_task, fbi);
	init_MUTEX(&fbi->ctrlr_sem);

	return fbi;
}

#ifdef CONFIG_FB_PXA_PARAMETERS
static int __init pxafb_parse_options(struct device *dev, char *options)
{
	struct pxafb_mach_info *inf = dev->platform_data;
	char *this_opt;

        if (!options || !*options)
                return 0;

	dev_dbg(dev, "options are \"%s\"\n", options ? options : "null");

	/* could be made table driven or similar?... */
        while ((this_opt = strsep(&options, ",")) != NULL) {
                if (!strncmp(this_opt, "mode:", 5)) {
			const char *name = this_opt+5;
			unsigned int namelen = strlen(name);
			int res_specified = 0, bpp_specified = 0;
			unsigned int xres = 0, yres = 0, bpp = 0;
			int yres_specified = 0;
			int i;
			for (i = namelen-1; i >= 0; i--) {
				switch (name[i]) {
				case '-':
					namelen = i;
					if (!bpp_specified && !yres_specified) {
						bpp = simple_strtoul(&name[i+1], NULL, 0);
						bpp_specified = 1;
					} else
						goto done;
					break;
				case 'x':
					if (!yres_specified) {
						yres = simple_strtoul(&name[i+1], NULL, 0);
						yres_specified = 1;
					} else
						goto done;
					break;
				case '0'...'9':
					break;
				default:
					goto done;
				}
			}
			if (i < 0 && yres_specified) {
				xres = simple_strtoul(name, NULL, 0);
				res_specified = 1;
			}
		done:
			if ( res_specified ) {
				dev_info(dev, "overriding resolution: %dx%x\n", xres, yres);
				inf->xres = xres; inf->yres = yres;
			}
			if ( bpp_specified )
				switch (bpp) {
				case 1:
				case 2:
				case 4:
				case 8:
				case 16:
					inf->bpp = bpp;
					dev_info(dev, "overriding bit depth: %d\n", bpp);
					break;
				default:
					dev_err(dev, "Depth %d is not valid\n", bpp);
				}
                } else if (!strncmp(this_opt, "pixclock:", 9)) {
                        inf->pixclock = simple_strtoul(this_opt+9, NULL, 0);
			dev_info(dev, "override pixclock: %u\n", inf->pixclock);
                } else if (!strncmp(this_opt, "left:", 5)) {
                        inf->left_margin = simple_strtoul(this_opt+5, NULL, 0);
			dev_info(dev, "override left: %u\n", inf->left_margin);
                } else if (!strncmp(this_opt, "right:", 6)) {
                        inf->right_margin = simple_strtoul(this_opt+6, NULL, 0);
			dev_info(dev, "override right: %u\n", inf->right_margin);
                } else if (!strncmp(this_opt, "upper:", 6)) {
                        inf->upper_margin = simple_strtoul(this_opt+6, NULL, 0);
			dev_info(dev, "override upper: %u\n", inf->upper_margin);
                } else if (!strncmp(this_opt, "lower:", 6)) {
                        inf->lower_margin = simple_strtoul(this_opt+6, NULL, 0);
			dev_info(dev, "override lower: %u\n", inf->lower_margin);
                } else if (!strncmp(this_opt, "hsynclen:", 9)) {
                        inf->hsync_len = simple_strtoul(this_opt+9, NULL, 0);
			dev_info(dev, "override hsynclen: %u\n", inf->hsync_len);
                } else if (!strncmp(this_opt, "vsynclen:", 9)) {
                        inf->vsync_len = simple_strtoul(this_opt+9, NULL, 0);
			dev_info(dev, "override vsynclen: %u\n", inf->vsync_len);
                } else if (!strncmp(this_opt, "hsync:", 6)) {
                        if ( simple_strtoul(this_opt+6, NULL, 0) == 0 ) {
				dev_info(dev, "override hsync: Active Low\n");
				inf->sync &= ~FB_SYNC_HOR_HIGH_ACT;
			} else {
				dev_info(dev, "override hsync: Active High\n");
				inf->sync |= FB_SYNC_HOR_HIGH_ACT;
			}
                } else if (!strncmp(this_opt, "vsync:", 6)) {
                        if ( simple_strtoul(this_opt+6, NULL, 0) == 0 ) {
				dev_info(dev, "override vsync: Active Low\n");
				inf->sync &= ~FB_SYNC_VERT_HIGH_ACT;
			} else {
				dev_info(dev, "override vsync: Active High\n");
				inf->sync |= FB_SYNC_VERT_HIGH_ACT;
			}
                } else if (!strncmp(this_opt, "dpc:", 4)) {
                        if ( simple_strtoul(this_opt+4, NULL, 0) == 0 ) {
				dev_info(dev, "override double pixel clock: false\n");
				inf->lccr3 &= ~LCCR3_DPC;
			} else {
				dev_info(dev, "override double pixel clock: true\n");
				inf->lccr3 |= LCCR3_DPC;
			}
                } else if (!strncmp(this_opt, "outputen:", 9)) {
                        if ( simple_strtoul(this_opt+9, NULL, 0) == 0 ) {
				dev_info(dev, "override output enable: active low\n");
				inf->lccr3 = ( inf->lccr3 & ~LCCR3_OEP ) | LCCR3_OutEnL;
			} else {
				dev_info(dev, "override output enable: active high\n");
				inf->lccr3 = ( inf->lccr3 & ~LCCR3_OEP ) | LCCR3_OutEnH;
			}
                } else if (!strncmp(this_opt, "pixclockpol:", 12)) {
                        if ( simple_strtoul(this_opt+12, NULL, 0) == 0 ) {
				dev_info(dev, "override pixel clock polarity: falling edge\n");
				inf->lccr3 = ( inf->lccr3 & ~LCCR3_PCP ) | LCCR3_PixFlEdg;
			} else {
				dev_info(dev, "override pixel clock polarity: rising edge\n");
				inf->lccr3 = ( inf->lccr3 & ~LCCR3_PCP ) | LCCR3_PixRsEdg;
			}
                } else if (!strncmp(this_opt, "color", 5)) {
			inf->lccr0 = (inf->lccr0 & ~LCCR0_CMS) | LCCR0_Color;
                } else if (!strncmp(this_opt, "mono", 4)) {
			inf->lccr0 = (inf->lccr0 & ~LCCR0_CMS) | LCCR0_Mono;
                } else if (!strncmp(this_opt, "active", 6)) {
			inf->lccr0 = (inf->lccr0 & ~LCCR0_PAS) | LCCR0_Act;
                } else if (!strncmp(this_opt, "passive", 7)) {
			inf->lccr0 = (inf->lccr0 & ~LCCR0_PAS) | LCCR0_Pas;
                } else if (!strncmp(this_opt, "single", 6)) {
			inf->lccr0 = (inf->lccr0 & ~LCCR0_SDS) | LCCR0_Sngl;
                } else if (!strncmp(this_opt, "dual", 4)) {
			inf->lccr0 = (inf->lccr0 & ~LCCR0_SDS) | LCCR0_Dual;
                } else if (!strncmp(this_opt, "4pix", 4)) {
			inf->lccr0 = (inf->lccr0 & ~LCCR0_DPD) | LCCR0_4PixMono;
                } else if (!strncmp(this_opt, "8pix", 4)) {
			inf->lccr0 = (inf->lccr0 & ~LCCR0_DPD) | LCCR0_8PixMono;
		} else {
			dev_err(dev, "unknown option: %s\n", this_opt);
			return -EINVAL;
		}
        }
        return 0;

}
#endif
//====add by pht.
static void s3c2410fb_irq_fifo(int irq, void *dev_id, struct pt_regs *regs)
{
	volatile register int a,b;
	local_irq_disable();
	
	__raw_writel(__raw_readl(S3C2410_LCDINTMSK)|=3, S3C2410_LCDINTMSK);
	__raw_writel(1, S3C2410_LCDSRCPND);
	__raw_writel(1, S3C2410_LCDINTPND);
	__raw_writel(__raw_readl(S3C2410_LCDINTMSK)&=(~(1)), S3C2410_LCDINTMSK);
	
	b=0;
	for(a=0;a<2000;a++)b++;
	
	//printk("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!%d\n",b);
	
	
	ClearPending(irq);
}
//====
int __init pxafb_probe(struct device *dev)
{
	struct pxafb_info *fbi;
	struct pxafb_mach_info *inf;
	unsigned long flags;
	int ret;

	//DPRINTK("pxafb_probe\n");
  	printk(KERN_ERR "pxafb_probe start!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	inf = &fs2410_info;//dev->platform_data;
	ret = -ENOMEM;
	fbi = NULL;
	if (!inf)
		goto failed;

#ifdef CONFIG_FB_PXA_PARAMETERS
	ret = pxafb_parse_options(dev, g_options);
	if ( ret < 0 )
		goto failed;
#endif

	dev_dbg(dev, "got a %dx%dx%d LCD\n",inf->xres, inf->yres, inf->bpp);
	if (inf->xres == 0 || inf->yres == 0 || inf->bpp == 0) {
		dev_err(dev, "Invalid resolution or bit depth\n");
		ret = -EINVAL;
		goto failed;
	}
	pxafb_backlight_power = inf->pxafb_backlight_power;
	pxafb_lcd_power = inf->pxafb_lcd_power;

	fbi = pxafb_init_fbinfo(dev);
	if (!fbi) {
		dev_err(dev, "Failed to initialize framebuffer device\n");
		ret = -ENOMEM; // only reason for pxafb_init_fbinfo to fail is kmalloc
		goto failed;
	}

	/* Initialize video memory */
	ret = pxafb_map_video_memory(fbi);
	if (ret) {
		dev_err(dev, "Failed to allocate video RAM: %d\n", ret);
		ret = -ENOMEM;
		goto failed;
	}
	/* enable LCD controller clock */
//	local_irq_save(flags);
//	CKEN |= CKEN16_LCD;
//	local_irq_restore(flags);

	/*
	 * This makes sure that our colour bitfield
	 * descriptors are correctly initialised.
	 */
	pxafb_check_var(&fbi->fb.var, &fbi->fb);
	pxafb_set_par(&fbi->fb);

	dev_set_drvdata(dev, fbi);

	ret = register_framebuffer(&fbi->fb);
	if (ret < 0) {
		dev_err(dev, "Failed to register framebuffer device: %d\n", ret);
		goto failed;
	}
        printk(KERN_ERR "success to register framebuffer device: %d!!!\n", ret);
	//====add by pht.        
	disable_irq(IRQ_LCD);
	ret = request_irq(IRQ_LCD, s3c2410fb_irq_fifo, SA_INTERRUPT,
		  "LCD", fbi);
	if (ret) {
		printk(KERN_ERR "s3c2440fb: request_irq failed: %d\n", ret);
		goto failed;
	}
	enable_irq(IRQ_LCD);
        
//====
#ifdef CONFIG_PM
	// TODO
#endif

#ifdef CONFIG_CPU_FREQ
	fbi->freq_transition.notifier_call = pxafb_freq_transition;
	fbi->freq_policy.notifier_call = pxafb_freq_policy;
	cpufreq_register_notifier(&fbi->freq_transition, CPUFREQ_TRANSITION_NOTIFIER);
	cpufreq_register_notifier(&fbi->freq_policy, CPUFREQ_POLICY_NOTIFIER);
#endif

	/*
	 * Ok, now enable the LCD controller
	 */
	set_ctrlr_state(fbi, C_ENABLE);
        printk(KERN_ERR "done probe!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	return 0;

failed:
	dev_set_drvdata(dev, NULL);
	if (fbi)
		kfree(fbi);
	return ret;
}

static struct device_driver pxafb_driver = {
	.name		= "s3c2410-lcd",
	.bus		= &platform_bus_type,
	.probe	= pxafb_probe,
#ifdef CONFIG_PM
	.suspend	= pxafb_suspend,
	.resume		= pxafb_resume,
#endif
};

int __devinit s3c2410fb_init(void)
{
	int ret;
 	printk(KERN_ERR "--- s3c2410fb init ---!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	//	DPRINTK("--- s3c2410fb init ---\n");
	ret = driver_register(&pxafb_driver);
	if(ret)
		printk("register device driver failed, return code is %d\n", ret);
		
	__raw_writel(__raw_readl(S3C2410_LCDINTMSK)|=3, S3C2410_LCDINTMSK);
	__raw_writel(1, S3C2410_LCDSRCPND);
	__raw_writel(1, S3C2410_LCDINTPND);
	__raw_writel(__raw_readl(S3C2410_LCDINTMSK)&=(~(1)), S3C2410_LCDINTMSK);
	
	
	return ret;
}

/*
#ifndef MODULE
int __devinit s3c2410fb_setup(char *options)
{
# ifdef CONFIG_FB_PXA_PARAMETERS
	strlcpy(g_options, options, sizeof(g_options));
# endif
	return 0;
}
#else
*/
module_init(s3c2410fb_init);
# ifdef CONFIG_FB_PXA_PARAMETERS
module_param_string(options, g_options, sizeof(g_options), 0);
MODULE_PARM_DESC(options, "LCD parameters (see Documentation/fb/pxafb.txt)");
# endif
//#endif

MODULE_DESCRIPTION("loadable framebuffer driver for PXA");
MODULE_LICENSE("GPL");
