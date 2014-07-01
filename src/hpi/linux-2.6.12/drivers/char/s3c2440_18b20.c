#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#define DEVICE_NAME	"18b20"
#define tp_MAJOR  232

unsigned char sdata;//测量到的温度的整数部分
unsigned char xiaoshu1;//小数第一位
unsigned char xiaoshu2;//小数第二位
unsigned char xiaoshu;//两位小数


void tmreset (void)       //18b20初始化
{      
       s3c2410_gpio_cfgpin(S3C2410_GPG0, S3C2410_GPG0_OUTP);
	s3c2410_gpio_setpin(S3C2410_GPG0, 1);
       udelay(100);

	s3c2410_gpio_setpin(S3C2410_GPG0, 0);   
       udelay(600);
   	 
	s3c2410_gpio_setpin(S3C2410_GPG0, 1);
       udelay(100);

	s3c2410_gpio_cfgpin(S3C2410_GPG0, S3C2410_GPG0_INP);     //设置寄存器对18b20进行读操作
	
	//if(s3c2410_gpio_getpin(S3C2410_GPG0)==0)
		//printk("18b20 initialization is sucessful\n");
	 
}  

void tmwbyte (unsigned char dat)     //写一个字节函数
{                       
        unsigned char j;
	 s3c2410_gpio_cfgpin(S3C2410_GPG0, S3C2410_GPG0_OUTP);   
	 for (j=1;j<=8;j++)      
	 { 
	   s3c2410_gpio_setpin(S3C2410_GPG0, 0); 
	   udelay(1); 
	   if((dat&0x01)==1)
	   	{
		   s3c2410_gpio_setpin(S3C2410_GPG0, 1);	  	  
	   	}	  
	   else 
	   	{
	   	  // s3c2410_gpio_setpin(S3C2410_GPG0, 0);
	   	}
	   udelay(60);
	   s3c2410_gpio_setpin(S3C2410_GPG0, 1);
	   udelay(15);
	   dat = dat >> 1;
        }  
        s3c2410_gpio_setpin(S3C2410_GPG0, 1);
} 

unsigned char tmrbyte (void)        //读一个字节函数
  {                
	 unsigned char i,u=0;      
	       
	 for (i=1;i<=8;i++)      
	 {
	  
	  s3c2410_gpio_cfgpin(S3C2410_GPG0, S3C2410_GPG0_OUTP); 
	  s3c2410_gpio_setpin(S3C2410_GPG0, 0); 
	  udelay(1);
	   u >>= 1; 
	  s3c2410_gpio_setpin(S3C2410_GPG0, 1);
	  //udelay(12);
	  s3c2410_gpio_cfgpin(S3C2410_GPG0, S3C2410_GPG0_INP); 
	  if( s3c2410_gpio_getpin(S3C2410_GPG0))    u=u|0x80;
	  udelay(60);  
	 }  
	 return (u);       
      
}

void DS18B20PRO(void)         
{    
  unsigned char a,b; 
  tmreset();             //复位 
  udelay(120);
  tmwbyte(0xcc);         //跳过序列号命令   
  tmwbyte(0x44);         //发转换命令 44H,
  udelay(5);
  tmreset ();      //复位 
  udelay(200);
  tmwbyte (0xcc);  //跳过序列号命令   
  tmwbyte (0xbe);  //发送读取命令
  a = tmrbyte ();  //读取低位温度    
  b= tmrbyte ();  //读取高位温度           
	sdata = a/16+b*16;      //整数部分	
	xiaoshu1 = (a&0x0f)*10/16;    //小数第一位
	xiaoshu2 = (b&0x0f)*100/16%10;//小数第二位
	xiaoshu=xiaoshu1*10+xiaoshu2; //小数两位
}	

static ssize_t  s3c2440_18b20_read(struct file *filp, char *buf, size_t len, loff_t *off)

{
   DS18B20PRO();
    *buf=sdata;
	//printk("%d\n",xiaoshu);
	//printk("%d\n",sdata);
	return 1;
}

static struct file_operations s3c2440_18b20_fops = {
	.owner	=	THIS_MODULE,
	.read	=	s3c2440_18b20_read,
};

static int __init s3c2440_18b20_init(void)
{
	int ret;

	ret = register_chrdev(tp_MAJOR, DEVICE_NAME, &s3c2440_18b20_fops);
	if (ret < 0) {
	  printk(DEVICE_NAME " can't register major number\n");
	  return ret;
	}

	devfs_mk_cdev(MKDEV(tp_MAJOR, 0), S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP, DEVICE_NAME);
       //s3c2410_gpio_pullup(S3C2410_GPG0, 0);
	 tmreset();   
	
}

static void __exit s3c2440_18b20_exit(void)
{
	devfs_remove(DEVICE_NAME);
	unregister_chrdev(tp_MAJOR, DEVICE_NAME);
}


module_init(s3c2440_18b20_init);
module_exit(s3c2440_18b20_exit);

