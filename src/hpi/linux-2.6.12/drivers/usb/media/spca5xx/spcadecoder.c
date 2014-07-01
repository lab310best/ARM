/*******************************************************************************
#	 	spcadecoder: Generic decoder for various input stream yyuv	#
# yuyv yuvy jpeg411 jpeg422 bayer rggb with gamma correct			#
# and various output palette rgb16 rgb24 rgb32 yuv420p				#
# various output size with crop feature						#
# 		Copyright (C) 2003 2004 2005 Michel Xhaard			#
# 		mxhaard@magic.fr						#
# 		Sonix Decompressor by B.S. (C) 2004				#
# 		Spca561decoder (C) 2005 Andrzej Szombierski [qq@kuku.eu.org]	#
# This program is free software; you can redistribute it and/or modify		#
# it under the terms of the GNU General Public License as published by		#
# the Free Software Foundation; either version 2 of the License, or		#
# (at your option) any later version.						#
#										#
# This program is distributed in the hope that it will be useful,		#
# but WITHOUT ANY WARRANTY; without even the implied warranty of		#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the			#
# GNU General Public License for more details.					#
#										#
# You should have received a copy of the GNU General Public License		#
# along with this program; if not, write to the Free Software			#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	#
********************************************************************************/


#ifndef __KERNEL__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else /* __KERNEL__ */
#include <linux/string.h>
#endif /* __KERNEL__ */


#include "spcadecoder.h"


/* special markers */
#define M_BADHUFF	-1

#define ERR_CORRUPTFRAME 16

#define JPEGHEADER_LENGTH 589

const unsigned char JPEGHeader[JPEGHEADER_LENGTH] =
{
 0xff, 0xd8, 0xff, 0xdb, 0x00, 0x84, 0x00, 0x06, 0x04, 0x05, 0x06, 0x05, 0x04, 0x06, 0x06, 0x05,
 0x06, 0x07, 0x07, 0x06, 0x08, 0x0a, 0x10, 0x0a, 0x0a, 0x09, 0x09, 0x0a, 0x14, 0x0e, 0x0f, 0x0c,
 0x10, 0x17, 0x14, 0x18, 0x18, 0x17, 0x14, 0x16, 0x16, 0x1a, 0x1d, 0x25, 0x1f, 0x1a, 0x1b, 0x23,
 0x1c, 0x16, 0x16, 0x20, 0x2c, 0x20, 0x23, 0x26, 0x27, 0x29, 0x2a, 0x29, 0x19, 0x1f, 0x2d, 0x30,
 0x2d, 0x28, 0x30, 0x25, 0x28, 0x29, 0x28, 0x01, 0x07, 0x07, 0x07, 0x0a, 0x08, 0x0a, 0x13, 0x0a,
 0x0a, 0x13, 0x28, 0x1a, 0x16, 0x1a, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0xff, 0xc4, 0x01, 0xa2, 0x00, 0x00, 0x01, 0x05,
 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01,
 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05,
 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7d, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21,
 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23,
 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17,
 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a,
 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a,
 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5,
 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1,
 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04,
 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04,
 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14,
 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16,
 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36,
 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56,
 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76,
 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94,
 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2,
 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xff, 0xc0, 0x00, 0x11,
 0x08, 0x01, 0xe0, 0x02, 0x80, 0x03, 0x01, 0x21, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff,
 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00
 };
