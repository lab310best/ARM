/*
 * Block driver for media (i.e., flash cards)
 *
 * Copyright 2002 Hewlett-Packard Company
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * HEWLETT-PACKARD COMPANY MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Many thanks to Alessandro Rubini and Jonathan Corbet!
 *
 * Author:  Andrew Christian
 *          28 May 2002
 */
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/devfs_fs_kernel.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/protocol.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include "mmc_queue.h"

#ifdef CONFIG_MMC_DEBUG
#define DBG(x...)	printk(KERN_DEBUG x)
#else
#define DBG(x...)	do { } while (0)
#endif

/*
 * max 8 partitions per card
 */
#define MMC_SHIFT	3

static int mmc_major;
static int maxsectors = 8;

/*
 * There is one mmc_blk_data per slot.
 */
struct mmc_blk_data {
	spinlock_t	lock;
	struct gendisk	*disk;
	struct mmc_queue queue;

	unsigned int	usage;
	unsigned int	block_bits;
	unsigned int	suspended;
};

static DECLARE_MUTEX(open_lock);

static struct mmc_blk_data *mmc_blk_get(struct gendisk *disk)
{
	struct mmc_blk_data *md;

	down(&open_lock);
	md = disk->private_data;
	if (md && md->usage == 0)
		md = NULL;
	if (md)
		md->usage++;
	up(&open_lock);

	return md;
}

static void mmc_blk_put(struct mmc_blk_data *md)
{
	down(&open_lock);
	md->usage--;
	if (md->usage == 0) {
		put_disk(md->disk);
		mmc_cleanup_queue(&md->queue);
		kfree(md);
	}
	up(&open_lock);
}

static int mmc_blk_open(struct inode *inode, struct file *filp)
{
	struct mmc_blk_data *md;
	int ret = -ENXIO;

	md = mmc_blk_get(inode->i_bdev->bd_disk);
	if (md) {
		if (md->usage == 2)
			check_disk_change(inode->i_bdev);
		ret = 0;
	}

	return ret;
}

static int mmc_blk_release(struct inode *inode, struct file *filp)
{
	struct mmc_blk_data *md = inode->i_bdev->bd_disk->private_data;

	mmc_blk_put(md);
	return 0;
}

static int
mmc_blk_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct block_device *bdev = inode->i_bdev;

	if (cmd == HDIO_GETGEO) {
		struct hd_geometry geo;

		memset(&geo, 0, sizeof(struct hd_geometry));

		geo.cylinders	= get_capacity(bdev->bd_disk) / (4 * 16);
		geo.heads	= 4;
		geo.sectors	= 16;
		geo.start	= get_start_sect(bdev);

		return copy_to_user((void *)arg, &geo, sizeof(geo))
			? -EFAULT : 0;
	}

	return -ENOTTY;
}

static struct block_device_operations mmc_bdops = {
	.open			= mmc_blk_open,
	.release		= mmc_blk_release,
	.ioctl			= mmc_blk_ioctl,
	.owner			= THIS_MODULE,
};

struct mmc_blk_request {
	struct mmc_request	req;
	struct mmc_command	cmd;
	struct mmc_command	stop;
	struct mmc_data		data;
};

static int mmc_blk_prep_rq(struct mmc_queue *mq, struct request *req)
{
	struct mmc_blk_data *md = mq->data;

	/*
	 * If we have no device, we haven't finished initialising.
	 */
	if (!md || !mq->card) {
		printk(KERN_ERR "%s: killing request - no device/host\n",
		       req->rq_disk->disk_name);
		goto kill;
	}

	if (md->suspended) {
		blk_plug_device(md->queue.queue);
		goto defer;
	}

	/*
	 * Check for excessive requests.
	 */
	if (req->sector + req->nr_sectors > get_capacity(req->rq_disk)) {
		printk(KERN_ERR "%s: bad request size\n",
		       req->rq_disk->disk_name);
		goto kill;
	}

	return BLKPREP_OK;

 defer:
	return BLKPREP_DEFER;
 kill:
	return BLKPREP_KILL;
}

#ifdef	CONFIG_LEDS
#include <asm/leds.h>
#define	MMC_BLK_RQ_LED_START()	leds_event(led_blue_on)
#define	MMC_BLK_RQ_LED_STOP()	leds_event(led_blue_off)
#else
#define	MMC_BLK_RQ_LED_START()
#define	MMC_BLK_RQ_LED_STOP()
#endif

