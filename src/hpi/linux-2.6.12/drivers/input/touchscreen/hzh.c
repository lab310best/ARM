/*
 * s3c2410-ts.c
 *
 * touchScreen driver for SAMSUNG S3C2410
 *
 * Author: Janghoon Lyu <nandy@mizi.com>
 * Date  : $Date: 2002/06/04 07:11:00 $ 
 *
 * $Revision: 1.1.2.6 $
 *
 * Based on pt036001b-ts.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * History:
 *
 * 2002-05-27: Janghoon Lyu <nandy@mizi.com>
 *    - Add HOOK_FOR_DRAG
 *    - PM 코드가 들어가 있긴 하지만 테스트 되지 않았음.
 * 
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/devfs_fs_kernel.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-adc.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

/* debug macros */
#define	DEBUG
#undef DEBUG
#ifdef DEBUG
#define DPRINTK( x... )	printk("%s: ", __FUNCTION__,  ##x)
#else
#define DPRINTK( x... )
#endif

typedef struct {
  unsigned short pressure;
  unsigned short x;
  unsigned short y;
  unsigned short pad;
} TS_RET;

typedef struct {
  int xscale;
  int xtrans;
  int yscale;
  int ytrans;
  int xyswap;
} TS_CAL;

#define PEN_UP	        0		
#define PEN_DOWN	1
#define PEN_FLEETING	2
#define MAX_TS_BUF	16	/* how many do we want to buffer */

#undef USE_ASYNC	
#define DEVICE_NAME	"s3c2410ts"
#define TSRAW_MINOR	1

typedef struct {
	unsigned int penStatus;		/* PEN_UP, PEN_DOWN, PEN_SAMPLE */
	TS_RET buf[MAX_TS_BUF];		/* protect against overrun */
	unsigned int head, tail;	/* head and tail for queued events */
	wait_queue_head_t wq;
	spinlock_t lock;
#ifdef USE_ASYNC
	struct fasync_struct *aq;
#endif
#ifdef CONFIG_PM
	struct pm_dev *pm_dev;
#endif
} TS_DEV;

static TS_DEV tsdev;

#define BUF_HEAD	(tsdev.buf[tsdev.head])
#define BUF_TAIL	(tsdev.buf[tsdev.tail])
#define INCBUF(x,mod) 	((++(x)) & ((mod) - 1))

static int tsMajor = 0;

static void (*tsEvent)(void);

#define HOOK_FOR_DRAG
#ifdef HOOK_FOR_DRAG
#define TS_TIMER_DELAY  (HZ/100) /* 10 ms */
static struct timer_list ts_timer;
#endif

#define wait_down_int()	__raw_writel(DOWN_INT | XP_PULL_UP_EN | \
				XP_AIN | XM_HIZ | YP_AIN | YM_GND | \
				XP_PST(WAIT_INT_MODE), S3C2410_ADCTSC)
#define wait_up_int()	__raw_writel(UP_INT | XP_PULL_UP_EN | XP_AIN | XM_HIZ | \
				YP_AIN | YM_GND | XP_PST(WAIT_INT_MODE), S3C2410_ADCTSC)
#define mode_x_axis()	__raw_writel(XP_EXTVLT | XM_GND | YP_AIN | YM_HIZ | \
				XP_PULL_UP_DIS | XP_PST(X_AXIS_MODE), S3C2410_ADCTSC)
#define mode_x_axis_n()	__raw_writel(XP_EXTVLT | XM_GND | YP_AIN | YM_HIZ | \
				XP_PULL_UP_DIS | XP_PST(NOP_MODE), S3C2410_ADCTSC)
#define mode_y_axis()	__raw_writel(XP_AIN | XM_HIZ | YP_EXTVLT | YM_GND | \
				XP_PULL_UP_DIS | XP_PST(Y_AXIS_MODE), S3C2410_ADCTSC)
#define start_adc_x()	do {__raw_writel(PRESCALE_EN | PRSCVL(49) | \
				ADC_INPUT(ADC_IN7) | ADC_START_BY_RD_EN | \
				ADC_NORMAL_MODE, S3C2410_ADCCON); \
				__raw_readl(S3C2410_ADCDAT0); } while(0)
