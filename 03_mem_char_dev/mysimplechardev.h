/**
 * @file    mysimplechardev.h
 * @author  Andrzej Dziarnik
 * @date    29.06.2023
 * @version 0.2
 * @brief  Simple character driver created for training purposes
*/
#ifndef _MYSIMPLECHARDEV_H_
#define _MYSIMPLECHARDEV_H_

#include <linux/cdev.h>

#define MSG_MAX_SIZE 256

struct mysimplechardev_dev {
	struct cdev cdev;
	char *data;
	size_t data_size;
};

#endif /* _MYSIMPLECHARDEV_H_ */