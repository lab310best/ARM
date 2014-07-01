#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
//#include <linux/i2c.h>
//#include <linux/i2c-dev.h>
//#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <linux/devfs_fs_kernel.h>
#include <asm-arm/io.h>
#include <asm/system.h>
#include "ioctl.h"
//----------------------------------- porting can test route from ads-------------------------------
/****************************************************************************
MCP2510_CS		GPG2		output		( nSS0 )
MCP2510_SI		GPE12		output		( SPIMOSI0 )
MCP2510_SO		GPE11		input		( SPIMISO0 )
MCP2510_SCK		GPE13		output		( SPICLK0 )
MCP2510_INT		GPG0		input		( EINT8 )
****************************************************************************/
typedef u16 U16;
typedef u8 U8;
typedef u32 U32;

#define rGPECON    (0x56000040)	//Port E control
#define rGPEDAT    (0x56000044)	//Port E data
#define rGPEUP     (0x56000048)	//Pull-up control 

#define rGPGCON    (0x56000060)	//Port G control
#define rGPGDAT    (0x56000064)	//Port G data
#define rGPGUP     (0x56000068)	//Pull-up control 

static unsigned long gpecon,gpeup,gpgcon,SPI_SPCON0,SPI_SPSTA0,SPI_SPPIN0,
                     SPI_SPPRE0;
volatile unsigned long gpedat,gpgdat,SPI_SPTDAT0,SPI_SPRDAT0;


#define MCP2510_DEBUG    1
#define DELAY_TIME		500

//#define MCP2510_CS_OUT		( rGPGCON = rGPGCON & (~(3<<4)) | (1<<4) )		//GPG2     output
#define MCP2510_CS_OUT    writel(readl(gpgcon)& (~(3<<4)) | (1<<4) ,gpgcon); mb()


//#define MCP2510_CS_H		( rGPGDAT = rGPGDAT | (1<<2) )
#define MCP2510_CS_H      writel(readl(gpgdat)| (1<<2),gpgdat); mb()

//#define MCP2510_CS_L		( rGPGDAT = rGPGDAT & (~(1<<2))  )
#define MCP2510_CS_L      writel(readl(gpgdat)& (~(1<<2)),gpgdat); mb()

//#define MCP2510_SI_OUT		( rGPECON = rGPECON & (~(3<<24)) | (1<<24) )		//GPE12  output
#define MCP2510_SI_OUT    writel(readl(gpecon) & (~(3<<24)) | (1<<24),gpecon); mb()

//#define MCP2510_SI_H		( rGPEDAT = rGPEDAT | (1<<12) )
#define MCP2510_SI_H      {mb();writel(readl(gpedat) | (1<<12),gpedat); mb();}

//#define MCP2510_SI_L		( rGPEDAT = rGPEDAT & (~(1<<12)) )
#define MCP2510_SI_L      {mb();writel(readl(gpedat)& (~(1<<12)) ,gpedat); mb();}

//#define MCP2510_SCK_OUT		( rGPECON = rGPECON & (~(3<<26)) | (1<<26) )		//GPE13    ouput
#define MCP2510_SCK_OUT   writel(readl(gpecon)& (~(3<<26)) | (1<<26),gpecon); mb()

//#define MCP2510_SCK_H		( rGPEDAT = rGPEDAT | (1<<13) )
#define MCP2510_SCK_H     writel(readl(gpedat)| (1<<13) ,gpedat); mb()

//#define MCP2510_SCK_L		( rGPEDAT = rGPEDAT & (~(1<<13)) )
#define MCP2510_SCK_L     writel(readl(gpedat)& (~(1<<13)),gpedat); mb()

//#define MCP2510_SO_IN		( rGPECON = rGPECON & (~(3<<22)) | (0<<22) )		//GPE11   input
#define MCP2510_SO_IN     writel(readl(gpecon)& (~(3<<22)) | (0<<22) ,gpecon); mb()

//#define MCP2510_SO_GET		( rGPEDAT & (1<<11) )
#define MCP2510_SO_GET    readl(gpedat)& (1<<11)

//#define MCP2510_SO_PULLUP		( rGPEUP = rGPEUP & (~(1<<11)) )
#define MCP2510_SO_PULLUP writel(readl(gpeup)& (~(1<<11)) ,gpeup); mb()

