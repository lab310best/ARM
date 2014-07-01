#include <linux/unistd.h>
#include <linux/module.h>
//#include <asm-arm/arch-s3c2410/S3C2410.h>
#include <asm-arm/arch-s3c2410/irqs.h>
#include <linux/interrupt.h> //fla
#include <asm/arch/regs-gpio.h>//fla
#include <asm/arch/hardware.h> //fla
#include <linux/slab.h>  //fla
#include <asm/arch/irqs.h> //fla
#if defined(CONFIG_SMP)
#define __SMP__
#endif

#if defined(CONFIG_MODVERSIONS)
#define MODVERSIONS
#endif

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h> 	/* for -EBUSY */
//#include <linux/malloc.h>
#include <linux/slab.h>
#include <linux/ioport.h>	/* for verify_area */
#include <linux/mm.h>
#include <linux/interrupt.h>
//#include <linux/tqueue.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/init.h>		/* for module_init */
#include <linux/module.h>

#include <asm/io.h>
#include <asm/segment.h>
#include <asm/irq.h>
#include <asm/signal.h>
#include <asm/siginfo.h>
#include <asm/uaccess.h>  	/* for get_user and put_user */
#include <asm/fcntl.h>
#include <asm/cache.h>
#include <asm/atomic.h>

#include <asm/arch-s3c2410/regs-irq.h>  //fla
#include <asm/arch-s3c2410/regs-clock.h> 

//#include <asm-arm/arch-s3c2410/S3C2410_2.h>

//#include <sys/syscall.h>

//#include <asm-arm/arch-s3c2410/S3C2410_2.h>
//#include	<asm-arm/arch-s3c2410/S3C2410_2.h>
//#include "S3C2410_2.h"

#include "mcpcan.h"
#include	"spi_cmd.h"
#include    "spi.h"

#define	MCP2510_FLAG

#define BUF_SIZE sizeof(struct MCP_device) * 100

unsigned long eintmask,spcon0,spsta0,sppin0,sppre0,sptdat0,sprdat0,spcon1,spsta1,sppin1,sppre1,sptdat1,sprdat1; //fla add for init interrupt!

//#define DEBUG

#ifdef DEBUG
#define DbgPrintk(S) printk(S)
#else
#define DbgPrintk(S)
#endif

struct MCP_device {
       volatile unsigned long RecvBuf;
       volatile unsigned long RecvHead;
       volatile unsigned long RecvTail;
       volatile unsigned long RecvNum;
                                                                                                                                               
       wait_queue_head_t inq;   //or use as a global variable
                                                                                                                                               
       struct semaphore sem;    //semaphore used for mutex
                                                                                                                                               
       unsigned int      IrqNum;
       unsigned int      MinorNum;/*device minor number*/
       unsigned int      BaudRate;
                                                                                                                                               
       unsigned char     *SendBuf;
       unsigned int      SendNum;
       volatile unsigned long SendHead;
       volatile unsigned long SendTail;
};//__attribute__((aligned(L1_CACHE_BYTES),packed));
                                                                                                                                               

/*****************************Variable define*******************************/
unsigned int MCP_major = MCP_MAJOR;
struct MCP_device *Device[MCP_NUM];
unsigned int MCP_irq[MCP_NUM] = {IRQ_EINT4t7,IRQ_EINT8t23};
static char *MCP_name[MCP_NUM] = {"mcpcan0","mcpcan1"};



