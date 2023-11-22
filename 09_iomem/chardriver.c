#include <asm/io.h>
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
#define IO_MEM_SIZE 8

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A B");
MODULE_DESCRIPTION("Char driver");
MODULE_VERSION("1.0");

static int dev_major;
static int dev_minor = 0;
static int open_devices = 0;

static void __iomem* io_mem;
static unsigned long base_mem = 0x880000000;

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
    .llseek = no_llseek,
    .release = dev_release
};

static ssize_t dev_read(struct file* file, char* __user buffer, size_t count, loff_t* offset)
{
    uint8_t driver_buffer[IO_MEM_SIZE];
    uint8_t* db_ptr = driver_buffer;
    void __iomem* io_mem_ptr = io_mem;
    size_t size;

    if (count > IO_MEM_SIZE)
        count = IO_MEM_SIZE;

    size = count;
    while (size)
    {
        *db_ptr = ioread8(io_mem_ptr);
        rmb();
        io_mem_ptr++;
        size--;
        db_ptr++;
    }

    if (copy_to_user(buffer, driver_buffer, count))
    {
        printk(KERN_ERR "CharDriver: Read failed while copying to user\n");
        return -EFAULT;
    }

    *offset += count;
    
    printk(KERN_INFO "CharDriver: Read %zu characters", count);
    return count;
}

static ssize_t dev_write(struct file* file, const char* __user buffer, size_t count, loff_t* offset)
{
    uint8_t driver_buffer[IO_MEM_SIZE];
    uint8_t* db_ptr = driver_buffer;
    void __iomem* io_mem_ptr = io_mem;
    size_t size;

//    if (!capable(CAP_SYS_RAWIO))  // CHECK
//        return -EPERM;

    if (*offset > 0)
        return -EINVAL;

    if (count > IO_MEM_SIZE)
        return -EFBIG;

    if (copy_from_user(driver_buffer, buffer, count))
    {
        printk(KERN_ERR "CharDriver: Write failed while copying from user\n");
        return -EFAULT;
    }

    size = count;
    while (size)
    {
        iowrite8(*db_ptr++, io_mem_ptr);
        wmb();
        io_mem_ptr++;
        size--;
    }

    *offset += count;
    
    printk(KERN_INFO "CharDriver: Wrote %zu characters", count);

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

    if (!request_mem_region(base_mem, IO_MEM_SIZE, "chardev"))
    {
        printk(KERN_ERR "CharDriver: Claim of I/O region failed 0x%lx\n", base_mem);
        unregister_chrdev_region(0, DEV_NUMBER);
        return -ENODEV;
    }

    io_mem = ioremap(base_mem, IO_MEM_SIZE);

    if (!io_mem)
    {
        printk(KERN_ERR "CharDriver: Memory mapping error 0x%lx\n", base_mem);
        iounmap(io_mem);
        release_mem_region(base_mem, IO_MEM_SIZE);
        unregister_chrdev_region(0, DEV_NUMBER);
        return -ENODEV;        
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
    dev_t dev = MKDEV(dev_major, dev_minor);
    clear_devices_data();
    class_destroy(dev_class);
    unregister_chrdev_region(dev, DEV_NUMBER);
}

module_init(dev_init);
module_exit(dev_exit);
