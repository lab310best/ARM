#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/major.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <asm/arch/regs-irq.h>
#include <asm/arch/regs-gpio.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>
#include <asm/arch/irqs.h>
#include <asm/io.h>
#include <asm/semaphore.h> 

#include "hpi.h"

#define HPI_MAJOR 138
#define DRIVER_NAME "HPI"
#define PFX DRIVER_NAME "|"

#define ERR(format, arg...) do { \
printk(KERN_ERR PFX "ERROR|%s:" format "\n", __FUNCTION__ , ## arg); \
} while (0);

#define DBG(format, arg...) do { \
printk(KERN_ERR PFX "DEBUG|%s:" format "\n", __FUNCTION__ , ## arg); \
} while (0);

/* bus width and wait state control */
#define BWSCON		0x48000000
#define BANKCON5	0x48000018

/* hpi base addr */
#define DSP_ADDR_BASE	0x28000000
//#define DSP_ADDR_LEN	0x00000C80//3200
//#define DSP_ADDR_LEN	0x0000C80//6480
#define DSP_ADDR_LEN	0x0000100//256

/* hpi IRQ number */
#define HPI_RD_IRQ		IRQ_EINT4
#define HPI_WR_IRQ		IRQ_EINT5

typedef struct{
	void __iomem* dsp_memory_ptr;
	void __iomem* hpic_write_ptr;
	void __iomem* hpic_read_ptr;
	void __iomem* hpia_write_ptr;
	void __iomem* hpia_read_ptr;
	void __iomem* hpid_autowrite_ptr;
	void __iomem* hpid_autoread_ptr;
	void __iomem* hpid_write_ptr;
	void __iomem* hpid_read_ptr;
	spinlock_t hpi_spinlock;
	spinlock_t rdBufLock;
	spinlock_t wrBufLock;
}hpi_dev_t;

static hpi_dev_t hpi_dev;
static unsigned int g_total_rdIrq = 0;
static unsigned int g_buffull_rdIrq = 0;
static unsigned int g_cledarint_rdIrq = 0;
static unsigned int g_handled_rdIrq = 0;
static unsigned int g_total_wrIrq = 0;
static unsigned int g_buffull_wrIrq = 0;
static unsigned int g_wrong_wrIrq = 0;
static unsigned int g_handled_wrIrq = 0;

static unsigned int g_read_num = 0;
static unsigned int g_write_num = 0;
static int g_int_inited = 0;//
/* virtural address of BWSCON */
static void *bwscon;
/* virtual address of BANKCON5 */
static void *bankcon5;

static DECLARE_WAIT_QUEUE_HEAD(wq_read);

static DECLARE_WAIT_QUEUE_HEAD(wq_write);

#define HPI_PACKET_SIZE	3240
#define HPI_PACKET_INT_LEN (HPI_PACKET_SIZE/4)
static unsigned char rdBuf[HPI_PACKET_SIZE];//dsp -> arm
static unsigned char wrBuf[HPI_PACKET_SIZE];//arm -> dsp

static int g_RdBufSta = BUF_EMPTY;

static int g_WrBufLeft = HPI_PACKET_SIZE;
static unsigned char * wr_p = wrBuf;

static int packet_length = 0;

void msleep(unsigned int msecs);

static inline void hpic_write(u32 u)
{
	outl(u, (u32)hpi_dev.hpic_write_ptr);
}

static inline u32 hpic_read(void)
{
	u32 u;

	u = inl((u32)hpi_dev.hpic_read_ptr);
	return u;
}

static inline void hpia_write(u32 u)
{
	outl(u, (u32)hpi_dev.hpia_write_ptr);
}

static inline u32 hpia_read(void)
{
	u32 u;

	u = inl((u32)hpi_dev.hpia_read_ptr);
	return u;
}

static inline void hpid_autowrite(u32 u)
{
	outl(u, (u32)hpi_dev.hpid_autowrite_ptr);
}

static inline u32 hpid_autoread(void)
{
	u32 u;

	u = inl((u32)hpi_dev.hpid_autoread_ptr);
	return u;
}

