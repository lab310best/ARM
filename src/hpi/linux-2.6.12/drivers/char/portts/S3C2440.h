#ifndef __ASM_ARCH_REGS_ADC_H
#define __ASM_ARCH_REGS_ADC_H __FILE__

#define S3C2410_ADCREG(x) ((x) + S3C2410_VA_ADC)

#define S3C2410_ADCCON	      S3C2410_ADCREG(0x00)
#define S3C2410_ADCTSC	      S3C2410_ADCREG(0x04)
#define S3C2410_ADCDLY	      S3C2410_ADCREG(0x08)
#define S3C2410_ADCDAT0	      S3C2410_ADCREG(0x0c)
#define S3C2410_ADCDAT1	      S3C2410_ADCREG(0x10)
#endif






#define ADC_CTL_BASE		0x58000000
#define bADC_CTL(Nb)		__REG(ADC_CTL_BASE + (Nb))
/* Offset */
#define oADCCON			0x00	/* R/W, ADC control register */
#define oADCTSC			0x04	/* R/W, ADC touch screen ctl reg */
#define oADCDLY			0x08	/* R/W, ADC start or interval delay reg */
#define oADCDAT0		0x0c	/* R  , ADC conversion data reg */
#define oADCDAT1		0x10	/* R  , ADC conversion data reg */
//laputa append for the s3c2440 ts status pen_up or pen_down  030917
#define oADC_UPDN         0x14    /* R/W Stylus Up or Down Interrpt status register*/
#define TSC_UP			 0x02
#define TSC_DN  		 0x01
//laputa end s3c2440 adc touch screen up/down regster 030917 

/* Registers */
#define ADCCON			bADC_CTL(oADCCON)
#define ADCTSC			bADC_CTL(oADCTSC)
#define ADCDLY			bADC_CTL(oADCDLY)
#define ADCDAT0			bADC_CTL(oADCDAT0)
#define ADCDAT1			bADC_CTL(oADCDAT1)
//laputa append s3c2440 ts 030917  
#define PEN_UPDN		bADC_CTL(oADC_UPDN)




#define fADCCON_PRSCVL		Fld(8, 6)
#define fADCCON_INPUT		Fld(3, 3)
#define fTSC_XY_PST		Fld(2, 0)
#define fADC_DELAY		Fld(6, 0)
#define fDAT_UPDOWN		Fld(1, 15)
#define fDAT_AUTO_PST		Fld(1, 14)
#define fDAT_XY_PST		Fld(2, 12)
#define fDAT_XPDATA		Fld(10, 0)
#define fDAT_YPDATA		Fld(10, 0)
/* ... */
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
#if 0
#define PRSCVL(x)		({ FClrFld(ADCCON, fADCCON_PRSCVL); \
				   FInsrt((x), fADCCON_PRSCVL); })
#define ADC_INPUT(x)		({ FClrFld(ADCCON, fADCCON_INPUT); \
				   FInsrt((x), fADCCON_INPUT); })
#endif
#define PRSCVL(x)		(x << 6)
#define ADC_INPUT(x)		(x << 3)
#define ADCCON_STDBM		(1 << 2)        /* 1: standby mode, 0: normal mode */
#define ADC_NORMAL_MODE		FClrBit(ADCCON, ADCCON_STDBM)
#define ADC_STANDBY_MODE	(ADCCON_STDBM*1)
#define ADCCON_READ_START	(1 << 1)
#define ADC_START_BY_RD_DIS	FClrBit(ADCCON, ADCCON_READ_START)
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