/*****************************SJA device initialize*************************/
//这个函数用来对struct MCP_device进行初始化
static int MCP_device_init(void)
{
        int res,i;
//        printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);
        for(i=0;i<MCP_NUM;i++) {
                Device[i] = kmalloc(sizeof(struct MCP_device),GFP_KERNEL);  //malloc the device struct
                if(Device[i] == NULL) {
                        printk(KERN_ALERT"allocate device memory failed.\n");
                        return(-ENOMEM);
                }
                memset((char *)Device[i],0,sizeof(struct MCP_device));



                //Device[i]->RecvBuf = (unsigned char *)get_free_page(GFP_KERNEL);  //malloc the recv buffer
                Device[i]->RecvBuf = (unsigned long)kmalloc(sizeof(struct MCP_device) * 100,GFP_KERNEL);  //malloc the recv buffer
                if(Device[i]->RecvBuf == 0) {
                        printk(KERN_ALERT"allocate Recv memory failed.\n");
                        return(-ENOMEM);
                }
                memset((char *)Device[i]->RecvBuf,0,sizeof(struct MCP_device) * 100);

                Device[i]->RecvHead = Device[i]->RecvBuf;
                Device[i]->RecvTail = Device[i]->RecvBuf;
                Device[i]->RecvNum = 0;
                //Device[i]->RecvQueue = NULL;

                
                //Device[i]->SendBuf = (unsigned char *)get_free_page(GFP_KERNEL);   //malloc the send buffer
                Device[i]->SendBuf = kmalloc(sizeof(struct MCP_device) * 100,GFP_KERNEL);  //malloc the send buffer
                if(Device[i]->SendBuf == NULL) {
                        printk(KERN_ALERT"allocate Send memory failed.\n");
                        return(-ENOMEM);
                }
                memset((char *)Device[i]->SendBuf,0,sizeof(struct MCP_device) * 100);

                Device[i]->MinorNum = i;
                //Device[i]->FrameMode = 1;
                Device[i]->IrqNum = MCP_irq[i];
                //Device[i]->BaudRate = MCP_baudrate[i];
		init_waitqueue_head(&(Device[i]->inq));
		//init_waitqueue_head(&(Device[i]->outq));
		init_MUTEX(&(Device[i]->sem));
		}
        return 0;
}


/*****************************MCP device initialize*************************/
/* Initialize the mcp2510 */
void MCP2510_Init(int MinorNum )
{
	// Configure the SPI Interface in MX1
	//int canstat = 0;
//  printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);
	SPI_Init( ); 
	// Configure the mcp2510 through spi interface
//  printk("look if in configure mode:%s\n",CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->CANSTAT)), ARG_UNUSED, ARG_UNUSED )>>5!=0x04?"faile":"okay");
	// Reset controller
	CAN_SPI_CMD( SPI_CMD_RESET, ARG_UNUSED, ARG_UNUSED, ARG_UNUSED );
	

	CAN_SPI_CMD( SPI_CMD_BITMOD, TOLONG(&(MCP2510_MAP->CANCTRL)), 0xe0, 0x80 );		// CONFIG MODE

	// make sure we are in configuration mode
	while( (CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->CANSTAT)), ARG_UNUSED, ARG_UNUSED )>>5)!=0x04 );
//  printk("look if in configure mode:%s\n",CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->CANSTAT)), ARG_UNUSED, ARG_UNUSED )>>5!=0x04?"faile":"okay");
//  printk("hmmmmmmmmmmm!MCAP2510 NOW IN CONFIGURE MODE!\n");
	// start configuration
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->BFPCTRL)), 	BFPCTRL_INIT_VAL, 	ARG_UNUSED);
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->TXRTSCTRL)), 	TXRTSCTRL_INIT_VAL, ARG_UNUSED);
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->CNF3)), 		CNF3_INIT_VAL, 		ARG_UNUSED);
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->CNF2)), 		CNF2_INIT_VAL, 		ARG_UNUSED);
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->CNF1)),		CNF1_INIT_VAL, 		ARG_UNUSED);
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->CANINTE)),	CANINTE_INIT_VAL, 	ARG_UNUSED);
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->CANINTF)), 	CANINTF_INIT_VAL, 	ARG_UNUSED);
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->EFLG)), 		EFLG_INIT_VAL, 		ARG_UNUSED);
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->TXB0CTRL)), 	TXBnCTRL_INIT_VAL, 	ARG_UNUSED);
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->TXB1CTRL)), 	TXBnCTRL_INIT_VAL, 	ARG_UNUSED);
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->TXB2CTRL)), 	TXBnCTRL_INIT_VAL, 	ARG_UNUSED);
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->RXB0CTRL)), 	RXB1CTRL_INIT_VAL, 	ARG_UNUSED);
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->RXB1CTRL)), 	RXB1CTRL_INIT_VAL, 	ARG_UNUSED);
	
	// switch to normal mode or loopback mode ( for testing)
	CAN_SPI_CMD( SPI_CMD_BITMOD, TOLONG(&(MCP2510_MAP->CANCTRL)), 0xe0, 0x40 );		// LOOP BACK MODE
	//CAN_SPI_CMD( SPI_CMD_BITMOD, TOLONG(&(MCP2510_MAP->CANCTRL)), 0xe0, 0x00 );		// NORMAL MODE
	
	//printk("look if in configure mode:%s\n\
