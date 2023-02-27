#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/semaphore.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include <linux/poll.h>

#include <woab_debug.h>
#include <woab_sync.h>
#include <woab_seq_file.h>
#include <woab_ioctl.h>

// FIXME error handling in init function

MODULE_LICENSE("GPL");
MODULE_AUTHOR("woab");
MODULE_DESCRIPTION("A simple Linux char driver");
MODULE_VERSION("0.1");

/*
 * Global defines
 */
#define DEVICE_NAME "woabchar"
#define DEFAULT_BUFFER 32
#define DEFAULT_CACHE 1024

/*
 * Types definition
 */
struct woab_dev {
	struct cdev cdev;
	int index;
	char *data;
	char *cache;
	size_t data_length;
	size_t buffer_length;
	int number_opens;

	// synchronisation
	sync_mode mode;
	union {
		struct completion comp;
		struct semaphore sem;
		struct sync_mode_poll poll;
	};
	spinlock_t spinlock;
};

/*
 * File scope deeclarations
 */
static int woab_devchar_open(struct inode *, struct file *);
static int woab_devchar_release(struct inode *, struct file *);
static ssize_t woab_devchar_read(struct file *, char *, size_t, loff_t *);
static ssize_t woab_devchar_write(struct file *, const char *, size_t,
				  loff_t *);
static long woab_devchar_ioctl(struct file *, unsigned int, unsigned long);
static unsigned int woab_devchar_poll(struct file *,
				      struct poll_table_struct *);
static int __init woab_devchar_init(void);
static void __exit woab_devchar_exit(void);
static int woab_device_alloc(unsigned int dev_count);
static int woab_device_alloc_buffer(struct woab_dev *dev, size_t buffer_length);
static void woab_devchar_setup_cdev(struct woab_dev *dev, int index);
static void woab_devchar_delete_cdev(struct woab_dev *dev);
static void woab_device_cleanup(void);

/*
 * File scope variables
 */
static int woabchar_major;
static int woabchar_minor;
static unsigned int woabchar_nr_devices = 4;
static struct woab_dev *woab_device;
static struct file_operations woab_devchar_fops = {
	.owner = THIS_MODULE,
	.open = woab_devchar_open,
	.read = woab_devchar_read,
	.write = woab_devchar_write,
	.release = woab_devchar_release,
	.unlocked_ioctl = woab_devchar_ioctl,
	.poll = woab_devchar_poll,
};
struct kmem_cache *woab_dev_cache;

/*
 * Global scope function definitions
 */
int woab_device_return_opens(int index)
{
	if (index < woabchar_nr_devices)
		return woab_device[index].number_opens;
	return -1;
}

/*
 * File scope function definitions
 */
static int woab_device_alloc_buffer(struct woab_dev *dev, size_t buffer_length)
{
	if (!dev)
		return -ENXIO;
	kfree(dev->data);
	dev->data = (char *)kmalloc(buffer_length * sizeof(char), GFP_KERNEL);
	if (!dev->data) {
		WERROR("Failed to alocate memory buffer\n");
		return -ENOMEM;
	}
	dev->buffer_length = buffer_length;

	dev->cache = kmem_cache_alloc(woab_dev_cache, GFP_KERNEL);
	if (!dev->cache) {
		WERROR("Failed to alocate cache buffer\n");
		return -ENOMEM;
	}
	return 0;
}

static int woab_device_alloc(unsigned int dev_count)
{
	woab_device = kcalloc(dev_count, sizeof(struct woab_dev), GFP_KERNEL);
	if (!woab_device) {
		WERROR("Failed to alocate device struct\n");
		return -ENOMEM;
	}

	return 0;
}

static void woab_device_cleanup(void)
{
	int i;
	for (i = 0; i < woabchar_nr_devices; i++) {
		kfree(woab_device[i].data);
		kmem_cache_free(woab_dev_cache, woab_device[i].cache);
	}
	kfree(woab_device);
}

