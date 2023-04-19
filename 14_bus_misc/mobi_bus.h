#ifndef _MOBI_BUS_H_
#define _MOBI_BUS_H_

#define MOB_FUNCTION_DEBUG()   (pr_info("Mobi: %s\n", __FUNCTION__))

#define BUFFER_SIZE 1024

//extern struct device mob_bus;
extern struct bus_type mob_bus_type;

/*
 * The MOB driver type.
 */

struct mob_device;

struct mob_driver {
	const char *type;
	struct device_driver driver;

    int (*probe)(struct mob_device *dev);
	void (*remove)(struct mob_device *dev);
};

/*
 * A device type for things "plugged" into the MOB bus.
 */
struct mob_device {
	char *type;
	int version;
	char buffer[BUFFER_SIZE];
	struct device dev;
};

extern int mob_register_driver(struct mob_driver *);
extern void mob_unregister_driver(struct mob_driver *);

#endif /* _MOBI_BUS_H_ */