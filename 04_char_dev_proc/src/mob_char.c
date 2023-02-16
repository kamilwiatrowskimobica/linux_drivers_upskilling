#include <linux/init.h> /* needed for module_init and exit */
#include <linux/module.h>
#include <linux/moduleparam.h> /* needed for module_param */
#include <linux/kernel.h> /* needed for printk */
#include <linux/sched.h> /* needed for current-> */
#include <linux/types.h> /* needed for dev_t type */
#include <linux/kdev_t.h> /* needed for macros MAJOR, MINOR, MKDEV... */
#include <linux/fs.h> /* needed for register_chrdev_region, file_operations */
#include <linux/cdev.h> /* cdev definition */
#include <linux/slab.h>
#include <asm/uaccess.h> /* copy_to copy_from _user */

#include "mob_debug.h"
#include "mob_char.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AKP");
MODULE_VERSION("1.0");

int mob_major = 0;
int mob_minor = 0;
unsigned int mob_nr_devs = 2;
int device_max_size = DEVICE_MAX_SIZE;

module_param(mob_major, int, S_IRUGO);
module_param(mob_minor, int, S_IRUGO);
module_param(mob_nr_devs, int, S_IRUGO);
module_param(device_max_size, int, S_IRUGO);

// Fops prototypes
static int __init mob_init(void);
static void __exit mob_exit(void);
static int mob_open(struct inode *inode, struct file *filp);
static int mob_release(struct inode *inode, struct file *filp);
static ssize_t mob_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *f_pos);
static ssize_t mob_read(struct file *filp, char __user *buf, size_t count,
			loff_t *f_pos);
void mob_cleanup_module(void);

/**
 * @brief Allocates mob_dev struct.
 * 
 * @param mob_num_devs Number of mob_dev instances.
 * @return int 0 if success, else linux err code.
 */
static int mob_device_alloc(unsigned int mob_num_devs);
/**
 * @brief Allocates mob_dev buffer.
 * 
 * @param len Buffer len.
 * @return int 0 if success, else linux err code.
 */
static int mob_buffer_alloc(size_t len);
/**
 * @brief Set up the char_dev structure for this device.
 * 
 * @param dev mob_dev struct pointer.
 */
static void mob_setup_cdev(struct mob_dev *dev);
// ------------------------------------------------------

struct mob_dev *mob_devices; /* allocated in mob_init_module */

struct file_operations mob_fops = {
	.owner = THIS_MODULE,
	.read = mob_read,
	.write = mob_write,
	.open = mob_open,
	.release = mob_release,
};

static int mob_buffer_alloc(size_t len)
{
	// sanity
	int ret = 0;
	if (NULL == mob_devices) {
		ret = PTR_ERR(mob_devices);
	}

	if (!ret) {
		mob_devices->p_data =
			(char *)kzalloc(len * sizeof(char), GFP_KERNEL);
		if (!mob_devices->p_data) {
			ret = -ENOMEM;
			printk(KERN_WARNING "ERROR kmalloc p_data\n");
		} else {
			mob_devices->buffer_len = len;
		}
	}

	return ret;
}

static int mob_device_alloc(unsigned int mob_num_devs)
{
	int result = 0;
	mob_devices =
		kmalloc(mob_num_devs * sizeof(struct mob_dev), GFP_KERNEL);
	if (!mob_devices) {
		result = -ENOMEM;
		printk(KERN_WARNING "ERROR kmalloc dev struct\n");
	} else {
		memset(mob_devices, 0, mob_num_devs * sizeof(struct mob_dev));
	}

	return result;
}