static inline void hpid_write(u32 u)
{
	outl(u, (u32)hpi_dev.hpid_write_ptr);
}

static inline u32 hpid_read(void)
{
	u32 u;

	u = inl((u32)hpi_dev.hpid_read_ptr);
	return u;
}

static inline void clear_int(void)
{
	unsigned int val = hpic_read();
	val &= 0xfffbfffb;
	val |= 0x00040004;
	hpic_write(val);
	val = hpic_read();
	g_cledarint_rdIrq++;
}

irqreturn_t hpi_wr_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	u32 val;
	unsigned long hpiFlags;
	unsigned long wrBufFlags;
	u32 * pu = (u32 *)wrBuf;
	static int wrongIntNum = 0;
	int i;

	g_total_wrIrq++;
	if (g_WrBufLeft == HPI_PACKET_SIZE) {
		g_buffull_wrIrq++;
		//DBG("write buffer empty.");
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&hpi_dev.hpi_spinlock, hpiFlags);
	hpia_write(0x22000);
	val = hpid_read();

	if (val != 0x5A5A) {
		g_wrong_wrIrq++;
		//DBG("Wrong write int(%d).", ++wrongIntNum);
		spin_unlock_irqrestore(&hpi_dev.hpi_spinlock, hpiFlags);
		return IRQ_HANDLED;
	}

	s3c2410_gpio_setpin(S3C2410_GPF6, 1);
	spin_lock_irqsave(&hpi_dev.wrBufLock, wrBufFlags);
	if (g_WrBufLeft > 2) {
		*wr_p = 0;
		wr_p++;
		*wr_p = 0;
	}
	hpia_write(0x21000);
	i = 0;
	while(i++ < HPI_PACKET_INT_LEN) {
		hpid_autowrite(*pu);
		pu++;
	}
	hpia_write(0x22000);
	hpid_write(1);
	wr_p = wrBuf;
	g_WrBufLeft = HPI_PACKET_SIZE;
	DBG("packet length to DSP IS (%d) ..." , packet_length);
	packet_length = 0;
	spin_unlock_irqrestore(&hpi_dev.wrBufLock, wrBufFlags);
	s3c2410_gpio_setpin(S3C2410_GPF6, 0);

	spin_unlock_irqrestore(&hpi_dev.hpi_spinlock, hpiFlags);

	wake_up_interruptible(&wq_write);
	g_handled_wrIrq++;

	return IRQ_HANDLED;
}

irqreturn_t hpi_rd_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int i;
	u32 * pu = (u32 *)rdBuf;
	unsigned long flags;

	g_total_rdIrq++;

	if(g_RdBufSta == BUF_FULL) {
		g_buffull_rdIrq++;
		clear_int();
		return IRQ_HANDLED;
	}
	spin_lock_irqsave(&hpi_dev.hpi_spinlock, flags);
	hpia_write(0x20000);
	i = 0;
	while(i++ < HPI_PACKET_INT_LEN) {
		*pu = hpid_autoread();
		pu++;
	}
	clear_int();
	spin_unlock_irqrestore(&hpi_dev.hpi_spinlock, flags);

	g_RdBufSta = BUF_FULL;
	wake_up_interruptible(&wq_read);
	g_handled_rdIrq++;

	return IRQ_HANDLED;
}

static int hpi_open(struct inode *inode, struct file *filp)
{
	int rc;

	if (g_int_inited == 0) {
		s3c2410_gpio_cfgpin(S3C2410_GPF5, S3C2410_GPF5_EINT5);
		set_irq_type(HPI_WR_IRQ, IRQT_BOTHEDGE);
		if((rc = request_irq(HPI_WR_IRQ, hpi_wr_interrupt, 0, "hpi write", NULL)) 	< 0) {
			ERR("unable to request write IRQ %d.", HPI_WR_IRQ);
			return -1;
		}
		
		set_irq_type(HPI_RD_IRQ, IRQT_FALLING);
		if((rc = request_irq(HPI_RD_IRQ, hpi_rd_interrupt, 0, "hpi read", NULL))	< 0) {
			ERR("unable to request read IRQ %d.", HPI_RD_IRQ);
			free_irq(HPI_WR_IRQ, NULL);
			return -1;
		}
		g_int_inited = 1;
	}

	return 0;
}

