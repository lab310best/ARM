/***********************************************************************
 *
 * linux/arch/arm/mach-s3c2410/mach-smdk2410.c
 *
 * Copyright (C) 2004 by FS Forth-Systeme GmbH
 * All rights reserved.
 *
 * $Id: mach-smdk2410.c,v 1.1 2004/05/11 14:15:38 mpietrek Exp $
 * @Author: Jonas Dietsche
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * @History:
 * derived from linux/arch/arm/mach-s3c2410/mach-bast.c, written by
 * Ben Dooks <ben@simtec.co.uk>
 *
 * 10-Mar-2005 LCVR  Changed S3C2410_VA to S3C24XX_VA
 *
 ***********************************************************************/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <asm/arch/usb-control.h>   //fla

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/arch/s3c2410_ts.h> //fla
#include <asm/arch/regs-serial.h>
#include <asm/arch/regs-clock.h> //fla for usb
#include <linux/delay.h>         //fla
#include <linux/device.h>          //fla for platform_device in usb_yl_init()



#include "devs.h"
#include "cpu.h"
//extern struct platform_device  s3c_device_1341;
static struct map_desc smdk2410_iodesc[] __initdata = {
      {0xE0000000,0x19000000,SZ_1M,MT_DEVICE},//fla  cs8900
      //  {0xD0000000,0x08000000,0x00100000,DOMAIN_IO,0,1,0,0},//8250
      {S3C24XX_VA_IIS,S3C2410_PA_IIS,S3C24XX_SZ_IIS,MT_DEVICE},//1341
      {S3C24XX_VA_GPIO,S3C2410_PA_GPIO,S3C24XX_SZ_GPIO,MT_DEVICE},//1341
      {S3C24XX_VA_DMA,S3C2410_PA_DMA,S3C24XX_SZ_DMA,MT_DEVICE},//1341
      {S3C24XX_VA_IRQ,S3C2410_PA_IRQ,S3C24XX_SZ_IRQ,MT_DEVICE},//for use __raw_readl in ide.h 
      {S3C2410_VA_SDI,S3C2410_PA_SDI,S3C24XX_SZ_SDI,MT_DEVICE},
      { S3C2410_ADDR(0x02100300),0x20000300, SZ_1M,MT_DEVICE}      

};

#define UCON S3C2410_UCON_DEFAULT
#define ULCON S3C2410_LCON_CS8 | S3C2410_LCON_PNONE | S3C2410_LCON_STOPB
#define UFCON S3C2410_UFCON_RXTRIG8 | S3C2410_UFCON_FIFOMODE

static struct s3c2410_uartcfg smdk2410_uartcfgs[] = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	},
	[2] = {
		.hwport	     = 2,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	}
};

//ts fla
static struct s3c2410_ts_mach_info yl2410_ts_cfg __initdata = {

     .delay = 10000,

     .presc = 49,

     .oversampling_shift = 2,

};
/////

/////////////////usb  fla
static struct s3c2410_hcd_info usb_yl_info = {
.port[0]= {
.flags = S3C_HCDFLG_USED
},
//.port[1]= {
//.flags = S3C_HCDFLG_USED
//},
};

int usb_yl_init(void)
{
  // unsigned long upllvalue = (0x38<<12)|(0x02<<4)|(0x01);//fla for 2440 ,this if diff to 2410

 s3c_device_usb.dev.platform_data = &usb_yl_info;
 // while(upllvalue!=__raw_readl(S3C2410_UPLLCON))
 //{
 //__raw_writel(upllvalue,S3C2410_UPLLCON);
   //mdelay(1);
 //}
 return 0;
}
////////////////////
static struct platform_device *smdk2410_devices[] __initdata = {
	&s3c_device_usb,
	&s3c_device_lcd,
	&s3c_device_wdt,
	&s3c_device_i2c,
	&s3c_device_iis,
	&s3c_device_nand,
        &s3c_device_ts, //fla
        &s3c_device_sdi,//fla
        &s3c_device_rtc,  //added by bwx
        &s3c_device_dm9000,

     
};

static struct s3c24xx_board smdk2410_board __initdata = {
	.devices       = smdk2410_devices,
	.devices_count = ARRAY_SIZE(smdk2410_devices)
};

void __init smdk2410_map_io(void)
{
	s3c24xx_init_io(smdk2410_iodesc, ARRAY_SIZE(smdk2410_iodesc));
	s3c24xx_init_clocks(0);
	s3c24xx_init_uarts(smdk2410_uartcfgs, ARRAY_SIZE(smdk2410_uartcfgs));
	s3c24xx_set_board(&smdk2410_board);
	set_s3c2410ts_info(&yl2410_ts_cfg);//fla
	usb_yl_init();                  //fla
}

void __init smdk2410_init_irq(void)
{
	s3c24xx_init_irq();
}

MACHINE_START(SMDK2410, "SMDK2410") /* @TODO: request a new identifier and switch
				    * to SMDK2410 */
     MAINTAINER("Jonas Dietsche")
     BOOT_MEM(S3C2410_SDRAM_PA, S3C2410_PA_UART, (u32)S3C24XX_VA_UART)
     BOOT_PARAMS(S3C2410_SDRAM_PA + 0x100)
     MAPIO(smdk2410_map_io)
     INITIRQ(smdk2410_init_irq)
	.timer		= &s3c24xx_timer,
MACHINE_END


