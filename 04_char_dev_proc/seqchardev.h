#ifndef _SEQCHARDEV_H_
#define _SEQCHARDEV_H_

#define DEVICE_MAX_SIZE (100)

struct char_dev {
	char *p_data;
	size_t data_size;
	struct cdev cdev;
};

#endif /* _SEQCHARDEV_H_ */