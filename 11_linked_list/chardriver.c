#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A B");
MODULE_DESCRIPTION("Char driver");
MODULE_VERSION("1.0");

static int dev_major;
static int dev_minor = 0;
static int open_devices = 0;

LIST_HEAD(chardev_list);

struct chardev_node
{
    struct list_head list;
    int data;
};

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

static ssize_t dev_read(struct file* file, char* __user buffer, size_t count, loff_t* offset)
{
    printk(KERN_INFO "CharDriver: Read\n");

    struct chardev_node* node;
    int index = 0;

    list_for_each_entry(node, &chardev_list, list)
    {
        printk(KERN_INFO "Node %d data = %d\n", index++, node->data);
    }

    printk(KERN_INFO "CharDriver: Read %zu characters", index);
    return 0;
}

static ssize_t dev_write(struct file* file, const char* __user buffer, size_t count, loff_t* offset)
{
    printk(KERN_INFO "CharDriver: Write\n");

    struct chardev_node* node = kmalloc(sizeof(struct chardev_node), GFP_KERNEL);
    if (!node)
    {
        printk(KERN_INFO "CharDriver: Error during node alloaction\n");
        return -ENOMEM;
    }

    node->data = count;
    INIT_LIST_HEAD(&node->list);
    list_add_tail(&node->list, &chardev_list);

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

    struct chardev_node* node;
    struct chardev_node* temp;

    list_for_each_entry_safe(node, temp, &chardev_list, list)
    {
        list_del(&node->list);
        kfree(node);
    }

    dev_t dev = MKDEV(dev_major, dev_minor);
    clear_devices_data();
    class_destroy(dev_class);
    unregister_chrdev_region(dev, DEV_NUMBER);
}

module_init(dev_init);
module_exit(dev_exit);
