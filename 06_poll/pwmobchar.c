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
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include "pwmobchar.h"

#define DEVICE_NAME "pwmobChar"
#define CLASS_NAME "pwmob"
#define NUMBER_OF_DEVICES 5

#define PWMOBDEV_IOC_MAGIC 'a'
//ioctl values for write and read
#define WR_VALUE _IOW(PWMOBDEV_IOC_MAGIC, 1, char *)
#define RD_VALUE _IOR(PWMOBDEV_IOC_MAGIC, 2, char *)
#define MOBDEV_IOC_RESET _IO(PWMOBDEV_IOC_MAGIC, 0)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Piotr Wozniak");
MODULE_DESCRIPTION("Linux char driver");
MODULE_VERSION("1");

wait_queue_head_t wait_queue;
static bool can_write = false;
static bool can_read  = false;

static int majorNumber;

static int numberOpens = 0;
static struct class *pwmobCharClass = NULL;
static struct pwmob_dev *pwmobCharDevices = NULL;
static uint dev_count = NUMBER_OF_DEVICES;
char string[255] = { 0 };

/*declarations*/
static int pwmob_dev_open(struct inode *, struct file *);
static int pwmob_dev_release(struct inode *, struct file *);
static ssize_t pwmob_dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t pwmob_dev_write(struct file *, const char *, size_t, loff_t *);
static loff_t pwmob_dev_llseek(struct file *, loff_t, int);
static long pwmob_dev_ioctl(struct file *, unsigned int, unsigned long);
static unsigned int pwmmob_dev_poll(struct file *, poll_table *);

static struct file_operations fops = { .open = pwmob_dev_open,
				       .read = pwmob_dev_read,
				       .write = pwmob_dev_write,
				       .release = pwmob_dev_release,
				       .llseek = pwmob_dev_llseek,
				       .unlocked_ioctl = pwmob_dev_ioctl,
				       .poll = pwmmob_dev_poll };

static unsigned int pwmmob_dev_poll(struct file *file, poll_table *wait)
{
	__poll_t mask = 0;

	poll_wait(file, &wait_queue, wait);
	printk("Poll function\n");

	if( can_read )
	{
		can_read = false;
		mask |= POLLIN | POLLRDNORM;
	}

	if( can_write )
	{
		can_write = false;
		mask |= POLLOUT | POLLWRNORM;
	}

	return mask;
}

static int pwmob_dev_open(struct inode *inodep, struct file *filep)
{
	struct pwmob_dev *pwmdev;
	pwmdev = container_of(inodep->i_cdev, struct pwmob_dev, cdev);
	filep->private_data = pwmdev; /* for other methods */

	printk(KERN_INFO "Device opened %d time(s)\n", ++numberOpens);
	return 0;
}
static int pwmob_dev_init_cdev(struct pwmob_dev *pwmdev, size_t index)
{
	int ret = 0;
	dev_t dev = MKDEV(majorNumber, index);
	struct device *pwmob_dev = NULL;

	cdev_init(&pwmdev->cdev, &fops);
	pwmdev->cdev.owner = THIS_MODULE;

	pwmdev->p_data = kcalloc(1, PWMOB_DATA_MAX, GFP_KERNEL);
	if (IS_ERR(pwmdev->p_data)) {
		printk(KERN_ALERT "failed to alloc p_data\n");
		return PTR_ERR(pwmdev->p_data);
	}

	ret = cdev_add(&pwmdev->cdev, dev, 1);
	if (ret < 0) {
		printk(KERN_ALERT "failed to add cdev\n");
		return ret;
	}

	pwmob_dev = device_create(pwmobCharClass, NULL, dev, NULL, "%s_%zu",
				  DEVICE_NAME, index);
	if (IS_ERR(pwmob_dev)) {
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(pwmob_dev);
	}

	printk(KERN_INFO "created %s_%d\n", DEVICE_NAME, MINOR(dev));

	return 0;
}
static void pwmmob_dev_clean_cdev(struct pwmob_dev *pwmdev)
{
	if (NULL == pwmdev)
		return;

	if (pwmdev->p_data)
		kfree(pwmdev->p_data);

	device_destroy(pwmobCharClass, pwmdev->cdev.dev);
	cdev_del(&pwmdev->cdev);
}