static int mmc_blk_issue_rq(struct mmc_queue *mq, struct request *req)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	int err, sz = 0;
	int blk_cnt = req->current_nr_sectors;	//add by hzh
	unsigned long start_sect = req->sector;
	unsigned int clock, data_retries = 3;

	clock = card->host->ios.clock;		//add by hzh, save clock

	err = mmc_card_claim_host(card);
	if (err)
		goto cmd_err;

	MMC_BLK_RQ_LED_START();		//add by hzh

	do {
		struct mmc_blk_request rq;
		struct mmc_command cmd;

		memset(&rq, 0, sizeof(struct mmc_blk_request));
		rq.req.cmd = &rq.cmd;
		rq.req.data = &rq.data;
		//modified by hzh, req->sector << 9 -> start_sect << md->block_bits
		rq.cmd.arg = start_sect << md->block_bits;
		rq.cmd.flags = MMC_RSP_SHORT | MMC_RSP_CRC;
		rq.data.rq = req;
		rq.data.timeout_ns = card->csd.tacc_ns * 10;
		rq.data.timeout_clks = card->csd.tacc_clks * 10;
		rq.data.blksz_bits = md->block_bits;
		rq.data.blocks = req->current_nr_sectors >> (md->block_bits - 9);
		rq.data.offset = (start_sect - req->sector) << md->block_bits;	//add by hzh
		rq.data.flags  = (card->type&MMC_TYPE_SD)?MMC_DATA_4BITS:0;	//add by hzh
		rq.stop.opcode = MMC_STOP_TRANSMISSION;
		rq.stop.arg = 0;
		rq.stop.flags = MMC_RSP_SHORT | MMC_RSP_CRC | MMC_RSP_BUSY;
try_again:
		rq.cmd.retries = 3;		//add by hzh

		if (rq_data_dir(req) == READ) {
			//modified by hzh, some mmc can't do multiple read
			if(card->type & MMC_TYPE_SD)	
				rq.cmd.opcode = rq.data.blocks > 1 ? MMC_READ_MULTIPLE_BLOCK : MMC_READ_SINGLE_BLOCK;
			else {
				rq.cmd.opcode = MMC_READ_SINGLE_BLOCK;
				//rq.cmd.opcode = MMC_READ_DAT_UNTIL_STOP;
				//rq.data.flags |= MMC_DATA_STREAM;
				rq.data.blocks = 1;
			}
			rq.data.flags |= MMC_DATA_READ;
			//mmc_set_clock(card->host,0);	//modified by hzh, don't use this
		} else {
			rq.cmd.opcode = MMC_WRITE_BLOCK;
			rq.cmd.flags |= MMC_RSP_BUSY;
			rq.data.flags |= MMC_DATA_WRITE;
			rq.data.blocks = 1;
			//mmc_set_clock(card->host,0);	//modified by hzh, don't slow write clock
		}
		rq.req.stop = rq.data.blocks > 1 ? &rq.stop : NULL;
//	printk("%x %x %x\n", rq.cmd.opcode, rq.cmd.arg, rq.data.blocks);
		mmc_wait_for_req(card->host, &rq.req);

		if (rq.cmd.error) {
			//err = rq.cmd.error;
			//printk(KERN_ERR "%s: error %d sending read/write command\n",
			//       req->rq_disk->disk_name, err);
			//goto cmd_err;
			goto slow_down_clock;
		}

		if (rq_data_dir(req) == READ) {
			sz = rq.data.bytes_xfered;
		} else {
			sz = 0;
		}

		if (rq.data.error) {
			//err = rq.data.error;
			//printk(KERN_ERR "%s: error %d transferring data\n",
			//       req->rq_disk->disk_name, err);
			//goto cmd_err;
			if(data_retries--) {
				rq.data.error = 0;
				goto try_again;
			}
slow_down_clock:	//modified by hzh
			data_retries = 3;
			if(card->host->ios.clock>5000000) {
				rq.cmd.error = rq.data.error = 0;
				//wait a while to change clock if data transfer started
				set_current_state(TASK_INTERRUPTIBLE);
				schedule_timeout(HZ/50);
				if(card->host->ios.clock>10000000)
					card->host->ios.clock = 10000000;
				else
					card->host->ios.clock = 5000000;
				card->host->ops->set_ios(card->host, &card->host->ios);
				printk(KERN_DEBUG "%s slow down clock %u to try again!\n", 
					req->rq_disk->disk_name, card->host->ios.clock);
				goto try_again;
			}
			printk(KERN_ERR "%s: %s %lu error ", 
					req->rq_disk->disk_name, 
					(rq_data_dir(req)==READ)?"read":"write", 
					start_sect<<9);
			if(rq.cmd.error) {
				err = rq.cmd.error;
				printk(KERN_ERR "%d sending read/write command\n", err);
			} else {
				err = rq.data.error;
				printk(KERN_ERR "%d transferring data\n", err);
			}
			card->host->ios.clock = clock;
			card->host->ops->set_ios(card->host, &card->host->ios);
			goto cmd_err;
		}

		if (rq.stop.error) {
			err = rq.stop.error;
			printk(KERN_ERR "%s: error %d sending stop command\n",
			       req->rq_disk->disk_name, err);
			goto cmd_err;
		}

		do {
			cmd.opcode = MMC_SEND_STATUS;
			cmd.arg = card->rca << 16;
			cmd.flags = MMC_RSP_SHORT | MMC_RSP_CRC;
			err = mmc_wait_for_cmd(card->host, &cmd, 5);
			if (err) {
				printk(KERN_ERR "%s: error %d requesting status\n",
				       req->rq_disk->disk_name, err);
				goto cmd_err;
			}
		} while (!(cmd.resp[0] & R1_READY_FOR_DATA));

#if 0
		if (cmd.resp[0] & ~0x00000900)
			printk(KERN_ERR "%s: status = %08x\n",
			       req->rq_disk->disk_name, cmd.resp[0]);
		err = mmc_decode_status(cmd.resp);
		if (err)
			goto cmd_err;
#endif

		sz = rq.data.bytes_xfered;
		blk_cnt -= rq.data.blocks;
		start_sect += rq.data.blocks;
		
		if(clock!=card->host->ios.clock) {
			card->host->ios.clock = clock;
			card->host->ops->set_ios(card->host, &card->host->ios);
		}
	} while (blk_cnt);//(end_that_request_chunk(req, 1, sz));

	MMC_BLK_RQ_LED_STOP();

	mmc_card_release_host(card);

	return 1;

 cmd_err:
	mmc_card_release_host(card);

	end_that_request_chunk(req, 1, sz);
	req->errors = err;

	return 0;
}

