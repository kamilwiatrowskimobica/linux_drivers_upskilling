/**
 * @file    mysimplechardev_main.h
 * @author  Andrzej Dziarnik
 * @date    29.06.2023
 * @version 0.2
 * @brief  Simple character driver created for training purposes
*/
#ifndef _MYSIMPLECHARDEV_MAIN_H_
#define _MYSIMPLECHARDEV_MAIN_H_

#include <linux/cdev.h>

#define MSG_MAX_SIZE 256
#define MY_DEV_COUNT 3

struct mysimplechardev_dev {
	struct cdev cdev;
	char *data;
	size_t data_size;
};

extern struct mysimplechardev_dev *my_devices;

#endif /* _MYSIMPLECHARDEV_MAIN_H_ */