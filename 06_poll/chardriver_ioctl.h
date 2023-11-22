#ifndef CHARDEV_IOCTL_H
#define CHARDEV_IOCTL_H

#include <linux/ioctl.h>

struct ioctl_data
{
    size_t size;
    char* buffer;
};

#define DEV_IOC_MAGIC 'x'
#define DEV_IOC_RESET _IO(DEV_IOC_MAGIC, 0)
#define DEV_IOC_WRITE _IOW(DEV_IOC_MAGIC, 1, struct ioctl_data)
#define DEV_IOC_READ _IOR(DEV_IOC_MAGIC, 2, struct ioctl_data)
#define DEV_IOC_READ_NTH_BYTE _IOWR(DEV_IOC_MAGIC, 3, char)

#endif // CHARDEV_IOCTL_H