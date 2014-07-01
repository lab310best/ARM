#ifndef _HPI_H_
#define _HPI_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#define HPI_IOCTL_BASE 	'W'

#define BUF_FULL 1
#define BUF_EMPTY 0

#define HPI_CLRINT		_IO(HPI_IOCTL_BASE, 0)
#define HPI_TEST 		_IO(HPI_IOCTL_BASE, 1)
#define HPI_WR_RDY 		_IOR(HPI_IOCTL_BASE, 2, int)

#endif