/*********************************/
const unsigned char GsmartQTable[22][64]=
{
	// index 0, Q50
	{  16, 11, 12, 14, 12, 10, 16, 14, 13, 14, 18, 17, 16, 19, 24, 40,
       26, 24, 22, 22, 24, 49, 35, 37, 29, 40, 58, 51, 61, 60, 57, 51,
       56, 55, 64, 72, 92, 78, 64, 68, 87, 69, 55, 56, 80,109, 81, 87,
       95, 98,103,104,103, 62, 77,113,121,112,100,120, 92,101,103, 99 },
	{  17, 18, 18, 24, 21, 24, 47, 26, 26, 47, 99, 66, 56, 66, 99, 99,
       99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	   99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
       99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99 },

	// index 1, Q70
	{  10,  7,  7,  8,  7,  6, 10,  8,  8,  8, 11, 10, 10, 11, 14, 24,
	   16, 14, 13, 13, 14, 29, 21, 22, 17, 24, 35, 31, 37, 36, 34, 31,
	   34, 33, 38, 43, 55, 47, 38, 41, 52, 41, 33, 34, 48, 65, 49, 52,
	   57, 59, 62, 62, 62, 37, 46, 68, 73, 67, 60, 72, 55, 61, 62, 59 },
	{  10, 11, 11, 14, 13, 14, 28, 16, 16, 28, 59, 40, 34, 40, 59, 59,
	   59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59,
	   59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59,
	   59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59 },

	// index 2, Q80
	{   6,  4,  5,  6,  5,  4,  6,  6,  5,  6,  7,  7,  6,  8, 10, 16,
	   10, 10,  9,  9, 10, 20, 14, 15, 12, 16, 23, 20, 24, 24, 23, 20,
	   22, 22, 26, 29, 37, 31, 26, 27, 35, 28, 22, 22, 32, 44, 32, 35,
	   38, 39, 41, 42, 41, 25, 31, 45, 48, 45, 40, 48, 37, 40, 41, 40 },
	{   7,  7,  7, 10,  8, 10, 19, 10, 10, 19, 40, 26, 22, 26, 40, 40,
	   40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
	   40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
	   40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40 },

	// index 3, Q85
	{   5,  3,  4,  4,  4,  3,  5,  4,  4,  4,  5,  5,  5,  6,  7, 12,
	    8,  7,  7,  7,  7, 15, 11, 11,  9, 12, 17, 15, 18, 18, 17, 15,
	   17, 17, 19, 22, 28, 23, 19, 20, 26, 21, 17, 17, 24, 33, 24, 26,
	   29, 29, 31, 31, 31, 19, 23, 34, 36, 34, 30, 36, 28, 30, 31, 30 },
	{   5,  5,  5,  7,  6,  7, 14,  8,  8, 14, 30, 20, 17, 20, 30, 30,
	   30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
	   30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
	   30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 },

	// index 4, Q90
	{   3,  2,  2,  3,  2,  2,  3,  3,  3,  3,  4,  3,  3,  4,  5,  8,
	    5,  5,  4,  4,  5, 10,  7,  7,  6,  8, 12, 10, 12, 12, 11, 10,
	   11, 11, 13, 14, 18, 16, 13, 14, 17, 14, 11, 11, 16, 22, 16, 17,
	   19, 20, 21, 21, 21, 12, 15, 23, 24, 22, 20, 24, 18, 20, 21, 20 },
	{   3,  4,  4,  5,  4,  5,  9,  5,  5,  9, 20, 13, 11, 13, 20, 20,
	   20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
	   20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
	   20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20 },

	// index 5, Q60
	{  13,  9, 10, 11, 10,  8, 13, 11, 10, 11, 14, 14, 13, 15, 19, 32,
	   21, 19, 18, 18, 19, 39, 28, 30, 23, 32, 46, 41, 49, 48, 46, 41,
	   45, 44, 51, 58, 74, 62, 51, 54, 70, 55, 44, 45, 64, 87, 65, 70,
	   76, 78, 82, 83, 82, 50, 62, 90, 97, 90, 80, 96, 74, 81, 82, 79 },
	{  14, 14, 14, 19, 17, 19, 38, 21, 21, 38, 79, 53, 45, 53, 79, 79,
	   79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79,
	   79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79,
	   79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79 },

	// index 6, Q25
	{  32, 22, 24, 28, 24, 20, 32, 28, 26, 28, 36, 34, 32, 38, 48, 80,
	   52, 48, 44, 44, 48, 98, 70, 74, 58, 80,116,102,122,120,114,102,
	  112,110,128,144,184,156,128,136,174,138,110,112,160,218,162,174,
	  190,196,206,208,206,124,154,226,242,224,200,240,184,202,206,198 },
	{  34, 36, 36, 48, 42, 48, 94, 52, 52, 94,198,132,112,132,198,198,
	  198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
	  198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
	  198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198 },

	// index 7, Q95
	{   2,  1,  1,  1,  1,  1,  2,  1,  1,  1,  2,  2,  2,  2,  2,  4,
	    3,  2,  2,  2,  2,  5,  4,  4,  3,  4,  6,  5,  6,  6,  6,  5,
	    6,  6,  6,  7,  9,  8,  6,  7,  9,  7,  6,  6,  8, 11,  8,  9,
	   10, 10, 10, 10, 10,  6,  8, 11, 12, 11, 10, 12,  9, 10, 10, 10 },
	{   2,  2,  2,  2,  2,  2,  5,  3,  3,  5, 10,  7,  6,  7, 10, 10,
	   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
	   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
	   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 },

	// index 8, Q93
	{   2,  2,  2,  2,  2,  1,  2,  2,  2,  2,  3,  2,  2,  3,  3,  6,
	    4,  3,  3,  3,  3,  7,  5,  5,  4,  6,  8,  7,  9,  8,  8,  7,
	    8,  8,  9, 10, 13, 11,  9, 10, 12, 10,  8,  8, 11, 15, 11, 12,
	   13, 14, 14, 15, 14,  9, 11, 16, 17, 16, 14, 17, 13, 14, 14, 14 },
	{   2,  3,  3,  3,  3,  3,  7,  4,  4,  7, 14,  9,  8,  9, 14, 14,
	   14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
	   14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
	   14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14 },

	// index 9, Q40
	{  20, 14, 15, 18, 15, 13, 20, 18, 16, 18, 23, 21, 20, 24, 30, 50,
	   33, 30, 28, 28, 30, 61, 44, 46, 36, 50, 73, 64, 76, 75, 71, 64,
	   70, 69, 80, 90,115, 98, 80, 85,109, 86, 69, 70,100,136,101,109,
	  119,123,129,130,129, 78, 96,141,151,140,125,150,115,126,129,124 },
	{  21, 23, 23, 30, 26, 30, 59, 33, 33, 59,124, 83, 70, 83,124,124,
	  124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,
	  124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,
	  124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124 },

	// index 10, pccam300
	{   5,  3,  3,  5,  7, 12, 15, 18,  4,  4,  4,  6,  8, 17, 18, 17,
	    4,  4,  5,  7, 12, 17, 21, 17,  4,  5,  7,  9, 15, 26, 24, 19,
	    5,  7, 11, 17, 20, 33, 31, 23,  7, 11, 17, 19, 24, 31, 34, 28,
	   15, 19, 23, 26, 31, 36, 36, 30, 22, 28, 29, 29, 34, 30, 31, 30 },
	{   5,  5,  7, 14, 30, 30, 30, 30,  5,  6,  8, 20, 30, 30, 30, 30,
	    7,  8, 17, 30, 30, 30, 30, 30, 14, 20, 30, 30, 30, 30, 30, 30,
	   30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
	   30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 }
};

