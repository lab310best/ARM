#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/dma-mapping.h>

#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-clock.h>
#include <asm/arch/regs-iis.h>

#undef  DEBUG
#ifdef	DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif

#define	DISRC1	(S3C2410_VA_DMA+0x40)
#define	DISRCC1	(S3C2410_VA_DMA+0x44)
#define	DIDST1	(S3C2410_VA_DMA+0x48)
#define	DIDSTC1	(S3C2410_VA_DMA+0x4c)
#define	DCON1	(S3C2410_VA_DMA+0x50)
#define	DSTAT1	(S3C2410_VA_DMA+0x54)
#define	DCSRC1	(S3C2410_VA_DMA+0x58)
#define	DCDST1	(S3C2410_VA_DMA+0x5c)
#define	DMTRIG1	(S3C2410_VA_DMA+0x60)

#define	DISRC2	(S3C2410_VA_DMA+0x80)
#define	DISRCC2	(S3C2410_VA_DMA+0x84)
#define	DIDST2	(S3C2410_VA_DMA+0x88)
#define	DIDSTC2	(S3C2410_VA_DMA+0x8c)
#define	DCON2	(S3C2410_VA_DMA+0x90)
#define	DSTAT2	(S3C2410_VA_DMA+0x94)
#define	DCSRC2	(S3C2410_VA_DMA+0x98)
#define	DCDST2	(S3C2410_VA_DMA+0x9c)
#define	DMTRIG2	(S3C2410_VA_DMA+0xa0)

#define IISCON_CH_RIGHT (1 << 8)        /* Right channel */
#define IISCON_CH_LEFT  (0 << 8)        /* Left channel */
#define IISCON_TX_RDY   (1 << 7)        /* Transmit FIFO is ready(not empty) */
#define IISCON_RX_RDY   (1 << 6)        /* Receive FIFO is ready (not full) */
#define IISCON_TX_DMA   (1 << 5)        /* Transmit DMA service reqeust */
#define IISCON_RX_DMA   (1 << 4)        /* Receive DMA service reqeust */
#define IISCON_TX_IDLE  (1 << 3)        /* Transmit Channel idle */
#define IISCON_RX_IDLE  (1 << 2)        /* Receive Channel idle */
#define IISCON_PRESCALE (1 << 1)        /* IIS Prescaler Enable */
#define IISCON_EN       (1 << 0)        /* IIS enable(start) */

#define IISMOD_SEL_MA   (0 << 8)        /* Master mode(IISLRCK, IISCLK are Output) */
#define IISMOD_SEL_SL   (1 << 8)        /* Slave mode(IISLRCK, IISCLK are Input) */
#define	IISMOD_SEL_NONE	(0 << 6)
#define	IISMOD_SEL_RX	(1 << 6)
#define	IISMOD_SEL_TX	(2 << 6)
#define	IISMOD_SEL_TXRX	(3 << 6)
#define IISMOD_CH_RIGHT (0 << 5)        /* high for right channel */
#define IISMOD_CH_LEFT  (1 << 5)        /* high for left channel */
#define IISMOD_FMT_IIS  (0 << 4)        /* IIS-compatible format */
#define IISMOD_FMT_MSB  (1 << 4)        /* MSB(left)-justified format */
#define IISMOD_BIT_8    (0 << 3)        /* Serial data bit/channel is 8 bit*/
#define IISMOD_BIT_16   (1 << 3)        /* Serial data bit/channel is 16 bit*/
#define IISMOD_FREQ_256 (0 << 2)        /* Master clock freq = 256 fs */
#define IISMOD_FREQ_384 (1 << 2)        /* Master clock freq = 384 fs */
#define	IISMOD_SFREQ_16	(0 << 0)
#define	IISMOD_SFREQ_32	(1 << 0)
#define	IISMOD_SFREQ_48	(2 << 0)

#define IISFCON_TX_NORM (0 << 15)       /* Transmit FIFO access mode: normal */
#define IISFCON_TX_DMA  (1 << 15)       /* Transmit FIFO access mode: DMA */
#define IISFCON_RX_NORM (0 << 14)       /* Receive FIFO access mode: normal */
#define IISFCON_RX_DMA  (1 << 14)       /* Receive FIFO access mode: DMA */
#define IISFCON_TX_EN   (1 << 13)       /* Transmit FIFO enable */
#define IISFCON_RX_EN   (1 << 12)       /* Recevice FIFO enable */

/* UDA1341 Register bits */
#define UDA1341_ADDR		0x14

#define UDA1341_REG_DATA0	(UDA1341_ADDR + 0)
#define UDA1341_REG_STATUS	(UDA1341_ADDR + 2)

/* status control */
#define STAT0			(0x00)
#define STAT0_RST               (1 << 6)
#define STAT0_SC_MASK           (3 << 4)
#define STAT0_SC_512FS          (0 << 4)
#define STAT0_SC_384FS          (1 << 4)
#define STAT0_SC_256FS          (2 << 4)
#define STAT0_IF_MASK           (7 << 1)
#define STAT0_IF_I2S            (0 << 1)
#define STAT0_IF_LSB16          (1 << 1)
#define STAT0_IF_LSB18          (2 << 1)
#define STAT0_IF_LSB20          (3 << 1)
#define STAT0_IF_MSB            (4 << 1)
#define STAT0_IF_LSB16MSB       (5 << 1)
#define STAT0_IF_LSB18MSB       (6 << 1)
#define STAT0_IF_LSB20MSB       (7 << 1)
#define STAT0_DC_FILTER         (1 << 0)
#define STAT0_DC_NO_FILTER	(0 << 0)

#define STAT1			(0x80)
#define STAT1_DAC_GAIN          (1 << 6)        /* gain of DAC */
#define STAT1_ADC_GAIN          (1 << 5)        /* gain of ADC */
#define STAT1_ADC_POL           (1 << 4)        /* polarity of ADC */
#define STAT1_DAC_POL           (1 << 3)        /* polarity of DAC */
#define STAT1_DBL_SPD           (1 << 2)        /* double speed playback */
#define STAT1_ADC_ON            (1 << 1)        /* ADC powered */
#define STAT1_DAC_ON            (1 << 0)        /* DAC powered */

/* data0 direct control */
#define DATA0     		(0x00)
#define DATA0_VOLUME_MASK       (0x3f)
#define DATA0_VOLUME(x)         (x)

#define DATA1     		(0x40)
#define DATA1_BASS(x)           ((x) << 2)
#define DATA1_BASS_MASK         (15 << 2)
#define DATA1_TREBLE(x)         ((x))
#define DATA1_TREBLE_MASK       (3)

