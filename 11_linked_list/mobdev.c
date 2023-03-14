#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h> // kmalloc()
#include <linux/uaccess.h> // copy_to/from_user()
#include <linux/err.h>
#include <linux/seq_file.h>

#include "mobdev.h"

#define CLASS_NAME "mobica"
#define DEVICE_NAME "mobdev"

static const unsigned int mob_devs_num = 4;
static int dev_minor_start = 0;
static int dev_major = 0;

static struct mob_dev *mob_devices = NULL;
static struct class *mob_class = NULL;

int mobdev_read_procsimple(struct seq_file *s, void *v)
{
	size_t i;
	if (!mob_devices) {
		pr_warn("failed, no device data\n");
		return -EFAULT;
	}

	for (i = 0; i < mob_devs_num; ++i) {
		struct mob_dev *mdev = mob_devices + i;
		struct mob_data *data;
		int count = 0;
		if (!mutex_trylock(&mdev->lock)) {
			pr_info("failed ps read, busy\n");
			return -EBUSY;
		}
		count = 0;
		list_for_each_entry (data, &mdev->items, list) {
			seq_printf(s, "Mobdev_%zu, node %d, size = %zu\n", i,
				   count++, data->size);
		}
		mutex_unlock(&mdev->lock);
	}
	seq_printf(s, "Finished mobdev_read_procsimple!\n");

	return 0;
}

static void *mobdev_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= mob_devs_num || *pos < 0)
		return NULL; /* No more devs in array */
	return mob_devices + *pos;
}

static void *mobdev_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;
	if (*pos >= mob_devs_num)
		return NULL;
	return mob_devices + *pos;
}

static void mobdev_seq_stop(struct seq_file *s, void *v)
{
	pr_info("mobdev: seq_stop\n");
}

static int mobdev_seq_show(struct seq_file *s, void *v)
{
	struct mob_dev *mdev = (struct mob_dev *)v;
	struct mob_data *data;
	size_t acc = 0;
	int count = 0;
	if (!mdev) {
		pr_warn("failed, no device data\n");
		return -EFAULT;
	}

	if (!mutex_trylock(&mdev->lock)) {
		pr_info("failed seq read, busy\n");
		return -EBUSY;
	}
	list_for_each_entry (data, &mdev->items, list) {
		count++;
		acc += data->size;
	}
	seq_printf(s, "Mobdev_%i, nodes %d, total size = %zu\n",
		   (int)(mdev - mob_devices), count, acc);
	mutex_unlock(&mdev->lock);
	return 0;
}

static struct seq_operations proc_sops = {
	.start = mobdev_seq_start,
	.next = mobdev_seq_next,
	.stop = mobdev_seq_stop,
	.show = mobdev_seq_show,
};

static int mobdev_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &proc_sops);
}

