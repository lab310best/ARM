/*

Module Name:	SPI.CPP

Abstract:		SPI Interface Routines for Samsung SC2410 CPU
  
Notes:			Presently, only the SPI Channel 0 is supported.

Environment:	Samsung SC2410 CPU

Date: 			2004/01/28

By: 			Li zhongmin
   
*/

#include	<asm-arm/arch-s3c2410/regs-gpio.h> //fla
#include	<asm-arm/arch-s3c2410/map.h>  //fla
#include	<asm-arm/arch-s3c2410/regs-irq.h>  //fla
#include	<linux/delay.h>
#include  <asm-arm/io.h>  //fla
#include  <asm-arm/arch-s3c2410/regs-clock.h> //fla
extern unsigned long eintmask,spcon0,spsta0,sppin0,sppre0,sptdat0,sprdat0,spcon1,spsta1,sppin1,sppre1,sptdat1,sprdat1; //fla from mcpcan.c
//#include 	"2410addr.h"
#include 	"def.h"
#include 	"spi.h"
#include	"option.h"

volatile DWORD g_dwWaitCounter = 0;
//----------------------------------------------------------------------------------
//extern str[256];

// Used to wait the specified # of clock cycles
#define WAIT(x)		{ for(g_dwWaitCounter=0; g_dwWaitCounter<x; g_dwWaitCounter++); }				