#define DATA2     		(0x80)
#define DATA2_PEAKAFTER         (0x1 << 5)
#define DATA2_DEEMP_NONE        (0x0 << 3)
#define DATA2_DEEMP_32KHz       (0x1 << 3)
#define DATA2_DEEMP_44KHz       (0x2 << 3)
#define DATA2_DEEMP_48KHz       (0x3 << 3)
#define DATA2_MUTE              (0x1 << 2)
#define DATA2_FILTER_FLAT       (0x0 << 0)
#define DATA2_FILTER_MIN        (0x1 << 0)
#define DATA2_FILTER_MAX        (0x3 << 0)
/* data0 extend control */
#define EXTADDR(n)              (0xc0 | (n))
#define EXTDATA(d)              (0xe0 | (d))

#define EXT0                    0
#define EXT0_CH1_GAIN(x)        (x)
#define EXT1                    1
#define EXT1_CH2_GAIN(x)        (x)
#define EXT2                    2
#define EXT2_MIC_GAIN_MASK      (7 << 2)
#define EXT2_MIC_GAIN(x)        ((x) << 2)
#define EXT2_MIXMODE_DOUBLEDIFF (0)
#define EXT2_MIXMODE_CH1        (1)
#define EXT2_MIXMODE_CH2        (2)
#define EXT2_MIXMODE_MIX        (3)
#define EXT4                    4
#define EXT4_AGC_ENABLE         (1 << 4)
#define EXT4_INPUT_GAIN_MASK    (3)
#define EXT4_INPUT_GAIN(x)      ((x) & 3)
#define EXT5                    5
#define EXT5_INPUT_GAIN(x)      ((x) >> 2)
#define EXT6                    6
#define EXT6_AGC_CONSTANT_MASK  (7 << 2)
#define EXT6_AGC_CONSTANT(x)    ((x) << 2)
#define EXT6_AGC_LEVEL_MASK     (3)
#define EXT6_AGC_LEVEL(x)       (x)

#define AUDIO_NAME				"UDA1341"
#define AUDIO_NAME_VERBOSE		"UDA1341 audio driver"

#define AUDIO_FMT_MASK          (AFMT_S16_LE)
#define AUDIO_FMT_DEFAULT       (AFMT_S16_LE)

#define AUDIO_CHANNELS_DEFAULT	2
#define AUDIO_RATE_DEFAULT		22050

#define AUDIO_NBFRAGS_DEFAULT	8
#define AUDIO_FRAGSIZE_DEFAULT	8192

#define	DFT_VOLUME	100

static struct device *this_dev;

typedef struct {
	int size;		/* buffer size */
	char *start;		/* point to actual buffer */
	dma_addr_t dma_addr;	/* physical buffer address */
	struct semaphore sem;	/* down before touching the buffer */
	int master;		/* owner for buffer allocation, contain size when true */
	int dma_size;
} audio_buf_t;

typedef struct {
	audio_buf_t *buffers;	/* pointer to audio buffer structures */
	audio_buf_t *buf;	/* current buffer used by read/write */
	u_int buf_idx;		/* index for the pointer above */
	u_int fragsize;		/* fragment i.e. buffer size */
	u_int nbfrags;		/* nbr of fragments */
//	dmach_t dma_ch;		/* DMA channel (channel2 for audio) */
	int   dma_ch;
} audio_stream_t;

static audio_stream_t output_stream;
static audio_stream_t input_stream; /* input */
static u_int rd_buf_tail;
static u_int wr_buf_tail;
static u_int play_dma_running;

#define NEXT_BUF(_s_,_b_) { \
        (_s_)->_b_##_idx++; \
        (_s_)->_b_##_idx %= (_s_)->nbfrags; \
        (_s_)->_b_ = (_s_)->buffers + (_s_)->_b_##_idx; }


static u_int audio_rate;
static int audio_channels;
static int audio_fmt;
static u_int audio_fragsize;
static u_int audio_nbfrags;

static int audio_dev_dsp;
static int audio_dev_mixer;

static int audio_rd_refcount;
static int audio_wr_refcount;
#define	audio_active			(audio_rd_refcount | audio_wr_refcount)

static int uda1341_volume;
static int uda1341_boost;
static int mixer_igain = 0x4;	/* -6db*/

#define	L3MOD_HI()	__raw_writel(__raw_readl(S3C2410_GPBDAT)|(1<<2), S3C2410_GPBDAT)
#define	L3MOD_LOW()	__raw_writel(__raw_readl(S3C2410_GPBDAT)&~(1<<2), S3C2410_GPBDAT)
#define	L3DAT_HI()	__raw_writel(__raw_readl(S3C2410_GPBDAT)|(1<<3), S3C2410_GPBDAT)
#define	L3DAT_LOW()	__raw_writel(__raw_readl(S3C2410_GPBDAT)&~(1<<3), S3C2410_GPBDAT)
#define	L3CLK_HI()	__raw_writel(__raw_readl(S3C2410_GPBDAT)|(1<<4), S3C2410_GPBDAT)
#define	L3CLK_LOW()	__raw_writel(__raw_readl(S3C2410_GPBDAT)&~(1<<4), S3C2410_GPBDAT)

static void uda1341_l3_address(u8 data)
{
	int i;
	unsigned long flags;
  
	local_irq_save(flags);

	L3MOD_LOW();//write_gpio_bit(GPIO_L3MODE, 0);
	L3DAT_LOW();//write_gpio_bit(GPIO_L3DATA, 0);
	L3CLK_HI();	//write_gpio_bit(GPIO_L3CLOCK, 1);
	
	udelay(1);
	
	for (i = 0; i < 8; i++) {
		if (data & 0x1) {
			L3CLK_LOW();//write_gpio_bit(GPIO_L3CLOCK, 0);
			udelay(1);
			L3DAT_HI();	//write_gpio_bit(GPIO_L3DATA, 1);
			udelay(1);
			L3CLK_HI();	//write_gpio_bit(GPIO_L3CLOCK, 1);
			udelay(1);
		} else {
			L3CLK_LOW();//write_gpio_bit(GPIO_L3CLOCK, 0);
			udelay(1);
			L3DAT_LOW();//write_gpio_bit(GPIO_L3DATA, 0);
			udelay(1);
			L3CLK_HI();	//write_gpio_bit(GPIO_L3CLOCK, 1);
			udelay(1);
		}
		data >>= 1;
	}

	L3MOD_HI();	//write_gpio_bit(GPIO_L3MODE, 1);
	udelay(1);
	local_irq_restore(flags);
}

