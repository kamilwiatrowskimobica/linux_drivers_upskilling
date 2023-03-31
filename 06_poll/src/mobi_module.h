#ifndef _MOBI_MODULE_H_
#define _MOBI_MODULE_H_

#include <linux/cdev.h>
#include <linux/mutex.h>

#define MOB_MESSAGE_LEN 256
#define MOB_DEFAULT_DEV_COUNT 1

struct mob_dev {
    size_t id;
    size_t msg_size;
    struct mutex lock;
    struct cdev cdev;
    uint ioctl_variable;
    wait_queue_head_t outq;
    char *msg;
};


#endif /* _MOBI_MODULE_H_ */