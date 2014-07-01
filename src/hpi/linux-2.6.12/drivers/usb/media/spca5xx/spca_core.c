/*
 * SPCA5xx based usb camera driver (currently supports
 * yuv native stream spca501a, spca501c, spca505, spca508, spca506
 * jpeg native stream spca500, spca551, spca504a, spca504b, spca533a, spca536a, zc0301, zc0302, cx11646, sn9c102p
 * bayer native stream spca561a, sn9c101, sn9c102, tv8532 ).
 * Z-star Vimicro chips zc0301 zc0301P zc0302
 * Sunplus spca501a, spca501c, spca505, spca508, spca506, spca500, spca551, spca504a, spca504b, spca533a, spca536a
 * Sonix sn9c101, sn9c102, sn9c102p sn9c105 sn9c120
 * Conexant cx11646
 * Transvision tv_8532 
 * Etoms Et61x151 Et61x251
 * Pixat Pac207-BCA-32
 * SPCA5xx version by Michel Xhaard <mxhaard@users.sourceforge.net>
 * Based on :
 * SPCA50x version by Joel Crisp <cydergoth@users.sourceforge.net>
 * OmniVision OV511 Camera-to-USB Bridge Driver
 * Copyright (c) 1999-2000 Mark W. McClelland
 * Kernel 2.6.x port Michel Xhaard && Reza Jelveh (feb 2004)
 * Based on the Linux CPiA driver written by Peter Pregler,
 * Scott J. Bertin and Johannes Erdfelt.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#define SPCA5XX_VERSION "00.57.06LE"
#define VID_HARDWARE_SPCA5XX 0xFF


static const char version[] = SPCA5XX_VERSION;


#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>


#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/pagemap.h>
#include <linux/usb.h>
#include <asm/io.h>
#include <asm/semaphore.h>
#include <linux/videodev.h>

#include <asm/page.h>
#include <asm/uaccess.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#include <linux/wrapper.h>
#endif
#include <linux/param.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
#include <linux/moduleparam.h>
#endif

#include "spca5xx.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#  undef CONFIG_VIDEO_PROC_FS
#	 undef CONFIG_PROC_FS
#endif

//#define RH9_REMAP 1

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#include "spcaCompat.h"
#ifndef pte_offset_kernel
# define pte_offset_kernel(dir, address)	pte_offset(dir, address)
#endif
#endif


#include "spcadecoder.h"


#define PROC_NAME_LEN 10	//length of the proc name



/* Video Size 640 x 480 jpeg only */
#define MAX_FRAME_SIZE (640 * 480)
#define MAX_DATA_SIZE (MAX_FRAME_SIZE + sizeof(struct timeval))



/* Hardware auto exposure / whiteness (PC-CAM 600) */
static int autoexpo = 1;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,5)
/* Video device number (-1 is first available) */
static int video_nr = -1;
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,5) */
#ifdef SPCA50X_ENABLE_DEBUG
/* 0=no debug messages
 * 1=init/detection/unload and other significant messages,
 * 2=some warning messages
 * 3=config/control function calls
 * 4=most function calls and data parsing messages
 * 5=highly repetitive mesgs
 * NOTE: This should be changed to 0, 1, or 2 for production kernels
 */
 
static int debug = 0;
#endif
/* Force image to be read in RGB instead of BGR. This option allow
 * programs that expect RGB data (e.g. gqcam) to work with this driver. */


/* Enable compression. This is for experimentation only; compressed images
 * still cannot be decoded yet. */
static int compress = 0;


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)

module_param (autoexpo, int, 0644);
#ifdef SPCA50X_ENABLE_DEBUG
module_param (debug, int, 0644);
#endif

#ifdef SPCA50X_ENABLE_COMPRESSION
module_param (compress, int, 0644);
#endif /* SPCA50X_ENABLE_COMPRESSION */



#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9) */


MODULE_PARM (autoexpo, "i");
#ifdef SPCA50X_ENABLE_DEBUG
MODULE_PARM (debug, "i");
#endif

#ifdef SPCA50X_ENABLE_COMPRESSION
MODULE_PARM (compress, "i");
#endif /* SPCA50X_ENABLE_COMPRESSION */

#endif
/***************/


MODULE_PARM_DESC (autoexpo,
		  "Enable/Disable hardware auto exposure / whiteness (default: enabled) (PC-CAM 600 only !!)");
#ifdef SPCA50X_ENABLE_DEBUG
MODULE_PARM_DESC (debug,
		  "Debug level: 0=none, 1=init/detection, 2=warning, 3=config/control, 4=function call, 5=max");
#endif

#ifdef SPCA50X_ENABLE_COMPRESSION
MODULE_PARM_DESC (compress, "Turn on/off compression (not functional yet)");
#endif /* SPCA50X_ENABLE_COMPRESSION */

/****************/
MODULE_AUTHOR
  ("Michel Xhaard <mxhaard@users.sourceforge.net> based on spca50x driver by Joel Crisp <cydergoth@users.sourceforge.net>,ov511 driver by Mark McClelland <mwm@i.am>");
MODULE_DESCRIPTION ("SPCA5LE USB Camera Driver");
MODULE_LICENSE ("GPL");



static int spca50x_move_data (struct usb_spca50x *spca50x, struct urb *urb);



static struct usb_driver spca5xx_driver;

#ifndef max
static inline int
max (int a, int b)
{
  return (a > b) ? a : b;
}
#endif /* max */

/**********************************************************************
 * List of known SPCA50X-based cameras
 **********************************************************************/
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0)
/* Camera type jpeg yuvy yyuv yuyv grey gbrg*/
static struct palette_list Plist[] = {
  {JPEG, "JPEG"},
  {JPGH, "JPEG"},
  {JPGC, "JPEG"},
  {JPGS, "JPEG"},
  {JPGM, "JPEG"},
  {-1, NULL}
};
#endif

static struct bridge_list Blist[] = {
  {BRIDGE_SPCA504, "SPCA504"},
  {BRIDGE_SPCA504B, "SPCA504B"},
  {BRIDGE_SPCA533, "SPCA533"},
  {BRIDGE_SPCA504C, "SPCA504C"},
  {BRIDGE_SPCA536, "SPCA536"},
  {BRIDGE_ZC3XX, "ZC301-2"},
  {BRIDGE_SN9CXXX,"SN9CXXX"},
  {-1, NULL}
};
enum
{
  UnknownCamera = 0,		// 0
  MustekGsmartMini2,
  MustekGsmartMini3,
  CreativePCCam600,
  MaxellMaxPocket,
  AiptekMiniPenCam2,
  AiptekPocketDVII,
  AiptekPenCamSD,
  AiptekMiniPenCam13,
  MustekGsmartLCD3,
  MustekMDC5500Z, //10
  MegapixV4,
  AiptekPocketDV3100,
  AiptekPocketCam3M,
  MustekGsmartLCD2,
  PureDigitalDakota,
  BenqDC1500,
  LogitechClickSmart420,
  BenqDC1300,
  MustekDV3000,
  CreativePccam750,//20
  BenqDC3410,
  Jenoptikjdc21lcd,
  Terratec2move13,
  MustekDV4000,
  AiptekDV3500,
  LogitechClickSmart820,
  Enigma13,
  Epsilon13,
  AiptekPocketCam2M,
  PolaroidPDC2030,//30
  CreativeNotebook,
  CreativeMobile,
  LabtecPro,
  MustekWcam300A,
  GeniusVideoCamV2,
  GeniusVideoCamV3,
  GeniusVideoCamExpressV2b,
  CreativeNxPro,
  Vimicro,
  Digitrex2110,//40
  GsmartD30,
  CreativeNxPro2,
  Bs888e,
  Zc302,
  AiptekSlim3200,
  CreativeLive,	
  MercuryDigital,
  Wcam300A,
  TyphoonWebshotIIUSB300k,//50
  PolaroidPDC3070,
  QCim,
  WebCam320,
  AiptekPocketCam4M,
  AiptekPocketDV5100,
  AiptekPocketDV5300,
  SunplusGeneric536,
  QCimA1,
  QCchat,
  QCimB9,//60
  SonixWC311P,
  Concord3045,
  Mercury21,
  CreativeNX,
  CreativeInstant1,
  CreativeInstant2,
  QuickCamNB,
  WCam300AN,
  GeniusDsc13,
  MustekMDC4000,//70
  LogitechQCCommunicateSTX,
  Pccam168,
  Sn535,
  Pccam,
  Lic300,
  PolaroidIon80,
  Zc0305b,
  LogitechNotebookDeluxe,
  LabtecNotebook,
  JvcGcA50,//80
  PcCam350,
  Vimicro303b,
  CyberpixS550V,
  LastCamera
};

