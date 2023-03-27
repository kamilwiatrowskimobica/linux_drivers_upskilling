#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "woab.h"

MODULE_LICENSE ("GPL");

#ifdef DEBUG
        #define WDEBUG(fmt, args...) printk(KERN_DEBUG "WOABBUS: " fmt, ##args)
#else
        #define WDEBUG(fmt, args...)
#endif

#ifdef TRACE
        #define WTRACE() printk(KERN_INFO "WOABBUS: %s %d\n", __FUNCTION__, __LINE__)
#else
        #define WTRACE()
#endif

/*
 * Forward declaration of bus
 */
extern struct bus_type woab_bus_type;


/* =============================================== *
 *            Device related functions             *
 * =============================================== */
 
static int woab_dev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
        WTRACE();

        return add_uevent_var(env, "MODALIAS=woab:%s", dev_name(dev));
}

/*
 * Function called when device is removed from bus to free resources
 */
static void woab_dev_release(struct device *dev)
{
        struct woab_device *woab_dev = container_of(dev, struct woab_device, dev);

        WTRACE();

        kfree(woab_dev->type);
        kfree(woab_dev);
}

/*
 * Function to handle /sys/devices/<dev_name>/type file
 */
static ssize_t
woab_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct woab_device *woab_dev = container_of(dev, struct woab_device, dev);

        WTRACE();

        return sprintf(buf, "%s\n", woab_dev->type);
}

/*
 * Macro definiing handler description of /sys/devices/<dev_name>/type file
 */
DEVICE_ATTR(type, 0444, woab_type_show, NULL);

/*
 * Function to handle /sys/devices/<dev_name>/version file
 */
static ssize_t
woab_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct woab_device *woab_dev = container_of(dev, struct woab_device, dev);

        WTRACE();

        return sprintf(buf, "%d\n", woab_dev->version);
}

/*
 * Macro definiing handler description of /sys/devices/<dev_name>/version file
 */
DEVICE_ATTR(version, 0444, woab_version_show, NULL);

/*
 * Struct which join /sys/devices/<dev_name>/<filename> handlers together
 */
static struct attribute *woab_dev_attrs[] = {
        &dev_attr_type.attr,
        &dev_attr_version.attr,
        NULL
};

/*
 * Helper macro which takes woab_dev_attrs and defines correctly 
 * woab_dev_group and woab_dev_groups variables
 */
ATTRIBUTE_GROUPS(woab_dev);

/*
 * Final structure describing device with associated file handlers
 */
struct device_type woab_device_type = {
        .groups  = woab_dev_groups,
        .uevent  = woab_dev_uevent,
        .release = woab_dev_release,
};

/* =============================================== *
 *            Bus related functions                *
 * =============================================== */

static int woab_bus_match(struct device *dev, struct device_driver *driver)
{        
        struct woab_device *woab_dev = container_of(dev, struct woab_device, dev);
        struct woab_driver *woab_drv = container_of(driver, struct woab_driver, driver);
        struct kobject *kobj = &woab_dev->dev.kobj;
        
        WTRACE();
        WDEBUG("Try to match device %s type %s with driver type %s\n", kobj->name, woab_dev->type, woab_drv->type);

        if (!strcmp(woab_dev->type, woab_drv->type)) {
                WDEBUG("Succesfully matched device %s type %s with driver type %s\n", kobj->name, woab_dev->type, woab_drv->type);
                return 1;
        }

        return 0;
}

static int woab_bus_probe(struct device *dev)
{
        struct woab_device *woab_dev = container_of(dev, struct woab_device, dev);
        struct woab_driver *woab_drv = container_of(dev->driver, struct woab_driver, driver);
        struct kobject *kobj = &woab_dev->dev.kobj;

        WTRACE();

        WDEBUG("Try to probe device %s type %s with driver type %s\n", kobj->name, woab_dev->type, woab_drv->type);

        return woab_drv->probe(woab_dev);
}

static void woab_bus_remove(struct device *dev)
{
        struct woab_device *woab_dev = container_of(dev, struct woab_device, dev);
        struct woab_driver *woab_drv = container_of(dev->driver, struct woab_driver, driver);
        struct kobject *kobj = &woab_dev->dev.kobj;

        WTRACE();

        WDEBUG("Removing device %s\n", kobj->name);

        woab_drv->remove(woab_dev);
}

/*
 * Function to add device to bus
 */
