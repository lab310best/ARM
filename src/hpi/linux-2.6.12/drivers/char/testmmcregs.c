#include <linux/init.h>
#include <linux/module.h>
#include <asm-arm/io.h>
//#include <asm/arch/regs-gpio.h>   //for mmc
//#include <asm/arch/regs-sdi.h>    //for mmc
//#include <asm/arch/regs-clock.h>  //for mmc
#include <linux/delay.h>          //for mmc
#include <linux/ioport.h>
#define UPLLCON (0x4c000008)
#define CLKCON  (0x4c00000C)
#define CLKSLOW (0x4c000010)
#define CLKDIVN (0x4c000014)


#define SUBSRCPND (0x4A000018)
#define INTSUBMSK (0x4A00001C)
#define INTMSK    (0x4A000008)
#define ADCCON (0x58000000)
#define ADCTSC (0x58000004)
#define ADCDLY (0x58000008)


//for mmc driver use dma transfer!
#define SDICON (0x5A000000)
#define SDIPRE (0x5A000004)
#define SDICCON (0x5A00000C)
#define SDIBSIZE (0x5A000028)
#define SDIDCON (0x5A00002C)
#define SDIIMSK (0x5A00003C)

#define DCON0 (0x4B000010)
#define DCON1 (0x4B000050)
#define DCON2 (0x4B000090)
#define DCON3 (0x4B0000D0)

#define DMASKTRIG0 (0x4B000020)
#define DMASKTRIG1 (0x4B000060)
#define DMASKTRIG2 (0x4B0000A0)
#define DMASKTRIG3 (0x4B0000E0)

MODULE_LICENSE("Dual BSD/GPL");
//#define FIFO_TYPEA (0<<4)
//#define FIFO_TYPEB (1<<4)
//#define FIFO_TYPE   FIFO_TYPEA 
//unsigned long subsrcpnd,intsubmsk,intmsk,adccon,adctsc,adcdly;

//unsigned long upllcon,clkcon,clkslow,clkdivn,upllconbak,clkconbak,clkslowbak,clkdivnbak;
unsigned long sdicon,sdipre,sdiccon,sdibsize,sdidcon,sdiimsk,
              dcon0,dcon1,dcon2,dcon3,dmsktrig0,dmsktrig1,dmsktrig2,dmsktrig3;

static int hello_init(void)

