#include <linux/init.h> // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h> // Core header for loading LKMs into the kernel
#include <linux/device.h> // Header to support the kernel Driver Model
#include <linux/kernel.h> // Contains types, macros, functions for the kernel
#include <linux/fs.h> // Header for the Linux file system support
#include <linux/uaccess.h> // Required for the copy to user function
#include <linux/slab.h> // kmalloc()
#include <linux/completion.h>

#include "mobi_module.h"
#include "lock_api.h"

///< The device will appear at /dev/mob using this value
#define MOB_DEVICE_NAME "mob"
///< The device class -- this is a character device driver
#define CLASS_NAME "lukr"
#define DEVICE_NAME_FORMAT "%s_%zu"

#define MOB_MESSAGE_LEN 256
#define MOB_DEFAULT_DEV_COUNT 1

#define WAIT_EN

static uint dev_count = MOB_DEFAULT_DEV_COUNT;
module_param(dev_count, uint, S_IRUGO);
MODULE_PARM_DESC(dev_count, "Number of creating mob devices");

static int major_number = 0;
///< The device-driver class struct pointer
static struct class *mob_class = NULL;
static struct mob_dev *mob_devices = NULL;

// The prototype functions for the character driver -- must come before the struct definition
static int mobdev_open(struct inode *, struct file *);
static int mobdev_release(struct inode *, struct file *);
static ssize_t mobdev_read(struct file *, char *, size_t, loff_t *);
static ssize_t mobdev_write(struct file *, const char *, size_t, loff_t *);
static loff_t mobdev_llseek(struct file *, loff_t, int);

static struct file_operations fops = {
	.open = mobdev_open,
	.read = mobdev_read,
	.write = mobdev_write,
	.release = mobdev_release,
	.llseek = mobdev_llseek,
};

typedef enum {
	CLEANUP_U_REGION = 1,
	CLEANUP_CLASS_DESTROY = 2,
	CLEANUP_KFREE = 4,
	CLEANUP_DEVS = 8,
	CLEANUP_ALL = 15
} type_of_cleanup_t;

static void mobdev_clean_cdev(struct mob_dev *mdev)
{
	if (mdev == NULL)
		return;

	if (mdev->msg)
		kfree(mdev->msg);

	//destroy locks
	lock_deinit(mdev, lock_mutex);

	device_destroy(mob_class, mdev->cdev.dev);
	cdev_del(&mdev->cdev);
}

void exit_handler(uint type_c)
{
	int i = 0;
	dev_t dev = MKDEV(major_number, 0);

	if ((type_c & CLEANUP_DEVS) && mob_devices) {
		for (i = 0; i < dev_count; i++) {
			mobdev_clean_cdev(&mob_devices[i]);
		}
		printk(KERN_INFO "mobdev_clean_cdev\n");
	}

	if ((type_c & CLEANUP_KFREE) && mob_devices) {
		kfree(mob_devices);
		printk(KERN_INFO "kfree\n");
	}

	if (type_c & CLEANUP_CLASS_DESTROY) {
		class_destroy(mob_class);
		printk(KERN_INFO "class_destroy\n");
	}

	if (type_c & CLEANUP_U_REGION) {
		unregister_chrdev_region(dev, dev_count);
		printk(KERN_INFO "unregister_chrdev_region\n");
	}
}