int spca50x_outpicture ( struct spca50x_frame *myframe );

static int make_jpeg ( struct spca50x_frame *myframe );
static int make_jpeg_conexant (struct spca50x_frame *myframe);

void
init_jpeg_decoder (struct usb_spca50x *spca50x, unsigned int qIndex)
{
int i,j;
/* set up a quantization table */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 64; j++) {
			spca50x->maindecode.quant[i][j] = GsmartQTable[qIndex * 2 + i][j];
		}
	}
}
int spca50x_outpicture ( struct spca50x_frame *myframe )
{	/* general idea keep a frame in the temporary buffer from the tasklet*/
	/* decode with native format at input and asked format at output */
	/* myframe->cameratype is the native input format */
	/* myframe->format is the asked format */
	
	int width = 0;
	int height = 0;
	int done = 0;

	switch(myframe->cameratype){
		case JPGC: //conexant
			height = (myframe->data[11] << 8) | myframe->data[12];
			width = (myframe->data[13] << 8) | myframe->data[14];
			if (myframe->hdrheight != height || myframe->hdrwidth != width){	
				done = ERR_CORRUPTFRAME;
			} else {
			
				memcpy(myframe->tmpbuffer,myframe->data,myframe->scanlength);
			 	done = make_jpeg_conexant ( myframe );
			
			}
		break;
		case JPGH://Vimicro
			width = (myframe->data[10] << 8) | myframe->data[11];
			height = (myframe->data[12] << 8) | myframe->data[13];
			/* some camera did not respond with the good height ie:Labtec Pro 240 -> 232 */
			if (myframe->hdrwidth != width){	
				done = ERR_CORRUPTFRAME;
			} else {
				memcpy(myframe->tmpbuffer,myframe->data+16,myframe->scanlength - 16);
				done = make_jpeg ( myframe );
			
			}
		break;
		case JPGM://Mars-semi
		case JPGS://Sonix
		case JPEG://Sunplus
			memcpy(myframe->tmpbuffer,myframe->data,myframe->scanlength );
			done = make_jpeg ( myframe );

		break;

	default : done = -1;
	break;
	}
return done;	
}

