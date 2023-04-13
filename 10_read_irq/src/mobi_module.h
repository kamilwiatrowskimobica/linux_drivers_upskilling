#ifndef _MOBI_MODULE_H_
#define _MOBI_MODULE_H_

#include <linux/cdev.h>
#include <linux/mutex.h>

struct mob_dev {
    size_t id;
    struct cdev cdev;
};


#endif /* _MOBI_MODULE_H_ */