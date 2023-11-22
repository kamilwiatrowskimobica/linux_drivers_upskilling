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

#define DEV_NUMBER 2
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

struct device_data
{
    struct cdev cdev;
    size_t size;
    char* buffer;
};

static struct device_data* devices = NULL;
static struct class* dev_class = NULL;

static ssize_t dev_read(struct file* file, char* __user buffer, size_t count, loff_t* offset);
static ssize_t dev_write(struct file* file, const char* __user buffer, size_t count, loff_t* offset);
static int dev_open(struct inode* inode, struct file* file);
static int dev_release(struct inode* inode, struct file* file);
static loff_t dev_llseek(struct file* file, loff_t offset, int whence);

static void clear_devices_data(void);

static struct file_operations fops = 
{
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .llseek = dev_llseek,
    .release = dev_release
};

static ssize_t dev_read(struct file* file, char* __user buffer, size_t count, loff_t* offset)
{
    struct device_data* device_data = (struct device_data*) file->private_data;
    ssize_t len = min(device_data->size - *offset, count);

    printk(KERN_DEBUG "CharDriver: Read\n");
    printk(KERN_DEBUG "CharDriver: Device buffer size: %li\n", device_data->size);
    printk(KERN_DEBUG "CharDriver: Offset size: %lli\n", *offset);
    printk(KERN_DEBUG "CharDriver: User space buffer size %zu\n", count);

    if (len <= 0)
    {
        printk(KERN_DEBUG "CharDriver: Invalid message length: %li\n", len);
        return -EFAULT;
    }

    if (copy_to_user(buffer, device_data->buffer + *offset, len))
    {
        printk(KERN_DEBUG "CharDriver: Invalid message length: %li\n", len);
        return -EFAULT;
    }

    *offset += len;
    printk(KERN_INFO "CharDriver: Read %zu bytes to user space\n", len);

    return len;
}

static ssize_t dev_write(struct file* file, const char* __user buffer, size_t count, loff_t* offset)
{
    struct device_data* device_data = (struct device_data*) file->private_data;
    ssize_t len = min(device_data->size - *offset, count); // CHECK

    printk(KERN_DEBUG "CharDriver: Write\n");
    printk(KERN_DEBUG "CharDriver: Device buffer size: %li\n", device_data->size);
    printk(KERN_DEBUG "CharDriver: Offset size: %lli\n", *offset);
    printk(KERN_DEBUG "CharDriver: User space buffer size %zu\n", count);

    if (len <= 0)
    {
        printk(KERN_DEBUG "CharDriver: Invalid message length: %li\n", len);
        return -EFAULT;
    }

    if (device_data->size + len > MAX_DEV_BUFFER_SIZE)
    {
        printk(KERN_DEBUG "CharDriver: Data to be written to long, buffer size: %li, data length: %li, max buffer size: %i\n",
                          device_data->size, len, MAX_DEV_BUFFER_SIZE);
        return -EFBIG; //CHECK
    }

    if (copy_from_user(device_data->buffer + *offset, buffer, len))
    {
        printk(KERN_DEBUG "CharDriver: Invalid message length: %li\n", len);
        return -EFAULT;
    }

    *offset += len;
    printk(KERN_INFO "CharDriver: Wrote %zu bytes from user space\n", len);

    return len;
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

static loff_t dev_llseek(struct file* file, loff_t offset, int whence)
{
    struct device_data* device_data = (struct device_data*) file->private_data;
    loff_t new_pos;

    switch (whence)
    {
        case SEEK_SET:
            printk(KERN_DEBUG "CharDriver: Seek set\n");
            new_pos = offset;
            break;
        case SEEK_CUR:
            printk(KERN_DEBUG "CharDriver: Seek set\n");
            new_pos = file->f_pos + offset;        
            break;
        case SEEK_END:
            printk(KERN_DEBUG "CharDriver: Seek set\n");
            new_pos = device_data->size + offset;
            break;
        default:
            return -EINVAL;    
    }

    if (new_pos < 0 || new_pos >= MAX_DEV_BUFFER_SIZE)
    {
        printk(KERN_ERR "CharDriver: Seek error\n");
        return -EINVAL;
    }

    file->f_pos = new_pos;

    return new_pos;
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
        dev_data->buffer = (char*) kmalloc(sizeof(char) * MAX_DEV_BUFFER_SIZE, GFP_KERNEL);

        if (!dev_data->buffer)
        {
            printk(KERN_ERR "CharDriver: Memory allocation error\n");
            class_destroy(dev_class);
            unregister_chrdev_region(0, DEV_NUMBER);
            clear_devices_data();
            return -ENOMEM;
        }

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
            kfree(devices[i].buffer);
            device_destroy(dev_class, (&devices[i])->cdev.dev);
            cdev_del(&devices[i].cdev);
        }

        kfree(devices);
    }
}

static void dev_exit(void)
{
    printk(KERN_INFO "CharDriver: Exit\n");
    dev_t dev = MKDEV(dev_major, dev_minor);
    clear_devices_data();
    class_destroy(dev_class);
    unregister_chrdev_region(dev, DEV_NUMBER);
}

module_init(dev_init);
module_exit(dev_exit);
