#ifndef _CHARDEV_H_
#define _CHARDEV_H_

#include <linux/cdev.h>
#include <linux/mutex.h>

#define DEVICE_MAX_SIZE (1024 * 16 * 2)
struct char_dev {
	char *p_data;
	size_t data_size;
	struct cdev cdev;
};

#endif /* _CHARDEV_H_ */