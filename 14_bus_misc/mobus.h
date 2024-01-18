#ifndef _MOBUS_H
#define _MOBUS_H

#include <linux/device.h>

struct mob_device;

struct mob_driver {
	const char *type;
	int (*probe)(struct mob_device *dev);
	void (*remove)(struct mob_device *dev);
	struct device_driver driver;
};

struct mob_device {
	char* name;
	const char *type;
	struct device dev;
	struct mob_driver *driver;
};

int mob_register_driver(struct mob_driver *drv);
void mob_unregister_driver(struct mob_driver *drv);

#endif
