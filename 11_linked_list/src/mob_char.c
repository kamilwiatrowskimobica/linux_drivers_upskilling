#include <linux/init.h> /* needed for module_init and exit */
#include <linux/module.h>
#include <linux/moduleparam.h> /* needed for module_param */
#include <linux/kernel.h> /* needed for printk */
#include <linux/sched.h> /* needed for current-> */
#include <linux/types.h> /* needed for dev_t type */
#include <linux/kdev_t.h> /* needed for macros MAJOR, MINOR, MKDEV... */
#include <linux/fs.h> /* needed for register_chrdev_region, file_operations */
#include <linux/cdev.h> /* cdev definition */
#include <linux/slab.h>
#include <asm/uaccess.h> /* copy_to copy_from _user */
#include <linux/device.h>

#include "mob_debug.h"
#include "mob_char.h"
#include "mob_sync.h"
#include "mob_ioctl.h"
#include "mob_list.h"

#define DEVICE_NAME "mob_char"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AKP");
MODULE_VERSION("1.0");


int mob_major = 0;
int mob_minor = 0;
unsigned int mob_nr_devs = 2;
int device_max_size = DEVICE_MAX_SIZE;

module_param(mob_major, int, S_IRUGO);
module_param(mob_minor, int, S_IRUGO);
module_param(mob_nr_devs, int, S_IRUGO);
module_param(device_max_size, int, S_IRUGO);

// Fops prototypes
static int __init mob_init(void);
static void __exit mob_exit(void);
static int mob_open(struct inode *inode, struct file *filp);
static int mob_release(struct inode *inode, struct file *filp);
static ssize_t mob_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *f_pos);
static ssize_t mob_read(struct file *filp, char __user *buf, size_t count,
			loff_t *f_pos);
void mob_cleanup_module(void);

/**
 * @brief Allocates mob_dev struct.
 * 
 * @param mob_num_devs Number of mob_dev instances.
 * @return int 0 if success, else linux err code.
 */
static int mob_device_alloc(unsigned int mob_num_devs);
/**
 * @brief Allocates mob_dev buffer.
 * 
 * @param len Buffer len.
 * @return int 0 if success, else linux err code.
 */
static int mob_buffer_alloc(struct mob_dev *dev, size_t len);
/**
 * @brief Set up the char_dev structure for this device.
 * 
 * @param dev mob_dev struct pointer.
 */
static void mob_setup_cdev(struct mob_dev *dev, int minor);

static unsigned int mob_poll(struct file *p_fl, struct poll_table_struct *p_tbl);

static long mob_ioctl(struct file *p_fl, unsigned int cmd, unsigned long arg);
// ------------------------------------------------------

struct mob_dev *mob_devices; /* allocated in mob_init_module */

struct file_operations mob_fops = {
	.owner = THIS_MODULE,
	.read = mob_read,
	.write = mob_write,
	.open = mob_open,
	.release = mob_release,
	.poll = mob_poll,
	.unlocked_ioctl = mob_ioctl,
};

static struct kmem_cache *mob_dev_cache;

static int mob_buffer_alloc(struct mob_dev *dev, size_t len)
{
	// sanity
	int ret = 0;
	if (NULL == dev) {
		ret = PTR_ERR(dev);
	}

	if (!ret) {
		dev->p_data = (char *)kzalloc(len * sizeof(char), GFP_KERNEL);
		if (!dev->p_data) {
			ret = -ENOMEM;
			printk(KERN_WARNING "ERROR kmalloc p_data\n");
		} else {
			dev->buffer_len = len;
		}
	}

	if (!ret) {
		dev->cache = kmem_cache_alloc(mob_dev_cache, GFP_KERNEL);
		if (!dev->cache) {
			MOB_PRINT("Cache allocation failed\n");
			ret = -ENOMEM;
		}
	}

	return ret;
}

static int mob_device_alloc(unsigned int mob_num_devs)
{
	int result = 0;
	mob_devices =
		kmalloc(mob_num_devs * sizeof(struct mob_dev), GFP_KERNEL);
	if (!mob_devices) {
		result = -ENOMEM;
		printk(KERN_WARNING "ERROR kmalloc dev struct\n");
	}

	return result;
}

