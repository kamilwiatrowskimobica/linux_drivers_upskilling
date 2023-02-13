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

#include "hello.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AKP");
MODULE_VERSION("1.0");

int hello_major = 0;
int hello_minor = 0;
unsigned int hello_nr_devs = 1;
int device_max_size = DEVICE_MAX_SIZE;

module_param(hello_major, int, S_IRUGO);
module_param(hello_minor, int, S_IRUGO);
module_param(device_max_size, int, S_IRUGO);

// Fops prototypes
static int __init hello_init(void);
static void __exit hello_exit(void);
static int hello_open(struct inode *inode, struct file *filp);
static int hello_release(struct inode *inode, struct file *filp);
static ssize_t hello_write(struct file *filp, const char __user *buf,
			   size_t count, loff_t *f_pos);
static ssize_t hello_read(struct file *filp, char __user *buf, size_t count,
			  loff_t *f_pos);
void hello_cleanup_module(void);

/**
 * @brief Allocates hello_dev struct.
 * 
 * @param hello_num_devs Number of hello_dev instances.
 * @return int 0 if success, else linux err code.
 */
static int hello_device_alloc(unsigned int hello_num_devs);
/**
 * @brief Allocates hello_dev buffer.
 * 
 * @param len Buffer len.
 * @return int 0 if success, else linux err code.
 */
static int hello_buffer_alloc(size_t len);
/**
 * @brief Set up the char_dev structure for this device.
 * 
 * @param dev hello_dev struct pointer.
 */
static void hello_setup_cdev(struct hello_dev *dev);
// ------------------------------------------------------

struct hello_dev *hello_devices; /* allocated in hello_init_module */

struct file_operations hello_fops = {
	.owner = THIS_MODULE,
	.read = hello_read,
	.write = hello_write,
	.open = hello_open,
	.release = hello_release,
};

static int hello_buffer_alloc(size_t len)
{
	// sanity
	int ret = 0;
	if (NULL == hello_devices) {
		ret = PTR_ERR(hello_devices);
	}

	if (!ret) {
		/*
		hello_devices->p_data =
			(char *)kmalloc(len * sizeof(char), GFP_KERNEL);
		*/
		hello_devices->p_data =
			(char *)kzalloc(len * sizeof(char), GFP_KERNEL);
		if (!hello_devices->p_data) {
			ret = -ENOMEM;
			printk(KERN_WARNING "ERROR kmalloc p_data\n");
		} else {
			hello_devices->data_len = len;
		}
	}

	return ret;
}

static int hello_device_alloc(unsigned int hello_num_devs)
{
	int result = 0;
	hello_devices =
		kmalloc(hello_num_devs * sizeof(struct hello_dev), GFP_KERNEL);
	if (!hello_devices) {
		result = -ENOMEM;
		printk(KERN_WARNING "ERROR kmalloc dev struct\n");
	} else {
		memset(hello_devices, 0,
		       hello_num_devs * sizeof(struct hello_dev));
	}

	return result;
}

static int __init hello_init(void)
{
	int result = 0;
	dev_t dev = 0;

	if (hello_major) {
		printk(KERN_INFO "static allocation of major number (%d)\n",
		       hello_major);
		dev = MKDEV(hello_major, hello_minor);
		result = register_chrdev_region(dev, hello_nr_devs, "hello");
	} else {
		printk(KERN_INFO "dinamic allocation of major number\n");
		result = alloc_chrdev_region(&dev, hello_minor, hello_nr_devs,
					     "hello");
		hello_major = MAJOR(dev);
		printk(KERN_INFO "Major number is:%d\n", hello_major);
	}
	if (result < 0) {
		printk(KERN_WARNING "Hello: can't get major %d\n", hello_major);
		return result;
	}

	if (!result)
		// Device allocation
		result = hello_device_alloc(hello_nr_devs);

	if (!result)
		// Buffer allocation
		result = hello_buffer_alloc(DEVICE_MAX_SIZE);

	if (!result)
		hello_setup_cdev(hello_devices);

	if (!result) {
		printk(KERN_INFO "Hello INIT success\n");
	}

	if (0 > result)
		hello_cleanup_module();

	return result;
}

/*
 * The exit function is simply calls the cleanup
 */

static void __exit hello_exit(void)
{
	printk(KERN_INFO "--calling cleanup function--\n");
	hello_cleanup_module();
}

// Open function
static int hello_open(struct inode *inode, struct file *filp)
{
	struct hello_dev *dev; /* device information */
	printk(KERN_INFO "Performing 'open' operation\n");
	dev = container_of(inode->i_cdev, struct hello_dev, cdev);
	filp->private_data = dev; /* for other methods */

	return 0; /* success */
}

// Release function
static int hello_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "Prforming 'release' operation\n");
	return 0;
}

void hello_cleanup_module(void)
{
	dev_t devno = MKDEV(hello_major, hello_minor);
	cdev_del(&(hello_devices->cdev));
	/* freeing the memory */
	if ((hello_devices->p_data) != 0) {
		kfree(hello_devices->p_data);
		printk(KERN_INFO "Kfree the string-memory\n");
	}
	if ((hello_devices) != 0) {
		kfree(hello_devices);
		printk(KERN_INFO "Kfree hello_devices\n");
	}
	unregister_chrdev_region(devno,
				 hello_nr_devs); /* unregistering device */
	printk(KERN_INFO "Cdev deleted, kfree, chdev unregistered\n");
}

static ssize_t hello_read(struct file *filp, char __user *buf, size_t count,
			  loff_t *f_pos)
{
	ssize_t retval;
	int len = 0;
	if (count > hello_devices->data_len) {
		len = hello_devices->data_len;
	} else {
		len = count;
	}

	int err = copy_to_user(buf, (void *)hello_devices->p_data, len);

	if (0 == err) {
		retval = len;
		printk(KERN_INFO "%u chars are sent to the user\n", len);

	} else {
		retval = -EPERM;
		printk(KERN_WARNING "Failed to send chars to the user!\n");
	}

	return retval;
}

static ssize_t hello_write(struct file *filp, const char __user *buf,
			   size_t count, loff_t *f_pos)
{
	int retval = 0;
	if (count > hello_devices->data_len) {
		printk(KERN_WARNING
		       "Hello: trying to write more than possible. Aborting write\n");
		retval = -EFBIG;

	} else if (copy_from_user((void *)hello_devices->p_data, buf, count)) {
		printk(KERN_WARNING "Hello: can't use copy_from_user. \n");
		retval = -EPERM;
	}

	return retval;
}

/*
 * Set up the char_dev structure for this device.
 */
static void hello_setup_cdev(struct hello_dev *dev)
{
	int err;
	int devno = MKDEV(hello_major, hello_minor);

	cdev_init(&dev->cdev, &hello_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &hello_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_WARNING "Error %d adding hello cdev_add", err);

	printk(KERN_INFO "cdev initialized\n");
}

module_init(hello_init);
module_exit(hello_exit);