static struct cam_list clist[] = {
  {UnknownCamera, "Unknown"},
  {MustekGsmartMini2, "Mustek gSmart mini 2"},
  {MustekGsmartMini3, "Mustek gSmart mini 3"},
  {CreativePCCam600, "Creative PC-CAM 600"},
  {MaxellMaxPocket, "Maxell Max Pocket LEdit. 1.3 MPixels"},
  {AiptekMiniPenCam2, "Aiptek Mini PenCam  2 MPixels"},
  {AiptekPocketDVII, "Aiptek PocketDVII  1.3 MPixels"},
  {AiptekPenCamSD, "Aiptek Pencam SD  2 MPixels"},
  {AiptekMiniPenCam13, "Aiptek mini PenCam 1.3 MPixels"},
  {MustekGsmartLCD3, "Mustek Gsmart LCD 3"},
  {MustekMDC5500Z, "Mustek MDC5500Z"},
  {MegapixV4, "Megapix V4"},
  {AiptekPocketDV3100, "Aiptek PocketDV3100+ "},
  {AiptekPocketCam3M, "Aiptek PocketCam  3 M "},
  {MustekGsmartLCD2, "Mustek Gsmart LCD 2"},
  {PureDigitalDakota, "Pure Digital Dakota"},
  {BenqDC1500, "Benq DC1500"},
  {LogitechClickSmart420, "Logitech Inc. ClickSmart 420"},
  {BenqDC1300, "Benq DC1300"}, 
  {MustekDV3000, "Mustek DV 3000"},
  {CreativePccam750, "Creative PCcam750"}, 
  {BenqDC3410, "Benq DC3410"}, 
  {Jenoptikjdc21lcd, "Jenoptik DC 21 LCD"}, 
  {Terratec2move13, "Terratec 2 move 1.3"},
  {MustekDV4000, "Mustek DV4000 Mpeg4"},
  {AiptekDV3500, "Aiptek DV3500 Mpeg4"},
  {LogitechClickSmart820, "Logitech ClickSmart 820"},
  {Enigma13, "Digital Dream Enigma 1.3"},
  {Epsilon13, "Digital Dream Epsilon 1.3"},
  {AiptekPocketCam2M, "Aiptek PocketCam 2Mega"},
  {PolaroidPDC2030, "Polaroid PDC2030"},
  {CreativeNotebook, "Creative Notebook PD1171"},
  {CreativeMobile, "Creative Mobile PD1090"},
  {LabtecPro, "Labtec Webcam Pro"},
  {MustekWcam300A, "Mustek Wcam300A"},
  {GeniusVideoCamV2, "Genius Videocam V2"},
  {GeniusVideoCamV3, "Genius Videocam V3"},
  {GeniusVideoCamExpressV2b, "Genius Videocam Express V2 Firmware 2"},
  {CreativeNxPro, "Creative Nx Pro"}, 
  {Vimicro, "Z-star Vimicro zc0301p"},
  {Digitrex2110, "ApexDigital Digitrex2110 spca533"},
  {GsmartD30, "Mustek Gsmart D30 spca533"},
  {CreativeNxPro2, "Creative NX Pro FW2"},
  {Bs888e, "Kowa Bs888e MicroCamera"},
  {Zc302, "Z-star Vimicro zc0302"},
  {AiptekSlim3200, "Aiptek Slim 3200"},
  {CreativeLive, "Creative Live! "},
  {MercuryDigital, "Mercury Digital Pro 3.1Mp"},
  {Wcam300A, "Mustek Wcamm300A 2"},
  {TyphoonWebshotIIUSB300k, " Typhoon Webshot II"},
  {PolaroidPDC3070, " Polaroid PDC3070"}, 
  {QCim,"Logitech QuickCam IM"},
  {WebCam320,"Micro Innovation WebCam 320"},
  {AiptekPocketCam4M,"Aiptek Pocket Cam 4M"},
  {AiptekPocketDV5100,"Aiptek Pocket DV5100"},
  {AiptekPocketDV5300,"Aiptek Pocket DV5300"},
  {SunplusGeneric536,"Sunplus Generic spca536a"},
  {QCimA1,"Logitech QuickCam IM + sound"},
  {QCchat,"Logitech QuickCam chat"},
  {QCimB9,"Logitech QuickCam IM ???"}, 
  {SonixWC311P,"Sonix sn9c102P Hv7131R"},
  {Concord3045,"Concord 3045 spca536a"},
  {Mercury21,"Mercury Peripherals Inc."},
  {CreativeNX,"Creative NX"},
  {CreativeInstant1,"Creative Instant P0620"},
  {CreativeInstant2,"Creative Instant P0620D"},
  {QuickCamNB,"Logitech QuickCam for Notebooks"},
  {WCam300AN,"Mustek WCam300AN "},
  {GeniusDsc13,"Genius Dsc 1.3 Smart spca504B-P3"},
  {MustekMDC4000, "Mustek MDC4000"},
  {LogitechQCCommunicateSTX,"Logitech QuickCam Communicate STX"}, 
  {Pccam168,"Sonix PcCam"},
  {Sn535,"Sangha 350k"},
  {Pccam,"Sonix Pccam +"},
  {Lic300,"LG Lic-300"},
  {PolaroidIon80,"Polaroid Ion 80"},
  {Zc0305b,"Generic Zc0305b"},
  {LogitechNotebookDeluxe,"Logitech Notebook Deluxe"},
  {LabtecNotebook,"Labtec Webcam Notebook"},
  {JvcGcA50,"JVC GC-A50"}, 
  {PcCam350,"PC-Cam350"}, 
  {Vimicro303b,"Generic Vimicro 303b"},
  {CyberpixS550V,"Mercury Cyberpix S550V"},
  {-1, NULL}
};


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,3,0)
static __devinitdata struct usb_device_id device_table[] = {
  {USB_DEVICE (0x055f, 0xc420)},	/* Mustek gSmart Mini 2 */
  {USB_DEVICE (0x055f, 0xc520)},	/* Mustek gSmart Mini 3 */
  {USB_DEVICE (0x041E, 0x400B)},	/* Creative PC-CAM 600 */
  {USB_DEVICE (0x04fc, 0x504b)},	/* Maxell MaxPocket LE 1.3 */
  {USB_DEVICE (0x08ca, 0x2008)},	/* Aiptek Mini PenCam 2 M */
  {USB_DEVICE (0x08ca, 0x0104)},	/* Aiptek PocketDVII 1.3 */
  {USB_DEVICE (0x08ca, 0x2018)},	/* Aiptek Pencam SD 2M */
  {USB_DEVICE (0x04fc, 0x504a)},	/* Aiptek Mini PenCam 1.3 */
  {USB_DEVICE (0x055f, 0xc530)},	/* Mustek Gsmart LCD 3 */
  {USB_DEVICE (0x055f, 0xc650)},	/* Mustek MDC5500Z */
  {USB_DEVICE (0x052b, 0x1513)},	/* Megapix V4 */
  {USB_DEVICE (0x08ca, 0x0106)},	/* Aiptek Pocket DV3100+ */
  {USB_DEVICE (0x08ca, 0x2010)},	/* Aiptek PocketCam 3M */
  {USB_DEVICE (0x055f, 0xc430)},	/* Mustek Gsmart LCD 2 */
  {USB_DEVICE (0x04fc, 0xffff)},	/* Pure DigitalDakota */
  {USB_DEVICE (0x04a5, 0x3008)},	/* Benq DC 1500 */
  {USB_DEVICE (0x046d, 0x0960)},	/* Logitech Inc. ClickSmart 420 */
  {USB_DEVICE (0x04a5, 0x3003)},	/* Benq DC 1300 */
  {USB_DEVICE (0x055f, 0xc440)},	/* Mustek DV 3000 */
  {USB_DEVICE (0x041e, 0x4013)},	/* Creative Pccam750 */
  {USB_DEVICE (0x04a5, 0x300a)},	/* Benq DC3410 */
  {USB_DEVICE (0x055f, 0xc220)},	/* Gsmart Mini */
  {USB_DEVICE (0x0733, 0x2211)},	/* Jenoptik jdc 21 LCD */
  {USB_DEVICE (0x055f, 0xc360)},	/* Mustek DV4000 Mpeg4  */
  {USB_DEVICE (0x08ca, 0x2024)},	/* Aiptek DV3500 Mpeg4  */
  {USB_DEVICE (0x046d, 0x0905)},	/* Logitech ClickSmart820  */
  {USB_DEVICE (0x05da, 0x1018)},	/* Digital Dream Enigma 1.3 */
  {USB_DEVICE (0x0733, 0x1311)},	/* Digital Dream Epsilon 1.3 */
  {USB_DEVICE (0x041e, 0x401d)},	/* Creative Webcam NX ULTRA */
  {USB_DEVICE (0x08ca, 0x2016)},	/* Aiptek PocketCam 2 Mega */
  {USB_DEVICE (0x0546, 0x3273)},	/* Polaroid PDC2030 */
  {USB_DEVICE (0x041e, 0x401f)},	/* Creative Webcam Notebook PD1171 */
  {USB_DEVICE (0x041e, 0x4017)},	/* Creative Webcam Mobile PD1090 */
  {USB_DEVICE (0x046d, 0x08a2)},	/* Labtec Webcam Pro */
  {USB_DEVICE (0x055f, 0xd003)},	/* Mustek WCam300A */
  {USB_DEVICE (0x0458, 0x7007)},	/* Genius VideoCam V2 */
  {USB_DEVICE (0x0458, 0x700c)},	/* Genius VideoCam V3 */
  {USB_DEVICE (0x0458, 0x700f)},	/* Genius VideoCam Web V2 */
  {USB_DEVICE (0x041e, 0x401e)},	/* Creative Nx Pro */
  {USB_DEVICE (0x04fc, 0x5330)},	/* Digitrex 2110 */
  {USB_DEVICE (0x055f, 0xc540)},	/* Gsmart D30 */
  {USB_DEVICE (0x0ac8, 0x301b)},	/* Asam Vimicro */
  {USB_DEVICE (0x041e, 0x403a)},	/* Creative Nx Pro 2 */
  {USB_DEVICE (0x055f, 0xc211)},	/* Kowa Bs888e Microcamera */
  {USB_DEVICE (0x0ac8, 0x0302)},	/* Z-star Vimicro zc0302 */
  {USB_DEVICE (0x08ca, 0x2022)},	/* Aiptek Slim 3200 */
  {USB_DEVICE (0x0733, 0x2221)},	/* Mercury Digital Pro 3.1p */
  {USB_DEVICE (0x041e, 0x4036)},	/* Creative Live ! */
  {USB_DEVICE (0x055f, 0xc005)},	/* Mustek Wcam300A */
  {USB_DEVICE (0x10fd, 0x8050)},	/* Typhoon Webshot II USB 300k */
  {USB_DEVICE (0x0546, 0x3155)},	/* Polaroid PDC3070 */
  {USB_DEVICE (0x046d, 0x08a0)},	/* Logitech QC IM */
  {USB_DEVICE (0x0461, 0x0a00)},	/* MicroInnovation WebCam320 */
  {USB_DEVICE (0x08ca, 0x2028)},	/* Aiptek PocketCam4M */
  {USB_DEVICE (0x08ca, 0x2042)},	/* Aiptek PocketDV5100 */
  {USB_DEVICE (0x08ca, 0x2060)},	/* Aiptek PocketDV5300 */
  {USB_DEVICE (0x04fc, 0x5360)},	/* Sunplus Generic */
  {USB_DEVICE (0x046d, 0x08a1)},	/* Logitech QC IM 0x08A1 +sound*/
  {USB_DEVICE (0x046d, 0x08a3)},	/* Logitech QC Chat */
  {USB_DEVICE (0x046d, 0x08b9)},	/* Logitech QC IM ??? */
  {USB_DEVICE (0x10fd, 0x0128)},	/* Typhoon Webshot II USB 300k 0x0128 */
  {USB_DEVICE (0x0c45, 0x607c)},	/* Sonix sn9c102p Hv7131R*/
  {USB_DEVICE (0x0733, 0x3261)},	/* Concord 3045 spca536a*/
  {USB_DEVICE (0x0733, 0x1314)},        /* Mercury 2.1MEG Deluxe Classic Cam*/
  {USB_DEVICE (0x041e, 0x401c)},	/* Creative NX */
  {USB_DEVICE (0x041e, 0x4034)},	/* Creative Instant P0620 */
  {USB_DEVICE (0x041e, 0x4035)},	/* Creative Instant P0620D */
  {USB_DEVICE (0x046d, 0x08ae)},	/* Logitech QuickCam for Notebooks */
  {USB_DEVICE (0x055f, 0xd004)},	/* Mustek WCam300 AN */
  {USB_DEVICE (0x0458, 0x7006)},	/* Genius Dsc 1.3 Smart */
  {USB_DEVICE (0x055f, 0xc630)},	/* Mustek MDC4000 */
  {USB_DEVICE (0x046d, 0x08ad)},	/* Logitech QCCommunicate STX*/
  {USB_DEVICE (0x0c45, 0x613c)},	/* Sonix Pccam168 */
  {USB_DEVICE (0x0c45, 0x6130)},	/* Sonix Pccam */
  {USB_DEVICE (0x0c45, 0x60c0)},	/* Sangha Sn535 */
  {USB_DEVICE (0x0c45, 0x60fc)},	/* LG-LIC300 */
  {USB_DEVICE (0x0546, 0x3191)},	/* Polaroid Ion 80 */
  {USB_DEVICE (0x0ac8, 0x305b)},	/* Z-star Vimicro zc0305b */
  {USB_DEVICE (0x046d, 0x08a9)},	/* Logitech Notebook Deluxe*/
  {USB_DEVICE (0x046d, 0x08aa)},	/* Labtec Webcam  Notebook */
  {USB_DEVICE (0x04f1, 0x1001)},	/* JVC GC A50*/
  {USB_DEVICE (0x041e, 0x4012)},	/* PC-Cam350*/
  {USB_DEVICE (0x0ac8, 0x303b)},	/* Vimicro 0x303b*/
  {USB_DEVICE (0x0733, 0x3281)},	/* Cyberpix S550V*/
  {}				/* Terminating entry */
};


MODULE_DEVICE_TABLE (usb, device_table);
/* 
 We also setup the function for getting 
 page number from the virtual address 
*/
#define VIRT_TO_PAGE virt_to_page
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,3,0) */
#define VIRT_TO_PAGE MAP_NR
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,3,0) */
/*
 * Let's include the initialization data for each camera type
 */
#include "spcausb.h"
#include "sp5xxfw2.h"
#include "zc3xx.h"
#include "sn9cxxx.h"

/* function for the tasklet */

void outpict_do_tasklet (unsigned long ptr);

/**********************************************************************
 *
 * Memory management
 *
 * This is a shameless copy from the USB-cpia driver (linux kernel
 * version 2.3.29 or so, I have no idea what this code actually does ;).
 * Actually it seems to be a copy of a shameless copy of the bttv-driver.
 * Or that is a copy of a shameless copy of ... (To the powers: is there
 * no generic kernel-function to do this sort of stuff?)
 *
 * Yes, it was a shameless copy from the bttv-driver. IIRC, Alan says
 * there will be one, but apparentely not yet -jerdfelt
 *
 * So I copied it again for the ov511 driver -claudio
 * And again for the spca50x driver -jcrisp
 **********************************************************************/

/* Given PGD from the address space's page table, return the kernel
 * virtual mapping of the physical memory mapped at ADR.
 */
#ifndef RH9_REMAP
static inline unsigned long
uvirt_to_kva (pgd_t * pgd, unsigned long adr)
{
  unsigned long ret = 0UL;
  pmd_t *pmd;
  pte_t *ptep, pte;

  if (!pgd_none (*pgd))
    {
#if PUD_SHIFT
      pud_t *pud = pud_offset (pgd, adr);
      if (!pud_none (*pud))
	{
	  pmd = pmd_offset (pud, adr);
#else
      pmd = pmd_offset (pgd, adr);
#endif
      if (!pmd_none (*pmd))
	{
	  ptep = pte_offset_kernel (pmd, adr);
	  pte = *ptep;
	  if (pte_present (pte))
	    {
	      ret = (unsigned long) page_address (pte_page (pte));
	      ret |= (adr & (PAGE_SIZE - 1));
	    }
#if PUD_SHIFT
	}
#endif
    }
}

return ret;
}
#endif /* RH9_REMAP */
/* Here we want the physical address of the memory.
 * This is used when initializing the contents of the
 * area and marking the pages as reserved.
 */
#ifdef RH9_REMAP
static inline unsigned long
kvirt_to_pa (unsigned long adr)
{
  unsigned long kva, ret;

  kva = (unsigned long) page_address (vmalloc_to_page ((void *) adr));
  kva |= adr & (PAGE_SIZE - 1);	/* restore the offset */
  ret = __pa (kva);
  return ret;
}

#else /* RH9_REMAP */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
static inline unsigned long
kvirt_to_pa (unsigned long adr)
{
  unsigned long kva, ret;

  kva = (unsigned long) page_address (vmalloc_to_page ((void *) adr));
  kva |= adr & (PAGE_SIZE - 1);
  ret = __pa (kva);
  return ret;
}
#else
static inline unsigned long
kvirt_to_pa (unsigned long adr)
{
  unsigned long va, kva, ret;

  va = VMALLOC_VMADDR (adr);
  kva = uvirt_to_kva (pgd_offset_k (va), va);
  ret = __pa (kva);
  return ret;
}
#endif
#endif /* RH9_REMAP */


static void *
rvmalloc (unsigned long size)
{
  void *mem;
  unsigned long adr;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 23)
unsigned long page;
#endif
  size = PAGE_ALIGN (size);
  mem = vmalloc_32 (size);
  if (!mem)
    return NULL;

  memset (mem, 0, size);	/* Clear the ram out, no junk to the user */
  adr = (unsigned long) mem;
  while ((long) size > 0)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 68)
      SetPageReserved (vmalloc_to_page ((void *) adr));
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 23)
      mem_map_reserve (vmalloc_to_page ((void *) adr));
#else
	page = kvirt_to_pa (adr);
	mem_map_reserve(VIRT_TO_PAGE(__va (page)));
#endif
#endif
      adr += PAGE_SIZE;
      size -= PAGE_SIZE;
    }

  return mem;
}

static void
rvfree (void *mem, unsigned long size)
{
  unsigned long adr;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 23)
unsigned long page;
#endif
  if (!mem)
    return;

  adr = (unsigned long) mem;
  while ((long) size > 0)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 68)
      ClearPageReserved (vmalloc_to_page ((void *) adr));
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 23)
      mem_map_unreserve (vmalloc_to_page ((void *) adr));
#else
	page = kvirt_to_pa (adr);
	mem_map_unreserve(VIRT_TO_PAGE(__va (page)));
#endif
#endif
      adr += PAGE_SIZE;
      size -= PAGE_SIZE;
    }
  vfree (mem);
}



static int
spca50x_set_packet_size (struct usb_spca50x *spca50x, int size)
{
  int alt;
	/**********************************************************************/
	/******** Try to find real Packet size from usb struct ****************/
  struct usb_device *dev = spca50x->dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  struct usb_interface_descriptor *interface = NULL;

  struct usb_config_descriptor *config = dev->actconfig;
#else
  struct usb_host_interface *interface = NULL;
  struct usb_interface *intf;
#endif
  int mysize = 0;
  int ep = 0;
  

	/**********************************************************************/

  if (size == 0)
    alt = SPCA50X_ALT_SIZE_0;
  else if (size == 128)
    alt = SPCA50X_ALT_SIZE_128;
  else if (size == 256)
    alt = SPCA50X_ALT_SIZE_256;
  else if (size == 384)
    alt = SPCA50X_ALT_SIZE_384;
  else if (size == 512)
    alt = SPCA50X_ALT_SIZE_512;
  else if (size == 640)
    alt = SPCA50X_ALT_SIZE_640;
  else if (size == 768)
    alt = SPCA50X_ALT_SIZE_768;
  else if (size == 896)
    alt = SPCA50X_ALT_SIZE_896;
  else if (size == 1023)
    if (spca50x->bridge == BRIDGE_SN9CXXX )
      {
	alt = 8;
      }
    else
      {
	alt = SPCA50X_ALT_SIZE_1023;
      }
  else
    {
      /* if an unrecognised size, default to the minimum */
      PDEBUG (5, "Set packet size: invalid size (%d), defaulting to %d",
	      size, SPCA50X_ALT_SIZE_128);
      alt = SPCA50X_ALT_SIZE_128;
    }


  PDEBUG (5, "iface alt size: %d %d %d", spca50x->iface, alt, size);
  if (usb_set_interface (spca50x->dev, spca50x->iface, alt) < 0)
    {
      err ("Set packet size: set interface error");
      return -EBUSY;
    }

   ep = SPCA50X_ENDPOINT_ADDRESS - 1;


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)
  intf = usb_ifnum_to_if (dev, spca50x->iface);
  if (intf)
    {
      interface = usb_altnum_to_altsetting (intf, alt);
    }
  else
    {
      PDEBUG (0, "intf not found");
      return -ENXIO;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
   mysize = le16_to_cpu(interface->endpoint[ep].desc.wMaxPacketSize);
#else   
	mysize = (interface->endpoint[ep].desc.wMaxPacketSize);  
#endif
#else
  interface = &config->interface[spca50x->iface].altsetting[alt];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
  mysize = le16_to_cpu(interface->endpoint[ep].wMaxPacketSize);
#else
  mysize = (interface->endpoint[ep].wMaxPacketSize);
#endif  
#endif
  
  spca50x->packet_size = mysize & 0x03ff;
  spca50x->alt = alt;
  PDEBUG (1, "set real packet size: %d, alt=%d", mysize, alt);
  return 0;
}

/* Returns number of bits per pixel (regardless of where they are located; planar or
 * not), or zero for unsupported format.
 */