//#define MCP2510_SO_DISPULLUP		( rGPEUP = rGPEUP | (1<<11) )
#define MCP2510_SO_DISPULLUP writel(readl(gpeup) | (1<<11),gpeup); mb()

//#define MCP2510_INT_IN		( rGPGCON = rGPGCON & (~(3<<0)) )		//GPG0
#define MCP2510_INT_IN    writel(readl(gpgcon) & (~(3<<0)),gpgcon); mb()

//#define MCP2510_INT_GET		( rGPGDAT & 0x01 )
#define MCP2510_INT_GET   readl(gpgdat)& 0x01

/********************** MCP2510 Instruction *********************************/
#define MCP2510INSTR_RESET		0xc0		//复位为缺省状态，并设定为配置模式
#define MCP2510INSTR_READ		0x03		//从寄存器中读出数据
#define MCP2510INSTR_WRITE		0x02		//向寄存器写入数据
#define MCP2510INSTR_RTS		0x80		//启动一个或多个发送缓冲器的报文发送
#define MCP2510INSTR_RDSTAT		0xa0		//读取状态
#define MCP2510INSTR_BITMDFY	0x05		//位修改
//***************************************************************************
#include <linux/types.h>

void MCP2510_RW_Start( void ) 
{
   MCP2510_SI_L		//SI put 0
   MCP2510_SCK_L ;		//SCK put 0
   { U16 k=0; for( ; k <= DELAY_TIME; k++ ) ;  }  //延时至少300ns
   MCP2510_CS_L ;			// Select the MCP2510
   { U16 k=0; for( ; k <= DELAY_TIME; k++ ) ;  }  //延时至少300ns
}
//-----------------------------------for porting ads can test end-------------------------------

//MODULE_LICENSE("GPL");
MODULE_LICENSE("GPL");

#define GPIO_MINOR 1  /*定义从设备号*/

//int test_major = 0;

static int gpioMajor = 0;

static int gpio_open(struct inode *inode, struct file *file);               //函数声明

static int gpio_release(struct inode *inode, struct file *file);

static int gpio_ioctl(struct inode *inode, struct file *file,unsigned int cmd, unsigned long arg);

static ssize_t gpio_read(struct file *file, char __user *buf, size_t count,loff_t *offset);

static ssize_t gpio_write(struct file *file, char __user *buf, size_t count,loff_t *offset);

u8 Spi_Read();///u8-->int

void Spi_Write( u8 Data );

static struct file_operations gpio_ctl_fops = {
	.owner		= THIS_MODULE,
	.read		  = gpio_read,
	.write		= gpio_write,
	.ioctl		= gpio_ioctl,
	.open		  = gpio_open,
	.release	= gpio_release,
};

int  _init_module(void) 
{
	printk(KERN_ERR "enter gpio_init!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	int ret=0;
        printk(KERN_ERR "gpio_init\n");
	ret = register_chrdev(0, "gpio", &gpio_ctl_fops); //IOPORT_MAJOR-->0
	if (ret < 0) {
	  printk(KERN_ERR " can't get gpio major number\n");
	  return ret;
	}
	printk(KERN_ERR "success to get gpio major number!!!!!!\n");
	gpioMajor = ret;
	
	gpecon = ioremap(rGPECON,0x00000004);
	gpedat = ioremap(rGPEDAT,0x00000004);
	gpeup = ioremap(rGPEUP,0x00000004);
	gpgcon = ioremap(rGPGCON,0x00000004);
	gpgdat = ioremap(rGPGDAT,0x00000004);
	
	SPI_SPCON0 =ioremap(0x59000000,4);
  SPI_SPSTA0 =ioremap(0x59000004,4);
  SPI_SPPIN0 =ioremap(0x59000008,4);
  SPI_SPPRE0 =ioremap(0x5900000c,4);
  SPI_SPTDAT0 =ioremap(0x59000010,4);
  SPI_SPRDAT0 =ioremap(0x59000014,4);
	
#ifdef CONFIG_DEVFS_FS
	devfs_mk_dir("gpio");
  devfs_mk_cdev(MKDEV(gpioMajor,GPIO_MINOR), S_IFCHR|S_IRUGO|S_IWUSR, "gpio/%d", 0);
#endif

//add spi init
//spi_gpecon &=0xf03fffff;//GPECON-11,12,13=10 spi mode
//spi_gpecon |=0x0a800000;
writel((readl(gpecon)&0xf03fffff|0x0a800000),gpecon);

//spi_gpeup |=0x2000;
writel((readl(gpeup)&(~0x3800)|0x2000),gpeup);//GPEUP-11,12=0;GPEUP-13=1

MCP2510_CS_OUT;  //instead to set gpg2 output,not nss!

//init further more
writel(0x0,SPI_SPPRE0);//SPI Baud Rate Prescaler Register,Baud Rate=PCLK/2/(Prescaler value+1)
writel((0<<5)|(1<<4)|(1<<3)|(1<<2)|(0<<1)|(0<<0),SPI_SPCON0);//polling,en-sck,master,low,format A,nomal
writel((0<<2)|(1<<1)|(0<<0),SPI_SPPIN0);//Multi Master error detect disable,reserved,release
return 0;
}

static int gpio_open(struct inode *inode, struct file *file)
{
	printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);
	return 0;
}