#define start_adc_y()	do {__raw_writel(PRESCALE_EN | PRSCVL(49) | \
				ADC_INPUT(ADC_IN5) | ADC_START_BY_RD_EN | \
				ADC_NORMAL_MODE, S3C2410_ADCCON); \
				__raw_readl(S3C2410_ADCDAT1); } while(0)
#define disable_ts_adc()	__raw_writel(__raw_readl(S3C2410_ADCCON)&~ADCCON_READ_START, S3C2410_ADCCON)

static int adc_state = 0;
static int x, y;	/* touch screen coorinates */

#define	RT_BT_EMU_TM	((HZ>>1)+(HZ>>2))	//0.75S
static u16 ts_r_x[5];
static u16 ts_r_y[5];
static u16 ts_r_idx;
static u16 ts_r_beg;
static u32 dn_start;

static void tsEvent_raw(void)
{

	if (tsdev.penStatus == PEN_DOWN) {
		u16 i, j;
#ifdef HOOK_FOR_DRAG 
		ts_timer.expires = jiffies + TS_TIMER_DELAY;
		add_timer(&ts_timer);
#endif
		ts_r_x[ts_r_idx] = x;
		ts_r_y[ts_r_idx] = y;
		ts_r_idx++;
		if(ts_r_idx>=5)
			ts_r_idx = 0;

		if(ts_r_beg) {
			ts_r_beg--;
			if(ts_r_beg)
				return;
			dn_start = jiffies;
		}

		for(i=4; i; i--)
			for(j=i; j; j--) {
				u16 swap_xy;
				if(ts_r_x[j-1]>ts_r_x[i]) {
					swap_xy = ts_r_x[j-1];
					ts_r_x[j-1] = ts_r_x[i];
					ts_r_x[i] = swap_xy;
				}
				if(ts_r_y[j-1]>ts_r_y[i]) {
					swap_xy = ts_r_y[j-1];
					ts_r_y[j-1] = ts_r_y[i];
					ts_r_y[i] = swap_xy;
				}
			}

		BUF_HEAD.x = x = ts_r_x[3];
		BUF_HEAD.y = y = ts_r_y[3];
		BUF_HEAD.pressure = ((jiffies - dn_start) >= RT_BT_EMU_TM) ? 2 : PEN_DOWN;
	} else {
#ifdef HOOK_FOR_DRAG 
		del_timer(&ts_timer);
#endif
		if(ts_r_beg)
			return;
		BUF_HEAD.x = x;//0;
		BUF_HEAD.y = y;//0;
		BUF_HEAD.pressure = PEN_UP;
	}

	tsdev.head = INCBUF(tsdev.head, MAX_TS_BUF);
	wake_up_interruptible(&(tsdev.wq));

#ifdef USE_ASYNC
	if (tsdev.aq)
		kill_fasync(&(tsdev.aq), SIGIO, POLL_IN);
#endif

#ifdef CONFIG_PM
	pm_access(tsdev.pm_dev);
#endif
}

static int tsRead(TS_RET * ts_ret)
{
	spin_lock_irq(&(tsdev.lock));
	ts_ret->x = BUF_TAIL.x;
	ts_ret->y = BUF_TAIL.y;
	ts_ret->pressure = BUF_TAIL.pressure;
	tsdev.tail = INCBUF(tsdev.tail, MAX_TS_BUF);
	spin_unlock_irq(&(tsdev.lock));

	return sizeof(TS_RET);
}


static ssize_t s3c2410_ts_read(struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
	TS_RET ts_ret;

retry: 
	if (tsdev.head != tsdev.tail) {
		int count;
		count = tsRead(&ts_ret);
		if (count) copy_to_user(buffer, (char *)&ts_ret, count);
		return count;
	} else {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		interruptible_sleep_on(&(tsdev.wq));
		if (signal_pending(current))
			return -ERESTARTSYS;
		goto retry;
	}

	return sizeof(TS_RET);
}

