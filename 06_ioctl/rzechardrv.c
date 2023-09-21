/**
 * @file   rzechardrv.c
 * @author Rafal Zielinski
 * @date   11 July 2023
 * @version 0.1
 * @brief   An introductory character driver to support reading and writing to chunk of allocated memory
 */

#include <linux/init.h>          // Macros used to mark up functions e.g. __init __exit
#include <linux/kernel.h>        // printk
#include <linux/module.h>
#include <linux/kdev_t.h>        // MAJOR, MINOR, MKDEV..
#include <linux/fs.h>            // register_chrdev_region
#include <linux/device.h>        // class_create
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include "rzechar_ioctl.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rafal Zielinski");
MODULE_DESCRIPTION("introductory char driver");
MODULE_VERSION("0.1");

struct char_dev {
	char *p_data;
	struct cdev cdev;
        struct mutex lock;
};

#define CLASS_NAME  "rz_class"
#define DEVICE_NAME "rzechar"
#define DEVICE_MAX_SIZE 1048
static struct class*  rzecharClass  = NULL;
static struct device* rzecharDevice = NULL;
static struct char_dev* my_devs = NULL;


static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static loff_t dev_llseek(struct file *filp, loff_t off, int whence);
static long dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

static int rzechar_major = 0;
static int rzechar_nr_devs = 1;

module_param(rzechar_nr_devs, int, S_IRUGO);


static struct file_operations fops =
{
        .owner = THIS_MODULE,
        .open = dev_open,
        .read = dev_read,
        .write = dev_write,
        .release = dev_release,
        .llseek = dev_llseek,
        .unlocked_ioctl = dev_ioctl,
};

static int __init rzechardrv_init(void){

        int result = 0;
	dev_t devt = 0;
        size_t i = 0;

        result = alloc_chrdev_region(&devt, 0, rzechar_nr_devs, DEVICE_NAME);
        rzechar_major = MAJOR(devt);

        if (result < 0) printk(KERN_WARNING "rzechar: can't allocate region for %d devs. major: %d\n", rzechar_nr_devs, rzechar_major);
        else printk(KERN_INFO "rzechar: LKM registered with major:%d\n", rzechar_major);

        // Register the device class
        rzecharClass = class_create(THIS_MODULE, CLASS_NAME);
        if (IS_ERR(rzecharClass)){
                unregister_chrdev(rzechar_major, DEVICE_NAME);
                printk(KERN_ALERT "Failed to register device class\n");
                return PTR_ERR(rzecharClass);
        }
        printk(KERN_INFO "rzechar: device class registered correctly\n");

        rzecharDevice = device_create(rzecharClass, NULL, devt, NULL, DEVICE_NAME);
        if (IS_ERR(rzecharDevice)){
                class_destroy(rzecharClass);
                unregister_chrdev(rzechar_major, DEVICE_NAME);
                printk(KERN_ALERT "Failed to create the device\n");
                return PTR_ERR(rzecharDevice);
        }
        printk(KERN_INFO "rzechar: device class created correctly\n");

        my_devs = kmalloc(rzechar_nr_devs * sizeof(struct char_dev), GFP_KERNEL);
        if (!my_devs) {
		result = -ENOMEM;
		printk(KERN_WARNING " ERROR kmalloc dev struct\n");
                device_destroy(rzecharClass, MKDEV(rzechar_major, 0));
                class_destroy(rzecharClass);
		unregister_chrdev(rzechar_major, DEVICE_NAME);
                return result;
	}

        memset(my_devs, 0, rzechar_nr_devs * sizeof(struct char_dev));

        for(i = 0; i < rzechar_nr_devs; i++){  

                my_devs[i].p_data = (char *)kmalloc(DEVICE_MAX_SIZE * sizeof(char), GFP_KERNEL);
                if (!my_devs[i].p_data) {
                        result = -ENOMEM;
                        printk(KERN_WARNING "ERROR kmalloc p_data\n");

                        device_destroy(rzecharClass, MKDEV(rzechar_major, 0));
                        class_destroy(rzecharClass);
		        unregister_chrdev(rzechar_major, DEVICE_NAME);
                        return result;
                }
                mutex_init(&my_devs[i].lock);
                cdev_init(&my_devs[i].cdev, &fops);
                my_devs[i].cdev.owner = THIS_MODULE;
                my_devs[i].cdev.ops = &fops;

                result = cdev_add(&my_devs[i].cdev, devt, rzechar_nr_devs);
                if (result)
                        printk(KERN_NOTICE "rzechar: Error %d adding hello", result);
        }
    
        return result;
}

static void __exit rzechardrv_exit(void){

        size_t i = 0;
        for(i = 0; i < rzechar_nr_devs; i++){  
                cdev_del(&my_devs[i].cdev);
                mutex_destroy(&my_devs[i].lock);
                if ((my_devs[i].p_data) != 0) {
		        kfree(my_devs[i].p_data);
		        printk(KERN_INFO "kfree the string-memory\n");
	        }
        }

        device_destroy(rzecharClass, MKDEV(rzechar_major, 0)); 
        class_destroy(rzecharClass);
        
	if ((my_devs) != 0) {
		kfree(my_devs);
		printk(KERN_INFO " kfree devices\n");
	}
        unregister_chrdev(rzechar_major, "rzechar");

        printk(KERN_INFO "rzechar LKM unloaded!\n");
}

static int dev_open(struct inode *inodep, struct file *filep){

	struct char_dev *dev;
	printk(KERN_INFO "rzechar: opening Device\n" );
        dev = container_of(inodep->i_cdev, struct char_dev, cdev);
        filep->private_data = dev;
	

        printk(KERN_INFO "rzechar: Device has been opened\n" );
        return 0;
}