static int __init mob_init(void)
{
	int result = 0;
	dev_t dev = 0;

	if (mob_major) {
		printk(KERN_INFO "static allocation of major number (%d)\n",
		       mob_major);
		dev = MKDEV(mob_major, mob_minor);
		result = register_chrdev_region(dev, mob_nr_devs, "mob_char");
	} else {
		printk(KERN_INFO "dinamic allocation of major number\n");
		result = alloc_chrdev_region(&dev, mob_minor, mob_nr_devs,
					     "mob_char");
		mob_major = MAJOR(dev);
		MOB_PRINT("Major number is %d", mob_major);
	}
	if (result < 0) {
		printk(KERN_WARNING "MOBCHAR: can't get major %d\n", mob_major);
		return result;
	}

	if (!result)
		// Device allocation
		result = mob_device_alloc(mob_nr_devs);

	if (!result)
		// Buffer allocation
		result = mob_buffer_alloc(DEVICE_MAX_SIZE);

	if (!result)
		mob_setup_cdev(mob_devices);

	if (!result) {
		printk(KERN_INFO "MOBCHAR INIT success\n");
	}

	if (0 > result)
		mob_cleanup_module();

	return result;
}

/*
 * The exit function is simply calls the cleanup
 */

static void __exit mob_exit(void)
{
	printk(KERN_INFO "--calling cleanup function--\n");
	mob_cleanup_module();
}

// Open function
static int mob_open(struct inode *inode, struct file *filp)
{
	struct mob_dev *dev; /* device information */
	printk(KERN_INFO "Performing 'open' operation\n");
	dev = container_of(inode->i_cdev, struct mob_dev, cdev);
	filp->private_data = dev; /* for other methods */

	return 0; /* success */
}

// Release function
static int mob_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "Prforming 'release' operation\n");
	return 0;
}

void mob_cleanup_module(void)
{
	dev_t devno = MKDEV(mob_major, mob_minor);
	cdev_del(&(mob_devices->cdev));
	/* freeing the memory */
	if ((mob_devices->p_data) != 0) {
		kfree(mob_devices->p_data);
		printk(KERN_INFO "Kfree the string-memory\n");
	}
	if ((mob_devices) != 0) {
		kfree(mob_devices);
		printk(KERN_INFO "Kfree mob_devices\n");
	}
	unregister_chrdev_region(devno, mob_nr_devs); /* unregistering device */
	printk(KERN_INFO "Cdev deleted, kfree, chdev unregistered\n");
}

static ssize_t mob_read(struct file *filp, char __user *buf, size_t count,
			loff_t *f_pos)
{
	ssize_t retval;
	size_t size;

	MOB_PRINT("%s: len = %lu, offset = %llu\n, data_len = %d",
		  __FUNCTION__, count, *f_pos, mob_devices->data_len);

	if (*f_pos >= mob_devices->data_len)
		return 0;

	if (mob_devices->data_len > count) {
		size = count;
	} else {
		size = mob_devices->data_len;
	}

	int err = copy_to_user(buf, (void *)mob_devices->p_data + *f_pos, size);

	if (0 == err) {
		retval = size;
		printk(KERN_INFO "%lu chars are sent to the user\n", size);
		*f_pos += size;

	} else {
		retval = -EPERM;
		printk(KERN_WARNING "Failed to send chars to the user!\n");
	}

	return retval;
}

static ssize_t mob_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *f_pos)
{
	int retval = 0;

	MOB_PRINT("%s: len = %lu, offset = %llu\n", __FUNCTION__, count, *f_pos);

	if (count > mob_devices->buffer_len) {
		printk(KERN_WARNING
		       "MOBCHAR: trying to write more than possible. Aborting write\n");
		retval = -EFBIG;

	} else if (copy_from_user((void *)(mob_devices->p_data + *f_pos), buf,
				  count)) {
		printk(KERN_WARNING "MOBCHAR: can't use copy_from_user. \n");
		retval = -EPERM;
	}

	if (!retval) {
		mob_devices->data_len = count;
		*f_pos += count;
		MOB_PRINT("%lu number of written bytes\n", count);
	}

	return retval;
}

/*
 * Set up the char_dev structure for this device.
 */
static void mob_setup_cdev(struct mob_dev *dev)
{
	int err;
	int devno = MKDEV(mob_major, mob_minor);

	cdev_init(&dev->cdev, &mob_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &mob_fops;
	err = cdev_add(&dev->cdev, devno, mob_nr_devs);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_WARNING "Error %d adding mobchar cdev_add", err);

	printk(KERN_INFO "cdev initialized\n");
}

module_init(mob_init);
module_exit(mob_exit);