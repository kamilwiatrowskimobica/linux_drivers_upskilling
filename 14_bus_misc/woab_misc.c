#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include "woab.h"

MODULE_LICENSE("GPL");

#ifdef DEBUG
	#define WDEBUG(fmt, args...) printk(KERN_DEBUG "WOABMISC: " fmt, ##args)
#else
	#define WDEBUG(fmt, args...)
#endif

#ifdef TRACE
	#define WTRACE() printk(KERN_INFO "WOABMISC: %s %d\n", __FUNCTION__, __LINE__)
#else
	#define WTRACE()
#endif

struct woab_misc_device {
	struct miscdevice misc;
	struct woab_device *dev;
};

/* =============================================== *
 *          Misc device related functions          *
 * =============================================== */

static int woab_misc_open(struct inode *inode, struct file *file)
{
	WTRACE();
	return 0;
}

static int woab_misc_release(struct inode *inode, struct file *file)
{
	WTRACE();
	return 0;
}

static ssize_t woab_misc_read(struct file *file, char __user *user_buffer,
		   size_t size, loff_t *offset)
{
	struct woab_misc_device *woab_md = (struct woab_misc_device *)file->private_data;
	ssize_t len = min(sizeof(woab_md->dev->buffer) - (ssize_t)*offset, size);
	
	WTRACE();

	if (len <= 0)
		return 0;

	if (copy_to_user(user_buffer, woab_md->dev->buffer + *offset, len))
		return -EFAULT;

	*offset += len;
	return len;

}

static ssize_t woab_misc_write(struct file *file, const char __user *user_buffer,
		    size_t size, loff_t *offset)
{
	struct woab_misc_device *woab_md = (struct woab_misc_device *)file->private_data;
	ssize_t len = min(sizeof(woab_md->dev->buffer) - (ssize_t)*offset, size);

	WTRACE();	

	if (len <= 0)
		return 0;

	if (copy_from_user(woab_md->dev->buffer + *offset, user_buffer, len))
		return -EFAULT;

	*offset += len;
	return len;
}

struct file_operations woab_misc_fops = {
	.owner = THIS_MODULE,
	.open = woab_misc_open,
	.read = woab_misc_read,
	.write = woab_misc_write,
	.release = woab_misc_release,
};

/* =============================================== *
 *          Misc driver related functions          *
 * =============================================== */

int woab_misc_probe(struct woab_device *dev)
{
	static int woab_misc_count;
	struct woab_misc_device *woab_md;
	char buf[32];
	int ret;

	WTRACE();
	WDEBUG("Driver %s tries to probe device %s attached to bus %s with type %s and version %d\n", dev->dev.driver->name, dev->dev.kobj.name, dev->dev.bus->name, dev->type, dev->version);
	// Above debug call could be replaced by something more user friendly
	// dev_info(&dev->dev, "%s: %s %d\n", __func__, dev->type, dev->version);

	if (dev->version > 1) {
		WDEBUG("Driver %s does not support version: %d\n", dev->dev.driver->name, dev->version);
		return -ENODEV;
	}

	woab_md = kzalloc(sizeof(*woab_md), GFP_KERNEL);
	if (!woab_md)
		return -ENOMEM;

	woab_md->misc.minor = MISC_DYNAMIC_MINOR;
	snprintf(buf, sizeof(buf), "woab-misc-%d", woab_misc_count++);
	woab_md->misc.name = kstrdup(buf, GFP_KERNEL);
	woab_md->misc.parent = &dev->dev;
	woab_md->misc.fops = &woab_misc_fops;
	woab_md->dev = dev;
	dev_set_drvdata(&dev->dev, woab_md);

	ret = misc_register(&woab_md->misc);
	if (ret) {
		dev_err(&dev->dev, "failed to register misc device: %d\n", ret);
		return ret;
	}

	WDEBUG("Registered misc device %s in /dev/\n", buf);

	return 0;
}

void woab_misc_remove(struct woab_device *dev)
{
	struct woab_misc_device *woab_md;

	WTRACE();

	woab_md = (struct woab_misc_device *)dev_get_drvdata(&dev->dev);

	misc_deregister(&woab_md->misc);
	kfree(woab_md);
}

struct woab_driver woab_misc_driver = {
	.type = "misc",
	.probe = woab_misc_probe,
	.remove = woab_misc_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "woab_misc",
	},
};

/* =============================================== *
 *       Misc driver module related functions      *
 * =============================================== */
 
static int woab_driver_init(void)
{
	int err;

	WTRACE();

	err = woab_register_driver(&woab_misc_driver);
	if(err) {
		pr_err("unable to register driver: %d\n", err);
		return err;
	}

	return 0;
}

static void woab_driver_exit(void)
{
	WTRACE();
	woab_unregister_driver(&woab_misc_driver);
}

module_init(woab_driver_init);
module_exit(woab_driver_exit);