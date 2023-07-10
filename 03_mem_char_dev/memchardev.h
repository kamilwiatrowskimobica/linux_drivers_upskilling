#ifndef _MEMCHARDEV_H_
#define _MEMCHARDEV_H_

#define DEVICE_MAX_SIZE (20)

struct char_dev {
	char *p_data;
	struct cdev cdev;
};

#endif /* _MEMCHARDEV_H_ */