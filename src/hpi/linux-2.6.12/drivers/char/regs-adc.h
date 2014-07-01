/* linux/include/asm/arch-s3c2410/regs-adc.h
 *
 * Copyright (c) 2003 Simtec Electronics <linux@simtec.co.uk>
 *		      http://www.simtec.co.uk/products/SWLINUX/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * S3C2410 Internal RTC register definition
 *
 *  Changelog:
 *    19-06-2003     BJD     Created file
 *    12-03-2004     BJD     Updated include protection
*/
/*
#ifndef __ASM_ARCH_REGS_ADC_H
#define __ASM_ARCH_REGS_ADC_H __FILE__

#define S3C2410_ADCREG(x) ((x) + S3C2410_VA_ADC)

#define S3C2410_ADCCON	      S3C2410_ADCREG(0x00)
#define S3C2410_ADCTSC	      S3C2410_ADCREG(0x04)
#define S3C2410_ADCDLY	      S3C2410_ADCREG(0x08)
#define S3C2410_ADCDAT0	      S3C2410_ADCREG(0x0c)
#define S3C2410_ADCDAT1	      S3C2410_ADCREG(0x10)
*/

#ifndef __ASM_ARCH_REGS_ADC_H
#define __ASM_ARCH_REGS_ADC_H "regs-adc.h"

#define S3C2410_ADCREG(x) (x)

#define S3C2410_ADCCON	   S3C2410_ADCREG(0x00)
#define S3C2410_ADCTSC	   S3C2410_ADCREG(0x04)
#define S3C2410_ADCDLY	   S3C2410_ADCREG(0x08)
#define S3C2410_ADCDAT0	   S3C2410_ADCREG(0x0C)
#define S3C2410_ADCDAT1	   S3C2410_ADCREG(0x10)




#define ADC_IN0                 0
#define ADC_IN1                 1
#define ADC_IN2                 2
#define ADC_IN3                 3
#define ADC_IN4                 4
#define ADC_IN5                 5
#define ADC_IN6                 6
#define ADC_IN7                 7
#define ADC_BUSY		1
#define ADC_READY		0
#define NOP_MODE		0
#define X_AXIS_MODE		1
#define Y_AXIS_MODE		2
#define WAIT_INT_MODE		3
/* ... */
#define ADCCON_ECFLG		(1 << 15)
#define PRESCALE_ENDIS		(1 << 14)
#define PRESCALE_DIS		(PRESCALE_ENDIS*0)
#define PRESCALE_EN		(PRESCALE_ENDIS*1)

#define PRSCVL(x)		(x << 6)
#define ADC_INPUT(x)		(x << 3)
#define ADCCON_STDBM		(1 << 2)        /* 1: standby mode, 0: normal mode */
#define ADC_NORMAL_MODE		(ADCCON_STDBM*0)
#define ADC_STANDBY_MODE	(ADCCON_STDBM*1)
#define ADCCON_READ_START	(1 << 1)
#define ADC_START_BY_RD_DIS	(ADCCON_READ_START*0)
#define ADC_START_BY_RD_EN	(ADCCON_READ_START*1)
#define ADC_START		(1 << 0)

#define UD_SEN			(1 << 8)
#define DOWN_INT		(UD_SEN*0)
#define UP_INT			(UD_SEN*1)
#define YM_SEN			(1 << 7)
#define YM_HIZ			(YM_SEN*0)
#define YM_GND			(YM_SEN*1)
#define YP_SEN			(1 << 6)
#define YP_EXTVLT		(YP_SEN*0)
#define YP_AIN			(YP_SEN*1)
#define XM_SEN			(1 << 5)
#define XM_HIZ			(XM_SEN*0)
#define XM_GND			(XM_SEN*1)
#define XP_SEN			(1 << 4)
#define XP_EXTVLT		(XP_SEN*0)
#define XP_AIN			(XP_SEN*1)
#define XP_PULL_UP		(1 << 3)
#define XP_PULL_UP_EN		(XP_PULL_UP*0)
#define XP_PULL_UP_DIS		(XP_PULL_UP*1)
#define AUTO_PST		(1 << 2)
#define CONVERT_MAN		(AUTO_PST*0)
#define CONVERT_AUTO		(AUTO_PST*1)
#define XP_PST(x)		(x << 0)

#endif /* __ASM_ARCH_REGS_ADC_H */


















