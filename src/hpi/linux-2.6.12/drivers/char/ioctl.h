#include <linux/ioctl.h>  //for cmd

#define SCULL_IOC_MAGIC  'z'

#define INT_GET    _IO(SCULL_IOC_MAGIC, 0)

#define INT_IN _IO(SCULL_IOC_MAGIC,  1)

#define SO_DISPULLUP    _IO(SCULL_IOC_MAGIC,  2)

#define SO_PULLUP _IO(SCULL_IOC_MAGIC,   3)

#define SO_GET    _IO(SCULL_IOC_MAGIC,   4)

#define SO_IN _IO(SCULL_IOC_MAGIC,  5)

#define SCK_L    _IO(SCULL_IOC_MAGIC,  6)

#define SCK_H    _IO(SCULL_IOC_MAGIC,   7)

#define SCK_OUT  _IO(SCULL_IOC_MAGIC,   8)

#define SI_L     _IO(SCULL_IOC_MAGIC, 9)

#define SI_H     _IO(SCULL_IOC_MAGIC,10)

#define SI_OUT   _IO(SCULL_IOC_MAGIC,  11)

#define CS_L     _IO(SCULL_IOC_MAGIC,  12)

#define CS_H     _IO(SCULL_IOC_MAGIC,  13)

#define CS_OUT   _IO(SCULL_IOC_MAGIC,  14)