loff_t pwmob_dev_llseek(struct file *filep, loff_t offset, int whence)
{
	loff_t npos = 0;
	struct pwmob_dev *pwmdev = (struct pwmob_dev *)filep->private_data;

	printk(KERN_INFO "llseek, offset = %lld, whence = %d\n", offset,
	       whence);

	if (pwmdev == NULL) {
		printk(KERN_ALERT "%s error accessing mob_dev\n", __FUNCTION__);
		return -EFAULT;
	}

	if (pwmdev->p_data == NULL) {
		printk(KERN_ALERT "%s error accessing mob_dev msg\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	switch (whence) {
	case SEEK_SET:
		if (offset >= PWMOB_DATA_MAX)
			return -EFBIG;
		npos = offset;
		break;

	case SEEK_CUR:
		if (filep->f_pos + offset >= PWMOB_DATA_MAX)
			return -EFBIG;
		npos = filep->f_pos + offset;
		break;

	case SEEK_END:
		if (pwmdev->data_size + offset >= PWMOB_DATA_MAX)
			return -EFBIG;
		npos = pwmdev->data_size + offset;
		break;

	default:
		return -EINVAL;
	}

	if (npos < 0)
		return -EINVAL;
	filep->f_pos = npos;
	return npos;
}

static int __init pwmob_dev_init(void)
{
	int ret = 0;
	dev_t dev = 0;
	int i = 0;

	ret = alloc_chrdev_region(&dev, 0, dev_count, DEVICE_NAME);
	if (0 > ret) {
		printk(KERN_ALERT "Failed to alloc char device");
	}

	majorNumber = MAJOR(dev);

	pwmobCharClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(pwmobCharClass)) {
		printk(KERN_ALERT "pwmob: Failed to register device class\n");
		goto clean_class;
	}

	pwmobCharDevices =
		kcalloc(dev_count, sizeof(struct pwmob_dev), GFP_KERNEL);
	if (IS_ERR(pwmobCharDevices)) {
		printk(KERN_ALERT "pwmobCharDevices: Failed kcalloc\n");
		goto clean_device;
	}

	for (i = 0; i < dev_count; i++) {
		ret = pwmob_dev_init_cdev(&pwmobCharDevices[i], i);
		if (0 > ret) {
			goto cleanup;
		}
	}
	can_read = true;
	can_write = true;
	init_waitqueue_head(&wait_queue);
	printk(KERN_INFO "pwpwmobChar initialized\n");
	return 0;

cleanup:
	for (i = 0; i < dev_count; i++) {
		pwmmob_dev_clean_cdev(&pwmobCharDevices[i]);
	}
clean_device:
	kfree(pwmobCharDevices);
clean_class:
	class_destroy(pwmobCharClass);
	unregister_chrdev_region(dev, dev_count);
	return EFAULT;
}

static void __exit pwmobChar_exit(void)
{
	int i = 0;
	dev_t dev = MKDEV(majorNumber, 0);

	for (i = 0; i < dev_count; i++) {
		pwmmob_dev_clean_cdev(&pwmobCharDevices[i]);
	}
	kfree(pwmobCharDevices);
	class_destroy(pwmobCharClass);
	unregister_chrdev_region(dev, dev_count);

	printk(KERN_INFO "pwGoodbye from the LKM!\n");
}

static ssize_t pwmob_dev_read(struct file *filep, char *buffer, size_t len,
			      loff_t *offset)
{
	int result = 0;
	struct pwmob_dev *pwmdev = (struct pwmob_dev *)filep->private_data;
	size_t size = pwmdev->data_size - *offset;

	if (!pwmdev) {
		printk(KERN_ALERT "%s error accessing pwmob_dev\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	if (*offset > pwmdev->data_size) {
		size = 0;
		goto out;
	}

	if (*offset + len > pwmdev->data_size)
		len = pwmdev->data_size - *offset;

	result = copy_to_user(buffer, pwmdev->p_data + *offset, size);
	if (result != 0) {
		printk(KERN_ALERT "Failed to send %d characters to the user\n",
		       result);
		return -EFAULT;
	}

	printk("p-data: %s", pwmdev->p_data);
	pwmdev->data_size -= size;
	*offset += size;

out:
	printk(KERN_INFO " %s_%d Sent %ld characters to user\n", DEVICE_NAME,
	       MINOR(pwmdev->cdev.dev), size);

	return size;
}

static ssize_t pwmob_dev_write(struct file *filep, const char *buffer,
			       size_t len, loff_t *offset)
{
	struct pwmob_dev *pwmdev = (struct pwmob_dev *)filep->private_data;
	int result = 0;

	if (pwmdev == NULL) {
		printk(KERN_ALERT " %s error accessing pwmob_dev\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	if (pwmdev->p_data == NULL) {
		printk(KERN_ALERT "%s error accessing pwmob_dev msg\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	if (len + *offset > PWMOB_DATA_MAX) {
		printk(KERN_ALERT "Message too long\n");
		return -EMSGSIZE;
	}

	result = copy_from_user(pwmdev->p_data + *offset, buffer, len);
	if (result != 0) {
		printk(KERN_ALERT "Failed to send %d characters from user\n",
		       result);
		return -EFAULT;
	}

	pwmdev->data_size = len + *offset;

	can_read = true;
	wake_up_interruptible(&wait_queue);
	printk(KERN_INFO "%s_%d Received %zu characters from the user\n",
	       DEVICE_NAME, MINOR(pwmdev->cdev.dev), len);

	return len;
}

static int pwmob_dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "pwDevice successfully closed\n");
	return 0;
}

static long pwmob_dev_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{

	if (_IOC_TYPE(cmd) != PWMOBDEV_IOC_MAGIC)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE)) {
		if (!access_ok((void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	switch (cmd) {
	case WR_VALUE:
		pr_info("Value1 = %s\n", string);
		if (copy_from_user(string, (char *)arg, (sizeof(char)) * 255)) {
			pr_err("Data Write : Err!\n");
		}
		pr_info("Value2 = %s\n", string);
		break;
	case RD_VALUE:
		if (copy_to_user((char *)arg, string, (sizeof(char)) * 255)) {
			pr_err("Data Read : Err!\n");
		}
		printk("read data from device: %s", string);
		break;
	default:
		pr_info("Default\n");
		break;
	}
	return 0;
}

module_init(pwmob_dev_init);
module_exit(pwmobChar_exit);
