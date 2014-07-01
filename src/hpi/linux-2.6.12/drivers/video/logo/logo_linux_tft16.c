#include <linux/linux_logo.h>

#include "tft16_logo.c"

struct linux_logo logo_linux_tft16 __initdata = {
    .type	= LINUX_LOGO_TFT16,
    .width	= LOGO_WIDTH,
    .height	= LOGO_HEIGHT,
    .clutsize	= 0,
    .clut	= (void *)0,
    .data	= logo_linux_tft16_data
};

