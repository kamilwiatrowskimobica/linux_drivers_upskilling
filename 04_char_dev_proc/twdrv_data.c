#include <linux/slab.h>	
#include "twdrv_data.h"

extern int twdrv_major;
extern int twdrv_minor;
extern int twdrv_nr_devs;
extern int twdrv_max_dev_size;

int twdrv_data_alloc(struct twdrv_dev **devices)
{
    printk(KERN_INFO "[twdrv_data] alloc driver memory\n");
    *devices = kmalloc(twdrv_nr_devs * sizeof(struct twdrv_dev), GFP_KERNEL);
    if (!devices) {
        
        printk(KERN_WARNING "[twdrv_data] ERROR kmalloc dev struct\n");
        return -ENOMEM;
    }

    memset(*devices, 0, twdrv_nr_devs * sizeof(struct twdrv_dev));

    (*devices)->p_data = (char*)kmalloc(twdrv_max_dev_size * sizeof(char), GFP_KERNEL);
    if (!(*devices)->p_data) {
        kfree(*devices);
        printk(KERN_WARNING "[twdrv_data] ERROR kmalloc p_data\n");
        return -ENOMEM;
    }

    return 0;
}

void twdrv_data_free(struct twdrv_dev * devices)
{
    if((devices->p_data) != 0){
        kfree(devices->p_data);
        printk(KERN_INFO "[twdrv_data] kfree p_data\n");
    }
    if((devices) != 0){
        kfree(devices);
        printk(KERN_INFO "[twdrv_data] kfree twdrv_devices\n");
    }
}

void twdrv_data_setup_cdev(struct twdrv_dev *devices, struct file_operations *fops)
{
	int result = 0; 
    int devno = MKDEV(twdrv_major, twdrv_minor);
	cdev_init(&devices->cdev, fops);
	devices->cdev.owner = THIS_MODULE;
	devices->cdev.ops = fops;
	result = cdev_add (&devices->cdev, devno, 1);
	if (result) {
        printk(KERN_WARNING "[twdrv_data] Setup_cdev: Error %d adding cdev", result);
        return;
    }

    printk(KERN_INFO "[twdrv_data] Setup_cdev successful\n");
}

int twdrv_data_init(struct twdrv_dev **devices, struct file_operations *fops)
{
    int result = 0;
 
    result = twdrv_data_alloc(devices);
    if (result) {
        return result;
    }

    twdrv_data_setup_cdev(*devices, fops);
    return result;

}

void twdrv_data_release(struct twdrv_dev *devices)
{
    twdrv_data_free(devices);
}