static int hpi_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t hpi_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int ret;

	if (g_RdBufSta == BUF_EMPTY)
		wait_event_interruptible(wq_read, g_RdBufSta == BUF_FULL);

	if(__copy_to_user(buf, rdBuf, count)) {
		ret = -EFAULT;
	} else {
		ret = count;
	}
	g_RdBufSta = BUF_EMPTY;

	g_read_num++;

	return ret;
}

static ssize_t hpi_write (struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	unsigned long flags;

	if (count > g_WrBufLeft)
		wait_event_interruptible(wq_write, g_WrBufLeft >= count);

	spin_lock_irqsave(&hpi_dev.wrBufLock, flags);
	copy_from_user(wr_p, buf, count);
	wr_p += count;
	g_WrBufLeft -= count;
	packet_length += count;
	spin_unlock_irqrestore(&hpi_dev.wrBufLock, flags);

	g_write_num++;

	return count;
}

static int hpi_ioctl(struct inode *inode, struct file *file,
						unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
		case HPI_CLRINT:
			DBG("CLEAR_INT cmd.");
			clear_int();
			break;

		default:
			ret = -ENOIOCTLCMD;
	}

	return ret;
}

static struct file_operations hpi_fops = {
	.owner		= THIS_MODULE,
	.open		= hpi_open,
	.release	= hpi_release,
	.read 		= hpi_read,
	.write 		= hpi_write,
	.ioctl		= hpi_ioctl,
};

static void reset_dsp(void)
{
	s3c2410_gpio_cfgpin(S3C2410_GPF1, S3C2410_GPF1_OUTP);//no use
	s3c2410_gpio_cfgpin(S3C2410_GPF3, S3C2410_GPF3_OUTP);//reset dsp
	s3c2410_gpio_setpin(S3C2410_GPF1, 0);
	s3c2410_gpio_setpin(S3C2410_GPF3, 1);
	msleep(1);

	s3c2410_gpio_setpin(S3C2410_GPF3, 0);
	msleep(1);
	s3c2410_gpio_setpin(S3C2410_GPF3, 1);
	s3c2410_gpio_setpin(S3C2410_GPF1, 1);

	DBG("DSP RESET.");
}

static int hpi_show(struct seq_file *m, void *v)
{
	seq_printf(m, "HPI interrupts info:\n" "read total:\t%10u\n"
					"read handled:\t%10u\n" "read full:\t%10u\n"
					"read num:\t%10u\n" "write total:\t%10u\n"
					"write handled:\t%10u\n" "write wrong:\t%10u\n"
					"write full:\t%10u\n" "write num:\t%10u\n"
					"clear int:\t%10u\n",
					g_total_rdIrq, g_handled_rdIrq, g_buffull_rdIrq,
					g_read_num, g_total_wrIrq, g_handled_wrIrq,
					g_wrong_wrIrq, g_buffull_wrIrq, g_write_num,
					g_cledarint_rdIrq);

	return 0;
}

static void *hpi_start(struct seq_file *m, loff_t *pos)
{
	return (*pos < 1) ? (void *)1 : NULL;
}

static void *hpi_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void hpi_stop(struct seq_file *m, void *v)
{
}

static struct seq_operations hpi_seq_op = {
	.start	= hpi_start,
	.next	= hpi_next,
	.stop	= hpi_stop,
	.show	= hpi_show
};

static int hpi_drivers_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &hpi_seq_op);
}

