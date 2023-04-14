#ifndef _MOBI_MODULE_H_
#define _MOBI_MODULE_H_

#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/types.h>

struct mob_data {
	struct list_head list;
	char *p_data;
	size_t size;
};

struct mob_dev {
    size_t id;
    struct mutex lock;
    struct cdev cdev;
    struct list_head items;
    u32 items_counter;
};


#endif /* _MOBI_MODULE_H_ */