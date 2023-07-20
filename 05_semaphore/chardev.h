#ifndef _CHARDEV_H_
#define _CHARDEV_H_

#include <linux/cdev.h>
#include <linux/mutex.h>
#define DEVICE_MAX_SIZE (100)

struct char_dev {
	char *p_data;
	size_t data_size;
	struct cdev cdev;
	struct mutex lock;
};

#endif /* _CHARDEV_H_ */