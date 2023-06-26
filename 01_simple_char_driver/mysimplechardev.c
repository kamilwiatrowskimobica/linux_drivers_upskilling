/**
 * @file    mysimplechar.c
 * @author  Andrzej Dziarnik
 * @date    20.06.2023
 * @version 0.1
 * @brief  Simple character driver created for training purposes
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "mysimplechardev"
#define CLASS_NAME "ldd"
#define MSG_MAX_SIZE 256

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrzej Dziarnik");
MODULE_DESCRIPTION("A simple character driver.");
MODULE_VERSION("0.1");

static int major_number;
static char message[MSG_MAX_SIZE] = { 0 };
static short size_of_message;
static int number_opens = 0;
static struct class *my_class = NULL;
static struct device *my_device = NULL;

static int dev_open(struct inode *inodep, struct file *filep)
{
	number_opens++;
	printk(KERN_INFO "SCHD: Device has been opened %d time(s)\n",
	       number_opens);
	return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "SCHD: Device successfully closed\n");
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len,
			loff_t *offset)
{
	int count = 0;
	if (len > MSG_MAX_SIZE || len < size_of_message) {
		printk(KERN_WARNING "SCHD: wrong buffer size. Aborting read\n");
		return -EFBIG;
	}
	count = copy_to_user(buffer, message, size_of_message);
	if (count == 0) {
		printk(KERN_INFO "SCHD: Sent %d characters to the user\n",
		       size_of_message);
		count = size_of_message;
		size_of_message = 0;
		return count;
	}
	printk(KERN_INFO "SCHD: Failed to send %d characters to the user\n",
	       count);
	return -EFAULT;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len,
			 loff_t *offset)
{
	int count = 0;
	if (len > MSG_MAX_SIZE) {
		printk(KERN_WARNING
		       "SCHD: wrong buffer size %zu. Aborting write\n",
		       len);
		return -EFBIG;
	}
	count = copy_from_user(message, buffer, len);
	if (count != 0) {
		printk(KERN_INFO
		       "SCHD: Failed to read %d characters from user\n",
		       count);
		return -EFAULT;
	}
	size_of_message = len;
	printk(KERN_INFO "SCHD: Received %zu characters from the user\n", len);
	return len;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = dev_open,
	.release = dev_release,
	.read = dev_read,
	.write = dev_write,
};

static int __init mysimplechardev_init(void)
{
	printk(KERN_INFO "SCHD: My simple character driver init\n");
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0) {
		printk(KERN_ALERT "SCHD: Failed to register a major number\n");
		return major_number;
	}
	my_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(my_class)) {
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "SCHD: Failed to create device class\n");
		return PTR_ERR(my_class);
	}
	my_device = device_create(my_class, NULL, MKDEV(major_number, 0), NULL,
				  DEVICE_NAME);
	if (IS_ERR(my_device)) {
		class_destroy(my_class);
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(my_device);
	}
	return 0;
}

static void __exit mysimplechardev_exit(void)
{
	printk(KERN_INFO "SCHD: My simple character driver exit\n");
	device_destroy(my_class, MKDEV(major_number, 0));
	class_destroy(my_class);
	unregister_chrdev(major_number, DEVICE_NAME);
}

module_init(mysimplechardev_init);
module_exit(mysimplechardev_exit);