static int __init mob_init(void)
{
	int result = 0;
	dev_t dev = 0;

	if (mob_major) {
		printk(KERN_INFO "static allocation of major number (%d)\n",
		       mob_major);
		dev = MKDEV(mob_major, mob_minor);
		result = register_chrdev_region(dev, mob_nr_devs, DEVICE_NAME);
	} else {
		printk(KERN_INFO "dinamic allocation of major number\n");
		result = alloc_chrdev_region(&dev, mob_minor, mob_nr_devs,
					     DEVICE_NAME);
		mob_major = MAJOR(dev);
		MOB_PRINT("Major number is %d", mob_major);
	}

	if (result < 0) {
		printk(KERN_WARNING "MOBCHAR: can't get major %d\n", mob_major);
		return result;
	}

	if (!result)
		// Device allocation
		result = mob_device_alloc(mob_nr_devs);

	mob_dev_cache = kmem_cache_create(DEVICE_NAME, CACHE_SIZE, 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!mob_dev_cache){
		result = -ENOMEM;
	}	

	if (!result) {
		// Buffer allocation
		for (int it = 0; it < mob_nr_devs; it++) {
			result = mob_buffer_alloc(&mob_devices[it],
						  DEVICE_MAX_SIZE);
			mob_devices[it].index_of_dev = it;
			mob_devices[it].ioctl_var = it*5;
			if (result)
				break;
		}
	}

	if (!result) {
		for (int it = 0; it < mob_nr_devs; it++)
			mob_setup_cdev(&mob_devices[it], it);
	}

	mob_devices->mode = SYNC_MODE_POLL;

	//Spinlock init
	for (int it = 0; it < mob_nr_devs; it++) {
		if (SYNC_MODE_SPNLCK == mob_devices->mode) {
			spinlock_dynamic_init(&mob_devices[it].spn_lck);
		} else if (SYNC_MODE_SMPHR == mob_devices->mode) {
			MOB_PRINT("Initing semaphore\n");
			mob_sync_semphr_init(&mob_devices[it].semphr);
		} else if (SYNC_MODE_CMPLT == mob_devices->mode) {
			mob_sync_completition_init(&mob_devices[it].compl);
		} else if (SYNC_MODE_POLL == mob_devices->mode) {
			mob_sync_poll_queue_init(&mob_devices[it].poll);
		}

		mob_list_init(&mob_devices[it].list);
	}

	if (!result) {
		printk(KERN_INFO "MOBCHAR INIT success\n");
	}

	if (0 > result)
		mob_cleanup_module();

	return result;
}

/*
 * The exit function is simply calls the cleanup
 */

static void __exit mob_exit(void)
{
	printk(KERN_INFO "--calling cleanup function--\n");
	mob_cleanup_module();
}

// Open function
static int mob_open(struct inode *inode, struct file *filp)
{
	struct mob_dev *dev; /* device information */
	printk(KERN_INFO "Performing 'open' operation\n");
	dev = container_of(inode->i_cdev, struct mob_dev, cdev);
	filp->private_data = dev; /* for other methods */

	return 0; /* success */
}

// Release function
static int mob_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "Prforming 'release' operation\n");
	return 0;
}

void mob_cleanup_module(void)
{
	dev_t devno = MKDEV(mob_major, mob_minor);
	for (int it = 0; it < mob_nr_devs; it++) {
		cdev_del(&(mob_devices[it].cdev));
	}
	/* freeing the memory */

	for (int it = 0; it < mob_nr_devs; it++) {
		if ((mob_devices[it].p_data) != 0)
			kfree(mob_devices[it].p_data);
		
		kmem_cache_free(mob_dev_cache, mob_devices[it].cache);
	}
	printk(KERN_INFO "Kfree the string-memory\n");

	if ((mob_devices) != 0) {
		kfree(mob_devices);
		printk(KERN_INFO "Kfree mob_devices\n");
	}

	unregister_chrdev_region(devno, mob_nr_devs); /* unregistering device */

	if (mob_dev_cache){
		kmem_cache_destroy(mob_dev_cache);
	}
	printk(KERN_INFO "Cdev deleted, kfree, chdev unregistered\n");
}

static ssize_t mob_read(struct file *filp, char __user *buf, size_t count,
			loff_t *f_pos)
{
	ssize_t retval;
	size_t size;

	struct mob_dev *mdev = (struct mob_dev *)filp->private_data;

	MOB_PRINT("%s: len = %lu, offset = %llu\n, data_len = %d", __FUNCTION__,
		  count, *f_pos, mob_devices->data_len);

	MOB_PRINT("Mode %d\n", mdev->mode);

	mob_list_print(&mdev->list);

	if (mdev->mode == SYNC_MODE_SMPHR) {
		MOB_PRINT("Taking read semaphore\n");
		mob_sync_semphr_take(&(mdev->semphr));
	} else if (SYNC_MODE_CMPLT == mdev->mode) {
		mob_sync_wait_for_completion(&mdev->compl, SYNC_COMPLT_TIMEOUT);
	}

	if (*f_pos >= mdev->data_len) {
		if (mdev->mode == SYNC_MODE_SMPHR) {
			MOB_PRINT("Giving read semaphore\n");
			mob_sync_semphr_give(&(mdev->semphr));
		}
		return 0;
	}

	if (mdev->data_len > count) {
		size = count;
	} else {
		size = mdev->data_len;
	}

	int err = copy_to_user(buf, (const void *)mdev->p_data + *f_pos, size);

	if (0 == err) {
		retval = size;
		printk(KERN_INFO "%lu chars are sent to the user\n", size);
		*f_pos += size;

	} else {
		retval = -EPERM;
		printk(KERN_WARNING "Failed to send chars to the user!\n");
	}

	if (mdev->mode == SYNC_MODE_SMPHR) {
		mob_sync_semphr_give(&(mdev->semphr));
	}

	return retval;
}

