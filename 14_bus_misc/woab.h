#ifndef __WOAB_H__
#define __WOAB_H__

//#define DEBUG
//#define TRACE

#define BUFFER_SIZE 1024

/*
 * Device description, it simply consist of string 'type' and version
 * for testing purposes
 */
struct woab_device {
	const char *type;
	int version;
	char buffer[BUFFER_SIZE];
	struct device dev;
};

struct woab_driver {
	const char *type;

	int (*probe)(struct woab_device *dev);
	void (*remove)(struct woab_device *dev);

	struct device_driver driver;
};

int woab_register_driver(struct woab_driver *drv);
void woab_unregister_driver(struct woab_driver *drv);

#endif /* __WOAB_H__ */