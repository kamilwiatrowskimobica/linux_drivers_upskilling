#include <linux/init.h> // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h> // Core header for loading LKMs into the kernel
#include <linux/device.h> // Header to support the kernel Driver Model
#include <linux/kernel.h> // Contains types, macros, functions for the kernel
#include <linux/fs.h> // Header for the Linux file system support
#include <linux/uaccess.h> // Required for the copy to user function
#include <linux/cdev.h>
#include <linux/types.h> /* needed for dev_t type */
#include <linux/kdev_t.h> /* needed for macros MAJOR, MINOR, MKDEV... */
#include <linux/slab.h> /* kmalloc(),kfree() */
#include <asm/uaccess.h> /* copy_to copy_from _user */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "seqchardev.h"

#define DEVICE_NAME                                                            \
	"seqchardev" ///< The device will appear at /dev/ebbchar using this value
#define CLASS_NAME                                                             \
	"seqchardevclass" ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rafal Cieslak");
MODULE_VERSION("0.1");

static int device_count = 1;

module_param(device_count, int, S_IRUGO);
MODULE_PARM_DESC(device_count, "The number of devices to create");
static int majorNumber = 0;
static int minorNumber = 0;

int device_max_size = DEVICE_MAX_SIZE;

static struct class *seq_char_dev_class = NULL;

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static void dev_clean_cdev(struct char_dev *dev);
static int dev_setup_cdev(struct char_dev *dev, size_t index);
static loff_t dev_llseek(struct file *filep, loff_t offset, int whence);
static void *seq_start(struct seq_file *sfile, loff_t *pos);
static void *seq_next(struct seq_file *sfile, void *v, loff_t *pos);
static void seq_stop(struct seq_file *sfile, void *v);
static int seq_show(struct seq_file *s, void *v);
static int seq_proc_open(struct inode *inode, struct file *file);

static struct file_operations fops = { .open = dev_open,
				       .read = dev_read,
				       .write = dev_write,
				       .release = dev_release,
				       .llseek = dev_llseek };

static struct proc_ops seq_pops = { .proc_open = seq_proc_open,
				    .proc_read = seq_read,
				    .proc_lseek = seq_lseek,
				    .proc_release = seq_release };

static struct seq_operations seq_ops = { .start = seq_start,
					 .next = seq_next,
					 .stop = seq_stop,
					 .show = seq_show };

struct char_dev *char_devices;

static void *seq_start(struct seq_file *sfile, loff_t *pos)
{
	if (*pos >= device_count || *pos < 0)
		return NULL;
	return char_devices + *pos;
}

void *seq_next(struct seq_file *sfile, void *v, loff_t *pos)
{
	(*pos)++;
	if (*pos >= device_count)
		return NULL;
	return char_devices + *pos;
}

static void seq_stop(struct seq_file *sfile, void *v)
{
}

static int seq_show(struct seq_file *s, void *v)
{
	struct char_dev *dev = (struct char_dev *)v;
	if (!dev) {
		printk(KERN_ALERT "no device data\n");
	}
	seq_printf(s, "seq_char_dev%i, data_size = %zu\n",
		   (int)(dev - char_devices), dev->data_size);
	return 0;
}

static int seq_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &seq_ops);
}

static int __init char_init(void)
{
	int result = 0;
	dev_t dev = 0;
	size_t i = 0;
	struct proc_dir_entry *entry;

	printk(KERN_INFO "Initializing the device LKM\n");

	printk(KERN_INFO "dynamic allocation of major number\n");
	result = alloc_chrdev_region(&dev, minorNumber, device_count,
				     DEVICE_NAME);

	if (result < 0) {
		printk(KERN_WARNING "can't get major number %d\n", majorNumber);
		return result;
	}
	majorNumber = MAJOR(dev);

	printk(KERN_INFO "Major number %d\n", majorNumber);

	// Register the device class
	seq_char_dev_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(seq_char_dev_class)) {
		class_destroy(seq_char_dev_class);
		unregister_chrdev_region(dev, device_count);
		printk(KERN_ALERT "Failed to register device class\n");
		return PTR_ERR(seq_char_dev_class);
	}
	printk(KERN_INFO "device class registered correctly\n");

	char_devices =
		kcalloc(device_count, sizeof(struct char_dev), GFP_KERNEL);

	if (!char_devices) {
		printk(KERN_WARNING " ERROR failed kcalloc \n");
		goto clean_device;
	}

	for (i = 0; i < device_count; i++) {
		result = dev_setup_cdev(&char_devices[i], i);
		if (result < 0) {
			goto cleanup;
		}
	}

	entry = proc_create("seq_char_dev", 0, NULL, &seq_pops);
	if (entry == NULL)
		return -ENOMEM;

	return 0;

cleanup:
	for (i = 0; i < device_count; i++) {
		dev_clean_cdev(&char_devices[i]);
	}
	return EFAULT;
clean_device:
	kfree(char_devices);
	return EFAULT;
}

