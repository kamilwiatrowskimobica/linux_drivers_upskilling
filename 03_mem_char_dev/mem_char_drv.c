/**
 * @file   mem_char_drv.c
 * @author Michal Binek
 * @date   23 January 2024
 * @version 0.1
 * @brief   Memory char driver
 */

#include <linux/init.h> /* needed for module_init and exit */
#include <linux/module.h>
#include <linux/moduleparam.h> /* needed for module_param */
#include <linux/kernel.h> /* needed for printk */
#include <linux/sched.h> /* needed for current-> */
#include <linux/types.h> /* needed for dev_t type */
#include <linux/kdev_t.h> /* needed for macros MAJOR, MINOR, MKDEV... */
#include <linux/fs.h> /* needed for register_chrdev_region, file_operations */
#include <linux/cdev.h> /* cdev definition */
#include <linux/slab.h> /* kmalloc(),kfree() */
#include <asm/uaccess.h> /* copy_to copy_from _user */
#include "mem_char_drv.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michal Binek");
MODULE_DESCRIPTION("Memory character driver");
MODULE_VERSION("0.1");

static int mcd_major_number = 0;

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
loff_t dev_llseek(struct file *, loff_t, int);

static struct file_operations fops = {
	.llseek = dev_llseek,
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

static struct mcd_dev *mcd_devices = NULL;

static void mcd_cleanup(void)
{
	dev_t devno = MKDEV(mcd_major_number, 0);
	if (mcd_devices) {
		for (int i = 0; i < MCD_DEVICE_COUNT; i++) {
			if (mcd_devices[i].data) {
				cdev_del(&(mcd_devices[i].cdev));
				kfree(mcd_devices[i].data);
			}
		}
		kfree(mcd_devices);
	}
	unregister_chrdev_region(devno, MCD_DEVICE_COUNT);
	printk(KERN_INFO "MCD: cleaned up\n");
}

static int __init mcd_init(void)
{
	int result = 0;
	dev_t devno = 0;

	printk(KERN_INFO "MCD: Initializing\n");

	result = alloc_chrdev_region(&devno, 0, MCD_DEVICE_COUNT, "mcd");
	mcd_major_number = MAJOR(devno);

	if (result < 0) {
		printk(KERN_WARNING "MCD: can't get major %d\n",
		       mcd_major_number);
		return result;
	}

	mcd_devices =
		kmalloc(MCD_DEVICE_COUNT * sizeof(struct mcd_dev), GFP_KERNEL);
	if (!mcd_devices) {
		result = -ENOMEM;
		printk(KERN_WARNING "MCD: ERROR kmalloc dev struct\n");
		goto fail;
	}
	memset(mcd_devices, 0, MCD_DEVICE_COUNT * sizeof(struct mcd_dev));

	struct class *mcd_class = class_create("mcd_class");
	for (int i = 0; i < MCD_DEVICE_COUNT; i++) {
		mcd_devices[i].data = (char *)kmalloc(
			MCD_DEVICE_MAX_SIZE * sizeof(char), GFP_KERNEL);
		if (!mcd_devices[0].data) {
			result = -ENOMEM;
			printk(KERN_WARNING "MCD: ERROR kmalloc data\n");
			goto fail;
		}
		cdev_init(&mcd_devices[i].cdev, &fops);
		mcd_devices[i].cdev.owner = THIS_MODULE;
		mcd_devices[i].cdev.ops = &fops;
		devno = MKDEV(mcd_major_number, i);
		device_create(mcd_class, NULL, devno, NULL, "mcd_dev%d", i);
		if (cdev_add(&mcd_devices[i].cdev, devno, 1)) {
			printk(KERN_WARNING "MCD:Error cdev_add");
			goto fail;
		}
		printk(KERN_INFO "MCD: Device %d initialized\n", i);
	}

	return 0;

fail:
	mcd_cleanup();
	return result;
}

static void __exit mcd_exit(void)
{
	mcd_cleanup();
	printk(KERN_INFO "MCD: Goodbye from the LKM!\n");
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	struct mcd_dev *dev =
		container_of(inodep->i_cdev, struct mcd_dev, cdev);
	filep->private_data = dev;
	printk(KERN_INFO "MCD: Device opened\n");
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len,
			loff_t *offset)
{
	struct mcd_dev *dev = filep->private_data;
	size_t size = dev->size - *offset;
	if (*offset >= MCD_DEVICE_MAX_SIZE) {
		return 0;
	}
	if (*offset >= dev->size) {
		return 0;
	}
	if (len > size) {
		printk(KERN_WARNING
		       "MCD: Trying to read more than possible.\n");
		len = size;
	}
	if (copy_to_user(buffer, (void *)(dev->data + *offset), len)) {
		printk(KERN_WARNING "MCD: can't use copy_to_user. \n");
		return -EPERM;
	}
	printk(KERN_INFO "MCD: READ Operation sucessfully\n");
	*offset += len;
	return len;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len,
			 loff_t *offset)
{
	struct mcd_dev *dev = filep->private_data;
	if (len + *offset > MCD_DEVICE_MAX_SIZE) {
		printk(KERN_WARNING
		       "MCD: Trying to write more than possible.\n");
		return -EFBIG;
	}

	if (copy_from_user((void *)(dev->data + *offset), buffer, len)) {
		printk(KERN_WARNING "MCD: can't use copy_from_user. \n");
		return -EPERM;
	}
	printk(KERN_INFO "MCD: WRITE Operation sucessfully\n");
	if (len + *offset > dev->size) {
		dev->size = len + *offset;
	}
	*offset += len;
	return len;
}

loff_t dev_llseek(struct file *filep, loff_t offset, int whence)
{
	loff_t npos;
	struct mcd_dev *dev = filep->private_data;

	switch (whence) {
	case SEEK_SET:
		if (offset >= MCD_DEVICE_MAX_SIZE)
			return -EFBIG;
		npos = offset;
		break;

	case SEEK_CUR:
		if (filep->f_pos + offset >= MCD_DEVICE_MAX_SIZE)
			return -EFBIG;
		npos = filep->f_pos + offset;
		break;

	case SEEK_END:
		if (dev->size + offset >= MCD_DEVICE_MAX_SIZE)
			return -EFBIG;
		npos = dev->size + offset;
		break;

	default: /* not expected value */
		return -EINVAL;
	}
	if (npos < 0)
		return -EINVAL;
	filep->f_pos = npos;
	printk(KERN_INFO "MCD: LLSEEK Operation sucessfully\n");
	return npos;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "MCD: Device successfully closed\n");
	return 0;
}

module_init(mcd_init);
module_exit(mcd_exit);
