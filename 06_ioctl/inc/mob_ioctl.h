#ifndef _MOB_IOCTL_H_
#define _MOB_IOCTL_H_

#include <linux/ioctl.h>

#define MOB_IOCTL_MAGIC 'H'

#define MOB_DEVICE_HELLO _IO(MOB_IOCTL_MAGIC, 0)
#define MOB_DEVICE_READ _IOR(MOB_IOCTL_MAGIC, 1, int)
#define MOB_DEVICE_WRITE _IOW(MOB_IOCTL_MAGIC, 2, int)

#define MOB_DEVICE_MAX (2)

#endif //_MOB_IOCTL_H_