static struct proc_ops seq_pops = {
	.proc_open = mobdev_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

static void mobdev_create_proc(void)
{
	struct proc_dir_entry *entry;
	entry = proc_create_single("mobdev_simple", 0, NULL,
				   mobdev_read_procsimple);
	if (!entry)
		pr_warn("Proc simple entry not created!\n");
	entry = proc_create("mobdev_seq", 0, NULL, &seq_pops);
	if (!entry)
		pr_warn("Proc seq entry not created!\n");
}

static void mobdev_remove_proc(void)
{
	/* No problem if it was not registered */
	remove_proc_entry("mobdev_simple", NULL);
	remove_proc_entry("mobdev_seq", NULL);
}

static int mobdev_open(struct inode *inode, struct file *file)
{
	struct mob_dev *mdev;
	pr_debug_ratelimited("device file open...!!!\n");
	mdev = container_of(inode->i_cdev, struct mob_dev, c_dev);
	file->private_data = mdev; /* for other methods */

	return nonseekable_open(inode, file);
}

static int mobdev_release(struct inode *inode, struct file *file)
{
	pr_debug_ratelimited("device file released...!!!\n");
	return 0;
}

static ssize_t mobdev_read(struct file *filp, char __user *buf, size_t len,
			   loff_t *off)
{
	struct mob_data *data;
	struct mob_dev *mdev = (struct mob_dev *)filp->private_data;
	ssize_t ret_size;
	pr_debug_ratelimited("mobdev_read, len = %zu, off = %lld\n", len, *off);
	if (!mdev) {
		pr_warn("failed, no device data\n");
		return -EFAULT;
	}

	if (mutex_lock_interruptible(&mdev->lock))
		return -ERESTARTSYS;

	if (list_empty(&mdev->items)) {
		ret_size = 0;
		goto clean;
	}

	data = list_first_entry(&mdev->items, struct mob_data, list);
	if (len < data->size) {
		ret_size = -ETOOSMALL;
		goto clean;
	}

	//Copy the data from the kernel space to the user-space
	if (copy_to_user(buf, data->p_data, data->size)) {
		pr_warn("copy to user failed !\n");
		ret_size = -EFAULT;
		goto clean;
	}
	list_del(&data->list);

	mutex_unlock(&mdev->lock);

	ret_size = data->size;
	pr_debug_ratelimited("Data Read %zu: Done!\n", ret_size);
	kfree(data->p_data);
	kfree(data);

	return ret_size;

clean:
	mutex_unlock(&mdev->lock);
	return ret_size;
}

static ssize_t mobdev_write(struct file *filp, const char __user *buf,
			    size_t len, loff_t *off)
{
	struct mob_data *data;
	struct mob_dev *mdev = (struct mob_dev *)filp->private_data;
	pr_debug_ratelimited("mobdev_write, len = %zu, off = %lld\n", len,
			     *off);
	if (!mdev) {
		pr_warn("failed, no device data\n");
		return -EFAULT;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->p_data = kzalloc(len, GFP_KERNEL);
	if (!data->p_data) {
		kfree(data);
		return -ENOMEM;
	}

	data->size = len;
	INIT_LIST_HEAD(&data->list);

	//Copy the data to kernel space from the user-space
	if (copy_from_user(data->p_data, buf, len)) {
		kfree(data->p_data);
		kfree(data);
		pr_err("copy from user failed!\n");
		return -EFAULT;
	}

	if (mutex_lock_interruptible(&mdev->lock)) {
		kfree(data->p_data);
		kfree(data);
		return -ERESTARTSYS;
	}

	list_add_tail(&data->list, &mdev->items);

	mutex_unlock(&mdev->lock);

	*off += len;
	pr_debug_ratelimited("Data Write %zu: Done!\n", len);

	return len;
}

/*
** File Operations structure
*/
static struct file_operations fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = mobdev_read,
	.write = mobdev_write,
	.open = mobdev_open,
	.release = mobdev_release,
};

static void mobdev_clean_cdev(struct mob_dev *mdev)
{
	struct mob_data *data, *temp;

	device_destroy(mob_class, mdev->c_dev.dev);

	cdev_del(&mdev->c_dev);

	list_for_each_entry_safe (data, temp, &mdev->items, list) {
		kfree(data->p_data);
		kfree(data);
	}
}

static int mobdev_init_cdev(struct mob_dev *mdev, size_t index)
{
	int result = 0;
	struct device *mob_device = NULL;
	dev_t dev = MKDEV(dev_major, dev_minor_start + index);

	cdev_init(&mdev->c_dev, &fops);
	mdev->c_dev.owner = THIS_MODULE;

	INIT_LIST_HEAD(&mdev->items);

	result = cdev_add(&mdev->c_dev, dev, 1);
	if (result < 0) {
		pr_warn("failed to add cdev\n");
		return result;
	}

	mob_device = device_create(mob_class, NULL, dev, NULL, "%s_%zu",
				   DEVICE_NAME, index);
	if (IS_ERR(mob_device)) {
		pr_warn("failed to create device\n");
		return PTR_ERR(mob_device);
	}

	return 0;
}

/*
** Module Init function
*/
static __init int mobdev_init(void)
{
	int result = 0;
	size_t i;
	dev_t dev = 0;

	result = alloc_chrdev_region(&dev, dev_minor_start, mob_devs_num,
				     DEVICE_NAME);
	if (result < 0) {
		pr_warn("Cannot allocate major number\n");
		return result;
	}

	dev_major = MAJOR(dev);

	mob_devices = kcalloc(mob_devs_num, sizeof(struct mob_dev), GFP_KERNEL);
	if (!mob_devices) {
		pr_warn("kcalloc failed for mob devices\n");
		result = -ENOMEM;
		goto clean_chrdev;
	}

	mob_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(mob_class)) {
		pr_warn("failed to create class\n");
		result = PTR_ERR(mob_class);
		goto clean_mobdevs;
	}

	/* Initialize each cdevice */
	for (i = 0; i < mob_devs_num; i++) {
		mutex_init(&mob_devices[i].lock);
		result = mobdev_init_cdev(&mob_devices[i], i);
		if (result < 0) {
			pr_warn("Error adding mob device %zu\n", i);
			goto clean_cdevs;
		}
	}

	mobdev_create_proc();
	pr_info("mobdev_init done\n");
	return 0;

clean_cdevs:
	for (i = 0; i < mob_devs_num; i++) {
		mobdev_clean_cdev(&mob_devices[i]);
		mutex_destroy(&mob_devices[i].lock);
	}
	class_destroy(mob_class);
clean_mobdevs:
	kfree(mob_devices);
clean_chrdev:
	unregister_chrdev_region(dev, mob_devs_num);
	return result;
}

/*
** Module exit function
*/
static __exit void mobdev_exit(void)
{
	size_t i;
	pr_info("mobdev_exit\n");
	mobdev_remove_proc();
	if (mob_devices) {
		for (i = 0; i < mob_devs_num; i++) {
			mobdev_clean_cdev(&mob_devices[i]);
			mutex_destroy(&mob_devices[i].lock);
		}
		kfree(mob_devices);
	}
	class_destroy(mob_class);
	unregister_chrdev_region(MKDEV(dev_major, dev_minor_start),
				 mob_devs_num);
}

module_init(mobdev_init);
module_exit(mobdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kamil Wiatrowski");
MODULE_DESCRIPTION("Read/Write Character driver using lists.");
MODULE_VERSION("0.3.2");