#define MMC_NUM_MINORS	(256 >> MMC_SHIFT)

static unsigned long dev_use[MMC_NUM_MINORS/(8*sizeof(unsigned long))];

static struct mmc_blk_data *mmc_blk_alloc(struct mmc_card *card)
{
	struct mmc_blk_data *md;
	int devidx, ret;

	devidx = find_first_zero_bit(dev_use, MMC_NUM_MINORS);
	if (devidx >= MMC_NUM_MINORS)
		return ERR_PTR(-ENOSPC);
	__set_bit(devidx, dev_use);

	md = kmalloc(sizeof(struct mmc_blk_data), GFP_KERNEL);
	if (md) {
		memset(md, 0, sizeof(struct mmc_blk_data));

		md->disk = alloc_disk(1 << MMC_SHIFT);
		if (md->disk == NULL) {
			kfree(md);
			md = ERR_PTR(-ENOMEM);
			goto out;
		}

		spin_lock_init(&md->lock);
		md->usage = 1;

		ret = mmc_init_queue(&md->queue, card, &md->lock);
		if (ret) {
			put_disk(md->disk);
			kfree(md);
			md = ERR_PTR(ret);
			goto out;
		}
		md->queue.prep_fn = mmc_blk_prep_rq;
		md->queue.issue_fn = mmc_blk_issue_rq;
		md->queue.data = md;

		md->disk->major	= mmc_major;
		md->disk->first_minor = devidx << MMC_SHIFT;
		md->disk->fops = &mmc_bdops;
		md->disk->private_data = md;
		md->disk->queue = md->queue.queue;
		md->disk->driverfs_dev = &card->dev;

		sprintf(md->disk->disk_name, "mmcblk%d", devidx);
		sprintf(md->disk->devfs_name, "mmc/blk%d", devidx);

		md->block_bits = md->queue.card->csd.read_blkbits;

		blk_queue_max_sectors(md->queue.queue, maxsectors);
		blk_queue_hardsect_size(md->queue.queue, 1 << md->block_bits);
		set_capacity(md->disk, md->queue.card->csd.capacity);
	}
 out:
	return md;
}

