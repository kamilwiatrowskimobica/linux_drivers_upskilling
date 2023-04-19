#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "mobi_bus.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simple Linux mob-bus module");
MODULE_VERSION("0.1");


/* =============================================== *
 *            Device related functions             *
 * =============================================== */

/*
 * Respond to udev events.
 */
static int mob_dev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    MOB_FUNCTION_DEBUG();
    return add_uevent_var(env, "MODALIAS=mobi:%s", dev_name(dev));
}

/*
 * Function called when device is removed from bus to free resources
 */
static void mob_dev_release(struct device *dev)
{
    struct mob_device *mob_dev = container_of(dev, struct mob_device, dev);

    MOB_FUNCTION_DEBUG();

    kfree(mob_dev->type);
    kfree(mob_dev);
}

/*
 * Function to handle /sys/devices/<dev_name>/type file
 */
static ssize_t
mob_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct mob_device *mob_dev = container_of(dev, struct mob_device, dev);

    MOB_FUNCTION_DEBUG();

    return sprintf(buf, "%s\n", mob_dev->type);
}

/*
 * Macro definiing handler description of /sys/devices/<dev_name>/type file
 */
DEVICE_ATTR(type, 0444, mob_type_show, NULL);

/*
 * Function to handle /sys/devices/<dev_name>/version file
 */
static ssize_t
mob_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct mob_device *mob_dev = container_of(dev, struct mob_device, dev);

    MOB_FUNCTION_DEBUG();

    return sprintf(buf, "%d\n", mob_dev->version);
}

/*
 * Macro definiing handler description of /sys/devices/<dev_name>/version file
 */
DEVICE_ATTR(version, 0444, mob_version_show, NULL);

/*
 * Struct which join /sys/devices/<dev_name>/<filename> handlers together
 */
static struct attribute *mob_dev_attrs[] = {
        &dev_attr_type.attr,
        &dev_attr_version.attr,
        NULL
};

/*
 * Helper macro which takes mob_dev_attrs and defines correctly
 * mob_dev_group and mob_dev_groups variables
 */
ATTRIBUTE_GROUPS(mob_dev);

/*
 * Final structure describing device with associated file handlers
 */
struct device_type mob_device_type = {
        .groups  = mob_dev_groups,
        .uevent  = mob_dev_uevent,
        .release = mob_dev_release,
};


/* =============================================== *
 *            Bus related functions                *
 * =============================================== */

static int mob_bus_match(struct device *dev, struct device_driver *driver)
{
    struct mob_device *mob_dev = container_of(dev, struct mob_device, dev);
    struct mob_driver *mob_drv = container_of(driver, struct mob_driver, driver);
    struct kobject *kobj = &mob_dev->dev.kobj;

    MOB_FUNCTION_DEBUG();
    pr_info("Try to match device %s type %s with driver type %s\n", kobj->name, mob_dev->type, mob_drv->type);

    if (!strcmp(mob_dev->type, mob_drv->type)) {
            pr_info("Succesfully matched device %s type %s with driver type %s\n", kobj->name, mob_dev->type, mob_drv->type);
            return 1;
    }

    return 0;
}

static int mob_bus_probe(struct device *dev)
{
    struct mob_device *mob_dev = container_of(dev, struct mob_device, dev);
    struct mob_driver *mob_drv = container_of(dev->driver, struct mob_driver, driver);
    struct kobject *kobj = &mob_dev->dev.kobj;

    MOB_FUNCTION_DEBUG();

    pr_info("Try to probe device %s type %s with driver type %s\n", kobj->name, mob_dev->type, mob_drv->type);

    return mob_drv->probe(mob_dev);
}

static int mob_bus_remove(struct device *dev)
{
    struct mob_device *mob_dev = container_of(dev, struct mob_device, dev);
    struct mob_driver *mob_drv = container_of(dev->driver, struct mob_driver, driver);
    struct kobject *kobj = &mob_dev->dev.kobj;

    MOB_FUNCTION_DEBUG();

    pr_info("Removing device %s\n", kobj->name);

    mob_drv->remove(mob_dev);

    return 0;
}

/*
 * Function to add device to bus
 */
