#include <linux/version.h>		//KERNEL_VERSION
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>			//kmalloc, etc..., must be include in 2.4.x
#include <linux/mm.h>			//GFP_KERNEL, etc..., must be include in 2.4.4
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/delay.h>		//udelay
#include <linux/errno.h>

#include <asm/sizes.h>
#include <asm/io.h>				//__raw_readb, etc...
#include <asm/uaccess.h>		//copy_from_user, etc...
#include <asm/semaphore.h>		//sem_init, etc...

#include <linux/blkdev.h>		//struct request

#include "nfdrv.h"

//#define	NO_VIR_TO_PHY_MAP

#ifndef	BUG_ON
#define	BUG_ON(cond)			\
	do {				\
		if(cond)		\
			printk(KERN_ERR "%s:%d unexpected error!", __FUNCTION__, __LINE__);	\
		/*(void *)0 = 0;*/	\
	} while(0)
#endif

/*********************************************************************/

/*
 * Pre-calculated 256-way 1 byte column parity
 */
static const u_char nand_ecc_precalc_table[] = {
	0x00, 0x55, 0x56, 0x03, 0x59, 0x0c, 0x0f, 0x5a, 0x5a, 0x0f, 0x0c, 0x59, 0x03, 0x56, 0x55, 0x00,
	0x65, 0x30, 0x33, 0x66, 0x3c, 0x69, 0x6a, 0x3f, 0x3f, 0x6a, 0x69, 0x3c, 0x66, 0x33, 0x30, 0x65,
	0x66, 0x33, 0x30, 0x65, 0x3f, 0x6a, 0x69, 0x3c, 0x3c, 0x69, 0x6a, 0x3f, 0x65, 0x30, 0x33, 0x66,
	0x03, 0x56, 0x55, 0x00, 0x5a, 0x0f, 0x0c, 0x59, 0x59, 0x0c, 0x0f, 0x5a, 0x00, 0x55, 0x56, 0x03,
	0x69, 0x3c, 0x3f, 0x6a, 0x30, 0x65, 0x66, 0x33, 0x33, 0x66, 0x65, 0x30, 0x6a, 0x3f, 0x3c, 0x69,
	0x0c, 0x59, 0x5a, 0x0f, 0x55, 0x00, 0x03, 0x56, 0x56, 0x03, 0x00, 0x55, 0x0f, 0x5a, 0x59, 0x0c,
	0x0f, 0x5a, 0x59, 0x0c, 0x56, 0x03, 0x00, 0x55, 0x55, 0x00, 0x03, 0x56, 0x0c, 0x59, 0x5a, 0x0f,
	0x6a, 0x3f, 0x3c, 0x69, 0x33, 0x66, 0x65, 0x30, 0x30, 0x65, 0x66, 0x33, 0x69, 0x3c, 0x3f, 0x6a,
	0x6a, 0x3f, 0x3c, 0x69, 0x33, 0x66, 0x65, 0x30, 0x30, 0x65, 0x66, 0x33, 0x69, 0x3c, 0x3f, 0x6a,
	0x0f, 0x5a, 0x59, 0x0c, 0x56, 0x03, 0x00, 0x55, 0x55, 0x00, 0x03, 0x56, 0x0c, 0x59, 0x5a, 0x0f,
	0x0c, 0x59, 0x5a, 0x0f, 0x55, 0x00, 0x03, 0x56, 0x56, 0x03, 0x00, 0x55, 0x0f, 0x5a, 0x59, 0x0c,
	0x69, 0x3c, 0x3f, 0x6a, 0x30, 0x65, 0x66, 0x33, 0x33, 0x66, 0x65, 0x30, 0x6a, 0x3f, 0x3c, 0x69,
	0x03, 0x56, 0x55, 0x00, 0x5a, 0x0f, 0x0c, 0x59, 0x59, 0x0c, 0x0f, 0x5a, 0x00, 0x55, 0x56, 0x03,
	0x66, 0x33, 0x30, 0x65, 0x3f, 0x6a, 0x69, 0x3c, 0x3c, 0x69, 0x6a, 0x3f, 0x65, 0x30, 0x33, 0x66,
	0x65, 0x30, 0x33, 0x66, 0x3c, 0x69, 0x6a, 0x3f, 0x3f, 0x6a, 0x69, 0x3c, 0x66, 0x33, 0x30, 0x65,
	0x00, 0x55, 0x56, 0x03, 0x59, 0x0c, 0x0f, 0x5a, 0x5a, 0x0f, 0x0c, 0x59, 0x03, 0x56, 0x55, 0x00
};


/*
 * Creates non-inverted ECC code from line parity
 */
static void nand_trans_result(u_char reg2, u_char reg3,
	u_char *ecc_code)
{
	u_char a, b, i, tmp1, tmp2;
	
	/* Initialize variables */
	a = b = 0x80;
	tmp1 = tmp2 = 0;
	
	/* Calculate first ECC byte */
	for (i = 0; i < 4; i++) {
		if (reg3 & a)		/* LP15,13,11,9 --> ecc_code[0] */
			tmp1 |= b;
		b >>= 1;
		if (reg2 & a)		/* LP14,12,10,8 --> ecc_code[0] */
			tmp1 |= b;
		b >>= 1;
		a >>= 1;
	}
	
	/* Calculate second ECC byte */
	b = 0x80;
	for (i = 0; i < 4; i++) {
		if (reg3 & a)		/* LP7,5,3,1 --> ecc_code[1] */
			tmp2 |= b;
		b >>= 1;
		if (reg2 & a)		/* LP6,4,2,0 --> ecc_code[1] */
			tmp2 |= b;
		b >>= 1;
		a >>= 1;
	}
	
	/* Store two of the ECC bytes */
	ecc_code[0] = tmp1;
	ecc_code[1] = tmp2;
}

/*
 * Calculate 3 byte ECC code for 256 byte block
 */
static void nand_calculate_ecc(const u_char *dat, u_char *ecc_code)
{
	u_char idx, reg1, reg2, reg3;
	int j;
	
	/* Initialize variables */
	reg1 = reg2 = reg3 = 0;
	ecc_code[0] = ecc_code[1] = ecc_code[2] = 0;
	
	/* Build up column parity */ 
	for(j = 0; j < 256; j++) {
		
		/* Get CP0 - CP5 from table */
		idx = nand_ecc_precalc_table[dat[j]];
		reg1 ^= (idx & 0x3f);
		
		/* All bit XOR = 1 ? */
		if (idx & 0x40) {
			reg3 ^= (u_char) j;
			reg2 ^= ~((u_char) j);
		}
	}
	
	/* Create non-inverted ECC code from line parity */
	nand_trans_result(reg2, reg3, ecc_code);
	
	/* Calculate final ECC code */
	ecc_code[0] = ~ecc_code[0];
	ecc_code[1] = ~ecc_code[1];
	ecc_code[2] = ((~reg1) << 2) | 0x03;
}

/*
 * Detect and correct a 1 bit error for 256 byte block
 */
static int nand_correct_data(u_char *dat, u_char *read_ecc, u_char *calc_ecc)
{
	u_char a, b, c, d1, d2, d3, add, bit, i;
	
	/* Do error detection */ 
	d1 = calc_ecc[0] ^ read_ecc[0];
	d2 = calc_ecc[1] ^ read_ecc[1];
	d3 = calc_ecc[2] ^ read_ecc[2];
	
	if ((d1 | d2 | d3) == 0) {
		/* No errors */
		return 0;
	}
	else {
		a = (d1 ^ (d1 >> 1)) & 0x55;
		b = (d2 ^ (d2 >> 1)) & 0x55;
		c = (d3 ^ (d3 >> 1)) & 0x54;
		
		/* Found and will correct single bit error in the data */
		if ((a == 0x55) && (b == 0x55) && (c == 0x54)) {
			c = 0x80;
			add = 0;
			a = 0x80;
			for (i=0; i<4; i++) {
				if (d1 & c)
					add |= a;
				c >>= 2;
				a >>= 1;
			}
			c = 0x80;
			for (i=0; i<4; i++) {
				if (d2 & c)
					add |= a;
				c >>= 2;
				a >>= 1;
			}
			bit = 0;
			b = 0x04;
			c = 0x80;
			for (i=0; i<3; i++) {
				if (d3 & c)
					bit |= b;
				c >>= 2;
				b >>= 1;
			}
			b = 0x01;
			a = dat[add];
			a ^= (b << bit);
			dat[add] = a;
			return 1;
		}
		else {
			i = 0;
			while (d1) {
				if (d1 & 0x01)
					++i;
				d1 >>= 1;
			}
			while (d2) {
				if (d2 & 0x01)
					++i;
				d2 >>= 1;
			}
			while (d3) {
				if (d3 & 0x01)
					++i;
				d3 >>= 1;
			}
			if (i == 1) {
				/* ECC Code Error Correction */
				read_ecc[0] = calc_ecc[0];
				read_ecc[1] = calc_ecc[1];
				read_ecc[2] = calc_ecc[2];
				return 2;
			}
			else {
				/* Uncorrectable Error */
				return -1;
			}
		}
	}
	
	/* Should never happen */
	return -1;
}