static int
mmc_blk_set_blksize(struct mmc_blk_data *md, struct mmc_card *card)
{
	struct mmc_command cmd;
	int err;

	mmc_card_claim_host(card);
	cmd.opcode = MMC_SET_BLOCKLEN;
	cmd.arg = 1 << card->csd.read_blkbits;
	cmd.flags = MMC_RSP_SHORT | MMC_RSP_CRC;
	err = mmc_wait_for_cmd(card->host, &cmd, 5);

	if( card->type & MMC_TYPE_SD ) {	//modified by hzh, && -> &
		DBG("Setting to 4 bit bus transfer\n");
		cmd.opcode = MMC_APP_CMD;
		cmd.arg = card->rca << 16;
		cmd.flags =  MMC_RSP_SHORT | MMC_RSP_CRC;
		err = mmc_wait_for_cmd(card->host, &cmd, 5);
		cmd.opcode = SET_BUS_WIDTH;
#ifdef	CONFIG_ARCH_FS_PXA255			//modified by hzh, set 1 bit mode
		cmd.arg = 0;
#else
		cmd.arg = 2;
#endif
		cmd.flags =  MMC_RSP_SHORT | MMC_RSP_CRC;
		err = mmc_wait_for_cmd(card->host, &cmd, 5);
		
		if( err != MMC_ERR_NONE ) {
			printk(KERN_ERR "%s: unable to set bus width to 4bit\n",
				md->disk->disk_name);
				return -EINVAL;
			/* FIXME: We should fallback to 1 bit mode */
		}
	}

	mmc_card_release_host(card);

	if (err) {
		printk(KERN_ERR "%s: unable to set block size to %d: %d\n",
			md->disk->disk_name, cmd.arg, err);
		return -EINVAL;
	}

	return 0;
}

static int mmc_blk_probe(struct mmc_card *card)
{
	struct mmc_blk_data *md;
	int err;

	if (card->csd.cmdclass & ~0x1ff)
		return -ENODEV;

	if (card->csd.read_blkbits < 9) {
		printk(KERN_WARNING "%s: read blocksize too small (%u)\n",
			mmc_card_id(card), 1 << card->csd.read_blkbits);
		return -ENODEV;
	}

	md = mmc_blk_alloc(card);
	if (IS_ERR(md))
		return PTR_ERR(md);

	err = mmc_blk_set_blksize(md, card);
	if (err)
		goto out;
	//add by hzh, show card type and block size
	printk(KERN_INFO "[%s card] %s: %s %s %dKiB block size %dB\n",
		(card->type & MMC_TYPE_SD) ? "SD":"MMC",
		md->disk->disk_name, mmc_card_id(card), mmc_card_name(card),
		(card->csd.capacity << card->csd.read_blkbits) / 1024, 1<<card->csd.read_blkbits);

	mmc_set_drvdata(card, md);
	add_disk(md->disk);
	return 0;

 out:
	mmc_blk_put(md);

	return err;
}

static void mmc_blk_remove(struct mmc_card *card)
{
	struct mmc_blk_data *md = mmc_get_drvdata(card);

	if (md) {
		int devidx;

		del_gendisk(md->disk);

		/*
		 * I think this is needed.
		 */
		md->disk->queue = NULL;

		devidx = md->disk->first_minor >> MMC_SHIFT;
		__clear_bit(devidx, dev_use);

		mmc_blk_put(md);
	}
	mmc_set_drvdata(card, NULL);
}

#ifdef CONFIG_PM
static int mmc_blk_suspend(struct mmc_card *card, u32 state)
{
	struct mmc_blk_data *md = mmc_get_drvdata(card);

	if (md) {
		blk_stop_queue(md->queue.queue);
	}
	return 0;
}

static int mmc_blk_resume(struct mmc_card *card)
{
	struct mmc_blk_data *md = mmc_get_drvdata(card);

	if (md) {
		mmc_blk_set_blksize(md, md->queue.card);
		blk_start_queue(md->queue.queue);
	}
	return 0;
}
#else
#define	mmc_blk_suspend	NULL
#define mmc_blk_resume	NULL
#endif

static struct mmc_driver mmc_driver = {
	.drv		= {
		.name	= "mmcblk",
	},
	.probe		= mmc_blk_probe,
	.remove		= mmc_blk_remove,
	.suspend	= mmc_blk_suspend,
	.resume		= mmc_blk_resume,
};

static int __init mmc_blk_init(void)
{
	int res = -ENOMEM;

	res = register_blkdev(mmc_major, "mmc");
	if (res < 0) {
		printk(KERN_WARNING "Unable to get major %d for MMC media: %d\n",
		       mmc_major, res);
		goto out;
	}
	if (mmc_major == 0)
		mmc_major = res;

	devfs_mk_dir("mmc");
	return mmc_register_driver(&mmc_driver);

 out:
	return res;
}

static void __exit mmc_blk_exit(void)
{
	mmc_unregister_driver(&mmc_driver);
	devfs_remove("mmc");
	unregister_blkdev(mmc_major, "mmc");
}

module_init(mmc_blk_init);
module_exit(mmc_blk_exit);
module_param(maxsectors, int, 0444);

MODULE_PARM_DESC(maxsectors, "Maximum number of sectors for a single request");

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Multimedia Card (MMC) block device driver");