static void uda1341_l3_data(u8 data)
{
	int i;
	unsigned long flags;
  
	local_irq_save(flags);

	L3MOD_HI();	//write_gpio_bit(GPIO_L3MODE, 1);
	udelay(1);

	L3MOD_LOW();//write_gpio_bit(GPIO_L3MODE, 0);
	udelay(1);
	L3MOD_HI();	//write_gpio_bit(GPIO_L3MODE, 1);

	for (i = 0; i < 8; i++) {
		if (data & 0x1) {
			L3CLK_LOW();//write_gpio_bit(GPIO_L3CLOCK, 0);
			udelay(1);
			L3DAT_HI();	//write_gpio_bit(GPIO_L3DATA, 1);
			udelay(1);
			L3CLK_HI();	//write_gpio_bit(GPIO_L3CLOCK, 1);
			udelay(1);
		} else {
			L3CLK_LOW();//write_gpio_bit(GPIO_L3CLOCK, 0);
			udelay(1);
			L3DAT_LOW();//write_gpio_bit(GPIO_L3DATA, 0);
			udelay(1);
			L3CLK_HI();	//write_gpio_bit(GPIO_L3CLOCK, 1);
			udelay(1);
		}

		data >>= 1;
	}

	L3MOD_HI();	//write_gpio_bit(GPIO_L3MODE, 1);
	L3MOD_LOW();//write_gpio_bit(GPIO_L3MODE, 0);
	udelay(1);
	L3MOD_HI();	//write_gpio_bit(GPIO_L3MODE, 1);
	local_irq_restore(flags);
}

struct iis_psr_val {
	__u32 rate;
	__u16 psr;
	__u16 fs;
};

static struct iis_psr_val iis_psr_para[] = {
			{8000,  16, 384},
			{11025, 11, 384},
			{16000, 8, 384},
			{22050, 6, 384},
			{24000, 8, 256},
			{32000, 4, 384},
			{44100, 3, 384},
			{48000, 4, 256}
		};

static void init_s3c2410_iis_bus_rx(void)
{
	
	__raw_writel(0, S3C2410_IISCON);
	__raw_writel(0, S3C2410_IISMOD);
	__raw_writel(0, S3C2410_IISFCON);
	
	/* 22.05 KHz , 384fs */
	__raw_writel(((iis_psr_para[3].psr-1)<<5)|(iis_psr_para[3].psr-1), S3C2410_IISPSR);

	__raw_writel(IISCON_RX_DMA  /* Transmit DMA service request */
		|IISCON_TX_IDLE         /* Receive Channel idle */
		|IISCON_PRESCALE,		/* IIS Prescaler Enable */
		S3C2410_IISCON);

	__raw_writel(IISMOD_SEL_MA         /* Master mode */
		| IISMOD_SEL_RX         /* Transmit */
		| IISMOD_CH_RIGHT       /* Low for left channel */
		| IISMOD_FMT_MSB        /* MSB-justified format */
		| IISMOD_BIT_16         /* Serial data bit/channel is 16 bit */
		| IISMOD_FREQ_384       /* Master clock freq = 384 fs */
		| IISMOD_SFREQ_32,      /* 32 fs */
		S3C2410_IISMOD);

	__raw_writel(IISFCON_RX_DMA /* Transmit FIFO access mode: DMA */
		| IISFCON_RX_EN,       	/* Transmit FIFO enable */
		S3C2410_IISFCON);                

	__raw_writel(__raw_readl(S3C2410_IISCON)|IISCON_EN, S3C2410_IISCON);	/* IIS enable(start) */
}

static void init_s3c2410_iis_bus_tx(void)
{
	
	__raw_writel(0, S3C2410_IISCON);
// 	printk(KERN_INFO "have done __raw_ writel() about S3C2410_IISCON-->(VIRTUAL).\n");
// 	printk(KERN_INFO "S3C2410_IISCON=%08x.\n",S3C2410_IISCON); 
	
	__raw_writel(0, S3C2410_IISMOD);
//        printk(KERN_ERR "S3C2410_IISMOD=%08x.\n",S3C2410_IISMOD);
	
	__raw_writel(0, S3C2410_IISFCON);
//	printk(KERN_ERR "S3C2410_IISFCON=%08x.\n",S3C2410_IISFCON);
        

	/* 22.05 KHz , 384fs */
	__raw_writel(((iis_psr_para[3].psr-1)<<5)|(iis_psr_para[3].psr-1), S3C2410_IISPSR);

	__raw_writel(IISCON_TX_DMA	/* Transmit DMA service request */
	  	|IISCON_RX_IDLE			/* Receive Channel idle */
		|IISCON_PRESCALE, 		/* IIS Prescaler Enable */
		S3C2410_IISCON);

 	__raw_writel(IISMOD_SEL_MA	/* Master mode */
  		| IISMOD_SEL_TX         /* Transmit */
		| IISMOD_CH_RIGHT       /* Low for left channel */
		| IISMOD_FMT_MSB        /* MSB-justified format */
		| IISMOD_BIT_16         /* Serial data bit/channel is 16 bit */
		| IISMOD_FREQ_384       /* Master clock freq = 384 fs */
		| IISMOD_SFREQ_32,		/* 32 fs */
		S3C2410_IISMOD);

	__raw_writel(IISFCON_TX_DMA /* Transmit FIFO access mode: DMA */
		| IISFCON_TX_EN,		/* Transmit FIFO enable */
		S3C2410_IISFCON);

	__raw_writel(__raw_readl(S3C2410_IISCON)|IISCON_EN, S3C2410_IISCON);	/* IIS enable(start) */

}

static void audio_clear_buf(audio_stream_t * s)
{
	DPRINTK("audio_clear_buf\n");
  
//	s3c2410_dma_flush_all(s->dma_ch);

	if (s->buffers) {
		int frag;

		for (frag = 0; frag < s->nbfrags; frag++) {
			if (!s->buffers[frag].master)	//master = size
				continue;
			dma_free_writecombine(this_dev, s->buffers[frag].master, 
									s->buffers[frag].start, s->buffers[frag].dma_addr);
		}
		kfree(s->buffers);
		s->buffers = NULL;
	}

	s->buf_idx = 0;
	s->buf = NULL;
}

