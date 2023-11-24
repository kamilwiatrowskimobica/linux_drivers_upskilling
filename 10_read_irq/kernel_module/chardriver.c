#include <asm/hw_irq.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "chardriver.h"

#define DEV_NUMBER 1
#define MAX_DEV_BUFFER_SIZE 255
#define DEVICE_NAME "chardevice"
#define CLASS_NAME "charclass"

#define IRQ_NO 11
#define IRQ_VEC_NO (FIRST_EXTERNAL_VECTOR + 0x010)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A B");
MODULE_DESCRIPTION("Char driver");
MODULE_VERSION("1.0");

static int dev_major;
static int dev_minor = 0;
static int open_devices = 0;

static void tasklet_job(struct tasklet_struct* s)
{
    printk(KERN_INFO "CharDriver: Tasklet\n");
}

DECLARE_TASKLET(tasklet, tasklet_job);

struct device_data
{
    struct cdev cdev;
};

static struct device_data* devices = NULL;
static struct class* dev_class = NULL;

static ssize_t dev_read(struct file* file, char* __user buffer, size_t count, loff_t* offset);
static ssize_t dev_write(struct file* file, const char* __user buffer, size_t count, loff_t* offset);
static int dev_open(struct inode* inode, struct file* file);
static int dev_release(struct inode* inode, struct file* file);

static void clear_devices_data(void);

static struct file_operations fops = 
{
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release
};

static irqreturn_t irq_handler(int irq, void* dev_id)
{
    printk(KERN_INFO "CharDriver: Interrupt incoming!");
    tasklet_schedule(&tasklet);
    return IRQ_HANDLED;
}

static ssize_t dev_read(struct file* file, char* __user buffer, size_t count, loff_t* offset)
{
    struct irq_desc* desc;
    printk(KERN_INFO "CharDriver: Read\n");
    //desc = irq_to_desc(IRQ_NO);
    desc = irq_data_to_desc(irq_get_irq_data(IRQ_NO));
    if (!desc)
        return -EINVAL;

    __this_cpu_write(vector_irq[IRQ_VEC_NO], desc);
    asm("int $0x3B");

    printk(KERN_INFO "CharDriver: Read %zu characters", count);
    return count;
}

static ssize_t dev_write(struct file* file, const char* __user buffer, size_t count, loff_t* offset)
{
    printk(KERN_INFO "CharDriver: Write\n");
    return count;
}

static int dev_open(struct inode* inode, struct file* file)
{
    struct device_data* device_data = container_of(inode->i_cdev, struct device_data, cdev);
    file->private_data = device_data;

    printk(KERN_INFO "CharDriver: Opening device, number of open devices: %i\n", open_devices);

    return 0;
}

static int dev_release(struct inode* inode, struct file* file)
{
    printk(KERN_INFO "CharDriver: Releasing device, number of open devices: %i\n", open_devices);

    return 0;
}

static int dev_init(void)
{
    int i, result;
    dev_t dev = MKDEV(dev_major, dev_minor);

    if (dev_major)
    {
        printk(KERN_DEBUG "CharDriver: Static device registration\n");
        result = register_chrdev_region(MKDEV(dev_major, dev_minor), 0, DEVICE_NAME);
    }
    else
    {
        printk(KERN_DEBUG "CharDriver: Dynamic device registration\n");
        result = alloc_chrdev_region(&dev, dev_minor, DEV_NUMBER, DEVICE_NAME);

        dev_major = MAJOR(dev);
        printk(KERN_DEBUG "CharDriver: Major: %i, minor %i\n", dev_major, dev_minor);
    }

    if (result)
    {
        printk(KERN_ERR "CharDriver: Registration error\n");
        return result;
    }

    dev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (!dev_class)
    {
        printk(KERN_DEBUG "CharDriver: Class creation error\n");
        unregister_chrdev_region(0, DEV_NUMBER);
        return PTR_ERR(dev_class);
    }

    devices = kmalloc(DEV_NUMBER * sizeof(struct device_data), GFP_KERNEL);
    if (!devices)
    {
        printk(KERN_ERR "CharDriver: Memory allocation error\n");
        class_destroy(dev_class);
        unregister_chrdev_region(0, DEV_NUMBER);
        return PTR_ERR(devices);
    }

    for (i = 0; i != DEV_NUMBER; i++)
    {
        dev_t dev = MKDEV(dev_major, dev_minor + i);
        struct device* chardev_device = device_create(dev_class, NULL, dev, NULL, "%s%i", DEVICE_NAME, i);
        struct device_data* dev_data = &devices[i];

        if (!chardev_device)
        {
            printk(KERN_ERR "CharDriver: Device creation error\n");
            class_destroy(dev_class);
            unregister_chrdev_region(0, DEV_NUMBER);
            clear_devices_data();
            return -ENOMEM;
        }

        cdev_init(&dev_data->cdev, &fops);

        if (cdev_add(&dev_data->cdev, MKDEV(dev_major, dev_minor + i), 1))
        {
            printk(KERN_ERR "CharDriver: Memory allocation error\n");
            class_destroy(dev_class);
            unregister_chrdev_region(0, DEV_NUMBER);
            clear_devices_data();
            return -ENOMEM;            
        }
    }

    if (request_irq(IRQ_NO, irq_handler, IRQF_SHARED, DEVICE_NAME, (void*) (irq_handler)))
    {
        printk(KERN_ERR "CharDriver: Irq handler request error\n");
        class_destroy(dev_class);
        unregister_chrdev_region(0, DEV_NUMBER);
        clear_devices_data();
        free_irq(IRQ_NO, (void*) (irq_handler));
        return -ENOMEM;
    }

    return result;
}

static void clear_devices_data(void)
{
    if (devices)
    {
        int i;
        for (i = 0; i != DEV_NUMBER; i++)
        {
            device_destroy(dev_class, (&devices[i])->cdev.dev);
            cdev_del(&devices[i].cdev);
        }

        kfree(devices);
    }
}

static void dev_exit(void)
{
    printk(KERN_INFO "CharDriver: Exit\n");
    tasklet_kill(&tasklet);
    free_irq(IRQ_NO, (void*) (irq_handler));
    dev_t dev = MKDEV(dev_major, dev_minor);
    clear_devices_data();
    class_destroy(dev_class);
    unregister_chrdev_region(dev, DEV_NUMBER);
}

module_init(dev_init);
module_exit(dev_exit);
