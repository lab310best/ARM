
#ifndef	NAND_FLASH_DRIVER_H
#define	NAND_FLASH_DRIVER_H

#include <linux/types.h>
#include <asm/sizes.h>

#define	SIZE_1K		SZ_1K
#define	SIZE_2K		SZ_2K
#define	SIZE_4K		SZ_4K
#define	SIZE_8K		SZ_8K
#define	SIZE_16K	SZ_16K
#define	SIZE_32K	SZ_32K
#define	SIZE_64K	SZ_64K
#define	SIZE_128K	SZ_128K
#define	SIZE_256K	SZ_256K
#define	SIZE_512K	SZ_512K
#define	SIZE_1M		SZ_1M
#define	SIZE_2M		SZ_2M
#define	SIZE_4M		SZ_4M
#define	SIZE_8M		SZ_8M
#define	SIZE_16M	SZ_16M
#define	SIZE_32M	SZ_32M
#define	SIZE_64M	SZ_64M
#define	SIZE_128M	SZ_128M
#define	SIZE_256M	SZ_256M
#define	SIZE_512M	SZ_512M
#define	SIZE_1G		SZ_1G
#define	SIZE_2G		SZ_2G

#define	READCMD0	0
#define	READCMD1	1
#define	READCMD2	0x50
#define	ERASECMD0	0x60
#define	ERASECMD1	0xd0
#define	PROGCMD0	0x80
#define	PROGCMD1	0x10
#define	QUERYCMD	0x70
#define	READIDCMD	0x90
#define	RSTCHIPCMD	0xff

#define	NF_OOB_SIZE			16
#define	NF_PAGE_SIZE		512
#define	NF_BLOCK_SIZE		SZ_16K
#define	NF_PG_SFT			9
#define	NF_BLK_SFT			14
#define	BLK_SFT_PG			5
#define	PAGES_PER_BLOCK		32

#define	MAX_NAND_FLASH_DEV	16
#define	MAX_NAND_FLASH_PART	16

#define	NF_NULL_BLK			((u16)-1)
#define	NF_SIZE_FULL		((u32)-1)

#define	OOB_ECCL0			0
#define	OOB_ECCL1			1
#define	OOB_ECCL2			2
#define	OOB_ECCH0			3
#define	OOB_ECCH1			6
#define	OOB_ECCH2			7
#define	OOB_BAD_MARK		5
#define	OOB_BLK_NOL			8
#define	OOB_BLK_NOH			9
#define	OOB_FMT_TAG			15

struct reg_partition {
	char *name;
	u32 size;
	u16 rsv_blks;
	u8 ro_flag;
	u8 disk_flag;
};

struct emu_disk_para {
	u32 sect_size;
	u32 sect_count;
	u32 blk_size;
	u32 ro;
};

struct nf_statics {
	u16 init_errs;
	u16 erase_errs;
	u16 write_errs;
	u16 ecc_fail;		//read errors
	u32 ecc_success;
};

struct nf_ctl_info;

struct nf_partition {
	char *name;
	u32 offset;
	u32 size;
	u16 *map;
//	u16 *erase_queue;
//	u16 *null_queue;
	u32 pg_chg_msk;
	u16 null_blk_idx;
	u16 last_phy_blk;
	u16 last_vir_blk;
	u16 bad_blks;
	u16 used_blks;
	u16 rsv_blks;
	u16 flags;
	u32 w_jiffies;
	u32 open_count;
	struct nf_ctl_info *parent;
	char *blk_buf;
	struct nf_statics statics;
	struct emu_disk_para emu_disk;
	void *disk;
};

struct nand_flash_itf {
	u32 data;
	u32 addr;
	u32 cmd;
	void (*nf_ce_ctl)(int ce);
	int (*nf_ready)(void);
	u16 addr_cycle;
} __attribute__((packed));

#define	NF_DRV_AUTO_MAJOR
#define	NF_DRV_MAJOR		119

#define	NF_PART_BLK_FMT		1
#define	NF_PART_BLK_DEV		2
#define	NF_PART_BLK_RO		0x80
#define	NF_PART_BLK_DISK	0x100

#define	DEVICE_NAME			"nfblock"
#define	NF_DRV_VER			"1.00"
#define	NF_DRV_FMT_TAG		"Formatted For NAND FLASH Driver"	//must be less than 32

#define	IOC_NF				0x89				//usually defined in asm/hardware/ioc.h
#define	IOC_NF_GET_BLKS		_IOR(IOC_NF, 0, 4)	//read total blocks	_IO,_IOR,_IOW,_IOWR defined in asm/ioctl.h
#define	IOC_NF_FMT_BLK		_IOWR(IOC_NF, 0, 4)	//format block, return status
#define	IOC_NF_RST_FMT		_IO(IOC_NF, 0)		//set NF_PART_BLK_FMT and initializ map table

int register_nand_flash_itf(struct nand_flash_itf *, 
							u32, struct reg_partition *, 
							void(*open_release)(int), 
							struct device *dev);
int unregister_nand_flash_itf(struct nand_flash_itf *);

struct attach_bdops {
	int (*open)(void *);//(struct inode *, struct file *);
	int (*release)(void *);//(struct inode *, struct file *);
	int (*ioctl)(void *, unsigned int, unsigned long);//(struct inode *, struct file *,	unsigned int, unsigned long);
	ssize_t (*read)(void *, char *, unsigned long);
	ssize_t (*write)(void *, char *, unsigned long);
	int (*do_transfer)(void *, const struct request *);
	int (*proc_rd_entry)(char *);
};

#endif
