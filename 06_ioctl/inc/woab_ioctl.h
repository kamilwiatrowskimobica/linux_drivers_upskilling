#ifndef _WOAB_IOCTL_H_
#define _WOAB_IOCTL_H_

#include <linux/ioctl.h>

#define WOAB_IOCTL_IOC_MAGIC 'w'

#define WOAB_DEVICE_HELLO _IO(WOAB_IOCTL_IOC_MAGIC, 0)
#define WOAB_DEVICE_READ_SYNC_MODE _IOR(WOAB_IOCTL_IOC_MAGIC, 1, int)
#define WOAB_DEVICE_WRITE_SYNC_MODE _IOW(WOAB_IOCTL_IOC_MAGIC, 2, int)

#define WOAB_IOCTL_IOC_MAXNR (2)

#endif /* _WOAB_IOCTL_H_ */