//failed is right,because now in loop back mode!\n",CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->CANSTAT)), ARG_UNUSED, ARG_UNUSED )>>5!=0x04?"faile":"okay");
	//fla mask for a while!CAN_SPI_CMD( SPI_CMD_BITMOD, TOLONG(&(MCP2510_MAP->CANCTRL)), 0xe0, 0x00 );	// NORMAL OPERATION MODE
	
	// Flush the MX1 SPI receive buffer
	
	CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->CANSTAT)), ARG_UNUSED, ARG_UNUSED );
}




/* Transmit data */
/*
 TxBuf	: select the transmit buffer( 0=buffer0 or 1=buffer1 2=buffer2 )
 IdType	: 0=standard id or 1=extended id
 id	: frame identifier
 DataLen	: the number of byte
 data	: the pointer to data byte
*/
void MCP2510_TX( int TxBuf, int IdType, unsigned int id, int DataLen, char * data )
{
	int i, offset;
  //printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);
	switch( TxBuf ){
		case TXBUF0:
			offset = 0;
			break;
		case TXBUF1:
			offset = 0x10;
			break;
		case TXBUF2:
			offset = 0x20;
			break;
	}
	//printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);
	// Set the frame identifier
	if( IdType==STANDID ){
		//printk(KERN_ERR "STANDID,line %d passed!\n",__LINE__);
		CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->TXB0SIDL))+offset, (id&0x7)<<5, ARG_UNUSED );
		CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->TXB0SIDH))+offset, (id>>3)&0xff, ARG_UNUSED );
	}else if( IdType==EXTID ){
		//printk(KERN_ERR "EXTID,line %d passed!\n",__LINE__);
		CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->TXB0EID0))+offset, id&0xff, ARG_UNUSED );
		CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->TXB0EID8))+offset, (id>>8)&0xff, ARG_UNUSED );			
		CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->TXB0SIDL)), ((id>>16)&0x3)|0x08, ARG_UNUSED );
	}
	// Set the data length
	CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->TXB0DLC))+offset, DataLen, ARG_UNUSED );
	// fill the data
	if( DataLen>8 )
		DataLen = 8;
	for( i=0; i<DataLen; i++ ){
		CAN_SPI_CMD( SPI_CMD_WRITE, TOLONG(&(MCP2510_MAP->TXB0D0))+offset+i, data[i], ARG_UNUSED );
	}

	// initiate transmit
	i = 0;
	while( CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->TXB0CTRL))+offset, ARG_UNUSED, ARG_UNUSED )&0x08 ){
		i++;
		if(i == 1000)printk("Please connect the CAN PORT with wire.");
	}
	//printk("HELLO,jupm over error!!\n");
	CAN_SPI_CMD( SPI_CMD_BITMOD, TOLONG(&(MCP2510_MAP->TXB0CTRL))+offset, 0x08, 0x08 );
}

/* Receive data */
/*
 * RxBuf		: The receive buffer from which the data is get
 * IdType		: Identifier type of the data frame ( STANDID, EXTID )
 * id		: The identifier of the received frame
 * DataLen	: The number of bytes received
 * data		: The received data
 */