static int
spca5xx_get_depth (struct usb_spca50x *spca50x, int palette)
{
  switch (palette)
    {

    case VIDEO_PALETTE_RAW_JPEG:
      return 8;		/* raw jpeg. what should we return ?? */
    case VIDEO_PALETTE_JPEG:
      if (spca50x->cameratype == JPEG ||
	  spca50x->cameratype == JPGH ||
	  spca50x->cameratype == JPGC ||
	  spca50x->cameratype == JPGS ||
	  spca50x->cameratype == JPGM )
	{
	  return 8;
	}
      else
	return 0;
    default:
      return 0;			/* Invalid format */
    }
}

/**********************************************************************
* spca50x_isoc_irq
* Function processes the finish of the USB transfer by calling 
* spca50x_move_data function to move data from USB buffer to internal
* driver structures 
***********************************************************************/
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
static void
spca50x_isoc_irq (struct urb *urb, struct pt_regs *regs)
{
  int i;
#else
static void
spca50x_isoc_irq (struct urb *urb)
{
#endif
  int len;
  struct usb_spca50x *spca50x;

  if (!urb->context)
    {
      PDEBUG (4, "no context");
      return;
    }

  spca50x = (struct usb_spca50x *) urb->context;

  if (!spca50x->dev)
    {
      PDEBUG (4, "no device ");
      return;
    }
  if (!spca50x->user)
    {
      PDEBUG (4, "device not open");
      return;
    }
  if (!spca50x->streaming)
    {
      /* Always get some of these after close but before packet engine stops */
      PDEBUG (4, "hmmm... not streaming, but got interrupt");
      return;
    }
  if (!spca50x->present)
    {
      /*  */
      PDEBUG (4, "device disconnected ..., but got interrupt !!");
      return;
    }
  /* Copy the data received into our scratch buffer */
  if (spca50x->curframe >= 0)
    {
      len = spca50x_move_data (spca50x, urb);
    }
  else if (waitqueue_active (&spca50x->wq))
    {
      wake_up_interruptible (&spca50x->wq);
    }

  /* Move to the next sbuf */
  spca50x->cursbuf = (spca50x->cursbuf + 1) % SPCA50X_NUMSBUF;

  urb->dev = spca50x->dev;
  urb->status = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 4)
  if ((i = usb_submit_urb (urb, GFP_ATOMIC)) != 0)
    err ("usb_submit_urb() ret %d", i);
#else
/* If we use urb->next usb_submit_urb() is not need in 2.4.x */
  //if ((i = usb_submit_urb(urb)) != 0)
  //err("usb_submit_urb() ret %d", i);
#endif

  return;
}


/**********************************************************************
* spca50x_init_isoc
* Function starts the ISO USB transfer by enabling this process
* from USB side and enabling ISO machine from the chip side
***********************************************************************/
static inline void
spcaCameraStart (struct usb_spca50x *spca50x)
{
  switch (spca50x->bridge)
    {
    case BRIDGE_SN9CXXX:
      sn9cxxx_start(spca50x);
       break;
    case BRIDGE_ZC3XX:
      zc3xx_start (spca50x);
      break;
    case BRIDGE_SPCA504:      
    case BRIDGE_SPCA504C:      
    case BRIDGE_SPCA536:      
    case BRIDGE_SPCA533:
    case BRIDGE_SPCA504B:
      {
	sp5xxfw2_start(spca50x);
	break;
      }

    }

}

static inline void
spcaCameraStop (struct usb_spca50x *spca50x)
{

  switch (spca50x->bridge)
    {
    case BRIDGE_SN9CXXX:
      sn9cxxx_stop (spca50x);
      break;


    case BRIDGE_ZC3XX:
      zc3xx_stop (spca50x);
      break;

    case BRIDGE_SPCA504C:
    case BRIDGE_SPCA504:
    case BRIDGE_SPCA536:
    case BRIDGE_SPCA533:
    case BRIDGE_SPCA504B:
      {
	sp5xxfw2_stop (spca50x);
	break;
      }

    }

}
static int
spca50x_init_isoc (struct usb_spca50x *spca50x)
{

  struct urb *urb;
  int fx, err, n;

  PDEBUG (3, "*** Initializing capture ***");
/* reset iso context */
  spca50x->compress = compress;
  spca50x->curframe = 0;
  spca50x->cursbuf = 0;
  spca50x->frame[0].seq = -1;
  spca50x->lastFrameRead = -1;
  
  spca50x_set_packet_size (spca50x, spca50x->pipe_size);
  PDEBUG (2, "setpacketsize %d", spca50x->pipe_size);

  for (n = 0; n < SPCA50X_NUMSBUF; n++)
    {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
      urb = usb_alloc_urb (FRAMES_PER_DESC, GFP_KERNEL);
#else
      urb = usb_alloc_urb (FRAMES_PER_DESC);
#endif
      if (!urb)
	{
	  err ("init isoc: usb_alloc_urb ret. NULL");
	  return -ENOMEM;
	}
      spca50x->sbuf[n].urb = urb;
      urb->dev = spca50x->dev;
      urb->context = spca50x;

	urb->pipe = usb_rcvisocpipe (spca50x->dev, SPCA50X_ENDPOINT_ADDRESS);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
      urb->transfer_flags = URB_ISO_ASAP;
      urb->interval = 1;
#else
      urb->transfer_flags = USB_ISO_ASAP;
#endif
      urb->transfer_buffer = spca50x->sbuf[n].data;
      urb->complete = spca50x_isoc_irq;
      urb->number_of_packets = FRAMES_PER_DESC;
      urb->transfer_buffer_length = spca50x->packet_size * FRAMES_PER_DESC;
      for (fx = 0; fx < FRAMES_PER_DESC; fx++)
	{
	  urb->iso_frame_desc[fx].offset = spca50x->packet_size * fx;
	  urb->iso_frame_desc[fx].length = spca50x->packet_size;
	}
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,41)
  spca50x->sbuf[SPCA50X_NUMSBUF - 1].urb->next = spca50x->sbuf[0].urb;
  for (n = 0; n < SPCA50X_NUMSBUF - 1; n++)
    spca50x->sbuf[n].urb->next = spca50x->sbuf[n + 1].urb;
#endif
  spcaCameraStart (spca50x);
  PDEBUG (5, "init isoc int %d altsetting %d", spca50x->iface, spca50x->alt);
  for (n = 0; n < SPCA50X_NUMSBUF; n++)
    {
      spca50x->sbuf[n].urb->dev = spca50x->dev;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,41)
      err = usb_submit_urb (spca50x->sbuf[n].urb, GFP_KERNEL);
#else
      err = usb_submit_urb (spca50x->sbuf[n].urb);
#endif
      if (err){
	err ("init isoc: usb_submit_urb(%d) ret %d", n, err);
	return err;
	}
    }

  for (n = 0; n < SPCA50X_NUMFRAMES; n++)
    {
      spca50x->frame[n].grabstate = FRAME_UNUSED;
      spca50x->frame[n].scanstate = STATE_SCANNING;
    }

  spca50x->streaming = 1;

  return 0;

}


/**********************************************************************
* spca50x_stop_isoc
* Function stops the USB ISO pipe by stopping the chip ISO machine
* and stopping USB transfer
***********************************************************************/
static void
spca5xx_kill_isoc (struct usb_spca50x *spca50x)
{
  int n;

  if (!spca50x)
    return;

  PDEBUG (3, "*** killing capture ***");
  spca50x->streaming = 0;

  /* Unschedule all of the iso td's */
  for (n = SPCA50X_NUMSBUF - 1; n >= 0; n--)
    {
      if (spca50x->sbuf[n].urb)
	{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)
	  usb_kill_urb (spca50x->sbuf[n].urb);
#else
	  usb_unlink_urb (spca50x->sbuf[n].urb);
#endif
	  usb_free_urb (spca50x->sbuf[n].urb);
	  spca50x->sbuf[n].urb = NULL;
	}
    }

  PDEBUG (3, "*** Isoc killed ***");
}

static void
spca50x_stop_isoc (struct usb_spca50x *spca50x)
{

  if (!spca50x->streaming || !spca50x->dev)
    return;

  PDEBUG (3, "*** Stopping capture ***");
  spcaCameraStop (spca50x);
  spca5xx_kill_isoc(spca50x);
  spca50x_set_packet_size (spca50x, 0);

  PDEBUG (3, "*** Capture stopped ***");
}

/**********************************************************************
* spca50x_smallest_mode_index
* Function finds the mode index in the modes table of the smallest
* available mode.
***********************************************************************/
static int spca5xx_getDefaultMode(struct usb_spca50x *spca50x)
{
  int i;
  for (i = QCIF;i < TOTMODE; i++){
  if(spca50x->mode_cam[i].method == 0 && spca50x->mode_cam[i].width){
		spca50x->width = spca50x->mode_cam[i].width;
		spca50x->height = spca50x->mode_cam[i].height;
		spca50x->method = 0;
		spca50x->pipe_size = spca50x->mode_cam[i].pipe ;
		spca50x->mode = spca50x->mode_cam[i].mode;
		return 0;
		}
  }
return -EINVAL;
}


/**********************************************************************
* spca50x_set_mode
* Function sets up the resolution directly. 
* Attention!!! index, the index in modes array is NOT checked. 
***********************************************************************/
static int spca5xx_getcapability(struct usb_spca50x *spca50x )
{
	
	int maxw,maxh,minw,minh;
	int i;
	minw = minh = 255*255;
	maxw = maxh = 0;
	for (i = QCIF; i < TOTMODE; i++){
	if( spca50x->mode_cam[i].width){
	  if (maxw < spca50x->mode_cam[i].width || maxh < spca50x->mode_cam[i].height){
	    maxw = spca50x->mode_cam[i].width;
	    maxh = spca50x->mode_cam[i].height;
	  }
	  if (minw > spca50x->mode_cam[i].width || minh > spca50x->mode_cam[i].height){
	    minw = spca50x->mode_cam[i].width;
	    minh = spca50x->mode_cam[i].height;
	  }
	 }
	}
	spca50x->maxwidth = maxw;
	spca50x->maxheight = maxh;
	spca50x->minwidth = minw;
	spca50x->minheight = minh;
	PDEBUG(0,"maxw %d maxh %d minw %d minh %d",maxw,maxh,minw,minh);	
return 0;
}
static inline int v4l_to_spca5xx(int format)
{switch(format){
    case VIDEO_PALETTE_RGB565: return P_RGB16;
    case VIDEO_PALETTE_RGB24: return P_RGB24;
    case VIDEO_PALETTE_RGB32: return P_RGB32;
    case VIDEO_PALETTE_YUV422P: return P_YUV422;
    case VIDEO_PALETTE_YUV420P: return P_YUV420;
    case VIDEO_PALETTE_RAW_JPEG: return P_RAW;
    case VIDEO_PALETTE_JPEG: return P_JPEG;
    default: return -EINVAL;
   }
} 
static inline int spca5xx_setMode(struct usb_spca50x *spca50x,int width,int height,int format)
{
  int i,j;
  int formatIn;
  int crop = 0, cropx1 = 0, cropx2 = 0, cropy1 = 0, cropy2 = 0, x = 0, y = 0;
   /* Avoid changing to already selected mode */
   /* convert V4l format to our internal format */
   PDEBUG(2,"Set mode asked w %d h %d p %d",width,height,format);
  
  if ((formatIn = v4l_to_spca5xx(format)) < 0) 
    return -EINVAL;
  for (i = QCIF; i< TOTMODE; i++){
    if((spca50x->mode_cam[i].width == width) &&
     (spca50x->mode_cam[i].height == height) &&
     (spca50x->mode_cam[i].t_palette & formatIn)){
        spca50x->width = spca50x->mode_cam[i].width;
        spca50x->height = spca50x->mode_cam[i].height;
        spca50x->pipe_size = spca50x->mode_cam[i].pipe ;
        spca50x->mode = spca50x->mode_cam[i].mode;
	spca50x->method = spca50x->mode_cam[i].method;
	spca50x->format = format; //palette in use
     if(spca50x->method){
     if (spca50x->method == 1){
      for (j = i; j < TOTMODE ; j++){
       if(spca50x->mode_cam[j].method == 0 && spca50x->mode_cam[j].width){
        spca50x->hdrwidth = spca50x->mode_cam[j].width;
        spca50x->hdrheight = spca50x->mode_cam[j].height;
	spca50x->mode = spca50x->mode_cam[j].mode; // overwrite by the hardware mode
        break;
       }
      } // end match hardware mode
      if (!spca50x->hdrwidth && !spca50x->hdrheight)
          return -EINVAL;  
     } 
     }
    /* match found */
    break;
    }
   } // end match mode
  /* initialize the hdrwidth and hdrheight for the first init_source */
  /* precompute the crop x y value for each frame */
  if (!spca50x->method)
    {
      /* nothing todo hardware found stream */
      cropx1 = cropx2 = cropy1 = cropy2 = x = y = 0;
      spca50x->hdrwidth = spca50x->width;
      spca50x->hdrheight = spca50x->height;
    }
  if (spca50x->method & 0x01)
    {
      /* cropping method */
      if (spca50x->hdrwidth > spca50x->width)
	{

	  crop = (spca50x->hdrwidth - spca50x->width);
	  if (spca50x->cameratype == JPEG || spca50x->cameratype == JPGH || spca50x->cameratype == JPGS)
	    crop = crop >> 4;
	  cropx1 = crop >> 1;
	  cropx2 = cropx1 + (crop % 2);
	}
      else
	{
	  cropx1 = cropx2 = 0;
	}
      if (spca50x->hdrheight > spca50x->height)
	{
	  crop = (spca50x->hdrheight - spca50x->height);
	  if (spca50x->cameratype == JPEG)
	    crop = crop >> 4;
	  if (spca50x->cameratype == JPGH || spca50x->cameratype == JPGS)
	    crop = crop >> 3;
	  cropy1 = crop >> 1;
	  cropy2 = cropy1 + (crop % 2);
	}
      else
	{
	  cropy1 = cropy2 = 0;
	}
    }
  if (spca50x->method & 0x02)
    {
      /* what can put here for div method */
    }
  if (spca50x->method & 0x04)
    {
      /* and here for mult */
    }
  PDEBUG (2, "Found code %d method %d", spca50x->mode, spca50x->method);
  PDEBUG (2, "Soft Win width height %d x %d", spca50x->width,
	  spca50x->height);
  PDEBUG (2, "Hard Win width height %d x %d", spca50x->hdrwidth,
	  spca50x->hdrheight);

  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    {
      spca50x->frame[i].method = spca50x->method;
      spca50x->frame[i].cameratype = spca50x->cameratype;
      spca50x->frame[i].cropx1 = cropx1;
      spca50x->frame[i].cropx2 = cropx2;
      spca50x->frame[i].cropy1 = cropy1;
      spca50x->frame[i].cropy2 = cropy2;
      spca50x->frame[i].x = x;
      spca50x->frame[i].y = y;
      spca50x->frame[i].hdrwidth = spca50x->hdrwidth;
      spca50x->frame[i].hdrheight = spca50x->hdrheight;
      spca50x->frame[i].width = spca50x->width;
      spca50x->frame[i].height = spca50x->height;
       spca50x->frame[i].format = spca50x->format;
      spca50x->frame[i].scanlength = spca50x->width * spca50x->height * 3 / 2;
      // ?? assumes 4:2:0 data
    }

  return 0;  
}