{
  /*  upllcon = ioremap(UPLLCON,0x00000004);
    clkcon  = ioremap(CLKCON,0x00000004);
    clkslow = ioremap(CLKSLOW,0x00000004);
    clkdivn = ioremap(CLKDIVN,0x00000004);
    printk(KERN_ERR "OLD:\nUPLLCON = 0x%08x\nCLKCON = 0x%08x\nCLKSLOW = 0x%08x\nCLKDIVN = 0x%08x\nstarting modifiy upllcon...\n\n\n",readl(upllcon),readl(clkcon),readl(clkslow),readl(clkdivn));
    
    upllconbak = readl(upllcon);
    clkconbak =  readl(clkcon);
    clkslowbak = readl(clkslow);
    clkdivnbak = readl(clkdivn);
    
    writel(0x00f2eb70,clkcon);
    writel(0x00000004,clkslow);
    writel(0x0000000d,clkdivn);
    writel(0x00038021,upllcon);
    printk(KERN_ERR "NEW:\nUPLLCON = 0x%08x\nCLKCON = 0x%08x\nCLKSLOW = 0x%08x\nCLKDIVN = 0x%08x\n",readl(upllcon),readl(clkcon),readl(clkslow),readl(clkdivn));   
    
/*subsrcpnd = ioremap(SUBSRCPND,0x00000004);
intsubmsk = ioremap(INTSUBMSK,0x00000004);
intmsk = ioremap(INTMSK,0x00000004);

adccon = ioremap(ADCCON,0x00000004);
adctsc = ioremap(ADCTSC,0x00000004);
adcdly = ioremap(ADCDLY,0x00000004);
//printk(KERN_ERR "ADCCON= 0x%08x\nADCTSC = 0x%08x\nADCDLY = 0x%08x\nSUBPND = 0x%08x\
//INTSUBMSK = 0x%08x\nINTMSK = 0x%08x\n",readl(adccon),readl(adctsc),readl(adcdly),readl(subsrcpnd),readl(intsubmsk),readl(intmsk));

 /* writel(0x000059f7,intsubmsk);
	writel(0x637e3d9f,intmsk);
	//itel(readl(intsubmsk)&(~(0x1<<9)),intsubmsk);
  writel(0x0000cc40,adccon);
  writel(0x000000d3,adctsc);
  writel(0x00007530,adcdly);
  */
  
  
  /*
  writel(readl(intsubmsk)&0x00000000|0x000059f7,intsubmsk);
  writel(readl(intmsk)&0x00000000|0x637e3d9f,intmsk);
  writel(readl(adccon)&0x00000000|0xcc40,adccon);
  writel(readl(adctsc)&0x00000000|0xd3,adctsc);
  writel(readl(adcdly)&0x00000000|0x7530,adcdly);
  */
  /*__raw_writel(0x000000d3,adctsc);
  printk(KERN_ERR "	hello,ADCTSC = 0x%08x\n",__raw_readl(adctsc));
/*
printk(KERN_ERR "new set.......\n");*/
//printk(KERN_ERR "\nADCTSC = 0x%08x\nADCDLY = 0x%08x\nSUBPND = 0x%08x\n\
//INTSUBMSK = 0x%08x\nINTMSK = 0x%08x\n",readl(adctsc),readl(adcdly),readl(subsrcpnd),readl(intsubmsk),readl(intmsk));


//ADCCON= 0x%08x readl(adccon),


    sdicon  = ioremap(SDICON,0x00000004);
    sdipre = ioremap(SDIPRE,0x00000004);
    sdiccon = ioremap(SDICCON,0x00000004);
    sdibsize  = ioremap(SDIBSIZE,0x00000004);
    sdidcon = ioremap(SDIDCON,0x00000004);
    sdiimsk = ioremap(SDIIMSK,0x00000004);
    dcon0  = ioremap(DCON0,0x00000004);
    dcon1 = ioremap(DCON1,0x00000004);
    dcon2 = ioremap(DCON2,0x00000004);
    dcon3  = ioremap(DCON3,0x00000004);
    dmsktrig0 = ioremap(DMASKTRIG0,0x00000004);
    dmsktrig1 = ioremap(DMASKTRIG1,0x00000004);
    dmsktrig2  = ioremap(DMASKTRIG2,0x00000004);
    dmsktrig3 = ioremap(DMASKTRIG3,0x00000004);
    
printk(KERN_ERR "!SDICON = %08x\n\
                  SDIPRE = %08x\n\
                  SDICCON = %08x\n\
                  SDIBSIZE = %08x\n\
                 !SDIDCON = %08x\n\
                  SDIIMSK = %08x\n\
                  DCON0 = %08x\n\
                  DCON1 = %08x\n\
                  DCON2 = %08x\n\
                  DCON3 = %08x\n\
                  DMASKTRIG0 = %08x\n\
                  DMASKTRIG1 = %08x\n\
                  DMASKTRIG2 = %08x\n\
                  DMASKTRIG3 = %08x\n",
                 readl(sdicon),readl(sdipre),
                 readl(sdiccon),readl(sdibsize),
                 readl(sdidcon),readl(sdiimsk),
                 readl(dcon0),readl(dcon1),
                 readl(dcon2),readl(dcon3),
                 readl(dmsktrig0),readl(dmsktrig1),
                 readl(dmsktrig2),readl(dmsktrig3));
                
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 return 0;

}



static void hello_exit(void)

{
    /*writel(clkconbak,clkcon);
    writel(clkslowbak,clkslow);
    writel(clkdivnbak,clkdivn);
    writel(upllconbak,upllcon);
    printk(KERN_ALERT "Goodbye,reset old regs...\n");
    printk(KERN_ALERT "CLKCON = %08x\nCLKSLOW=%08x\nCLKDIVN=%08x\nUPLLCON=%08x\n"
     ,clkconbak,clkslowbak,clkdivnbak,upllconbak);*/
printk(KERN_ALERT "Goodbye,beatiful world!\n");
}



module_init(hello_init);

module_exit(hello_exit);
 
