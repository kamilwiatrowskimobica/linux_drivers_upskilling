#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "chardriver.h"
#include "chardriver_ioctl.h"

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
static struct kmem_cache* chardev_cache;

DEFINE_SPINLOCK(dev_spinlock);

struct device_data
{
    struct cdev cdev;
    size_t size;
    char* buffer;
    struct mutex rw_mutex;
    wait_queue_head_t pollq;
};

static struct device_data* devices = NULL;
static struct class* dev_class = NULL;

static ssize_t dev_read(struct file* file, char* __user buffer, size_t count, loff_t* offset);
static ssize_t dev_write(struct file* file, const char* __user buffer, size_t count, loff_t* offset);
static int dev_open(struct inode* inode, struct file* file);
static int dev_release(struct inode* inode, struct file* file);
static void* dev_seq_start(struct seq_file* seq_file, loff_t* pos);
static void* dev_seq_next(struct seq_file* seq_file, void* v, loff_t* pos);
static void dev_seq_stop(struct seq_file* seq_file, void* v);
static int dev_seq_show(struct seq_file* seq_file, void* v);
static int dev_proc_open(struct inode* inode, struct file* file);
static loff_t dev_llseek(struct file* file, loff_t offset, int whence);
static long dev_ioctl(struct file* file, unsigned int cmd, unsigned long arg);
static unsigned int dev_poll(struct file* file, poll_table* wait);

static void clear_devices_data(void);

static struct file_operations fops = 
{
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .llseek = dev_llseek,
    .unlocked_ioctl = dev_ioctl,
    .poll = dev_poll,    
    .release = dev_release
};

static struct seq_operations sops = 
{
    .start = dev_seq_start,
    .next = dev_seq_next,
    .stop = dev_seq_stop,
    .show = dev_seq_show
};

static const struct proc_ops pops = 
{
    .proc_open = dev_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release
};

static ssize_t dev_read(struct file* file, char* __user buffer, size_t count, loff_t* offset)
{
    struct device_data* device_data = (struct device_data*) file->private_data;
    ssize_t len = min(device_data->size - *offset, count);

    mutex_lock_interruptible(&device_data->rw_mutex);

    printk(KERN_DEBUG "CharDriver: Read\n");
    printk(KERN_DEBUG "CharDriver: Device buffer size: %li\n", device_data->size);
    printk(KERN_DEBUG "CharDriver: Offset size: %lli\n", *offset);
    printk(KERN_DEBUG "CharDriver: User space buffer size %zu\n", count);

    if (len <= 0)
    {
        printk(KERN_DEBUG "CharDriver: Invalid message length: %li\n", len);
        mutex_unlock(&device_data->rw_mutex);
        return -EFAULT;
    }

    if (copy_to_user(buffer, device_data->buffer + *offset, len))
    {
        printk(KERN_DEBUG "CharDriver: Invalid message length: %li\n", len);
        mutex_unlock(&device_data->rw_mutex);
        return -EFAULT;
    }

    *offset += len;
    printk(KERN_INFO "CharDriver: Read %zu bytes to user space\n", len);
    mutex_unlock(&device_data->rw_mutex);

    return len;
}