static int woab_bus_add_device(const char *name, const char *type, int version)
{
        struct woab_device *woab_dev;

        WTRACE();

        woab_dev = kzalloc(sizeof(*woab_dev), GFP_KERNEL);
        if (!woab_dev)
                return -ENOMEM;

        woab_dev->type = kstrdup(type, GFP_KERNEL);
        woab_dev->version = version;

        woab_dev->dev.bus = &woab_bus_type;
        woab_dev->dev.type = &woab_device_type;
        woab_dev->dev.parent = NULL;

        dev_set_name(&woab_dev->dev, "%s", name);

        WDEBUG("Added device %s type %s version %d\n", name, woab_dev->type, woab_dev->version);

        return device_register(&woab_dev->dev);
}

/*
 * Function to remove device from bus
 */
static int woab_bus_del_device(const char *name)
{
        struct device *dev;

        WTRACE();

        dev = bus_find_device_by_name(&woab_bus_type, NULL, name);
        if (!dev)
                return -EINVAL;

        device_unregister(dev);
        put_device(dev);

        WDEBUG("Removed device %s\n", name);

        return 0;
}

/*
 * Function to remove device from bus
 */
static int woab_bus_del_device_by_dev(struct device *dev, void *ptr)
{
        WTRACE();
                
        woab_bus_del_device(dev_name(dev));
        
        return 0;
}

/*
 * Function to add device to bus via /sys/bus/woab/add file
 * it simply parses input and call correct function
 */
static ssize_t
woab_add_store(struct bus_type *bt, const char *buf, size_t count)
{
        char type[32], name[32];
        int version;
        int ret;

        WTRACE();

        // hardcoded according to format expected by woab device
        ret = sscanf(buf, "%31s %31s %d", name, type, &version);
        if (ret != 3)
                return -EINVAL;

        return woab_bus_add_device(name, type, version) ? : count;
}

/*
 * Handler description of /sys/bus/woab/add file
 */
struct bus_attribute bus_attr_add = {
        .attr.name = "add", 
        .attr.mode = 0644,
        .show = NULL,
        .store = woab_add_store,
};

/*
 * Function to remove device from bus via /sys/bus/woab/del
 * it simply parses input and call correct function
 */
static ssize_t
woab_del_store(struct bus_type *bt, const char *buf, size_t count)
{
        char name[32];

        WTRACE();

        if (sscanf(buf, "%s", name) != 1)
                return -EINVAL;

        return woab_bus_del_device(name) ? 0 : count;
}

/*
 * Handler description of /sys/bus/woab/del file
 */
struct bus_attribute bus_attr_del = {
        .attr.name = "del", 
        .attr.mode = 0644,
        .show = NULL,
        .store = woab_del_store,
};

/*
 * Struct which join /sys/bus/woab/<filename> handlers together
 */
static struct attribute *woab_bus_attrs[] = {
        &bus_attr_add.attr,
        &bus_attr_del.attr,
        NULL
};

/*
 * Helper macro which takes woab_bus_attrs and defines correctly 
 * woab_bus_group and woab_bus_groups variables
 */
ATTRIBUTE_GROUPS(woab_bus);

/*
 * Final structure describing bus with associated file handlers
 */
struct bus_type woab_bus_type = {
        .name   = "woab",
        .match  = woab_bus_match,
        .probe  = woab_bus_probe,
        .remove  = woab_bus_remove,
        .bus_groups = woab_bus_groups,
};

/* =============================================== *
 *            Driver related functions             *
 * =============================================== */
 
int woab_register_driver(struct woab_driver *drv)
{
        int ret;

        WTRACE();

        drv->driver.bus = &woab_bus_type;
        ret = driver_register(&drv->driver);
        if (ret)
                return ret;

        return 0;
}
EXPORT_SYMBOL(woab_register_driver);

void woab_unregister_driver(struct woab_driver *drv)
{
        WTRACE();

        driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(woab_unregister_driver);

/* =============================================== *
 *           BUS module related functions          *
 * =============================================== */
static int __init woab_bus_init (void)
{
        int ret;

        WTRACE();

        ret = bus_register(&woab_bus_type);
        if (ret < 0) {
                pr_err("Unable to register bus\n");
                return ret;
        }

        return 0;
}

static void woab_bus_exit (void)
{
        WTRACE();
        
	// remove remaining devices
        bus_for_each_dev(&woab_bus_type, NULL, NULL, woab_bus_del_device_by_dev);
        
        bus_unregister(&woab_bus_type);
}

module_init (woab_bus_init);
module_exit (woab_bus_exit);