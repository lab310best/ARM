/* linux/arch/arm/mach-s3c2410/devs.c
 *
 * Copyright (c) 2004 Simtec Electronics
 * Ben Dooks <ben@simtec.co.uk>
 *
 * Base S3C2410 platform device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Modifications:
 *     10-Mar-2005 LCVR Changed S3C2410_{VA,SZ} to S3C24XX_{VA,SZ}
 *     10-Feb-2005 BJD  Added camera from guillaume.gourat@nexvision.tv
 *     29-Aug-2004 BJD  Added timers 0 through 3
 *     29-Aug-2004 BJD  Changed index of devices we only have one of to -1
 *     21-Aug-2004 BJD  Added IRQ_TICK to RTC resources
 *     18-Aug-2004 BJD  Created initial version
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/device.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/arch/s3c2410_ts.h> //add by fla
#include <asm/arch/regs-serial.h>
//FLAAAAAAAAAAAAAA
#include <linux/mtd/partitions.h>
#include <asm/arch/nand.h>
#include <linux/mtd/nand.h>
//..............
#include "devs.h"

/* Serial port registrations */

struct platform_device *s3c24xx_uart_devs[3];

/* USB Host Controller */

static struct resource s3c_usb_resource[] = {
	[0] = {
		.start = S3C2410_PA_USBHOST,
		.end   = S3C2410_PA_USBHOST + S3C24XX_SZ_USBHOST,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_USBH,
		.end   = IRQ_USBH,
		.flags = IORESOURCE_IRQ,
	}
};

static u64 s3c_device_usb_dmamask = 0xffffffffUL;

struct platform_device s3c_device_usb = {
	.name		  = "s3c2410-ohci",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_usb_resource),
	.resource	  = s3c_usb_resource,
	.dev              = {
		.dma_mask = &s3c_device_usb_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};

EXPORT_SYMBOL(s3c_device_usb);

/* LCD Controller */

static struct resource s3c_lcd_resource[] = {
	[0] = {
		.start = S3C2410_PA_LCD,
		.end   = S3C2410_PA_LCD + S3C24XX_SZ_LCD,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_LCD,
		.end   = IRQ_LCD,
		.flags = IORESOURCE_IRQ,
	}

};

static u64 s3c_device_lcd_dmamask = 0xffffffffUL;

struct platform_device s3c_device_lcd = {
        .name		  = "s3c2410-lcd",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_lcd_resource),
	.resource	  = s3c_lcd_resource,
	.dev              = {
		.dma_mask		= &s3c_device_lcd_dmamask,
		.coherent_dma_mask	= 0xffffffffUL
	}
};

EXPORT_SYMBOL(s3c_device_lcd);

/* NAND Controller */

static struct resource s3c_nand_resource[] = {
	[0] = {
		.start = S3C2410_PA_NAND,
		.end   = S3C2410_PA_NAND + S3C24XX_SZ_NAND,
		.flags = IORESOURCE_MEM,
	}
};
/*
static struct mtd_partition partition_info[] ={
{
name: "loader",
size: 0x00020000,
offset: 0,
}, {
name: "param",
size: 0x00010000,
offset: 0x00020000,
}, {
name: "kernel",
size: 0x001c0000,
offset: 0x00030000,
}, {
name: "root",
size: 0x00200000,
offset: 0x00200000,
mask_flags: MTD_WRITEABLE,
}, {
name: "user",
size: 0x03af8000,
offset: 0x00400000,
}
};
*/
//64M
struct mtd_partition TE24xx_default_nand_part_a[] = {
	[0] = {
		.name	= "Boot",
		.size	= 0x00030000,
		.offset	= 0
	},
	[1] = {
		.name	= "MyApp",
		.size	= 0x00200000,
		.offset	= 0x00030000,
	},
	[2] = {
		.name	= "Kernel",
		.size	= 0x001d0000,
		.offset	= 0x00230000,
	},
	[3] = {
		.name	= "fs_cramfs",
		.size	= 0x01400000,    //30M
		.offset	= 0x00400000,
	},
	[4] = {
		.name	= "fs_yaffs",
		.size	= 0x00800000,    //30M
		.offset	= 0x01800000,
	},	
	[5] = {
		.name	= "WINCE",
		.size	= 0x02000000,
		.offset	= 0x02000000,
	}
};
//----
//128M
struct mtd_partition TE24xx_default_nand_part_b[]= {
	[0] = {
		.name	= "Boot",
		.size	= 0x00100000,
		.offset	= 0
	},
	[1] = {
		.name	= "MyApp",
		.size	= 0x003c0000,
		.offset	= 0x00140000,
	},
	[2] = {
		.name	= "Kernel",
		.size	= 0x00300000,
		.offset	= 0x00500000,
	},
	[3] = {
		.name	= "fs_yaffs",
		.size	= 0x07800000,    //30M
		.offset	= 0x00800000,
	},	
/*	[4] = {
		.name	= "WINCE",
		.size	= 0x03c00000,
		.offset	= 0x04400000,
	}*/
};
/*
struct mtd_partition TE24xx_default_nand_part_b[]= {
	[0] = {
		.name	= "Boot",
		.size	= 0x00020000,
		.offset	= 0
	},
	[1] = {
		.name	= "MyApp",
		.size	= 0x00480000,
		.offset	= 0x00060000,
	},
	[2] = {
		.name	= "Kernel",
		.size	= 0x00320000,
		.offset	= 0x004e0000,
	},
	[3] = {
		.name	= "fs_cramfs",
		.size	= 0x01400000,    //30M
		.offset	= 0x00800000,
	},
	[4] = {
		.name	= "fs_yaffs",
		.size	= 0x02800000,    //30M
		.offset	= 0x01c00000,
	},	
	[5] = {
		.name	= "WINCE",
		.size	= 0x03c00000,
		.offset	= 0x04400000,
	}
};
*/
//----
struct s3c2410_nand_set nandset ={
nr_partitions: 4,

partitions: TE24xx_default_nand_part_b,  //partition_info 64M

};

