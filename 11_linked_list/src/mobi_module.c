#include <linux/init.h> // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h> // Core header for loading LKMs into the kernel
#include <linux/device.h> // Header to support the kernel Driver Model
#include <linux/kernel.h> // Contains types, macros, functions for the kernel
#include <linux/fs.h> // Header for the Linux file system support
#include <linux/uaccess.h> // Required for the copy to user function
#include <linux/slab.h> // kmalloc()
#include <linux/list.h>
#include <linux/utsname.h>

#include "mobi_module.h"

///< The device will appear at /dev/mob using this value
#define MOB_DEVICE_NAME "mob"
///< The device class -- this is a character device driver
#define CLASS_NAME "lukr"
#define DEVICE_NAME_FORMAT "%s_%zu"

#define MOB_MESSAGE_LEN 256
#define MOB_DEFAULT_DEV_COUNT 1

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lukasz Krakowiak");
MODULE_DESCRIPTION("A simple Linux char driver with linked list");
MODULE_VERSION("0.1");

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

static struct file_operations fops = {
	.open = mobdev_open,
	.read = mobdev_read,
	.write = mobdev_write,
	.release = mobdev_release,
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
	struct list_head *ptr;
	struct mob_data *entry;

	if (mdev == NULL)
		return;

	list_for_each(ptr, &mdev->items) {
		entry = list_entry(ptr, struct mob_data, list);
		kfree(entry->p_data);
		kfree(entry);
	}

	mutex_destroy(&mdev->lock);
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
		pr_info("mobdev_clean_cdev\n");
	}

	if ((type_c & CLEANUP_KFREE) && mob_devices) {
		kfree(mob_devices);
		pr_info("kfree\n");
	}

	if (type_c & CLEANUP_CLASS_DESTROY) {
		class_destroy(mob_class);
		pr_info("class_destroy\n");
	}

	if (type_c & CLEANUP_U_REGION) {
		unregister_chrdev_region(dev, dev_count);
		pr_info("unregister_chrdev_region\n");
	}
}