void MCP2510_RX( int RxBuf, int *IdType, unsigned int *id, int *DataLen, char *data )
{
	unsigned int flag;
	int offset, i;
	//printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);
	switch( RxBuf ){
		case RXBUF0:
			flag = 0x1;
			offset = 0x00;
			break;
		case RXBUF1:
			flag = 0x2;
			offset = 0x10;
			break;
	}
	// wait for a frame to com
	while( !(CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->CANINTF)), ARG_UNUSED, ARG_UNUSED )&flag) );

	// Get the identifier
	if( CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->RXB0SIDL))+offset, ARG_UNUSED, ARG_UNUSED )&0x08 ){
		// Extended identifier
		if( IdType )
			*IdType = EXTID;
		if( id ){
			*id = (CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->RXB0SIDL))+offset, ARG_UNUSED, ARG_UNUSED )&0x3)<<16;
			*id |= (CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->RXB0EID8))+offset, ARG_UNUSED, ARG_UNUSED ))<<8;
			*id |= (CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->RXB0EID0))+offset, ARG_UNUSED, ARG_UNUSED ));
		}
	}else{
		// Standard identifier
		if( IdType )
			*IdType = STANDID;
		if( id ){
			*id = (CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->RXB0SIDH))+offset, ARG_UNUSED, ARG_UNUSED ))<<3;
			*id |= (CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->RXB0SIDL))+offset, ARG_UNUSED, ARG_UNUSED ))>>5;
		}
	}
	// Get the data frame lenth
	if( DataLen )
		*DataLen = (CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->RXB0DLC))+offset, ARG_UNUSED, ARG_UNUSED )&0xf);
	// Get the data
	for( i=0; DataLen&&(i<*DataLen)&&data; i++ ){
		data[i] = CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->RXB0D0))+offset+i, ARG_UNUSED, ARG_UNUSED );
	}
	// clear the receive int flag
	CAN_SPI_CMD( SPI_CMD_BITMOD, TOLONG(&(MCP2510_MAP->CANINTF)), flag, 0x00 );
	
}


/*
 * Atomicly increment an index into shortp_in_buffer
 */
static inline void incr_buffer_pointer(volatile unsigned long*index, int delta, int i)
{
	unsigned long newvalue = *index + delta;
	//printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);
	barrier ();  /* Don't optimize these two together */
	*index = (newvalue >= (Device[i]->RecvBuf + BUF_SIZE)) ? Device[i]->RecvBuf : newvalue;
}

