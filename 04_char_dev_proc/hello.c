#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h> // Required for the copy to user function
#include <linux/device.h> //device_create
//#include <linux/device/class.h> //class_create
#include <linux/moduleparam.h>
#include <linux/cdev.h>
//#include <sys/stat.h> //mknod
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>






#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/miscdevice.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/toshiba.h>

#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/fs.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mateusz Leszczynski, Mobica Ltd.");
MODULE_DESCRIPTION("A super-simple LKM.");
MODULE_VERSION("0.1");

#define DEVICE_NAME "hellodrvchar" /* device at /dev/hellodrvchar */
#define CLASS_NAME "hellodrv"
#define DEV "/dev/foo"
#define MAX_MESSAGE_LEN 256

#ifdef MTLE_DEBUG
#pragma GCC push_options
#pragma GCC optimize ("O0")
#endif // MTLE_DEBUG
static int majorNumber = 0;
static int minorNumber = 0;
static int nr_devs = 1;
static char message[MAX_MESSAGE_LEN] = {
	0
}; ///< string that is passed from userspace
static short size_of_message; ///< size of the string stored
static int numberOpens = 0; ///< Counts the number of times the device is opened
static struct class *hellodrvcharClass = NULL;
static struct device *hellodrvcharDevice = NULL;
struct hello_dev *hello_devices = NULL;
#ifdef MTLE_DEBUG
#pragma GCC pop_options
#endif // MTLE_DEBUG

module_param(majorNumber, int, S_IRUGO);
module_param(minorNumber, int, S_IRUGO);
module_param(nr_devs, int, S_IRUGO);


static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static void __exit hello_exit(void);
static int ct_open(struct inode *inode, struct file *file);
static void *ct_seq_start(struct seq_file *s, loff_t *pos);
static void *ct_seq_next(struct seq_file *s, void *v, loff_t *pos);
static void ct_seq_stop(struct seq_file *s, void *v);
static int ct_seq_show(struct seq_file *s, void *v);


typedef struct hello_dev {
        char *p_data;
        struct cdev cdev;
        struct mutex mut;
	size_t size; /* actual number of bytes in data */
} hello_dev;

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

/*
 * Tie them all together into a set of seq_operations.
 */
static struct seq_operations ct_seq_ops = {
	.start = ct_seq_start,
	.next  = ct_seq_next,
	.stop  = ct_seq_stop,
	.show  = ct_seq_show
};

/*
 * The file operations structure contains our open function along with
 * set of the canned seq_ ops.
 */
//static struct file_operations ct_file_ops = {
////static struct proc_ops ct_file_ops = {
////	.owner   = THIS_MODULE,
//	.open    = ct_open,
//	.read    = seq_read,
//	.llseek  = seq_lseek,
//	.release = seq_release
//};
static struct proc_ops ct_file_ops = {
//	.owner   = THIS_MODULE,
	.proc_open    = ct_open,
	.proc_read    = seq_read,
	//.proc_llseek  = seq_lseek,
	.proc_lseek  = seq_lseek,
	.proc_release = seq_release
};

static int ct_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &ct_seq_ops);
};

/*
 * The sequence iterator functions.  We simply use the count of the
 * next line as our internal position.
 */
static void *ct_seq_start(struct seq_file *s, loff_t *pos)
{
	loff_t *spos = kmalloc(sizeof(loff_t), GFP_KERNEL);
	if (! spos)
		return NULL;
	*spos = *pos;
	return spos;
}

static void *ct_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	loff_t *spos = (loff_t *) v;
	*pos = ++(*spos);
	return spos;
}

static void ct_seq_stop(struct seq_file *s, void *v)
{
	kfree (v);
}

/*
 * The show function.
 */
static int ct_seq_show(struct seq_file *s, void *v)
{
	loff_t *spos = (loff_t *) v;
	seq_printf(s, "%Ld\n", *spos);
	msleep_interruptible(100);
	return 0;
}

int hello_trim(struct hello_dev *dev)
{
        kfree(dev->p_data);
        dev->p_data = NULL;

        return 0;
}

/*
 * Set up the char_dev structure for this device.
 */