/*********************************************************************/

static struct {
	u32 id;
	u32 size;
	char *prod_name;
} nf_chip_info[] = {
	{0xec73, SIZE_16M},
	{0xec75, SIZE_32M},
	{0xec76, SIZE_64M},
	{0xec79, SIZE_128M},
	{0, 0},
};

#define	page_addr(addr)		((addr)>>NF_PG_SFT)
#define	block_addr(addr)	((addr)>>NF_BLK_SFT)

struct nf_ctl_info {
	struct list_head list;
	struct nand_flash_itf nf_itf;
	u8 chip_idx;
	u8 parts;
	void (*open_release)(int);
	u32 open_count;
	char *prod_name;
	u32 size;			//		
	struct nf_partition *part_table;
	struct semaphore op_permit;
/*	struct {
		u16 part;
		u16 org_blk_no;
		u16 phy_blk_no;
		u16 vir_blk_no;
		u32 pg_chg_msk;
	} cur_wr_blk;
	u16 *blk_buf;*/
} __attribute__((packed));

static char nf_fmt_tag[] = NF_DRV_FMT_TAG;

static struct nf_ctl_info *nf_ctl_ptr[MAX_NAND_FLASH_DEV];

static struct list_head nf_dev_list;
static struct semaphore nf_dev_list_op;
//static LIST_HEAD(nf_dev_list);
static int nf_dev_count;

#define	nf_sel(nf_itf)			((*(nf_itf)->nf_ce_ctl)(0))
#define	nf_desel(nf_itf)		((*(nf_itf)->nf_ce_ctl)(1))
#define	nf_wait_busy(nf_itf)	while(!(*(nf_itf)->nf_ready)())
#define	nf_rd_data(nf_itf)		__raw_readb((nf_itf)->data)
#define	nf_wr_data(nf_itf, d)	__raw_writeb((u8)(d), (nf_itf)->data)
#define	nf_wr_addr(nf_itf, d)	__raw_writeb((u8)(d), (nf_itf)->addr)
#define	nf_wr_cmd(nf_itf, d)	__raw_writeb((u8)(d), (nf_itf)->cmd)

#define	is_bad_blk(oob)			((oob)[OOB_BAD_MARK]!=0xff)
#define	mark_bad_blk(oob)		((oob)[OOB_BAD_MARK]=0)
#define	mark_good_blk(oob)		((oob)[OOB_BAD_MARK]=0xff)
#define	get_blk_no(oob)			((oob)[OOB_BLK_NOL]|((oob)[OOB_BLK_NOH]<<8))
#define	set_blk_no(oob, no)		do {(oob)[OOB_BLK_NOL]=(u8)(no);(oob)[OOB_BLK_NOH]=(u8)((no)>>8);} while(0)
#define	is_null_blk(blk_no)		((blk_no)==NF_NULL_BLK)
#define	pg_of_blk(p)			((p)&(PAGES_PER_BLOCK-1))
#define	blk_pg(b, p)			(((b)<<BLK_SFT_PG)+(p))

#define	part_phy_blk_start(p)	((p)->offset>>NF_BLK_SFT)
#define	part_max_blk(p)			((p)->size>>NF_BLK_SFT)
#define	part_phy_blk_end(p)		(((p)->offset+(p)->size)>>NF_BLK_SFT)
#define	part_phy_blk(p, no)		(((p)->offset>>NF_BLK_SFT)+(no))
#define	part_bad_blks(p)		((p)->bad_blks)
#define	part_avail_blks(p)		(part_max_blk(p)-part_bad_blks(p))


/**********************************************************************/

static inline u8 nf_rd_stat(struct nand_flash_itf *nf_itf)
{
	nf_wr_cmd(nf_itf, QUERYCMD);
	return nf_rd_data(nf_itf);
}

static void nf_mark_bad_blk(struct nand_flash_itf *nf_itf, u32 phy_pg)
{
	int i;
	
	nf_sel(nf_itf);
	
	nf_wr_cmd(nf_itf, READCMD2);
	nf_wr_cmd(nf_itf, PROGCMD0);
	nf_wr_addr(nf_itf, 0);
	nf_wr_addr(nf_itf, phy_pg);
	nf_wr_addr(nf_itf, phy_pg>>8);
	if(nf_itf->addr_cycle)
		nf_wr_addr(nf_itf, phy_pg>>16);

	for(i=0; i<NF_OOB_SIZE; i++)
		nf_wr_data(nf_itf, 0);
	
	nf_wr_cmd(nf_itf, PROGCMD1);
	nf_wait_busy(nf_itf);
	
	nf_wr_cmd(nf_itf, READCMD0);
	
	nf_desel(nf_itf);
}
/*
static int nf_chk_bad_blk(struct nand_flash_itf *nf_itf, u32 phy_pg)
{
	nf_sel(nf_itf);
	nf_desel(nf_itf);
}
*/
static int nf_erase_blk(struct nand_flash_itf *nf_itf, u32 phy_pg)
{
	u8 stat;
	
	nf_sel(nf_itf);
	
	nf_wr_cmd(nf_itf, ERASECMD0);
	nf_wr_addr(nf_itf, phy_pg);
	nf_wr_addr(nf_itf, phy_pg>>8);
	if(nf_itf->addr_cycle)
		nf_wr_addr(nf_itf, phy_pg>>16);
	nf_wr_cmd(nf_itf, ERASECMD1);
	
	nf_wait_busy(nf_itf);
	
//	stat = nf_rd_stat(nf_itf)&1;
	do {
		stat = nf_rd_stat(nf_itf);		
	} while(!(stat&0x40));			//must!!!
	stat &= 1;
	
	nf_desel(nf_itf);

#ifdef	NO_VIR_TO_PHY_MAP
	return	0;
#endif
	if(stat)
		nf_mark_bad_blk(nf_itf, phy_pg);
	
	return stat;
}

static int nf_wr_pgf(struct nand_flash_itf *nf_itf, u32 phy_pg, char *buf, char *oob_buf)
{
	int i;
	u8 stat;
	u_char cal_ecc[3];
	u_char ecc_code[3];
	u_char idx;
	u_char *buf_p;
///	unsigned long flag;
	
///		printk("w pg 0x%x:%x,%x,%x,%x\n", phy_pg,
///			buf[255], buf[511], oob_buf[8], oob_buf[9]);
	nf_sel(nf_itf);
	
///	local_irq_save(flag);
	nf_wr_cmd(nf_itf, READCMD0);	//set pointer to area a???
	
	nf_wr_cmd(nf_itf, PROGCMD0);
	nf_wr_addr(nf_itf, 0);
	nf_wr_addr(nf_itf, phy_pg);
	nf_wr_addr(nf_itf, phy_pg>>8);
	if(nf_itf->addr_cycle)
		nf_wr_addr(nf_itf, phy_pg>>16);

	buf_p = buf;
	cal_ecc[0] = cal_ecc[1] = cal_ecc[2] = 0;
	ecc_code[0] = ecc_code[1] = ecc_code[2] = 0;
	
	/* Write data and Build up column parity */
	for(i=0; i<(NF_PAGE_SIZE>>1); i++) {
	
		nf_wr_data(nf_itf, buf_p[i]);
		
		/* Get CP0 - CP5 from table */
		idx = nand_ecc_precalc_table[buf_p[i]];
		cal_ecc[0] ^= (idx & 0x3f);
		
		/* All bit XOR = 1 ? */
		if (idx & 0x40) {
			cal_ecc[2] ^= (u_char) i;
			cal_ecc[1] ^= ~((u_char) i);
		}
	}
	
	/* Create non-inverted ECC code from line parity */
	nand_trans_result(cal_ecc[1], cal_ecc[2], ecc_code);
	
	/* Calculate final ECC code */
	oob_buf[OOB_ECCL0] = ~ecc_code[0];
	oob_buf[OOB_ECCL1] = ~ecc_code[1];
	oob_buf[OOB_ECCL2] = ((~cal_ecc[0]) << 2) | 0x03;
	
	buf_p = buf+(NF_PAGE_SIZE>>1);
	cal_ecc[0] = cal_ecc[1] = cal_ecc[2] = 0;
	ecc_code[0] = ecc_code[1] = ecc_code[2] = 0;

	/* Write data and Build up column parity */		
	for(i=0; i<(NF_PAGE_SIZE>>1); i++) {
	
		nf_wr_data(nf_itf, buf_p[i]);
		/* Get CP0 - CP5 from table */
		idx = nand_ecc_precalc_table[buf_p[i]];
		cal_ecc[0] ^= (idx & 0x3f);
		
		/* All bit XOR = 1 ? */
		if (idx & 0x40) {
			cal_ecc[2] ^= (u_char) i;
			cal_ecc[1] ^= ~((u_char) i);
		}
	}
	
	/* Create non-inverted ECC code from line parity */
	nand_trans_result(cal_ecc[1], cal_ecc[2], ecc_code);
	
	/* Calculate final ECC code */
	oob_buf[OOB_ECCH0] = ~ecc_code[0];
	oob_buf[OOB_ECCH1] = ~ecc_code[1];
	oob_buf[OOB_ECCH2] = ((~cal_ecc[0]) << 2) | 0x03;

	for(i=0; i<NF_OOB_SIZE; i++)
		nf_wr_data(nf_itf, oob_buf[i]);
		
	nf_wr_cmd(nf_itf, PROGCMD1);
	
///	local_irq_restore(flag);
	
	nf_wait_busy(nf_itf);
	
//	stat = nf_rd_stat(nf_itf)&1;
	do {
		stat = nf_rd_stat(nf_itf);		
	} while(!(stat&0x40));			//must!!!
	stat &= 1;
	
	nf_desel(nf_itf);
	
	return stat;
}

