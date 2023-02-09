#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEVICE_NAME	"woabchar"
#define CLASS_NAME	"woab"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("woab");
MODULE_DESCRIPTION("A simple Linux char driver");
MODULE_VERSION("0.1");

static int	majorNumber;
static char	message[256] = {0};
static short	size_of_message;
static int	numberOpens = 0;
static struct	class*  ebbcharClass  = NULL;
static struct	device* ebbcharDevice = NULL;

static int	dev_open(struct inode *, struct file *);
static int	dev_release(struct inode *, struct file *);
static ssize_t	dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t	dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops =
{
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};


static int __init woabchar_init(void)
{
	printk(KERN_INFO "WOABchar: Initializing the WOABchar LKM\n");

	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber < 0){
		printk(KERN_ALERT "WOABchar failed to register a major number\n");
		return majorNumber;
	}
	printk(KERN_INFO "WOABchar: registered correctly with major number %d\n", majorNumber);

	ebbcharClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(ebbcharClass)) {
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "Failed to register device class\n");
		return PTR_ERR(ebbcharClass);
	}
	printk(KERN_INFO "WOABchar: device class registered correctly\n");

	ebbcharDevice = device_create(ebbcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if (IS_ERR(ebbcharDevice)) {
		class_destroy(ebbcharClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(ebbcharDevice);
	}
	printk(KERN_INFO "WOABchar: device class created correctly\n");
	return 0;
}

static void __exit woabchar_exit(void)
{
	device_destroy(ebbcharClass, MKDEV(majorNumber, 0));
	class_destroy(ebbcharClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);
	printk(KERN_INFO "WOABchar: Goodbye from the LKM!\n");
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	numberOpens++;
	printk(KERN_INFO "WOABchar: Device has been opened %d time(s)\n", numberOpens);
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	int error_count = 0;
	
	error_count = copy_to_user(buffer, message, size_of_message);

	if (0 == error_count) {
	  printk(KERN_INFO "WOABchar: Sent %d characters to the user\n", size_of_message);
	  return size_of_message;
	}
	else {
	  printk(KERN_INFO "WOABchar: Failed to send %d characters to the user\n", error_count);
	  return -EFAULT;
	}
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	int uncopied;
	size_of_message = len < sizeof(message) ? len : sizeof(message);
	uncopied = copy_from_user(message, buffer, size_of_message);
	printk(KERN_INFO "WOABchar: Received %zu (%lu uncopied) characters from the user"
			", size_of_message = %d\n", len, 
			uncopied + (len < sizeof(message) ? 0 : len - sizeof(message)),
			size_of_message);
	return len;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "WOABchar: Device successfully closed\n");
	return 0;
}

module_init(woabchar_init);
module_exit(woabchar_exit);