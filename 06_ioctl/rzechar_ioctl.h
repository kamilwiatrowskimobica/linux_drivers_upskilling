#ifndef RZECHAR_IOCTL
#define RZECHAR_IOCTL

#include <linux/ioctl.h>

struct iocdata
{
    unsigned int  size;
    unsigned int offset;
    #ifdef __KERNEL__
    unsigned char __user *data;
    #else
    unsigned char *data;
    #endif
};

#define RZECHAR_IOC_MAGIC   'R'
#define RZECHAR_IOC_HELLO   _IO(RZECHAR_IOC_MAGIC, 0)
#define RZECHAR_IOC_RESET   _IO(RZECHAR_IOC_MAGIC, 1)
#define RZECHAR_IOC_READ    _IOR(RZECHAR_IOC_MAGIC, 2, struct iocdata)

#endif //RZECHAR_IOCTL