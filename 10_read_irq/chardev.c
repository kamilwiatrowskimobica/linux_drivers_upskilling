#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

// #include <linux/mutex.h>
// #include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/atmioc.h>

#include <linux/ioctl.h>

#include <linux/poll.h>

#include <linux/slab.h>

#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/hw_irq.h>

#define DEV_NAME "pm_char_dev"
#define CLASS_NAME "pm_char_dev_class"
#define DEVICE_NAME "pm_char_device"
#define DATA_SIZE 256
#define CACHE_NAME "cache_name"
#define CACHE_SIZE 14
#define PROBE_IRQ_ATTEMP 5

#define IRQ_NO 11

struct command_t{
    uint32_t size;
    uint8_t *command;
};

#define IOCTL_COMMAND 'k'
#define IOCTL_COMMAND_WRITE _IOW(IOCTL_COMMAND, 1, struct command_t)
#define IOCTL_COMMAND_READ _IOR(IOCTL_COMMAND, 2, struct command_t)
#define IOCTL_OPTION_WRITE _IOW(IOCTL_COMMAND, 3, int)
#define IOCTL_OPTION_READ _IOR(IOCTL_COMMAND, 4, int)
#define IOCTL_RESET _IO(IOCTL_COMMAND, 5)
#define IOCTL_COMMAND_CACHE_WRITE _IOW(IOCTL_COMMAND, 6, char*)
#define IOCTL_COMMAND_CACHE_READ _IOR(IOCTL_COMMAND, 7, char*)
#define IOCTL_COM_MAX_NR 7

struct char_device_t {
    dev_t dev_number;
    char data[DATA_SIZE];
    char *cache;
    int pos_read;
    int pos_write;
    int option;
    int irq_pin;

    spinlock_t spin_lock;
    atomic_t atom;
    // struct mutex lock_mutex;
    // struct completion complet_write;
    // struct completion complet_read;

    struct cdev char_dev; 
    struct class *dev_class;
    struct device *device;
} pm_dev;

struct kmem_cache *dev_cache;

static irqreturn_t irq_handler(int irq, void* data){
    // some action ...
    pr_info("Shared IRQ: Interrupt Occurred\n");
    return IRQ_HANDLED;
}

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

    // run IRQ
    if(pm_dev.irq_pin > 0) {
        struct irq_desc *desc;
        desc = irq_data_to_desc(irq_get_irq_data(IRQ_NO));
        if (!desc) 
        {
            return -EINVAL;
        }
        __this_cpu_write(vector_irq[59], desc);
        asm("int $0x3B");
    }

    struct char_device_t *dev;
    dev = (struct char_device_t*)pfile->private_data;

    // pr_info("DEC AND READ ATOM: %d\n", atomic_dec_return(&dev->atom));

    // if(wait_for_completion_interruptible(&dev->complet_write)){
    //     return -ERESTARTSYS;
    // }

    //     if(mutex_lock_interruptible(&dev->lock_mutex)){
    //     return -ERESTARTSYS;
    // }

    spin_lock(&dev->spin_lock);

    if(count > dev->pos_write - dev->pos_read){
        count = dev->pos_write - dev->pos_read;
    }

    if(*offset + count > DATA_SIZE){
        count = DATA_SIZE - (*offset);
    }    
 
    if(copy_to_user(buf, &(dev->data[*offset]), count)){
        pr_err("Reading error\n");
        spin_unlock(&dev->spin_lock);
        // mutex_unlock(&pm_dev.lock_mutex);
        return -EFAULT;
    }

    *offset += count;
    dev->pos_read += count;

    pr_info("Read %zu\n", count);
    pr_info("Update offset %lld\n", *offset);

    // mutex_unlock(&pm_dev.lock_mutex);
    // complete(&(dev->complet_read));
    spin_unlock(&dev->spin_lock);
    return count;
}