static int nf_wr_pg(struct nand_flash_itf *nf_itf, u32 phy_pg, char *buf, char *oob_buf)
{
	int i;
	u_char cal_ecc[3];
	u8 stat;
	
	nand_calculate_ecc(buf, cal_ecc);
	oob_buf[OOB_ECCL0] = cal_ecc[0];
	oob_buf[OOB_ECCL1] = cal_ecc[1];
	oob_buf[OOB_ECCL2] = cal_ecc[2];
	nand_calculate_ecc(buf+(NF_PAGE_SIZE>>1), cal_ecc);
	oob_buf[OOB_ECCH0] = cal_ecc[0];
	oob_buf[OOB_ECCH1] = cal_ecc[1];
	oob_buf[OOB_ECCH2] = cal_ecc[2];
	
	nf_sel(nf_itf);
	
	nf_wr_cmd(nf_itf, READCMD0);	//set pointer to area a???
	
	nf_wr_cmd(nf_itf, PROGCMD0);
	nf_wr_addr(nf_itf, 0);
	nf_wr_addr(nf_itf, phy_pg);
	nf_wr_addr(nf_itf, phy_pg>>8);
	if(nf_itf->addr_cycle)
		nf_wr_addr(nf_itf, phy_pg>>16);

	for(i=0; i<NF_PAGE_SIZE; i++)
		nf_wr_data(nf_itf, buf[i]);
	for(i=0; i<NF_OOB_SIZE; i++)
		nf_wr_data(nf_itf, oob_buf[i]);
		
	nf_wr_cmd(nf_itf, PROGCMD1);
	
	nf_wait_busy(nf_itf);
	
	do {
		stat = nf_rd_stat(nf_itf);		
	} while(!(stat&0x40));			//must!!!
	stat &= 1;
	
	nf_desel(nf_itf);
	
	return stat;
}

static int nf_rd_pgf(struct nand_flash_itf *nf_itf, u32 phy_pg, char *buf, char *oob_buf)
{
	int i, ret = 0;
	u_char cal_ecc[3];
	u_char rd_ecc[3];
	u_char ecc_code[2][3];
	u_char idx;
	signed char ecc_err;
	u_char *buf_p;

	nf_sel(nf_itf);
	
	nf_wr_cmd(nf_itf, READCMD0);
	nf_wr_addr(nf_itf, 0);
	nf_wr_addr(nf_itf, phy_pg);
	nf_wr_addr(nf_itf, phy_pg>>8);
	if(nf_itf->addr_cycle)
		nf_wr_addr(nf_itf, phy_pg>>16);
	
	nf_wait_busy(nf_itf);
	
	buf_p = buf;
	cal_ecc[0] = cal_ecc[1] = cal_ecc[2] = 0;
	ecc_code[0][0] = ecc_code[0][1] = ecc_code[0][2] = 0;
	
	/* Read data and Build up column parity */ 
	for(i=0; i<(NF_PAGE_SIZE>>1); i++) {
	
		buf_p[i] = nf_rd_data(nf_itf);
		
		/* Get CP0 - CP5 from table */
		idx = nand_ecc_precalc_table[buf_p[i]];
		cal_ecc[0] ^= (idx & 0x3f);
		
		/* All bit XOR = 1 ? */
		if (idx & 0x40) {
			cal_ecc[2] ^= (u_char) i;
			cal_ecc[1] ^= ~((u_char) i);
		}
	}
	
	/* Create non-inverted ECC code from line parity */
	nand_trans_result(cal_ecc[1], cal_ecc[2], ecc_code[0]);
	
	/* Calculate final ECC code */
	ecc_code[0][0] = ~ecc_code[0][0];
	ecc_code[0][1] = ~ecc_code[0][1];
	ecc_code[0][2] = ((~cal_ecc[0]) << 2) | 0x03;
	
	buf_p = buf+(NF_PAGE_SIZE>>1);
	cal_ecc[0] = cal_ecc[1] = cal_ecc[2] = 0;
	ecc_code[1][0] = ecc_code[1][1] = ecc_code[1][2] = 0;
	
	/* Read data and Build up column parity */ 
	for(i=0; i<(NF_PAGE_SIZE>>1); i++) {
	
		buf_p[i] = nf_rd_data(nf_itf);
		
		/* Get CP0 - CP5 from table */
		idx = nand_ecc_precalc_table[buf_p[i]];
		cal_ecc[0] ^= (idx & 0x3f);
		
		/* All bit XOR = 1 ? */
		if (idx & 0x40) {
			cal_ecc[2] ^= (u_char) i;
			cal_ecc[1] ^= ~((u_char) i);
		}
	}
	
	/* Create non-inverted ECC code from line parity */
	nand_trans_result(cal_ecc[1], cal_ecc[2], ecc_code[1]);
	
	/* Calculate final ECC code */
	ecc_code[1][0] = ~ecc_code[1][0];
	ecc_code[1][1] = ~ecc_code[1][1];
	ecc_code[1][2] = ((~cal_ecc[0]) << 2) | 0x03;
	
	for(i=0; i<NF_OOB_SIZE; i++)
		oob_buf[i] = nf_rd_data(nf_itf);
	
	nf_desel(nf_itf);
///		printk("r pg 0x%x:%x,%x,%x,%x\n", phy_pg,
///			buf[255], buf[511], oob_buf[8], oob_buf[9]);

	rd_ecc[0] = oob_buf[OOB_ECCL0];
	rd_ecc[1] = oob_buf[OOB_ECCL1];
	rd_ecc[2] = oob_buf[OOB_ECCL2];
	
	ecc_err = nand_correct_data(buf, rd_ecc, ecc_code[0]);
	if(ecc_err>0)		//correct
		ret += 0x100 ;
	else if(ecc_err<0)	//uncorrect
		ret += 1;
	
	rd_ecc[0] = oob_buf[OOB_ECCH0];
	rd_ecc[1] = oob_buf[OOB_ECCH1];
	rd_ecc[2] = oob_buf[OOB_ECCH2];
	
	ecc_err = nand_correct_data(buf+256, rd_ecc, ecc_code[1]);
	if(ecc_err>0)		//correct
		ret += 0x100 ;
	else if(ecc_err<0)	//uncorrect
		ret += 1;
	
	return ret;
}

static int nf_rd_pg(struct nand_flash_itf *nf_itf, u32 phy_pg, char *buf, char *oob_buf)
{
	int i, ret, err;
	u_char cal_ecc[3];
	u_char rd_ecc[3];
	
	nf_sel(nf_itf);
	
	nf_wr_cmd(nf_itf, READCMD0);
	nf_wr_addr(nf_itf, 0);
	nf_wr_addr(nf_itf, phy_pg);
	nf_wr_addr(nf_itf, phy_pg>>8);
	if(nf_itf->addr_cycle)
		nf_wr_addr(nf_itf, phy_pg>>16);
	
	nf_wait_busy(nf_itf);
	
	for(i=0; i<NF_PAGE_SIZE; i++)
		buf[i] = nf_rd_data(nf_itf);
	for(i=0; i<NF_OOB_SIZE; i++)
		oob_buf[i] = nf_rd_data(nf_itf);
	
	nf_desel(nf_itf);
	
	ret = 0;
	
	nand_calculate_ecc(buf, cal_ecc);
	rd_ecc[0] = oob_buf[OOB_ECCL0];
	rd_ecc[1] = oob_buf[OOB_ECCL1];
	rd_ecc[2] = oob_buf[OOB_ECCL2];
	err = nand_correct_data(buf, rd_ecc, cal_ecc);
	if(err>0)
		ret = 0x100;
	else if(err<0)
		ret = 1;
	
	nand_calculate_ecc(buf+(NF_PAGE_SIZE>>1), cal_ecc);
	rd_ecc[0] = oob_buf[OOB_ECCH0];
	rd_ecc[1] = oob_buf[OOB_ECCH1];
	rd_ecc[2] = oob_buf[OOB_ECCH2];
	err = nand_correct_data(buf+(NF_PAGE_SIZE>>1), rd_ecc, cal_ecc);
	if(err>0)
		ret += 0x100;
	else if(err<0)
		ret += 1;
	
	return ret;
}

