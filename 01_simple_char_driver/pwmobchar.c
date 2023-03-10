/**
 * @file   pwmobChar.c
 * @author Piotr Wozniak
 * @date   9.03.2023
 * @version 0.1
 * @brief   Linux driver to return characters
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#define DEVICE_NAME "pwmobChar"
#define CLASS_NAME "pwmob"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Piotr Wozniak");
MODULE_DESCRIPTION("Linux char driver");
MODULE_VERSION("1");

static DEFINE_MUTEX(pwmobChar_mutex);

static int majorNumber;
static char message[256] = { 0 };
static short size_of_message;
static int numberOpens = 0;
static struct class *pwmobCharClass = NULL;
static struct device *pwmobCharDevice = NULL;

/*declarations*/
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

static int __init pwmobChar_init(void)
{
	printk(KERN_INFO "pwmobChar: Initializing the pwmobChar LKM\n");

	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber < 0) {
		printk(KERN_ALERT
		       "pwmobChar failed to register a major number\n");
		return majorNumber;
	}
	printk(KERN_INFO
	       "pwmobChar: registered correctly with major number %d\n",
	       majorNumber);

	pwmobCharClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(pwmobCharClass)) {
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "Failed to register device class\n");
		return PTR_ERR(pwmobCharClass);
	}
	printk(KERN_INFO "pwmobChar: device class registered correctly\n");

	pwmobCharDevice = device_create(
		pwmobCharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if (IS_ERR(pwmobCharDevice)) {
		class_destroy(pwmobCharClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(pwmobCharDevice);
	}
	mutex_init(&pwmobChar_mutex);
	printk(KERN_INFO "pwmobChar: device class created correctly\n");
	return 0;
}

static void __exit pwmobChar_exit(void)
{
	mutex_destroy(&pwmobChar_mutex);
	device_destroy(pwmobCharClass, MKDEV(majorNumber, 0));
	class_unregister(pwmobCharClass);
	class_destroy(pwmobCharClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);
	printk(KERN_INFO "pwmobChar: Goodbye from the LKM!\n");
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	if (!mutex_trylock(&pwmobChar_mutex)) {
		printk(KERN_ALERT
		       "pwmobChar: Device in use by another process");
		return -EBUSY;
	}
	numberOpens++;
	printk(KERN_INFO "pwmobChar: Device has been opened %d time(s)\n",
	       numberOpens);
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len,
			loff_t *offset)
{
	int error_count = 0;
	error_count = copy_to_user(buffer, message, size_of_message);

	if (error_count == 0) {
		printk(KERN_INFO "pwmobChar: Sent %d characters to the user\n",
		       size_of_message);
		return (size_of_message = 0);
	} else {
		printk(KERN_INFO
		       "pwmobChar: Failed to send %d characters to the user\n",
		       error_count);
		return -EFAULT;
	}
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len,
			 loff_t *offset)
{
	sprintf(message, "%s(%zu letters)", buffer, len);
	size_of_message = strlen(message);
	printk(KERN_INFO "pwmobChar: Received %zu characters from the user\n",
	       len);
	return len;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	mutex_unlock(&pwmobChar_mutex);
	printk(KERN_INFO "pwmobChar: Device successfully closed\n");
	return 0;
}

module_init(pwmobChar_init);
module_exit(pwmobChar_exit);
