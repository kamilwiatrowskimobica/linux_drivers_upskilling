#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h> // Required for the copy to user function
#include <linux/device.h> //device_create
//#include <linux/device/class.h> //class_create

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mateusz Leszczynski, Mobica Ltd.");
MODULE_DESCRIPTION("A super-simple LKM.");
MODULE_VERSION("0.1");

#define DEVICE_NAME "hellodrvchar" /* device at /dev/hellodrvchar */
#define CLASS_NAME "hellodrv"
#define MESSAGE_LEN 256

static int majorNumber;
static char message[MESSAGE_LEN] = {
	0
}; ///< string that is passed from userspace
static short size_of_message; ///< size of the string stored
static int numberOpens = 0; ///< Counts the number of times the device is opened
static struct class *hellodrvcharClass = NULL;
static struct device *hellodrvcharDevice = NULL;

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

static int hello_init(void)
{
	long rc = 0;

	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber < 0) {
		printk(KERN_ALERT "Failed to register a major number\n");
		return majorNumber;
	}

	// Register the device class
	hellodrvcharClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(hellodrvcharClass)) {
		printk(KERN_ALERT "Failed to register device class\n");
		rc = PTR_ERR(
			hellodrvcharClass); // Correct way to return an error on a pointer
		goto err_class;
	}

	// Register the device driver
	hellodrvcharDevice = device_create(hellodrvcharClass, NULL,
					   MKDEV(majorNumber, 0), NULL,
					   DEVICE_NAME);
	if (IS_ERR(hellodrvcharDevice)) { // Clean up if there is an error
		printk(KERN_ALERT "Failed to create the device\n");
		rc = PTR_ERR(hellodrvcharDevice);
		goto err_dev;
	}
	printk(KERN_INFO "%s: device class created correctly\n",
	       DEVICE_NAME); // Made it! device was initialized

	printk(KERN_INFO "MODULE LOADED\n");
	return rc;

err_dev:
	device_destroy(hellodrvcharClass, MKDEV(majorNumber, 0));
err_class:
	class_destroy(hellodrvcharClass);
	//TODO is unregister_chrdev() unnecessary?
	unregister_chrdev(majorNumber, DEVICE_NAME);
	return rc;
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	numberOpens++;
	printk(KERN_INFO "%s: Device has been opened %d time(s)\n", DEVICE_NAME,
	       numberOpens);
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len,
			loff_t *offset)
{
	int error_count = 0;
	// copy_to_user has the format ( * to, *from, size) and returns 0 on success
	error_count = copy_to_user(buffer, message, size_of_message);

	if (error_count == 0) {
		printk(KERN_INFO "%s: Sent %d characters to the user\n",
		       DEVICE_NAME, size_of_message);
		return (size_of_message = 0);
	} else {
		printk(KERN_INFO
		       "%s: Failed to send %d characters to the user\n",
		       DEVICE_NAME, error_count);
		return -EFAULT;
	}
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len,
			 loff_t *offset)
{
	long left;

	// sanity check
	if (len >= MESSAGE_LEN) {
		printk(KERN_INFO
		       "%s: too long message from the user. Should be less than %u chars\n",
		       DEVICE_NAME, MESSAGE_LEN);
	}

	left = copy_from_user(message, buffer, len);
	if (left) {
		printk(KERN_INFO
		       "%s: problem on receiving characters from the user\n",
		       DEVICE_NAME);
	}

	size_of_message = strnlen(message, MESSAGE_LEN);
	printk(KERN_INFO "%s: Received %zu characters from the user\n",
	       DEVICE_NAME, len);
	printk(KERN_INFO "%s: Received message: %s\n", DEVICE_NAME, message);
	return len;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "%s: Device successfully closed\n", DEVICE_NAME);
	return 0;
}

static void __exit hello_exit(void)
{
	device_destroy(hellodrvcharClass,
		       MKDEV(majorNumber, 0)); // remove the device
	class_unregister(hellodrvcharClass); // unregister the device class
	class_destroy(hellodrvcharClass); // remove the device class
	//TODO is unregister_chrdev() unnecessary?
	unregister_chrdev(majorNumber,
			  DEVICE_NAME); // unregister the major number
	printk(KERN_INFO "MODULE UNLOADED\n");
}

module_init(hello_init);
module_exit(hello_exit);