static int dev_setup_cdev(struct char_dev *dev, size_t index)
{
	int err, devno = MKDEV(majorNumber, index);
	struct device *pdevice = NULL;
	cdev_init(&dev->cdev, &fops);

	dev->cdev.owner = THIS_MODULE;

	dev->p_data = kcalloc(1, DEVICE_MAX_SIZE, GFP_KERNEL);

	if (!dev->p_data) {
		printk(KERN_WARNING "ERROR kcalloc p_data\n");
		return PTR_ERR(dev->p_data);
	}

	err = cdev_add(&dev->cdev, devno, 1);

	if (err)
		printk(KERN_NOTICE "Error %d adding device", err);
	else
		printk(KERN_INFO "Device added \n");

	pdevice = device_create(seq_char_dev_class, NULL, devno, NULL, "%s_%zu",
				DEVICE_NAME, index);

	if (IS_ERR(pdevice)) {
		printk(KERN_ALERT "Failed to create device \n");
		return PTR_ERR(pdevice);
	}
	printk(KERN_INFO "created %s_%d\n", DEVICE_NAME, MINOR(devno));
	return 0;
}

static void dev_clean_cdev(struct char_dev *dev)
{
	if (!dev) {
		return;
	} else if (dev->p_data) {
		kfree(dev->p_data);
	}
	device_destroy(seq_char_dev_class, dev->cdev.dev);
	cdev_del(&dev->cdev);
}

static void __exit char_exit(void)
{
	dev_t devno = MKDEV(majorNumber, minorNumber);
	int i = 0;
	printk(KERN_INFO "Exiting\n");

	for (i = 0; i < device_count; i++) {
		dev_clean_cdev(&char_devices[i]);
	}
	kfree(char_devices);
	class_destroy(seq_char_dev_class);
	unregister_chrdev_region(devno, device_count);
	remove_proc_entry("seq_char_dev", NULL);

	printk(KERN_INFO "Exited\n");
}

static int dev_open(struct inode *inodep, struct file *filp)
{
	struct char_dev *dev;
	dev = container_of(inodep->i_cdev, struct char_dev, cdev);
	filp->private_data = dev;

	return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "Device successfully closed\n");
	return 0;
}

static ssize_t dev_read(struct file *filp, char *buffer, size_t count,
			loff_t *f_pos)
{
	int error_count = 0;
	struct char_dev *dev = (struct char_dev *)filp->private_data;

	if ((*f_pos + count) > dev->data_size) {
		count = 0;
		goto out;
	}
	error_count = copy_to_user(buffer, dev->p_data + *f_pos, count);

	if (error_count != 0) {
		printk(KERN_INFO "Failed to send %d characters to the user\n",
		       error_count);
		return -EFAULT;
	}
	*f_pos += count;
out:
	printk(KERN_INFO " %s_%d Sent %ld characters to user\n", DEVICE_NAME,
	       MINOR(dev->cdev.dev), count);

	return count;
}

static ssize_t dev_write(struct file *filp, const char *buf, size_t count,
			 loff_t *f_pos)
{
	struct char_dev *dev = (struct char_dev *)filp->private_data;
	printk(KERN_INFO "write\n");

	if (dev == NULL) {
		printk(KERN_ALERT " %s error accessing device\n", __FUNCTION__);
		return -EFAULT;
	}

	if (count + *f_pos > device_max_size) {
		printk(KERN_WARNING
		       "trying to write more than possible. Aborting write\n");
		return -EFBIG;
	}

	if (copy_from_user(dev->p_data + *f_pos, buf, count)) {
		printk(KERN_WARNING "can't use copy_from_user. \n");
		return -EPERM;
	}
	if (count + *f_pos > dev->data_size)
		dev->data_size = count + *f_pos;
	*f_pos += count;

	printk(KERN_INFO
	       "%s_%d Received %zu characters from the user. message: %s, pos %d\n",
	       DEVICE_NAME, MINOR(dev->cdev.dev), count, buf, *f_pos);

	return count;
}
loff_t dev_llseek(struct file *filep, loff_t offset, int whence)
{
	loff_t new_position = 0;
	struct char_dev *dev = (struct char_dev *)filep->private_data;

	printk(KERN_INFO "Seek file\n");
	if (!dev) {
		printk(KERN_INFO " No device data\n");
		return -EFAULT;
	}
	switch (whence) {
	case SEEK_SET:
		if (offset >= device_max_size)
			return -EFBIG;
		new_position = offset;
		printk(KERN_INFO
		       "Seek relative to beginning of file by % lld\n",
		       offset);
		break;

	case SEEK_CUR:
		if (filep->f_pos + offset >= device_max_size)
			return -EFBIG;
		new_position = filep->f_pos + offset;
		printk(KERN_INFO "Seek relative to current position by %lld\n",
		       offset);
		break;

	case SEEK_END:
		if (dev->data_size + offset >= device_max_size)
			return -EFBIG;
		new_position = dev->data_size + offset;
		printk(KERN_INFO "Seek relative to end of file by %lld\n",
		       offset);
		break;

	default:
		return -EINVAL;
	}
	if (new_position < 0)
		return -EINVAL;
	filep->f_pos = new_position;
	return new_position;
}

module_init(char_init);
module_exit(char_exit);