static int audio_setup_buf(audio_stream_t * s)
{
	int frag;
	int dmasize = 0;
	char *dmabuf = 0;
	dma_addr_t dmaphys = 0;
  
	if (s->buffers)
		{
	return -EBUSY;}
	
	wr_buf_tail = rd_buf_tail = play_dma_running = 0;

	s->nbfrags = audio_nbfrags;
	s->fragsize = audio_fragsize;

	s->buffers = (audio_buf_t *)
	    kmalloc(sizeof(audio_buf_t) * s->nbfrags, GFP_KERNEL);
	if (!s->buffers)
		{
	goto err;}
	memset(s->buffers, 0, sizeof(audio_buf_t) * s->nbfrags);

	for (frag = 0; frag < s->nbfrags; frag++) {
		audio_buf_t *b = &s->buffers[frag];

/*		if (!dmasize) {
			dmasize = (s->nbfrags - frag) * s->fragsize;
			do {
				dmabuf = dma_alloc_writecombine(dev, dmasize, &dmaphys, GFP_KERNEL),
				if (!dmabuf) 
				    	dmasize -= s->fragsize;
			} while (!dmabuf && dmasize);
			if (!dmabuf)
				goto err;
			b->master = dmasize;
		}*/
		dmabuf = dma_alloc_writecombine(this_dev, s->fragsize, &dmaphys, GFP_KERNEL);
		if(!dmabuf) {
			//printk("Unable to get DMA buffer\n");
			{
		goto err;}
		}
		b->master = s->fragsize;

		b->start = dmabuf;
		b->dma_addr = dmaphys;
		sema_init(&b->sem, 1);
		DPRINTK("buf %d: start %p dma %d\n", frag, b->start, b->dma_addr);
    
		dmabuf += s->fragsize;
		dmaphys += s->fragsize;
		dmasize -= s->fragsize;
	}

	s->buf_idx = 0;
	s->buf = &s->buffers[0];

	return 0;

err:
	
	printk(AUDIO_NAME ": unable to allocate audio memory\n ");
	audio_clear_buf(s);
	return -ENOMEM;
}

static irqreturn_t iisin_dma_done_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	
//	printk(KERN_DEBUG"%d\n", input_stream.buf_idx);
	audio_stream_t *s = &input_stream;
	
	if(s->buffers) {
		audio_buf_t *b = s->buffers+rd_buf_tail;
		
		rd_buf_tail++;
		if(rd_buf_tail>=s->nbfrags)
			rd_buf_tail = 0;
		__raw_writel(s->buffers[rd_buf_tail].dma_addr, DIDST1);
		__raw_writel(2, DMTRIG1);
		if(s->buf_idx==rd_buf_tail) {
			s->buf_idx++;
			if(s->buf_idx>=s->nbfrags)
				s->buf_idx = 0;
			s->buf = s->buffers+s->buf_idx;
		}
			
		if(b->size==0)
			up(&(b->sem));
		b->size = s->fragsize;
	}
	return IRQ_HANDLED;
}

static irqreturn_t iisout_dma_done_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	audio_stream_t *s = &output_stream;	
 //printk(KERN_ERR "line %d passed! enter iisout_dma_done_handler()\n",__LINE__);
	if(s->buffers) {
		audio_buf_t *b = s->buffers+wr_buf_tail;
		
		wr_buf_tail++;
		if(wr_buf_tail>=s->nbfrags)
			wr_buf_tail = 0;
		
		up(&b->sem);
//		wake_up(&b->sem.wait);

		b = s->buffers+wr_buf_tail;
		if(b!=s->buf) {			
			__raw_writel(b->dma_addr, DISRC2); 		//DMA src memory
		//	__raw_writel(0, DISRCC2);				//AHB, address increase
		//	__raw_writel(0x55000010, DIDST2);		//IISFIFO phy address
		//	__raw_writel(3, DIDSTC2);				//APB, address fixed
			__raw_writel(0xa0d00000|(b->dma_size>>1), DCON2);	//set mode and length
			__raw_writel(2, DMTRIG2);				//set channel on
			b->dma_size = 0;
		} else
			play_dma_running = 0;
	}
	return IRQ_HANDLED;
}

 /* using when write */
static int audio_sync(struct file *file)
{
  
	audio_stream_t *s = &output_stream;
	audio_buf_t *b = s->buf;
	unsigned long flags;

	DPRINTK("audio_sync\n");

	if (!s->buffers)
		return 0;

	if (b->size != 0) {
		down(&b->sem);
		//s3c2410_dma_queue_buffer(s->dma_ch, (void *) b,
		//			 b->dma_addr, b->size, DMA_BUF_WR);
		
		local_irq_save(flags);
		if(!play_dma_running) {
			__raw_writel(b->dma_addr, DISRC2); 		//DMA src memory
			__raw_writel(0, DISRCC2);				//AHB, address increase
			//__raw_writel(S3C2410_IISFIFO, DIDST2);	//IISFIFO address
			__raw_writel(0x55000010, DIDST2);		//IISFIFO phy address
			__raw_writel(3, DIDSTC2);				//APB, address fixed
			__raw_writel(0xa0d00000|(b->size>>1), DCON2);	//set mode and length
			__raw_writel(2, DMTRIG2);				//set channel on
		} else
			b->dma_size = b->size;					
		b->size = 0;
		NEXT_BUF(s, buf);
		local_irq_restore(flags);
	}

	b = s->buffers + ((s->nbfrags + s->buf_idx - 1) % s->nbfrags);
	if (down_interruptible(&b->sem))
		return -EINTR;
	up(&b->sem);

	return 0;
}

static inline int copy_from_user_mono_stereo(char *to, const char *from, int count)
{
	u_int *dst = (u_int *)to;
	const char *end = from + count;
 
	if (verify_area(VERIFY_READ, from, count))
		return -EFAULT;

	if ((int)from & 0x2) {
		u_int v;
		__get_user(v, (const u_short *)from); from += 2;
		*dst++ = v | (v << 16);
	}

	while (from < end-2) {
		u_int v, x, y;
		__get_user(v, (const u_int *)from); from += 4;
		x = v << 16;
		x |= x >> 16;
		y = v >> 16;
		y |= y << 16;
		*dst++ = x;
		*dst++ = y;
	}

	if (from < end) {
		u_int v;
		__get_user(v, (const u_short *)from);
		*dst = v | (v << 16);
	}

	return 0;
}

