#ifndef _MOBCHAR_H_
#define _MOBCHAR_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include "mobchar_debug.h"

#define DEVICE_NAME     "mobchar"
#define CLASS_NAME      "mob"
#define DEVICE_MAX_SIZE (4)
#define BUFFER_MAX_SIZE (256)
#define COMPLETION_TIMEOUT 1000

#define logk(a, ...) printk(a PRINT_PREFIX __VA_ARGS__);

struct mobchar_dev {
	char *p_data;
	struct cdev cdev;
	struct completion comp;
};

#endif //_MOBCHAR_H_