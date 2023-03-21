#ifndef _MOBI_MODULE_H_
#define _MOBI_MODULE_H_

#include <linux/cdev.h>
#include <linux/mutex.h>

struct mob_dev {
    size_t id;
    size_t msg_size;
    struct mutex lock;
    struct completion sync_write;
    struct cdev cdev;
    char *msg;
};


#endif /* _MOBI_MODULE_H_ */