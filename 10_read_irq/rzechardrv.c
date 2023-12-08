/**
 * @file   rzechardrv.c
 * @brief   An introductory character driver to support reading and writing to chunk of allocated memory
 */

#include <linux/init.h>          // Macros used to mark up functions e.g. __init __exit
#include <linux/kernel.h>        // printk
#include <linux/module.h>
#include <linux/kdev_t.h>        // MAJOR, MINOR, MKDEV..
#include <linux/fs.h>            // register_chrdev_region
#include <linux/device.h>        // class_create
#include <linux/cdev.h>
#include <linux/slab.h> 	//kmalloc
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/hw_irq.h>
#include <linux/ioport.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rafal Zielinski");
MODULE_DESCRIPTION("introductory irq driver");
MODULE_VERSION("0.1");

struct char_dev {
	struct cdev cdev;
};

#define CLASS_NAME  "rz_class"
#define DEVICE_NAME "rzechar"
#define IRQ_NO 11



struct tasklet_struct *tasklet = NULL;
 
void tasklet_callback(struct tasklet_struct *t){
        printk(KERN_INFO "Executing Tasklet Function \n");
}

static struct class*  rzecharClass  = NULL;
static struct device* rzecharDevice = NULL;
static struct char_dev* my_devs = NULL;

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static int rzechar_major = 0;
static int rzechar_nr_devs = 1;

//Interrupt handler for IRQ 11. 
static irqreturn_t irq_handler(int irq,void *dev_id) {
	printk(KERN_INFO "Shared IRQ: Interrupt Occurred");
	tasklet_schedule(tasklet); 
        return IRQ_HANDLED;
}



static struct file_operations fops =
{
        .owner = THIS_MODULE,
        .open = dev_open,
        .read = dev_read,
        .write = dev_write,
        .release = dev_release,
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

                cdev_init(&my_devs[i].cdev, &fops);
                my_devs[i].cdev.owner = THIS_MODULE;
                my_devs[i].cdev.ops = &fops;

                result = cdev_add(&my_devs[i].cdev, devt, rzechar_nr_devs);
                if (result)
                        printk(KERN_NOTICE "rzechar: Error %d adding hello", result);
        }
        
        if (request_irq(IRQ_NO, irq_handler, IRQF_SHARED, DEVICE_NAME, (void *)(irq_handler))) {
        		result = -ENOMEM;
            		printk(KERN_WARNING "ERROR: cannot register IRQ ");
                    	free_irq(IRQ_NO,(void *)(irq_handler));
                    	device_destroy(rzecharClass, MKDEV(rzechar_major, 0));
			class_destroy(rzecharClass);
			unregister_chrdev(rzechar_major, DEVICE_NAME);
			return result;
        }
        
        tasklet  = kmalloc(sizeof(struct tasklet_struct),GFP_KERNEL);
        if(tasklet == NULL) {
            		printk(KERN_INFO "etx_device: cannot allocate tasklet Memory");
            		result = -ENOMEM;
                    	free_irq(IRQ_NO,(void *)(irq_handler));
                    	device_destroy(rzecharClass, MKDEV(rzechar_major, 0));
			class_destroy(rzecharClass);
			unregister_chrdev(rzechar_major, DEVICE_NAME);
			return result;
        }
        tasklet_setup(tasklet, tasklet_callback);
    
        return result;
}

static void __exit rzechardrv_exit(void){

        size_t i = 0;
        tasklet_kill(tasklet);
        if(tasklet != NULL){
          kfree(tasklet);
        }
        free_irq(IRQ_NO, (void *)(irq_handler));
        for(i = 0; i < rzechar_nr_devs; i++){  
                cdev_del(&my_devs[i].cdev);
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
	struct irq_desc *desc;
	desc = irq_to_desc(11);
        
        if (!desc) return -EINVAL;
        
        printk(KERN_INFO "rzechar: firing irq\n");
        __this_cpu_write(vector_irq[59], desc);
        asm("int $0x3B");  // Corresponding to irq 11
        
        return 0;
}

ssize_t dev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
        printk(KERN_INFO "rzechar: dev_write count: %zu f_pos: %lld \n", count, *f_pos);

        return 0;
}

module_init(rzechardrv_init);
module_exit(rzechardrv_exit);