static int mobdev_open(struct inode *inodep, struct file *filep)
{
	struct mob_dev *mdev;
	mdev = container_of(inodep->i_cdev, struct mob_dev, cdev);
	if (!mdev) {
		pr_alert( "MOBChar: %s error accessing mob_dev\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	filep->private_data = mdev; /* for other methods */

	pr_info_ratelimited( "MOBChar: Device [%ld] has been opened\n",
			   mdev->id);
	return nonseekable_open(inodep, filep);
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
	struct mob_data *data;
	u32 counter;

	if (!mdev) {
		pr_alert( "MOBChar: %s error accessing mob_dev\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	if (mutex_lock_interruptible(&mdev->lock) != 0)
		return -EINTR;

	if (list_empty(&mdev->items)) {
		pr_alert( "MOBChar: nothing to read\n");
		size = 0;
		goto ERR;
	}

	data = list_first_entry(&mdev->items, struct mob_data, list);
	if (len < data->size) {
		pr_alert( "MOBChar: read buff to small %ld, exp: %ld\n",
		       len, data->size);
		size = 0;
		goto ERR;
	}

	result = copy_to_user(buffer, data->p_data, data->size);
	if (result != 0) {
		pr_alert(
		       "MOBChar: Failed to send %d characters to the user\n",
		       result);
		size = -EFAULT;
		goto ERR;
	}

	size = data->size;

	list_del(&data->list);
	counter = mdev->items_counter--;

	mutex_unlock(&mdev->lock);

	kfree(data->p_data);
	kfree(data);

	pr_info("MOBChar: %s_%d Sent %ld bytes to user [list:%d]\n",
		MOB_DEVICE_NAME, MINOR(mdev->cdev.dev), size, counter);

	return size;

ERR:
	mutex_unlock(&mdev->lock);
	return size;
}

static ssize_t mobdev_write(struct file *filep, const char *buffer, size_t len,
			    loff_t *offset)
{
	struct mob_dev *mdev = (struct mob_dev *)filep->private_data;
	struct mob_data *data;
	int result = 0;
	u32 counter;

	if (mdev == NULL) {
		pr_alert("MOBChar: %s error accessing mob_dev\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->p_data = kzalloc(len, GFP_KERNEL);
	if (!data->p_data) {
		kfree(data);
		return -ENOMEM;
	}

	data->size = len;
	INIT_LIST_HEAD(&data->list);

	result = copy_from_user(data->p_data, buffer, len);
	if (result != 0) {
		kfree(data->p_data);
		kfree(data);
		pr_alert("MOBChar: Failed to send %d characters from user\n",
		       result);
		return -EFAULT;
	}

	if (mutex_lock_interruptible(&mdev->lock) != 0)
		return -EINTR;

	list_add_tail(&data->list, &mdev->items);
	counter = ++mdev->items_counter;

	mutex_unlock(&mdev->lock);

	pr_info("MOBChar: %s_%d Add %zu bytes to list [%d]\n",
		MOB_DEVICE_NAME, MINOR(mdev->cdev.dev), len, counter);

	return len;
}

static int mobdev_init_cdev(struct mob_dev *mdev, size_t index)
{
	int result = 0;
	dev_t dev = MKDEV(major_number, index);
	struct device *mob_device = NULL;

	cdev_init(&mdev->cdev, &fops);
	mdev->cdev.owner = THIS_MODULE;

	result = cdev_add(&mdev->cdev, dev, 1);
	if (result < 0) {
		pr_alert("MOBChar: failed to add cdev\n");
		return result;
	}

	mutex_init(&mdev->lock);
	mdev->id = index;
	INIT_LIST_HEAD(&mdev->items);

	// Register the device driver
	mob_device = device_create(mob_class, NULL, dev, NULL,
				   DEVICE_NAME_FORMAT, MOB_DEVICE_NAME, index);
	if (IS_ERR(mob_device)) {
		pr_alert("Failed to create the device\n");
		return PTR_ERR(mob_device);
	}

	pr_info("MOBChar: created %s_%d\n", MOB_DEVICE_NAME,
	       MINOR(dev));

	return 0;
}

static int __init mobdev_init(void)
{
	dev_t dev = 0;
	int result = 0;
	int i = 0;

	/* print information */
	printk("arch   Size:  char  short  int  long   ptr long-long "
		" u8 u16 u32 u64\n");
	printk("%-12s  %3i   %3i   %3i   %3i   %3i   %3i      "
		"%3i %3i %3i %3i\n",
	        init_uts_ns.name.machine,
		(int)sizeof(char), (int)sizeof(short), (int)sizeof(int),
		(int)sizeof(long),
		(int)sizeof(void *), (int)sizeof(long long), (int)sizeof(__u8),
		(int)sizeof(__u16), (int)sizeof(__u32), (int)sizeof(__u64));


	result = alloc_chrdev_region(&dev, 0, dev_count, MOB_DEVICE_NAME);
	if (result < 0) {
		pr_alert("MOBChar: failed to alloc char device\n");
		return result;
	}

	// get allocated dev major number from dev_t
	major_number = MAJOR(dev);

	mob_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(mob_class)) {
		pr_alert("MOBChar: Failed to reg device class\n");
		exit_handler(CLEANUP_U_REGION);
		return PTR_ERR(mob_class);
	}

	/* Initialize the device. */
	mob_devices = kcalloc(dev_count, sizeof(struct mob_dev), GFP_KERNEL);
	if (IS_ERR(mob_devices)) {
		pr_alert("MOBChar: Failed kcalloc\n");
		exit_handler(CLEANUP_U_REGION | CLEANUP_CLASS_DESTROY);
		return PTR_ERR(mob_devices);
	}

	for (i = 0; i < dev_count; i++) {
		result = mobdev_init_cdev(&mob_devices[i], i);
		if (result < 0) {
			exit_handler(CLEANUP_ALL);
			pr_alert("MOBChar: Error init device %d\n", i);
			return result;
		}
	}

	pr_info("MOBChar: Init LKM correctly with major number %d\n",
	       major_number);
	return 0;
}

static void __exit mobdev_exit(void)
{
	exit_handler(CLEANUP_ALL);
	pr_info("MOBChar: Goodbye from the LKM!\n");
}

module_init(mobdev_init);
module_exit(mobdev_exit);
