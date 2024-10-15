#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define DEV_NAME "pm_char_dev"
#define CLASS_NAME "pm_char_dev_class"
#define DEVICE_NAME "pm_char_device"
#define DATA_SIZE 256
#define NO_OF_DEV 3

#define PROC_FILE_NAME_ALL_IN_ONE "chardev_all_in_one"
#define PROC_FILE_NAME_ITER "chardev_iter"

struct dev_data {
    char data[DATA_SIZE];
    struct cdev char_dev;
    int id;
};

struct char_device_t {
    dev_t dev_number;
    struct dev_data devs[NO_OF_DEV];
    struct class *dev_class;
    struct device *device;
} pm_dev;


//proc all in one

int read_proc_all_in_one(struct seq_file *psfile, void *v){
    int len = 0;
    for(int i= 0; i< NO_OF_DEV; i++){
        seq_printf(psfile, "Device ID: %d\n", pm_dev.devs[i].id);
        seq_printf(psfile, "Device data: %s\n", pm_dev.devs[i].data);
    }
    return len;
}

//proc iter

static void *seq_proc_start(struct seq_file *psfile, loff_t *pos){
    if(*pos >= NO_OF_DEV){
        return NULL;
    }
    return pm_dev.devs + *pos;
}

static void *seq_proc_next(struct seq_file *psfile, void *v, loff_t *pos){
    (*pos)++;
    if(*pos >= NO_OF_DEV){
        return NULL;
    }
    return pm_dev.devs + *pos;
}

int seq_proc_show(struct seq_file *psfile, void* v){
    if(v == NULL){
        return -EFAULT;
    }

    struct dev_data *dev = (struct dev_data*)v;
    seq_printf(psfile, "Device ID: %d\n", dev->id);
    seq_printf(psfile, "Device data: %s\n", dev->data);
    return 0;
}

static void seq_proc_stop(struct seq_file *psfile, void *v) {
}

static struct seq_operations seq_proc_ops = {
    .start = seq_proc_start,
    .next = seq_proc_next,
    .stop = seq_proc_stop,
    .show = seq_proc_show
};

static int seq_proc_open(struct inode *inode, struct file *pfile){
    return seq_open(pfile, &seq_proc_ops);
}

static struct proc_ops seq_ops = {
    // .owner = THIS_MODULE,
    .proc_open = seq_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release
};

// normal operations

static int dev_open(struct inode *pinode, struct file *pfile){
    pr_info("Open device\n");
    struct dev_data *dev;
    dev = container_of(pinode->i_cdev, struct dev_data, char_dev);
    pfile->private_data = dev;
    return 0;
}

static int dev_close(struct inode * pinode, struct file *pfile){
    return 0;
}

static ssize_t dev_read(struct file *pfile, char *buf, size_t count, loff_t *offset){
    pr_info("Reading\n");

    struct dev_data *dev = (struct dev_data*)pfile->private_data;  

    if(*offset + count > DATA_SIZE){
        count = DATA_SIZE - (*offset);
    }

    if(copy_to_user(buf, &dev->data[*offset], count)){
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

    struct dev_data *dev = (struct dev_data*)pfile->private_data;

    if(copy_from_user(&dev->data[*offset], buf, count)){
        pr_err("Writting error");
        return -EFAULT;
    }

    *offset += count;

    pr_info("Saved %d\n", count);
    pr_info("Data: %s\n", dev->data);
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
    .open = dev_open,
    .release = dev_close,
    .read = dev_read,
    .write = dev_write,
    .llseek = dev_lseek,
    .owner = THIS_MODULE,
};

static void clean_up_dev(int dev_num){
    if(dev_num > NO_OF_DEV){
        dev_num = NO_OF_DEV;
    }

    for(int i = 0; i < dev_num; i++){
        pr_info("Remove cdev and device with ID %d\n", dev_num-1-i);
        device_destroy(pm_dev.dev_class, pm_dev.dev_number + dev_num-1-i);
        cdev_del(&(pm_dev.devs[dev_num-1-i].char_dev));
    }
    pr_info("Remove class and unregister region\n");
    class_destroy(pm_dev.dev_class);
    unregister_chrdev_region(pm_dev.dev_number, NO_OF_DEV);
}

static int __init init_char_dev(void){
    pr_info("Start init device\n");

    int ret;

    pr_info("1. Allocation a device number\n");
    ret = alloc_chrdev_region(&pm_dev.dev_number, 0, NO_OF_DEV, DEV_NAME);
    if(ret < 0){
        pr_err("Allocation error\n");
        return ret;
    }

    pr_info("Major and minor number: %d:%d\n", MAJOR(pm_dev.dev_number), MINOR(pm_dev.dev_number));
    char buf[64];
    int n = print_dev_t(buf, pm_dev.dev_number);
    pr_info("Major and minor number ver2[%d]: %s\n",n, buf);

    pr_info("2. Creating class\n");
    pm_dev.dev_class = class_create(CLASS_NAME);
    if(IS_ERR(pm_dev.dev_class)){
        pr_err("Creating class error\n");

        unregister_chrdev_region(pm_dev.dev_number, 1);

        return PTR_ERR(pm_dev.dev_class);
    }

    for(int i = 0; i < NO_OF_DEV; i++){

        pr_info("3.%d Init char device\n", i);
        cdev_init(&pm_dev.devs[i].char_dev, &foper);
        pm_dev.devs[i].char_dev.owner=THIS_MODULE;

        pr_info("4.%d Pair char device with a device number\n", i);
        ret = cdev_add(&pm_dev.devs[i].char_dev, pm_dev.dev_number + i, 1);
        if(ret < 0){
            pr_err("Parring error\n");

            clean_up_dev(i);

            return ret;
        }

        pr_info("5.%d Creating device\n", i);
        pm_dev.device = device_create(pm_dev.dev_class, NULL, pm_dev.dev_number + i, NULL, "pm_char_device_%d", i);
        if(IS_ERR(pm_dev.device)){
            pr_err("Creating device error\n");

            clean_up_dev(i);

            return PTR_ERR(pm_dev.device);
        }
        pm_dev.devs[i].id = i;
    }

    proc_create_single_data(PROC_FILE_NAME_ALL_IN_ONE, 0, NULL, read_proc_all_in_one, NULL);

    struct proc_dir_entry *entry;
    entry = proc_create(PROC_FILE_NAME_ITER, 0, NULL, &seq_ops);

    return 0;
}

static void __exit exit_char_dev(void){
    pr_info("Remove char device\n");

    remove_proc_entry(PROC_FILE_NAME_ALL_IN_ONE, NULL);
    remove_proc_entry(PROC_FILE_NAME_ITER, NULL);

    clean_up_dev(NO_OF_DEV);
}

module_init(init_char_dev);
module_exit(exit_char_dev);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pawel Marchewka");
MODULE_DESCRIPTION("pseudo multiple char drivers");
MODULE_VERSION("0.1");
