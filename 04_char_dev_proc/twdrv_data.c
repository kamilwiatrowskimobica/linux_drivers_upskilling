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

    for (int i = 0; i < (twdrv_nr_devs - twdrv_minor); i ++) {
        struct twdrv_dev *twdrv_device = &(*devices)[i];
        printk(KERN_WARNING "[twdrv_data] kmalloc p_data for 0x%lx\n", (unsigned long)twdrv_device);
        twdrv_device->p_data = (char*)kmalloc(twdrv_max_dev_size * sizeof(char), GFP_KERNEL);
    }
    
    if (!(*devices)->p_data) {
        kfree(*devices);
        printk(KERN_WARNING "[twdrv_data] ERROR kmalloc p_data\n");
        return -ENOMEM;
    }

    return 0;
}

void twdrv_data_free(struct twdrv_dev * devices)
{
    struct twdrv_dev *twdrv_device;

    for (int i = twdrv_minor; i < twdrv_nr_devs; i++) {
        twdrv_device = &devices[i];
        if (twdrv_device->p_data) {
            printk(KERN_INFO "[twdrv_data] kfree p_data for dev: 0x%lx\n", (unsigned long)twdrv_device);
            kfree(twdrv_device->p_data);
        }
    }

    if((devices) != 0){
        kfree(devices);
        printk(KERN_INFO "[twdrv_data] kfree twdrv_devices\n");
    }
}

void twdrv_data_setup_cdev(struct twdrv_dev *devices, struct file_operations *fops)
{
	int result = 0; 
    int devno = 0;
    struct twdrv_dev *twdrv_device;
        
    for (int i = twdrv_minor; i < twdrv_nr_devs; i++) {
        twdrv_device = &devices[i];
        twdrv_device->id = i;
        devno = MKDEV(twdrv_major, i);
        cdev_init(&twdrv_device->cdev, fops);
        twdrv_device->cdev.owner = THIS_MODULE;
        twdrv_device->cdev.ops = fops;
        printk(KERN_WARNING "[twdrv_data] Setup_cdev: init device %d, 0x%lx", i, (unsigned long)twdrv_device);
        result = cdev_add (&twdrv_device->cdev, devno, 1);
        if (result) {
            printk(KERN_WARNING "[twdrv_data] Setup_cdev: Error %d adding cdev", result);
            return;
        }
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