struct s3c2410_platform_nand superlpplatform={
tacls:0,   //0
twrph0:30,   //30
twrph1:0,    //0
sets: &nandset,
nr_sets: 1,
};


//flaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa




struct platform_device s3c_device_nand = {
	.name		  = "s3c2410-nand",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_nand_resource),
	.resource	  = s3c_nand_resource,
	.dev = {
.platform_data = &superlpplatform //***********add here*****
} 
};

EXPORT_SYMBOL(s3c_device_nand);

//static u64 uhc_dma_mask = ~(u64)0;   //fla
/* UDA1341 */

/*static struct resource s3c_1341_resource[] = {
	[0] = {
		.start = S3C2410_PA_DMA,
		.end   = S3C2410_PA_DMA + S3C2410_SZ_DMA,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_DMA2,
		.end   = IRQ_DMA2,
		.flags = IORESOURCE_IRQ,
	}

	};*/

//static u64 s3c_device_lcd_dmamask = 0xffffffffUL;
/*
struct platform_device s3c_device_1341 = {
	.name		  = "s3c2410-uda1341",
	.id		  = -1,
	.num_resources	  = 0,       //ARRAY_SIZE(s3c_1341_resource),
	.resource	  = NULL,     //s3c_1341_resource,
		.dev              = {
		  .dma_mask		= &uhc_dma_mask,//s3c_device_lcd_dmamask,
		.coherent_dma_mask	= 0xffffffffUL
		}
};

EXPORT_SYMBOL(s3c_device_1341);
*/

//touch section add by fla
/* Touchscreen */
static struct s3c2410_ts_mach_info s3c2410ts_info;

void __init set_s3c2410ts_info(struct s3c2410_ts_mach_info *hard_s3c2410ts_info)
{
	memcpy(&s3c2410ts_info,hard_s3c2410ts_info,sizeof(struct s3c2410_ts_mach_info));
}
EXPORT_SYMBOL(set_s3c2410ts_info);

struct platform_device s3c_device_ts = {
	.name		  = "s3c2410-ts",
	.id		  = -1,
	.dev              = {
 		.platform_data	= &s3c2410ts_info,
	}
};
EXPORT_SYMBOL(s3c_device_ts);

/* USB Device (Gadget)*/