static ssize_t smdk2410_audio_write(struct file *file, const char *buffer, 
				    size_t count, loff_t * ppos)
{
	const char *buffer0 = buffer;
	audio_stream_t *s = &output_stream;
	int chunksize, ret = 0;
	unsigned long flags;
 // 
	DPRINTK("audio_write : start count=%d\n", count);

	switch (file->f_flags & O_ACCMODE) {
	  	case O_WRONLY:
	  	case O_RDWR:
	  	
			break;
	  	default:
	  		
		  	return -EPERM;
	}

	if (!s->buffers && audio_setup_buf(s))
		{
	return -ENOMEM;}

	count &= ~0x03;

	while (count > 0) {
		
		audio_buf_t *b = s->buf;

		if (file->f_flags & O_NONBLOCK) {
			
			ret = -EAGAIN;
			if (down_trylock(&b->sem))
				{
			break;}
		} else {
			ret = -ERESTARTSYS;
			
			if (down_interruptible(&b->sem))
				{
			break;}
		}

		if (audio_channels == 2) {
			//printk(KERN_ERR "line %d passed!audio_channels == 2\n",__LINE__);
			chunksize = s->fragsize - b->size;
			if (chunksize > count) //!!!
				{chunksize = count;
			}
			DPRINTK("write %d to %d\n", chunksize, s->buf_idx);
			if (copy_from_user(b->start + b->size, buffer, chunksize)) {
				up(&b->sem);
				
				return -EFAULT;
			}
			b->size += chunksize;
			
		} else {
			
			chunksize = (s->fragsize - b->size) >> 1;

			if (chunksize > count)
				{
			chunksize = count;}
			DPRINTK("write %d to %d\n", chunksize*2, s->buf_idx);
			if (copy_from_user_mono_stereo(b->start + b->size, 
				    		       buffer, chunksize)) {
				up(&b->sem);
				
				return -EFAULT;
			}

			b->size += chunksize*2;
			
		}

		buffer += chunksize;
		count -= chunksize;
		
		if (b->size < s->fragsize) { //!!!
			
			up(&b->sem);
			break; //jump out while
		}
//in while
		//s3c2410_dma_queue_buffer(s->dma_ch, (void *) b,
		//			   b->dma_addr, b->size, DMA_BUF_WR);
		
		local_irq_save(flags);
		
		if(!play_dma_running) {
			
			play_dma_running = 1;
			__raw_writel(b->dma_addr, DISRC2); 		//DMA src memory
			__raw_writel(0, DISRCC2);				//AHB, address increase
			//__raw_writel(S3C2410_IISFIFO, DIDST2);	//IISFIFO address
			__raw_writel(0x55000010, DIDST2);		//IISFIFO phy address
			__raw_writel(3, DIDSTC2);				//APB, address fixed
			__raw_writel(0xa0d00000|(b->size>>1), DCON2);	//set mode and length
			__raw_writel(2, DMTRIG2);				//set channel on
			
			//printk("%x\n", __raw_readl(S3C2410_IISCON));
			//printk("%x\n", __raw_readl(S3C2410_IISMOD));
			//printk("%x\n", __raw_readl(S3C2410_IISPSR));
			//printk("%x\n", __raw_readl(S3C2410_IISFCON));
			//printk("%x\n", __raw_readl(DISRC2));
			//printk("%x\n", __raw_readl(DISRCC2));
			//printk("%x\n", __raw_readl(DIDST2));
			//printk("%x\n", __raw_readl(DIDSTC2));
			//printk("%x\n", __raw_readl(DCON2));
			//printk("%x\n", __raw_readl(DMTRIG2));
		} else
			
		b->dma_size = b->size;
		b->size = 0;
		NEXT_BUF(s, buf);
		local_irq_restore(flags);
		//in while
	}   //out while
	if ((buffer - buffer0))
		
	ret = buffer - buffer0;

	DPRINTK("audio_write : end count=%d\n\n", ret);
  //printk(KERN_ERR "line %d passed!audio_write : end count=%d\n",__LINE__);
	return ret;
}

static ssize_t smdk2410_audio_read(struct file *file, char *buffer, 
                                        size_t count, loff_t * ppos)
{
	const char *buffer0 = buffer;
	audio_stream_t *s = &input_stream;
	int chunksize, ret = 0;
	DPRINTK("audio_read: count=%d\n", count);
/*
	if (ppos != &file->f_pos)
	{	
		printk(KERN_ERR "ppos != &file->f_pos!!!!!!!!!!!\n");
		return -ESPIPE;
	}
*/	

	if (!s->buffers) {
		int i;
 
		if (audio_setup_buf(s))
			return -ENOMEM;
 
		for (i = 0; i < s->nbfrags; i++) {
			audio_buf_t *b = s->buf;
			down(&b->sem);
			NEXT_BUF(s, buf);
		}

//		DMTRIG1 = 4;
//		DISRC1  = 0x55000010;			//DMA src IIS FIFO
//		DISRCC1 = 3;					//APB, address fixed
//		DIDST1  = s->buf->dma_addr;		//Memory address
//		DIDSTC1 = 0;					//AHB, address increase
//		DCON1   = 0xa2d00000;			//set mode and length
//		DCON1  |= (s->fragsize>>1);
//		DMTRIG1 = 2;					//set channel on
//		IISCON |= IISCON_EN;			/* IIS enable(start) */
		
		__raw_writel(4, DMTRIG1);				//channel off
		//__raw_writel(S3C2410_IISFIFO, DISRC1);	//IISFIFO address
		__raw_writel(0x55000010, DISRC1);		//IISFIFO phy address
		__raw_writel(3, DISRCC1);				//APB, address fixed
		__raw_writel(s->buf->dma_addr, DIDST1);	//DMA dst memory
		__raw_writel(0, DIDSTC1);				//AHB, address increase
		__raw_writel(0xa2d00000|(s->fragsize>>1), DCON1);	//set mode and length
		__raw_writel(2, DMTRIG1);				//set channel on
		
		__raw_writel(__raw_readl(S3C2410_IISCON)|IISCON_EN, S3C2410_IISCON);	//IIS enable
	}
	while (count > 0) {
		audio_buf_t *b = s->buf;

		/* Wait for a buffer to become full */
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			if (down_trylock(&b->sem))
				break;
		} else {
			ret = -ERESTARTSYS;
			if (down_interruptible(&b->sem))
				break;
		}
		chunksize = b->size;
		if (chunksize > count)
			chunksize = count;
		DPRINTK("read %d from %d\n", chunksize, s->buf_idx);
		if (copy_to_user(buffer, b->start + s->fragsize - b->size, chunksize)) {
			up(&b->sem);
			return -EFAULT;
		}
		b->size -= chunksize;
		buffer += chunksize;
		count -= chunksize;
		if (b->size > 0) {
			up(&b->sem);
			break;
		}
		
		NEXT_BUF(s, buf);
	}
	
	if ((buffer - buffer0))
		ret = buffer - buffer0;
	DPRINTK("audio_read: return=%d\n", ret);

	return ret;
}

static unsigned int smdk2410_audio_poll(struct file *file, 
					struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	int i;
  
	DPRINTK("audio_poll(): mode=%s\n",
		(file->f_mode & FMODE_WRITE) ? "w" : "");

	if (file->f_mode & FMODE_READ) {
                if (!input_stream.buffers && audio_setup_buf(&input_stream))
                        return -ENOMEM;
                poll_wait(file, &input_stream.buf->sem.wait, wait);

                for (i = 0; i < input_stream.nbfrags; i++) {
                        if (atomic_read(&input_stream.buffers[i].sem.count) > 0)
                                mask |= POLLIN | POLLWRNORM;
				break;
                }
        }

	if (file->f_mode & FMODE_WRITE) {
		if (!output_stream.buffers && audio_setup_buf(&output_stream))
			return -ENOMEM;
		poll_wait(file, &output_stream.buf->sem.wait, wait);

		for (i = 0; i < output_stream.nbfrags; i++) {
			if (atomic_read(&output_stream.buffers[i].sem.count) > 0)
				mask |= POLLOUT | POLLWRNORM;
				break;
		}
	}

	DPRINTK("audio_poll() returned mask of %s\n",
		(mask & POLLOUT) ? "w" : "");

	return mask;
}

