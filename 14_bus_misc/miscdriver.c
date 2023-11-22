#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "mobbus.h"

static int __init misc_init(void);
static void __exit misc_exit(void);
static int misc_open(struct inode* inode, struct file* file);
static int misc_release(struct inode* inode, struct file* file);
static ssize_t misc_read(struct file* file, char __user* buffer, size_t size, loff_t* offset);
static ssize_t misc_write(struct file* file, const char __user* buffer, size_t size, loff_t* offset);
static int mob_misc_probe(struct mob_device* dev);
static void mob_misc_remove(struct mob_device* dev);

struct file_operations misc_fops =
{
    .owner = THIS_MODULE,
    .open = misc_open,
    .read = misc_read,
    .write = misc_write,
    .release = misc_release
};

struct mob_miscdevice
{
    struct miscdevice misc;
    struct mob_device* device; 
};

struct mob_driver mob_misc_driver =
{
    .type = "misc",
    .probe = mob_misc_probe,
    .remove = mob_misc_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = "mob_misc"
    }
};

static int __init misc_init(void)
{
    printk(KERN_INFO "MiscDriver: Init\n");

    if (mob_register_driver(&mob_misc_driver))
        printk(KERN_ERR "MiscDriver: Registration failed\n");

    return 0;
}

static void __exit misc_exit(void)
{
    mob_unregister_driver(&mob_misc_driver);
    printk(KERN_INFO "MiscDriver: Exit\n");
}

static int misc_open(struct inode* inode, struct file* file)
{
    printk(KERN_INFO "MiscDriver: Open");
    return 0;
}

static int misc_release(struct inode* inode, struct file* file)
{
    printk(KERN_INFO "MiscDriver: Release");
    return 0;
}

static ssize_t misc_read(struct file* file, char __user* buffer, size_t size, loff_t* offset)
{
    printk(KERN_INFO "MiscDriver: Read\n");
    return size;
}

static ssize_t misc_write(struct file* file, const char __user* buffer, size_t size, loff_t* offset)
{
    printk(KERN_INFO "MiscDriver: Write\n");
    return size;
}

static int mob_misc_probe(struct mob_device* dev)
{
    printk(KERN_INFO "MiscDriver: Probe\n");

    struct mob_miscdevice* miscdevice;
    char buffer[100];

    miscdevice = kzalloc(sizeof(struct mob_miscdevice), GFP_KERNEL);

    if (!miscdevice)
    {
        printk(KERN_INFO "MiscDriver: Memory allocation error");
        return -ENOMEM;
    }

    miscdevice->misc.minor = MISC_DYNAMIC_MINOR;
    snprintf(buffer, sizeof(buffer), "mob_misc_%d", 0);
    miscdevice->misc.name = "mob_misc";
    miscdevice->misc.parent = &dev->dev;
    miscdevice->misc.fops = &misc_fops;
    miscdevice->device = dev;

    dev_set_drvdata(&dev->dev, miscdevice);

    if (misc_register(&miscdevice->misc))
    {
        printk(KERN_ERR "MiscDriver: Misc device registration error");
        return -EFAULT;
    }

    return 0;
}

static void mob_misc_remove(struct mob_device* dev)
{
    printk(KERN_INFO "MiscDriver: Remove\n");

    struct mob_miscdevice* miscdevice = (struct mob_miscdevice*) dev_get_drvdata(&dev->dev);
    misc_deregister(&miscdevice->misc);
    kfree(miscdevice);
}

module_init(misc_init);
module_exit(misc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A B");
MODULE_DESCRIPTION("Misc driver");
MODULE_VERSION("1.0");
