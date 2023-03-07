#include <linux/init.h> // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h> // Core header for loading LKMs into the kernel
#include <linux/device.h> // Header to support the kernel Driver Model
#include <linux/kernel.h> // Contains types, macros, functions for the kernel
#include <linux/fs.h> // Header for the Linux file system support
#include <linux/uaccess.h> // Required for the copy to user function

///< The device will appear at /dev/mob using this value
#define DEVICE_NAME "mob"
///< The device class -- this is a character device driver
#define CLASS_NAME "lukr"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lukasz Krakowiak");
MODULE_DESCRIPTION("A simple Linux char driver");
MODULE_VERSION("0.1");

#define MOB_MESSAGE_LEN 256

static int majorNumber; ///< Stores the device number -- determined automatically
///< Memory for the string that is passed from userspace
static char modouleMessage[MOB_MESSAGE_LEN] = { 0 };
///< Used to remember the size of the string stored
static short size_of_message;
///< Counts the number of times the device is opened
static int numberOpens = 0;
///< The device-driver class struct pointer
static struct class *MobClass = NULL;
///< The device-driver device struct pointer
static struct device *MobDevice = NULL;

// The prototype functions for the character driver -- must come before the struct definition
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

/// A macro that is used to declare a new mutex that is visible in this file
/// results in a semaphore variable mob_mutex with value 1 (unlocked)
/// DEFINE_MUTEX_LOCKED() results in a variable with value 0 (locked)
static DEFINE_MUTEX(mob_mutex);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops = {
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

static int __init mob_init(void)
{
	printk(KERN_INFO "MOBChar: Initializing mob LKM\n");

	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber < 0) {
		printk(KERN_ALERT "mob failed to register a major number\n");
		return majorNumber;
	}
	printk(KERN_INFO "MOBChar: registered correctly with major number %d\n",
	       majorNumber);

	MobClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(MobClass)) {
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "Failed to register device class\n");
		return PTR_ERR(MobClass);
	}
	printk(KERN_INFO "MOBChar: device class registered correctly\n");

	// Register the device driver
	MobDevice = device_create(MobClass, NULL, MKDEV(majorNumber, 0), NULL,
				  DEVICE_NAME);
	if (IS_ERR(MobDevice)) {
		class_destroy(MobClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(MobDevice);
	}
	/// Initialize the mutex lock dynamically at runtime
	mutex_init(&mob_mutex);
	printk(KERN_INFO "MOBChar: device class created correctly\n");
	return 0;
}

static void __exit mob_exit(void)
{
	device_destroy(MobClass, MKDEV(majorNumber, 0)); // remove the device
	class_unregister(MobClass); // unregister the device class
	class_destroy(MobClass); // remove the device class
	// unregister the major number
	unregister_chrdev(majorNumber, DEVICE_NAME);
	mutex_destroy(&mob_mutex); /// destroy the dynamically-allocated mutex
	printk(KERN_INFO "MOBChar: Goodbye from the LKM!\n");
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	if (!mutex_trylock(&mob_mutex)) {
		printk(KERN_ALERT "MOBChar: Device in use by another process");
		return -EBUSY;
	}
	numberOpens++;
	printk(KERN_INFO "MOBChar: Device has been opened %d time(s)\n",
	       numberOpens);
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len,
			loff_t *offset)
{
	int error_count = 0;
	if (len < size_of_message) {
		printk(KERN_ALERT
		       "MOBChar: read failed. User buffer to small\n");
		return -ENOBUFS;
	}
	error_count = copy_to_user(buffer, modouleMessage, size_of_message);

	if (error_count == 0) {
		printk(KERN_INFO "MOBChar: Sent %d characters to the user\n",
		       size_of_message);
		// clear the position to the start and return 0
		return (size_of_message = 0);
	} else {
		printk(KERN_ALERT
		       "MOBChar: Failed to send %d characters to the user\n",
		       error_count);
		return -EFAULT;
	}
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len,
			 loff_t *offset)
{
	if (len > MOB_MESSAGE_LEN) {
		printk(KERN_ALERT "MOBChar: User buffer to big\n");
		return -EMSGSIZE;
	}
	copy_from_user(modouleMessage, buffer, len);
	// save length of the msg
	size_of_message = strlen(modouleMessage);
	printk(KERN_INFO "MOBChar: Received %zu characters from the user\n",
	       len);
	return len;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	mutex_unlock(&mob_mutex);
	printk(KERN_INFO "MOBChar: Device successfully closed\n");
	return 0;
}

module_init(mob_init);
module_exit(mob_exit);