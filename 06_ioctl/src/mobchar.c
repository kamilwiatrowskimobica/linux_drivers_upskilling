/**
 * @file    main.c
 * @author  Drost Krzysztof
 * @date    10 February 2023
 * @version 0.1
 * @brief   Simple character driver with support for read and write to device.
 *          Tested on linux version: 5.15.0-58-generic
 */

#include "mobchar.h"
#include "mobchar_ioctl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Drost Krzysztof");
MODULE_DESCRIPTION(
	"Simple character driver with support for read and write to device.");
MODULE_VERSION("0.1");

struct mobchar_dev *mobchar_devices;

static int mobchar_major = 0;
static int mobchar_minor = 0;
static unsigned int mobchar_nr_devs = 1;
static int device_max_size = DEVICE_MAX_SIZE;

static char message[BUFFER_MAX_SIZE] = { 0 };
static short meseageSize;
static int opensCounts = 0;

static int mobchar_dev_open(struct inode *, struct file *);
static int mobchar_dev_release(struct inode *, struct file *);
static ssize_t mobchar_dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t mobchar_dev_write(struct file *, const char *, size_t, loff_t *);
static long mobchar_dev_ioctl(struct file *filep, unsigned int cmd,
			      unsigned long arg);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations mobchar_fops = {
	.owner = THIS_MODULE,
	.open = mobchar_dev_open,
	.read = mobchar_dev_read,
	.write = mobchar_dev_write,
	.release = mobchar_dev_release,
	.unlocked_ioctl = mobchar_dev_ioctl,
};

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void mobchar_cleanup_module(void)
{
	dev_t devno = MKDEV(mobchar_major, mobchar_minor);
	cdev_del(&(mobchar_devices->cdev));
	/* freeing the memory */
	if ((mobchar_devices->p_data) != 0) {
		kfree(mobchar_devices->p_data);
		logk(KERN_INFO, "kfree the string-memory\n");
	}
	if ((mobchar_devices) != 0) {
		kfree(mobchar_devices);
		logk(KERN_INFO, "kfree hello_devices\n");
	}
	unregister_chrdev_region(devno, mobchar_nr_devs);
	logk(KERN_INFO, "cdev deleted, kfree, chdev unregistered\n");
}

/** @brief The LKM initialization function
 *  @return returns 0 if successful
 */