static int mobdev_open(struct inode *inodep, struct file *filep)
{
	struct mob_dev *mdev;
	mdev = container_of(inodep->i_cdev, struct mob_dev, cdev);
	if (!mdev) {
		printk(KERN_ALERT "MOBChar: %s error accessing mob_dev\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	filep->private_data = mdev; /* for other methods */

	printk_ratelimited(KERN_INFO "MOBChar: Device [%ld] has been opened\n",
			   mdev->id);
	return 0;
}

static int mobdev_release(struct inode *inodep, struct file *filep)
{
	struct mob_dev *mdev;
	mdev = container_of(inodep->i_cdev, struct mob_dev, cdev);
	if (mdev)
		printk_ratelimited(
			KERN_INFO "MOBChar: Device [%ld] successfully closed\n",
			mdev->id);
	return 0;
}

static ssize_t mobdev_read(struct file *filep, char *buffer, size_t len,
			   loff_t *offset)
{
	int result = 0;
	size_t size = 0;
	struct mob_dev *mdev = (struct mob_dev *)filep->private_data;

	if (!mdev) {
		printk(KERN_ALERT "MOBChar: %s error accessing mob_dev\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	lock(mdev, lock_action_spinlock);
	size = mdev->msg_size - *offset;

	if (*offset > mdev->msg_size) {
		size = 0;
		goto END;
	}

	if (*offset + len > mdev->msg_size)
		len = mdev->msg_size - *offset;

	result = copy_to_user(buffer, mdev->msg + *offset, size);
	if (result != 0) {
		unlock(mdev, lock_action_spinlock);
		printk(KERN_ALERT
		       "MOBChar: Failed to send %d characters to the user\n",
		       result);
		return -EFAULT;
	}

	mdev->msg_size -= size;
	*offset += size;

END:

	unlock(mdev, lock_action_spinlock);

	printk_ratelimited(KERN_INFO
			   "MOBChar: %s_%d Sent %ld characters to user\n",
			   MOB_DEVICE_NAME, MINOR(mdev->cdev.dev), size);

	return size;
}

static ssize_t mobdev_write(struct file *filep, const char *buffer, size_t len,
			    loff_t *offset)
{
	struct mob_dev *mdev = (struct mob_dev *)filep->private_data;
	int result = 0;

	if (mdev == NULL) {
		printk(KERN_ALERT "MOBChar: %s error accessing mob_dev\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	if (mdev->msg == NULL) {
		printk(KERN_ALERT "MOBChar: %s error accessing mob_dev msg\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	lock(mdev, lock_action_spinlock);

	if (len + *offset > MOB_MESSAGE_LEN) {
		unlock(mdev, lock_action_spinlock);
		printk(KERN_ALERT "MOBChar: User msg to big\n");
		return -EMSGSIZE;
	}

	result = copy_from_user(mdev->msg + *offset, buffer, len);
	if (result != 0) {
		unlock(mdev, lock_action_spinlock);
		printk(KERN_ALERT
		       "MOBChar: Failed to send %d characters from user\n",
		       result);
		return -EFAULT;
	}

	mdev->msg_size = len + *offset;
	*offset += len;

	unlock(mdev, lock_action_spinlock);

	printk_ratelimited(
		KERN_INFO
		"MOBChar: %s_%d Received %zu characters from the user\n",
		MOB_DEVICE_NAME, MINOR(mdev->cdev.dev), len);

	return len;
}

loff_t mobdev_llseek(struct file *filep, loff_t offset, int whence)
{
	loff_t npos = 0;
	int result;
	struct mob_dev *mdev = (struct mob_dev *)filep->private_data;

	printk_ratelimited(KERN_INFO
			   "MOBChar: llseek, offset = %lld, whence = %d\n",
			   offset, whence);

	if (mdev == NULL) {
		printk(KERN_ALERT "MOBChar: %s error accessing mob_dev\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	if (mdev->msg == NULL) {
		printk(KERN_ALERT "MOBChar: %s error accessing mob_dev msg\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	result = lock(mdev, lock_action_spinlock);
	if (result != 0)
		return result;

	switch (whence) {
	case SEEK_SET:
		if (offset >= MOB_MESSAGE_LEN) {
			unlock(mdev, lock_action_spinlock);
			return -EFBIG;
		}
		npos = offset;
		break;

	case SEEK_CUR:
		if (filep->f_pos + offset >= MOB_MESSAGE_LEN) {
			unlock(mdev, lock_action_spinlock);
			return -EFBIG;
		}
		npos = filep->f_pos + offset;
		break;

	case SEEK_END:

		if (mdev->msg_size + offset >= MOB_MESSAGE_LEN) {
			unlock(mdev, lock_action_spinlock);
			return -EFBIG;
		}
		npos = mdev->msg_size + offset;
		break;

	default: /* not expected value */
		return -EINVAL;
	}

	if (npos < 0)
		return -EINVAL;
	filep->f_pos = npos;
	unlock(mdev, lock_action_spinlock);

	return npos;
}

static int mobdev_init_cdev(struct mob_dev *mdev, size_t index)
{
	int result = 0;
	dev_t dev = MKDEV(major_number, index);
	struct device *mob_device = NULL;

	cdev_init(&mdev->cdev, &fops);
	mdev->cdev.owner = THIS_MODULE;

	mdev->msg = kcalloc(1, MOB_MESSAGE_LEN, GFP_KERNEL);
	if (IS_ERR(mdev->msg)) {
		printk(KERN_ALERT "MOBChar: failed to alloc p_data\n");
		return PTR_ERR(mdev->msg);
	}

	result = cdev_add(&mdev->cdev, dev, 1);
	if (result < 0) {
		printk(KERN_ALERT "MOBChar: failed to add cdev\n");
		return result;
	}
	lock_init(mdev, lock_spinlock);
	mdev->id = index;

	// Register the device driver
	mob_device = device_create(mob_class, NULL, dev, NULL,
				   DEVICE_NAME_FORMAT, MOB_DEVICE_NAME, index);
	if (IS_ERR(mob_device)) {
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(mob_device);
	}

	printk(KERN_INFO "MOBChar: created %s_%d\n", MOB_DEVICE_NAME,
	       MINOR(dev));

	return 0;
}

static int __init mobdev_init(void)
{
	dev_t dev = 0;
	int result = 0;
	int i = 0;

	result = alloc_chrdev_region(&dev, 0, dev_count, MOB_DEVICE_NAME);
	if (result < 0) {
		printk(KERN_ALERT "MOBChar: failed to alloc char device\n");
		return result;
	}

	// get allocated dev major number from dev_t
	major_number = MAJOR(dev);

	mob_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(mob_class)) {
		printk(KERN_ALERT "MOBChar: Failed to reg device class\n");
		exit_handler(CLEANUP_U_REGION);
		return PTR_ERR(mob_class);
	}

	/* Initialize the device. */
	mob_devices = kcalloc(dev_count, sizeof(struct mob_dev), GFP_KERNEL);
	if (IS_ERR(mob_devices)) {
		printk(KERN_ALERT "MOBChar: Failed kcalloc\n");
		exit_handler(CLEANUP_U_REGION | CLEANUP_CLASS_DESTROY);
		return PTR_ERR(mob_devices);
	}

	for (i = 0; i < dev_count; i++) {
		result = mobdev_init_cdev(&mob_devices[i], i);
		if (result < 0) {
			exit_handler(CLEANUP_ALL);
			printk(KERN_ALERT "MOBChar: Error init device %d\n", i);
			return result;
		}
	}

	printk(KERN_INFO "MOBChar: Init LKM correctly with major number %d\n",
	       major_number);
	return 0;
}

static void __exit mobdev_exit(void)
{
	exit_handler(CLEANUP_ALL);
	printk(KERN_INFO "MOBChar: Goodbye from the LKM!\n");
}

module_init(mobdev_init);
module_exit(mobdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lukasz Krakowiak");
MODULE_DESCRIPTION("A simple Linux char driver");
MODULE_VERSION("0.1");
