#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "mobbus.h"

static int mobbus_match(struct device* dev, struct device_driver* driver);
static int mobbus_probe(struct device* dev);
static void mobbus_remove(struct device* dev);
static int mobbus_uevent(struct device* dev, struct kobj_uevent_env* env);
static void mobbus_device_release(struct device* dev);
static int mob_register_device(void);
static void mob_unregister_device(const char* name);

struct bus_type bustype =
{
    .name = "mobbus",
    .match = mobbus_match,
    .probe = mobbus_probe,
    .remove = mobbus_remove,
    .uevent = mobbus_uevent
};

static struct device busdevice =
{
    .release = mobbus_device_release
};

static int mobbus_match(struct device* dev, struct device_driver* driver)
{
    printk(KERN_INFO "MobBus: Match");

    int result;
    struct mob_device* mob_device = container_of(dev, struct mob_device, dev);
    struct mob_driver* mob_driver = container_of(driver, struct mob_driver, driver);
    struct kobject* kobj = &mob_device->dev.kobj;

    printk(KERN_INFO "MobBus: Matching device %s type %s with driver type %s\n",
                      kobj->name, mob_device->type, mob_driver->type);
    
    // printk(dev_name(dev));
    // printk(driver->name);
    result = strncmp(dev_name(dev), driver->name, strlen(driver->name));
    
    return !result;
}

static int mobbus_probe(struct device* dev)
{
    printk(KERN_INFO "MobBus: Probe");

    struct mob_device* mob_device = container_of(dev, struct mob_device, dev);
    struct mob_driver* mob_driver = container_of(dev->driver, struct mob_driver, driver);
    struct kobject* kobj = &mob_device->dev.kobj;

    printk(KERN_INFO "MobBus: Probing %s", kobj->name);
    
    return mob_driver->probe(mob_device);
}

static void mobbus_remove(struct device* dev)
{
    printk(KERN_INFO "MobBus: Remove");

    struct mob_device* mob_device = container_of(dev, struct mob_device, dev);
    struct mob_driver* mob_driver = container_of(dev->driver, struct mob_driver, driver);
    struct kobject* kobj = &mob_device->dev.kobj;

    printk(KERN_INFO "MobBus: Removing %s", kobj->name);
    
    mob_driver->remove(mob_device);
}

static int mobbus_uevent(struct device* dev, struct kobj_uevent_env* env)
{
    printk(KERN_INFO "MobBus: Uevent");
    add_uevent_var(env, "DEV_NAME=%s", dev_name(dev));
    return 0;
}

static void mobbus_device_release(struct device* dev)
{
    printk(KERN_INFO "MobBus: Device release");
}

static int mob_register_device(void)
{
    printk(KERN_INFO "MobBus: Register device");

    struct mob_device* mob_device;
    char name[] = "mobdev0";

    mob_device = kzalloc(sizeof(struct mob_device), GFP_KERNEL);

    if (!mob_device)
    {
        printk(KERN_INFO "MobBus: Memory allocation error");
        return -ENOMEM;
    }

    mob_device->type = "misc";
    mob_device->dev.bus= &bustype;
    mob_device->dev.parent = &busdevice; // CHECK
    mob_device->dev.release = mobbus_device_release;
    dev_set_name(&mob_device->dev, "%s", name);

    printk(KERN_INFO "MobBus: Added device %s tpe %s\n", name, mob_device->type);

    return device_register(&mob_device->dev);
}

static void mob_unregister_device(const char* name)
{
    printk(KERN_INFO "MobBus: Unregister device");

    struct device* dev = bus_find_device_by_name(&bustype, NULL, name);

    if (!dev)
    {   
        printk(KERN_INFO "MobBus: Device %s not found", name);
        return;
    }

    device_unregister(dev);
    put_device(dev);

    printk(KERN_INFO "MobBus: Removed device %s", name);
}

////////////////////////////// DRIVER REGISTRATION ///////////////////////////////
int mob_register_driver(struct mob_driver* driver)
{
    printk(KERN_INFO "MobBus: Register driver");

    int value;
    driver->driver.bus = &bustype;
    value = driver_register(&driver->driver);

    if (value)
        printk(KERN_INFO "MobBus: A");

    printk(KERN_INFO "MobBus: B");

    return value;
}

void mob_unregister_driver(struct mob_driver* driver)
{
    printk(KERN_INFO "MobBus: Unregister driver");
    driver_unregister(&driver->driver);
}

EXPORT_SYMBOL(mob_register_driver);
EXPORT_SYMBOL(mob_unregister_driver);
///////////////////////////////////////////////////////////////////////////////////

static int __init mobbus_init(void)
{
    int result;

    result = bus_register(&bustype);

    if (result)
    {
        printk(KERN_INFO "MobBus: Bus registration error");
        return result;
    }

    dev_set_name(&busdevice, "mobbus_device");

    result = device_register(&busdevice);

    if (result)
    {
        printk(KERN_INFO "MobBus: Device registration error");
        bus_unregister(&bustype);
    }

    mob_register_device();

    return 0;
}

static void __exit mobbus_exit(void)
{
    printk(KERN_INFO "MobBus: Exit");
    device_unregister(&busdevice);
    bus_unregister(&bustype);
}

module_init(mobbus_init);
module_exit(mobbus_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A B");
MODULE_DESCRIPTION("Mobbus");
MODULE_VERSION("1.0");