static ssize_t dev_write(struct file *pfile, const char *buf, size_t count, loff_t *offset){
    pr_info("Writting\n");

    struct char_device_t *dev;
    dev = (struct char_device_t*)pfile->private_data;

    // pr_info("INC AND READ ATOM: %d\n", atomic_inc_return(&dev->atom));

    // if(wait_for_completion_interruptible(&dev->complet_read)){
    //     return -ERESTARTSYS;
    // }

    // if(mutex_lock_interruptible(&pm_dev.lock_mutex)){
    //     return -ERESTARTSYS;
    // }    

    spin_lock(&dev->spin_lock);

    if(*offset > DATA_SIZE){
        pr_err("Out of data range\n");
        spin_unlock(&dev->spin_lock);
        // mutex_unlock(&pm_dev.lock_mutex);
        return -ENOMEM;
    }

    if(*offset + count > DATA_SIZE){
        count = DATA_SIZE - (*offset);
    }

    // if(count == 0){
    //     pr_err("No space on device");
    //     return -ENOMEM;
    // }

    if(copy_from_user(&(dev->data[*offset]), buf, count)){
        pr_err("Writting error");
        spin_unlock(&dev->spin_lock);
        // mutex_unlock(&pm_dev.lock_mutex);
        return -EFAULT;
    }

    *offset += count;
    dev->pos_write += count;

    pr_info("Saved %zu\n", count);
    pr_info("Data: %s\n", pm_dev.data);
    pr_info("Update offset %lld\n", *offset);

    // mutex_unlock(&pm_dev.lock_mutex);
    // complete(&(dev->complet_write));
    spin_unlock(&dev->spin_lock);
    return count;
}

static loff_t dev_lseek(struct file *pfile, loff_t offset, int option){
    loff_t tmp;
    // if(mutex_lock_interruptible(&pm_dev.lock_mutex)){
    //     return -ERESTARTSYS;
    // }
    switch(option){
        case SEEK_SET:
            if(offset > DATA_SIZE || offset < 0){
                // mutex_unlock(&pm_dev.lock_mutex);
                return -EINVAL;
            }
            
            pfile->f_pos = offset;
            break;
        case SEEK_CUR:
            tmp = pfile->f_pos + offset;
            if(tmp > DATA_SIZE || tmp < 0){
                // mutex_unlock(&pm_dev.lock_mutex);
                return -EINVAL;
            }
            pfile->f_pos = tmp;
            break;
        case SEEK_END:
            tmp = DATA_SIZE - offset;
            if(tmp > DATA_SIZE || tmp < 0){
                // mutex_unlock(&pm_dev.lock_mutex);
                return -EINVAL;
            }
            pfile->f_pos = tmp;
            break;
        default:
            // mutex_unlock(&pm_dev.lock_mutex);
            return -EINVAL;
    }
    pr_info("Set new position: %lld\n", pfile->f_pos);
    // mutex_unlock(&pm_dev.lock_mutex);
    return pfile->f_pos;
}

static long dev_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg){
    pr_info("IOCTL\n");
    int ret = 0;
    struct char_device_t *dev;
    dev = (struct char_device_t*)pfile->private_data;
    struct command_t buf;

    if(_IOC_TYPE(cmd) != IOCTL_COMMAND){
        pr_err("Wrong command name\n");
        return -ENOTTY;
    }

    if(_IOC_NR(cmd) > IOCTL_COM_MAX_NR){
        pr_err("Wrong command number\n");
        return -ENOTTY;
    }

    if(!access_ok((void __user *)arg, _IOC_SIZE(cmd))){
        pr_err("Invalid adress\n");
        return -EFAULT;
    }

    if( cmd == IOCTL_COMMAND_WRITE || cmd == IOCTL_COMMAND_READ){
        if(copy_from_user(&buf, (void __user *) arg, _IOC_SIZE(cmd))){
            pr_err("write/read by pointer command error");
            return -ENOTTY;
        } else {
            if(buf.size > DATA_SIZE){
                buf.size = DATA_SIZE;
            }
        }
    }

    spin_lock(&dev->spin_lock);
    switch(cmd) {
        case IOCTL_OPTION_READ:
            pr_info("read option by value\n");
            ret = dev->option;
            break;
        case IOCTL_OPTION_WRITE:
            pr_info("write option by value\n");
            dev->option = (int)arg;
            break;
        case IOCTL_COMMAND_WRITE:
            pr_info("write struct/data by pointer\n");
            if(copy_from_user(dev->data, buf.command, buf.size)){
                pr_err("write data error");
                ret = -ENOTTY;
            } else {
                pr_info("data %s\n", dev->data);
            }
            break;
        case IOCTL_COMMAND_READ:
            pr_info("read struct/data by pointer\n");
            if(copy_to_user(buf.command, dev->data, buf.size)){
                pr_err("read data error");
                ret = -ENOTTY;
            }
            break;
        case IOCTL_RESET:
            pr_info("reset buffor\n");
            dev->pos_read = 0;
            dev->pos_write = 0;
            memset(dev->data, '\0', sizeof(dev->data));
            dev->option = 0;
            break;
        case IOCTL_COMMAND_CACHE_WRITE:
            pr_info("cache write\n");
            if(!dev->cache){
                pr_info("alloc new cache\n");
                dev->cache = kmem_cache_alloc(dev_cache, GFP_KERNEL);
            }

            if(copy_from_user(dev->cache, (void __user *) arg, CACHE_SIZE)){
                pr_err("write cache error\n");
                ret -ENOTTY;
            } else {
                pr_info("data %s\n", dev->cache);
            }
            break;
        case IOCTL_COMMAND_CACHE_READ:
            pr_info("cache read\n");
            if(!dev->cache){
                pr_err("read no memory allocated\n");
                ret -ENOTTY;
                break;
            }

            if(copy_to_user((char *)arg, dev->cache, CACHE_SIZE)){
                pr_err("read cache error\n");
                ret -ENOTTY;
            }
            kmem_cache_free(dev_cache, dev->cache);
            dev->cache = NULL;
            pr_info("free cache\n");
            break;

        default:
            pr_err("unknow command [%u]\n", cmd);
            ret = -ENOTTY;
    }
    spin_unlock(&dev->spin_lock);
    return ret;
}

