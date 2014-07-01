/*

Module Name:	SPI_CMD.C

Abstract:		Function definition for the CAN's spi command interface
  
Notes:			Presently, only the SPI Channel 0 is supported.

Environment:	MCP2515

Date: 			2004/01/28

By: 			Li zhongmin
   
*/ 


#include	<asm-arm/arch-s3c2410/regs-gpio.h>
#include  <asm-arm/arch-s3c2410/map.h>
#include <asm/io.h>
#include 	"spi.h"
#include	"spi_cmd.h"

unsigned char CAN_SPI_CMD( unsigned char cmd, unsigned long addr, unsigned char arg1, unsigned char arg2 )
{
	unsigned char ret=0;
	unsigned char data=0;	

	// Empty the spi's transfer and receive buffer
	// SPI_TXFlush( CAN_SPI_MODULE );
	// SPI_RXFlush( CAN_SPI_MODULE );
	
	//StartSPIClock();
//	printk("in CAN_SPI_CMD,line %d passed!\n",__LINE__);
	//*(unsigned int *)rGPGDAT = (*(unsigned int *)rGPGDAT) & CHIP_SELECT_nSS0;
  writel(readl(rGPGDAT)& CHIP_SELECT_nSS0,rGPGDAT);  //set cs:gpg2 to 0
 // printk("in CAN_SPI_CMD,have set cs effect!\n");
 
	switch( cmd ){
		case SPI_CMD_READ:
			
			//printk("SPI_CMD_READ\n");
			ret = SPI_SendByte(SPI_CMD_READ, &data);
			ret = SPI_SendByte(addr, &data);			
			ret = SPI_SendByte(0xff, &data);	
			//ret = SPI_ReadByte(&data); //fla
			//printk("readed bytedate = %08x\n",data) ;//fla
			break;
			
		case SPI_CMD_WRITE:
			//printk("SPI_CMD_WRITE\n");
			ret = SPI_SendByte(SPI_CMD_WRITE, &data);
			//printk("send byte return value = %d\n",ret);
			ret = SPI_SendByte(addr, &data);
			ret = SPI_SendByte(arg1, &data);	
						
			break;
		case SPI_CMD_RTS:
			
			ret = SPI_SendByte(SPI_CMD_RTS, &data);
			
			break;
		case SPI_CMD_READSTA:
			
			ret = SPI_SendByte(SPI_CMD_READSTA, &data);
			ret = SPI_SendByte(0xff, &data);			
			
			break;
		case SPI_CMD_BITMOD:
		
			//printk("SPI_CMD_BITMOD\n");	
			ret = SPI_SendByte(SPI_CMD_BITMOD, &data);
			//printk("send byte 1 return value = %d\n",ret);

			ret = SPI_SendByte(addr, &data);
	//	printk("send byte 2 return value = %d\n",ret);

			ret = SPI_SendByte(arg1, &data);	
		//	printk("send byte 3 return value = %d\n",ret);

			ret = SPI_SendByte(arg2, &data);				
		//	printk("send byte 4 return value = %d\n",ret);
			
			break;
		case SPI_CMD_RESET:

			printk("SPI_CMD_RESET\n");
			ret = SPI_SendByte(SPI_CMD_RESET, &data);
			break;
		default:
			ret = 0x30;										// any value is ok
	}
	
	//*(unsigned int *)rGPGDAT = (*(unsigned int *)rGPGDAT)|CHIP_DESELECT_nSS0;
	writel(readl(rGPGDAT)|CHIP_DESELECT_nSS0,rGPGDAT);
	//	printk("in CAN_SPI_CMD,have set cs noeffect!\n");
 	//StopSPIClock();
			 
	//SPI_RXFlush( CAN_SPI_MODULE );
	
	return data;
}