#ifdef USE_ASYNC
static int s3c2410_ts_fasync(int fd, struct file *filp, int mode) 
{
	return fasync_helper(fd, filp, mode, &(tsdev.aq));
}
#endif

static unsigned int s3c2410_ts_poll(struct file *filp, struct poll_table_struct *wait)
{
	poll_wait(filp, &(tsdev.wq), wait);
	return (tsdev.head == tsdev.tail) ? 0 : (POLLIN | POLLRDNORM); 
}

static inline void start_ts_adc(void)
{
	adc_state = 0;
	mode_x_axis();
	start_adc_x();
}

static inline void s3c2410_get_XY(void)
{
	if (adc_state == 0) { 
		adc_state = 1;
		disable_ts_adc();
		y = __raw_readl(S3C2410_ADCDAT1) & 0x3ff;
		mode_y_axis();
		start_adc_y();
	} else if (adc_state == 1) { 
		adc_state = 0;
		disable_ts_adc();
		x = __raw_readl(S3C2410_ADCDAT0) & 0x3ff;
		DPRINTK("PEN DOWN: x: %08d, y: %08d\n", x, y);
		wait_up_int();
		tsdev.penStatus = PEN_DOWN;
		tsEvent();
	}
}

static irqreturn_t s3c2410_isr_adc(int irq, void *dev_id, struct pt_regs *reg)
{
#if 0
	DPRINTK("Occured Touch Screen Interrupt\n");
	DPRINTK("SUBSRCPND = 0x%08lx\n", SUBSRCPND);
#endif
	spin_lock_irq(&(tsdev.lock));
	if (tsdev.penStatus == PEN_UP)
	  s3c2410_get_XY();
#ifdef HOOK_FOR_DRAG
	else
	  s3c2410_get_XY();
#endif
	spin_unlock_irq(&(tsdev.lock));

	return IRQ_HANDLED;
}

static irqreturn_t s3c2410_isr_tc(int irq, void *dev_id, struct pt_regs *reg)
{
#if 0
	DPRINTK("Occured Touch Screen Interrupt\n");
	DPRINTK("SUBSRCPND = 0x%08lx\n", SUBSRCPND);
#endif
	spin_lock_irq(&(tsdev.lock));
	if (tsdev.penStatus == PEN_UP) {
	  ts_r_idx = 0;		
	  ts_r_beg = 5;
	  start_ts_adc();
	} else {
	  tsdev.penStatus = PEN_UP;
	  DPRINTK("PEN UP: x: %08d, y: %08d\n", x, y);
	  wait_down_int();
	  tsEvent();
	}
	spin_unlock_irq(&(tsdev.lock));

	return IRQ_HANDLED;
}

#ifdef HOOK_FOR_DRAG
static void ts_timer_handler(unsigned long data)
{
	spin_lock_irq(&(tsdev.lock));
	if (tsdev.penStatus == PEN_DOWN) {
		start_ts_adc();
	}
	spin_unlock_irq(&(tsdev.lock));
}
#endif

static int s3c2410_ts_open(struct inode *inode, struct file *filp)
{
	tsdev.head = tsdev.tail = 0;
	tsdev.penStatus = PEN_UP;
#ifdef HOOK_FOR_DRAG 
	init_timer(&ts_timer);
	ts_timer.function = ts_timer_handler;
#endif
	tsEvent = tsEvent_raw;
	init_waitqueue_head(&(tsdev.wq));

	//MOD_INC_USE_COUNT;
	return 0;
}

static int s3c2410_ts_release(struct inode *inode, struct file *filp)
{
#ifdef HOOK_FOR_DRAG
	del_timer(&ts_timer);
#endif
	//MOD_DEC_USE_COUNT;
	return 0;
}

static struct file_operations s3c2410_fops = {
	owner:	THIS_MODULE,
	open:	s3c2410_ts_open,
	read:	s3c2410_ts_read,	
	release:	s3c2410_ts_release,
#ifdef USE_ASYNC
	fasync:	s3c2410_ts_fasync,
#endif
	poll:	s3c2410_ts_poll,
};