static int gpio_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int gpio_ioctl(struct inode *inode, struct file *file,unsigned int cmd, unsigned long arg)
{
	//printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);
	switch ( cmd ) {
	case CS_OUT:
		   MCP2510_CS_OUT;
		   printk(KERN_ERR "cs_out!\n");
		   return 0;
		   
	case CS_H:
		   MCP2510_CS_H;
		   printk(KERN_ERR "cs_h!\n");
		   	return 0;
		   	
  case CS_L:
  	   MCP2510_CS_L;
  	   printk(KERN_ERR "cs_l!\n");
  	   return 0;
  	   
  case SI_OUT:
  	   MCP2510_SI_OUT;
  	   printk(KERN_ERR "si_out!\n");
  	   return 0;
  	   
  case SI_H:
  	   MCP2510_SI_H
  	   printk(KERN_ERR "si_h!\n");
  	   return 0;
  	   
  case SI_L:
  	   MCP2510_SI_L
  	   printk(KERN_ERR "si_l!\n");
       return 0;
       
  case SCK_OUT:
  	   MCP2510_SCK_OUT;
  	   printk(KERN_ERR "sck_out!\n");
  	   return 0;
  	   
  case SCK_H:
  	   MCP2510_SCK_H;
  	   printk(KERN_ERR "sck_h!\n");
  	   return 0;
  	   
  case SCK_L:
  	   MCP2510_SCK_L;
  	   printk(KERN_ERR "sck_l!\n");
  	   return 0;
  	   
  case SO_IN:
  	   MCP2510_SO_IN;
  	   printk(KERN_ERR "so_in!\n");
  	   return 0;
  	   
  case SO_GET:
  	   MCP2510_SO_GET;
  	   printk(KERN_ERR "so_get!\n");
  	   return 0;
  	   
  case SO_PULLUP:
  	   MCP2510_SO_PULLUP;
  	   printk(KERN_ERR "so_pullup!\n");
  	   return 0;
  	   
  case SO_DISPULLUP:
  	   MCP2510_SO_DISPULLUP;
  	   printk(KERN_ERR "so_dispullup!\n");
       return 0;
       
  case INT_IN:
       MCP2510_INT_IN;
       printk(KERN_ERR "int_in!\n");
       return 0;
       
  case INT_GET:
  	   MCP2510_INT_GET;
  	   printk(KERN_ERR "int_get!\n");
		   return 0;
  }
}

static ssize_t gpio_read(struct file *file, char __user *buf, size_t count,loff_t *offset)
{
	u8 tmp;  //char *   --->  u8 *
	int ret;
	//u8 m ;
//	u8 data = 0 ;
//	size_t i;
	//printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);
	//tmp = kmalloc(count,GFP_ATOMIC);
	//if (tmp==NULL)
	//	return -ENOMEM;
		


	//MCP2510_RW_Start() ;
//for (i=0; i<count; i++)
//	{
		//*tmp = Spi_Read();
		if((__raw_readb(SPI_SPSTA0)&0x01)==1)
	     tmp = __raw_readb(SPI_SPRDAT0);
	  else printk(KERN_ERR "ERROR! need read but hw isn't ready!\n");
		//if( MCP2510_DEBUG )    Uart_Printf( "  0x%x\n", (unsigned char)*pdata ) ;
	//	tmp++;
//	}
	//MCP2510_CS_H ;

  ret = copy_to_user(buf,&tmp,1);
	//kfree(tmp);
	return ret;
}

