/**
 * @file    mysimplechar.c
 * @author  Andrzej Dziarnik
 * @date    20.06.2023
 * @version 0.2
 * @brief  Simple character driver created for training purposes
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

#include "mysimplechardev_main.h"
#include "mysimplechardev_proc.h"

#define DEVICE_NAME "mysimplechardev"
#define CLASS_NAME "ldd"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrzej Dziarnik");
MODULE_DESCRIPTION("A simple character driver.");
MODULE_VERSION("0.2");

static int dev_major = 0;
static int dev_first_minor = 0;
static struct class *my_class = NULL;
struct mysimplechardev_dev *my_devices = NULL;

static int dev_open(struct inode *inodep, struct file *filep)
{
	struct mysimplechardev_dev *mdev = NULL;

	mdev = container_of(inodep->i_cdev, struct mysimplechardev_dev, cdev);
	if (!mdev) {
		pr_warn("SCHD: Failed device data not found\n");
		return -EFAULT;
	}
	filep->private_data = mdev;
	printk(KERN_INFO "SCHD: Device successfully opened\n");
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
	size_t size = 0;
	struct mysimplechardev_dev *mdev =
		(struct mysimplechardev_dev *)filep->private_data;

	if (!mdev) {
		pr_warn("SCHD: No device data\n");
		return -EFAULT;
	}
	if (*offset > mdev->data_size) {
		goto out;
	}
	size = mdev->data_size - *offset;
	if (size > len)
		size = len;
	if (size == 0)
		goto out;
	mdev->data_size -= size;
	if (copy_to_user(buffer, mdev->data + *offset, size)) {
		pr_warn("SCHD: Read %zu of data failed\n", size);
		return -EFAULT;
	}
out:
	pr_info("SCHD: Read %zu of data\n", size);
	return size;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len,
			 loff_t *offset)
{
	struct mysimplechardev_dev *mdev =
		(struct mysimplechardev_dev *)filep->private_data;

	if (!mdev) {
		pr_warn("SCHD: No device data\n");
		return -EFAULT;
	}
	if (len + *offset > MSG_MAX_SIZE) {
		pr_warn("SCHD: Wrong buffer size %zu.\n", len);
		return -EFBIG;
	}
	if (copy_from_user(mdev->data + *offset, buffer, len)) {
		pr_err("SCHD: Write %zu of data failed\n", len);
	}
	if (len + *offset > mdev->data_size)
		mdev->data_size = len + *offset;
	pr_info("SCHD: Write %zu of data\n", len);
	*offset += len;
	return len;
}

loff_t dev_llseek(struct file *filep, loff_t offset, int whence)
{
	loff_t new_position = 0;
	struct mysimplechardev_dev *mdev =
		(struct mysimplechardev_dev *)filep->private_data;

	pr_info("SCHD: Seek file\n");
	if (!mdev) {
		pr_warn("SCHD: No device data\n");
		return -EFAULT;
	}
	switch (whence) {
	case SEEK_SET:
		if (offset >= MSG_MAX_SIZE)
			return -EFBIG;
		new_position = offset;
		pr_info("SCHD: Seek relative to beginning of file by %lld\n",
			offset);
		break;

	case SEEK_CUR:
		if (filep->f_pos + offset >= MSG_MAX_SIZE)
			return -EFBIG;
		new_position = filep->f_pos + offset;
		pr_info("SCHD: Seek relative to current position by %lld\n",
			offset);
		break;

	case SEEK_END:
		if (mdev->data_size + offset >= MSG_MAX_SIZE)
			return -EFBIG;
		new_position = mdev->data_size + offset;
		pr_info("SCHD: Seek relative to end of file by %lld\n", offset);
		break;

	default:
		return -EINVAL;
	}
	if (new_position < 0)
		return -EINVAL;
	filep->f_pos = new_position;
	return new_position;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.llseek = dev_llseek,
	.open = dev_open,
	.release = dev_release,
	.read = dev_read,
	.write = dev_write,
};

static int __init my_dev_init(int dev_index)
{
	int ret = 0;
	struct device *p_dev = NULL;
	struct mysimplechardev_dev *mdev = &my_devices[dev_index];
	dev_t dev = MKDEV(dev_major, dev_first_minor + dev_index);

	cdev_init(&mdev->cdev, &fops);
	mdev->cdev.owner = THIS_MODULE;
	mdev->data = kcalloc(1, MSG_MAX_SIZE, GFP_KERNEL);
	if (!mdev->data) {
		pr_warn("SCHD: Cannot allocate memory for device buffer\n");
		return -ENOMEM;
	}
	ret = cdev_add(&mdev->cdev, dev, 1);
	if (ret < 0) {
		pr_warn("SCHD: Failed to add cdev\n");
		return ret;
	}
	p_dev = device_create(my_class, NULL, dev, NULL, "%s_%d", DEVICE_NAME,
			      dev_index);
	if (IS_ERR(p_dev)) {
		pr_warn("SCHD: Failed to create device\n");
		return PTR_ERR(p_dev);
	}
	return ret;
}

static int my_dev_clean(int dev_index)
{
	struct mysimplechardev_dev *mdev = &my_devices[dev_index];

	device_destroy(my_class, MKDEV(dev_major, dev_first_minor + dev_index));
	cdev_del(&mdev->cdev);
	if (mdev->data) {
		kfree(mdev->data);
		mdev->data = NULL;
	}
	return 0;
}

static int __init mysimplechardev_init(void)
{
	int ret = 0;
	static dev_t dev = 0;
	int i;

	ret = alloc_chrdev_region(&dev, 0, MY_DEV_COUNT, DEVICE_NAME);
	if (ret < 0) {
		pr_warn("SCHD: Cannot allocate major number\n");
		return ret;
	}
	dev_major = MAJOR(dev);
	my_devices = kcalloc(MY_DEV_COUNT, sizeof(struct mysimplechardev_dev),
			     GFP_KERNEL);
	if (!my_devices) {
		pr_warn("SCHD: Cannot allocate memory for mysimplechardev\n");
		ret = -ENOMEM;
		goto clean_device;
	}
	my_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(my_class)) {
		pr_warn("SCHD: Cannot create class for mysimplechardev\n");
		ret = PTR_ERR(my_class);
		goto free_my_devices;
	}
	for (i = 0; i < MY_DEV_COUNT; i++) {
		ret = my_dev_init(i);
		if (ret < 0) {
			pr_warn("SCHD: Error adding mysimplechardev_%d\n", i);
			goto clean_my_devices;
		}
	}
	mysimplechardev_create_proc();
	return ret;
clean_my_devices:
	for (i = 0; i < MY_DEV_COUNT; i++)
		my_dev_clean(i);
	class_destroy(my_class);
free_my_devices:
	kfree(my_devices);
	my_devices = NULL;
clean_device:
	unregister_chrdev_region(dev, MY_DEV_COUNT);
	return ret;
}

static void __exit mysimplechardev_exit(void)
{
	int i = 0;
	mysimplechardev_remove_proc();
	for (i = 0; i < MY_DEV_COUNT; i++)
		my_dev_clean(i);
	class_destroy(my_class);
	kfree(my_devices);
	my_devices = NULL;
	unregister_chrdev_region(MKDEV(dev_major, dev_first_minor),
				 MY_DEV_COUNT);
}

module_init(mysimplechardev_init);
module_exit(mysimplechardev_exit);
