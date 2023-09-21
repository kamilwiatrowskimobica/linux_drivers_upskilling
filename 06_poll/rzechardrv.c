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
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/kobject.h>
#include <linux/err.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rafal Zielinski");
MODULE_DESCRIPTION("introductory char driver");
MODULE_VERSION("0.1");

struct char_dev {
	char *p_data;
	struct cdev cdev;
        struct mutex lock;
};

//Waitqueue
DECLARE_WAIT_QUEUE_HEAD(wait_queue_etx_data);

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
static unsigned int etx_poll(struct file *filp, struct poll_table_struct *wait);

static int rzechar_major = 0;
static int rzechar_nr_devs = 1;
static bool can_write = false;
static bool can_read  = false;

//module_param(rzechar_nr_devs, int, S_IRUGO);


static struct file_operations fops =
{
        .owner = THIS_MODULE,
        .open = dev_open,
        .read = dev_read,
        .write = dev_write,
        .release = dev_release,
        .llseek = dev_llseek,
        .poll   = etx_poll,
};

static int __init rzechardrv_init(void){

        int result = 0;
	dev_t devt = 0;
        size_t i = 0;

        result = alloc_chrdev_region(&devt, 0, rzechar_nr_devs, DEVICE_NAME);
        rzechar_major = MAJOR(devt);

        if (result < 0) printk(KERN_WARNING "rzechar: can't allocate region for %d devs. major: %d\n", rzechar_nr_devs, rzechar_major);
        else printk(KERN_INFO "rzechar: LKM registered for %d devs with major:%d\n", rzechar_nr_devs, rzechar_major);

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
                mutex_destroy(&my_devs[i].lock);
                cdev_del(&my_devs[i].cdev);
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
        can_write = true;
        printk(KERN_WARNING "can_write set to true\n");
  
        //wake up the waitqueue
        wake_up(&wait_queue_etx_data);
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
        can_read = true;
        printk(KERN_WARNING "can_read set to true\n");
        //wake up the waitqueue
        wake_up(&wait_queue_etx_data);
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

static unsigned int etx_poll(struct file *filp, struct poll_table_struct *wait)
{
  __poll_t mask = 0;
  
  poll_wait(filp, &wait_queue_etx_data, wait);
  pr_info("Poll function\n");
  
  if( can_read )
  {
    can_read = false;
    printk(KERN_WARNING "can_read set to false\n");
    mask |= ( POLLIN | POLLRDNORM );
  }
  
  if( can_write )
  {
    can_write = false;
        printk(KERN_WARNING "can_write set to false\n");
    mask |= ( POLLOUT | POLLWRNORM );
  }
    
  return mask;
}

module_init(rzechardrv_init);
module_exit(rzechardrv_exit);