/* this function restore the missing header for the jpeg camera */
/* adapted from Till Adam create_jpeg_from_data() */
static int make_jpeg (struct spca50x_frame *myframe)
{
  __u8 *start;
 int i;
 __u8 value;
 int width = myframe->hdrwidth;
 int height = myframe->hdrheight;
 long inputsize = myframe->scanlength;
 __u8 *buf = myframe->tmpbuffer;
 __u8 *dst = myframe->data;;

	 start = dst;	
	/* set up the default header */
	memcpy(dst,JPEGHeader,JPEGHEADER_LENGTH);
	/* setup quantization table */
	*(dst+6) = 0;
	memcpy(dst+7,myframe->decoder->quant[0],64);
	*(dst+7+64) = 1;
	memcpy(dst+8+64,myframe->decoder->quant[1],64);
	
	*(dst + 564) = width & 0xFF;	//Image width low byte
	*(dst + 563) = width >> 8 & 0xFF;	//Image width high byte
	*(dst + 562) = height & 0xFF;	//Image height low byte
	*(dst + 561) = height >> 8 & 0xFF;	//Image height high byte
	/* set the format */
	if(myframe->cameratype == JPEG){
	 *(dst + 567) = 0x22;
	 dst += JPEGHEADER_LENGTH;
	 for (i=0 ; i < inputsize; i++){
		value = *(buf + i) & 0xFF;
		*dst = value;
		dst++;
		if (value == 0xFF){
			*dst = 0;
			dst++;
		}	
	 }	  
	} else {
	 *(dst + 567) = 0x21;
	 dst += JPEGHEADER_LENGTH;
	 memcpy(dst,buf,inputsize);
	 dst += inputsize;

	}
	/* Add end of image marker */	
	*(dst++) = 0xFF; 	
	*(dst++) = 0xD9;
	myframe->scanlength = (long)(dst - start);
return 0;
}

static int make_jpeg_conexant (struct spca50x_frame *myframe)
{

 __u8 *buf = myframe->data;
 __u8 *dst = myframe->tmpbuffer;

	memcpy(dst,JPEGHeader,JPEGHEADER_LENGTH-33);
	*(dst+6) = 0;
	memcpy(dst+7,myframe->decoder->quant[0],64);
	*(dst+7+64) = 1;
	memcpy(dst+8+64,myframe->decoder->quant[1],64);
	dst += (JPEGHEADER_LENGTH-33);
	memcpy(dst,buf,myframe->scanlength);
	myframe->scanlength +=(JPEGHEADER_LENGTH-33);
return 0;
}
