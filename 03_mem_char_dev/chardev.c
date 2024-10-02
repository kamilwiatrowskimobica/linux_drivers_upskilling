#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#define DEV_NAME "pm_char_dev"
#define CLASS_NAME "pm_char_dev_class"
#define DEVICE_NAME "pm_char_device"
#define DATA_SIZE 256

struct char_device_t {
    dev_t dev_number;
    char data[DATA_SIZE];
    struct cdev char_dev; 
    struct class *dev_class;
    struct device *device;
} pm_dev;

static int dev_open(struct inode *pinode, struct file *pfile){
    pr_info("Open device\n");
    struct char_device_t *dev;
    dev = container_of(pinode->i_cdev, struct char_device_t, char_dev);
    pfile->private_data = dev;
    return 0;
}

static int dev_close(struct inode * pinode, struct file *pfile){
    return 0;
}

static ssize_t dev_read(struct file *pfile, char *buf, size_t count, loff_t *offset){
    pr_info("Reading\n");

    if(*offset + count > DATA_SIZE){
        count = DATA_SIZE - (*offset);
    }    

    if(copy_to_user(buf, &pm_dev.data[*offset], count)){
        pr_err("Reading error\n");
        return -EFAULT;
    }

    *offset += count;

    pr_info("Read %d\n", count);
    pr_info("Update offset %lld\n", *offset);
    
    return count;
}

static ssize_t dev_write(struct file *pfile, const char *buf, size_t count, loff_t *offset){
    pr_info("Writting\n");

    if(*offset > DATA_SIZE){
        pr_err("Out of data range\n");
        return -ENOMEM;
    }

    if(*offset + count > DATA_SIZE){
        count = DATA_SIZE - (*offset);
    }

    // if(count == 0){
    //     pr_err("No space on device");
    //     return -ENOMEM;
    // }

    if(copy_from_user(&pm_dev.data[*offset], buf, count)){
        pr_err("Writting error");
        return -EFAULT;
    }

    *offset += count;

    pr_info("Saved %d\n", count);
    pr_info("Data: %s\n", pm_dev.data);
    pr_info("Update offset %lld\n", *offset);

    return count;
}

static loff_t dev_lseek(struct file *pfile, loff_t offset, int option){
    loff_t tmp;
    switch(option){
        case SEEK_SET:
            if(offset > DATA_SIZE || offset < 0){
                return -EINVAL;
            }
            pfile->f_pos = offset;
            break;
        case SEEK_CUR:
            tmp = pfile->f_pos + offset;
            if(tmp > DATA_SIZE || tmp < 0){
                return -EINVAL;
            }
            pfile->f_pos = tmp;
            break;
        case SEEK_END:
            tmp = DATA_SIZE - offset;
            if(tmp > DATA_SIZE || tmp < 0){
                return -EINVAL;
            }
            pfile->f_pos = tmp;
            break;
        default:
            return -EINVAL;
    }
    pr_info("Set new position: %lld\n", pfile->f_pos);
    return pfile->f_pos;
}

static struct file_operations foper = {
    .open= dev_open,
    .release = dev_close,
    .read = dev_read,
    .write = dev_write,
    .llseek = dev_lseek,
    .owner = THIS_MODULE,
};

static int __init init_char_dev(void){
    pr_info("Start init device\n");

    int ret;

    pr_info("1. Allocation a device number\n");
    ret = alloc_chrdev_region(&pm_dev.dev_number, 0, 1, DEV_NAME);
    if(ret < 0){
        pr_err("Allocation error\n");
        return ret;
    }

    pr_info("Major and minor number: %d:%d\n", MAJOR(pm_dev.dev_number), MINOR(pm_dev.dev_number));

    pr_info("2. Init char device\n");
    cdev_init(&pm_dev.char_dev, &foper);
    pm_dev.char_dev.owner=THIS_MODULE;

    pr_info("3. Pair char device with a device number\n");
    ret = cdev_add(&pm_dev.char_dev, pm_dev.dev_number, 1);
    if(ret < 0){
        pr_err("Parring error\n");
        unregister_chrdev_region(pm_dev.dev_number, 1);
        return ret;
    }

    pr_info("3. Creating class\n");
    pm_dev.dev_class = class_create(CLASS_NAME);
    if(IS_ERR(pm_dev.dev_class)){
        pr_err("Creating class error\n");

        cdev_del(&(pm_dev.char_dev));
        unregister_chrdev_region(pm_dev.dev_number, 1);

        return PTR_ERR(pm_dev.dev_class);
    }

    pr_info("4. Creating device\n");
    pm_dev.device = device_create(pm_dev.dev_class, NULL, pm_dev.dev_number, NULL, DEVICE_NAME);
    if(IS_ERR(pm_dev.device)){
        pr_err("Creating device error\n");

        class_destroy(pm_dev.dev_class);
        cdev_del(&(pm_dev.char_dev));
        unregister_chrdev_region(pm_dev.dev_number, 1);

        return PTR_ERR(pm_dev.device);
    }

    return 0;
}

static void __exit exit_char_dev(void){
    pr_info("Remove char device\n");
    device_destroy(pm_dev.dev_class, pm_dev.dev_number);
    class_destroy(pm_dev.dev_class);
    cdev_del(&(pm_dev.char_dev));
    unregister_chrdev_region(pm_dev.dev_number, 1);
}

module_init(init_char_dev);
module_exit(exit_char_dev);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pawel Marchewka");
MODULE_DESCRIPTION("pseudo char driver");
MODULE_VERSION("0.1");
