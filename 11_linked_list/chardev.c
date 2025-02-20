#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#include <linux/spinlock.h>

#include <linux/list.h>

#define DEV_NAME "pm_char_dev"
#define CLASS_NAME "pm_char_dev_class"
#define DEVICE_NAME "pm_char_device"
#define DATA_SIZE 256

struct llist_data_t {
    struct list_head list;
    char data[DATA_SIZE];
};

struct char_device_t {
    dev_t dev_number;
    char data[DATA_SIZE];

    struct list_head llist_head;

    spinlock_t spin_lock;

    struct cdev char_dev; 
    struct class *dev_class;
    struct device *device;
} pm_dev;

struct kmem_cache *dev_cache;

static int dev_open(struct inode *pinode, struct file *pfile){
    pr_info("Open\n");
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

    struct char_device_t *dev = (struct char_device_t*)pfile->private_data;
    char tmp[DATA_SIZE];
    uint8_t tmp_pos = 0;
    struct list_head *ptr;

    spin_lock(&dev->spin_lock);

    pr_info("Init counter: %zu\n", count);
    pr_info("Init offset: %lld\n", *offset);

    if(*offset + count > DATA_SIZE){
        count = DATA_SIZE - (*offset);
    }

    list_for_each(ptr, &pm_dev.llist_head) {
        struct llist_data_t* item = list_entry(ptr, struct llist_data_t, list);
        
        strncpy(tmp + tmp_pos, item->data, strlen(item->data));
        tmp_pos += strlen(item->data);

        pr_info("Read list item (size %zu): '%s' \n", strlen(item->data), item->data);   
    }

    if(count > strlen(tmp)) {
        count = strlen(tmp);
    }

    if(copy_to_user(buf, tmp, count)){
        pr_err("reading link list error\n");
        spin_unlock(&dev->spin_lock);
        return -EFAULT;
    }

    *offset += count;

    pr_info("Updated counter: %zu\n", count);
    pr_info("Updated offset: %lld\n", *offset);

    spin_unlock(&dev->spin_lock);

    return count;
}

static ssize_t dev_write(struct file *pfile, const char *buf, size_t count, loff_t *offset){
    pr_info("Writting\n");

    struct char_device_t *dev = (struct char_device_t*)pfile->private_data;
    char tmp[DATA_SIZE];

    spin_lock(&dev->spin_lock);

    pr_info("Init counter: %zu\n", count);
    pr_info("Init offset: %lld\n", *offset);

    if(*offset + count > DATA_SIZE){
        count = DATA_SIZE - (*offset);
    }

    if(copy_from_user(tmp, buf, count)){
        pr_err("Write failed\n");
        spin_unlock(&dev->spin_lock);
        return -EFAULT;
    }

    struct llist_data_t* list_item = (struct llist_data_t*)kmalloc(sizeof(struct llist_data_t), GFP_KERNEL);
    if(list_item != NULL){
        strncpy(list_item->data, tmp, count); 
        pr_info("data in linked list item: %s\n", list_item->data);
        list_add_tail(&list_item->list, &pm_dev.llist_head);
    } else {
        pr_err("Linked list item creation failed.");
    }

    *offset += count;
    pr_info("Updated counter: %zu\n", count);
    pr_info("Updated offset: %lld\n", *offset);
    spin_unlock(&dev->spin_lock);
    return count;
}


static struct file_operations foper = {
    .open= dev_open,
    .release = dev_close,
    .read = dev_read,
    .write = dev_write,
    .owner = THIS_MODULE,
};

static int __init init_char_dev(void){
    pr_info("Start init device\n");

    int ret = 0;

    pr_info("1. Allo region\n");
    ret = alloc_chrdev_region(&pm_dev.dev_number, 0, 1, DEV_NAME);
    if(ret < 0){
        pr_err("Allocation failed\n");
        return ret;
    }
    pr_info("Dev ID: %d:%d \n", MAJOR(pm_dev.dev_number), MINOR(pm_dev.dev_number));

    pr_info("2. Init dev\n");
    cdev_init(&pm_dev.char_dev, &foper);
    pm_dev.char_dev.owner=THIS_MODULE;

    pr_info("3. Pair cdev with dev number\n");
    ret = cdev_add(&pm_dev.char_dev, pm_dev.dev_number, 1);
    if(ret < 0){
        pr_err("Paired failed\n");
        unregister_chrdev_region(pm_dev.dev_number, 1);
        return ret;
    }

    pr_info("4. Creating class\n");
    pm_dev.dev_class = class_create(CLASS_NAME);
    if(IS_ERR(pm_dev.dev_class)){
        pr_err("Class creation failed\n");

        cdev_del(&pm_dev.char_dev);
        unregister_chrdev_region(pm_dev.dev_number, 1);

        return PTR_ERR(pm_dev.dev_class);
    }

    pr_info("5. Creating device");
    pm_dev.device = device_create(pm_dev.dev_class, NULL, pm_dev.dev_number, NULL, DEVICE_NAME);
    if(IS_ERR(pm_dev.device)){
        pr_err("Device creation failed\n");

        class_destroy(pm_dev.dev_class);
        cdev_del(&pm_dev.char_dev);
        unregister_chrdev_region(pm_dev.dev_number, 1);

        return PTR_ERR(pm_dev.device);
    }

    spin_lock_init(&pm_dev.spin_lock);

    INIT_LIST_HEAD(&pm_dev.llist_head);

    return 0;
}

static void __exit exit_char_dev(void){
    pr_info("Remove char device\n");
    // mutex_destroy(&pm_dev.lock_mutex);

    struct llist_data_t *ptr, *tmp;
    list_for_each_entry_safe(ptr, tmp, &pm_dev.llist_head, list) {
        pr_info("Remove list item (%s) \n", ptr->data);
        kfree(ptr->data);
        list_del(&ptr->list);
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
MODULE_DESCRIPTION("linked list");
MODULE_VERSION("0.1");