static void nf_rd_oob(struct nand_flash_itf *nf_itf, u32 phy_pg, char *buf)
{
	int i;
	
	nf_sel(nf_itf);
	
	nf_wr_cmd(nf_itf, READCMD2);
	nf_wr_addr(nf_itf, 0);
	nf_wr_addr(nf_itf, phy_pg);
	nf_wr_addr(nf_itf, phy_pg>>8);
	if(nf_itf->addr_cycle)
		nf_wr_addr(nf_itf, phy_pg>>16);
		
	nf_wait_busy(nf_itf);
	
	for(i=0; i<NF_OOB_SIZE; i++)
		buf[i] = nf_rd_data(nf_itf);
		
	nf_wr_cmd(nf_itf, READCMD0);
	
	nf_desel(nf_itf);
}

static inline void nf_reset_chip(struct nand_flash_itf *nf_itf)
{
	nf_sel(nf_itf);
	nf_wr_cmd(nf_itf, RSTCHIPCMD);
	nf_wait_busy(nf_itf);
	nf_desel(nf_itf);
}

static void dft_nf_ce_ctl(int ce)
{
}

static int dft_nf_ready(void)
{
	udelay(20);
	return 1;
}

/**********************************************************************/

static int nf_blk_dev_open(void *disk)
{
	struct nf_partition *p = (struct nf_partition *)disk;
	
	if(!p)
		return -ENODEV;
		//printk("open 1\n");
//	if(!p->parent->open_count)
		p->parent->open_release(1);
	p->parent->open_count++;
	
	p->open_count++;
		//printk("open 2\n");
//	printk("nand flash chip %d open count=%d\n", p->parent->chip_idx, p->parent->open_count++);
	
	return 0;
}

static inline u16 part_update_blk(struct nf_partition *p);

static int nf_blk_dev_release(void *disk)
{
	struct nf_partition *p = (struct nf_partition *)disk;
	
	if(!p)
		return -ENODEV;
	
	p->open_count--;
	if(!p->open_count) {
		int act;
		act = p->flags&(NF_PART_BLK_DEV|NF_PART_BLK_FMT|NF_PART_BLK_RO);
		act = (act==(NF_PART_BLK_DEV|NF_PART_BLK_FMT));
		if(act) {
			down(&p->parent->op_permit);
			if(!is_null_blk(p->last_vir_blk)) {
				p->last_phy_blk = part_update_blk(p);
				p->last_vir_blk = NF_NULL_BLK;
				p->pg_chg_msk   = 0;
			}
			up(&p->parent->op_permit);
		}
	}
	
	p->parent->open_count--;
//	if(!p->parent->open_count)	//can't use if(--p->parent->open_count)
		p->parent->open_release(0);
	
//	printk("nand flash chip %d open count=%d\n", p->parent->chip_idx, p->parent->open_count++);
		
	return 0;
}

static void init_nfpart_map(struct nand_flash_itf *nf_itf, struct nf_partition *p);

static int nf_blk_dev_ioctl(void *disk, unsigned int cmd, unsigned long arg)
{
	struct nf_partition *p = (struct nf_partition *)disk;
	u32 blk;
	int stat;
	
	if(!p)
		return 1;//-ENODEV;
	
	switch (cmd) {
	
	case IOC_NF_GET_BLKS:
		//return all blocks
		return put_user(part_max_blk(p), (unsigned long *)arg);
		
	case IOC_NF_FMT_BLK:
		//can't erase a block if the part is opened by other application
		//for read or write if the p->flags&NF_PART_BLK_FMT
		stat = get_user(blk, (unsigned long *)arg);
		if(stat<0)
			return stat;
		if(blk>=part_max_blk(p))
			return -EINVAL;
		if(down_interruptible(&p->parent->op_permit))
			return -ERESTARTSYS;
	//	if(p->map)
	//		for(stat=0; stat<part_max_blk(p); stat++)
	//			if(p->map[stat]==blk) {
	//				p->map[stat] = -1;//0xffff;
	//				break;
	//			}
		stat = 0;
		blk = part_phy_blk(p, blk);
		if(nf_erase_blk(&p->parent->nf_itf, blk_pg(blk, 0))) {
			printk("bad block %d\n", blk);
			//p->bad_blks++;
			stat = -1;
		}
		up(&p->parent->op_permit);
		return put_user(stat, (unsigned long *)arg);
		
	case IOC_NF_RST_FMT:
//		if(!p->map)			//if p-flags&NF_PART_BLK_DEV, p->map is not null
//			return -ENOMEM;
		//use this after all blocks are erased!!!
		//can't read or write this part at this time
		if(down_interruptible(&p->parent->op_permit))
			return -ERESTARTSYS;
		p->flags |= NF_PART_BLK_FMT;
		init_nfpart_map(&p->parent->nf_itf, p);
		up(&p->parent->op_permit);
		return 0;
	}
	
	return 1;//-ENOSYS;
}

static int nf_proc_rd_entry(char *info)
{
	int len = 0;
	int d, i;
	
	if(down_interruptible(&nf_dev_list_op))
		return 0;//-ERESTARTSYS;
	
	for(d=0; d<nf_dev_count; d++) {		
		if(nf_ctl_ptr[d]) {
			struct nf_partition *p = nf_ctl_ptr[d]->part_table;
			len += sprintf(info+len, "NAND Flash chip %d, size=%dM\n", d, nf_ctl_ptr[d]->size>>20);
			len += sprintf(info+len, "part offset size used_blks bad_blks init_err erase_err wr_err ecc_ok ecc_fail\n");
			for(i=0; i<nf_ctl_ptr[d]->parts; i++, p++) {
				len += sprintf(info+len, "%d(R%c): 0x%08x, 0x%08x, %5d, %5d, %5d, %5d, %5d, %5d, %5d\n", 
									i, (p->flags&NF_PART_BLK_RO)?'O':'W', p->offset, p->size, 
									p->used_blks, p->bad_blks, 
									p->statics.init_errs, 
									p->statics.erase_errs, 
									p->statics.write_errs, 
									p->statics.ecc_success, 
									p->statics.ecc_fail);
			}
		}
	}
	
	up(&nf_dev_list_op);
	
	return len;
}

static int nf_blk_dev_rdsect(void *data, char *buffer, unsigned long sector)
{
	struct nf_partition *p;
	struct nf_ctl_info *nf_dev;
	u16 blk_no;
	
	p = (struct nf_partition *)data;
	nf_dev = p->parent;
	
	//if the sector in last write block and be changed, read it from block buffer
	if(p->last_vir_blk==(sector>>BLK_SFT_PG))
		if(p->pg_chg_msk&(1<<pg_of_blk(sector))) {
			memcpy(buffer, p->blk_buf+(pg_of_blk(sector)<<NF_PG_SFT), NF_PAGE_SIZE);
			return 0;
		}

#ifdef	NO_VIR_TO_PHY_MAP
	blk_no = sector>>BLK_SFT_PG;
#else
	blk_no = p->map[sector>>BLK_SFT_PG];
#endif

	//no phy block mapped to this virt block, fill the buffer with 'n'
	if(is_null_blk(blk_no))
		memset(buffer, 'n', NF_PAGE_SIZE);
	else {
		u32 phy_pg;
		char oob_buf[NF_OOB_SIZE];
		int rd_ret;
				
		phy_pg = blk_pg(part_phy_blk(p, blk_no), pg_of_blk(sector));
	///	printk("r pg 0x%x\n", phy_pg);
		rd_ret = nf_rd_pg(&nf_dev->nf_itf, phy_pg, buffer, oob_buf);
		if(rd_ret&0x3) {
			p->statics.ecc_fail += rd_ret&3;
			printk(KERN_WARNING "nand ecc uncorrectable %d\n", rd_ret&3);
		}
		if(rd_ret&0x300) {
			p->statics.ecc_success += rd_ret>>8;
			printk(KERN_INFO "nand ecc correction %d\n", rd_ret>>8);
		}
	}
	
	return 0;
}

static inline u16 part_search_null_blk(struct nf_partition *p)
{
	struct nf_ctl_info *nf_dev;
	u16 eb, rb;
	
	nf_dev = p->parent;
	
	//recored the start point
	eb = p->null_blk_idx;
	rb = part_max_blk(p);
	
	do {
		char oob_buf[NF_OOB_SIZE];
		//check if it's a null block
		nf_rd_oob(&nf_dev->nf_itf, blk_pg(part_phy_blk(p, p->null_blk_idx), 0), oob_buf);
		if(is_null_blk(get_blk_no(oob_buf))) {
			eb = p->null_blk_idx;
			p->null_blk_idx++;	//increase the null block number index
			if(p->null_blk_idx>=rb)
				p->null_blk_idx = 0;	//up to the limit, reset it to zero
			return eb;	//it's a null block, return it's number
		}
		
		p->null_blk_idx++;	//increase the null block number index
		if(p->null_blk_idx>=rb)
			p->null_blk_idx = 0;	//up to the limit, reset it to zero
	} while(p->null_blk_idx!=eb);
	
	return NF_NULL_BLK;	//fail to find a null block
}

