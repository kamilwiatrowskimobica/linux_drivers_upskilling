#ifndef _MOBI_IOCTL_H_
#define _MOBI_IOCTL_H_

#include <linux/ioctl.h>

#define MOBI_IOC_MAGIC 'M'

typedef struct mob_ioctl_data {
    size_t off;
    size_t len;
    void *data;
} mob_ioctl_data_t;


#define MOBI_MSG_LENGTH _IOR(MOBI_IOC_MAGIC, 0x00, mob_ioctl_data_t)
#define MOBI_MSG_SET    _IOW(MOBI_IOC_MAGIC, 0x01, mob_ioctl_data_t)
#define MOBI_MSG_GET    _IOR(MOBI_IOC_MAGIC, 0x02, mob_ioctl_data_t)

#endif /* _MOBI_IOCTL_H_ */