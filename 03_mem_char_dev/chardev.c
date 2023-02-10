#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>

#define DEVICE_NAME "woabchar"
#define CLASS_NAME "woab"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("woab");
MODULE_DESCRIPTION("A simple Linux char driver");
MODULE_VERSION("0.1");

#define DEFAULT_BUFFER 32

static int woabchar_major;
static int woabchar_minor;
unsigned int woabchar_nr_devices = 1;

struct woab_dev {
	struct cdev cdev;
	char *data;
	size_t data_length;
	size_t buffer_length;
	int numberOpens;
};

static int woab_dev_open(struct inode *, struct file *);
static int woab_dev_release(struct inode *, struct file *);
static ssize_t woab_dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t woab_dev_write(struct file *, const char *, size_t, loff_t *);
static int woab_device_alloc_buffer(size_t buffer_length);
static int woab_device_init(void);
static void woab_device_cleanup(void);
static void woab_setup_cdev(struct woab_dev *dev);
static int __init woabchar_init(void);
static void __exit woabchar_exit(void);

static struct file_operations woab_fops = {
	.owner = THIS_MODULE,
	.open = woab_dev_open,
	.read = woab_dev_read,
	.write = woab_dev_write,
	.release = woab_dev_release,
};

static struct woab_dev *woab_device;

static int woab_device_alloc_buffer(size_t buffer_length)
{
	if (!woab_device)
		return -ENXIO;
	kfree(woab_device->data);
	woab_device->data =
		(char *)kmalloc(buffer_length * sizeof(char), GFP_KERNEL);
	if (!woab_device->data) {
		printk(KERN_WARNING "WOABchar: ERROR kmalloc data\n");
		return -ENOMEM;
	}
	woab_device->buffer_length = buffer_length;

	return 0;
}

static int woab_device_init(void)
{
	int ret = 0;

	woab_device = kmalloc(sizeof(struct woab_dev), GFP_KERNEL);
	if (!woab_device) {
		printk(KERN_WARNING "WOABchar: ERROR kmalloc dev struct\n");
		return -ENOMEM;
	}
	memset(woab_device, 0, sizeof(struct woab_dev));

	printk(KERN_INFO "WOABchar: Initializing the WOABchar LKM\n");

	return ret;
}

static void woab_device_cleanup(void)
{
	kfree(woab_device->data);
	kfree(woab_device);
}

static void woab_setup_cdev(struct woab_dev *dev)
{
	int err, devno;
	devno = MKDEV(woabchar_major, woabchar_minor);

	cdev_init(&dev->cdev, &woab_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &woab_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		printk(KERN_WARNING "[WOABchar] Error %d adding cdev", err);

	printk(KERN_INFO "[WOABchar] cdev initialized\n");
}

static int __init woabchar_init(void)
{
	int ret;
	dev_t dev = 0;

	ret = alloc_chrdev_region(&dev, woabchar_minor, woabchar_nr_devices,
				  "woabchar");
	woabchar_major = MAJOR(dev);

	if (ret < 0) {
		printk(KERN_WARNING "[WOABchar]: Can't get major %d\n",
		       woabchar_major);
		return ret;
	}

	ret = woab_device_init();
	if (ret)
		return ret;

	woab_setup_cdev(woab_device);

	ret = woab_device_alloc_buffer(DEFAULT_BUFFER);
	if (ret)
		return ret;

	printk(KERN_INFO "[WOABchar]: Device initialized\n");

	return 0;
}

static void __exit woabchar_exit(void)
{
	dev_t devno = MKDEV(woabchar_major, woabchar_minor);
	cdev_del(&(woab_device->cdev));
	unregister_chrdev_region(devno, woabchar_nr_devices);

	woab_device_cleanup();
	printk(KERN_INFO "WOABchar: Goodbye from the LKM!\n");
}

static int woab_dev_open(struct inode *inodep, struct file *filep)
{
	struct woab_dev *dev;

	dev = container_of(inodep->i_cdev, struct woab_dev, cdev);
	filep->private_data = dev;

	woab_device->numberOpens++;

	printk(KERN_INFO "WOABchar: Device has been opened %d time(s)\n",
	       woab_device->numberOpens);
	return 0;
}

static ssize_t woab_dev_read(struct file *filep, char *buffer, size_t len,
			     loff_t *offset)
{
	int error_count = 0;

	error_count = copy_to_user(buffer, woab_device->data,
				   woab_device->data_length);

	if (0 == error_count) {
		printk(KERN_INFO "WOABchar: Sent %lu characters to the user\n",
		       woab_device->data_length);
		return woab_device->data_length;
	} else {
		printk(KERN_INFO
		       "WOABchar: Failed to send %d characters to the user\n",
		       error_count);
		return -EFAULT;
	}
}

static ssize_t woab_dev_write(struct file *filep, const char *buffer,
			      size_t len, loff_t *offset)
{
	int uncopied, ret;
	if (len > woab_device->buffer_length) {
		ret = woab_device_alloc_buffer(len);
		if (ret)
			return ret;
		printk(KERN_WARNING "WOABchar: New memory alocated succesfully"
				    " with length of %lu\n",
		       len);
	}

	woab_device->data_length = len;
	uncopied = copy_from_user(woab_device->data, buffer, len);
	printk(KERN_INFO
	       "WOABchar: Received %lu (%d uncopied) characters from the user",
	       len, uncopied);
	return len;
}

static int woab_dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "WOABchar: Device successfully closed\n");
	return 0;
}

module_init(woabchar_init);
module_exit(woabchar_exit);