static inline u16 part_update_blk(struct nf_partition *p)
{
	struct nf_ctl_info *nf_dev;
	int i;
	u16 orig_blk, ret = NF_NULL_BLK;
	u32 phy_pg;
	char oob_buf[NF_OOB_SIZE];
	
	nf_dev = p->parent;
	
#ifdef	NO_VIR_TO_PHY_MAP
	orig_blk = p->last_vir_blk;
#else
	orig_blk = p->map[p->last_vir_blk];
#endif
//		printk("w blk p=0x%x,v=0x%x\n", p->last_phy_blk, p->last_vir_blk);
	if(is_null_blk(orig_blk))
		p->used_blks++;		//add used blocks statics
	else {
		//copy unchanged pages from orignal block to block buffer
		phy_pg = blk_pg(part_phy_blk(p, orig_blk), 0);
		for(i=0; i<PAGES_PER_BLOCK; i++)
			if(!(p->pg_chg_msk&(1<<i))) {
				int rd_ret;
				
				rd_ret = nf_rd_pg(&nf_dev->nf_itf, phy_pg+i, p->blk_buf+(i<<NF_PG_SFT), oob_buf);
				if(rd_ret&0x3) {
					p->statics.ecc_fail += rd_ret&3;
					printk(KERN_WARNING "nand ecc uncorrectable %d\n", rd_ret&3);
				}
				if(rd_ret&0x300) {
					p->statics.ecc_success += rd_ret>>8;
					printk(KERN_INFO "nand ecc correction %d\n", rd_ret>>8);
				}
			}
		//erase orignal block
		if(nf_erase_blk(&nf_dev->nf_itf, phy_pg)) {
			p->bad_blks++;	//if erase fail, add bad blocks staics
			p->statics.erase_errs++;
		} else
			ret = orig_blk;	//else, set it to be return
	}

	//set oob info after copy
	for(i=0; i<NF_OOB_SIZE; i++)
		oob_buf[i] =0xff;
//	mark_good_blk(oob_buf);	//it's 0xff
	set_blk_no(oob_buf, p->last_vir_blk);

	do {
		//start page of the block	
		phy_pg = blk_pg(part_phy_blk(p, p->last_phy_blk), 0);
		for(i=0; i<PAGES_PER_BLOCK; i++) {
			oob_buf[OOB_FMT_TAG] = nf_fmt_tag[i];	//set format tag
	///		printk("w pg 0x%x\n", phy_pg+i);
			if(nf_wr_pg(&nf_dev->nf_itf, phy_pg+i, p->blk_buf+(i<<NF_PG_SFT), oob_buf)) {
				//if a error occured when write page
				printk(KERN_WARNING "fail to write page 0x%x when update block\n", phy_pg);
#ifndef	NO_VIR_TO_PHY_MAP
				p->statics.write_errs++;
				//mark it to bad block
				nf_mark_bad_blk(&nf_dev->nf_itf, phy_pg);
				p->bad_blks++;	//add bad block statics
				//re-find a null block to be writen
				p->last_phy_blk = part_search_null_blk(p);
				if(is_null_blk(p->last_phy_blk)) {
					//if fail to find a null block
					//mark the map with this virtual block with null and return
					p->map[p->last_vir_blk] = NF_NULL_BLK;
					return NF_NULL_BLK;
				}
				//if the new found null block is equal to
				//the orignal block erased before, set return block to be null
				if(p->last_phy_blk==orig_blk)
					ret = NF_NULL_BLK;
				break;	//try to write block buffer back to new block
#endif
			}
		}
	} while(i<PAGES_PER_BLOCK);
	
	//update the virtual block map
	p->map[p->last_vir_blk] = p->last_phy_blk;

	//return the erased orignal block number or null block
	return ret;
	
/*	if(!is_null_blk(orig_blk)) {
		phy_pg = blk_pg(part_phy_blk(p, orig_blk), 0);
		if(nf_erase_blk(&nf_dev->nf_itf, phy_pg)) {
			p->bad_blks++;
			p->statics.erase_errs++;
			return NF_NULL_BLK;
		}
		return orig_blk;
	}

	return NF_NULL_BLK;
*/
}

static int nf_blk_dev_wrsect(void *data, char *buffer, unsigned long sector)
{
	struct nf_partition *p;
	u16 blk_no;
	
	p = (struct nf_partition *)data;
	
	//if it's a read only part, return with no error, no action
	if(p->flags&NF_PART_BLK_RO)
		return 0;//1;
//	if(!(p->flags&NF_PART_BLK_FMT))	//it has been checked when locate device
//		return 1;

	blk_no = sector>>BLK_SFT_PG;
	
	//if the block to be write is not qrual to the last write block 
	//or it the first write block from the time device be initilized
	if(blk_no!=p->last_vir_blk) {
		//if it's not the first write block
		//write the block buffer back to block
		if(!is_null_blk(p->last_vir_blk))
			p->last_phy_blk = part_update_blk(p);
#ifdef	NO_VIR_TO_PHY_MAP
		p->last_phy_blk = blk_no;
#else
		//acquire a null block for new write
		if(is_null_blk(p->last_phy_blk))
			p->last_phy_blk = part_search_null_blk(p);
		//if fail to acquire a null block, return with error
		if(is_null_blk(p->last_phy_blk))
			return 1;
#endif
		//set current write virtul block number
		p->last_vir_blk = blk_no;
		//reset page change status mask flags
		p->pg_chg_msk = 0;
	}
	
	//copy the data to block buffer
	memcpy(p->blk_buf+(pg_of_blk(sector)<<NF_PG_SFT), buffer, NF_PAGE_SIZE);
	//set page change flag
	p->pg_chg_msk |= 1<<pg_of_blk(sector);
	//make a time stamp
	p->w_jiffies = jiffies;
	
	return 0;
}

static int nf_blk_dev_transfer(void *device, const struct request *req)
{
	unsigned long nsect, sects;
	char *buf;
	struct nf_partition *p;
	u16 blk_no, sect_no;
	u16 in_blk_buf, blk_is_null;
	u32 phy_pg;
	int cmd;
	
	p = (struct nf_partition *)device;
	
	nsect = req->current_nr_sectors;
	buf = req->buffer;
	
	//part_avail_blks will decrease if bad block increase
	if(req->sector+nsect>(part_avail_blks(p)<<BLK_SFT_PG)) {
		printk(KERN_WARNING "Access beyond end of device.\n");
		return 0;
	}
	
	blk_no = req->sector>>BLK_SFT_PG;
	sect_no = req->sector;
	
//KERNEL_VERSION defined in include/linux/version.h
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	cmd = rq_data_dir(req);
#else
	cmd = req->cmd;
#endif
	//	printk(KERN_INFO "%c 0x%08x, sects %u\n", (cmd==READ)?'r':'w', sect_no, nsect);
	switch(cmd) {

	case READ:
		if(down_interruptible(&p->parent->op_permit)<0)
			return 0;	//return with error
		
		for(; nsect; blk_no++) {
			sect_no = pg_of_blk(sect_no);
			sects = PAGES_PER_BLOCK-sect_no;
			if(sects>nsect)
				sects = nsect;
			nsect = nsect-sects;
			
			in_blk_buf = (blk_no==p->last_vir_blk);
			blk_is_null = is_null_blk(p->map[blk_no]);
			//if blk_is_null, phy_pg is no use
			phy_pg = blk_pg(part_phy_blk(p, p->map[blk_no]), 0);
			
			for(; sects; sects--, sect_no++, buf+=NF_PAGE_SIZE) {
				//if the sector in last write block and be changed, read it from block buffer
				if(in_blk_buf&&(p->pg_chg_msk&(1<<sect_no)))
					memcpy(buf, p->blk_buf+(sect_no<<NF_PG_SFT), NF_PAGE_SIZE);
				//no phy block mapped to this virt block, fill the buffer with 'n'
				else if(blk_is_null)
					memset(buf, 'n', NF_PAGE_SIZE);
				//read data from flash to buffer
				else {
					char oob_buf[NF_OOB_SIZE];
					int rd_ret;
				
				///	printk("r pg 0x%x\n", phy_pg+sect_no);
					rd_ret = nf_rd_pg(&p->parent->nf_itf, phy_pg+sect_no, buf, oob_buf);
					if(rd_ret&0x3) {
						p->statics.ecc_fail += rd_ret&3;
						printk(KERN_WARNING "nand ecc uncorrectable %d\n", rd_ret&3);
					}
					if(rd_ret&0x300) {
						p->statics.ecc_success += rd_ret>>8;
						printk(KERN_INFO "nand ecc correction %d\n", rd_ret>>8);
					}
				}
			}
		}
		
		up(&p->parent->op_permit);
		return 1;
		
	case WRITE:
		//if it's a read only part, return with no error, no action
		if(p->flags&NF_PART_BLK_RO)
			return 1;//0;
		
		if(down_interruptible(&p->parent->op_permit)<0)
			return 0;	//return with error
		
		for(; nsect; blk_no++) {			
			sect_no = pg_of_blk(sect_no);
			sects = PAGES_PER_BLOCK-sect_no;
			if(sects>nsect)
				sects = nsect;
			nsect = nsect-sects;
			
			//if the block to be write is not qrual to the last write block 
			//or it the first write block from the time device be initilized
			if(blk_no!=p->last_vir_blk) {
				//if it's not the first write block
				//write the block buffer back to block
				if(!is_null_blk(p->last_vir_blk))
					p->last_phy_blk = part_update_blk(p);
				//acquire a null block for new write
				if(is_null_blk(p->last_phy_blk))
					p->last_phy_blk = part_search_null_blk(p);
				//if fail to acquire a null block, return with error
				if(is_null_blk(p->last_phy_blk)) {
					up(&p->parent->op_permit);
					return 0;	//no space left, return with error
				}
				//set current write virtul block number
				p->last_vir_blk = blk_no;
				//reset page change status mask flags
				p->pg_chg_msk = 0;
			}
			
			memcpy(p->blk_buf+(sect_no<<NF_PG_SFT), buf, sects<<NF_PG_SFT);
			p->pg_chg_msk |= ((u32)-1>>(PAGES_PER_BLOCK-sect_no-sects))&((u32)-1<<sect_no);
			sect_no += sects;
			buf += sects<<NF_PG_SFT;

/*			for(; sects; sects--, sect_no++, buf+=NF_PAGE_SIZE) {
				//copy the data to block buffer
				memcpy(p->blk_buf+(sect_no<<NF_PG_SFT), buf, NF_PAGE_SIZE);
				//set page change flag
				p->pg_chg_msk |= 1<<sect_no;
			}
*/		
		}
		
		//make a time stamp
		p->w_jiffies = jiffies;
		
		up(&p->parent->op_permit);	
		return 1;

	default:
		printk("%s:Unknown request cmd 0x%x\n", __FUNCTION__, cmd);
		return 0;
	}

}

