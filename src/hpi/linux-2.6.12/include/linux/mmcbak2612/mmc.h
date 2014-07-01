/*
 *  linux/include/linux/mmc/mmc.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef MMC_H
#define MMC_H

#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/device.h>

struct request;
struct mmc_data;
struct mmc_request;

struct mmc_command {
	u32			opcode;
	u32			arg;
	u32			resp[4];
	unsigned int		flags;		/* expected response type */
#define MMC_RSP_NONE	(0 << 0)
#define MMC_RSP_SHORT	(1 << 0)
#define MMC_RSP_LONG	(2 << 0)
#define MMC_RSP_MASK	(3 << 0)
#define MMC_RSP_CRC	(1 << 3)		/* expect valid crc */
#define MMC_RSP_BUSY	(1 << 4)		/* card may send busy */

	unsigned int		retries;	/* max number of retries */
	unsigned int		error;		/* command error */

#define MMC_ERR_NONE	0
#define MMC_ERR_TIMEOUT	1
#define MMC_ERR_BADCRC	2
#define MMC_ERR_FIFO	3
#define MMC_ERR_FAILED	4
#define MMC_ERR_INVALID	5

	struct mmc_data		*data;		/* data segment associated with cmd */
	struct mmc_request	*req;		/* assoicated request */
};

struct mmc_data {
	unsigned int		timeout_ns;	/* data timeout (in ns, max 80ms) */
	unsigned int		timeout_clks;	/* data timeout (in clocks) */
	unsigned int		blksz_bits;	/* data block size */
	unsigned int		blocks;		/* number of blocks */
	struct request		*rq;		/* request structure */
	unsigned int		error;		/* data error */
	unsigned int		flags;

#define MMC_DATA_WRITE	(1 << 8)
#define MMC_DATA_READ	(1 << 9)
#define MMC_DATA_STREAM	(1 << 10)
#define MMC_DATA_4BITS	(1 << 12)	//add by hzh

	unsigned int		bytes_xfered;

	struct mmc_command	*stop;		/* stop command */
	struct mmc_request	*req;		/* assoicated request */
	unsigned long		offset;		/* offset in one request, add by hzh */
};

struct mmc_request {
	struct mmc_command	*cmd;
	struct mmc_data		*data;
	struct mmc_command	*stop;

	void			*done_data;	/* completion data */
	void			(*done)(struct mmc_request *);/* completion function */
};

struct mmc_host;
struct mmc_card;

extern int mmc_wait_for_req(struct mmc_host *, struct mmc_request *);
extern int mmc_wait_for_cmd(struct mmc_host *, struct mmc_command *, int);

extern int __mmc_claim_host(struct mmc_host *host, struct mmc_card *card);

static inline void mmc_claim_host(struct mmc_host *host)
{
	__mmc_claim_host(host, (struct mmc_card *)-1);
}

extern void mmc_release_host(struct mmc_host *host);

#endif