static ssize_t gpio_write(struct file *file, char __user *buf, size_t count,loff_t *offset)
{
	
	u8 tmp;//char * --> u8 *
	//printk(KERN_ERR "hello,enter %s\n",__FUNCTION__);
//	tmp = kmalloc(count,GFP_ATOMIC);
	//if (tmp==NULL)
	//	return -ENOMEM;
	copy_from_user(&tmp,buf,1);
//---------------my

	//Spi_Write( (unsigned char)*tmp );  //pdata --->tmp
	if((__raw_readb(SPI_SPSTA0)&0x01)==1)
		__raw_writeb(tmp,SPI_SPTDAT0);
	else printk(KERN_ERR "ERROR! need write but hw isn't ready!\n");
	return 0;
}	
		


void _cleanup_module(void)
{
    unregister_chrdev(gpioMajor, "gpio"); 
}



/****************************************************************************
【功能说明】SPI接口写入数据
****************************************************************************/
void Spi_Write( u8 Data ) 
{
	u8 m ;
	u8 temp;
  //printk(KERN_ERR "enter %s\n\
  //first reset cmddata = %08x\n",__FUNCTION__,Data);
	for( m = 0; m < 8; m++ )
	{
		if( (Data&0x80)==0x80 )
			MCP2510_SI_H		//SI put 1
		else
			MCP2510_SI_L		//SI put 0

		{ u16 k=0; 
		  for( ; k <= DELAY_TIME; k++ );
		 // temp = 1 ;
		  
	  }  //延时至少300ns
	  
		MCP2510_SCK_H ;		//SCK put 1
		Data = Data<<1 ;
		MCP2510_SCK_L ;		//SCK put 0
		{ u16 k=0; for( ; k <= DELAY_TIME; k++ ) ;  }  //延时至少300ns
	}
//	printk(KERN_ERR "have send the first reset cmd!\n");
}


u8 Spi_Read()
{
	u8 m ;
	u8 data = 0 ;
 // printk(KERN_ERR "enter %s\n",__FUNCTION__);
	for( m = 0; m < 8; m++ )
	{
		MCP2510_SCK_H ;		//SCK put 1
	
		{ u16 k=0; for( ; k <= DELAY_TIME; k++ ) ;  }  //延时至少300ns
		data = data<<1;
		if( MCP2510_SO_GET != 0 )
			data |= 0x01 ;
		else
			data &= 0xfe;

		{ u16 k=0; for( ; k <= DELAY_TIME; k++ ) ;  }  //延时至少300ns
		MCP2510_SCK_L ;		//SCK put 0
		{ u16 k=0; for( ; k <= DELAY_TIME; k++ ) ;  }  //延时至少300ns
	}
//printk(KERN_ERR "for check nicety,read data = %08x\n",data);
return (data);
}

/****************************************************************************
【功能说明】SPI接口IO片选初始化
****************************************************************************/
void MCP2510_IO_CS_Init( void ) 
{
   MCP2510_CS_OUT ;  //GPE12  output
   MCP2510_SI_OUT ;  //GPE13    ouput
   MCP2510_SCK_OUT ;  //GPE11   input
   MCP2510_SO_IN ;
   MCP2510_SO_PULLUP ;		//允许上拉
   //MCP2510_SO_DISPULLUP ;		//禁止上拉

   MCP2510_SI_L		//SI put 0  SI ON MCP2510 --fla
   MCP2510_SCK_L ;		//SCK put 0
   { U16 k=0; for( ; k <= DELAY_TIME; k++ ) ;  }  //延时至少300ns
   MCP2510_CS_H ;			// unselect the MCP2510
   { U16 k=0; for( ; k <= DELAY_TIME; k++ ) ;  }  //延时至少300ns
}

module_init(_init_module);           
module_exit(_cleanup_module); 

  