static int dev_release(struct inode *inodep, struct file *filep){

        printk(KERN_INFO "rzechar:: Device successfully closed\n");
        return 0;
}

ssize_t dev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
        struct char_dev *dev = filp->private_data;
        printk(KERN_INFO "rzechar: dev_read count: %zu f_pos: %lld \n", count, *f_pos);
        
        if (!dev) {
                printk(KERN_WARNING "rzechar: no device data \n");
                return -EFAULT;
        }
        if (*f_pos >= DEVICE_MAX_SIZE){          
                printk(KERN_WARNING "rzechar: reached end of file\n");
                return 0;
        }
        if (*f_pos + count > DEVICE_MAX_SIZE){
                printk(KERN_WARNING "rzechar: trying to read more than possible.\n");
                count = DEVICE_MAX_SIZE - *f_pos;
        }

        if (!mutex_trylock(&dev->lock)){
                printk(KERN_WARNING "failed ps read, busy\n");
		return -EBUSY;
	}
        if (copy_to_user(buf, dev->p_data + *f_pos, count)) {
                printk(KERN_WARNING "rzechar: can't copy to user. \n");
	        return -EFAULT;
	}

        *f_pos = *f_pos + count;

        mutex_unlock(&dev->lock);
    
        printk(KERN_INFO "rzechar: READ finished\n");
        return count;
}

ssize_t dev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
        struct char_dev *dev = filp->private_data;
        printk(KERN_INFO "rzechar: dev_write count: %zu f_pos: %lld \n", count, *f_pos);

        if (!dev) {
                printk(KERN_WARNING "rzechar: no device data \n");
                return -EFAULT;
        }    
        if (*f_pos >= DEVICE_MAX_SIZE){
                printk(KERN_WARNING "rzechar: reached end of file\n");
                return 0;
        }
        if (*f_pos + count > DEVICE_MAX_SIZE){
                printk(KERN_WARNING "rzechar: trying to write more than possible. Aborting read\n");
                count = DEVICE_MAX_SIZE - *f_pos;
        }
        
        if (!mutex_trylock(&dev->lock)){
                printk(KERN_WARNING "failed ps write, busy\n");
		return -EBUSY;
	}
        if (copy_from_user(dev->p_data + *f_pos, buf, count)){
                printk(KERN_WARNING "rzechar: can't copy from  user. \n");
	        return -EFAULT;
        }

        *f_pos = *f_pos + count;
        mutex_unlock(&dev->lock);
    
        printk(KERN_INFO "rzechar: WRITE finished\n");
        return count;
}

loff_t dev_llseek(struct file *filp, loff_t off, int whence)
{
	struct char_dev *dev = (struct char_dev *)filp->private_data;
	loff_t npos;
	
	if (!dev) {
		printk(KERN_WARNING "failed, no device data\n");
		return -EFAULT;
	}

	switch (whence) {
	case SEEK_SET:
		if (off >= DEVICE_MAX_SIZE)
			return -EFBIG;
		npos = off;
		break;

	case SEEK_CUR:
		if (filp->f_pos + off >= DEVICE_MAX_SIZE)
			return -EFBIG;
		npos = filp->f_pos + off;
		break;

	case SEEK_END:
		npos = DEVICE_MAX_SIZE + off;
		if (npos >= DEVICE_MAX_SIZE)
			return -EFBIG;
		break;

	default: /* not expected value */
		return -EINVAL;
	}
	if (npos < 0)
		return -EINVAL;
	filp->f_pos = npos;
	return npos;
}

long dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct char_dev *dev = filp->private_data;
	struct iocdata ioctl_data;
        void __user *up = (void __user *)arg;

	if (_IOC_TYPE(cmd) != RZECHAR_IOC_MAGIC)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE)) {
		if (!access_ok( (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;

		if (_IOC_SIZE(cmd) > sizeof(ioctl_data))
			return -ENOTTY;

                if (copy_from_user(&ioctl_data, up, _IOC_SIZE(cmd))) {
			pr_err("copy from user failed!\n");
			return -EFAULT;
		}
                printk(KERN_NOTICE "rzechar: ioctl size %d ", _IOC_SIZE(cmd));
                printk(KERN_NOTICE "rzechar: ioctl read size %d offset %d", ioctl_data.size, ioctl_data.offset);
	}
	if (!dev) {
		pr_warn("failed, no device data\n");
		return -EINVAL;
	}

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	switch (cmd) {
        case RZECHAR_IOC_HELLO:
		printk(KERN_INFO "hello ioctl from rzechar driver\n");
		break;

	case RZECHAR_IOC_RESET:
		memset(dev->p_data, 0, DEVICE_MAX_SIZE);
		break;
	// 	if (ioc_data.size + ioc_data.off > DEVICE_MAX_SIZE) {
	// 		ret = -EFBIG;
	// 	} else if (copy_from_user(dev->p_data + ioc_data.off, ioc_data.data,
	// 				  ioc_data.size)) {
	// 		pr_err("copy from user failed!\n");
	// 		ret = -EFAULT;
	// 	}
	// 	break;

	 case RZECHAR_IOC_READ:               
		if (ioctl_data.size + ioctl_data.offset > DEVICE_MAX_SIZE) {
			ret = -EFBIG;
		} else 
                if (copy_to_user(ioctl_data.data, dev->p_data, DEVICE_MAX_SIZE)) {
			pr_err("copy to user failed!\n");
			ret = -EFAULT;
		}
	 	break;

	default:
		ret = -ENOTTY;
	}

	mutex_unlock(&dev->lock);

	return ret;
}

module_init(rzechardrv_init);
module_exit(rzechardrv_exit);