#include "twdrv_fops.h"
#include "twdrv_data.h"


extern int twdrv_max_dev_size;
extern struct twdrv_dev *twdrv_devices;


int twdev_fops_open(struct inode *inode, struct file *filp)
{
    struct twdrv_dev *dev;
    
    dev = container_of(inode->i_cdev, struct twdrv_dev, cdev);
    filp->private_data = dev;
    printk(KERN_INFO "[twdrv.fops] Open successful\n");

	return 0;
}


int twdev_fops_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "[twdrv_fops] Release successful\n");
    return 0;
}


ssize_t twdev_fops_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    if (count > twdrv_max_dev_size){
        printk(KERN_WARNING "[twdrv_fops] Read Failed - Too big, aborting\n");
        return -EFBIG;
    }
    if (copy_to_user(buf, (void*)twdrv_devices->p_data, count)) {
        printk(KERN_WARNING "[twdrv_fops] Read: copy_to_user failed\n");
		return -EPERM;
    }
    printk(KERN_INFO "[twdrv_fops] Read successful\n");
    return 0;

}


ssize_t twdev_fops_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    if (count > twdrv_max_dev_size){
        printk(KERN_WARNING "[twdrv_fops] Write failed - too big, aborting\n");
        return -EFBIG;
    }
    if (copy_from_user((void*)twdrv_devices->p_data, buf, count)) {
        printk(KERN_WARNING "[twdrv_fops] Write: copy_from_user failed\n");
        return -EPERM;
    }
    printk(KERN_INFO "[twdrv_fops] Write successful\n");
    return 0;
}


int twdrv_fops_init(struct file_operations *fops)
{


    fops->release = twdev_fops_release;
    fops->open    = twdev_fops_open;
    fops->read    = twdev_fops_read;
    fops->write   = twdev_fops_write;

    printk("[twdrv_fops] Init successful");
    return 0;
}