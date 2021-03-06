#ifndef SPCA50X_H
#define SPCA50X_H

/*
 * Header file for SPCA50x based camera driver. Originally copied from ov511 driver.
 * Originally by Mark W. McClelland
 * SPCA50x version by Joel Crisp; all bugs are mine, all nice features are his.
 */

#ifdef __KERNEL__
#include <asm/uaccess.h>
#include <linux/videodev.h>
#include <linux/smp_lock.h>
#include <linux/usb.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,20) && LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)

#define urb_t struct urb
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,20) */

//static const char SPCA50X_H_CVS_VERSION[]="$Id: spca50x.h,v 1.28 2004/01/10 21:37:40 mxhaard Exp $";

/* V4L API extension for raw JPEG (=JPEG without header) and JPEG with header   
 */
#define VIDEO_PALETTE_RAW_JPEG  20
#define VIDEO_PALETTE_JPEG 21

#ifdef SPCA50X_ENABLE_DEBUG

#  define PDEBUG(level, fmt, args...) \
if (debug >= level) info("[%s:%d] " fmt, __PRETTY_FUNCTION__, __LINE__ , ## args)
#else /* SPCA50X_ENABLE_DEBUG */
#  define PDEBUG(level, fmt, args...) do {} while(0)
#endif /* SPCA50X_ENABLE_DEBUG */

//#define FRAMES_PER_DESC		10	/* Default value, should be reasonable */
#define FRAMES_PER_DESC		16	/* Default value, should be reasonable */
#define MAX_FRAME_SIZE_PER_DESC 1024

#define SPCA50X_MAX_WIDTH 640
#define SPCA50X_MAX_HEIGHT 480

#define SPCA50X_ENDPOINT_ADDRESS 1	/* Isoc endpoint number */

/* only 2 or 4 frames are allowed here !!! */
#define SPCA50X_NUMFRAMES	2
#define SPCA50X_NUMSBUF	2


#define BRIDGE_SPCA504 0 //4
#define BRIDGE_SPCA504B 1 //6
#define BRIDGE_SPCA533 2 //7
#define BRIDGE_SPCA504C 3 //8
#define BRIDGE_SPCA536 4 //10
#define BRIDGE_ZC3XX 5 //12
#define BRIDGE_SN9CXXX 6 //16



#define SENSOR_INTERNAL 1
#define SENSOR_HV7131B  2
#define SENSOR_HDCS1020 3
#define SENSOR_PB100_BA 4
#define SENSOR_PB100_92	5
#define SENSOR_PAS106_80 6
#define SENSOR_TAS5130C 7
#define SENSOR_ICM105A 8
#define SENSOR_HDCS2020 9
#define SENSOR_PAS106 10
#define SENSOR_PB0330 11
#define SENSOR_HV7131C 12
#define SENSOR_CS2102 13
#define SENSOR_HDCS2020b 14
#define SENSOR_HV7131R 15
#define SENSOR_OV7630 16
#define SENSOR_MI0360 17
#define SENSOR_TAS5110 18
#define SENSOR_PAS202 19


/* Alternate interface transfer sizes */
#define SPCA50X_ALT_SIZE_0       0
#define SPCA50X_ALT_SIZE_128     1
#define SPCA50X_ALT_SIZE_256     1
#define SPCA50X_ALT_SIZE_384     2
#define SPCA50X_ALT_SIZE_512     3
#define SPCA50X_ALT_SIZE_640     4
#define SPCA50X_ALT_SIZE_768     5
#define SPCA50X_ALT_SIZE_896     6
#define SPCA50X_ALT_SIZE_1023    7

/* Sequence packet identifier for a dropped packet */
#define SPCA50X_SEQUENCE_DROP 0xFF

/* Type bit for 10 byte header snapshot flag */
#define SPCA50X_SNAPBIT 0x40
#define SPCA50X_SNAPCTRL 0x80

/* Offsets into the 10 byte header on the first ISO packet */
#define SPCA50X_OFFSET_SEQUENCE 0

/* Generic frame packet header offsets */
#define SPCA50X_OFFSET_TYPE     1
#define SPCA50X_OFFSET_COMPRESS 2
#define SPCA50X_OFFSET_THRESHOLD 3
#define SPCA50X_OFFSET_QUANT 4
#define SPCA50X_OFFSET_QUANT2 5
#define SPCA50X_OFFSET_FRAMSEQ 6
#define SPCA50X_OFFSET_EDGE_AUDIO 7
#define SPCA50X_OFFSET_GPIO 8
#define SPCA50X_OFFSET_RESERVED 9
#define SPCA50X_OFFSET_DATA 10

/* Camera type jpeg yuvy yyuv yuyv grey gbrg*/
enum {  
	JPEG = 0, //Jpeg 4.1.1 Sunplus
	JPGH, //jpeg 4.2.2 Zstar
	JPGC, //jpeg 4.2.2 Conexant
	JPGS, //jpeg 4.2.2 Sonix
	JPGM, //jpeg 4.2.2 Mars-Semi
};

enum { QCIF = 1,
       QSIF,
       QPAL,
       CIF,
       SIF,
       PAL,
       VGA,
       CUSTOM,
       TOTMODE,
};
       
/* available palette */       
#define P_RGB16  1
#define P_RGB24  (1 << 1)
#define P_RGB32  (1 << 2)
#define P_YUV420  (1 << 3)
#define P_YUV422 ( 1 << 4)
#define P_RAW  (1 << 5)
#define P_JPEG  (1 << 6)

struct mwebcam {
	int width;
	int height;
	__u16 t_palette;
	__u16 pipe;
	int method;
	int mode;
};

/* State machine for each frame in the frame buffer during capture */
enum {
	STATE_SCANNING,		/* Scanning for start */
	STATE_HEADER,		/* Parsing header */
	STATE_LINES,		/* Parsing lines */
};

/* Buffer states */
enum {
	BUF_NOT_ALLOCATED,
	BUF_ALLOCATED,
	BUF_PEND_DEALLOC,	/* spca50x->buf_timer is set */
};

struct usb_device;

/* One buffer for the USB ISO transfers */
struct spca50x_sbuf {
	char       *data;
	struct urb *urb;
};

/* States for each frame buffer. */
enum {
	FRAME_UNUSED,		/* Unused (no MCAPTURE) */
	FRAME_READY,		/* Ready to start grabbing */
	FRAME_GRABBING,		/* In the process of being grabbed into */
	FRAME_DONE,		/* Finished grabbing, but not been synced yet */
	FRAME_ERROR,		/* Something bad happened while processing */
	FRAME_ABORTING,         /* Aborting everything. Caused by hot unplugging.*/

};
/************************ decoding data  **************************/
struct dec_data {	
	unsigned char quant[3][64];
};
/*************************End decoding data ********************************/	
struct spca50x_frame {
	unsigned char *data;		/* Frame buffer */
	unsigned char *tmpbuffer;	/* temporary buffer spca50x->tmpbuffer need for decoding*/
	struct dec_data *decoder;
	/*******************************************/
	int seq;                /* Frame sequence number */
	int depth;		/* Bytes per pixel */
	int width;		/* Width application is expecting */
	int height;		/* Height */

	int hdrwidth;		/* Width the frame actually is */
	int hdrheight;		/* Height */
	int method;		/* The decoding method for that frame 0 nothing 1 crop 2 div 4 mult */
	int cropx1;		/* value to be send with the frame for decoding feature */
	int cropx2;
	int cropy1;
	int cropy2;
	int x;
	int y;
	
	unsigned int format;	/* Format asked by apps for this frame */
	int cameratype;		/* native in frame format */
	volatile int grabstate;	/* State of grabbing */
	int scanstate;		/* State of scanning */	
	long scanlength;	/* uncompressed, raw data length of frame */
	int totlength;		/* length of the current reading byte in the Iso stream */
	wait_queue_head_t wq;	/* Processes waiting */
	int last_packet;        /* sequence number for last packet */
	unsigned char *highwater; /* used for debugging */
	
};


struct usb_spca50x {
	struct video_device *vdev;
	struct usb_device *dev;/* Device structure */
	struct tasklet_struct spca5xx_tasklet; /* use a tasklet per device */
	struct dec_data maindecode;
	unsigned char iface; /* interface in use */
	int alt; /* current alternate setting */
	int customid; /* product id get by probe */
	int desc; /* enum camera name */
	int ccd; /* If true, using the CCD otherwise the external input */
	int chip_revision; /* set when probe the camera spca561 */
	struct mwebcam mode_cam[TOTMODE]; /* all available mode registers by probe */	
	int bridge;		/* Type of bridge (BRIDGE_SPCA505 or BRIDGE_SPCA506) */
	int sensor;		/* Type of image sensor chip */
	int packet_size;	/* Frame size per isoc desc */
	int header_len;
	/* Determined by sensor type */
	int maxwidth;
	int maxheight;
	int minwidth;
	int minheight;
	/* What we think the hardware is currently set to */
	int brightness;
	int colour;
	int contrast;
	int hue;
	int whiteness;
	int exposure ;
	int width; /* use here for the init of each frame */
	int height;
	int hdrwidth;
	int hdrheight;
	unsigned int format;
	int method; /* method ask for output pict */
	int mode; /* requested frame size */
	int pipe_size; // requested pipe size set according to mode
	__u16 norme; /* norme in use Pal Ntsc Secam */
	__u16 channel; /* input composite video1 or svideo */
	int cameratype;	/* native in frame format */
	/* Statistics variables */
	spinlock_t v4l_lock; /* lock to protect shared data between isoc and process context */
	int avg_lum; //The average luminance (if available from theframe header)
	int avg_bg, avg_rg; //The average B-G and R-G for white balancing 
	struct semaphore lock;
	int user;		/* user count for exclusive use */
	int present;		/* driver loaded */
	
	int streaming;		/* Are we streaming Isochronous? */
	int grabbing;		/* Are we grabbing? */
	int packet;
	int compress;		/* Should the next frame be compressed? */
	
	char *fbuf;		/* Videodev buffer area */	
	int curframe;		/* Current receiving frame buffer */
	struct spca50x_frame frame[SPCA50X_NUMFRAMES];	
	int cursbuf;		/* Current receiving sbuf */
	struct spca50x_sbuf sbuf[SPCA50X_NUMSBUF];
	/* Temporary jpeg decoder workspace */
	char   *tmpBuffer;
	/* Framebuffer/sbuf management */
	int buf_state;
	struct semaphore buf_lock;
	
	wait_queue_head_t wq;	/* Processes waiting */		
	/* proc interface */
	struct semaphore param_lock;	/* params lock for this camera */

		
	int lastFrameRead;	
	uint i2c_ctrl_reg; // Camera I2C control register
	uint i2c_base;     // Camera I2C address base
	char i2c_trigger_on_write; //do trigger bit on write
	
	__u8 force_rgb; //Read RGB instead of BGR
	__u8 min_bpp; //The minimal color depth that may be set
	__u8 lum_level; //Luminance level for brightness autoadjustment
};

struct cam_list {
	int id;
	const char *description;
};

struct palette_list {
	int num;
	const char *name;
};

struct bridge_list {
	int num;
	const char *name;
};

#endif /* __KERNEL__ */


#endif /* SPCA50X_H */