static struct resource s3c_usbgadget_resource[] = {
	[0] = {
		.start = S3C2410_PA_USBDEV,
		.end   = S3C2410_PA_USBDEV + S3C24XX_SZ_USBDEV,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_USBD,
		.end   = IRQ_USBD,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_usbgadget = {
	.name		  = "s3c2410-usbgadget",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_usbgadget_resource),
	.resource	  = s3c_usbgadget_resource,
};

EXPORT_SYMBOL(s3c_device_usbgadget);

/* Watchdog */

static struct resource s3c_wdt_resource[] = {
	[0] = {
		.start = S3C2410_PA_WATCHDOG,
		.end   = S3C2410_PA_WATCHDOG + S3C24XX_SZ_WATCHDOG,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_WDT,
		.end   = IRQ_WDT,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_wdt = {
	.name		  = "s3c2410-wdt",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_wdt_resource),
	.resource	  = s3c_wdt_resource,
};

EXPORT_SYMBOL(s3c_device_wdt);

/* I2C */

static struct resource s3c_i2c_resource[] = {
	[0] = {
		.start = S3C2410_PA_IIC,
		.end   = S3C2410_PA_IIC + S3C24XX_SZ_IIC,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_IIC,
		.end   = IRQ_IIC,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_i2c = {
	.name		  = "s3c2410-i2c",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_i2c_resource),
	.resource	  = s3c_i2c_resource,
};

EXPORT_SYMBOL(s3c_device_i2c);

/* IIS */

static struct resource s3c_iis_resource[] = {
	[0] = {
		.start = S3C2410_PA_IIS,
		.end   = S3C2410_PA_IIS + S3C24XX_SZ_IIS,
		.flags = IORESOURCE_MEM,
	}
};

static u64 s3c_device_iis_dmamask = 0xffffffffUL;

struct platform_device s3c_device_iis = {
	.name		  = "s3c2410-iis",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_iis_resource),
	.resource	  = s3c_iis_resource,
	.dev              = {
		.dma_mask = &s3c_device_iis_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};

EXPORT_SYMBOL(s3c_device_iis);

/* RTC */

static struct resource s3c_rtc_resource[] = {
	[0] = {
		.start = S3C2410_PA_RTC,
		.end   = S3C2410_PA_RTC + 0xff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_RTC,
		.end   = IRQ_RTC,
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = IRQ_TICK,
		.end   = IRQ_TICK,
		.flags = IORESOURCE_IRQ
	}
};

struct platform_device s3c_device_rtc = {
	.name		  = "s3c2410-rtc",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_rtc_resource),
	.resource	  = s3c_rtc_resource,
};

EXPORT_SYMBOL(s3c_device_rtc);

/* ADC */

static struct resource s3c_adc_resource[] = {
	[0] = {
		.start = S3C2410_PA_ADC,
		.end   = S3C2410_PA_ADC + S3C24XX_SZ_ADC,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_TC,
		.end   = IRQ_ADC,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_adc = {
	.name		  = "s3c2410-adc",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_adc_resource),
	.resource	  = s3c_adc_resource,
};

/* SDI */
/*
static struct resource s3c_sdi_resource[] = {
	[0] = {
		.start = S3C2410_PA_SDI,
		.end   = S3C2410_PA_SDI + S3C24XX_SZ_SDI,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SDI,
		.end   = IRQ_SDI,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_sdi = {
	.name		  = "s3c2410-sdi",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_sdi_resource),
	.resource	  = s3c_sdi_resource,
};

EXPORT_SYMBOL(s3c_device_sdi);
*/
//-----------------------------fla
static struct resource s3c_sdi_resource[] = {
	[0] = {
		.start = S3C2410_PA_SDI,
		.end   = S3C2410_PA_SDI + S3C24XX_SZ_SDI - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SDI,
		.end   = IRQ_SDI,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_sdi = {
	.name		  = "s3c2440-sdi",                         //"s3c2410-sdi",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_sdi_resource),
	.resource	  = s3c_sdi_resource,
};

EXPORT_SYMBOL(s3c_device_sdi);

/* SPI (0) */

static struct resource s3c_spi0_resource[] = {
	[0] = {
		.start = S3C2410_PA_SPI,
		.end   = S3C2410_PA_SPI + 0x1f,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SPI0,
		.end   = IRQ_SPI0,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_spi0 = {
	.name		  = "s3c2410-spi",
	.id		  = 0,
	.num_resources	  = ARRAY_SIZE(s3c_spi0_resource),
	.resource	  = s3c_spi0_resource,
};

EXPORT_SYMBOL(s3c_device_spi0);

/* SPI (1) */

static struct resource s3c_spi1_resource[] = {
	[0] = {
		.start = S3C2410_PA_SPI + 0x20,
		.end   = S3C2410_PA_SPI + 0x20 + 0x1f,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SPI1,
		.end   = IRQ_SPI1,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_spi1 = {
	.name		  = "s3c2410-spi",
	.id		  = 1,
	.num_resources	  = ARRAY_SIZE(s3c_spi1_resource),
	.resource	  = s3c_spi1_resource,
};

EXPORT_SYMBOL(s3c_device_spi1);

/* pwm timer blocks */

static struct resource s3c_timer0_resource[] = {
	[0] = {
		.start = S3C2410_PA_TIMER + 0x0C,
		.end   = S3C2410_PA_TIMER + 0x0C + 0xB,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_TIMER0,
		.end   = IRQ_TIMER0,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_timer0 = {
	.name		  = "s3c2410-timer",
	.id		  = 0,
	.num_resources	  = ARRAY_SIZE(s3c_timer0_resource),
	.resource	  = s3c_timer0_resource,
};

EXPORT_SYMBOL(s3c_device_timer0);

/* timer 1 */

static struct resource s3c_timer1_resource[] = {
	[0] = {
		.start = S3C2410_PA_TIMER + 0x18,
		.end   = S3C2410_PA_TIMER + 0x23,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_TIMER1,
		.end   = IRQ_TIMER1,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_timer1 = {
	.name		  = "s3c2410-timer",
	.id		  = 1,
	.num_resources	  = ARRAY_SIZE(s3c_timer1_resource),
	.resource	  = s3c_timer1_resource,
};

EXPORT_SYMBOL(s3c_device_timer1);

/* timer 2 */

static struct resource s3c_timer2_resource[] = {
	[0] = {
		.start = S3C2410_PA_TIMER + 0x24,
		.end   = S3C2410_PA_TIMER + 0x2F,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_TIMER2,
		.end   = IRQ_TIMER2,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_timer2 = {
	.name		  = "s3c2410-timer",
	.id		  = 2,
	.num_resources	  = ARRAY_SIZE(s3c_timer2_resource),
	.resource	  = s3c_timer2_resource,
};

EXPORT_SYMBOL(s3c_device_timer2);

/* timer 3 */

static struct resource s3c_timer3_resource[] = {
	[0] = {
		.start = S3C2410_PA_TIMER + 0x30,
		.end   = S3C2410_PA_TIMER + 0x3B,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_TIMER3,
		.end   = IRQ_TIMER3,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_timer3 = {
	.name		  = "s3c2410-timer",
	.id		  = 3,
	.num_resources	  = ARRAY_SIZE(s3c_timer3_resource),
	.resource	  = s3c_timer3_resource,
};

EXPORT_SYMBOL(s3c_device_timer3);



#define    DM9000_BASE 0x20000300

//#define DM9000_IRQ IRQ_EINT7

static struct resource s3c_dm9000_resource[] = {

        [0] = { 

              .start = DM9000_BASE, 

      .end   = DM9000_BASE+ 0x3, 

      .flags = IORESOURCE_MEM 

       },

       [1]={

              .start = DM9000_BASE + 0x4,

              .end = DM9000_BASE + 0x4 + 0x7c,

              .flags = IORESOURCE_MEM

       },

        [2] = { 

      .start = IRQ_EINT7, 

      .end   = IRQ_EINT7, 

      .flags = IORESOURCE_IRQ 

       } 

}; 

 

static struct dm9000_plat_data s3c_device_dm9000_platdata = { 
   .flags=   DM9000_PLATF_16BITONLY
}; 

 

struct platform_device s3c_device_dm9000 = { 
   .name= "dm9000", 
   .id= 0, 
   .num_resources= ARRAY_SIZE(s3c_dm9000_resource), 
   .resource= s3c_dm9000_resource, 
   .dev= { 
   .platform_data = &s3c_device_dm9000_platdata, 
       } 

}; 

EXPORT_SYMBOL(s3c_device_dm9000);       

//end of DM9000

    


#ifdef CONFIG_CPU_S3C2440

/* Camif Controller */

static struct resource s3c_camif_resource[] = {
	[0] = {
		.start = S3C2440_PA_CAMIF,
		.end   = S3C2440_PA_CAMIF + S3C2440_SZ_CAMIF,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_CAM,
		.end   = IRQ_CAM,
		.flags = IORESOURCE_IRQ,
	}

};

static u64 s3c_device_camif_dmamask = 0xffffffffUL;

struct platform_device s3c_device_camif = {
	.name		  = "s3c2440-camif",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_camif_resource),
	.resource	  = s3c_camif_resource,
	.dev              = {
		.dma_mask = &s3c_device_camif_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};

EXPORT_SYMBOL(s3c_device_camif);

#endif // CONFIG_CPU_S32440