/**********************************************************************
* spca50x_mode_init_regs
* Function sets up the resolution with checking if it's necessary 
***********************************************************************/
static int 
spca5xx_restartMode (struct usb_spca50x *spca50x, int width, int height, int format)
{
 int was_streaming;
 int r;
 /* Avoid changing to already selected mode */
  if (spca50x->width == width && spca50x->height == height)
    return 0;
PDEBUG (1, "Mode changing to %d,%d", width, height);

  was_streaming = spca50x->streaming;
  if (was_streaming)    
	spca50x_stop_isoc (spca50x);
    
  r = spca5xx_setMode(spca50x,width,height,format);
  if (r < 0)
       goto out;
  if (was_streaming)
	  r = spca50x_init_isoc (spca50x);
out:    
  return r;
}

/**********************************************************************
* spca50x_get_brightness
* Function reads the brightness from the camera
* Receives the pointer to the description structure
* returns the value of brightness
**********************************************************************/
static inline __u16
spca50x_get_brightness (struct usb_spca50x *spca50x)
{
  __u16 brightness = 0;		// value of the brightness
  
      switch (spca50x->bridge)
	{

	case BRIDGE_SN9CXXX:
	  brightness = sn9cxxx_getbrightness (spca50x);
	  break;

	case BRIDGE_ZC3XX:
	  brightness = zc3xx_getbrightness (spca50x);
	  break;



	case BRIDGE_SPCA536:
	case BRIDGE_SPCA533:
	case BRIDGE_SPCA504:
	case BRIDGE_SPCA504B:
	case BRIDGE_SPCA504C:
	 brightness = sp5xxfw2_getbrightness (spca50x);
	break;
	default:
	  {
	    brightness = 0;
	    break;
	  }
	}
    
  return brightness;
}

/**********************************************************************
* spca50x_set_brightness
* Function sets the brightness to the camera
* Receives the pointer to the description structure 
* and brightness value
**********************************************************************/
static inline void
spca50x_set_brightness (struct usb_spca50x *spca50x, __u8 brightness)
{
      switch (spca50x->bridge)
	{

	case BRIDGE_SN9CXXX:
	spca50x->brightness = brightness << 8;
	  sn9cxxx_setbrightness (spca50x);
	break;

	case BRIDGE_ZC3XX:
	  spca50x->brightness = brightness << 8;
	  zc3xx_setbrightness (spca50x);
	  break;


	case BRIDGE_SPCA536:
	case BRIDGE_SPCA533:
	case BRIDGE_SPCA504:
	case BRIDGE_SPCA504B:
	case BRIDGE_SPCA504C:
	  {
	  spca50x->brightness = brightness << 8;
	  sp5xxfw2_setbrightness (spca50x);
	   
	    break;
	  }

	default:
	  {
	    break;
	  }
	}
    
}


/**********************************************************************
 *
 * SPCA50X data transfer, IRQ handler
 *
 **********************************************************************/