static loff_t smdk2410_audio_llseek(struct file *file, loff_t offset, 
				    int origin)
{
     
            return -ESPIPE;
}

static int audio_set_dsp_speed(__u32 val)
{
	int i;
	
	switch (val) {
	case 8000:
	case 11025:
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
		break;
	default:
		printk("Invalid value\n");
		return -1;
	}

	for(i=0; i<ARRAY_SIZE(iis_psr_para); i++) {		
		if(iis_psr_para[i].rate==val)
			break;
	}
	if(i>=ARRAY_SIZE(iis_psr_para)) {
		printk("Invalid psr parameters\n");
		return -1;
	}	
	
	__raw_writel(((iis_psr_para[i].psr-1)<<5)|(iis_psr_para[i].psr-1), S3C2410_IISPSR);
	
	uda1341_l3_address(UDA1341_REG_STATUS);
	if(iis_psr_para[i].fs==256) {
		uda1341_l3_data(STAT0_SC_256FS | STAT0_IF_MSB);     // set 384 system clock, MSB
		__raw_writel(__raw_readl(S3C2410_IISMOD)&~IISMOD_FREQ_384, S3C2410_IISMOD);
	}
	else {
		uda1341_l3_data(STAT0_SC_384FS | STAT0_IF_MSB);     // set 384 system clock, MSB
		__raw_writel(__raw_readl(S3C2410_IISMOD)|IISMOD_FREQ_384, S3C2410_IISMOD);
	}
	
	i = val;
	return i;
}

static int smdk2410_mixer_ioctl(struct inode *inode, struct file *file, 
                                unsigned int cmd, unsigned long arg)
{
	static __u32 audio_mix_modcnt = 0;
	int ret;
	long val = 0;
	
	audio_mix_modcnt++;

	switch (cmd) {
		case SOUND_MIXER_INFO:
		{
			mixer_info info;
			memset(&info, 0, sizeof(info));
			strncpy(info.id, "UDA1341", sizeof(info.id));
			strncpy(info.name,"Philips UDA1341", sizeof(info.name));
			info.modify_counter = audio_mix_modcnt;
			return copy_to_user((void *)arg, &info, sizeof(info));
		}
		case SOUND_OLD_MIXER_INFO:
		{
			_old_mixer_info info;
			memset(&info, 0, sizeof(info));
			strncpy(info.id, "UDA1341", sizeof(info.id));
			strncpy(info.name,"Philips UDA1341", sizeof(info.name));
			return copy_to_user((void *)arg, &info, sizeof(info));
		}

		case SOUND_MIXER_READ_STEREODEVS:
			return put_user(0, (long *) arg);

		case SOUND_MIXER_READ_CAPS:
			val = SOUND_CAP_EXCL_INPUT;
			return put_user(val, (long *) arg);

		case SOUND_MIXER_WRITE_VOLUME:
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
			uda1341_volume = 63 - (((val & 0xff) + 1) * 63) / 100;
			uda1341_l3_address(UDA1341_REG_DATA0);
			uda1341_l3_data(uda1341_volume);
			break;
		
		case SOUND_MIXER_READ_VOLUME:
			val = ((63 - uda1341_volume) * 100) / 63;
			val |= val << 8;
			return put_user(val, (long *) arg);
	
		case SOUND_MIXER_READ_IGAIN:
			val = ((31- mixer_igain) * 100) / 31;
			return put_user(val, (int *) arg);

		case SOUND_MIXER_WRITE_IGAIN:
			ret = get_user(val, (int *) arg);
			if (ret)
				return ret;
			mixer_igain = 31 - (val * 31 / 100);		
			/* use mixer gain channel 1*/
			uda1341_l3_address(UDA1341_REG_DATA0);
			uda1341_l3_data(EXTADDR(EXT0));
			uda1341_l3_data(EXTDATA(EXT0_CH1_GAIN(mixer_igain)));			
			break;

		default:
			DPRINTK("mixer ioctl %u unknown\n", cmd);
			return -ENOSYS;
	}
	
	return 0;
}

