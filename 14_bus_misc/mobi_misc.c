#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include "mobi_bus.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simple Linux misc driver");
MODULE_VERSION("0.1");


struct mob_misc_device {
	struct miscdevice misc;
	struct mob_device *dev;
};

/* =============================================== *
 *          Misc device related functions          *
 * =============================================== */

static int my_open(struct inode *inode, struct file *file)
{
    MOB_FUNCTION_DEBUG();
	return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
    MOB_FUNCTION_DEBUG();
	return 0;
}

static ssize_t my_read(struct file *file, char __user *user_buffer,
		   size_t size, loff_t *offset)
{
	struct mob_misc_device *mob_md = (struct mob_misc_device *)file->private_data;
	ssize_t len = min(sizeof(mob_md->dev->buffer) - (ssize_t)*offset, size);
	
	MOB_FUNCTION_DEBUG();

	if (len <= 0)
		return 0;

	if (copy_to_user(user_buffer, mob_md->dev->buffer + *offset, len))
		return -EFAULT;

	*offset += len;
	return len;
}

static ssize_t my_write(struct file *file, const char __user *user_buffer,
		    size_t size, loff_t *offset)
{
	struct mob_misc_device *mob_md = (struct mob_misc_device *)file->private_data;
	ssize_t len = min(sizeof(mob_md->dev->buffer) - (ssize_t)*offset, size);

	MOB_FUNCTION_DEBUG();	

	if (len <= 0)
		return 0;

	if (copy_from_user(mob_md->dev->buffer + *offset, user_buffer, len))
		return -EFAULT;

	*offset += len;
	return len;
}

struct file_operations mob_misc_fops = {
	.owner = THIS_MODULE,
	.open = my_open,
	.read = my_read,
	.write = my_write,
	.release = my_release,
};

/* =============================================== *
 *          Misc driver related functions          *
 * =============================================== */

static int mob_misc_count;


int mob_misc_probe(struct mob_device *dev)
{
	struct mob_misc_device *bmd;
	char buf[32];
	int ret;

    MOB_FUNCTION_DEBUG();

	dev_info(&dev->dev, "%s: \"%s\"\n", __func__, dev->type);

	bmd = kzalloc(sizeof(*bmd), GFP_KERNEL);
	if (!bmd)
		return -ENOMEM;

	bmd->misc.minor = MISC_DYNAMIC_MINOR;
	snprintf(buf, sizeof(buf), "mob-misc-%d", mob_misc_count++);
	bmd->misc.name = kstrdup(buf, GFP_KERNEL);
	bmd->misc.parent = &dev->dev;
	bmd->misc.fops = &mob_misc_fops;
	bmd->dev = dev;
	dev_set_drvdata(&dev->dev, bmd);

	ret = misc_register(&bmd->misc);
	if (ret) {
		dev_err(&dev->dev, "failed to register misc device: %d\n", ret);
		return ret;
	}

	pr_info("Registered misc device %s in /dev/\n", buf);

	return 0;
}

void mob_misc_remove(struct mob_device *dev)
{
	struct mob_misc_device *bmd;

    MOB_FUNCTION_DEBUG();

	bmd = (struct mob_misc_device *)dev_get_drvdata(&dev->dev);

	/* deregister the misc device */
	misc_deregister(&bmd->misc);
	kfree(bmd);
}

struct mob_driver mob_misc_driver = {
	.type = "misc",
	.probe = mob_misc_probe,
	.remove = mob_misc_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "mob_misc",
	},
};

/* =============================================== *
 *       Misc driver module related functions      *
 * =============================================== */

static int my_init(void)
{
	int err;

    MOB_FUNCTION_DEBUG();

	/* register the driver */
	err = mob_register_driver(&mob_misc_driver);
	if(err) {
		pr_err("unable to register driver: %d\n", err);
		return err;
	}

	return 0;
}

static void my_exit(void)
{
    MOB_FUNCTION_DEBUG();
	/* unregister the driver */
	mob_unregister_driver(&mob_misc_driver);
}

module_init(my_init);
module_exit(my_exit);