static void mobchar_setup_cdev(struct mobchar_dev *dev, int index)
{
	int error, devno = MKDEV(mobchar_major, mobchar_minor + index);
	cdev_init(&dev->cdev, &mobchar_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &mobchar_fops;
	error = cdev_add(&dev->cdev, devno, 1);
	if (error) {
		logk(KERN_NOTICE, "Error %d adding mobchar%d \n", error, index);
	}

	logk(KERN_INFO, "Device class created correctly\n");
}

/** @brief The LKM initialization function
 *  @return returns 0 if successful
 */
static int __init mobchar_init(void)
{
	int result = 0;
	dev_t dev = 0;

	logk(KERN_INFO, "dynamic allocation of major number\n");
	result = alloc_chrdev_region(&dev, mobchar_minor, mobchar_nr_devs,
				     "mobchar");
	mobchar_major = MAJOR(dev);

	if (result < 0) {
		logk(KERN_WARNING, "can't get major %d\n", mobchar_major);
		return result;
	}

	// init device
	mobchar_devices = kmalloc(mobchar_nr_devs * sizeof(struct mobchar_dev),
				  GFP_KERNEL);
	if (!mobchar_devices) {
		result = -ENOMEM;
		logk(KERN_WARNING, "ERROR kmalloc dev struct\n");
		goto fail;
	}
	memset(mobchar_devices, 0,
	       mobchar_nr_devs * sizeof(struct mobchar_dev));

	// init buffer
	mobchar_devices->p_data =
		(char *)kmalloc(device_max_size * sizeof(char), GFP_KERNEL);
	if (!mobchar_devices->p_data) {
		result = -ENOMEM;
		logk(KERN_WARNING, "ERROR kmalloc p_data\n");
		goto fail;
	}
	// semaphore init
	sema_init(&(mobchar_devices->sem), 1);
	mobchar_setup_cdev(mobchar_devices, 0);

	mobchar_devices->busy_read_count = 0;
	mobchar_devices->busy_write_count = 0;
	logk(KERN_INFO, "Init values. Busy read: %d. Busy write: %d\n",
	     mobchar_devices->busy_read_count,
	     mobchar_devices->busy_write_count);
	logk(KERN_INFO, "Init done correctly\n");
	return 0;

fail:
	mobchar_cleanup_module();
	return result;
}

/** @brief The LKM cleanup function
 */
static void __exit mobchar_exit(void)
{
	logk(KERN_INFO, "Exit function start.\n");
	logk(KERN_INFO, "Busy read: %d. Busy write: %d\n",
	     mobchar_devices->busy_read_count,
	     mobchar_devices->busy_write_count);
	mobchar_cleanup_module();
	logk(KERN_INFO, "Exit function end.\n");
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the opensCounts counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int mobchar_dev_open(struct inode *inodep, struct file *filep)
{
	struct mobchar_dev *dev;
	logk(KERN_INFO, "performing 'open' operation\n");
	dev = container_of(inodep->i_cdev, struct mobchar_dev, cdev);
	filep->private_data = dev;
	opensCounts++;
	logk(KERN_INFO, "Device has been opened %d time(s)\n", opensCounts);

	return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t mobchar_dev_read(struct file *filep, char *buffer, size_t len,
				loff_t *offset)
{
	int error_count = 0;

	if (down_trylock(&(mobchar_devices->sem))) {
		mobchar_devices->busy_read_count++;
		logk(KERN_WARNING, "Trying to read. Count [%d].\n",
		     mobchar_devices->busy_read_count);
		logk(KERN_WARNING, "Operation aborted\n");
		return -ERESTARTSYS;
	}

	error_count = copy_to_user(buffer, message, meseageSize);

	if (error_count == 0) {
		logk(KERN_INFO, "Sent %d characters to the user\n",
		     meseageSize);
		up(&(mobchar_devices->sem));
		return (meseageSize = 0);
	} else {
		logk(KERN_INFO, "Failed to send %d characters to the user\n",
		     error_count);
		up(&(mobchar_devices->sem));
		return -EFAULT;
	}
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t mobchar_dev_write(struct file *filep, const char *buffer,
				 size_t len, loff_t *offset)
{
	int error_count = 0;

	if (down_trylock(&(mobchar_devices->sem))) {
		mobchar_devices->busy_write_count++;
		logk(KERN_WARNING, "Trying to write. Count [%d].\n",
		     mobchar_devices->busy_write_count);
		logk(KERN_WARNING, "Operation aborted\n");
		return -ERESTARTSYS;
	}
	error_count = copy_from_user(message, buffer, len);

	if (error_count == 0) {
		meseageSize = strlen(message);
		logk(KERN_INFO, "Received %zu characters from the user\n", len);
		up(&(mobchar_devices->sem));
		return len;
	} else {
		logk(KERN_INFO, "Failed to receive characters from the user\n");
		up(&(mobchar_devices->sem));
		return -EFAULT;
	}
	return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int mobchar_dev_release(struct inode *inodep, struct file *filep)
{
	logk(KERN_INFO, "Device successfully closed\n");
	return 0;
}

static long mobchar_dev_ioctl(struct file *filep, unsigned int cmd,
			      unsigned long arg)
{
	int err = 0;
	int retval = 0;
	struct mobchar_dev *mobchar_dev =
		(struct mobchar_dev *)filep->private_data;

	if (_IOC_TYPE(cmd) != MOBCHAR_IOC_MAGIC) {
		DEBUG_PRINT("_IOC_TYPE(cmd): %u\n", _IOC_TYPE(cmd));
		DEBUG_PRINT("MOBCHAR_IOC_MAGIC: %u\n", MOBCHAR_IOC_MAGIC);
		printk(KERN_WARNING
		       "_IOC_TYPE(cmd) != MOBCHAR_IOC_MAGIC => false. Aborting ioctl\n");
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) > MOBCHAR_IOC_MAXNR) {
		printk(KERN_WARNING
		       "_IOC_NR(cmd) > MOBCHAR_IOC_MAXNR => false. Aborting ioctl\n");
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	switch (cmd) {
	case MOBCHAR_IOC_HELLO:
		DEBUG_PRINT("Hello from kernel\n");
		break;
	case MOBCHAR_IOC_READ:
		// copy to user
		DEBUG_PRINT("IOCTL read\n");
		put_user(mobchar_dev->ioctl_var, (int *)arg);
		DEBUG_PRINT("Value: %d\n", mobchar_dev->ioctl_var);
		break;
	case MOBCHAR_IOC_WRITE:
		int var_from_user;
		// copy from user
		DEBUG_PRINT("IOCTL write\n");
		get_user(var_from_user, (int *)arg);
		DEBUG_PRINT("Value: %d\n", mobchar_dev->ioctl_var);
		mobchar_dev->ioctl_var = var_from_user;
		break;
	}
	return retval;
}

module_init(mobchar_init);
module_exit(mobchar_exit);
