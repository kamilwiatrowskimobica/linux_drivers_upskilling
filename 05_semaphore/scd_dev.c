/**
 * @file   my_char_driver.c
 * @author Przemek Stekiel
 * @date   21 Jule 2023
 * @version 1.0
 * @brief   Character driver with proc for chapter 5 of Linux Device Driver book (sync with mutex)
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define CLASS_NAME  "scd" // simple char driver
#define DEVICE_NAME "scd_dev"
#define SCD_DEV_COUNT 3
#define SCD_DEV_LEN 256

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Przemek Stekiel");
MODULE_DESCRIPTION("Character driver with proc for chapter 5 of Linux Device Driver book (sync with mutex)");
MODULE_VERSION("1.0");

typedef struct scd_dev
{
    struct cdev cdev;
    unsigned char id;
    char *buffer;
    size_t buffer_size;
    size_t pos;
    struct mutex sem;
} scd_dev_t;

static int majorNumber;
static scd_dev_t* scd_devs[SCD_DEV_COUNT] = {0};
static struct class*  scd_dev_class  = NULL;
static dev_t dev = 0;
struct device *p_dev = NULL;

// The prototype functions for the character driver -- must come before the struct definition
static int scd_dev_open(struct inode *, struct file *);
static int scd_dev_release(struct inode *, struct file *);
static ssize_t scd_dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t scd_dev_write(struct file *, const char *, size_t, loff_t *);
static loff_t scd_dev_llseek(struct file *filep, loff_t offset, int whence);

// The prototype functions for sequence operations -- must come before the struct definition
static int scd_dev_seq_open(struct inode *, struct file *);
static void *scd_dev_seq_start(struct seq_file *, loff_t *);
static void *scd_dev_seq_next(struct seq_file *, void *, loff_t *);
static void scd_dev_seq_stop(struct seq_file *, void *);
static int scd_dev_seq_show(struct seq_file *, void *);

static struct file_operations fops =
{
    .owner = THIS_MODULE,
    .open = scd_dev_open,
    .read = scd_dev_read,
    .write = scd_dev_write,
    .release = scd_dev_release,
    .llseek  = scd_dev_llseek,
};

static struct seq_operations seq_ops =
{
    .start = scd_dev_seq_start,
    .next = scd_dev_seq_next,
    .stop = scd_dev_seq_stop,
    .show = scd_dev_seq_show
};

static const struct proc_ops seq_fops =
{
    .proc_open = scd_dev_seq_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release,
};

// SCD DEV OPERATIONS

static int __init scd_dev_init(void)
{
    int ret = 0;
    struct proc_dir_entry *entry;

    printk(KERN_INFO "scd_dev: Initializing the scd_dev LKM. \n");

    ret = alloc_chrdev_region(&dev, 0, SCD_DEV_COUNT, DEVICE_NAME);
    if (ret != 0)
    {
        printk(KERN_ALERT "scd_dev: failed to allocate region. \n");
        return ret;
    }
    majorNumber = MAJOR(dev);

    printk(KERN_INFO "scd_dev: registered correctly with major number %d. \n", majorNumber);

    scd_dev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(scd_dev_class))
    {
        unregister_chrdev_region(dev, SCD_DEV_COUNT);
        printk(KERN_ALERT "Failed to register device class. \n");
        return PTR_ERR(scd_dev_class);
    }
    printk(KERN_INFO "scd_dev: device class registered correctly. \n");

    for (int i = 0; i < SCD_DEV_COUNT; i++)
    {
        scd_devs[i] = kcalloc(1, sizeof(scd_dev_t), GFP_KERNEL);

        if (scd_devs[i] == NULL)
        {
            printk(KERN_ALERT "scd_dev: Unable to allocate memory for device. \n");
            ret = -ENOMEM;
            goto cleanup;
        }

        scd_devs[i]->buffer = kcalloc(1, SCD_DEV_LEN, GFP_KERNEL);

        if (scd_devs[i]->buffer == NULL)
        {
            printk(KERN_ALERT "scd_dev: Unable to allocate memory for device buffer. \n");
            ret = -ENOMEM;
            goto cleanup;
        }

        scd_devs[i]->buffer_size = SCD_DEV_LEN;
        scd_devs[i]->pos = 0;
        scd_devs[i]->id = i;
        mutex_init(&scd_devs[i]->sem);

        cdev_init(&scd_devs[i]->cdev, &fops);
        scd_devs[i]->cdev.owner = THIS_MODULE;
        scd_devs[i]->cdev.ops = &fops;
        printk(KERN_INFO "scd_dev: scd_dev[%d] init done. \n", i);

        ret = cdev_add(&scd_devs[i]->cdev, MKDEV(majorNumber, i), 1);
        if (ret != 0)
        {
            printk(KERN_ALERT "scd_dev: Failed to add cdev. \n");
            goto cleanup;
        }

        p_dev = device_create(scd_dev_class, NULL, MKDEV(majorNumber, i), NULL, "%s_%d", DEVICE_NAME, i);
        if (IS_ERR(p_dev))
        {
            printk(KERN_ALERT "scd_dev: Failed to create device\n");
            goto cleanup;
        }
        printk(KERN_INFO "scd_dev: scd_dev[%d] device created. \n", i);
    }

    // Create proces for device
    entry = proc_create(DEVICE_NAME, 0, NULL, &seq_fops);
    if (entry == NULL)
    {
        printk(KERN_ALERT "scd_dev: proc_create failure. \n");
        return -ENOMEM;
    }

    printk(KERN_INFO "scd_dev: Device setup done. \n");

    return 0;

cleanup:
    printk(KERN_ALERT "scd_dev: cleanup. \n");
    for (int i = 0; i < SCD_DEV_COUNT; i++)
    {
        kfree(scd_devs[i]->buffer);
        mutex_destroy(&scd_devs[i]->sem);
        printk(KERN_INFO "scd_dev: Device[%d] buffer released. \n", i);
        device_destroy(scd_dev_class, MKDEV(majorNumber, i));
        cdev_del(&scd_devs[i]->cdev);
        printk(KERN_INFO "scd_dev: Device[%d] cdev removed. \n", i);
        kfree(scd_devs[i]);
        printk(KERN_INFO "scd_dev: Device[%d] memory released. \n", i);
    }
    class_unregister(scd_dev_class);
    printk(KERN_INFO "scd_dev: Class unregistered. \n");
    unregister_chrdev_region(dev, SCD_DEV_COUNT);
    printk(KERN_INFO "scd_dev: region urregistered. \n");
    remove_proc_entry(DEVICE_NAME, NULL);
    printk(KERN_INFO "scd_dev: Goodbye!\n");

    return ret;
}

static void __exit scd_dev_exit(void)
{
    for (int i = 0; i < SCD_DEV_COUNT; i++)
    {
        kfree(scd_devs[i]->buffer);
        scd_devs[i]->buffer = NULL;
        printk(KERN_INFO "scd_dev: Device[%d] buffer released. \n", i);
        mutex_destroy(&scd_devs[i]->sem);
        printk(KERN_INFO "scd_dev: Device[%d] mutex destroyed. \n", i);
        device_destroy(scd_dev_class, MKDEV(majorNumber, i));
        cdev_del(&scd_devs[i]->cdev);
        printk(KERN_INFO "scd_dev: Device[%d] cdev removed. \n", i);
        kfree(scd_devs[i]);
        scd_devs[i] = NULL;
        printk(KERN_INFO "scd_dev: Device[%d] memory released. \n", i);
    }
    class_unregister(scd_dev_class);
    printk(KERN_INFO "scd_dev: Class unregistered. \n");
    unregister_chrdev_region(dev, SCD_DEV_COUNT);
    printk(KERN_INFO "scd_dev: region urregistered. \n");
    remove_proc_entry(DEVICE_NAME, NULL);
    printk(KERN_INFO "scd_dev: Goodbye!\n");
}

static int scd_dev_open(struct inode *inodep, struct file *filep)
{
    scd_dev_t *mdev = NULL;

    printk(KERN_INFO "scd_dev: Opening device... \n");
    mdev = container_of(inodep->i_cdev, scd_dev_t, cdev);
    if (!mdev)
    {
        printk(KERN_ALERT "scd_dev: Failed device data not found. \n");
        return -EFAULT;
    }
    filep->private_data = mdev;
    printk(KERN_INFO "scd_dev: Device successfully opened. \n");
    return 0;
}


static ssize_t scd_dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
    scd_dev_t *mdev = (scd_dev_t *)filep->private_data;

    printk(KERN_INFO "scd_dev_read (pos: %zu, offset: %llu, len: %zu.) \n", mdev->pos, *offset, len);

    if (!mdev)
    {
        printk(KERN_ALERT "scd_dev: No device data. \n");
        return -EFAULT;
    }

    if (mutex_lock_interruptible(&mdev->sem))
        return -ERESTARTSYS;

    if (mdev->pos > mdev->buffer_size)
    {
        mutex_unlock(&mdev->sem);
        return 0;
    }

    if (len > mdev->pos)
    {
        len = mdev->pos;
    }

    if (copy_to_user(buffer, mdev->buffer + mdev->pos - len, len))
    {
        printk(KERN_ALERT "scd_dev: Read %zu bytes failed. \n", len);
        mutex_unlock(&mdev->sem);
        return -EFAULT;
    }

    printk(KERN_INFO "scd_dev: Read %zu bytes. \n", len);

    mdev->pos -= len;
    *offset = mdev->pos;

    mutex_unlock(&mdev->sem);

    return len;
}

static ssize_t scd_dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
    scd_dev_t *mdev = (scd_dev_t *)filep->private_data;

    printk(KERN_INFO "scd_dev_write(pos: %zu, offset: %llu, len: %zu.) \n", mdev->pos, *offset, len);

    if (!mdev)
    {
        printk(KERN_ALERT "scd_dev: No device data. \n");
        return -EFAULT;
    }

    if (mutex_lock_interruptible(&mdev->sem))
        return -ERESTARTSYS;

    if (len + mdev->pos > mdev->buffer_size)
    {
        printk(KERN_ALERT "scd_dev: Wrong lenght or offset size %zu. \n", len);
        mutex_unlock(&mdev->sem);
        return -EFBIG;
    }
    if (copy_from_user(mdev->buffer + mdev->pos, buffer, len))
    {
        mutex_unlock(&mdev->sem);
        printk(KERN_ALERT "scd_dev: Write %zu bytes failed. \n", len);
    }

    printk(KERN_INFO "scd_dev: Write %zu bytes. \n", len);
    mdev->pos += len;
    *offset = mdev->pos;

    mutex_unlock(&mdev->sem);

    return len;
}

static int scd_dev_release(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "scd_dev: Device successfully closed. \n");
    return 0;
}

static loff_t scd_dev_llseek(struct file *filep, loff_t offset, int whence)
{
    loff_t new_position = 0;
    scd_dev_t *mdev = (scd_dev_t *)filep->private_data;

    printk(KERN_INFO "scd_dev: Seek\n");
    if (!mdev)
    {
        pr_warn("scd_dev: No device data. \n");
        return -EFAULT;
    }

    if (mutex_lock_interruptible(&mdev->sem))
        return -ERESTARTSYS;

    switch (whence)
    {
    case SEEK_SET:
        if (offset >= mdev->buffer_size) {
            mutex_unlock(&mdev->sem);
            return -EFBIG;
        }
        new_position = offset;
        printk(KERN_INFO "scd_dev: Seek relative to beginning of file by %lld\n",
               offset);
        break;

    case SEEK_CUR:
        if (mdev->pos + offset >= mdev->buffer_size)
        {
            mutex_unlock(&mdev->sem);
            return -EFBIG;
        }
        new_position = mdev->pos + offset;
        printk(KERN_INFO "scd_dev: Seek relative to current position by %lld\n",
               offset);
        break;

    case SEEK_END:
        if (mdev->buffer_size + offset >= mdev->buffer_size)
        {
            mutex_unlock(&mdev->sem);
            return -EFBIG;
        }
        new_position = mdev->buffer_size + offset;
        printk(KERN_INFO "scd_dev: Seek relative from end of file by %lld\n", offset);
        break;

    default:
        mutex_unlock(&mdev->sem);
        return -EINVAL;
    }
    if (new_position < 0)
    {
        mutex_unlock(&mdev->sem);
        return -EINVAL;
    }

    mdev->pos = new_position;
    mutex_unlock(&mdev->sem);
    return new_position;
}

// SCD DEV PROC OPERATIONS

static int scd_dev_seq_open(struct inode *inode, struct file *filep)
{
    return seq_open(filep, &seq_ops);
}

static void *scd_dev_seq_start(struct seq_file *s, loff_t *pos)
{
    printk(KERN_INFO "scd_dev: %s pos %lld dev_count %d\n", __FUNCTION__,
           *pos, SCD_DEV_COUNT);
    if (*pos >= SCD_DEV_COUNT || *pos < 0)
        return NULL;
    return scd_devs[*pos];
}

static void *scd_dev_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    (*pos)++;
    printk(KERN_INFO "scd_dev: %s pos %lld dev_count %d\n", __FUNCTION__,
           *pos, SCD_DEV_COUNT);
    if (*pos >= SCD_DEV_COUNT)
        return NULL;

    return scd_devs[*pos];
}

static void scd_dev_seq_stop(struct seq_file *s, void *v)
{
    printk(KERN_INFO "scd_dev: %s\n", __FUNCTION__);
}

static int scd_dev_seq_show(struct seq_file *s, void *v)
{
    scd_dev_t *mdev = (scd_dev_t *)v;

    if (mdev == NULL)
    {
        printk(KERN_ALERT "scd_dev: %s error accessing device\n",
               __FUNCTION__);
        return -EFAULT;
    }

    if (mutex_lock_interruptible(&mdev->sem))
        return -ERESTARTSYS;

    seq_printf(s, "scd_dev: %s_%u, size: %zu B, data[%zu B]: ", DEVICE_NAME,
               mdev->id, mdev->buffer_size, mdev->pos);

    for(int i = 0; i < mdev->pos; i++)
    {
        seq_printf(s, "%c", mdev->buffer[i]);
    }
    seq_printf(s, "\n");

    mutex_unlock(&mdev->sem);

    return 0;
}

module_init(scd_dev_init);
module_exit(scd_dev_exit);
