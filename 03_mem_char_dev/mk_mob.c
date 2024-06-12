/**
 * @file   mk_mob.c
 * @author Michal Kozikowski
 * @date   11 June 2024
 * @version 0.1
 * @brief   A simple character driver using preallocated RAM memory. 
 * Implementation is based on the book "Linux Device Drivers" 
 * by Alessandro Rubini and Jonathan Corbet, published by O'Reilly & Associates.
 */

#include "mk_mob.h"

#include <linux/init.h> // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h> // Core header for loading LKMs into the kernel
#include <linux/kernel.h> // Contains types, macros, functions for the kernel
#include <linux/fs.h> // Header for the Linux file system support
#include <linux/slab.h>
#include <linux/uaccess.h> // Required for the copy to user function

#define DEVICE_NAME "mk_mob" ///< The device will appear at /dev/mk_mob using this value
#define CLASS_NAME  "mob" ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL"); ///< The license type -- this affects available functionality
MODULE_AUTHOR("Michal Kozikowski"); ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple Linux char driver"); ///< The description -- see modinfo
MODULE_VERSION("0.1"); ///< A version number to inform users

static int majorNumber; ///< Stores the device number -- determined automatically
static const int minorNumberStart = 0; ///< Minor number of first device
static const int nr_devs = 4; ///< Number of bare devices

static struct mk_mob_dev *mk_mob_devices; ///< allocated in mk_mob_init_module
static struct class *mk_mob_class = NULL;

// The prototype functions for the character driver -- must come before the struct definition
static int mk_mob_dev_open(struct inode *, struct file *);
static int mk_mob_dev_release(struct inode *, struct file *);
static ssize_t mk_mob_dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t mk_mob_dev_write(struct file *, const char *, size_t, loff_t *);
static loff_t mk_mob_dev_llseek(struct file *filp, loff_t off, int whence);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = mk_mob_dev_open,
	.read = mk_mob_dev_read,
	.write = mk_mob_dev_write,
	.release = mk_mob_dev_release,
	.llseek = mk_mob_dev_llseek,
};

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
static void mk_mob_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(majorNumber, minorNumberStart);

	/* Get rid of our char dev entries */
	if (mk_mob_devices) {
		for (i = 0; i < nr_devs; i++) {
			if (mk_mob_devices[i].data) {
				kfree(mk_mob_devices[i].data);
			}
			device_destroy(mk_mob_class,
				       mk_mob_devices[i].cdev.dev);
			cdev_del(&mk_mob_devices[i].cdev);
		}
		kfree(mk_mob_devices);
	}

	class_destroy(mk_mob_class);

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, nr_devs);
}

/*
 * Set up the char_dev structure for this device.
 */