static void woab_devchar_setup_cdev(struct woab_dev *dev, int index)
{
	int err, devno;
	devno = MKDEV(woabchar_major, woabchar_minor + index);

	cdev_init(&dev->cdev, &woab_devchar_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &woab_devchar_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		WERROR("Error %d adding cdev", err);
}

static void woab_devchar_delete_cdev(struct woab_dev *dev)
{
	cdev_del(&dev->cdev);
}

static int __init woab_devchar_init(void)
{
	int ret, i;
	dev_t dev = 0;

	ret = alloc_chrdev_region(&dev, woabchar_minor, woabchar_nr_devices,
				  DEVICE_NAME);
	woabchar_major = MAJOR(dev);

	if (ret < 0) {
		WERROR("Can't get major %d\n", woabchar_major);
		return ret;
	}

	ret = woab_device_alloc(woabchar_nr_devices);
	if (ret)
		return ret;

	woab_dev_cache = kmem_cache_create(DEVICE_NAME, DEFAULT_CACHE, 0,
					   SLAB_HWCACHE_ALIGN, NULL);
	if (!woab_dev_cache) {
		kfree(woab_device);
		return -ENOMEM;
	}

	for (i = 0; i < woabchar_nr_devices; i++)
		woab_devchar_setup_cdev(&woab_device[i], i);

	for (i = 0; i < woabchar_nr_devices; i++) {
		ret = woab_device_alloc_buffer(&woab_device[i], DEFAULT_BUFFER);
		woab_device[i].mode = i;
		woab_device[i].index = i;
		if (SYNC_MODE_COMPLETION == woab_device[i].mode)
			woab_sync_init_completion(&woab_device[i].comp);
		else if (SYNC_MODE_SEMAPHORE == woab_device[i].mode)
			woab_sync_init_semaphore(&woab_device[i].sem);
		else if (SYNC_MODE_POLL == woab_device[i].mode)
			woab_sync_poll_wait_queue_init(&woab_device[i].poll);
		woab_sync_spin_lock_init(&woab_device[i].spinlock);

		if (ret)
			return ret;
	}

	ret = woab_seq_file_init();
	if (ret)
		return ret;

	WINFO("Device successfully initialized\n");

	return 0;
}

static void __exit woab_devchar_exit(void)
{
	dev_t devno;
	int i;

	woab_seq_file_close();

	for (i = 0; i < woabchar_nr_devices; i++)
		woab_devchar_delete_cdev(&woab_device[i]);

	devno = MKDEV(woabchar_major, woabchar_minor);
	unregister_chrdev_region(devno, woabchar_nr_devices);

	woab_device_cleanup();

	if (woab_dev_cache)
		kmem_cache_destroy(woab_dev_cache);

	WINFO("Device successfully uninitialized\n");
}

static int woab_devchar_open(struct inode *inodep, struct file *filep)
{
	struct woab_dev *wdev;

	wdev = container_of(inodep->i_cdev, struct woab_dev, cdev);
	filep->private_data = wdev;

	if (SYNC_MODE_NONE != wdev->mode)
		woab_sync_spin_lock(&wdev->spinlock);
	wdev->number_opens++;
	if (SYNC_MODE_NONE != wdev->mode)
		woab_sync_spin_unlock(&wdev->spinlock);

	WCHARDEBUG("Device has been opened %d time(s)\n", wdev->number_opens);
	return 0;
}

static ssize_t woab_devchar_read(struct file *filep, char *buffer, size_t len,
				 loff_t *offset)
{
	int error_count;
	size_t data_to_send;
	struct woab_dev *wdev = (struct woab_dev *)filep->private_data;

	if (SYNC_MODE_COMPLETION == wdev->mode &&
	    woab_sync_wait_for_completion(&wdev->comp,
					  SYNC_MODE_COMPLETION_TIMEOUT))
		goto no_data;
	else if (SYNC_MODE_SEMAPHORE == wdev->mode &&
		 woab_sync_down(&wdev->sem)) {
		goto no_data;
	}

	if (*offset >= wdev->data_length) {
		WCHARDEBUG("No more data offset = %lld data length = %lu\n",
			   *offset, wdev->data_length);
		goto no_data_unlock_semaphore;
	}

	data_to_send = len < wdev->data_length ? len : wdev->data_length;

	error_count = copy_to_user(buffer, &wdev->data[*offset], data_to_send);

	if (SYNC_MODE_SEMAPHORE == wdev->mode)
		woab_sync_up(&wdev->sem);

	if (error_count) {
		WERROR("Failed to send %d characters to the user\n",
		       error_count);
		goto write_error;
	}

	WCHARDEBUG("Sent %lu characters to the user\n", data_to_send);
	*offset += data_to_send;

	return data_to_send;

no_data_unlock_semaphore:
	if (SYNC_MODE_SEMAPHORE == wdev->mode)
		woab_sync_up(&wdev->sem);

no_data:
	return 0;

write_error:
	return -EFAULT;
}

static ssize_t woab_devchar_write(struct file *filep, const char *buffer,
				  size_t len, loff_t *offset)
{
	int uncopied, ret;
	struct woab_dev *wdev = (struct woab_dev *)filep->private_data;

	if (SYNC_MODE_SEMAPHORE == wdev->mode && woab_sync_down(&wdev->sem))
		goto semaphore_fail;

	if (len > wdev->buffer_length) {
		ret = woab_device_alloc_buffer(wdev, len);
		if (ret) {
			printk(KERN_ERR "alloc failed\n");
			return ret;
		}
		WCHARDEBUG("New memory alocated succesfully"
			   " with length of %lu\n",
			   len);
	}

	wdev->data_length = len + *offset;
	uncopied = copy_from_user(wdev->data + *offset, buffer, len);
	WCHARDEBUG("Received %lu (%d uncopied) characters from the user", len,
		   uncopied);

	*offset += len;

	if (SYNC_MODE_COMPLETION == wdev->mode)
		woab_sync_complete(&wdev->comp);
	else if (SYNC_MODE_SEMAPHORE == wdev->mode)
		woab_sync_up(&wdev->sem);
	else if (SYNC_MODE_POLL == wdev->mode) {
		woab_sync_poll_wait_queue_wake_up(&wdev->poll);
	}
	return len;

semaphore_fail:
	return -ERESTARTSYS;
}

static int woab_devchar_release(struct inode *inodep, struct file *filep)
{
	WCHARDEBUG("Device successfully closed\n");
	return 0;
}

static long woab_devchar_ioctl(struct file *filep, unsigned int cmd,
			       unsigned long arg)
{
	int err = 0, ret;
	struct woab_dev *wdev = (struct woab_dev *)filep->private_data;

	WCHARDEBUG(
		"cmd: 0x%X, _IOC_TYPE(cmd): 0x%X, _IOC_SIZE(cmd) = %d _IOC_DIR(cmd) = %d\n",
		cmd, _IOC_TYPE(cmd), _IOC_SIZE(cmd), _IOC_DIR(cmd));
	WCHARDEBUG("WOAB_IOCTL_IOC_MAGIC: 0x%X\n", WOAB_IOCTL_IOC_MAGIC);
	if (_IOC_TYPE(cmd) != WOAB_IOCTL_IOC_MAGIC) {
		WERROR("_IOC_TYPE(cmd) [0x%X] != WOAB_IOCTL_IOC_MAGIC [0x%X]. Aborting ioctl\n",
		       _IOC_TYPE(cmd), WOAB_IOCTL_IOC_MAGIC);
		return -ENOTTY;
	}

	if (_IOC_NR(cmd) > WOAB_IOCTL_IOC_MAXNR) {
		WERROR("_IOC_NR(cmd) [%d] > WOAB_IOCTL_IOC_MAXNR [%d]. Aborting ioctl\n",
		       _IOC_NR(cmd), WOAB_IOCTL_IOC_MAXNR);
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd))
		err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));

	if (err)
		return -EFAULT;

	switch (cmd) {
	case WOAB_DEVICE_HELLO:
		WINFO("Simple hello from hernel. I am device %d\n",
		      wdev->index);
		break;
	case WOAB_DEVICE_READ_SYNC_MODE:
		WCHARDEBUG("Executing WOAB_DEVICE_READ_SYNC_MODE\n");
		put_user(wdev->mode, (int32_t *)arg);

		break;
	case WOAB_DEVICE_WRITE_SYNC_MODE: {
		int mode;
		WCHARDEBUG("Executing WOAB_DEVICE_WRITE_SYNC_MODE\n");
		// FIXME synchronisation
		get_user(mode, (int32_t *)arg);
		if (SYNC_MODE_NONE == mode) {
			wdev->mode = SYNC_MODE_NONE;
		} else if (SYNC_MODE_COMPLETION == mode) {
			wdev->mode = SYNC_MODE_COMPLETION;
			woab_sync_init_completion(&wdev->comp);
		} else if (SYNC_MODE_SEMAPHORE == mode) {
			wdev->mode = SYNC_MODE_SEMAPHORE;
			woab_sync_init_semaphore(&wdev->sem);
		} else {
			WERROR("Wrong sync mode passed as parameter!\n");
		}
		break;
	}
	case WOAB_DEVICE_WRITE_CACHE:
		ret = copy_from_user(wdev->cache, (char *)arg, DEFAULT_CACHE);
		if (ret)
			WERROR("IOCTL command %d returned unexpected value\n",
			       cmd);
		break;
	case WOAB_DEVICE_READ_CACHE:
		ret = copy_to_user((char *)arg, wdev->cache, DEFAULT_CACHE);
		if (ret)
			WERROR("IOCTL command %d returned unexpected value\n",
			       cmd);
		break;
	default:
		WERROR("Validation went wrong!\n");
	}

	return 0;
}

static unsigned int woab_devchar_poll(struct file *filep,
				      struct poll_table_struct *wait)
{
	__poll_t mask = 0;
	struct woab_dev *wdev = (struct woab_dev *)filep->private_data;

	if (SYNC_MODE_POLL != wdev->mode) {
		mask = (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM);
	} else {
		woab_sync_poll_wait_queue_wait(&wdev->poll, filep, wait);
		WCHARDEBUG("Poll function \n");

		if (woab_sync_poll_get_data_readable(&wdev->poll)) {
			woab_sync_poll_set_data_readable(&wdev->poll, 0);
			mask |= (POLLIN | POLLRDNORM);
		}
	}
	return mask;
}

module_init(woab_devchar_init);
module_exit(woab_devchar_exit);
