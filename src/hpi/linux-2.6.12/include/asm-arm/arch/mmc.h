#ifndef ASMARM_ARCH_MMC_H
#define ASMARM_ARCH_MMC_H

#include <linux/mmc/protocol.h>
#include <linux/interrupt.h>

struct device;
struct mmc_host;

struct s3c24xx_mci_platform_data {
	unsigned int ocr_mask;			/* available voltages */
	unsigned long detect_delay;		/* delay in jiffies before detecting cards after interrupt */
	int (*init)(struct device *, irqreturn_t (*)(int, void *, struct pt_regs *), void *);
	int (*get_ro)(struct device *);
	void (*setpower)(struct device *, unsigned int);
	void (*exit)(struct device *, void *);
};



struct s3c24xx_mmc_platdata {
	unsigned int	gpio_detect;
	unsigned int	gpio_wprotect;
	unsigned int	detect_polarity;
	unsigned int	wprotect_polarity;

	unsigned long	f_max;
	unsigned long	ocr_avail;

	void		(*set_power)(unsigned int to);
};


extern void s3c24xx_set_mci_info(struct s3c24xx_mci_platform_data *info);

#endif