//use static struct can default initialize the undefinde area to zero
//if initialize the struct in function, must set the undefined area to zero visualy!!!
static struct attach_bdops nf_bdops = {
	open:			nf_blk_dev_open,
	release:		nf_blk_dev_release,
	ioctl:			nf_blk_dev_ioctl,
	do_transfer:	nf_blk_dev_transfer,
	read:			nf_blk_dev_rdsect,
	write:			nf_blk_dev_wrsect,
	proc_rd_entry:	nf_proc_rd_entry,
};

/**********************************************************************/

static inline void search_nf_fmt_tag(struct nand_flash_itf *nf_itf, struct nf_partition *p)
{
	u16 pg, tag_len;
	u32 blk, eb;
	char oob[NF_OOB_SIZE];
	
	tag_len = strlen(nf_fmt_tag);

//	p->flasgs &= ~NF_PART_BLK_FMT;
		
	blk = part_phy_blk_start(p);
	eb  = part_phy_blk_end(p);
		
	for(; blk<eb; blk++) {
		for(pg=0; pg<tag_len; pg++) {
			nf_rd_oob(nf_itf, blk_pg(blk, pg), oob);
			if(oob[OOB_FMT_TAG]!=nf_fmt_tag[pg])
				break;
		}
		if(pg>=tag_len) {
			p->flags |= NF_PART_BLK_FMT;
			break;
		}
	}
}

static void init_nfpart_map(struct nand_flash_itf *nf_itf, struct nf_partition *p)
{
	u16 vir_blk_no, null_blk_found = 0;
	u32 blk, eb;
	char oob[NF_OOB_SIZE];
	
	//must initialize these regardless of the NF_PARTBLK_FMT flag!
	p->last_vir_blk = p->last_phy_blk = NF_NULL_BLK;
	p->pg_chg_msk = 0;	//it needn't, when is_null_blk(p->last_vir_blk), it's set to 0
	p->bad_blks  = 0;
	p->used_blks = 0;
	memset(&p->statics, 0, sizeof(p->statics));
//	p->statics.init_errs   = 0;
//	p->statics.erase_errs  = 0;
//	p->statics.write_errs  = 0;
//	p->statics.ecc_fail    = 0;
//	p->statics.ecc_success = 0;

	if(p->map&&(p->flags&NF_PART_BLK_FMT)) {
		memset(p->map, 0xff, part_max_blk(p)<<1);
		
		blk = part_phy_blk_start(p);
		eb  = part_phy_blk_end(p);
		
		for(; blk<eb; blk++) {
			nf_rd_oob(nf_itf, blk_pg(blk, 0), oob);
			if(is_bad_blk(oob)) {
				p->bad_blks++;
			} else {
				vir_blk_no = get_blk_no(oob);
				if(!is_null_blk(vir_blk_no)) {
					if(vir_blk_no>=part_max_blk(p)) {
						printk(KERN_WARNING "block p:0x%x, v:0x%x beyond the part size\n", 
									blk-part_phy_blk_start(p), 
									vir_blk_no);
						if(!(p->flags&NF_PART_BLK_RO)) {
							printk(KERN_ERR "erase it!\n");
							if(nf_erase_blk(nf_itf, blk_pg(blk, 0))) {
								p->bad_blks++;
								p->statics.erase_errs++;
							} else if(!null_blk_found) {
									null_blk_found = 1;
									p->null_blk_idx = blk-part_phy_blk_start(p);
							}
						}
						p->statics.init_errs++;
					} else {
						if(!is_null_blk(p->map[vir_blk_no])) {
							printk(KERN_WARNING "block 0x%x and 0x%x map to the same virtual block 0x%x\n", 
									p->map[vir_blk_no], blk-part_phy_blk_start(p), vir_blk_no);
							if(!(p->flags&NF_PART_BLK_RO)) {
								printk(KERN_WARNING "erase it!\n");
								if(nf_erase_blk(nf_itf, blk_pg(blk, 0))) {
									p->bad_blks++;
									p->statics.erase_errs++;
								} else if(!null_blk_found) {
									null_blk_found = 1;
									p->null_blk_idx = blk-part_phy_blk_start(p);
								}
							}
							p->statics.init_errs++;
						} else {
							//a valid mapped block
							p->map[vir_blk_no] = blk-part_phy_blk_start(p);
							p->used_blks++;	//add used blocks statics
						}
					}
				} else {
					if(!null_blk_found) {
						null_blk_found = 1;
						p->null_blk_idx = blk-part_phy_blk_start(p);
					}
				}
			}
		}
	}
}

extern void *register_mmc_emu_disk(void *disk, 
								  struct attach_bdops *disk_ops, 
								  struct emu_disk_para *disk_para, 
								  struct device *dev);
extern int unregister_mmc_emu_disk(void *disk);

static int nf_register_blk_drv(struct nf_ctl_info *nf_dev, struct device *dev)
{
	int i, err;
	
/*	nf_dev->blk_buf = (u16 *)(__get_free_pages(GFP_KERNEL, 2));	//4 pages,16KB
	if(!nf_dev->blk_buf) {
		printk(KERN_ERR "fail to allocate %s memory for chip %d!\n", "buffer", nf_dev->chip_idx);
		return -1;
	}
*/	
	if(nf_dev->size<=SIZE_1G) {	//map memory <= 128K
		u16 *map_base, *map;
		
		map_base = kmalloc(nf_dev->size>>(NF_BLK_SFT-1), GFP_KERNEL);
		if(!map_base) {	
			printk(KERN_ERR "fail to allocate %s memory for chip %d!\n", "map", nf_dev->chip_idx);
			return -1;
		}
		
		map = map_base;
		for(i=0; i<nf_dev->parts; i++) {
			if(map>=(map_base+(nf_dev->size>>NF_BLK_SFT))||
			   map!=(map_base+(nf_dev->part_table[i].offset>>NF_BLK_SFT))) {
				//free_pages((unsigned long)nf_dev->blk_buf, 2);
				printk(KERN_WARNING "%s:unexpected error in part tabel", __FUNCTION__);
				kfree(map_base);
				for(i--; i>=0; i--)	//reset part->map to zero
					nf_dev->part_table[i].map = NULL;
				break;
			}
			nf_dev->part_table[i].map = map;
			map += nf_dev->part_table[i].size>>NF_BLK_SFT;
		}
		
		err = (i<0)?-1:0;
	} else {
		u32 offset = 0;
		
		err = -1;	//must set to be non zero;
		
		for(i=0; i<nf_dev->parts; i++) {
			if(nf_dev->part_table[i].offset>=nf_dev->size||
			   nf_dev->part_table[i].offset!=offset) {
				//free_pages((unsigned long)nf_dev->blk_buf, 2);
				printk(KERN_WARNING "%s:unexpected error in part tabel", __FUNCTION__);
				for(i--; i>=0; i--)
					if(nf_dev->part_table[i].map) {
						kfree(nf_dev->part_table[i].map);
						nf_dev->part_table[i].map = NULL;
					}
				err = -1;
				break;
			}
			nf_dev->part_table[i].map = kmalloc(nf_dev->part_table[i].size>>(NF_BLK_SFT-1), GFP_KERNEL);
			if(!nf_dev->part_table[i].map) {
				printk(KERN_WARNING "fail to allocate map memory for chip %d, part %d\n", 
							nf_dev->chip_idx, i);
			} else
				err = 0;	//success to allocate a buffer for one part at least
			offset += nf_dev->part_table[i].size;
		}
	}
	
	if(!err) {
		for(i=0; i<nf_dev->parts; i++) {
			struct nf_partition *p = nf_dev->part_table+i;
		
			search_nf_fmt_tag(&nf_dev->nf_itf, p);
			
			if(!(p->flags&NF_PART_BLK_FMT))
				printk(KERN_WARNING "can't find format tag on chip %d, part %d!\n", 
								nf_dev->chip_idx, i);

			//printk("%x,%d\n", p->flags, i);
			
			if(!p->map||!(p->flags&NF_PART_BLK_DISK))
				continue;
			
//			memset(nf_dev->part_table[i].map, 0xff, nf_dev->part_table[i].size>>(NF_BLK_SFT-1));
			//initilize map in init_nfpart_map
			init_nfpart_map(&nf_dev->nf_itf, p);
			
			p->emu_disk.sect_size  = NF_PAGE_SIZE;
			p->emu_disk.sect_count = (part_avail_blks(p)-p->rsv_blks)<<BLK_SFT_PG;
			p->emu_disk.blk_size   = NF_BLOCK_SIZE;
			p->emu_disk.ro         = p->flags&NF_PART_BLK_RO;
			
			p->disk = register_mmc_emu_disk(p, 
											&nf_bdops, 
											&p->emu_disk, 
											dev);
			if(p->disk) {
				//set regiserd flag
				p->flags  |= NF_PART_BLK_DEV;
				//if the part is writable, allocate block buffer for write operation
				if(!(p->flags&NF_PART_BLK_RO)) {
					p->blk_buf = (char *)(__get_free_pages(GFP_KERNEL, 2));	//4 pages,16KB
					if(!p->blk_buf) {
						//fail to allocate write buffer for this part, so set it to be read-only
						printk(KERN_WARNING "fail to alloctae buffer for chip %d, part %d, set to RO\n", 
								nf_dev->chip_idx, i);
						p->flags |= NF_PART_BLK_RO;
						p->emu_disk.ro = 1;
					}
				}
			}
		}
	}
	
	return err;
}