static int smdk2410_audio_ioctl(struct inode *inode, struct file *file, 
                                uint cmd, ulong arg)
{
	long val;

	switch (cmd) {
		case OSS_GETVERSION:
		return put_user(SOUND_VERSION, (int *)arg);
		
	  	case SNDCTL_DSP_SETFMT:
			get_user(val, (long *) arg);
		  	if (val & AUDIO_FMT_MASK) {
			    	audio_fmt = val;
			    	break;
		  	} else
				return -EINVAL;

	  	case SNDCTL_DSP_CHANNELS:
	  	case SNDCTL_DSP_STEREO:
		  	get_user(val, (long *) arg);
		  	if (cmd == SNDCTL_DSP_STEREO)
			  	val = val ? 2 : 1;
		  	if (val != 1 && val != 2)
			  	return -EINVAL;
		  	audio_channels = val;
		  	break;

	  	case SOUND_PCM_READ_CHANNELS:
		  	put_user(audio_channels, (long *) arg);
		 	break;

	  	case SNDCTL_DSP_SPEED:
		  	get_user(val, (long *) arg);
		  	val = audio_set_dsp_speed(val);
			if (val < 0)
				return -EINVAL;
		  	put_user(val, (long *) arg);
		  	break;

	  	case SOUND_PCM_READ_RATE:
		  	put_user(audio_rate, (long *) arg);
		  	break;

	  	case SNDCTL_DSP_GETFMTS:
		  	put_user(AUDIO_FMT_MASK, (long *) arg);
		  	break;

	  	case SNDCTL_DSP_GETBLKSIZE:
			if(file->f_mode & FMODE_WRITE)
		  		return put_user(audio_fragsize, (long *) arg);
			else		
				return put_user(audio_fragsize, (int *) arg);

	  	case SNDCTL_DSP_SETFRAGMENT:
		        if (file->f_mode & FMODE_WRITE) {	
		  		if (output_stream.buffers)
			  		return -EBUSY;
		  		get_user(val, (long *) arg);
		  		audio_fragsize = 1 << (val & 0xFFFF);
		  		if (audio_fragsize < 16)
			  		audio_fragsize = 16;
		  		if (audio_fragsize > 16384)
			  		audio_fragsize = 16384;
		  		audio_nbfrags = (val >> 16) & 0x7FFF;
				if (audio_nbfrags < 2)
					audio_nbfrags = 2;
		  		if (audio_nbfrags * audio_fragsize > 128 * 1024)
			  		audio_nbfrags = 128 * 1024 / audio_fragsize;
		  		if (audio_setup_buf(&output_stream))
			  		return -ENOMEM;
	
			}
			if (file->f_mode & FMODE_READ) {
				if (input_stream.buffers)
					return -EBUSY;
				get_user(val, (int *) arg);
				audio_fragsize =  1 << (val & 0xFFFF);
				if (audio_fragsize < 16)
					audio_fragsize = 16;
				if (audio_fragsize > 16384)
                                        audio_fragsize = 16384;
                                audio_nbfrags = (val >> 16) & 0x7FFF;
                                if (audio_nbfrags < 2)
                                        audio_nbfrags = 2;
                                if (audio_nbfrags * audio_fragsize > 128 * 1024)
                                        audio_nbfrags = 128 * 1024 / audio_fragsize;
                                if (audio_setup_buf(&input_stream))
                                        return -ENOMEM;

			}
		 	break;

	  	case SNDCTL_DSP_SYNC:
		  	return audio_sync(file);

	  	case SNDCTL_DSP_GETOSPACE:
		{
			audio_stream_t *s = &output_stream;
			audio_buf_info *inf = (audio_buf_info *) arg;
			int err = verify_area(VERIFY_WRITE, inf, sizeof(*inf));
			int i;
			int frags = 0, bytes = 0;

			if (err)
				return err;
			for (i = 0; i < s->nbfrags; i++) {
				if (atomic_read(&s->buffers[i].sem.count) > 0) {
					if (s->buffers[i].size == 0) frags++;
					bytes += s->fragsize - s->buffers[i].size;
				}
			}
			put_user(frags, &inf->fragments);
			put_user(s->nbfrags, &inf->fragstotal);
			put_user(s->fragsize, &inf->fragsize);
			put_user(bytes, &inf->bytes);
			break;
		}

		case SNDCTL_DSP_GETISPACE:
		{
			audio_stream_t *s = &input_stream;
			audio_buf_info *inf = (audio_buf_info *) arg;
			int err = verify_area(VERIFY_WRITE, inf, sizeof(*inf));
			int i;
			int frags = 0, bytes = 0;

			if (!(file->f_mode & FMODE_READ))
                                return -EINVAL;

			if (err)
				return err;
			for(i = 0; i < s->nbfrags; i++){
			if (atomic_read(&s->buffers[i].sem.count) > 0)
                                {
                                        if (s->buffers[i].size == s->fragsize)
                                                frags++;
                                        bytes += s->buffers[i].size;
                                }
                        }
			put_user(frags, &inf->fragments);
                        put_user(s->nbfrags, &inf->fragstotal);
                        put_user(s->fragsize, &inf->fragsize);
                        put_user(bytes, &inf->bytes);
                        break;
		}
	  	case SNDCTL_DSP_RESET:
			if (file->f_mode & FMODE_READ) {
                                audio_clear_buf(&input_stream);
                        }
                        if (file->f_mode & FMODE_WRITE) {
                                audio_clear_buf(&output_stream);
                        }
                        return 0;
		case SNDCTL_DSP_NONBLOCK:
			file->f_flags |= O_NONBLOCK;
                        return 0;
	 	case SNDCTL_DSP_POST:
	      	case SNDCTL_DSP_SUBDIVIDE:
	      	case SNDCTL_DSP_GETCAPS:
	      	case SNDCTL_DSP_GETTRIGGER:
	      	case SNDCTL_DSP_SETTRIGGER:
	      	case SNDCTL_DSP_GETIPTR:
	      	case SNDCTL_DSP_GETOPTR:
	      	case SNDCTL_DSP_MAPINBUF:
	      	case SNDCTL_DSP_MAPOUTBUF:
	      	case SNDCTL_DSP_SETSYNCRO:
	      	case SNDCTL_DSP_SETDUPLEX:
		  	return -ENOSYS;
	  	default:
		  	return smdk2410_mixer_ioctl(inode, file, cmd, arg);
	}

	return 0;
}


static int smdk2410_audio_open(struct inode *inode, struct file *file)
{
	int cold = !audio_active;

	DPRINTK("audio_open\n");

	if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
		if (audio_rd_refcount || audio_wr_refcount)
			return -EBUSY;
		audio_rd_refcount++;
	} else if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		if (audio_rd_refcount || audio_wr_refcount)
			return -EBUSY;
		audio_wr_refcount++;
	} else if ((file->f_flags & O_ACCMODE) == O_RDWR) {
		if (audio_rd_refcount || audio_wr_refcount)
			return -EBUSY;
		audio_rd_refcount++;
		audio_wr_refcount++;
	} else
		return -EINVAL;

	if (cold) {
		
		audio_rate = AUDIO_RATE_DEFAULT;
		audio_channels = AUDIO_CHANNELS_DEFAULT;
		audio_fragsize = AUDIO_FRAGSIZE_DEFAULT;
		audio_nbfrags = AUDIO_NBFRAGS_DEFAULT;
		if ((file->f_mode & FMODE_WRITE)){
				init_s3c2410_iis_bus_tx();
				audio_clear_buf(&output_stream);
		}
		if ((file->f_mode & FMODE_READ)){
				init_s3c2410_iis_bus_rx();
				audio_clear_buf(&input_stream);
		}
	}
  
	return 0;
}

static int smdk2410_audio_release(struct inode *inode, struct file *file)
{
	DPRINTK("audio_release\n");
  
	if (file->f_mode & FMODE_READ) {
		  	if (audio_rd_refcount == 1) {
				audio_clear_buf(&input_stream);
			  	audio_rd_refcount = 0;
			}
	}

	if(file->f_mode & FMODE_WRITE) {
		  	if (audio_wr_refcount == 1) {
			    	audio_sync(file);
			    	audio_clear_buf(&output_stream);
			    	audio_wr_refcount = 0;
		    	}
	  	}

	return 0;
}

static struct file_operations smdk2410_audio_fops = {
	.owner		= THIS_MODULE,
	.llseek		= smdk2410_audio_llseek,
	.write		= smdk2410_audio_write,
	.read		= smdk2410_audio_read,
	.poll			= smdk2410_audio_poll,
	.ioctl			= smdk2410_audio_ioctl,
	.open		= smdk2410_audio_open,
	.release		= smdk2410_audio_release,
};

static int smdk2410_mixer_open(struct inode *inode, struct file *file)
{
	
	return 0;
}

static int smdk2410_mixer_release(struct inode *inode, struct file *file)
{
	
	return 0;
}

static struct file_operations smdk2410_mixer_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.ioctl		= smdk2410_mixer_ioctl,
	.open		= smdk2410_mixer_open,
	.release	= smdk2410_mixer_release,
};