//add irqreturn_t fla
static irqreturn_t mcpcan0_handler(int irq, void *dev_id, struct pt_regs *reg)
{
	struct MCP_device *dev = dev_id;
	struct mcpcan_data datagram;
	int written;
	char regvalue, regvalue2;
	//struct mcpcan_data *datagramptr;
  //printk(KERN_ERR "hello,finally enter %s!!!!!!!!!!!!!!!!!!!!!!!!\n",__FUNCTION__);
	//printk(KERN_ERR "\rextern irq 8 handled\n");

	regvalue = CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->CANINTE)), ARG_UNUSED, ARG_UNUSED );
	//printk("CANINTE = 0x%02x\n",regvalue);

	regvalue = CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->CANINTF)), ARG_UNUSED, ARG_UNUSED );
	//printk("CANINTF = 0x%02x\n",regvalue);

	if(regvalue & 0x01){
		datagram.BufNo = RXBUF0;
		MCP2510_RX(datagram.BufNo, &(datagram.IdType), &(datagram.id), &(datagram.DataLen), datagram.data );
		//printk("RXBUF0\n");

        //printk("datagram.BufNo = %d\n",datagram.BufNo);
        //printk("datagram.IdType = %d\n",datagram.IdType);
        //printk("datagram.id = %d\n",datagram.id);
        //printk("datagram.DataLen = %d\n",datagram.DataLen);
        //printk("datagram.data = %s\n",datagram.data);

	    /* Write a 16 byte record. Assume BUF_SIZE is a multiple of 16 */
		memcpy((void *)dev->RecvHead, &datagram, sizeof(struct mcpcan_data));
		//datagramptr = (struct mcpcan_data *)dev->RecvHead;
		//printk("RecvHead->BufNo = %d\n",datagramptr->BufNo);
		//printk("RecvHead->IdType = %d\n",datagramptr->IdType);
		//printk("RecvHead->id = %d\n",datagramptr->id);
		//printk("RecvHead->DataLen = %d\n",datagramptr->DataLen);
		//printk("RecvHead->data = %s\n",datagramptr->data);


		incr_buffer_pointer(&(dev->RecvHead), sizeof(struct mcpcan_data), dev->MinorNum);
		wake_up_interruptible(&(dev->inq)); /* awake any reading process */
		CAN_SPI_CMD( SPI_CMD_BITMOD, TOLONG(&(MCP2510_MAP->CANINTF)), 0x01, 0x00 );     //in fact already clear interrupt bit in RX 
	}

	if(regvalue & 0x02){
		datagram.BufNo = RXBUF1;

		//this function should be rewritten to receive datagram from RXB1??????????????????
		MCP2510_RX(datagram.BufNo, &(datagram.IdType), &(datagram.id), &(datagram.DataLen), datagram.data );
		//printk("RXBUF1\n");


	    /* Write a 16 byte record. Assume BUF_SIZE is a multiple of 16 */
		memcpy((void *)dev->RecvHead, &datagram, sizeof(struct mcpcan_data));
		

		incr_buffer_pointer(&(dev->RecvHead), sizeof(struct mcpcan_data), dev->MinorNum);
		wake_up_interruptible(&dev->inq); /* awake any reading process */
		CAN_SPI_CMD( SPI_CMD_BITMOD, TOLONG(&(MCP2510_MAP->CANINTF)), 0x02, 0x00 );     //in fact already clear interrupt bit in RX
	}

	if(regvalue & 0xe0){
		printk("CAN error with CANINTF = 0x%02x.\n", regvalue);
		if(regvalue & 0x80)printk("MERRF:message error.\n");
		if(regvalue & 0x40)printk("WAKIF:wake up interrupt.\n");
		if(regvalue & 0x20)printk("ERRIF:CAN bus error interrupt.\n");
		
		regvalue2 = CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->EFLG)), ARG_UNUSED, ARG_UNUSED );
		printk("EFLG = 0x%02x.\n", regvalue2);
                regvalue2 = CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->TEC)), ARG_UNUSED, ARG_UNUSED );
                printk("TEC = 0x%02x.\n", regvalue2);
                regvalue2 = CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->REC)), ARG_UNUSED, ARG_UNUSED );
                printk("REC = 0x%02x.\n", regvalue2);

		CAN_SPI_CMD( SPI_CMD_BITMOD, TOLONG(&(MCP2510_MAP->CANINTF)), 0xe0, 0x00 );
	}

	if(CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->CANINTF)), ARG_UNUSED, ARG_UNUSED ) & 0x1c){  //didn't open the 3 send interrupts
		CAN_SPI_CMD( SPI_CMD_BITMOD, TOLONG(&(MCP2510_MAP->CANINTF)), 0x1c, 0x00 ); //clear TX bits directly(it will set 1,but no int)
	}

        regvalue = CAN_SPI_CMD( SPI_CMD_READ, TOLONG(&(MCP2510_MAP->CANINTF)), ARG_UNUSED, ARG_UNUSED );
	//printk("after interrupt processing, CANINTF = 0x%02x\n",regvalue);
	
	return IRQ_HANDLED; //fla
}

static void mcpcan1_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	//printk("\rextern irq ? handled");



}