void tsEvent_dummy(void) {}
#ifdef CONFIG_PM
static int s3c2410_ts_pm_callback(struct pm_dev *pm_dev, pm_request_t req, 
								   void *data) 
{
    switch (req) {
		case PM_SUSPEND:
			tsEvent = tsEvent_dummy;
			break;
		case PM_RESUME:
			tsEvent = tsEvent_raw;
			wait_down_int();
			break;
    }
    return 0;
}
#endif

static int __init s3c2410ts_probe(struct device *dev)
{
	int ret;

	tsEvent = tsEvent_dummy;

	ret = register_chrdev(0, DEVICE_NAME, &s3c2410_fops);
	if (ret < 0) {
	  printk(DEVICE_NAME " can't get major number\n");
	  return ret;
	}
	tsMajor = ret;
	printk("%s device driver MAJOR:%d\n", DEVICE_NAME, tsMajor);

	/* set gpio to XP, YM, YP and  YM */
//	set_gpio_ctrl(GPIO_YPON); 
//	set_gpio_ctrl(GPIO_YMON);
//	set_gpio_ctrl(GPIO_XPON);
//	set_gpio_ctrl(GPIO_XMON);
	__raw_writel(__raw_readl(S3C2410_GPGCON)|(0xff<<24), S3C2410_GPGCON);

	__raw_writel(30000, S3C2410_ADCDLY);	
	/* Enable touch interrupt */
	ret = request_irq(IRQ_ADC, s3c2410_isr_adc, SA_INTERRUPT, 
			  DEVICE_NAME, s3c2410_isr_adc);
	if (ret) goto adc_failed;
	ret = request_irq(IRQ_TC, s3c2410_isr_tc, SA_INTERRUPT, 
			  DEVICE_NAME, s3c2410_isr_tc);
	if (ret) goto tc_failed;

	/* Wait for touch screen interrupts */
	wait_down_int();

#ifdef CONFIG_DEVFS_FS
	devfs_mk_dir("touchscreen");
	devfs_mk_cdev(MKDEV(tsMajor, TSRAW_MINOR), S_IFCHR|S_IRUGO|S_IWUSR, "touchscreen/%d", 0);
#endif

#ifdef CONFIG_PM
#if 0
	tsdev.pm_dev = pm_register(PM_GP_DEV, PM_USER_INPUT,
				   s3c2410_ts_pm_callback);
#endif
	tsdev.pm_dev = pm_register(PM_DEBUG_DEV, PM_USER_INPUT,
				   s3c2410_ts_pm_callback);
#endif
	printk(DEVICE_NAME " initialized\n");

	return 0;
 tc_failed:
	free_irq(IRQ_ADC, s3c2410_isr_adc);
 adc_failed:
	return ret;
}

static struct device_driver s3c2410ts_driver = {
	.name		= DEVICE_NAME,
	.bus		= &platform_bus_type,
	.probe		= s3c2410ts_probe,
#ifdef CONFIG_PM
	.suspend	= s3c2410ts_suspend,
	.resume		= s3c2410ts_resume,
#endif
};

static int __init s3c2410ts_init(void)
{
	int ret;

	printk("s3c2410ts init\n");
	//return s3c2410ts_probe(NULL);
	ret = driver_register(&s3c2410ts_driver);
	if(ret)
		printk("register %s driver failed, return code is %d\n", DEVICE_NAME, ret);
	return ret;
}


static void __exit s3c2410ts_exit(void)
{
#ifdef CONFIG_DEVFS_FS	
	devfs_remove("touchscreen/%d", 0);
	devfs_remove("touchscreen");
#endif	
	unregister_chrdev(tsMajor, DEVICE_NAME);
#ifdef CONFIG_PM
	pm_unregister(tsdev.pm_dev);
#endif
	free_irq(IRQ_ADC, s3c2410_isr_adc);
	free_irq(IRQ_TC, s3c2410_isr_tc);
	driver_unregister(&s3c2410ts_driver);
}

module_init(s3c2410ts_init);
module_exit(s3c2410ts_exit);
