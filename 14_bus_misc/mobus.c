#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "mobus.h"


MODULE_AUTHOR ("rz");
MODULE_LICENSE ("GPL");
MODULE_DESCRIPTION ("mob bus module");

static int mobbus_match(struct device* dev, struct device_driver* driver);
static int mobbus_uevent(struct device* dev, struct kobj_uevent_env* env);
static void mobbus_dev_release(struct device* dev);

//sample device
static struct mob_device mobdev1 = {
	.name	= "mob1",
};

//mob bus parent device
struct device mobbus_dev = {
	.release  = mobbus_dev_release
};

struct bus_type mobbus_type = {
	.name	= "mob",
	.match	= mobbus_match,
	.uevent = mobbus_uevent,
};

// match devices to drivers
static int mobbus_match(struct device *dev, struct device_driver *driver){
	struct mob_device *mob_dev = container_of(dev, struct mob_device, dev);
	struct mob_driver *mob_drv = container_of(driver, struct mob_driver, driver);
	
	printk(KERN_INFO "Mobbus match: Device type: %s Driver type %s\n", mob_dev->type, mob_drv->type);

	return !strcmp(mob_dev->type, mob_drv->type);
}

// add environment variables for hotplug
static int mobbus_uevent(struct device *dev, struct kobj_uevent_env *env){
	printk(KERN_INFO "MobBus: Uevent");
	return add_uevent_var(env, "DEV_NAME=%s", dev_name(dev));
}

//parent device remove
static void mobbus_dev_release(struct device *dev){
	struct mob_device *mob_dev = container_of(dev, struct mob_device, dev);
	printk(KERN_DEBUG "mobbus parent dev release\n");

	kfree(mob_dev);
}
//device

int mob_register_device(struct mob_device *mydev)
{
	printk(KERN_DEBUG "mobus: registering device\n");
	mydev->dev.bus = &mobbus_type;
	mydev->dev.parent = &mobbus_dev;
	mydev->dev.release = mobbus_dev_release;
	mydev->type = kstrdup("mob", GFP_KERNEL);
	dev_set_name(&mydev->dev, mydev->name);

 	return device_register(&mydev->dev);
}

void mob_unregister_device(struct mob_device *mydev)
{
	printk(KERN_DEBUG "mobus: unregistering device\n");
 	device_unregister(&mydev->dev);
}

/* export register/unregister device functions */
EXPORT_SYMBOL(mob_register_device);
EXPORT_SYMBOL(mob_unregister_device);

//driver
int mob_register_driver(struct mob_driver* driver)
{
	int value;
	printk(KERN_DEBUG "Mobus: Register driver");
	driver->driver.bus = &mobbus_type;
	value = driver_register(&driver->driver);
	if(value){
		printk(KERN_DEBUG "Mobus: register driver failed");
		return value;
	}

	printk(KERN_DEBUG "Mobus: driver registered");

	return value;
}

void mob_unregister_driver(struct mob_driver* driver)
{
	printk(KERN_DEBUG "Mobus: Unregister driver");
	driver_unregister(&driver->driver);
}

EXPORT_SYMBOL(mob_register_driver);
EXPORT_SYMBOL(mob_unregister_driver);

/////
static int __init mobbus_init (void){
	int ret;

	// add to sysfs under /sys/bus
	ret = bus_register(&mobbus_type);
	if (ret) {
		printk(KERN_ERR "Unable to register bus, failure was %d\n",ret);
		return ret;
	}
	printk(KERN_INFO "mobus: registered bus\n");
	
	//register parent device
	dev_set_name(&mobbus_dev, "mobb0");
	
	ret = device_register(&mobbus_dev);
	if (ret) {
		printk(KERN_ERR "mobus: Unable to register mobb0, failure was %d\n",ret);
		bus_unregister(&mobbus_type);
		return ret;
	}
	
	//add another device
	ret = mob_register_device(&mobdev1);
	if (ret) {
		printk(KERN_ERR "mobus: Unable to register mob1, failure was %d\n",ret);
		device_unregister(&mobbus_dev);
		bus_unregister(&mobbus_type);
		return ret;
	}

	return 0;
}

static void mobbus_exit(void){
	device_unregister(&mobdev1.dev);
	device_unregister(&mobbus_dev);
	bus_unregister(&mobbus_type);
}

module_init(mobbus_init);
module_exit(mobbus_exit);