static struct file_operations proc_hpi_operations = {
	.open		= hpi_drivers_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static char banner[] __initdata = KERN_INFO "S3C2440 HPI Controller.\n";
static int __init hpi_init(void)
{
	int rc;
	u32 u32r;
	struct proc_dir_entry *entry;

	printk(banner);

	if(!(bwscon = ioremap_nocache(BWSCON, 0x00000004ul))) {
		ERR("ioremap bwscon failed.");
		return -1;
	}

	if(!(bankcon5 = ioremap_nocache(BANKCON5, 0x00000004ul))) {
		ERR("ioremap bankcon5, failed.");
		return -1;
	}

	u32r = readl(bwscon);
	//DBG("before write, bwscon = 0x%08x.", u32r);
	u32r &= 0xFF8FFFFF;
	u32r |= (0x0E << 20);
	writel(u32r, bwscon);
	u32r = readl(bwscon);
	//DBG("after write, bwscon = 0x%08x.", u32r);

	u32r = readl(bankcon5);
	//DBG("before write, bankcon5 = 0x%08x.", u32r);
	u32r = 0x6b60;
	writel(u32r, bankcon5);
	u32r = readl(bankcon5);
	//DBG("after  write, bankcon5 = 0x%08x.", u32r);

	if(!(request_mem_region(DSP_ADDR_BASE, DSP_ADDR_LEN, "dsp memory"))) {
		ERR("request dsp memory failed.");
		iounmap(bankcon5);
		iounmap(bwscon);
		return -1;
	}

	if(!(hpi_dev.dsp_memory_ptr = ioremap_nocache(DSP_ADDR_BASE, DSP_ADDR_LEN))) {
		ERR("ioremap dsp memory failed.");
		release_mem_region(DSP_ADDR_BASE, DSP_ADDR_LEN);
		iounmap(bankcon5);
		iounmap(bwscon);
		return -1;
	}

	//DBG("dsp_memory_ptr = 0x%08x.", (u32)hpi_dev.dsp_memory_ptr);
	hpi_dev.hpic_write_ptr = hpi_dev.dsp_memory_ptr + 0x00;
	hpi_dev.hpic_read_ptr = hpi_dev.dsp_memory_ptr + 0x20;
	hpi_dev.hpia_write_ptr = hpi_dev.dsp_memory_ptr + 0x04;
	hpi_dev.hpia_read_ptr = hpi_dev.dsp_memory_ptr + 0x24;
	hpi_dev.hpid_autowrite_ptr = hpi_dev.dsp_memory_ptr + 0x08;
	hpi_dev.hpid_autoread_ptr = hpi_dev.dsp_memory_ptr + 0x28;
	hpi_dev.hpid_write_ptr = hpi_dev.dsp_memory_ptr + 0x0c;
	hpi_dev.hpid_read_ptr = hpi_dev.dsp_memory_ptr + 0x2c;
	hpi_dev.hpi_spinlock = SPIN_LOCK_UNLOCKED;
	hpi_dev.wrBufLock = SPIN_LOCK_UNLOCKED;
	hpi_dev.rdBufLock = SPIN_LOCK_UNLOCKED;

	if((rc = register_chrdev(HPI_MAJOR, DRIVER_NAME, &hpi_fops)) < 0) {
		ERR("register %s char dev failed.", DRIVER_NAME);
		iounmap(hpi_dev.dsp_memory_ptr);
		release_mem_region(DSP_ADDR_BASE, DSP_ADDR_LEN);
		iounmap(bankcon5);
		iounmap(bwscon);
		return -1;
	}

	reset_dsp();
	clear_int();

	entry = create_proc_entry("hpi", 0, NULL);
	if (entry)
		entry->proc_fops = &proc_hpi_operations;

	s3c2410_gpio_cfgpin(S3C2410_GPF6, S3C2410_GPF6_OUTP);//debug gpio
	s3c2410_gpio_setpin(S3C2410_GPF6, 0);

	return 0;
}

static void __exit hpi_exit(void)
{
	remove_proc_entry("hpi", NULL);
	free_irq(HPI_WR_IRQ, NULL);
	free_irq(HPI_RD_IRQ, NULL);
	unregister_chrdev(HPI_MAJOR, DRIVER_NAME);
	iounmap(hpi_dev.dsp_memory_ptr);
	release_mem_region(DSP_ADDR_BASE, DSP_ADDR_LEN);
	iounmap(bankcon5);
	iounmap(bwscon);
}

module_init(hpi_init);
module_exit(hpi_exit);
MODULE_LICENSE("GPL");