static void hello_setup_cdev(struct hello_dev *dev, int index)
{
	int err, devno = MKDEV(majorNumber, minorNumber + index);
	mode_t mode =  S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH; // character device
#ifdef MTLE_DEBUG
		printk(KERN_INFO "Adding cdev%d:%d\n", MAJOR(devno), MINOR(devno));
#endif
    
	cdev_init(&dev->cdev, &fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &fops;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
        {
		printk(KERN_NOTICE "Error %d adding cdev%d\n", err, index);
        }
        dev->p_data = kzalloc(MAX_MESSAGE_LEN, GFP_KERNEL);
        if(!dev->p_data)
        {
                printk(KERN_NOTICE "Error allocating mem fir device data\n");
        }
//	err = mknod(DEV, mode, devno);
//	if (err)
//		printk(KERN_NOTICE "Error %d adding mknod %d", err, index);
}
static int hello_init(void)
{
//	long rc = 0;
//
//	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
//	if (majorNumber < 0) {
//		printk(KERN_ALERT "Failed to register a major number\n");
//		return majorNumber;
//	}
//
//	// Register the device class
//	hellodrvcharClass = class_create(THIS_MODULE, CLASS_NAME);
//	if (IS_ERR(hellodrvcharClass)) {
//		printk(KERN_ALERT "Failed to register device class\n");
//		rc = PTR_ERR(
//			hellodrvcharClass); // Correct way to return an error on a pointer
//		goto err_class;
//	}
//
//	// Register the device driver
//	hellodrvcharDevice = device_create(hellodrvcharClass, NULL,
//					   MKDEV(majorNumber, 0), NULL,
//					   DEVICE_NAME);
//	if (IS_ERR(hellodrvcharDevice)) { // Clean up if there is an error
//		printk(KERN_ALERT "Failed to create the device\n");
//		rc = PTR_ERR(hellodrvcharDevice);
//		goto err_dev;
//	}
//	printk(KERN_INFO "%s: device class created correctly\n",
//	       DEVICE_NAME); // Made it! device was initialized
//
//	printk(KERN_INFO "MODULE LOADED\n");
//	return rc;
//
//err_dev:
//	device_destroy(hellodrvcharClass, MKDEV(majorNumber, 0));
//err_class:
//	class_destroy(hellodrvcharClass);
//	//TODO is unregister_chrdev() unnecessary?
//	unregister_chrdev(majorNumber, DEVICE_NAME);
//	return rc;
//



	int result, i;
	dev_t dev = 0;
        struct proc_dir_entry *entry;

/*
 * Get a range of minor numbers to work with, asking for a dynamic
 * major unless directed otherwise at load time.
 */
        if (majorNumber)
        {
                dev = MKDEV(majorNumber, minorNumber);
                result = register_chrdev_region(dev, nr_devs, DEVICE_NAME);               
        }
        else
        {
                result = alloc_chrdev_region(&dev, minorNumber, nr_devs, DEVICE_NAME);
                majorNumber = MAJOR(dev);
        }
        if (result < 0)
        {
                printk(KERN_WARNING "can't get major number %d", majorNumber);
                return result;
        }

        /* 
	 * allocate the devices -- we can't have them static, as the number
	 * can be specified at load time
	 */
	hello_devices = kmalloc(nr_devs * sizeof(struct hello_dev), GFP_KERNEL);
	if(!hello_devices)
        {
                result = -ENOMEM;
                goto fail;
        }
        // kmalloc could be changed to kzalloc, then this memset would be useless
        memset(hello_devices, 0, nr_devs * sizeof(struct hello_dev));

        /* Initialize each device. */
        for (i = 0; i < nr_devs; i++)
        {
                mutex_init(&hello_devices[i].mut);
                hello_setup_cdev(&hello_devices[i], i);
        }

//        /* At this point call the init function for any friend device */
// //TODO       
//	dev = MKDEV(majorNumber, minorNumber + nr_devs);
//	dev += scull_p_init(dev);
//	dev += scull_access_init(dev);
//#ifdef SCULL_DEBUG /* only when debugging */
//	scull_create_proc();
//#endif


        // seq_file
        // https://www.kernel.org/doc/html/next/filesystems/seq_file.html
        // based on J. Corbet's https://lwn.net/Articles/22359/
        entry = proc_create("mtle_sequence", 0, NULL, &ct_file_ops);
        if (!entry)
        {
                goto fail;
        }
        //entry->proc_fops = &ct_file_ops;
        //entry->proc_dir_ops = &ct_file_ops;

	return 0; /* succeed */

  fail:
	hello_exit();
	return result;

}

static int dev_open(struct inode *inode, struct file *file)
{
	struct hello_dev *hdev;

	numberOpens++;
	printk(KERN_INFO "%s: Device has been opened. Totalling %d time(s)\n", DEVICE_NAME,
	       numberOpens);
	hdev = container_of(inode->i_cdev, struct hello_dev, cdev);
	file->private_data = hdev;

	return 0;
}

static ssize_t dev_read(struct file *filep, char *buf, size_t len,
			loff_t *off)
//{
//	int error_count = 0;
//	// copy_to_user has the format ( * to, *from, size) and returns 0 on success
//	error_count = copy_to_user(buffer, message, size_of_message);
//
//	if (error_count == 0) {
//		printk(KERN_INFO "%s: Sent %d characters to the user\n",
//		       DEVICE_NAME, size_of_message);
//		return (size_of_message = 0);
//	} else {
//		printk(KERN_INFO
//		       "%s: Failed to send %d characters to the user\n",
//		       DEVICE_NAME, error_count);
//		return -EFAULT;
//	}
//}
//
//
//static ssize_t mobdev_read(struct file *filp, char __user *buf, size_t len,
//			   loff_t *off)
{
	hello_dev *hdev = (struct hello_dev *)filep->private_data;
	size_t size = hdev->size - *off;
		printk(KERN_INFO "dev_read fn\n");
	pr_info("dev_read, len = %zu, off = %lld\n", len, *off);
	if (!hdev) {
		pr_warn("failed, no device data\n");
		return -EFAULT;
	}

	if (*off >= hdev->size) {
		size = 0;
		goto out;
	}
	if (size > len)
		size = len;
	if (size == 0)
		goto out;

	hdev->size -= size;

	//Copy the data from the kernel space to the user-space
	if (copy_to_user(buf, hdev->p_data + *off, size)) {
		pr_warn("copy to user failed !\n");
		return -EFAULT;
	}
	*off += size;
out:
	pr_info("Data Read %zu: Done!\n", size);
	return size;
}


static ssize_t dev_write(struct file *filp, const char *buffer, size_t len,
			 loff_t *off)
{
	long left;
	hello_dev *hdev = filp->private_data;

	if(!hdev)
		printk(KERN_INFO "hdev empty!\n");
	if(!hdev->p_data)
	{
		printk(KERN_INFO "hdev->p_data empty!\n");
		return -EFAULT;
	}

        pr_info("dev_write, len = %zu, off = %lld\n", len, *off);
	// sanity check
	if (len >= MAX_MESSAGE_LEN) {
		printk(KERN_INFO
		       "%s: too long message from the user. Should be less than %u chars\n",
		       DEVICE_NAME, MAX_MESSAGE_LEN);
		return -ENOBUFS;
	}

	//left = copy_from_user(message, buffer, len);
	left = copy_from_user(hdev->p_data, buffer, len);
	if (left) {
		printk(KERN_INFO
		       "%s: problem on receiving characters from the user\n",
		       DEVICE_NAME);
	}
//
	if (len + *off > hdev->size)
		hdev->size = len + *off;
	*off += len;
//
	printk(KERN_INFO "%s: Received %zu characters from the user\n",
	       DEVICE_NAME, len);
	//printk(KERN_INFO "%s: Received message: %s\n", DEVICE_NAME, message);
	printk(KERN_INFO "%s: Received message: %s\n", DEVICE_NAME, hdev->p_data);
	return len;
}

static int dev_release(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "%s: Device successfully closed\n", DEVICE_NAME);
	return 0;
}

static void __exit hello_exit(void)
{
//	device_destroy(hellodrvcharClass,
//		       MKDEV(majorNumber, 0)); // remove the device
//	class_unregister(hellodrvcharClass); // unregister the device class
//	class_destroy(hellodrvcharClass); // remove the device class
//	//TODO is unregister_chrdev() unnecessary?
//	unregister_chrdev(majorNumber,
//			  DEVICE_NAME); // unregister the major number



	int i;
	dev_t devno = MKDEV(majorNumber, minorNumber);

	/* Get rid of our char dev entries */
	if (hello_devices) {
		for (i = 0; i < nr_devs; i++) {
			hello_trim(hello_devices + i);
			cdev_del(&hello_devices[i].cdev);
		}
		kfree(hello_devices);
	}

//#ifdef SCULL_DEBUG /* use proc only if debugging */
//	scull_remove_proc();
//#endif

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, nr_devs);

//	/* and call the cleanup functions for friend devices */
//	scull_p_cleanup();
//	scull_access_cleanup();


        remove_proc_entry("mtle_sequence", NULL);


	printk(KERN_INFO "MODULE UNLOADED\n");
}

module_init(hello_init);
module_exit(hello_exit);