static unsigned int dev_poll(struct file *pfile, struct poll_table_struct *ptable){
    struct char_device_t* dev = (struct char_device_t*)pfile->private_data;
    unsigned int mask = 0;

    if(dev->pos_write > dev->pos_read){
        mask |= POLLIN | POLLRDNORM;
    }

    if(dev->pos_write < DATA_SIZE ){
        mask |= POLLOUT | POLLWRNORM;
    }

    if(dev->pos_write >= DATA_SIZE || dev->pos_read >= DATA_SIZE){
        mask = POLLERR;
    }

    return mask;
}

static struct file_operations foper = {
    .open= dev_open,
    .release = dev_close,
    .read = dev_read,
    .write = dev_write,
    .llseek = dev_lseek,
    .unlocked_ioctl = dev_ioctl,
    .poll = dev_poll,
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

    spin_lock_init(&pm_dev.spin_lock);
    atomic_set(&pm_dev.atom, 0);
    // mutex_init(&pm_dev.lock_mutex);
    
    // init_completion(&pm_dev.complet_write);
    // init_completion(&pm_dev.complet_read);
    // complete(&pm_dev.complet_read);

    dev_cache = kmem_cache_create(CACHE_NAME, CACHE_SIZE, 0, SLAB_HWCACHE_ALIGN, NULL);

    // virtual IRQ from reading file
    if(request_irq(IRQ_NO, irq_handler, IRQF_SHARED, "basic_irq", (void*)irq_handler)) {
        pr_err("IRQ Handler added failed \n");
        pm_dev.irq_pin = -1;
    } else {
        pr_info("IRQ Handler added successfully\n");
        pm_dev.irq_pin = IRQ_NO;
    }


    // IRQ pin probing HW case
    // unsigned long mask;
    // int irq = 0;
    // for (int i = 0; i < PROBE_IRQ_ATTEMP; i++){
    //     mask = probe_irq_on();
    //     mdaley(5000); // 5s to push button
    //     irq = probe_irq_off(mask);

    //     if (irq > 0) {
    //         pm_dev.irq_pin = irq;
    //         pr_info("catch irq from pin %d\n", pm_dev.irq_pin);
    //         break;
    //     } if (irq == 0)
    //     {
    //         pr_info("no IRQ was caught, next attempt [%d/%d]\n", i, PROBE_IRQ_ATTEMP);
    //     }
    //     else
    //     {
    //         pr_err("probe error: irq = %d, next attempt\n", irq);
    //         irq = 0;
    //     }
    // }

    // if( pm.irq_pin > 0){
    //     pr_info("Installing IRQ handler\n");
    //     int irq_respon = request_irq(pm.irq_pin, irq_handler, SA_INTERRUP, "basic_irq", NULL);
    // }


    return 0;
}

static void __exit exit_char_dev(void){
    pr_info("Remove char device\n");
    // mutex_destroy(&pm_dev.lock_mutex);

    if(pm_dev.irq_pin > 0) {
        free_irq(pm_dev.irq_pin, (void*)irq_handler);
    }

    if(dev_cache){
        kmem_cache_destroy(dev_cache);
    }

    device_destroy(pm_dev.dev_class, pm_dev.dev_number);
    class_destroy(pm_dev.dev_class);
    cdev_del(&(pm_dev.char_dev));
    unregister_chrdev_region(pm_dev.dev_number, 1);
}

module_init(init_char_dev);
module_exit(exit_char_dev);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pawel Marchewka");
MODULE_DESCRIPTION("pseudo char driver with IRQ");
MODULE_VERSION("0.1");
