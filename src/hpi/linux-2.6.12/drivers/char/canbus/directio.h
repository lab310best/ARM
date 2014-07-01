/*  leddrv.h - the header file with the ioctl definitions.
 *
 *  The declarations here have to be in a header file, because
 *  they need to be known both to the kernel module
 *  (in leddrv.c) and the process calling ioctl (ioctl.c)
 */

#ifndef LED_DRV_H
#define LED_DRV_H

#include <linux/ioctl.h>


/* The major device number. We can't rely on dynamic 
 * registration any more, because ioctls need to know 
 * it. */
#define MAJOR_NUMM 199


/* Set the message of the device driver */
//#define IOCTL_SET_MSG _IOR(MAJOR_NUMM, 0, int*)
/* _IOR means that we're creating an ioctl command 
 * number for passing information from a user process
 * to the kernel module. 
 *
 * The first arguments, MAJOR_NUM, is the major device 
 * number we're using.
 *
 * The second argument is the number of the command 
 * (there could be several with different meanings).
 *
 * The third argument is the type we want to get from 
 * the process to the kernel.
 */

/* Get the message of the device driver */
//#define IOCTL_GET_MSG _IOR(MAJOR_NUMM, 1, int*)
 /* This IOCTL is used for output, to get the message 
  * of the device driver. However, we still need the 
  * buffer to place the message in to be input, 
  * as it is allocated by the process.
  */

/* The name of the device file */
#define DEVICE_FILE_NAME "directio"

/* port data structure */
/* command received */
#define CMD_PORT      0
#define CMD_IRQ       1

/* byte sex */
#define PORT_BYTE     0
#define PORT_HWORD    1
#define PORT_WORD     2
typedef struct port_data{
    int          cmd;         /* command */

    /* CMD_PORT */
    unsigned int startaddr;   /* start addr */
    int          nums;        /* ports nums(bytes) */
    int          opts;  /* 0 - bytes opt
                         * 1 - half word opt
                         * 2 - word opt */
    int          value;      /* port value */
}port_data;

/* port access functions */
int port_init(void);
int port_outb(unsigned int , unsigned char );
int port_outw(unsigned int , unsigned short );
int port_outl(unsigned int , unsigned int );
unsigned char port_inb(unsigned int );
unsigned short port_inw(unsigned int );
unsigned int port_inl(unsigned int );
void port_uninit(void);
#endif

