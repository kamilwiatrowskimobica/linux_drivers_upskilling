#include <linux/init.h> // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h> // Core header for loading LKMs into the kernel
#include <linux/device.h> // Header to support the kernel Driver Model
#include <linux/kernel.h> // Contains types, macros, functions for the kernel
#include <linux/fs.h> // Header for the Linux file system support
#include <linux/uaccess.h> // Required for the copy to user function
#include <linux/cdev.h>
#include <linux/types.h> /* needed for dev_t type */
#include <linux/kdev_t.h> /* needed for macros MAJOR, MINOR, MKDEV... */
#include <linux/slab.h> /* kmalloc(),kfree() */
#include <asm/uaccess.h> /* copy_to copy_from _user */
#include "memchardev.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rafal Cieslak");
MODULE_VERSION("0.1");
static int majorNumber = 0;
static int minorNumber = 0;

int device_max_size = DEVICE_MAX_SIZE;

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static void dev_cleanup_module(void);
static void dev_setup_cdev(struct char_dev *dev);

static struct file_operations fops = {
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

struct char_dev *char_device;

static int __init char_init(void)
{
	int result = 0;
	dev_t dev = 0;

	printk(KERN_INFO "Initializing the device LKM\n");

	printk(KERN_INFO "dynamic allocation of major number\n");
	result = alloc_chrdev_region(&dev, minorNumber, 1, "memchardev");
	majorNumber = MAJOR(dev);
	printk(KERN_INFO "Major number %d\n", majorNumber);

	if (result < 0) {
		printk(KERN_WARNING "can't get major number %d\n", majorNumber);
		return result;
	}
	printk(KERN_INFO "Initializing the device LKM\n");

	char_device = kmalloc(sizeof(struct char_dev), GFP_KERNEL);

	if (!char_device) {
		result = -ENOMEM;
		printk(KERN_WARNING " ERROR kmalloc dev struct\n");
		goto fail;
	}

	memset(char_device, 0, sizeof(struct char_dev));

	char_device->p_data =
		(char *)kmalloc(device_max_size * sizeof(char), GFP_KERNEL);
	if (!char_device->p_data) {
		result = -ENOMEM;
		printk(KERN_WARNING "ERROR kmalloc p_data\n");
		goto fail;
	}
	dev_setup_cdev(char_device);

	return 0;

fail:
	dev_cleanup_module();
	return result;
}

static void dev_setup_cdev(struct char_dev *dev)
{
	int err, devno = MKDEV(majorNumber, minorNumber);
	cdev_init(&dev->cdev, &fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &fops;
	err = cdev_add(&dev->cdev, devno, 1);

	if (err)
		printk(KERN_NOTICE "Error %d adding device", err);
	else
		printk(KERN_INFO "Device added \n");
}

void dev_cleanup_module(void)
{
	dev_t devno = MKDEV(majorNumber, minorNumber);
	cdev_del(&(char_device->cdev));

	if ((char_device->p_data) != 0) {
		kfree(char_device->p_data);
		printk(KERN_INFO "kfree the string-memory\n");
	}
	if ((char_device) != 0) {
		kfree(char_device);
		printk(KERN_INFO " kfree devices\n");
	}
	unregister_chrdev_region(devno, 1);
	printk(KERN_INFO " cdev deleted, kfree, chdev unregistered\n");
}

static void __exit char_exit(void)
{
	printk(KERN_INFO "Exiting\n");
	dev_cleanup_module();
}

static int dev_open(struct inode *inodep, struct file *filp)
{
	struct char_dev *dev;
	dev = container_of(inodep->i_cdev, struct char_dev, cdev);
	filp->private_data = dev;

	return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "Device successfully closed\n");
	return 0;
}

static ssize_t dev_read(struct file *filp, char *buffer, size_t count,
			loff_t *f_pos)
{
	ssize_t retval = 0;
	int error_count = 0;

	if (count > device_max_size) {
		printk(KERN_WARNING
		       "trying to read more than possible. Aborting read\n");
		retval = -EFBIG;
		return retval;
	}

	error_count = copy_to_user(buffer, (void *)char_device->p_data, count);

	if (error_count == 0) {
		printk(KERN_INFO "Sent %d characters to the user\n", count);
		return retval;
	} else {
		printk(KERN_INFO "Failed to send %d characters to the user\n",
		       error_count);
		return -EFAULT;
	}
}

static ssize_t dev_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *f_pos)
{
	int retval = 0;

	if (count > device_max_size) {
		printk(KERN_WARNING
		       "trying to write more than possible. Aborting write\n");
		retval = -EFBIG;
		return retval;
	}
	if (copy_from_user((void *)char_device->p_data, buf, count)) {
		printk(KERN_WARNING "can't use copy_from_user. \n");
		retval = -EPERM;
	}
	return retval;
}

module_init(char_init);
module_exit(char_exit);