static void init_uda1341(void)
{
	unsigned long flags;
	unsigned char uda_sampling;

  	uda1341_volume = 62 - ((DFT_VOLUME * 61) / 100);
	uda1341_boost = 0;
  	uda_sampling = DATA2_DEEMP_NONE;
	uda_sampling &= ~(DATA2_MUTE);

	local_irq_save(flags);
	L3MOD_HI();
	L3CLK_HI();
	local_irq_restore(flags);
	
	uda1341_l3_address(UDA1341_REG_STATUS);
	uda1341_l3_data(0x60);				//reset uda1341	
	uda1341_l3_data(STAT0_SC_384FS | STAT0_IF_MSB);     // set 384 system clock, MSB
	uda1341_l3_data(STAT1 | STAT1_DAC_GAIN | STAT1_ADC_GAIN | STAT1_ADC_ON | STAT1_DAC_ON);

    uda1341_l3_address(UDA1341_REG_DATA0);
	uda1341_l3_data(DATA0 |DATA0_VOLUME(uda1341_volume));  // maximum volume
	uda1341_l3_data(DATA1 |DATA1_BASS(uda1341_boost)| DATA1_TREBLE(0));
    uda1341_l3_data(uda_sampling);
	uda1341_l3_data(EXTADDR(EXT2));
	uda1341_l3_data(EXTDATA(EXT2_MIC_GAIN(0x6)) | EXT2_MIXMODE_CH1);
	
}

static int __init s3c2410_uda1341_probe(struct device *dev)
{
	unsigned long flags;
	printk("just enter s3c2410_uda1341_probe()\n");	
	
	this_dev = dev;
	
	printk("S3C2410-UDA1341 Probe\n");	

	local_irq_save(flags);

	//L3 mode=GPB2, dat=GPB3, clk=GPB4, output
	__raw_writel(__raw_readl(S3C2410_GPBCON)&~(0x3f<<4), S3C2410_GPBCON);

	__raw_writel(__raw_readl(S3C2410_GPBCON)|(0x15<<4), S3C2410_GPBCON);
	printk(KERN_ERR "line %d passed! have set L3 mode=GPB2, dat=GPB3, clk=GPB4, output\n",__LINE__);
	//IISLRCK=GPE0, IISSCLK=GPE1, CDCLK=GPE2, IISSDI=GPE3, IISSDO=GPE4
	__raw_writel(__raw_readl(S3C2410_GPECON)&~0x3ff, S3C2410_GPECON);
	
	__raw_writel(__raw_readl(S3C2410_GPECON)|0x2aa, S3C2410_GPECON);
printk(KERN_ERR "line %d passed! \nhave set IISLRCK=GPE0, IISSCLK=GPE1, CDCLK=GPE2, IISSDI=GPE3, IISSDO=GPE4\n",__LINE__);
	local_irq_restore(flags);
	
	__raw_writel(__raw_readl(S3C2410_CLKCON)|(1<<17), S3C2410_CLKCON);		//Enable IIS clock
printk(KERN_ERR "line %d passed! have Enable IIS clock\n",__LINE__);
	init_uda1341();

/*	output_stream.dma_ch = DMA_CH2;
	if (audio_init_dma(&output_stream, "UDA1341 out")) {
		audio_clear_dma(&output_stream);
		printk( KERN_WARNING AUDIO_NAME_VERBOSE 
			": unable to get DMA channels\n" );
		return -EBUSY;
	}*/
/*	input_stream.dma_ch = DMA_CH1;
	if (audio_init_dma(&input_stream, "UDA1341 in")) {
		audio_clear_dma(&input_stream);
		printk( KERN_WARNING AUDIO_NAME_VERBOSE
			": unable to get DMA channels\n" );
		return -EBUSY;
	}*/
	
	if(request_irq(IRQ_DMA2, iisout_dma_done_handler, SA_INTERRUPT, 
					"IIS Play", NULL) < 0) {
		printk( KERN_ERR AUDIO_NAME_VERBOSE
					": unable to get DMA interrupts for play\n" );
		return -EBUSY;
	}else printk(KERN_ERR "success register irq: iisout_dma_done_handler\n" );
	if(request_irq(IRQ_DMA1, iisin_dma_done_handler, SA_INTERRUPT, 
					"IIS Record", NULL) < 0) {
		free_irq(IRQ_DMA2, NULL);
		printk( KERN_WARNING AUDIO_NAME_VERBOSE
					": unable to get DMA interrupts for record\n" );
		return -EBUSY;
	}else printk(KERN_ERR "success register irq: iisin_dma_done_handler\n" );
					
	audio_dev_dsp = register_sound_dsp(&smdk2410_audio_fops, -1);
	audio_dev_mixer = register_sound_mixer(&smdk2410_mixer_fops, -1);

	printk(AUDIO_NAME_VERBOSE " initialized\n");

	return 0;
}

static int s3c2410_uda1341_remove(struct device *dev)
{	
	
	printk(KERN_ERR "just enter s3c2410_uda1341_remove()\n");
	unregister_sound_dsp(audio_dev_dsp);
	unregister_sound_mixer(audio_dev_mixer);
//	audio_clear_dma(&output_stream);
//	audio_clear_dma(&input_stream); /* input */
	__raw_writel(4, DMTRIG2);		//DMA channel off
	__raw_writel(4, DMTRIG1);
	__raw_writel(__raw_readl(S3C2410_IISCON)&~IISCON_EN, S3C2410_IISCON);	// IIS disable
	__raw_writel(__raw_readl(S3C2410_CLKCON)&~(1<<17), S3C2410_CLKCON);		//Disable IIS clock
	free_irq(IRQ_DMA1, NULL);
	free_irq(IRQ_DMA2, NULL);	
	printk(AUDIO_NAME_VERBOSE " unloaded\n");
	return 0;
}

static struct device_driver s3c2410_uda1341_driver = {
	.name		= "s3c2410-iis",
	.bus		= &platform_bus_type,
	.probe		= s3c2410_uda1341_probe,
	.remove		= s3c2410_uda1341_remove,
//#ifdef CONFIG_PM
	//.suspend	= s3c2410_uda1341_suspend,
	//.resume		= s3c2410_uda1341_resume,
//#endif
};

static int __init s3c2410_uda1341_init(void)
{
	int ret;
	
  printk(KERN_INFO "virtual address of s3c2410_va_iis=%08x.\n",S3C2410_IISCON);  printk(KERN_ERR "next will do driver_register!\n");
	ret = driver_register(&s3c2410_uda1341_driver);
	if(ret)
		printk("register device driver failed, return code is %d\n", ret);
	printk(KERN_ERR "driver uda1341 in /sound/oss/s3c2410-oss.c\n");
	
        return ret;
}

static void __exit s3c2410_uda1341_exit(void)
{
	
	driver_unregister(&s3c2410_uda1341_driver);
}

#ifdef	MODULE
MODULE_AUTHOR("antiscle http://www.witech.com.cn");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("S3C2410 + UDA1341TS driver for OSS");

#endif

module_init(s3c2410_uda1341_init);
module_exit(s3c2410_uda1341_exit);