static struct spca50x_frame *
spca50x_next_frame (struct usb_spca50x *spca50x, unsigned char *cdata)
{
  int iFrameNext;
  struct spca50x_frame *frame = NULL;

  /* Cycle through the frame buffer looking for a free frame to overwrite */
  iFrameNext = (spca50x->curframe + 1) % SPCA50X_NUMFRAMES;
  while (frame == NULL && iFrameNext != (spca50x->curframe))
    {
      if (spca50x->frame[iFrameNext].grabstate == FRAME_READY ||
	  spca50x->frame[iFrameNext].grabstate == FRAME_UNUSED ||
	  spca50x->frame[iFrameNext].grabstate == FRAME_ERROR)
	{
	  spca50x->curframe = iFrameNext;
	  frame = &spca50x->frame[iFrameNext];
	  break;
	}
      else
	{
	  iFrameNext = (iFrameNext + 1) % SPCA50X_NUMFRAMES;
	}
    }

  if (frame == NULL)
    {
      PDEBUG (3, "Can't find a free frame to grab into...using next. "
	      "This is caused by the application not reading fast enough.");
      spca50x->curframe = (spca50x->curframe + 1) % SPCA50X_NUMFRAMES;
      frame = &spca50x->frame[spca50x->curframe];
    }

  frame->grabstate = FRAME_GRABBING;

  /* Record the frame sequence number the camera has told us */
  if (cdata)
    {
      switch (spca50x->bridge)
	{
	case BRIDGE_SPCA504:
	  frame->seq = cdata[SPCA50X_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA504C:
	  frame->seq = cdata[SPCA50X_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA504B:
	  frame->seq = cdata[SPCA50X_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA533:
	  frame->seq = cdata[SPCA533_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA536:
	  frame->seq = cdata[SPCA536_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_ZC3XX: 
	case BRIDGE_SN9CXXX:
	 frame->seq=00;
	 break;
	}
    }

  /* Reset some per-frame variables */
  frame->highwater = frame->data;
  frame->scanstate = STATE_LINES;
  frame->scanlength = 0;
  frame->last_packet = -1;
  frame->totlength = 0;
  spca50x->packet = 0;
  return frame;
}

/* Tasklet function to decode */

void
outpict_do_tasklet (unsigned long ptr)
{
  int err;
  struct spca50x_frame *taskletframe = (struct spca50x_frame *) ptr;

  taskletframe->scanlength = taskletframe->highwater - taskletframe->data;

  PDEBUG (2, "Tasklet ask spcadecoder hdrwidth %d hdrheight %d method %d",
	  taskletframe->hdrwidth, taskletframe->hdrheight,
	  taskletframe->method);
  err = spca50x_outpicture (taskletframe);
  if (err != 0)
    {
      PDEBUG (2, "frame decoder failed (%d)", err);
      taskletframe->grabstate = FRAME_ERROR;
    }
  else
    {
      taskletframe->grabstate = FRAME_DONE;
    }
  if (waitqueue_active (&taskletframe->wq))
    wake_up_interruptible (&taskletframe->wq);

}

/* ******************************************************************
* spca50x_move_data
* Function serves for moving data from USB transfer buffers
* to internal driver frame buffers.
******************************************************************* */
static int
spca50x_move_data (struct usb_spca50x *spca50x, struct urb *urb)
{
  unsigned char *cdata;		//Pointer to buffer where we do store next packet
  unsigned char *pData;		//Pointer to buffer where we do store next packet
  int i;
  struct spca50x_frame *frame;	//Pointer to frame data
  int iPix;			//Offset of pixel data in the ISO packet

  int totlen = 0;
  int tv8532 = 0;

  for (i = 0; i < urb->number_of_packets; i++)
    {
      int datalength = urb->iso_frame_desc[i].actual_length;
      int st = urb->iso_frame_desc[i].status;
      int sequenceNumber;
      int sof;

      /* PDEBUG(5,"Packet data [%d,%d,%d]", datalength, st,
         urb->iso_frame_desc[i].offset); */

      urb->iso_frame_desc[i].actual_length = 0;
      urb->iso_frame_desc[i].status = 0;

      cdata = ((unsigned char *) urb->transfer_buffer) +
	urb->iso_frame_desc[i].offset;
      /* Check for zero length block or no selected frame buffer */
      if (!datalength || spca50x->curframe == -1)
	{
	  tv8532 = 0;
	  continue;
	}

      PDEBUG (5, "Packet data [%d,%d,%d] Status: %d", datalength, st,
	      urb->iso_frame_desc[i].offset, st);

      if (st)
	PDEBUG (2, "data error: [%d] len=%d, status=%d", i, datalength, st);

      frame = &spca50x->frame[spca50x->curframe];
      totlen = frame->totlength;

      /* read the sequence number */
      if (spca50x->bridge == BRIDGE_ZC3XX ||
	  spca50x->bridge == BRIDGE_SN9CXXX )
	{
	  if (frame->last_packet == -1)
	    {
	      /*initialize a new frame */
	      sequenceNumber = 0;
	    }
	  else
	    {
	      sequenceNumber = frame->last_packet;
	    }
	}
      else
	{
	  sequenceNumber = cdata[SPCA50X_OFFSET_SEQUENCE];
	  PDEBUG (4, "Packet start  %x %x %x %x %x", cdata[0], cdata[1],
		  cdata[2], cdata[3], cdata[4]);
	}
      /* check frame start */
      switch (spca50x->bridge)
	{
	case BRIDGE_SN9CXXX:
		iPix = 0;
		sof = datalength - 64;
		if (sof < 0)
			sequenceNumber++;
		else if(cdata[sof] == 0xff && cdata[sof +1] == 0xd9){
			 sequenceNumber = 0; //start of frame
			 // copy the end of data frame
	  		memcpy (frame->highwater, cdata, sof+2);
      			frame->highwater += (sof+2);
      			totlen += (sof+2);
#if 0
			spin_lock(&spca50x->v4l_lock);
			spca50x->avg_lum = (cdata[sof+24] + cdata[sof+26]) >> 1 ;
			spin_unlock(&spca50x->v4l_lock);
			PDEBUG (5,"mean luma %d",spca50x->avg_lum );
#endif
			PDEBUG (5,
			"Sonix header packet found datalength %d totlength %d!!",
			  datalength, totlen);
			  PDEBUG(5,"%03d %03d %03d %03d %03d %03d %03d %03d",cdata[sof+24], cdata[sof+25],
			  cdata[sof+26] , cdata[sof+27], cdata[sof+28], cdata[sof+29],
			  cdata[sof+30], cdata[sof+31]);	
			// setting to skip the rest of the packet
			spca50x->header_len = datalength;
			} else
				sequenceNumber++;
	break;
	case BRIDGE_ZC3XX:
	  {
	    iPix = 0;
	    if (cdata[0] == 0xFF && cdata[1] == 0xD8)
	      {
		sequenceNumber = 0;
		spca50x->header_len = 2;	//18 remove 0xff 0xd8;
		PDEBUG (5,
			"Zc301 header packet found datalength %d totlength %d!!",
			datalength, totlen);
	      }
	    else
	      {
		sequenceNumber++;
	      }

	  }
	  break;
	case BRIDGE_SPCA533:
	  {
	    iPix = 1;
	    if (sequenceNumber == SPCA50X_SEQUENCE_DROP)
	      {
		if (cdata[1] == 0x01)
		  {
		    sequenceNumber = 0;
		  }
		else
		  {
		    /* drop packet */
		    PDEBUG (5, "Dropped packet (expected seq 0x%02x)",
			    frame->last_packet + 1);
		    continue;
		  }
	      }
	    else
	      {
		sequenceNumber++;
	      }
	  }
	  break;
	case BRIDGE_SPCA536:
	  {
	    iPix = 2;
	    if (sequenceNumber == SPCA50X_SEQUENCE_DROP)
	      {
		sequenceNumber = 0;
	      }
	    else
	      {
		sequenceNumber++;
	      }
	  }
	  break;
	case BRIDGE_SPCA504:
	case BRIDGE_SPCA504B:
	case BRIDGE_SPCA504C:
	  {
	    iPix = 1;
	    switch (sequenceNumber)
	      {
	      case 0xfe:
		sequenceNumber = 0;
		break;
	      case SPCA50X_SEQUENCE_DROP:
		/* drop packet */
		PDEBUG (5, "Dropped packet (expected seq 0x%02x)",
			frame->last_packet + 1);
		continue;
	      default:
		sequenceNumber++;
		break;
	      }
	  }
	  break;

	default:
	  {
	    iPix = 1;
	    /* check if this is a drop packet */
	    if (sequenceNumber == SPCA50X_SEQUENCE_DROP)
	      {
		PDEBUG (3, "Dropped packet (expected seq 0x%02x)",
			frame->last_packet + 1);
		continue;
	      }
	  }
	  break;
	}

      PDEBUG (3, "spca50x: Packet seqnum = 0x%02x.  curframe=%2d",
	      sequenceNumber, spca50x->curframe);
      pData = cdata;

      /* Can we find a frame start */
      if (sequenceNumber == 0)
	{
	  totlen = 0;
	  iPix = spca50x->header_len;
	  PDEBUG (3, "spca50x: Found Frame Start!, framenum = %d",
		  spca50x->curframe);
	  // Start of frame is implicit end of previous frame
	  // Check for a previous frame and finish it off if one exists
	  if (frame->scanstate == STATE_LINES){

	      if (frame->format != VIDEO_PALETTE_RAW_JPEG)
		{
		  /* Decode the frame */
		 
		tasklet_init (&spca50x->spca5xx_tasklet, outpict_do_tasklet,
				(unsigned long) frame);
		tasklet_schedule (&spca50x->spca5xx_tasklet);
		}
	      else
		{
		  /* RAW DATA stream */
		  frame->grabstate = FRAME_DONE;
		  if (waitqueue_active (&frame->wq))
		    wake_up_interruptible (&frame->wq);
		}
	      // If someone decided to wait for ANY frame - wake him up 
	      if (waitqueue_active (&spca50x->wq))
		wake_up_interruptible (&spca50x->wq);
	      frame = spca50x_next_frame (spca50x, cdata);
	    }
	  else
	    frame->scanstate = STATE_LINES;
	}

      /* Are we in a frame? */
      if (frame == NULL || frame->scanstate != STATE_LINES)
	continue;
      if (sequenceNumber != frame->last_packet + 1 &&
	  frame->last_packet != -1)
	{
	  /* Note, may get one of these for the first packet after opening */
	  PDEBUG (2, "Out of order packet, last = %d, this = %d",
		  frame->last_packet, sequenceNumber);

	}
      frame->last_packet = sequenceNumber;

      /* This is the real conversion of the raw camera data to BGR for V4L */
      datalength -= iPix;	// correct length for packet header
      pData = cdata + iPix;	// Skip packet header (1 or 10 bytes)

      // Consume data
      PDEBUG (5, "Processing packet seq  %d,length %d,totlength %d",
	      frame->last_packet, datalength, frame->totlength);
      /* this copy consume input data from the isoc stream */
      if ((datalength > 0) && (datalength <= 0x3ff)){
      memcpy (frame->highwater, pData, datalength);
      frame->highwater += datalength;
      totlen += datalength;
      }
      	
      frame->totlength = totlen;
#ifdef SPCA50X_ENABLE_EXP_BRIGHTNESS
      /* autoadjust is set to 1 to so now autobrightness will be
         calculated frome this frame. autoadjust will be set to 0 when
         autobrightness has been corrected if needed. */
      autoadjust = 1;
#endif /* SPCA50X_ENABLE_EXP_BRIGHTNESS */


    }
  return totlen;
}


/****************************************************************************
 *
 * Buffer management
 *
 ***************************************************************************/
static int
spca50x_alloc (struct usb_spca50x *spca50x)
{
  int i;

  PDEBUG (4, "entered");
  down (&spca50x->buf_lock);
  spca50x->tmpBuffer = rvmalloc (MAX_FRAME_SIZE);
  if (spca50x->buf_state == BUF_ALLOCATED)
    goto out;

  spca50x->fbuf = rvmalloc (SPCA50X_NUMFRAMES * MAX_DATA_SIZE);
  if (!spca50x->fbuf)
    goto error;

  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    {
      spca50x->frame[i].tmpbuffer = spca50x->tmpBuffer;
      spca50x->frame[i].decoder = &spca50x->maindecode; //connect each frame to the main data decoding 
      spca50x->frame[i].grabstate = FRAME_UNUSED;
      spca50x->frame[i].scanstate = STATE_SCANNING;
      spca50x->frame[i].data = spca50x->fbuf + i * MAX_DATA_SIZE;
      spca50x->frame[i].highwater = spca50x->frame[i].data;
      PDEBUG (4, "frame[%d] @ %p", i, spca50x->frame[i].data);
    }

  for (i = 0; i < SPCA50X_NUMSBUF; i++)
    {
      spca50x->sbuf[i].data = kmalloc (FRAMES_PER_DESC *
				       MAX_FRAME_SIZE_PER_DESC, GFP_KERNEL);
      if (!spca50x->sbuf[i].data)
	goto error;
      PDEBUG (4, "sbuf[%d] @ %p", i, spca50x->sbuf[i].data);
    }
  spca50x->buf_state = BUF_ALLOCATED;
out:
  up (&spca50x->buf_lock);
  PDEBUG (4, "leaving");
  return 0;
error:
  /* FIXME: IMHO, it's better to move error deallocation code here. */

  for (i = 0; i < SPCA50X_NUMSBUF; i++)
    {

      if (spca50x->sbuf[i].data)
	{
	  kfree (spca50x->sbuf[i].data);
	  spca50x->sbuf[i].data = NULL;
	}
    }
  if (spca50x->fbuf)
    {
      rvfree (spca50x->fbuf, SPCA50X_NUMFRAMES * MAX_DATA_SIZE);
      spca50x->fbuf = NULL;
    }
  if (spca50x->tmpBuffer)
    {
      rvfree (spca50x->tmpBuffer, MAX_FRAME_SIZE);
      spca50x->tmpBuffer = NULL;
    }

  spca50x->buf_state = BUF_NOT_ALLOCATED;
  up (&spca50x->buf_lock);
  PDEBUG (1, "errored");
  return -ENOMEM;
}

static void
spca5xx_dealloc (struct usb_spca50x *spca50x )
{
  int i;
  PDEBUG (2, "entered dealloc");
  down (&spca50x->buf_lock);
  if (spca50x->fbuf)
    {
      rvfree (spca50x->fbuf, SPCA50X_NUMFRAMES * MAX_DATA_SIZE);
      spca50x->fbuf = NULL;
      for (i = 0; i < SPCA50X_NUMFRAMES; i++)
	spca50x->frame[i].data = NULL;
    }

  if (spca50x->tmpBuffer)
    {
      rvfree (spca50x->tmpBuffer, MAX_FRAME_SIZE);
      spca50x->tmpBuffer = NULL;
    }

  for (i = 0; i < SPCA50X_NUMSBUF; i++)
    {
      if (spca50x->sbuf[i].data)
	{
	  kfree (spca50x->sbuf[i].data);
	  spca50x->sbuf[i].data = NULL;
	}
    }

  PDEBUG (2, "buffer memory deallocated");
  spca50x->buf_state = BUF_NOT_ALLOCATED;
  up (&spca50x->buf_lock);
  PDEBUG (2, "leaving dealloc");
}
/**
 * Reset the camera and send the correct initialization sequence for the
 * currently selected source
 */
static int
spca50x_init_source (struct usb_spca50x *spca50x)
{
  int err_code;

  switch (spca50x->bridge)
    {
    case BRIDGE_SN9CXXX:
      err_code = sn9cxxx_init (spca50x);
      PDEBUG (2, "Initializing Sonix finished %d",err_code);
      if (err_code < 0) return err_code;
      break;

    case BRIDGE_ZC3XX:
      err_code = zc3xx_init (spca50x);
      break;

    case BRIDGE_SPCA504:
    case BRIDGE_SPCA504C:
    case BRIDGE_SPCA536:
    case BRIDGE_SPCA533:
    case BRIDGE_SPCA504B:
      {
	PDEBUG (2, "Opening SPCA5xx FW2");
	sp5xxfw2_init (spca50x);
	break;
      }

    default:
      {
	err ("Unimplemented bridge type");
	return -EINVAL;
      }
    }
  spca50x->norme = 0;
  spca50x->channel = 0;

     err_code =spca5xx_setMode(spca50x,spca50x->width,spca50x->height,VIDEO_PALETTE_JPEG);
     PDEBUG(2,"set Mode return %d ", err_code);
     if (err_code < 0) return -EINVAL;

  return 0;
}


/****************************************************************************
 *
 * V4L API
 *
 ***************************************************************************/
static inline void spca5xx_setFrameDecoder(struct usb_spca50x *spca50x)
{ int i;
  /* Set default sizes in case IOCTL (VIDIOCMCAPTURE) is not used
   * (using read() instead). */
  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    {
      spca50x->frame[i].width = spca50x->width;
      spca50x->frame[i].height = spca50x->height;
      spca50x->frame[i].cameratype = spca50x->cameratype;
      spca50x->frame[i].scanlength = 0;
      spca50x->frame[i].depth = 8;
      spca50x->frame[i].format = VIDEO_PALETTE_JPEG;
    }
}

static inline void spca5xx_initDecoder(struct usb_spca50x *spca50x)
{
  if (spca50x->cameratype == JPEG)
    init_jpeg_decoder (spca50x,2);
  if (spca50x->cameratype == JPGH 
     || spca50x->cameratype == JPGC
     || spca50x->cameratype == JPGS)
    init_jpeg_decoder (spca50x,1);
  if (spca50x->cameratype == JPGM)
    init_jpeg_decoder (spca50x,0);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
static int
spca5xx_open (struct inode *inode, struct file *file)
{
  struct video_device *vdev = video_devdata (file);
#else
static int
spca5xx_open(struct video_device *vdev, int flags)
{
#endif
  struct usb_spca50x *spca50x = video_get_drvdata (vdev);

  int err;
  
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  MOD_INC_USE_COUNT;
#endif
  PDEBUG (2, "opening");

  down (&spca50x->lock);
  /* sanity check disconnect, in use, no memory available */
  err = -ENODEV;
  if (!spca50x->present)
    goto out;
  err = -EBUSY;
  if (spca50x->user)
    goto out;
  err = -ENOMEM;
  if (spca50x_alloc (spca50x))
    goto out;
 /* initialize sensor and decoding */
  err = spca50x_init_source (spca50x);
  if (err != 0){
  	PDEBUG (0, "DEALLOC error on spca50x_init_source\n");
	up (&spca50x->lock);
  	spca5xx_dealloc (spca50x);
    goto out2;
    }
  spca5xx_initDecoder(spca50x);
   /* open always start in rgb24 a bug in gqcam 
    did not select the palette nor the size  
    v4l spec need that the camera always start on the last setting*/ 
  spca5xx_setFrameDecoder(spca50x);
 
  spca50x->user++;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22) 
  file->private_data = vdev;
#endif 
  err = spca50x_init_isoc (spca50x);
  if (err)
    {
      PDEBUG (0, " DEALLOC error on init_Isoc\n");
      spca50x->user--;
      spca5xx_kill_isoc (spca50x);
      up (&spca50x->lock);
      spca5xx_dealloc (spca50x);
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22) 
      file->private_data = NULL;
#endif      
      goto out2;
    }

  /* Now, let's get brightness from the camera */
  spca50x->brightness = spca50x_get_brightness (spca50x) << 8;


out:
  up (&spca50x->lock);
out2:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  if (err)
    MOD_DEC_USE_COUNT;
#endif

  if (err)
    {
      PDEBUG (2, "Open failed");
    }
  else
    {
      PDEBUG (2, "Open done");
    }

  return err;
}

static void inline
spcaCameraShutDown (struct usb_spca50x *spca50x)
{
  if (spca50x->dev)
    {
      if (spca50x->bridge == BRIDGE_ZC3XX)
	
	  zc3xx_shutdown (spca50x);
	
    }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
static int
spca5xx_close (struct inode *inode, struct file *file)
{

  struct video_device *vdev = file->private_data;
#else
static void 
spca5xx_close( struct video_device *vdev)
{
#endif
  struct usb_spca50x *spca50x = video_get_drvdata (vdev);
  int i;
  PDEBUG (2, "spca50x_close");

  down (&spca50x->lock);

  spca50x->user--;
  spca50x->curframe = -1;
  if (spca50x->present)
    {
      spca50x_stop_isoc (spca50x);
      spcaCameraShutDown (spca50x);
    

  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    {
      if (waitqueue_active (&spca50x->frame[i].wq))
	wake_up_interruptible (&spca50x->frame[i].wq);
    }
  if (waitqueue_active (&spca50x->wq))
    wake_up_interruptible (&spca50x->wq);

  }  
  /* times to dealloc ressource */
   up (&spca50x->lock);
  spca5xx_dealloc (spca50x);
  PDEBUG(2,"Release ressources done");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  MOD_DEC_USE_COUNT;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
  file->private_data = NULL;
  return 0;
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
static int
spca5xx_do_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
		  void *arg)
{
  struct video_device *vdev = file->private_data;
#else
static int 
spca5xx_ioctl (struct video_device *vdev, unsigned int cmd, void *arg)
{
#endif
  struct usb_spca50x *spca50x = video_get_drvdata (vdev);

  PDEBUG (2, "do_IOCtl: 0x%X", cmd);

  if (!spca50x->dev)
    return -EIO;

  switch (cmd)
    {
    case VIDIOCGCAP:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	 struct video_capability *b = arg;
#else
	 struct video_capability j ;
	 struct video_capability *b = &j;
#endif	
	PDEBUG (2, "VIDIOCGCAP %p :", b);

	memset (b, 0, sizeof (struct video_capability));
	snprintf (b->name, 32, "%s", clist[spca50x->desc].description);
	b->type = VID_TYPE_CAPTURE;
	b->channels = 1;
	b->audios = 0;
	b->maxwidth = spca50x->maxwidth;
	b->maxheight = spca50x->maxheight;

	b->minwidth = spca50x->minwidth;
	b->minheight = spca50x->minheight;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
	if(copy_to_user(arg,b,sizeof(struct video_capability)))
	return -EFAULT;
#endif

	return 0;
      }
    case VIDIOCGCHAN:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	 struct video_channel *v = arg;
#else
	 struct video_channel k ;
	 struct video_channel *v = &k;
	 if(copy_from_user(v,arg,sizeof(struct video_channel)))
	 return -EFAULT;
#endif	 	
	switch (spca50x->bridge)
	  {
	  case BRIDGE_SPCA504:
	  case BRIDGE_SPCA504B:
	  case BRIDGE_SPCA504C:
	  case BRIDGE_SPCA533:
	  case BRIDGE_SPCA536:
	  case BRIDGE_ZC3XX:
	  case BRIDGE_SN9CXXX:
	    {
	      snprintf (v->name, 32, "%s", Blist[spca50x->bridge].name);
	      break;
	    }
	  }

	v->flags = 0;
	v->tuners = 0;
	v->type = VIDEO_TYPE_CAMERA;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
	if(copy_to_user(arg,v,sizeof(struct video_channel)))
	return -EFAULT;
#endif	

	return 0;
      }
    case VIDIOCSCHAN:
      {
	/* 
	   There seems to be some confusion as to whether this is a struct 
	   video_channel or an int. This should be safe as the first element 
	   of video_channel is an int channel number
	   and we just ignore the rest 
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_channel *v = arg;
#else
	 struct video_channel k ;
	 struct video_channel *v = &k;
	 if(copy_from_user(v,arg,sizeof(struct video_channel)))
	 return -EFAULT;
#endif	
/* make compiler happy i Hope */
	v->flags = 0;
	v->tuners = 0;
	v->type = VIDEO_TYPE_CAMERA;
	return 0;
      }
    case VIDIOCGPICT:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_picture *p = arg;
#else	
	struct video_picture p1 ;
	struct video_picture *p = &p1;
#endif	

	p->depth = spca50x->frame[0].depth;
	p->palette = spca50x->format;

	PDEBUG (4, "VIDIOCGPICT: depth=%d, palette=%d", p->depth, p->palette);

	    switch (spca50x->bridge)
	      {
		case BRIDGE_ZC3XX:
		spca50x->brightness =zc3xx_getbrightness (spca50x);
		p->brightness = spca50x->brightness;
		p->contrast = spca50x->contrast;
		p->colour = spca50x->colour;
		break;
	      case BRIDGE_SN9CXXX:	
		spca50x->brightness = sn9cxxx_getbrightness(spca50x);
		sn9cxxx_getcontrast(spca50x);
		//spca50x->colour = sn9cxxx_getcolors(spca50x);
		p->brightness = spca50x->brightness;
		p->contrast = spca50x->contrast;
		p->colour = spca50x->colour;
		break;

	      case BRIDGE_SPCA536:
	      case BRIDGE_SPCA533:
	      case BRIDGE_SPCA504:
	      case BRIDGE_SPCA504B:
	      case BRIDGE_SPCA504C:
		{		
		  p->colour = sp5xxfw2_getcolors(spca50x);		  
		  p->whiteness = 0;
		  p->contrast = sp5xxfw2_getcontrast(spca50x);
		  p->brightness = spca50x_get_brightness (spca50x);
		  p->hue = 0;		 
		  PDEBUG (0,
			  "color: 0x%02x contrast: 0x%02x brightness: 0x%02x hue: 0x%02x",
			  p->colour, p->contrast, p->brightness, p->hue);
		  break;
		}
	      default:
		{
		  /* default to not allowing any settings to change */
		  p->colour = spca50x->colour;
		  p->whiteness = spca50x->whiteness;
		  p->contrast = spca50x->contrast;
		  p->brightness = spca50x->brightness;
		  p->hue = spca50x->hue;
		  break;
		}
	      }
	  
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
	if(copy_to_user(arg,p,sizeof(struct video_picture)))
	return -EFAULT;
#endif	

	return 0;
      }
    case VIDIOCSPICT:
      {
      	int i;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_picture *p = arg;
#else	
	struct video_picture p1 ;
	struct video_picture *p = &p1;
	if(copy_from_user(p,arg,sizeof(struct video_picture)))
	 return -EFAULT;
#endif	
	

	PDEBUG (4, "VIDIOCSPICT");


	if (!spca5xx_get_depth (spca50x, p->palette))
	  return -EINVAL;

if(spca50x->format != p->palette){
/* FIXME test if the palette is available */
	PDEBUG (4, "Setting depth=%d, palette=%d", p->depth, p->palette);
	/* change the output palette the input stream is the same*/
	/* no need to stop the camera streaming and restart */
	for (i = 0; i < SPCA50X_NUMFRAMES; i++)
	  {
	    spca50x->frame[i].depth = p->depth;
	    spca50x->frame[i].format = p->palette;
	  }
	spca50x->format = p->palette;
}

	    switch (spca50x->bridge)
	      {

	      case BRIDGE_SN9CXXX:
	        spca50x->contrast = p->contrast;
		spca50x->brightness = p->brightness;		
		sn9cxxx_setbrightness(spca50x);
		sn9cxxx_setcontrast(spca50x);		
	        break;

	      case BRIDGE_ZC3XX:
		spca50x->contrast = p->contrast;
		spca50x->brightness = p->brightness;
		zc3xx_setbrightness (spca50x);
		break;

	      case BRIDGE_SPCA536:
	      case BRIDGE_SPCA533:
	      case BRIDGE_SPCA504:
	      case BRIDGE_SPCA504B:
	      case BRIDGE_SPCA504C:
		{		
		spca50x_set_brightness (spca50x, ((p->brightness >> 8) + 128) % 255);
		spca50x->contrast = p->contrast;
		spca50x->colour = p->colour;
		sp5xxfw2_setcolors(spca50x);
		sp5xxfw2_setcontrast(spca50x);
		p->whiteness = 0;
		
		  break;
		}

	      default:
		{
		  /* default to not allowing any settings to change ?? */
		  p->colour = spca50x->colour;
		  p->whiteness = spca50x->whiteness;
		  p->contrast = spca50x->contrast;
		  p->brightness = spca50x->brightness;
		  p->hue = spca50x->hue;
		  break;
		}
	      }
	  
	spca50x->contrast = p->contrast;
	spca50x->brightness = p->brightness;
	spca50x->colour = p->colour;
	spca50x->hue = p->hue;
	spca50x->whiteness = p->whiteness;
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
	spca50x->nstable = 0;
	spca50x->nunstable = 0;
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */

	return 0;
      }
    case VIDIOCGCAPTURE:
      {
	int *vf = arg;

	PDEBUG (4, "VIDIOCGCAPTURE");
	*vf = 0;
	// no subcapture
	return -EINVAL;
      }
    case VIDIOCSCAPTURE:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_capture *vc = arg;
#else	
	struct video_capture vc1 ;
	struct video_capture *vc = &vc1;
	if(copy_from_user(vc,arg,sizeof(struct video_capture)))
	 return -EFAULT;
#endif	

	if (vc->flags)
	  return -EINVAL;
	if (vc->decimation)
	  return -EINVAL;

	return -EINVAL;
      }
    case VIDIOCSWIN:
      {
      	int result;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_window *vw = arg;
#else	
	struct video_window vw1 ;
	struct video_window *vw = &vw1;
	if(copy_from_user(vw,arg,sizeof(struct video_window)))
	 return -EFAULT;
#endif	
	
	PDEBUG (3, "VIDIOCSWIN: width=%d, height=%d, flags=%d",
		vw->width, vw->height, vw->flags);
	if (vw->x)
	  return -EINVAL;
	if (vw->y)
	  return -EINVAL;
	if (vw->width > (unsigned int) spca50x->maxwidth)
	  return -EINVAL;
	if (vw->height > (unsigned int) spca50x->maxheight)
	  return -EINVAL;
	if (vw->width < (unsigned int) spca50x->minwidth)
	  return -EINVAL;
	if (vw->height < (unsigned int) spca50x->minheight)
	  return -EINVAL;


	 result = spca5xx_restartMode(spca50x,vw->width,vw->height,spca50x->frame[0].format);

	if (result == 0)
	  {
	    spca50x->frame[0].width = vw->width;
	    spca50x->frame[0].height = vw->height;
	  }

	return result;
      }
    case VIDIOCGWIN:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_window *vw = arg;
#else	
	struct video_window vw1 ;
	struct video_window *vw = &vw1;
#endif		

	memset (vw, 0, sizeof (struct video_window));
	vw->x = 0;
	vw->y = 0;
	vw->width = spca50x->frame[0].width;
	vw->height = spca50x->frame[0].height;
	vw->flags = 0;

	PDEBUG (4, "VIDIOCGWIN: %dx%d", vw->width, vw->height);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
	if(copy_to_user(arg,vw,sizeof(struct video_capture)))
	return -EFAULT;
#endif	

	return 0;
      }
    case VIDIOCGMBUF:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_mbuf *vm=arg;
#else	
	struct video_mbuf vm1;
	struct video_mbuf *vm = &vm1;
#endif		
	int i;
	PDEBUG (2, "VIDIOCGMBUF: %p ", vm);
	memset (vm, 0, sizeof (struct video_mbuf));
	vm->size = SPCA50X_NUMFRAMES * MAX_DATA_SIZE;
	vm->frames = SPCA50X_NUMFRAMES;

	for (i = 0; i < SPCA50X_NUMFRAMES; i++)
	  {
	    vm->offsets[i] = MAX_DATA_SIZE * i;
	  }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
	if(copy_to_user(arg,vm,sizeof(struct video_mbuf)))
	return -EFAULT;
#endif	

	return 0;
      }
    case VIDIOCMCAPTURE:
      {
      	int ret, depth;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_mmap *vm = arg;
#else	
	struct video_mmap vm1;
	struct video_mmap *vm = &vm1;
	if(copy_from_user(vm,arg,sizeof(struct video_mmap)))
	 return -EFAULT;
#endif	
	

	PDEBUG (2, "CMCAPTURE");
	PDEBUG (2, "frame: %d, size: %dx%d, format: %d",
		vm->frame, vm->width, vm->height, vm->format);
	depth = spca5xx_get_depth (spca50x, vm->format);
	if (!depth || depth < spca50x->min_bpp)
	  {
	    err ("VIDIOCMCAPTURE: invalid format (%d)", vm->format);
	    return -EINVAL;
	  }

	if ((vm->frame < 0) || (vm->frame > 3))
	  {
	    err ("VIDIOCMCAPTURE: invalid frame (%d)", vm->frame);
	    return -EINVAL;
	  }

	if (vm->width > spca50x->maxwidth || vm->height > spca50x->maxheight)
	  {
	    err ("VIDIOCMCAPTURE: requested dimensions too big");
	    return -EINVAL;
	  }
	if (vm->width < spca50x->minwidth || vm->height < spca50x->minheight)
	  {
	    err ("VIDIOCMCAPTURE: requested dimensions too small");
	    return -EINVAL;
	  }
	/* 
	 * If we are grabbing the current frame, let it pass
	 */
	if (spca50x->frame[vm->frame].grabstate == FRAME_GRABBING)
	  {
	    PDEBUG (4, "MCAPTURE: already grabbing");
	    ret = wait_event_interruptible (spca50x->wq,
					    (spca50x->frame[vm->frame].
					     grabstate != FRAME_GRABBING));
	    if (ret)
	      return -EINTR;

	  }
	if ((spca50x->frame[vm->frame].width != vm->width) ||
	    (spca50x->frame[vm->frame].height != vm->height) ||
	    (spca50x->frame[vm->frame].format != vm->format))
	  {

	     ret = spca5xx_restartMode(spca50x,vm->width,vm->height,vm->format);

	    if (ret < 0)
	      return ret;
	    spca50x->frame[vm->frame].width = vm->width;
	    spca50x->frame[vm->frame].height = vm->height;
	    spca50x->frame[vm->frame].format = vm->format;
	    spca50x->frame[vm->frame].depth = depth;

	  }

	/* Mark it as ready */
	spca50x->frame[vm->frame].grabstate = FRAME_READY;
	return 0;
      }
    case VIDIOCSYNC:
      {
      	int ret;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
		 unsigned int frame = *((unsigned int *) arg);
#else	
		 unsigned int frame ;
		if(copy_from_user((void*)&frame,arg,sizeof(int)))
	 return -EFAULT;
#endif	
	

	PDEBUG (3, "syncing to frame %d, grabstate = %d", frame,
		spca50x->frame[frame].grabstate);

	switch (spca50x->frame[frame].grabstate)
	  {
	  case FRAME_UNUSED:
	    return -EINVAL;
	  case FRAME_ABORTING:
	    return -ENODEV;
	  case FRAME_READY:
	  case FRAME_GRABBING:
	  case FRAME_ERROR:
	  redo:
	    if (!spca50x->dev)
	      return -EIO;


	    ret = wait_event_interruptible (spca50x->wq,
					    (spca50x->frame[frame].
					     grabstate == FRAME_DONE));
	    if (ret)
	      return -EINTR;

	    PDEBUG (5, "Synched on frame %d, grabstate = %d",
		    frame, spca50x->frame[frame].grabstate);

	    if (spca50x->frame[frame].grabstate == FRAME_ERROR)
	      {
		goto redo;
	      }
	    /* Fallthrough.
	     * We have waited in state FRAME_GRABBING until it
	     * becomes FRAME_DONE, so now we can move along.
	     */
	  case FRAME_DONE:

	    PDEBUG (3, "Already synched %d\n", frame);

	    /* Release the current frame. This means that it
	     * will be reused as soon as all other frames are
	     * full, so the app better be done with it quickly.
	     * Can this be avoided somehow?
	     */
	    spca50x->frame[frame].grabstate = FRAME_UNUSED;

	    break;
	  }			/* end switch */

	return 0;
      }
    case VIDIOCGFBUF:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
		struct video_buffer *vb=arg;
#else	
		struct video_buffer vb1;
		struct video_buffer *vb= &vb1;
#endif	
	memset (vb, 0, sizeof (struct video_buffer));
	vb->base = NULL;	/* frame buffer not supported, not used */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
	if(copy_to_user(arg,vb,sizeof(struct video_buffer)))
	return -EFAULT;
#endif	

	return 0;
      }
    case VIDIOCKEY:
      return 0;
    case VIDIOCCAPTURE:
      return -EINVAL;
    case VIDIOCSFBUF:
      return -EINVAL;
    case VIDIOCGTUNER:
    case VIDIOCSTUNER:
      return -EINVAL;
    case VIDIOCGFREQ:
    case VIDIOCSFREQ:
      return -EINVAL;
    case VIDIOCGAUDIO:
    case VIDIOCSAUDIO:
      return -EINVAL;
    default:
      return -ENOIOCTLCMD;
    }				/* end switch */

  return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
static int
spca5xx_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
	       unsigned long arg)
{
	int rc;
#if 0	
	struct video_device *vdev = file->private_data;
	struct usb_spca50x *spca50x = video_get_drvdata(vdev);
	

/* lock the device here is dramatic for latencies */
	if (down_interruptible(&spca50x->lock))
		return -EINTR;
#endif
  rc = video_usercopy (inode, file, cmd, arg, spca5xx_do_ioctl);
#if 0
/* see 7 lines up */ */
	up(&spca50x->lock);
#endif
  return rc;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
static ssize_t
spca5xx_read (struct file *file, char *buf, size_t cnt, loff_t * ppos)
{

  struct video_device *dev = file->private_data;
  int noblock = file->f_flags & O_NONBLOCK;
  unsigned long count = cnt;
#else
static long 
spca5xx_read(struct video_device *dev, char * buf, unsigned long
	count,int noblock)
{
#endif
  struct usb_spca50x *spca50x = video_get_drvdata (dev);

  int i;
  int frmx = -1;
  int rc;
  volatile struct spca50x_frame *frame;

  PDEBUG (4, "%ld bytes, noblock=%d", count, noblock);
if (down_interruptible(&spca50x->lock))
		return -ERESTARTSYS;

  if (!dev || !buf){
   	up(&spca50x->lock);
    return -EFAULT;
}
  if (!spca50x->dev){
   	up(&spca50x->lock);
    return -EIO;
}
  if (!spca50x->streaming){
   	up(&spca50x->lock);
    return -EIO;
}
/* what noblock can do ?? */
/* FIXME make compiler happy */
noblock += 1;
  /* Wait while we're grabbing the image */
	PDEBUG(4, "Waiting for a frame done");
#if (SPCA50X_NUMFRAMES == 4)	
	if((rc = wait_event_interruptible(spca50x->wq,
		spca50x->frame[0].grabstate == FRAME_DONE ||
		spca50x->frame[1].grabstate == FRAME_DONE ||
		spca50x->frame[2].grabstate == FRAME_DONE ||
		spca50x->frame[3].grabstate == FRAME_DONE ))){
	 	up(&spca50x->lock);
		return rc;
	}
#else // should be two 
	if((rc = wait_event_interruptible(spca50x->wq,
		spca50x->frame[0].grabstate == FRAME_DONE ||
		spca50x->frame[1].grabstate == FRAME_DONE ))){
	 	up(&spca50x->lock);
		return rc;
	}
#endif

  /* One frame has just been set to DONE. Find it. */
  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    if (spca50x->frame[i].grabstate == FRAME_DONE)
      frmx = i;

  PDEBUG (4, "Frame number: %d", frmx);
  if (frmx < 0)
    {
      /* We havent found a frame that is DONE. Damn. Should
       * not happen. */
      PDEBUG (2, "Couldnt find a frame ready to be read.");
      	up(&spca50x->lock);
      return -EFAULT;
    }
  frame = &spca50x->frame[frmx];
  PDEBUG (2, "count asked: %d available: %d", (int) count,
	  (int) frame->scanlength);

  if (count > frame->scanlength)
    count = frame->scanlength;

  if ((i = copy_to_user (buf, frame->data, count)))
    {
      PDEBUG (2, "Copy failed! %d bytes not copied", i);
      	up(&spca50x->lock);
      return -EFAULT;
    }
  /* Release the frame */
  frame->grabstate = FRAME_READY;

up(&spca50x->lock);
  return count;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
static int
spca5xx_mmap (struct file *file, struct vm_area_struct *vma)
{

  struct video_device *dev = file->private_data;
  unsigned long start = vma->vm_start;
  unsigned long size = vma->vm_end - vma->vm_start;
#else
static int 
spca5xx_mmap(struct video_device *dev,const char *adr, unsigned long size)
{
  unsigned long start=(unsigned long) adr;
#endif  
  struct usb_spca50x *spca50x = video_get_drvdata (dev);
  unsigned long page, pos;

  if (spca50x->dev == NULL)
    return -EIO;

  PDEBUG (4, "mmap: %ld (%lX) bytes", size, size);

  if (size >
      (((SPCA50X_NUMFRAMES * MAX_DATA_SIZE) + PAGE_SIZE - 1) & ~(PAGE_SIZE -
								 1)))
    return -EINVAL;
	if (down_interruptible(&spca50x->lock))
		return -EINTR;

  pos = (unsigned long) spca50x->fbuf;
  while (size > 0)
    {
      page = kvirt_to_pa (pos);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0) || defined (RH9_REMAP)
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
      if (remap_pfn_range
	  (vma, start, page >> PAGE_SHIFT, PAGE_SIZE, PAGE_SHARED)){
#else
      if (remap_page_range (vma, start, page, PAGE_SIZE, PAGE_SHARED)){
#endif /* KERNEL 2.6.9 */
#else /* RH9_REMAP */
      if (remap_page_range (start, page, PAGE_SIZE, PAGE_SHARED)){
#endif /* RH9_REMAP */
	up(&spca50x->lock);
	return -EAGAIN;
	}
      start += PAGE_SIZE;
      pos += PAGE_SIZE;
      if (size > PAGE_SIZE)
	size -= PAGE_SIZE;
      else
	size = 0;
    }
up(&spca50x->lock);
  return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,4,22)
static struct file_operations spca5xx_fops = {
  .owner = THIS_MODULE,
  .open = spca5xx_open,
  .release = spca5xx_close,
  .read = spca5xx_read,
  .mmap = spca5xx_mmap,
  .ioctl = spca5xx_ioctl,
  .llseek = no_llseek,
};
static struct video_device spca50x_template = {
  .owner = THIS_MODULE,
  .name = "SPCA5XX USB Camera",
  .type = VID_TYPE_CAPTURE,
  .hardware = VID_HARDWARE_SPCA5XX,
  .fops = &spca5xx_fops,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
  .release = video_device_release,
#endif
  .minor = -1,
};
#else
static struct video_device spca50x_template = {
	name: "SPCA5XX USB Camera",
	type:	VID_TYPE_CAPTURE,
	hardware:VID_HARDWARE_SPCA5XX,
	open:   spca5xx_open,
	close:  spca5xx_close,
  	read:   spca5xx_read,
  	mmap:   spca5xx_mmap,
  	ioctl:  spca5xx_ioctl
};
#endif //KERNEL VERSION 2,4,22
/****************************************************************************
 *
 * SPCA50X configuration
 *
 ***************************************************************************/

static int
spca50x_configure_sensor (struct usb_spca50x *spca50x)
{
 
 int err;
  PDEBUG (4, "starting configuration");
  /* Set sensor-specific vars */
     err = spca5xx_getcapability(spca50x);
  return 0;
}


static int
spca50x_configure (struct usb_spca50x *spca50x)
{
int i;
  PDEBUG (2, "video_register_device succeeded");
  /* Initialise the camera bridge */
  switch (spca50x->bridge)
    {
    
    case BRIDGE_SN9CXXX:   
      if(sonix_config(spca50x) < 0)
      	goto error;
    break;
    case BRIDGE_ZC3XX:
    
      if(zc3xx_config(spca50x) < 0)
      	goto error;
    break;

    case BRIDGE_SPCA536:
    case BRIDGE_SPCA533:
    case BRIDGE_SPCA504:
    case BRIDGE_SPCA504B:
    case BRIDGE_SPCA504C:
   
      if (sp5xxfw2_config (spca50x) < 0)
	  goto error;
    break;

    default:
      {
	err ("Unknown bridge type");
	goto error;
      }
    }

  spca50x_set_packet_size (spca50x, 0);

      i = spca5xx_getDefaultMode(spca50x);
      if (i < 0) return i;
  
  if (spca50x_configure_sensor (spca50x) < 0)
    {
      err ("failed to configure");
      goto error;
    }
  /* configure the frame detector with default parameters */
  
  spca5xx_setFrameDecoder(spca50x);
  
  PDEBUG (2, "Spca5xx Configure done !!");
  return 0;

error:

  return -EBUSY;
}


/************************************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
/****************************************************************************
 *  sysfs
 ***************************************************************************/

static inline struct usb_spca50x *
cd_to_spca50x (struct class_device *cd)
{
  struct video_device *vdev = to_video_device (cd);
  return video_get_drvdata (vdev);
}

static ssize_t
show_stream_id (struct class_device *cd, char *buf)
{
  struct usb_spca50x *spca50x = cd_to_spca50x (cd);
  return snprintf (buf, 5, "%s\n", Plist[spca50x->cameratype].name);
}

static CLASS_DEVICE_ATTR (stream_id, S_IRUGO, show_stream_id, NULL);

static ssize_t
show_model (struct class_device *cd, char *buf)
{
  struct usb_spca50x *spca50x = cd_to_spca50x (cd);
  return snprintf (buf, 32, "%s\n", (spca50x->desc) ?
		   clist[spca50x->desc].description : " Unknow ");
}

static CLASS_DEVICE_ATTR (model, S_IRUGO, show_model, NULL);

/*

static ssize_t show_brightness(struct class_device *cd, char *buf)
{
	struct usb_spca50x *spca50x = cd_to_spca50x(cd);
	unsigned short x;

	if (!spca50x->dev)
		return -ENODEV;
	sensor_get_brightness(spca50x, &x);
	return sprintf(buf, "%d\n", x >> 8);
} 
static CLASS_DEVICE_ATTR(brightness, S_IRUGO, show_brightness, NULL);

static ssize_t show_saturation(struct class_device *cd, char *buf)
{
	struct usb_spca50x *spca50x = cd_to_spca50x(cd);
	unsigned short x;

	if (!spca50x->dev)
		return -ENODEV;
	sensor_get_saturation(spca50x, &x);
	return sprintf(buf, "%d\n", x >> 8);
} 
static CLASS_DEVICE_ATTR(saturation, S_IRUGO, show_saturation, NULL);

static ssize_t show_contrast(struct class_device *cd, char *buf)
{
	struct usb_spca50x *spca50x = cd_to_spca50x(cd);
	unsigned short x;

	if (!spca50x->dev)
		return -ENODEV;
	sensor_get_contrast(spca50x, &x);
	return sprintf(buf, "%d\n", x >> 8);
} 
static CLASS_DEVICE_ATTR(contrast, S_IRUGO, show_contrast, NULL);

static ssize_t show_hue(struct class_device *cd, char *buf)
{
	struct usb_spca50x *spca50x = cd_to_spca50x(cd);
	unsigned short x;

	if (!spca50x->dev)
		return -ENODEV;
	sensor_get_hue(spca50x, &x);
	return sprintf(buf, "%d\n", x >> 8);
} 
static CLASS_DEVICE_ATTR(hue, S_IRUGO, show_hue, NULL);

static ssize_t show_exposure(struct class_device *cd, char *buf)
{
	struct usb_spca50x *spca50x = cd_to_spca50x(cd);
	unsigned char exp;

	if (!spca50x->dev)
		return -ENODEV;
	sensor_get_exposure(spca50x, &exp);
	return sprintf(buf, "%d\n", exp >> 8);
} 
static CLASS_DEVICE_ATTR(exposure, S_IRUGO, show_exposure, NULL);
*/

static void
spca50x_create_sysfs (struct video_device *vdev)
{

  video_device_create_file (vdev, &class_device_attr_stream_id);
  video_device_create_file (vdev, &class_device_attr_model);
/*	
	video_device_create_file(vdev, &class_device_attr_RGB);

	video_device_create_file(vdev, &class_device_attr_brightness);
	video_device_create_file(vdev, &class_device_attr_saturation);
	video_device_create_file(vdev, &class_device_attr_contrast);
	video_device_create_file(vdev, &class_device_attr_hue);
	video_device_create_file(vdev, &class_device_attr_exposure);
*/
}
#endif
/****************************************************************************
 *
 *  USB routines
 *
 ***************************************************************************/
static int
spcaDetectCamera (struct usb_spca50x *spca50x)
{
  struct usb_device *dev = spca50x->dev;
  __u8 fw = 0;
  __u16 vendor;
  __u16 product;
  /* Is it a recognised camera ? */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
vendor = le16_to_cpu(dev->descriptor.idVendor);
product = le16_to_cpu(dev->descriptor.idProduct);
#else
vendor = dev->descriptor.idVendor;
product = dev->descriptor.idProduct;
#endif
  switch ( vendor )
    {
    case 0x0733:		/* Rebadged ViewQuest (Intel) and ViewQuest cameras */
      switch ( product )
	{

	case 0x1314:
		spca50x->desc = Mercury21;
		spca50x->bridge = BRIDGE_SPCA533;
		spca50x->sensor = SENSOR_INTERNAL;
		spca50x->header_len = SPCA533_OFFSET_DATA;
		spca50x->i2c_ctrl_reg = 0;
		spca50x->i2c_base = 0;
		spca50x->i2c_trigger_on_write = 0;
		spca50x->cameratype = JPEG;
		info ("USB SPCA5XX camera found. Mercury Digital Pro 2.1Mp ");
		break;
		
	case 0x2211:
	  spca50x->desc = Jenoptikjdc21lcd;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Jenoptik JDC 21 LCD");
	  break;

	case 0x2221:
	  spca50x->desc = MercuryDigital;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Mercury Digital Pro 3.1Mp ");
	  break;

	case 0x1311:
	  spca50x->desc = Epsilon13;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Digital Dream Epsilon 1.3");
	  break;

	 case 0x3261:
	  spca50x->desc = Concord3045;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found.Concord 3045 Spca536 Mpeg4");
	  break;
	  case 0x3281:
	  spca50x->desc = CyberpixS550V;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found.Mercury Cyberpix Spca536 Mpeg4");
	  break;
	default:
	  goto error;
	};
      break;

    case 0x04a5:		/* Benq */
    case 0x08ca:		/* Aiptek */
    case 0x055f:		/* Mustek cameras */
    case 0x04fc:		/* SunPlus */
    case 0x052b:		/* ?? Megapix */
    case 0x04f1:		/* JVC */
      switch (product)
	{
	case 0xc520:
	  spca50x->desc = MustekGsmartMini3;
	  spca50x->bridge = BRIDGE_SPCA504;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Mustek gSmart Mini 3(SPCA504A)");
	  break;
	case 0xc420:
	  spca50x->desc = MustekGsmartMini2;
	  spca50x->bridge = BRIDGE_SPCA504;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Mustek gSmart Mini 2(SPCA504A)");
	  break;

	case 0xc360:
	  spca50x->desc = MustekDV4000;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Mustek DV4000 Spca536 Mpeg4");
	  break;

	case 0xc211:
	  spca50x->desc = Bs888e;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Kowa Bs-888e Spca536 Mpeg4");
	  break;

	case 0xc005:		// zc302 chips 
	  spca50x->desc = Wcam300A;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Mustek Wcam300a Zc0301 ");
	  break;

	case 0xd003:		// zc302 chips 
	  spca50x->desc = MustekWcam300A;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Mustek PCCam300a Zc0301 ");
	  break;
	 case 0xd004:		// zc302 chips 
	  spca50x->desc = WCam300AN;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Mustek WCam300aN Zc0302 ");
	 break;
	case 0x504a:
	  /*try to get the firmware as some cam answer 2.0.1.2.2 
	     and should be a spca504b then overwrite that setting */
	  spca5xxRegRead (dev, 0x20, 0, 0, &fw, 1);
	  if (fw == 1)
	    {
	      spca50x->desc = AiptekMiniPenCam13;
	      spca50x->bridge = BRIDGE_SPCA504;
	      spca50x->sensor = SENSOR_INTERNAL;
	      spca50x->header_len = SPCA50X_OFFSET_DATA;
	      spca50x->i2c_ctrl_reg = 0;
	      spca50x->i2c_base = 0;
	      spca50x->i2c_trigger_on_write = 0;
	      spca50x->cameratype = JPEG;
	      info
		("USB SPCA5XX camera found. Type Aiptek mini PenCam 1.3(SPCA504A)");
	    }
	  else if (fw == 2)
	    {
	      spca50x->desc = Terratec2move13;
	      spca50x->bridge = BRIDGE_SPCA504B;
	      spca50x->sensor = SENSOR_INTERNAL;
	      spca50x->header_len = SPCA50X_OFFSET_DATA;
	      spca50x->i2c_ctrl_reg = 0;
	      spca50x->i2c_base = 0;
	      spca50x->i2c_trigger_on_write = 0;
	      spca50x->cameratype = JPEG;
	      info
		("USB SPCA5XX camera found. Terratec 2 move1.3(SPCA504A FW2)");
	    }
	  else
	    return -ENODEV;
	  break;

	case 0x2018:

	  spca50x->desc = AiptekPenCamSD;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek PenCam SD(SPCA504A FW2)");
	  break;
	case 0x1001:

	  spca50x->desc = JvcGcA50;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. JVC GC-A50(SPCA504A FW2)");
	  break;
	case 0x2008:

	  spca50x->desc = AiptekMiniPenCam2;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek PenCam 2M(SPCA504A FW2)");
	  break;

	case 0x504b:

	  spca50x->desc = MaxellMaxPocket;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Maxell MaxPocket 1.3 (SPCA504A FW2)");
	  break;

	case 0xffff:

	  spca50x->desc = PureDigitalDakota;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Pure Digital Dakota (SPCA504A FW2)");
	  break;

	case 0x0104:

	  spca50x->desc = AiptekPocketDVII;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek PocketDVII 1.3Mp");
	  break;

	case 0x0106:

	  spca50x->desc = AiptekPocketDV3100;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek PocketDV3100+");
	  break;
	  
	case 0xc630:

	  spca50x->desc = MustekMDC4000;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Musteck MDC4000");
	  break;
	  
	case 0x5330:

	  spca50x->desc = Digitrex2110;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. ApexDigital Digitrex 2110 spca533");
	  break;

	case 0x2022:

	  spca50x->desc = AiptekSlim3200;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found type: Aiptek Slim3 200 spca533");
	  break;
	  
	case 0x2028:

	  spca50x->desc = AiptekPocketCam4M;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found type: Aiptek PocketCam 4M spca533");
	  break; 
	   
	case 0x5360:
	
	  spca50x->desc = SunplusGeneric536;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek Generic spca536a");
	  break;
	  
	case 0x2024:

	  spca50x->desc = AiptekDV3500;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek DV3500 Mpeg4");
	  break;
	  
	case 0x2042:
	
	  spca50x->desc = AiptekPocketDV5100;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek DV5100 Mpeg4");
	  break;
	  
	  case 0x2060:
	
	  spca50x->desc = AiptekPocketDV5300;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek DV5300 Mpeg4");
	  break;
	  
	case 0x3008:

	  spca50x->desc = BenqDC1500;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Benq DC 1500 Spca533");
	  break;

	case 0x3003:

	  spca50x->desc = BenqDC1300;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Benq DC 1300 Spca504b");
	  break;

	case 0x300a:

	  spca50x->desc = BenqDC3410;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Benq DC 3410 Spca533");
	  break;

	case 0x2010:

	  spca50x->desc = AiptekPocketCam3M;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek PocketCam 3M");
	  break;

	case 0x2016:

	  spca50x->desc = AiptekPocketCam2M;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Aiptek PocketCam 2 Mega (SPCA504A FW2)");
	  break;

	case 0xc530:
	  spca50x->desc = MustekGsmartLCD3;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Mustek Gsmart LCD 3");
	  break;

	case 0xc430:
	  spca50x->desc = MustekGsmartLCD2;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Mustek Gsmart LCD 2");
	  break;

	case 0xc440:

	  spca50x->desc = MustekDV3000;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. DV3000");
	  break;

	case 0xc540:

	  spca50x->desc = GsmartD30;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found.Mustek Gsmart D30");
	  break;
	case 0xc650:

	  spca50x->desc = MustekMDC5500Z;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found Mustek MDC5500Z");
	  break;

	case 0x1513:


	  spca50x->desc = MegapixV4;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found Megapix V4");

	  break;

	default:
	  goto error;
	};
      break;
    case 0x046d:		/* Logitech Labtec*/
    case 0x041E:		/* Creative cameras */
      switch (product)
	{

	  
	case 0x4012:
	  spca50x->desc = PcCam350;
	  spca50x->bridge = BRIDGE_SPCA504C;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA504_PCCAM600_OFFSET_DATA;;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Creative PC-CAM 350 (SPCA504c+unknown CCD)");
	break;
	

	case 0x08a0:
	  spca50x->desc = QCim;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Logitech QC IM ");
	  break;
	  
	case 0x08a1:
	
	  spca50x->desc = QCimA1;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Logitech QC IM ");
	  break;
	  
	case 0x08a2:		// zc302 chips 
	
	  spca50x->desc = LabtecPro;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_HDCS2020;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Labtec Webcam Pro Zc0302 + Hdcs2020");
	  break;
	  
	case 0x08a3:
	
	  spca50x->desc = QCchat;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Logitech QC Chat ");
	  break;
	  
	 case 0x08a9:
	
	  spca50x->desc = LogitechNotebookDeluxe;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_HDCS2020;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Logitech Notebooks Deluxe Zc0302 + Hdcs2020");
	  break;
	case 0x08ae:
	
	  spca50x->desc = QuickCamNB;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_HDCS2020;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Logitech QC for Notebooks ");
	  break;
	   
	 case 0x08ad:
	  spca50x->desc = LogitechQCCommunicateSTX;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_HV7131C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Logitech QC Communicate STX ");
	  break;
	  
	 case 0x08aa:
	  spca50x->desc = LabtecNotebook;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_HDCS2020;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Labtec for Notebooks ");
	  break;
	  
	case 0x08b9:
	  spca50x->desc = QCimB9;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Logitech QC IM ??? ");
	  break;

	case 0x0905:
	  spca50x->desc = LogitechClickSmart820;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Logitech ClickSmart 820 (SPCA533+unknown CCD)");
	  break;
	case 0x400B:
	  spca50x->desc = CreativePCCam600;
	  spca50x->bridge = BRIDGE_SPCA504C;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA504_PCCAM600_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Creative PC-CAM 600 (SPCA504+unknown CCD)");
	  break;
	case 0x4013:
	  spca50x->desc = CreativePccam750;
	  spca50x->bridge = BRIDGE_SPCA504C;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA504_PCCAM600_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Creative PC-CAM 750 (SPCA504+unknown CCD)");
	  break;
	case 0x0960:
	  spca50x->desc = LogitechClickSmart420;
	  spca50x->bridge = BRIDGE_SPCA504C;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA504_PCCAM600_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Logitech Clicksmart 420 (SPCA504+unknown CCD)");
	  break;

	case 0x401c:		// zc301 chips 
	  spca50x->desc = CreativeNX;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_PAS106;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative NX Zc301+ CCD PAS106B");
	  break;
	case 0x401e:		// zc301 chips 
	  spca50x->desc = CreativeNxPro;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_HV7131B;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative NX Pro Zc301+hv7131b");
	  break;
	case 0x4034:		// zc301 chips 
	  spca50x->desc = CreativeInstant1;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_PAS106;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative Instant P0620");
	  break; 
	case 0x4035:		// zc301 chips 
	  spca50x->desc = CreativeInstant2;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_PAS106;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative Instant P0620D");
	  break;    
	case 0x403a:
	  spca50x->desc = CreativeNxPro2;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative Nx Pro FW2 Zc301+Tas5130c");
	  break;

	case 0x4036:
	  spca50x->desc = CreativeLive;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative Live! Zc301+Tas5130c");
	  break;
	case 0x401f:		// zc301 chips 
	  spca50x->desc = CreativeNotebook;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative Webcam Notebook Zc301+Tas5130c");
	  break;
	case 0x4017:		// zc301 chips 
	  spca50x->desc = CreativeMobile;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_ICM105A;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative Webcam Mobile Zc301+Icm105a");
	  break;

	default:
	  goto error;
	};
      break;
    case 0x0AC8:		/* Vimicro z-star */
      switch (product)
	{
	case 0x301b:		/* Wasam 350r */
	  spca50x->desc = Vimicro;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_PB0330;	//overwrite by the sensor detect routine
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info ("USB SPCA5XX camera found. Type Vimicro Zc301P 0x301b");

	  break;
	case 0x303b:		/* Wasam 350r */
	  spca50x->desc = Vimicro303b;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_PB0330;	//overwrite by the sensor detect routine
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info ("USB SPCA5XX camera found. Type Vimicro Zc301P 0x303b");

	  break;  
	case 0x305b:	/* Generic */
	  spca50x->desc = Zc0305b;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;	//overwrite by the sensor detect routine
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info ("USB SPCA5XX camera found. Type Vimicro Zc305B 0x305b");
	break;
	
	case 0x0302:		/* Generic */
	  spca50x->desc = Zc302;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_ICM105A;	//overwrite by the sensor detect routine
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info ("USB SPCA5XX camera found. Type Vimicro Zc302 ");

	  break;

	default:
	  goto error;
	};
      break;

    case 0x0458:		/* Genius KYE cameras */
      switch (product)
	{
	case 0x7006:
	  spca50x->desc = GeniusDsc13;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Genius DSC 1.3 Smart Spca504B");
	  break;
	case 0x7007:		// zc301 chips 
	  spca50x->desc = GeniusVideoCamV2;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Genius VideoCam V2 Zc301+Tas5130c");
	  break;

	case 0x700c:		// zc301 chips 
	  spca50x->desc = GeniusVideoCamV3;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Genius VideoCam V3 Zc301+Tas5130c");
	  break;

	case 0x700f:		// zc301 chips 
	  spca50x->desc = GeniusVideoCamExpressV2b;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Genius VideoCam Express V2 Zc301+Tas5130c");
	  break;
	default:
	  goto error;
	};
      break;

    case 0x10fd:		/* FlyCam usb 100  */
      switch (product)
	{

	case 0x0128:
	case 0x8050:		// zc301 chips
	  spca50x->desc = TyphoonWebshotIIUSB300k;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Typhoon Webshot II Zc301p Tas5130c");
	  break;

	default:
	  goto error;
	};
      break;
    case 0x0461:		/* MicroInnovation  */
      switch (product)
	{

	case 0x0a00:		// zc301 chips 
	  spca50x->desc = WebCam320;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Micro Innovation PC Cam 300A Zc301");
	  break;
	default:
	  goto error;
	};
      break;

    case 0x05da:		/* Digital Dream cameras */
      switch (product)
	{
	case 0x1018:
	  spca50x->desc = Enigma13;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;

	  info ("USB SPCA5XX camera found. Digital Dream Enigma 1.3");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x0c45:		/* Sonix6025 TAS 5130d1b */
      switch (product)
	{
	case 0x607c:
	  spca50x->desc = SonixWC311P;
	  spca50x->bridge = BRIDGE_SN9CXXX;
	  spca50x->sensor = SENSOR_HV7131R;
	  spca50x->customid = SN9C102P;
	  spca50x->header_len = 0;
	  spca50x->i2c_ctrl_reg = 0x81;
	  spca50x->i2c_base = 0x11;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGS;	// jpeg 4.2.2 whithout header ;
	  info ("USB SPCA5XX camera found. SONIX sn9c102p + Hv7131R ");
	  break;
	case 0x613c:
	  spca50x->desc = Pccam168;
	  spca50x->bridge = BRIDGE_SN9CXXX;
	  spca50x->sensor = SENSOR_HV7131R;
	  spca50x->customid = SN9C120;
	  spca50x->header_len = 0;
	  spca50x->i2c_ctrl_reg = 0x81;
	  spca50x->i2c_base = 0x11;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGS;	// jpeg 4.2.2 whithout header ;
	  info ("USB SPCA5XX camera found. SONIX sn9c120 + Hv7131R ");
	  break;
	case 0x6130:
	  spca50x->desc = Pccam;
	  spca50x->bridge = BRIDGE_SN9CXXX;
	  spca50x->sensor = SENSOR_MI0360;
	  spca50x->customid = SN9C120;
	  spca50x->header_len = 0;
	  spca50x->i2c_ctrl_reg = 0x81;
	  spca50x->i2c_base = 0x5d;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGS;	// jpeg 4.2.2 whithout header ;
	  info ("USB SPCA5XX camera found. SONIX sn9c120 + MI0360 ");
	  break;
	case 0x60c0:
	  spca50x->desc = Sn535;
	  spca50x->bridge = BRIDGE_SN9CXXX;
	  spca50x->sensor = SENSOR_MI0360;
	  spca50x->customid = SN9C105;
	  spca50x->header_len = 0;
	  spca50x->i2c_ctrl_reg = 0x81;
	  spca50x->i2c_base = 0x5d;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGS;	// jpeg 4.2.2 whithout header ;
	  info ("USB SPCA5XX camera found. SONIX sn9c105 + MI0360 ");
	  break;
	case 0x60fc:
	  spca50x->desc = Lic300;
	  spca50x->bridge = BRIDGE_SN9CXXX;
	  spca50x->sensor = SENSOR_HV7131R;
	  spca50x->customid = SN9C105;
	  spca50x->header_len = 0;
	  spca50x->i2c_ctrl_reg = 0x81;
	  spca50x->i2c_base = 0x11;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGS;	// jpeg 4.2.2 whithout header ;
	  info ("USB SPCA5XX camera found. SONIX sn9c105 + HV7131R ");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x0546:		/* Polaroid */
      switch (product)
	{
	case 0x3273:
	  spca50x->desc = PolaroidPDC2030;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;

	  info ("USB SPCA5XX camera found. Polaroid PDC 2030");
	  break;

	case 0x3155:

	  spca50x->desc = PolaroidPDC3070;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Polaroid PDC 3070");
	  break;
	  
	case 0x3191:

	  spca50x->desc = PolaroidIon80;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found.Polaroid Ion80 (SPCA504A FW2)");
	  break;

	default:
	  goto error;
	};
      break;

    default:
      goto error;
    }
  return 0;
error:
  return -ENODEV;
}

 
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
static int
spca5xx_probe (struct usb_interface *intf, const struct usb_device_id *id)
#else
static void *
spca5xx_probe (struct usb_device *dev, unsigned int ifnum,
	       const struct usb_device_id *id)
#endif
{
  struct usb_interface_descriptor *interface;
  struct usb_spca50x *spca50x;
  int err_probe;
  int i;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
  struct usb_device *dev = interface_to_usbdev (intf);
#endif
  /* We don't handle multi-config cameras */
  if (dev->descriptor.bNumConfigurations != 1)
    goto nodevice;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  /* 2.4.x test Only work on Interface 0 */
  if (ifnum > 0)
    goto nodevice;

  interface = &dev->actconfig->interface[ifnum].altsetting[0];
  /* Since code below may sleep, we use this as a lock */
  MOD_INC_USE_COUNT;
#else
  /* 2.6.x test Only work on Interface 0 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,6)
  interface = &intf->cur_altsetting->desc;
#else
  interface = &intf->altsetting[0].desc;
#endif
  if (interface->bInterfaceNumber > 0)
    goto nodevice;
#endif


  if ((spca50x = kmalloc (sizeof (struct usb_spca50x), GFP_KERNEL)) == NULL)
    {
      err ("couldn't kmalloc spca50x struct");
      goto error;
    }

  memset (spca50x, 0, sizeof (struct usb_spca50x));

  spca50x->dev = dev;
  spca50x->iface = interface->bInterfaceNumber;
  if ((err_probe = spcaDetectCamera (spca50x)) < 0)
    {
      err (" Devices not found !! ");
      /* FIXME kfree spca50x and goto nodevice */
      goto error;
    }
  PDEBUG (0, "Camera type %s ", Plist[spca50x->cameratype].name);

  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    init_waitqueue_head (&spca50x->frame[i].wq);
  init_waitqueue_head (&spca50x->wq);

  if (!spca50x_configure (spca50x))
    {
      spca50x->user = 0;
      init_MUTEX (&spca50x->lock);	/* to 1 == available */
      init_MUTEX (&spca50x->buf_lock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
      spin_lock_init(&spca50x->v4l_lock);
#else
      spca50x->v4l_lock = SPIN_LOCK_UNLOCKED;
#endif
      spca50x->buf_state = BUF_NOT_ALLOCATED;
    }
  else
    {
      err ("Failed to configure camera");
      goto error;
    }
  /* Init video stuff */
  spca50x->vdev = video_device_alloc ();
  if (!spca50x->vdev)
    goto error;
  memcpy (spca50x->vdev, &spca50x_template, sizeof (spca50x_template));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
  spca50x->vdev->dev = &dev->dev;
#endif
  video_set_drvdata (spca50x->vdev, spca50x);

  PDEBUG (2, "setting video device = %p, spca50x = %p", spca50x->vdev,
	  spca50x);

  if (video_register_device (spca50x->vdev, VFL_TYPE_GRABBER, video_nr) < 0)
    {
      err ("video_register_device failed");
      goto error;
    }
  /* test on disconnect */
  spca50x->present = 1;


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
  usb_set_intfdata (intf, spca50x);
  spca50x_create_sysfs (spca50x->vdev);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  MOD_DEC_USE_COUNT;
  return spca50x;
#else
  return 0;
#endif

error:
  if (spca50x->vdev)
    {
      if (spca50x->vdev->minor == -1)
	video_device_release (spca50x->vdev);
      else
	video_unregister_device (spca50x->vdev);
      spca50x->vdev = NULL;
    }
  if (spca50x)
    {
      kfree (spca50x);
      spca50x = NULL;
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  MOD_DEC_USE_COUNT;
  return NULL;
#else
  return -EIO;
#endif

nodevice:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  return NULL;
#else
  return -ENODEV;
#endif
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
static void
spca5xx_disconnect (struct usb_interface *intf)
{
  struct usb_spca50x *spca50x = usb_get_intfdata (intf);
#else
static void
spca5xx_disconnect (struct usb_device *dev, void *ptr)
{
  struct usb_spca50x *spca50x = (struct usb_spca50x *) ptr;
#endif
  int n;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  MOD_INC_USE_COUNT;
#endif
  if (!spca50x)
    return;
  down (&spca50x->lock);
  spca50x->present = 0;
  for (n = 0; n < SPCA50X_NUMFRAMES; n++)
    spca50x->frame[n].grabstate = FRAME_ABORTING;
  spca50x->curframe = -1;

  /* This will cause the process to request another frame */
  for (n = 0; n < SPCA50X_NUMFRAMES; n++)
    if (waitqueue_active (&spca50x->frame[n].wq))
      wake_up_interruptible (&spca50x->frame[n].wq);

  if (waitqueue_active (&spca50x->wq))
    wake_up_interruptible (&spca50x->wq);

  spca5xx_kill_isoc(spca50x);

  PDEBUG (3,"Disconnect Kill isoc done");
  up (&spca50x->lock);
  while(spca50x->user) 
  	schedule();
    {
      down (&spca50x->lock);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
      /* be sure close did not use &intf->dev ? */
      dev_set_drvdata (&intf->dev, NULL);
#endif
      /* We don't want people trying to open up the device */
      if (spca50x->vdev)
	video_unregister_device (spca50x->vdev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
      usb_driver_release_interface (&spca5xx_driver,
				    &spca50x->dev->actconfig->
				    interface[spca50x->iface]);
#endif
      spca50x->dev = NULL;
      up (&spca50x->lock);

      /* Free the memory */
      if (spca50x && !spca50x->user)
	{
	  spca5xx_dealloc (spca50x);
	  kfree (spca50x);
	  spca50x = NULL;
	}
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  MOD_DEC_USE_COUNT;
#endif
  PDEBUG (3, "Disconnect complete");

}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)
static struct usb_driver spca5xx_driver = {
  .owner = THIS_MODULE,
  .name = "spca5xx",
  .id_table = device_table,
  .probe = spca5xx_probe,
  .disconnect = spca5xx_disconnect
};
#else
static struct usb_driver spca5xx_driver = {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,20)
	  THIS_MODULE,
#endif
	"spca5xx",
	spca5xx_probe,
	spca5xx_disconnect,
	{NULL,NULL}
};
#endif


/****************************************************************************
 *
 *  Module routines
 *
 ***************************************************************************/

static int __init
usb_spca5xx_init (void)
{

  if (usb_register (&spca5xx_driver) < 0)
    return -1;

  info ("spca5xx driver %s registered", version);

  return 0;
}

static void __exit
usb_spca5xx_exit (void)
{
  usb_deregister (&spca5xx_driver);
  info ("driver spca5xx deregistered");


}

module_init (usb_spca5xx_init);
module_exit (usb_spca5xx_exit);

//eof