static inline void nf_unregister_blk_drv(struct nf_ctl_info *nf_dev)
{
	int i;
	
//	if(nf_dev->blk_buf) {
//		memset(nf_dev->blk_buf, 0, NF_BLOCK_SIZE);
//		free_pages((unsigned long)nf_dev->blk_buf, 2);
//	}
		
	if(nf_dev->size<=SIZE_1G) {	//map memory <= 128K
		if(nf_dev->part_table[0].map) {
			memset(nf_dev->part_table[0].map, 0, nf_dev->size>>(NF_BLK_SFT-1));
			kfree(nf_dev->part_table[0].map);
		}
	} else {
		for(i=0; i<nf_dev->parts; i++)
			if(nf_dev->part_table[i].map) {
				memset(nf_dev->part_table[i].map, 0, nf_dev->part_table[i].size>>(NF_BLK_SFT-1));
				kfree(nf_dev->part_table[i].map);
			}
	}
		
	for(i=0; i<nf_dev->parts; i++) {
		//zero the map pointer
		nf_dev->part_table[i].map = NULL;
		if(nf_dev->part_table[i].flags&NF_PART_BLK_DEV) {
			nf_dev->part_table[i].flags &= ~NF_PART_BLK_DEV;
			
			if(nf_dev->part_table[i].disk)
				unregister_mmc_emu_disk(nf_dev->part_table[i].disk);
				
			if(!(nf_dev->part_table[i].flags&NF_PART_BLK_RO)) {
				nf_dev->part_table[i].flags |= NF_PART_BLK_RO;
				//free write buffer
				memset(nf_dev->part_table[i].blk_buf, 0, NF_BLOCK_SIZE);
				free_pages((unsigned long)nf_dev->part_table[i].blk_buf, 2);
				nf_dev->part_table[i].blk_buf = NULL;
			}
		}
	}

}

/**********************************************************************/

static void nf_dev_chk_dirty_parts(void *dummy)
{
	int i, j;
	struct nf_ctl_info *nf_dev;
	struct nf_partition *p;
	
	down(&nf_dev_list_op);	//how to do if module exit happen when this execute?

//	printk("check dirty\n");

	for(i=0; i<MAX_NAND_FLASH_DEV; i++)
		if(nf_ctl_ptr[i]) {
			nf_dev = nf_ctl_ptr[i];
			down(&nf_dev->op_permit);
			p = nf_dev->part_table;			
			for(j=0; j<nf_dev->parts; j++, p++) {
				int act;
				act = p->flags&(NF_PART_BLK_DEV|NF_PART_BLK_FMT|NF_PART_BLK_RO);
				act = (act==(NF_PART_BLK_DEV|NF_PART_BLK_FMT));
				if(!act)
					continue;
				if(is_null_blk(p->last_vir_blk))
					continue;
				if((jiffies-p->w_jiffies)>HZ) {
	//printk("chk_dirty:chip%d,part%d,0x%x 0x%x 0x%x\n",i,j,p->last_vir_blk,p->last_phy_blk,p->pg_chg_msk);
					p->last_phy_blk = part_update_blk(p);
					p->last_vir_blk = NF_NULL_BLK;
					p->pg_chg_msk   = 0;
				}
			}
			up(&nf_dev->op_permit);
		}		
	
	up(&nf_dev_list_op);	
}

static struct timer_list nf_dev_timer;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
//in kernel2.6, can use schedule_delayed_work, but here we still use timer+work
static struct work_struct nf_dev_timer_task;
#else
static struct tq_struct nf_dev_timer_task = {
	.routine = nf_dev_chk_dirty_parts,
};
#endif

static void nf_dev_timer_proc(unsigned long data)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	schedule_work(&nf_dev_timer_task);
#else
	schedule_task(&nf_dev_timer_task);
#endif
	mod_timer(&nf_dev_timer, jiffies+(HZ<<1));	//2 seconds
}

/**********************************************************************/

static inline int nf_probe_chip(struct nf_ctl_info *nf_dev)
{
	u32 i, id;
	struct nand_flash_itf *nf_itf = &nf_dev->nf_itf;
	
	nf_reset_chip(nf_itf);
	
	nf_sel(nf_itf);
	
	nf_wr_cmd(nf_itf, READIDCMD);
	nf_wr_addr(nf_itf, 0);
	
	nf_wait_busy(nf_itf);
	
	id  = nf_rd_data(nf_itf)<<8;
	id |= nf_rd_data(nf_itf);
	
	nf_desel(nf_itf);
	
	for(i=0; nf_chip_info[i].id; i++)
		if(nf_chip_info[i].id==id) {
			nf_dev->size = nf_chip_info[i].size;
			nf_itf->addr_cycle = (nf_dev->size>SIZE_32M)?1:0;
			nf_dev->prod_name = nf_chip_info[i].prod_name;
			printk(KERN_INFO "Found NAND Flash Chip ID=0x%x, Size=%dM\n", id, nf_dev->size>>20);
			return 0;
	}
	
	return -1;
}

int register_nand_flash_itf(struct nand_flash_itf *nf_itf, 
							u32 parts, 
							struct reg_partition *part_table, 
							void(*open_release)(int), 
							struct device *dev)
{
	u32 total_size;
	u16 i, chip_idx;
	struct nf_ctl_info *nf_dev;
	
	if(!parts&&!part_table&&!open_release) {
		printk(KERN_ERR "%s:the parameters are invalid!\n", __FUNCTION__);
		return -EINVAL;
	}
	
	if(nf_dev_count>=MAX_NAND_FLASH_DEV) {
			printk(KERN_ERR "%s:the nand flash count overflow!\n", __FUNCTION__);
			return -ETOOMANYREFS;
	}
	
	if(down_interruptible(&nf_dev_list_op))
		return -ERESTARTSYS;
		
	list_for_each_entry(nf_dev, &nf_dev_list, list) {
		if((nf_dev->nf_itf.data==nf_itf->data)||
		   (nf_dev->nf_itf.addr==nf_itf->addr)||
		   (nf_dev->nf_itf.cmd==nf_itf->cmd)||
		   (nf_dev->nf_itf.nf_ce_ctl==nf_itf->nf_ce_ctl)||
		   (nf_dev->nf_itf.nf_ready==nf_itf->nf_ready)) {
			up(&nf_dev_list_op);
			printk(KERN_ERR "%s:the nand flash interface is registered already!\n", __FUNCTION__);
			return -ETOOMANYREFS;
		}
	}
	
