#
# Makefile for the kernel mmc device drivers.
#

#
# Core
#
obj-$(CONFIG_MMC)		+= mmc_core.o

#
# Media drivers
#
obj-$(CONFIG_MMC_BLOCK)		+= mmc_block.o

#
# Host drivers
#
obj-$(CONFIG_MMC_ARMMMCI)	+= mmci.o
obj-$(CONFIG_MMC_PXA)		+= pxamci.o
obj-$(CONFIG_MMC_IMX)		+= imxmmc.o
obj-$(CONFIG_MMC_S3C2410)	+= s3c2410_sdi.o
obj-$(CONFIG_MMC_NF_EMU)	+= mmcnfdrv.o
obj-$(CONFIG_MMC_NF_PXA255)	+= mmcnf_pxa255.o

mmc_core-y := mmc.o mmc_queue.o mmc_sysfs.o
mmcnfdrv-y := nfdrv.o mmc_emu.o
