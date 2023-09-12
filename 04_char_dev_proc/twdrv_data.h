#ifndef _TWDRV_DATA_H_
#define _TWDRV_DATA_H_

#include <linux/cdev.h>

struct twdrv_dev {
	char *p_data;
	struct cdev cdev;
};


int  twdrv_data_init(struct twdrv_dev **devices, struct file_operations *fops);
void twdrv_data_release(struct twdrv_dev *devices);

#endif /* _TWDRV_DATA_H_ */