/*****************************MCPCAN open**********************************/
static int device_open(struct inode *inode,struct file *filp)
{
    int MinorNum;
	int eint_irq;
	int ret_val;
    struct MCP_device *dev;
	static int irqcount0=0, irqcount1=0;
	
	//printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);

   // MinorNum = MINOR(inode->i_rdev);
    MinorNum = iminor(inode);
    
    if(MinorNum >= MCP_NUM) return(-ENODEV);
        
    dev = Device[MinorNum];
    filp->private_data = dev;        //save to private_data

	
	//request_region(0x59000000,0x38,"MCPCAN");    //SPI interface
	request_mem_region(0x59000000,0x38,"MCPCAN");
	
	//request_region(0x56000000,0x94,"MCPCAN");  //I/O port
	request_mem_region(0x56000000,0x94,"MCPCAN");
	
	//request_region(0x4c00000c,4,"MCPCAN");  //clock
   request_mem_region(0x4c00000c,4,"MCPCAN");

 	/* register the eint5 irq */
	if(irqcount0 == 0){
		//eint_irq = IRQ_EINT5;
		eint_irq = IRQ_EINT11;
		
		//set_external_irq(eint_irq, EXT_FALLING_EDGE, GPIO_PULLUP_DIS);
//now set extint for eint8 instead of the up line!

s3c2410_gpio_cfgpin(S3C2410_GPG3, S3C2410_GPG3_EINT11);
writel(readl(S3C2410_INTMSK)& (~(1<<5)),S3C2410_INTMSK);
writel(readl(eintmask)& (~(1<<11)),eintmask);
writel(readl(S3C2410_EXTINT1)|(0x00007000)&(0xffffbfff),S3C2410_EXTINT1);
//s3c2410_gpio_pullup(S3C2410_GPG3,0); //dis

ret_val = request_irq(eint_irq, mcpcan0_handler, SA_INTERRUPT | SA_SHIRQ, MCP_name[dev->MinorNum], dev);
		if (ret_val < 0) {
			return ret_val;
		}
		irqcount0++;
	}

	MCP2510_Init(MinorNum);
//printk(KERN_ERR "INTMSK = %08x\nEINTMASK = %08x\n\
//CLKCON = %08x\nSPCON0 = %08x\nGPECON = %08x\nGPGCON = %08x\n",readl(S3C2410_INTMSK),readl(eintmask),
//readl(S3C2410_CLKCON),readl(spcon0),readl(rGPECON),readl(rGPGCON));
//	MOD_INC_USE_COUNT;
    return 0;
}

/*****************************Functin MCP_release**************************/
static int device_release(struct inode *inode,struct file *filp)
{
	struct MCP_device *dev = filp->private_data;
  //printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);
	release_region(0x59000000,0x38);
	release_region(0x56000000,0x94);
	release_region(0x4c00000c,4);

	free_irq(dev->IrqNum,dev);    //shared irq

	//MOD_DEC_USE_COUNT;
	return 0;
}

ssize_t device_read (struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	int count0;
	
	struct MCP_device *dev = filp->private_data;
	//printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);
	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	while (dev->RecvHead == dev->RecvTail) { /* nothing to read */
		up(&dev->sem); /* release the lock */
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(dev->inq, (dev->RecvHead != dev->RecvTail)))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		/* otherwise loop, but first reacquire the lock */
		if (down_interruptible(&dev->sem))
			return -ERESTARTSYS;
	}
	/* ok, data is there, return something */
	/* count0 is the number of readable data bytes */
	count0 = dev->RecvHead - dev->RecvTail;
	if (count0 < 0) /* wrapped */
		count0 = dev->RecvBuf + BUF_SIZE - dev->RecvTail;
	if (count0 < count) count = count0;
	if (copy_to_user(buf, (char *)dev->RecvTail, count)){
		up (&dev->sem);
		return -EFAULT;
	}
	incr_buffer_pointer(&(dev->RecvTail), count, dev->MinorNum);
	up (&dev->sem);

	return count;
}