	for(chip_idx=0; chip_idx<MAX_NAND_FLASH_DEV; chip_idx++) {
		int found = 0;
		list_for_each_entry(nf_dev, &nf_dev_list, list) {
			if(nf_dev->chip_idx==chip_idx) {
				found = 1;
				break;
			}
		}
//		if(nf_dev->chip_idx!=chip_idx)
//			break;
		if(!found)
			break;
	}
	if(chip_idx>=MAX_NAND_FLASH_DEV) {
		up(&nf_dev_list_op);
		printk(KERN_ERR "%s:fail to find a invalid chip index for nand flash interface!\n", __FUNCTION__);
		return -ENODEV;
	}
	
	if(parts>=MAX_NAND_FLASH_PART) {
		printk(KERN_WARNING "%s:register parts overflow, cut it!\n", __FUNCTION__);
		parts = MAX_NAND_FLASH_PART-1;
	}
	
	nf_dev = kmalloc(sizeof(struct nf_ctl_info)+parts*sizeof(struct nf_partition), GFP_KERNEL);
	if(!nf_dev) {
		up(&nf_dev_list_op);
		printk(KERN_ERR "%s:fail to allocate memory for nand flash interface!\n", __FUNCTION__);
		return -ENOMEM;
	}
	memset(nf_dev, 0, sizeof(struct nf_ctl_info)+parts*sizeof(struct nf_partition));
	
	nf_dev->nf_itf = *nf_itf;
	if(!nf_itf->nf_ce_ctl) {
		nf_dev->nf_itf.nf_ce_ctl = dft_nf_ce_ctl;
		printk(KERN_WARNING "%s:null ce_ctl function!\n", __FUNCTION__);
	}
	if(!nf_itf->nf_ready) {
		nf_dev->nf_itf.nf_ready = dft_nf_ready;
		printk(KERN_WARNING "%s:null ready function!\n", __FUNCTION__);
	}
		
	if(nf_probe_chip(nf_dev)<0) {
		up(&nf_dev_list_op);
		kfree(nf_dev);
		printk(KERN_ERR "%s:fail to probe nand flash interface!\n", __FUNCTION__);
		return -ENODEV;
	}
	
	nf_dev->open_release = open_release;
	
	printk(KERN_INFO "%s:register nand flash interface with parts\n", __FUNCTION__);
	total_size = 0;
	nf_dev->part_table = (struct nf_partition *)(nf_dev+1);
	for(i=0; (i<parts)&&(total_size<nf_dev->size); i++) {
		nf_dev->part_table[i].name   = part_table[i].name;
		nf_dev->part_table[i].offset = total_size;
		if(part_table[i].size==NF_SIZE_FULL) {
			part_table[i].size = nf_dev->size-total_size;
		}
		part_table[i].size &= ~(SIZE_64K-1);	//64K align
		nf_dev->part_table[i].size   = part_table[i].size;
		total_size += part_table[i].size;
		if(total_size>nf_dev->size)
			nf_dev->part_table[i].size -= total_size-nf_dev->size;
		if(nf_dev->part_table[i].size>=SIZE_1G) {
			nf_dev->part_table[i].size = SIZE_1G-NF_BLOCK_SIZE;	//reserve 1 block
			printk(KERN_WARNING "%s:can't set partition size larger than 1GB, cut it!\n", __FUNCTION__);
		}
		nf_dev->part_table[i].rsv_blks = part_table[i].rsv_blks;
		nf_dev->part_table[i].flags   |= part_table[i].ro_flag?NF_PART_BLK_RO:0;
		nf_dev->part_table[i].flags   |= part_table[i].disk_flag?NF_PART_BLK_DISK:0;
		nf_dev->part_table[i].parent   = nf_dev;
//		nf_dev->part_table[i].pg_chg_msk = 0;				//these initialize to 0
//		nf_dev->part_table[i].null_blk_idx = 0;				//after memory be allocated
//		nf_dev->part_table[i].last_vir_blk = NF_NULL_BLK;	//and set in init_nfpart_map
//		nf_dev->part_table[i].last_phy_blk = NF_NULL_BLK;
//		nf_dev->part_table[i].used_blks = 0;
//		nf_dev->part_table[i].bad_blks = 0;
		printk(KERN_INFO "part[%d] : start=0x%08x, size=0x%08x, name=%s(R%c)\n", 
						i, 
						nf_dev->part_table[i].offset, 
						nf_dev->part_table[i].size, 
						nf_dev->part_table[i].name,
						(nf_dev->part_table[i].flags&NF_PART_BLK_RO)?'O':'W');
	}
	if(i!=parts)
		printk(KERN_WARNING "%s:nand flash partitions extend the chip size, cut it!\n", __FUNCTION__);	
	nf_dev->parts = i;
	
	sema_init(&nf_dev->op_permit, 1);
	nf_dev->chip_idx = chip_idx;
//	nf_dev->cur_wr_blk.part = -1;
	
	printk(KERN_INFO "%s:register nand flash interface with index %d success\n", __FUNCTION__, nf_dev->chip_idx);

	down(&nf_dev->op_permit);
	
	if(nf_register_blk_drv(nf_dev, dev))
		printk(KERN_WARNING "%s:fail to register block driver for this nand flash\n", __FUNCTION__);
	
	up(&nf_dev->op_permit);
	
	list_add(&nf_dev->list, &nf_dev_list);
	nf_ctl_ptr[nf_dev->chip_idx] = nf_dev;	////////
	if(!nf_dev_count++) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
		//initialize list, func, data and timer in work_struct
		INIT_WORK(&nf_dev_timer_task, nf_dev_chk_dirty_parts, NULL);
#endif
		init_timer(&nf_dev_timer);
		nf_dev_timer.function = nf_dev_timer_proc;
		nf_dev_timer.data = 0;
		nf_dev_timer.expires = jiffies+(HZ<<1);	//2 seconds
		add_timer(&nf_dev_timer);
	}

	up(&nf_dev_list_op);
	
	return 0;
}

int unregister_nand_flash_itf(struct nand_flash_itf *nf_itf)
{
	struct nf_ctl_info *nf_dev;
	int found = 0;
	
	if(down_interruptible(&nf_dev_list_op))
		return -ERESTARTSYS;
	
	list_for_each_entry(nf_dev, &nf_dev_list, list) {
		if((nf_dev->nf_itf.data==nf_itf->data)&&
		   (nf_dev->nf_itf.addr==nf_itf->addr)&&
		   (nf_dev->nf_itf.cmd==nf_itf->cmd)) {
			found = 1;
			break;
		}
	}	
	if(!found) {
		up(&nf_dev_list_op);
		printk(KERN_ERR "%s:no such nand flash interface registered!\n", __FUNCTION__);
		return -ENODEV;
	} else {
		BUG_ON(nf_dev->open_count);	//defined in linux/kernel.h, BUG defined in asm/page.h
		
		if(down_interruptible(&nf_dev->op_permit)) {
			up(&nf_dev_list_op);
			return -ERESTARTSYS;
		}
		
		list_del(&nf_dev->list);	//delete from list after nf_unregister_blk_drv
		nf_ctl_ptr[nf_dev->chip_idx] = NULL;	////////
		nf_dev_count--;
		if(!nf_dev_count)
			del_timer_sync(&nf_dev_timer);
		
		nf_unregister_blk_drv(nf_dev);
		//up(&nf_dev->op_permit);	//needn't to up
		
		up(&nf_dev_list_op);
		
		printk(KERN_INFO "%s:unregister nand flash interface with index %d success\n", __FUNCTION__, nf_dev->chip_idx);
		memset(nf_dev, 0, sizeof(struct nf_ctl_info)+nf_dev->parts*sizeof(struct nf_partition));
		kfree(nf_dev);		
	}
	
	return 0;
}

static int __init nf_drv_init(void)
{
	INIT_LIST_HEAD(&nf_dev_list);
	sema_init(&nf_dev_list_op, 1);
	nf_dev_count = 0;
	memset(&nf_ctl_ptr, 0, sizeof(nf_ctl_ptr));
	
	printk(KERN_INFO "NAND Flash Driver Layer Ver:%s Initialized\n", NF_DRV_VER);
	
	return 0;
}

static void __exit nf_drv_exit(void)
{
	if(down_interruptible(&nf_dev_list_op)) {
		printk(KERN_ERR "fail to query nf_dev list\n");
		return;
	}
	if(!list_empty(&nf_dev_list)) {
		up(&nf_dev_list_op);
		printk(KERN_ERR "there is still nand flash interface reside in!\n");
		return;
	}
	
	nf_dev_count = 0;
	memset(&nf_ctl_ptr, 0, sizeof(nf_ctl_ptr));

	up(&nf_dev_list_op);	//needn't to up?

//	list_del_init(&nf_dev_list);	
	//INIT_LIST_HEAD(&nf_dev_list);
	
	printk(KERN_INFO "NAND Flash Driver Layer removed\n");
}

module_init(nf_drv_init);
module_exit(nf_drv_exit);

MODULE_AUTHOR("antiscle, hzhmei@hotmail.com");
MODULE_DESCRIPTION("NAND FLASH Driver");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(register_nand_flash_itf);		//must add nfdrv.o to Makefile export-objs in 2.4.x
EXPORT_SYMBOL(unregister_nand_flash_itf);