static ssize_t mob_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *f_pos)
{
	int retval = 0;

	MOB_PRINT("%s: len = %lu, offset = %llu\n", __FUNCTION__, count,
		  *f_pos);

	struct mob_dev *mdev = (struct mob_dev *)filp->private_data;

	if (mdev->mode == SYNC_MODE_SMPHR) {
		mob_sync_semphr_take(&mdev->semphr);
	}

	if (count > mdev->buffer_len) {
		printk(KERN_WARNING
		       "MOBCHAR: trying to write more than possible. Aborting write\n");
		retval = -EFBIG;

	} else if (copy_from_user((void *)(mdev->p_data + *f_pos), buf,
				  count)) {
		printk(KERN_WARNING "MOBCHAR: can't use copy_from_user. \n");
		retval = -EPERM;
	}

	if (!retval) {
		mdev->data_len = count;
		*f_pos += count;
		MOB_PRINT("%lu number of written bytes\n", count);
	}

	mob_list_add_item_to_back(&mdev->list);

	if (mdev->mode == SYNC_MODE_SMPHR) {
		mob_sync_semphr_give(&(mdev->semphr));
	} else if (mdev->mode == SYNC_MODE_CMPLT) {
		mob_sync_complete(&mdev->compl);
	} else if (mdev->mode == SYNC_MODE_POLL) {
		mob_sync_poll_queue_wakeup(&mdev->poll);
	}

	return retval;
}

static unsigned int mob_poll(struct file *p_fl, struct poll_table_struct *p_tbl)
{
	__poll_t mask = 0;
	struct mob_dev *mdev = (struct mob_dev *)p_fl->private_data;

	if (SYNC_MODE_POLL != mdev->mode) {
		mask = (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM);
	} else {
		mob_sync_poll_queue_wait(&mdev->poll, p_fl, p_tbl);
		MOB_PRINT("In poll function \n");

		if (mob_sync_get_data(&mdev->poll)) {
			mob_sync_set_data(&mdev->poll, 0);
			MOB_PRINT("Settings read mask\n");
			mask |= (POLLIN | POLLRDNORM);
		}
	}
	MOB_PRINT("Exiting poll function \n");
	return mask;
}

static long mob_ioctl(struct file *p_fl, unsigned int cmd, unsigned long arg)
{
	bool err;
	int ret = 0;
	struct mob_dev *mdev = (struct mob_dev *)p_fl->private_data;

	MOB_PRINT("Command:0x%X, _IOC_TYPE(cmd): 0x%X, _IOC_SIZE(cmd) = %d _IOC_DIR(cmd) = %d\n",
		cmd, _IOC_TYPE(cmd), _IOC_SIZE(cmd), _IOC_DIR(cmd));

	if (MOB_IOCTL_MAGIC != _IOC_TYPE(cmd)){
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd)){
		// Returns 1 for Ok
		err = !access_ok(arg, _IOC_SIZE(cmd));
	}

	if (err)
		return -EFAULT;
	
	switch(cmd) {
		case MOB_DEVICE_HELLO:
		{
			MOB_PRINT("Hello from kernel, dev[%d]\n", mdev->index_of_dev);
		}
		break;
		case MOB_DEVICE_READ:
		{
			MOB_PRINT("In dev read IO\n");
			// copy to user 
			MOB_PRINT("Var read %d\n", mdev->ioctl_var);
			put_user(mdev->ioctl_var, (int *)arg);
		}
		break;
		case MOB_DEVICE_WRITE:
		{
			int var_from_user;
			// copy from user
			get_user(var_from_user, (int *)arg);
			MOB_PRINT("In dev write IO\n");
			MOB_PRINT("Var write %d\n", mdev->ioctl_var);
			mdev->ioctl_var = var_from_user;
		}
		break;
		case MOB_CACHE_READ: 
		{
			ret = copy_to_user((char*)arg, mdev->cache, CACHE_SIZE);
			if (ret)
			{
				MOB_PRINT("Err with IOCTL cache read\n");
			}
		}
		break;
		case MOB_CACHE_WRITE:
		{
			ret = copy_from_user(mdev->cache, (char*)arg, CACHE_SIZE);
			if (ret)
			{
				MOB_PRINT("Err with IOCTL cache read\n");
			}
		}
		break;
	}


	return (long unsigned int)err;
}

/*
 * Set up the char_dev structure for this device.
 */
static void mob_setup_cdev(struct mob_dev *dev, int minor)
{
	int err;
	int devno = MKDEV(mob_major, minor);

	cdev_init(&dev->cdev, &mob_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &mob_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_WARNING "Error %d adding mobchar cdev_add", err);

	printk(KERN_INFO "cdev initialized\n");
}

module_init(mob_init);
module_exit(mob_exit);