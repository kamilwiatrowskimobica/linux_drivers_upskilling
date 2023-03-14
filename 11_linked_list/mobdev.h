#ifndef _MOBDEV_H_
#define _MOBDEV_H_

#include <linux/cdev.h>
#include <linux/mutex.h>

#define DATA_MAX_SIZE 255

struct mob_data {
	struct list_head list;
	char *p_data;
	size_t size;
};

struct mob_dev {
	struct list_head items; // list for mob_data
	struct cdev c_dev;
	struct mutex lock;
};

#endif /* _MOBDEV_H_ */