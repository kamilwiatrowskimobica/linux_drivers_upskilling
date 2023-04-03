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
#include <linux/spinlock.h>
#include <linux/completion.h>
#include "pwmobchar.h"

#define DEVICE_NAME "pwmobChar"
#define CLASS_NAME "pwmob"
#define NUMBER_OF_DEVICES 5

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Piotr Wozniak");
MODULE_DESCRIPTION("Linux char driver");
MODULE_VERSION("1");

static int majorNumber;

static int numberOpens = 0;
static struct class *pwmobCharClass = NULL;
static struct pwmob_dev *pwmobCharDevices = NULL;
static uint dev_count = NUMBER_OF_DEVICES;

//spinlock_t my_lock = SPIN_LOCK_UNLOCKED;

/*declarations*/
static int pwmob_dev_open(struct inode *, struct file *);
static int pwmob_dev_release(struct inode *, struct file *);
static ssize_t pwmob_dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t pwmob_dev_write(struct file *, const char *, size_t, loff_t *);
static loff_t pwmob_dev_llseek(struct file *, loff_t, int);

static struct file_operations fops = { .open = pwmob_dev_open,
				       .read = pwmob_dev_read,
				       .write = pwmob_dev_write,
				       .release = pwmob_dev_release,
				       .llseek = pwmob_dev_llseek };

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

	rwlock_init(&(pwmobCharDevices->my_rwlock));
	for (i = 0; i < dev_count; i++) {
		ret = pwmob_dev_init_cdev(&pwmobCharDevices[i], i);
		if (0 > ret) {
			goto cleanup;
		}
	}

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

	read_lock(&(pwmobCharDevices->my_rwlock));
	if (!pwmdev) {
		printk(KERN_ALERT "%s error accessing pwmob_dev\n",
		       __FUNCTION__);
		read_unlock(&(pwmobCharDevices->my_rwlock));
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
		read_unlock(&(pwmobCharDevices->my_rwlock));
		return -EFAULT;
	}

	pwmdev->data_size -= size;
	*offset += size;

out:
	printk(KERN_INFO " %s_%d Sent %ld characters to user\n", DEVICE_NAME,
	       MINOR(pwmdev->cdev.dev), size);
	read_unlock(&(pwmobCharDevices->my_rwlock));
	return size;
}

static ssize_t pwmob_dev_write(struct file *filep, const char *buffer,
			       size_t len, loff_t *offset)
{
	struct pwmob_dev *pwmdev = (struct pwmob_dev *)filep->private_data;
	int result = 0;

	write_lock(&(pwmobCharDevices->my_rwlock));
	if (pwmdev == NULL) {
		printk(KERN_ALERT " %s error accessing pwmob_dev\n",
		       __FUNCTION__);
		write_unlock(&(pwmobCharDevices->my_rwlock));
		return -EFAULT;
	}

	if (pwmdev->p_data == NULL) {
		printk(KERN_ALERT "%s error accessing pwmob_dev msg\n",
		       __FUNCTION__);
		write_unlock(&(pwmobCharDevices->my_rwlock));
		return -EFAULT;
	}

	if (len + *offset > PWMOB_DATA_MAX) {
		printk(KERN_ALERT "Message too long\n");
		write_unlock(&(pwmobCharDevices->my_rwlock));
		return -EMSGSIZE;
	}

	result = copy_from_user(pwmdev->p_data + *offset, buffer, len);
	if (result != 0) {
		printk(KERN_ALERT "Failed to send %d characters from user\n",
		       result);
		write_unlock(&(pwmobCharDevices->my_rwlock));
		return -EFAULT;
	}

	pwmdev->data_size = len + *offset;
	*offset += len;

	printk(KERN_INFO "%s_%d Received %zu characters from the user\n",
	       DEVICE_NAME, MINOR(pwmdev->cdev.dev), len);
	write_unlock(&(pwmobCharDevices->my_rwlock));

	return len;
}

static int pwmob_dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "pwDevice successfully closed\n");
	return 0;
}

module_init(pwmob_dev_init);
module_exit(pwmobChar_exit);
