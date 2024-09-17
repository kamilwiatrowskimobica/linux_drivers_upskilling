/**
 * @file   mk_mob.c
 * @author Michal Kozikowski
 * @date   11 June 2024
 * @version 0.1
 * @brief   A simple character driver using dynamically allocated memory. 
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

static struct mk_mob_dev *mk_mob_device; ///< allocated in mk_mob_init_module
static struct class *mk_mob_class = NULL;

// The prototype functions for the character driver -- must come before the struct definition
static int mk_mob_dev_open(struct inode *, struct file *);
static int mk_mob_dev_release(struct inode *, struct file *);
static ssize_t mk_mob_dev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t mk_mob_dev_write(struct file *, const char __user *, size_t, loff_t *);

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
};

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
static void mk_mob_cleanup_module(void)
{
	dev_t devno = MKDEV(majorNumber, minorNumberStart);

	/* Get rid of our char dev entries */
	if (mk_mob_device) {
		struct my_list *cursor, *temp;

		list_for_each_entry_safe(cursor, temp, &mk_mob_device->head->list, list) {
			list_del(&cursor->list);
			kfree(cursor);
		}

		device_destroy(mk_mob_class,
					mk_mob_device->cdev.dev);
		cdev_del(&mk_mob_device->cdev);
		kfree(mk_mob_device);
	}

	class_destroy(mk_mob_class);

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, 1);
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

	err = cdev_add(&dev->cdev, devno, 1);

	if (err) {
		printk(KERN_NOTICE "Error %d adding mk_mob device %zu", err,
		       index);
		return;
	}

	dev->head = kzalloc(sizeof(struct my_list), GFP_KERNEL);
	INIT_LIST_HEAD(&dev->head->list);

	dev->head->data = 0;

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
	dev_t dev = 0;

	printk(KERN_INFO "mk_mob: Initializing the mk_mob module LKM.\n");

	int result = alloc_chrdev_region(&dev, minorNumberStart, 1, "mk_mob");
	majorNumber = MAJOR(dev);

	if (result < 0) {
		printk(KERN_WARNING
		       "mk_mob: Can't get major number for devices.\n");
		return result;
	}

	/* Allocate the devices. */
	mk_mob_device = kmalloc(sizeof(struct mk_mob_dev), GFP_KERNEL);
	if (!mk_mob_device) {
		result = -ENOMEM;
		goto fail;
	}

	memset(mk_mob_device, 0, sizeof(struct mk_mob_dev));

	mk_mob_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(mk_mob_class)) {
		pr_warn("Failed to create class\n");
		result = PTR_ERR(mk_mob_class);
		goto fail;
	}

	/* Initialize each device. */
	sema_init(&mk_mob_device->sem, 1); // init as mutex
	mk_mob_setup_cdev(mk_mob_device, 0);

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

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	// read entire list or nothing at all
	if (len < PAGE_SIZE || *offset != 0) {
		goto out;
	}

	char *retBuffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
	
	if (!retBuffer) {
        retval = -ENOMEM;
        goto out;
    }
	
	len = 0;
	struct my_list *cursor;
	int cnt = 1;
	list_for_each_entry(cursor, &dev->head->list, list) {
		if (len + strlen("Node 100, value: 100\n") > PAGE_SIZE)
			break;

		len += sprintf(retBuffer + len, "Node %d, value: %d \n", cnt, cursor->data);

		cnt++;
    }

	if (copy_to_user(buffer, retBuffer, len)) {
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
	if (retBuffer) {
		kfree(retBuffer);
	}
	up(&dev->sem);
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

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	struct my_list *temp_node = NULL;
	
	/*Creating Node*/
	temp_node = kzalloc(sizeof(struct my_list), GFP_KERNEL);
	if (!temp_node) {
        goto out;
    }

	printk(KERN_INFO "Adding node\n");

	// /*Init the list within the struct*/
	INIT_LIST_HEAD(&temp_node->list);

	/*Add Node to Linked List*/
	list_add_tail(&temp_node->list, &dev->head->list);

	temp_node->data = list_entry(temp_node->list.prev, struct my_list, list)->data + 1;

out:
	up(&dev->sem);
	return retval;
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