void Port_Init(void)
{
    //CAUTION:Follow the configuration order for setting the ports. 
    // 1) setting value(GPnDAT) 
    // 2) setting control register  (GPnCON)
    // 3) configure pull-up resistor(GPnUP)  

    //32bit data bus configuration  
    //=== PORT A GROUP
    //Ports  : GPA22 GPA21  GPA20 GPA19 GPA18 GPA17 GPA16 GPA15 GPA14 GPA13 GPA12  
    //Signal : nFCE nRSTOUT nFRE   nFWE  ALE   CLE  nGCS5 nGCS4 nGCS3 nGCS2 nGCS1 
    //Binary :  1     1      1  , 1   1   1    1   ,  1     1     1     1
    //Ports  : GPA11   GPA10  GPA9   GPA8   GPA7   GPA6   GPA5   GPA4   GPA3   GPA2   GPA1  GPA0
    //Signal : ADDR26 ADDR25 ADDR24 ADDR23 ADDR22 ADDR21 ADDR20 ADDR19 ADDR18 ADDR17 ADDR16 ADDR0 
    //Binary :  1       1      1      1   , 1       1      1      1   ,  1       1     1      1         
    
    //*(unsigned int *)rGPACON = 0x7fffff; 
    writel(0x7fffff,rGPACON);

    //===* PORT B GROUP
    //Ports  : GPB10    GPB9    GPB8    GPB7    GPB6     GPB5    GPB4   GPB3   GPB2     GPB1      GPB0
    //Signal : nXDREQ0 nXDACK0 nXDREQ1 nXDACK1 nSS_KBD nDIS_OFF L3CLOCK L3DATA L3MODE nIrDATXDEN Keyboard
    //Setting: INPUT  OUTPUT   INPUT  OUTPUT   INPUT   OUTPUT   OUTPUT OUTPUT OUTPUT   OUTPUT    OUTPUT 
    //Binary :   00  ,  01       00  ,   01      00   ,  01       01  ,   01     01   ,  01        01  
    
    //*(unsigned int *)rGPBCON = 0x044555;
     writel(0x044555,rGPBCON);
    
    //*(unsigned int *)rGPBUP  = 0x7ff;     // The pull up function is disabled GPB[10:0]
     writel(0x7ff,rGPBUP);

    //=== PORT C GROUP
    //Ports  : GPC15 GPC14 GPC13 GPC12 GPC11 GPC10 GPC9 GPC8  GPC7   GPC6   GPC5 GPC4 GPC3  GPC2  GPC1 GPC0
    //Signal : VD7   VD6   VD5   VD4   VD3   VD2   VD1  VD0 LCDVF2 LCDVF1 LCDVF0 VM VFRAME VLINE VCLK LEND  
    //Binary :  10   10  , 10    10  , 10    10  , 10   10  , 10     10  ,  10   10 , 10     10 , 10   10
    
    
    //*(unsigned int *)rGPCCON = 0xaaaaaaaa;       
    writel(0xaaaaaaaa,rGPCCON);
    
    //*(unsigned int *)rGPCUP  = 0xffff;     // The pull up function is disabled GPC[15:0] 
     writel(0xffff,rGPCUP);
    
    //=== PORT D GROUP
    //Ports  : GPD15 GPD14 GPD13 GPD12 GPD11 GPD10 GPD9 GPD8 GPD7 GPD6 GPD5 GPD4 GPD3 GPD2 GPD1 GPD0
    //Signal : VD23  VD22  VD21  VD20  VD19  VD18  VD17 VD16 VD15 VD14 VD13 VD12 VD11 VD10 VD9  VD8
    //Binary : 10    10  , 10    10  , 10    10  , 10   10 , 10   10 , 10   10 , 10   10 ,10   10
   
   // *(unsigned int *)rGPDCON = 0xaaaaaaaa;       
   writel(0xaaaaaaaa,rGPDCON);
    
   // *(unsigned int *)rGPDUP  = 0xffff;     // The pull up function is disabled GPD[15:0]
    writel(0xffff,rGPDUP);

    //=== PORT E GROUP
    //Ports  : GPE15  GPE14 GPE13   GPE12   GPE11   GPE10   GPE9    GPE8     GPE7  GPE6  GPE5   GPE4  
    //Signal : IICSDA IICSCL SPICLK SPIMOSI SPIMISO SDDATA3 SDDATA2 SDDATA1 SDDATA0 SDCMD SDCLK I2SSDO 
    //Binary :  10     10  ,  10      10  ,  10      10   ,  10      10   ,   10    10  , 10     10  ,     
    //-------------------------------------------------------------------------------------------------------
    //Ports  :  GPE3   GPE2  GPE1    GPE0    
    //Signal : I2SSDI CDCLK I2SSCLK I2SLRCK     
    //Binary :  10     10  ,  10      10 
    
    
   // *(unsigned int *)rGPECON = 0xaaaaaaaa;       
    writel(0xaaaaaaaa,rGPECON);
    
    //*(unsigned int *)rGPEUP  = 0xffff;     // The pull up function is disabled GPE[15:0]
     writel(0xffff,rGPEUP);
     
    //=== PORT F GROUP
    //Ports  : GPF7   GPF6   GPF5   GPF4      GPF3     GPF2  GPF1   GPF0
    //Signal : nLED_8 nLED_4 nLED_2 nLED_1 nIRQ_PCMCIA EINT2 KBDINT EINT0
    //Setting: Output Output Output Output    EINT3    EINT2 EINT1  EINT0
    //Binary :  01      01 ,  01     01  ,     10       10  , 10     10
//    *(unsigned int *)rGPFCON = 0x55aa;
   
    //*(unsigned int *)rGPFCON = 0xaaaa;
     writel(0xaaaa,rGPFCON);
   
   // *(unsigned int *)rGPFUP  = 0xff;     // The pull up function is disabled GPF[7:0]
     writel(0xff,rGPFUP);


#if 0       // LIUSJ, Commented for AIJI
    //*** PORT G GROUP
    //Ports  : GPG15 GPG14 GPG13 GPG12 GPG11    GPG10    GPG9     GPG8     GPG7      GPG6    
    //Signal : nYPON  YMON nXPON XMON  EINT19 DMAMODE1 DMAMODE0 DMASTART KBDSPICLK KBDSPIMOSI
    //Setting: nYPON  YMON nXPON XMON  EINT19  Output   Output   Output   SPICLK1    SPIMOSI1
    //Binary :   11    11 , 11    11  , 10      01    ,   01       01   ,    11         11
    //-----------------------------------------------------------------------------------------
    //Ports  :    GPG5       GPG4    GPG3    GPG2    GPG1    GPG0    
    //Signal : KBDSPIMISO LCD_PWREN EINT11 nSS_SPI IRQ_LAN IRQ_PCMCIA
    //Setting:  SPIMISO1  LCD_PWRDN EINT11   nSS0   EINT9    EINT8
    //Binary :     11         11   ,  10      11  ,  10        10
    *(unsigned int *)rGPGCON = 0xff95ffba;
    *(unsigned int *)rGPGUP  = 0xffff;    // The pull up function is disabled GPG[15:0]
#else     
	//config GPG15-12 to Touch Panel signals
	//config GPG11-10 to output pins
	//config GPG9-8   to output pins
	//config GPG7-5   to output pins ----Testpoints:TP6,4,2
	//config GPG4     to LCD Power Enable
	//config GPG3-2   to output pins ----Testpints:TP8,7
	//config GPG1-0   to output pins ----GPRS_boot and GPRS_PWR 
	
	//*(unsigned int *)rGPGCON = 0xff055555;
	writel(0xff055556,rGPGCON); //5->6  fla
	
	//*(unsigned int *)rGPGUP  = 0xfffff;
	writel(0xfffff,rGPGUP);
	
#endif

/*    
    //GPG4 Output Port [9:8] 01      -> LCD_PWREN Enable
    rGPGCON = (rGPGCON & 0xfffffcff) | (1<<8);
    rGPGDAT = (rGPGDAT & 0xffef) | (1<<4);
*/
    //=== PORT H GROUP
    //Ports  :  GPH10    GPH9  GPH8 GPH7  GPH6  GPH5 GPH4 GPH3 GPH2 GPH1  GPH0 
    //Signal : CLKOUT1 CLKOUT0 UCLK nCTS1 nRTS1 RXD1 TXD1 RXD0 TXD0 nRTS0 nCTS0
    //Binary :   10   ,  10     10 , 11    11  , 10   10 , 10   10 , 10    10
    //*(unsigned int *)rGPHCON = 0x2afaaa;
    writel(0x2afaaa,rGPHCON);
    
    
    //*(unsigned int *)rGPHUP  = 0x7ff;    // The pull up function is disabled GPH[10:0]
    writel(0x7ff,rGPHUP);
    
    //External interrupt will be falling edge triggered. 
    //*(unsigned int *)rEXTINT0 = 0x22222222;    // EINT[7:0]
    writel(0x22222222,S3C2410_EXTINT0);
    
    //*(unsigned int *)rEXTINT1 = 0x22222222;    // EINT[15:8]
    writel(0x22222222,S3C2410_EXTINT1);
    
    //*(unsigned int *)rEXTINT2 = 0x22222222;    // EINT[23:16]
    writel(0x22222222,S3C2410_EXTINT2);
    
    //fla init interr again!
    s3c2410_gpio_cfgpin(S3C2410_GPG3, S3C2410_GPG3_EINT11);
    writel(readl(S3C2410_INTMSK)& (~(1<<5)),S3C2410_INTMSK);
    writel(readl(eintmask)& (~(1<<11)),eintmask);
    writel(readl(S3C2410_EXTINT1)|(0x00007000)&(0xffffbfff),S3C2410_EXTINT1);
  //  s3c2410_gpio_pullup(S3C2410_GPG3,0); //dis
    
    
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:		SPI_Init()

Description:	Initializes the Serial Peripheral Interface (SPI)

Notes:			This routine assumes that the control registers (see 
				the globals section above) have already been initialized.

Returns:		Boolean indicating success.
-------------------------------------------------------------------*/
BOOL SPI_Init(VOID)
{
	int index;

//	index = *(unsigned int *)rCLKCON;
//	*(unsigned int *)rCLKCON = index;
//	index = 0;
//	index = *(unsigned int *)rCLKCON;
//	printk("rCLKCON = %d\n",index);

	
	//*(unsigned int *)rGPGDAT = (*(unsigned int *)rGPGDAT)|CHIP_DESELECT_nSS0;
	writel(readl(rGPGDAT)|CHIP_DESELECT_nSS0,rGPGDAT);
	
	//chip select SPI1 ???
	
	//----- 1. IMPORTANT: By default, the internal clock is disabled.  To configure the controller ------
	//					  we must first enable it.
	StartSPIClock();
    
	SetSPIClockRate(CLK_RATE_SLOW);

	//----- 2. Configure the GPIO pins for SPI mode -----
	//
	//		   nSPICS0  (chip select)		= GPG2
	//		   SPICLK0  (SPI clock)			= GPE13
	//		   SPIMOSIO (SPI output data)	= GPE12
/*   
	*(unsigned int *)rGPGCON = (*(unsigned int *)rGPGCON) & CLEAR_GPG2_MASK;
	*(unsigned int *)rGPGCON = (*(unsigned int *)rGPGCON) | ENABLE_GPG2_OUTPUT;
	*(unsigned int *)rGPGUP  = (*(unsigned int *)rGPGUP)  & ENABLE_GPG2_PULLUP;
//	port_outl(rGPGUP , (port_inl(rGPGUP) & 0x0));
	*(unsigned int *)rGPGCON = (*(unsigned int *)rGPECON) & 0xf03fffff;
	*(unsigned int *)rGPECON = (*(unsigned int *)rGPECON) | ENABLE_SPICLK0 | ENABLE_SPIMSIO;
//	*(unsigned int *)rGPEUP  = (*(unsigned int *)rGPEUP£© | DISABLE_SPICLK_SPIMSIO_PULLUP;	// Disable pullup-resistor for SPICLK0 and SPIMOSIO   the name is too long 
//	*(unsigned int *)rGPEUP  = (*(unsigned int *)rGPEUP)  | TEST;    //#define TEST 0x00003800 in spi.h
	*(unsigned int *)rGPEUP  = (*(unsigned int *)rGPEUP)  | 0x00003800;
//	port_outl(rGPEUP , (port_inl(rGPEUP)  | 0xffffffff));
*/
	Port_Init();

	//----- 3. Configure the SPI controller with reasonable default values -----
	
	//*(unsigned int *)rSPCON0 = SPI_MODE_POLLING | SPI_SELECT_MASTER | SPI_CLOCK_ENABLE;
	writel(SPI_MODE_POLLING | SPI_SELECT_MASTER | SPI_CLOCK_ENABLE,spcon0);
	
	//*(unsigned int *)rSPCON1 = SPI_MODE_POLLING | SPI_SELECT_MASTER | SPI_CLOCK_ENABLE;
	writel(SPI_MODE_POLLING | SPI_SELECT_MASTER | SPI_CLOCK_ENABLE,spcon1);

	for( index = 0; index < 20; index++)
		//*(unsigned char *)rSPTDAT0 =  0xFF;
    writel(0xFF,sptdat0);
	
	for( index = 0; index < 20; index++)
		//*(unsigned char *)rSPTDAT1 =  0xFF;
		writel(0xFF,sptdat1);

    //StopSPIClock();

	return TRUE;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:		SPI_Deinit()

Description:	Deinitializes the Serial Peripheral Interface (SPI)

Notes:			This routine DOES NOT unmap the control registers;
				the caller is responsible for freeing this memory.

Returns:		Boolean indicating success.
-------------------------------------------------------------------*/
BOOL SPI_Deinit(VOID)
{
	//----- 1. Stop the SPI clocks -----
    StopSPIClock();

	return TRUE;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:		SPI_SendByte()

Description:	Sends the specified byte out onto the SPI bus.

Returns:		Boolean indicating success.
-------------------------------------------------------------------*/
BOOL SPI_SendByte(BYTE bData, BYTE* pData)
{
    //----- 0. Start clock
    //StartSPIClock();

	//----- 1. Chip select the slave device (active low) -----
	//rGPGDAT &= CHIP_SELECT_nSS0;

	//----- 2. Wait until the controller is ready to transfer -----
	if(SPI_WaitTxRxReady()==FALSE) return FALSE;
	

//	printk("after the 1st wait in SPI_SendByte.\n");

	//----- 3. Put the byte out onto the SPI bus -----
	
	//*(unsigned char *)rSPTDAT0 =  bData;
	//printk("in spi.c:the maped addr of sptdat = %#p\n",sptdat0);
	writel(bData,sptdat0);
	
	
	//----- 4. Delay a little bit so the byte finishes clocking out before the chip select line is deasserted -----
	if(SPI_WaitTxRxReady()==FALSE) return FALSE;
	
//	printk("after the 2nd wait in SPI_SendByte.\n");

	//*pData = *(unsigned char *)rSPRDAT0;
	*pData = readl(sprdat0);

	//----- 5. Deselect the slave device (active low) -----
	//rGPGDAT |= CHIP_DESELECT_nSS0;

   // StopSPIClock();

	return TRUE;
}

BOOL SPI_ReadByte(BYTE *pData)
{
    BOOL    bRet = TRUE;
    
    //----- 0. Start clock
    //StartSPIClock();

	//----- 1. Chip select the slave device (active low) -----
	//rGPGDAT &= CHIP_SELECT_nSS0;

	//----- 2. Wait until the controller is ready to transfer -----
	if(SPI_WaitTxRxReady()==FALSE) return FALSE;

	//----- 3. Put the byte out onto the SPI bus -----
	
	//*pData = *(unsigned char *)rSPRDAT0;
	*pData = readl(sprdat0);
	//----- 4. Delay a little bit so the byte finishes clocking out before the chip select line is deasserted -----
	//if(SPI_WaitTxRxReady()==FALSE) return FALSE;

	//----- 5. Deselect the slave device (active low) -----
	//rGPGDAT |= CHIP_DESELECT_nSS0;

   // StopSPIClock();

	return bRet;	
}

BOOL SPI_WaitTxRxReady(VOID)
{
	int waitCount;
//	int reg;
	
	//----- 1. Wait until the controller is ready to transfer -----
	waitCount = 1000;
	while(!((readl(spsta0)) & SPI_TRANSFER_READY))
	{
		if(--waitCount == 0)
		{
//			reg = *(unsigned int *)rSPSTA0;
//			printk("rSPSTA0 = %d\n",reg);
//			printk("WaitTxRxReady return FALSE.\n");			

//			ssleep(1);
//			msleep(1);

			mdelay(10);
		//	udelay(1);
//			ndelay(1);

			return FALSE;
		}
	}
	
	return TRUE;
	
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:		SPI_SendWord()

Description:	Sends the specified word out onto the SPI bus.

Returns:		Boolean indicating success.
-------------------------------------------------------------------*/
/*BOOL SPI_SendWord(WORD wData)
{
    BOOL    bRet = TRUE;
	DWORD   waitCount = 0;

    StartSPIClock();

	//----- 1. Chip select the slave device (active low) -----
	rGPGDAT &= CHIP_SELECT_nSS0;

	//----- 2. Wait until the controller is ready to transfer -----
	waitCount = 1000;
	while(!(rSPSTA0 & SPI_TRANSFER_READY))
	{
		if(--waitCount == 0)
		{
            bRet = FALSE;
			goto SEND_ERROR;
		}
	}

	//----- 3. Send first half of word -----
	rSPTDAT0 = (wData & 0xFF00) >> 8;

	//----- 4. Wait until the controller is ready to transfer -----
	waitCount = 1000;
	while(!(rSPSTA0 & SPI_TRANSFER_READY))
	{
		if(--waitCount == 0)
		{
			//RETAILMSG(1, (TEXT("WAVEDEV.DLL: SPI_SendWord() - timeout occurred while waiting to transfer byte\r\n")));
            bRet = FALSE;
			goto SEND_ERROR;
		}
	}

	//----- 5. Send second half of word -----
	rSPTDAT0 = (wData & 0x00FF);

	//----- 6. Delay a little bit so the byte finishes clocking out before the chip select line is deasserted -----
	WAIT(10000);

	//----- 7. Deselect the slave device (active low) -----
	rGPGDAT |= CHIP_DESELECT_nSS0;

SEND_ERROR:

    StopSPIClock();

	return bRet;
}
*/

//------------------------------------ Helper Routines ------------------------------------

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:		StartSPIClock()

Description:	Enables the SPI clock.

Returns:		N/A
-------------------------------------------------------------------*/
VOID StartSPIClock(VOID)
{
	//*(unsigned int *)rCLKCON = (*(unsigned int *)rCLKCON)|SPI_INTERNAL_CLOCK_ENABLE;// Enable the CPU clock to the SPI controller
  writel(readl(S3C2410_CLKCON)|SPI_INTERNAL_CLOCK_ENABLE,S3C2410_CLKCON);
	
	
	//*(unsigned int *)rSPCON0 = (*(unsigned int *)rSPCON0)|SPI_CLOCK_ENABLE|SPI_SELECT_MASTER;// Enable the SPI clock
	writel(readl(spcon0)|SPI_CLOCK_ENABLE|SPI_SELECT_MASTER,spcon0);
	
	return;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:		StopSPIClock()

Description:	Disables the SPI clock.

Returns:		N/A
-------------------------------------------------------------------*/
VOID StopSPIClock(VOID)
{
	//*(unsigned int *)rCLKCON = (*(unsigned int *)rCLKCON) & ~SPI_INTERNAL_CLOCK_ENABLE;	// Disable the CPU clock to the SPI controller
  	writel(readl(S3C2410_CLKCON) & ~SPI_INTERNAL_CLOCK_ENABLE,S3C2410_CLKCON);


	//*(unsigned int *)rSPCON0 = (*(unsigned int *)rCLKCON) & ~SPI_CLOCK_ENABLE;				// Disable the SPI clock
	writel(readl(spcon0)& ~SPI_CLOCK_ENABLE,spcon0);//fla should be a error in up line!
	//writel(readl(spcon0)&0x6f,spcon0);//Disable the SPI clock
	return;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:		SetSPIClockRate()

Description:	Sets the SPI clock (a.k.a. baud) rate:

Params:			ClockRate       0x00 =      25Mhz
								0x01 = 1/2  25Mhz
								0x02 = 1/4  25Mhz
								0x03 = 1/8  25Mhz
								0x04 = 1/16 25Mhz
								0x05 = 1/32 25Mhz
								0x06 = 1/64 25Mhz
								0x07 = 99.121Khz

Returns:		N/A
-------------------------------------------------------------------*/
VOID SetSPIClockRate(DWORD ClockRate)
{
	UINT16 prescale;

    //----- 1. Set the clock rate  -----
	//		   NOTE: Using the prescale value, the clock's baud rate is
	//				 determined as follows: baud_rate = (PCLK / 2 / (prescale + 1))
	//
	//				 Hence, the prescale value can be computed as follows:
	//				 prescale = ((PCLK / 2) / baud_rate) - 1
	switch(ClockRate)
	{
		case CLK_RATE_FULL:						// 25 Mhz			
		prescale = (PCLK / 40000000) - 1;
		break;

		case CLK_RATE_HALF:						// 1/2  25 Mhz						
		prescale = (PCLK / 30000000) - 1;
		break;

		case CLK_RATE_FOUR:						// 1/4  25 Mhz						
		prescale = (PCLK / 10000000) - 1;
		break;

		case CLK_RATE_EIGHT:					// 1/8  25 Mhz						
		prescale = (PCLK / 500000) - 1;
		break;

		case CLK_RATE_SIXTEEN:					// 1/16 25 Mhz						
		prescale = (PCLK / 2500000) - 1;
		break;

		case CLK_RATE_THIRTY2:					// 1/32 25 Mhz						
		prescale = (PCLK / 1250000) - 1;
		break;

		case CLK_RATE_SIXTY4:					// 1/64 25 Mhz						
		prescale = (PCLK / 625000) - 1;
		break;

		case CLK_RATE_SLOW:						// 99.121 kHz (i.e. ~1/256 25Mhz)
		prescale = 255;
		break;

		default:
		return;
		break;
	}

	//*(unsigned int *)rSPPRE0 = prescale;
  writel(prescale,sppre0);

    return;
}