static void mk_mob_setup_cdev(struct mk_mob_dev *dev, size_t index)
{
	struct device *mob_device = NULL;
	int err, devno = MKDEV(majorNumber, minorNumberStart + index);

	cdev_init(&dev->cdev, &fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &fops;

	dev->data = kmalloc(MK_DEV_MEMALLOC_MAX_SIZE, GFP_KERNEL);
	dev->size = 0;

	/* Check if we fail memory allocation for the device */
	if (dev->data == NULL) {
		return;
	}

	err = cdev_add(&dev->cdev, devno, 1);

	if (err) {
		printk(KERN_NOTICE "Error %d adding mk_mob device %zu", err,
		       index);
		return;
	}

	mob_device = device_create(mk_mob_class, NULL, devno, NULL, "%s_%zu",
				   DEVICE_NAME, index);

	if (IS_ERR(mob_device)) {
		pr_warn("failed to create device\n");
	}
}

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init mk_mob_init(void)
{
	int result, i;
	dev_t dev = 0;

	printk(KERN_INFO "mk_mob: Initializing the mk_mob module LKM.\n");

	result = alloc_chrdev_region(&dev, minorNumberStart, nr_devs, "mk_mob");
	majorNumber = MAJOR(dev);

	if (result < 0) {
		printk(KERN_WARNING
		       "mk_mob: Can't get major number for devices.\n");
		return result;
	}

	/* Allocate the devices. */
	mk_mob_devices =
		kmalloc(nr_devs * sizeof(struct mk_mob_dev), GFP_KERNEL);
	if (!mk_mob_devices) {
		result = -ENOMEM;
		goto fail;
	}

	memset(mk_mob_devices, 0, nr_devs * sizeof(struct mk_mob_dev));

	mk_mob_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(mk_mob_class)) {
		pr_warn("Failed to create class\n");
		result = PTR_ERR(mk_mob_class);
		goto fail;
	}

	/* Initialize each device. */
	for (i = 0; i < nr_devs; i++) {
		mk_mob_setup_cdev(&mk_mob_devices[i], i);
	}

	return 0;

fail:
	mk_mob_cleanup_module();
	return result;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit mk_mob_exit(void)
{
	mk_mob_cleanup_module();
	printk(KERN_INFO "mk_mob: Goodbye from the LKM!\n");
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inode A pointer to an inode object (defined in linux/fs.h)
 *  @param filp A pointer to a file object (defined in linux/fs.h)
 */
static int mk_mob_dev_open(struct inode *inode, struct file *filp)
{
	struct mk_mob_dev *dev; /* device information */

	dev = container_of(inode->i_cdev, struct mk_mob_dev, cdev);
	filp->private_data = dev; /* for other methods */

	printk(KERN_INFO "mk_mob: device opened!\n");

	return 0; /* success */
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user.
 *  @param filp A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t mk_mob_dev_read(struct file *filp, char __user *buffer,
			       size_t len, loff_t *offset)
{
	ssize_t retval = 0;
	struct mk_mob_dev *dev = (struct mk_mob_dev *)filp->private_data;

	if (*offset >= dev->size)
		goto out;
	if (*offset + len > dev->size)
		len = dev->size - *offset;

	if (copy_to_user(buffer, dev->data + *offset, len)) {
		retval = -EFAULT;
		printk(KERN_INFO
		       "mk_mob: Failed to send %zu characters to the user\n",
		       len);
		goto out;
	}

	printk(KERN_INFO "mk_mob: Sent %zu characters to the user\n", len);
	*offset += len;
	retval = len;

out:
	return retval;
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t mk_mob_dev_write(struct file *filp, const char __user *buffer,
				size_t len, loff_t *offset)
{
	struct mk_mob_dev *dev = (struct mk_mob_dev *)filp->private_data;
	ssize_t retval = -ENOMEM; /* value used in "goto out" statements */

	if (*offset >= MK_DEV_MEMALLOC_MAX_SIZE)
		goto out;
	if (*offset + len > MK_DEV_MEMALLOC_MAX_SIZE)
		len = MK_DEV_MEMALLOC_MAX_SIZE - *offset;

	if (copy_from_user(dev->data + *offset, buffer, len)) {
		printk(KERN_INFO
		       "mk_mob: Failed to receive %zu characters to the user\n",
		       len);
		retval = -EFAULT;
		goto out;
	}

	printk(KERN_INFO "mk_mob: Received %zu characters from the user\n",
	       len);
	*offset += len;
	retval = len;
	dev->size = *offset;
out:
	return retval;
}

static loff_t mk_mob_dev_llseek(struct file *filp, loff_t off, int whence)
{
	struct mk_mob_dev *dev = (struct mk_mob_dev *)filp->private_data;
	loff_t npos;

	pr_info("mk_mob_dev_llseek, whence = %d, off = %lld\n", whence, off);

	switch (whence) {
	case SEEK_SET:
		if (off >= MK_DEV_MEMALLOC_MAX_SIZE)
			return -EFBIG;
		npos = off;
		break;

	case SEEK_CUR:
		if (filp->f_pos + off >= MK_DEV_MEMALLOC_MAX_SIZE)
			return -EFBIG;
		npos = filp->f_pos + off;
		break;

	case SEEK_END:
		if (dev->size + off >= MK_DEV_MEMALLOC_MAX_SIZE)
			return -EFBIG;
		npos = dev->size + off;
		break;

	default: /* not expected value */
		return -EINVAL;
	}
	if (npos < 0)
		return -EINVAL;
	filp->f_pos = npos;
	return npos;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int mk_mob_dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "mk_mob: Device successfully closed\n");
	return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(mk_mob_init);
module_exit(mk_mob_exit);