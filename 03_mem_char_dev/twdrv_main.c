#include "twdrv_data.h"
#include "twdrv_fops.h"

 
int twdrv_major = 0;
int twdrv_minor = 0;
unsigned int twdrv_nr_devs   = 1;
unsigned int twdrv_max_dev_size = 20;

module_param(twdrv_major, int, S_IRUGO);
module_param(twdrv_minor, int, S_IRUGO);
module_param(twdrv_nr_devs, int, S_IRUGO);
module_param(twdrv_max_dev_size, int, S_IRUGO);

struct twdrv_dev *twdrv_devices;
struct file_operations twdrv_fops;


static void twdrv_main_exit(void)
{
    dev_t devno = 0;

	printk(KERN_INFO "[twdrv_main] Exit \n");
    
	devno = MKDEV(twdrv_major, twdrv_minor);
    cdev_del(&(twdrv_devices->cdev));

    twdrv_data_release(twdrv_devices);

	unregister_chrdev_region(devno, twdrv_nr_devs);
	printk(KERN_INFO "[twdrv_main] Exit: cdev deleted, kfree, chdev unregistered\n");
    printk(KERN_INFO "[twdrv_main] - EXITED ----------------------------------- \n");
}


static int twdrv_main_init(void)
{
	  int result =0;
	  dev_t dev = 0;

    printk(KERN_INFO "[twdrv_main] - INIT ------------------------------------- \n");

	if (twdrv_major) {
		printk(KERN_INFO "[twdrv_main] Init: static allocation of major number (%d)\n",twdrv_major);
		dev = MKDEV(twdrv_major, twdrv_minor);
		result = register_chrdev_region(dev, twdrv_nr_devs, "twdrv");
	} else {
		result = alloc_chrdev_region(&dev, twdrv_minor, twdrv_nr_devs, "twdrv");
		twdrv_major = MAJOR(dev);
        printk(KERN_INFO "[twdrv_main] Init: dynamic allocation of major number (%d)\n", twdrv_major);
	}
	if (result < 0) {
		printk(KERN_WARNING "[twdrv_main] Init: can't get major %d\n", twdrv_major);
		return result;
    }

    twdrv_fops_init(&twdrv_fops);

    result = twdrv_data_init(&twdrv_devices, &twdrv_fops);
    if (result) {
        printk(KERN_WARNING "[twdrv_main] Init: Failed to init driver data \n");
        goto fail;
    }

    return 0;

    fail:
        twdrv_main_exit();
        return result;
}



module_init(twdrv_main_init);
module_exit(twdrv_main_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("twakula");
MODULE_DESCRIPTION("Driver development training");
MODULE_VERSION("1.0");
