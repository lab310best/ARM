#ifndef __PXAFB_H__
#define __PXAFB_H__

/*
 * linux/drivers/video/pxafb.h
 *    -- Intel PXA250/210 LCD Controller Frame Buffer Device
 *
 *  Copyright (C) 1999 Eric A. Thomas.
 *  Copyright (C) 2004 Jean-Frederic Clere.
 *  Copyright (C) 2004 Ian Campbell.
 *  Copyright (C) 2004 Jeff Lackey.
 *   Based on sa1100fb.c Copyright (C) 1999 Eric A. Thomas
 *  which in turn is
 *   Based on acornfb.c Copyright (C) Russell King.
 *
 *  2001-08-03: Cliff Brake <cbrake@acclent.com>
 *	 - ported SA1100 code to PXA
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

/* Shadows for LCD controller registers */
struct pxafb_lcd_reg {
	unsigned long lcdcon1;
    unsigned long lcdcon2;
    unsigned long lcdcon3;
    unsigned long lcdcon4;
    unsigned long lcdcon5;
    unsigned long lcdsaddr1;
    unsigned long lcdsaddr2;
    unsigned long lcdsaddr3;
};

struct pxafb_info {
	struct fb_info		fb;
	struct device		*dev;

	u_int			max_bpp;
	u_int			max_xres;
	u_int			max_yres;

	/*
	 * These are the addresses we mapped
	 * the framebuffer memory region to.
	 */
	/* raw memory addresses */
	dma_addr_t		map_dma;	/* physical */
	u_char *		map_cpu;	/* virtual */
	u_int			map_size;

	/* addresses of pieces placed in raw buffer */
	u_char *		screen_cpu;	/* virtual address of frame buffer */
	dma_addr_t		screen_dma;	/* physical address of frame buffer */
	u16 *			palette_cpu;	/* virtual address of palette memory */
	dma_addr_t		palette_dma;	/* physical address of palette memory */
	u_int			palette_size;

	/* DMA descriptors */	
	dma_addr_t		dmadesc_fblow_dma;
	dma_addr_t		dmadesc_fbhigh_dma;
	dma_addr_t		dmadesc_palette_dma;
	
	u_int			cmap_inverse:1,
				cmap_static:1,
				unused:30;
	
	volatile u_char		state;
	volatile u_char		task_state;
	struct semaphore	ctrlr_sem;
	wait_queue_head_t	ctrlr_wait;
	struct work_struct	task;
	
	struct pxafb_lcd_reg	reg;

#ifdef CONFIG_CPU_FREQ
	struct notifier_block	freq_transition;
	struct notifier_block	freq_policy;
#endif
};

struct pxafb_mach_info {
	u_long		pixclock;

	u_short		xres;
	u_short		yres;

	u_char		bpp;
	u_char		hsync_len;
	u_char		left_margin;
	u_char		right_margin;

	u_char		vsync_len;
	u_char		upper_margin;
	u_char		lower_margin;
	u_char		sync;

	u_int		cmap_greyscale:1,
			cmap_inverse:1,
			cmap_static:1,
			unused:29;

	struct pxafb_lcd_reg	reg;
	
	void (*pxafb_backlight_power)(int);
	void (*pxafb_lcd_power)(int);

};

#define TO_INF(ptr,member) container_of(ptr,struct pxafb_info,member)

/*
 * These are the actions for set_ctrlr_state
 */
#define C_DISABLE		(0)
#define C_ENABLE		(1)
#define C_DISABLE_CLKCHANGE	(2)
#define C_ENABLE_CLKCHANGE	(3)
#define C_REENABLE		(4)
#define C_DISABLE_PM		(5)
#define C_ENABLE_PM		(6)
#define C_STARTUP		(7)

#define PXA_NAME	"PXA"

/*
 *  Debug macros
 */
//#define	DEBUG	1
#if DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

/*
 * Minimum X and Y resolutions
 */
#define MIN_XRES	64
#define MIN_YRES	64

#ifndef __ASSEMBLY__
#define UData(Data)	((unsigned long) (Data))
#else
#define UData(Data)	(Data)
#endif

#define Fld(Size, Shft)	(((Size) << 16) + (Shft))
#define FSize(Field)	((Field) >> 16)
#define FShft(Field)	((Field) & 0x0000FFFF)
#define FMsk(Field)	(((UData (1) << FSize (Field)) - 1) << FShft (Field))
#define FInsrt(Value, Field) \
                	(UData (Value) << FShft (Field))

#define fLCD2_LINEVAL	Fld(10,14)	/* TFT/STN: vertical size of LCD */
#define LCD2_LINEVAL(x)	FInsrt((x), fLCD2_LINEVAL)
#define LCD2_LINEVAL_MSK	FMsk(fLCD2_LINEVAL)
#define fLCD3_HOZVAL	Fld(11,8)	/* horizontal size of LCD */
#define LCD3_HOZVAL(x)	FInsrt((x), fLCD3_HOZVAL)
#define LCD3_HOZVAL_MSK	FMsk(fLCD3_HOZVAL)

#define fLCDADDR_BANK	Fld(9,21)	/* bank location for video buffer */
#define LCDADDR_BANK(x)	FInsrt((x), fLCDADDR_BANK)

#define fLCDADDR_BASEU	Fld(21,0)	/* address of upper left corner */
#define LCDADDR_BASEU(x)	FInsrt((x), fLCDADDR_BASEU)

#define fLCDADDR_BASEL	Fld(21,0)	/* address of lower right corner */
#define LCDADDR_BASEL(x)	FInsrt((x), fLCDADDR_BASEL)

#define fLCDADDR_OFFSET	Fld(11,11)	/* Virtual screen offset size
					   (# of half words) */
#define LCDADDR_OFFSET(x)	FInsrt((x), fLCDADDR_OFFSET)

#define fLCDADDR_PAGE	Fld(11,0)	/* Virtual screen page width
					   (# of half words) */
#define LCDADDR_PAGE(x)	FInsrt((x), fLCDADDR_PAGE)

#endif /* __PXAFB_H__ */