static ssize_t dev_write(struct file* file, const char* __user buffer, size_t count, loff_t* offset)
{
    struct device_data* device_data = (struct device_data*) file->private_data;
    ssize_t len = min(device_data->size - *offset, count); // CHECK

    mutex_lock_interruptible(&device_data->rw_mutex);

    printk(KERN_DEBUG "CharDriver: Write\n");
    printk(KERN_DEBUG "CharDriver: Device buffer size: %li\n", device_data->size);
    printk(KERN_DEBUG "CharDriver: Offset size: %lli\n", *offset);
    printk(KERN_DEBUG "CharDriver: User space buffer size %zu\n", count);

    if (len <= 0)
    {
        printk(KERN_DEBUG "CharDriver: Invalid message length: %li\n", len);
        mutex_unlock(&device_data->rw_mutex);
        return -EFAULT;
    }

    if (device_data->size + len > MAX_DEV_BUFFER_SIZE)
    {
        printk(KERN_DEBUG "CharDriver: Data to be written to long, buffer size: %li, data length: %li, max buffer size: %i\n",
                          device_data->size, len, MAX_DEV_BUFFER_SIZE);
        mutex_unlock(&device_data->rw_mutex);
        return -EFBIG; //CHECK
    }

    if (copy_from_user(device_data->buffer + *offset, buffer, len))
    {
        printk(KERN_DEBUG "CharDriver: Invalid message length: %li\n", len);
        mutex_unlock(&device_data->rw_mutex);
        return -EFAULT;
    }

    *offset += len;
    printk(KERN_INFO "CharDriver: Wrote %zu bytes from user space\n", len);
    mutex_unlock(&device_data->rw_mutex);

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

static void* dev_seq_start(struct seq_file* seq_file, loff_t* pos)
{
    seq_printf(seq_file, "CharDriver: 'device' : buffer'"); // ???

    if (*pos >= DEV_NUMBER)
        return NULL;
    
    return devices + *pos;
}

static void* dev_seq_next(struct seq_file* seq_file, void* v, loff_t* pos)
{
    (*pos)++;

    if (*pos >= DEV_NUMBER)
        return NULL;
    
    return devices + *pos;
}

static void dev_seq_stop(struct seq_file* seq_file, void* v)
{
    // EMPTY
}

static int dev_seq_show(struct seq_file* seq_file, void* v)
{
    struct device_data* iter = (struct device_data*) v;
    int device_number = (int) (devices - iter);
    seq_printf(seq_file, "chardev%i: %s", device_number, iter->buffer);

    return 0;
}

static int dev_proc_open(struct inode* inode, struct file* file)
{
    return seq_open(file, &sops);
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

static void dev_proc_exit(void)
{
    remove_proc_entry("chardriver_seq", NULL);
}

static int dev_proc_init(void)
{
    if (!proc_create("chardriver_seq", 0, NULL, &pops))
        return -ENOMEM;

    return 0;    
}

static long dev_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
    void __user* data = (void __user*) arg;

    if (_IOC_TYPE(cmd) != DEV_IOC_MAGIC)
        return -ENOTTY;

    if (_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE))
        if (!access_ok(data, _IOC_SIZE(cmd)))
            return -EFAULT;

    struct device_data* device_data = (struct device_data*) file->private_data;
    if (!device_data)
    {
        printk(KERN_ERR "CharDriver: No data available\n");
        return -EFAULT;
    }

    struct ioctl_data user_device_data;

    if (copy_from_user(&user_device_data, data, _IOC_SIZE(cmd)))
    {
        printk(KERN_ERR "CharDriver: No data available\n");
        return -EFAULT;        
    }

    switch (cmd)
    {
        case DEV_IOC_WRITE:
            printk(KERN_INFO "CharDriver: Ioctl write\n");
            if (copy_from_user(device_data->buffer, user_device_data.buffer, user_device_data.size))
            {
                printk(KERN_ERR "CharDriver: Ioctl write error\n");
                return -EFAULT;
            }
            device_data->size = user_device_data.size; // CHECK
            break;
        case DEV_IOC_READ:
            printk(KERN_INFO "CharDriver: Ioctl read\n");
            if (copy_from_user(user_device_data.buffer, device_data->buffer, device_data->size))
            {
                printk(KERN_ERR "CharDriver: Ioctl read error\n");
                return -EFAULT;
            }
            user_device_data.size = device_data->size; // CHECK
            break;
        case DEV_IOC_READ_NTH_BYTE:
            break;
        case DEV_IOC_RESET:
            printk(KERN_INFO "CharDriver: Ioctl reset\n");
            memset(device_data->buffer, 0, device_data->size);
            device_data->size = 0;
            break;
        default:
            break;        
    }

    return 0;
}

static unsigned int dev_poll(struct file* file, poll_table* wait)
{
    struct device_data* device_data = (struct device_data*) file->private_data;
    if (!device_data)
    {
        printk(KERN_ERR "CharDriver: No data available\n");
        return -EFAULT;
    }

    __poll_t mask = 0;

    mutex_lock(&device_data->rw_mutex);
    poll_wait(file, &device_data->pollq, wait);

    if (device_data->size > 0 && file->f_pos < MAX_DEV_BUFFER_SIZE)
    {
        printk(KERN_DEBUG "CharDriver: Read allowed\n");
        mask |= POLLIN | POLLRDNORM;
    }

    if (device_data->size < MAX_DEV_BUFFER_SIZE)
    {
        printk(KERN_DEBUG "CharDriver: Write allowed\n");
        mask |= POLLOUT | POLLWRNORM;
    }
    
    mutex_unlock(&device_data->rw_mutex);

    return mask;
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
    chardev_cache = kmem_cache_create_usercopy("chardev", MAX_DEV_BUFFER_SIZE, 0, SLAB_HWCACHE_ALIGN, 0, MAX_DEV_BUFFER_SIZE, NULL);
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

        mutex_init(&devices[i].rw_mutex);
        cdev_init(&dev_data->cdev, &fops);
        init_waitqueue_head(&devices[i].pollq);
        dev_data->buffer = kmem_cache_alloc(chardev_cache, GFP_KERNEL);
        // dev_data->buffer = (char*) __get_free_pages(GFP_KERNEL, 1);

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

    result = dev_proc_init(); // Init sequentional read

    return result;
}

static void clear_devices_data(void)
{
    if (devices)
    {
        int i;
        for (i = 0; i != DEV_NUMBER; i++)
        {
            // free_pages((unsigned long) devices[i].buffer, 1);
            kmem_cache_free(chardev_cache, devices[i].buffer);
            device_destroy(dev_class, (&devices[i])->cdev.dev);
            cdev_del(&devices[i].cdev);
            mutex_destroy(&devices[i].rw_mutex);
        }

        kfree(devices);
    }
}

static void dev_exit(void)
{
    printk(KERN_INFO "CharDriver: Exit\n");
    dev_t dev = MKDEV(dev_major, dev_minor);
    dev_proc_exit();
    clear_devices_data();
    class_destroy(dev_class);

    if (chardev_cache)
        kmem_cache_destroy(chardev_cache);

    unregister_chrdev_region(dev, DEV_NUMBER);
}

module_init(dev_init);
module_exit(dev_exit);