static int mob_bus_add_device(const char *name, const char *type, int version)
{
    struct mob_device *mob_dev;

    MOB_FUNCTION_DEBUG();

    mob_dev = kzalloc(sizeof(*mob_dev), GFP_KERNEL);
    if (!mob_dev)
            return -ENOMEM;

    mob_dev->type = kstrdup(type, GFP_KERNEL);
    mob_dev->version = version;

    mob_dev->dev.bus = &mob_bus_type;
    mob_dev->dev.type = &mob_device_type;
    mob_dev->dev.parent = NULL;

    dev_set_name(&mob_dev->dev, "%s", name);

    pr_info("Added device %s type %s version %d\n", name, mob_dev->type, mob_dev->version);

    return device_register(&mob_dev->dev);
}

/*
 * Function to remove device from bus
 */
static int mob_bus_del_device(const char *name)
{
    struct device *dev;

    MOB_FUNCTION_DEBUG();

    dev = bus_find_device_by_name(&mob_bus_type, NULL, name);
    if (!dev)
            return -EINVAL;

    device_unregister(dev);
    put_device(dev);

    pr_info("Removed device %s\n", name);

    return 0;
}

/*
 * Function to remove device from bus
 */
static int mob_bus_del_device_by_dev(struct device *dev, void *ptr)
{
    MOB_FUNCTION_DEBUG();

    mob_bus_del_device(dev_name(dev));

    return 0;
}

/*
 * Function to add device to bus via /sys/bus/mob/add file
 * it simply parses input and call correct function
 */
static ssize_t
mob_add_store(struct bus_type *bt, const char *buf, size_t count)
{
    char type[32], name[32];
    int version;
    int ret;

    MOB_FUNCTION_DEBUG();

    // hardcoded according to format expected by mob device
    ret = sscanf(buf, "%31s %31s %d", name, type, &version);
    if (ret != 3)
            return -EINVAL;

    return mob_bus_add_device(name, type, version) ? : count;
}

/*
 * Handler description of /sys/bus/mob/add file
 *
 * sample usage: # sudo echo "DEVICE MISC 1" > ./add
 */
struct bus_attribute bus_attr_add = {
        .attr.name = "add_driver",
        .attr.mode = 0644,
        .show = NULL,
        .store = mob_add_store,
};

/*
 * Function to remove device from bus via /sys/bus/mob/del
 * it simply parses input and call correct function
 */
static ssize_t
mob_del_store(struct bus_type *bt, const char *buf, size_t count)
{
    char name[32];

    MOB_FUNCTION_DEBUG();

    if (sscanf(buf, "%s", name) != 1)
            return -EINVAL;

    return mob_bus_del_device(name) ? 0 : count;
}

/*
 * Handler description of /sys/bus/mob/del file
 */
struct bus_attribute bus_attr_del = {
        .attr.name = "del_driver",
        .attr.mode = 0644,
        .show = NULL,
        .store = mob_del_store,
};

/*
 * Struct which join /sys/bus/mob/<filename> handlers together
 */
static struct attribute *mob_bus_attrs[] = {
        &bus_attr_add.attr,
        &bus_attr_del.attr,
        NULL
};

/*
 * Helper macro which takes mob_bus_attrs and defines correctly
 * mob_bus_group and mob_bus_groups variables
 */
ATTRIBUTE_GROUPS(mob_bus);

/*
 * Final structure describing bus with associated file handlers
 */
struct bus_type mob_bus_type = {
        .name   = "mobi",
        .match  = mob_bus_match,
        .probe  = mob_bus_probe,
        .remove  = mob_bus_remove,
        .bus_groups = mob_bus_groups,
};

/* =============================================== *
 *            Driver related functions             *
 * =============================================== */

int mob_register_driver(struct mob_driver *drv)
{
    int ret;

    MOB_FUNCTION_DEBUG();

    drv->driver.bus = &mob_bus_type;
    ret = driver_register(&drv->driver);
    if (ret)
            return ret;

    return 0;
}
EXPORT_SYMBOL(mob_register_driver);

void mob_unregister_driver(struct mob_driver *drv)
{
    MOB_FUNCTION_DEBUG();

    driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(mob_unregister_driver);

/* =============================================== *
 *           BUS module related functions          *
 * =============================================== */
static int __init mob_bus_init (void)
{
    int ret;

    MOB_FUNCTION_DEBUG();

    ret = bus_register(&mob_bus_type);
    if (ret < 0) {
            pr_err("Unable to register bus\n");
            return ret;
    }

    return 0;
}

static void mob_bus_exit (void)
{
    MOB_FUNCTION_DEBUG();

    // remove remaining devices
    bus_for_each_dev(&mob_bus_type, NULL, NULL, mob_bus_del_device_by_dev);

    bus_unregister(&mob_bus_type);
}

module_init (mob_bus_init);
module_exit (mob_bus_exit);