ssize_t device_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	mcpcan_data     temp;
	//printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);
	struct MCP_device *dev = filp->private_data;
	if(down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	copy_from_user(&temp, buf, sizeof(struct mcpcan_data));
//  printk(KERN_ERR "hello,TX data in kernel is %s\n",temp.data);
	MCP2510_TX(temp.BufNo,temp.IdType,temp.id,temp.DataLen,temp.data );
	up(&dev->sem);

	return count;
}





/* This function is called whenever a process tries to 
 * do an ioctl on our device file. We get two extra 
 * parameters (additional to the inode and file 
 * structures, which all device functions get): the number
 * of the ioctl called and the parameter given to the 
 * ioctl function.
 *
 * If the ioctl is write or read/write (meaning output 
 * is returned to the calling process), the ioctl call 
 * returns the output of this function.
 */
/*
int device_ioctl(
    struct inode *inode,
    struct file *file,
    unsigned int ioctl_num,// The number of the ioctl 
    unsigned long ioctl_param) // The parameter to it 
{
}
*/



/*****************************File operations define************************/
struct file_operations Fops = {
    .open = device_open,    //open
    .release = device_release,    //release
	.read = device_read,
	.write = device_write
};

/****************************Module initialize******************************/
static int __init mcpcan_init_module(void)
{
        int res;

        //EXPORT_NO_SYMBOLS;
//       printk(KERN_ERR "hello,enter mcpcan_init_module!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        /* Register the character device (atleast try) */
        res = register_chrdev(MCP_major,"MCPCAN",&Fops);
        if(res < 0) {
               printk("device register failed with %d.\n",res);
               return res;
        }

        if(MCP_major == 0) MCP_major = res;


	    MCP_device_init();
   
    //intmsk = ioremap(0x4A000008,4);
    eintmask = ioremap(0x560000A4,4);
   // extint0 = ioremap(0x56000088,4);
    //extint1 = ioremap(0x5600008c,4);
    //extint2 = ioremap(0x56000090,4);
    
    //map spi regs for spi.c
    spcon0 = ioremap(0x59000000,4);
    spsta0 = ioremap(0x59000004,4);
    sppin0 = ioremap(0x59000008,4);
    sppre0 = ioremap(0x5900000c,4);
    sptdat0 = ioremap(0x59000010,4);
    sprdat0 = ioremap(0x59000014,4);
    
    spcon1 = ioremap(0x59000020,4);
    spsta1 = ioremap(0x59000024,4);
    sppin1 = ioremap(0x59000028,4);
    sppre1 = ioremap(0x5900002c,4);
    sptdat1 = ioremap(0x59000030,4);
    sprdat1 = ioremap(0x59000034,4);
    
    //printk(KERN_ERR "in mcpcan.c:the maped addr of sptdat = %#p",sptdat0);
		//printk ("%s The major device number is %d.\n",
			//	"Registeration is a success", 
			//	MCP_major);
		          
		printk (KERN_ERR "hello,you have insert module that support MCP2510,to make\n");
		printk (KERN_ERR "use of it, you must use: mknod /dev/%s c %d 0,and you can\n","can",MCP_major);
		printk (KERN_ERR "run test demo named 2510test for a test in loop back mode\n");
    printk (KERN_ERR "you'll modify the driver to make it fall into normal mode\n");

        return 0;
}

/****************************Module release********************************/
static void __exit mcpcan_cleanup_module(void)
{
	int i;
    unregister_chrdev(MCP_major,"MCPCAN");
	for(i=0; i<2; i++){
		kfree((void *)Device[i]->RecvBuf);
		kfree(Device[i]->SendBuf);
		kfree(Device[i]);
	}
    printk("MCPCAN release success.\n");
}

module_init(mcpcan_init_module);
module_exit(mcpcan_cleanup_module);

MODULE_AUTHOR("witech <bdht@witech.com.cn>");
MODULE_DESCRIPTION("MCP251X driver");
MODULE_LICENSE("Dual BSD/GPL");