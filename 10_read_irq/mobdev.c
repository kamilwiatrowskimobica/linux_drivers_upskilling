#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h> // kmalloc()
#include <linux/uaccess.h> // copy_to/from_user()
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/err.h>
#include <asm/hw_irq.h>
// #include <asm/io.h>

#include "mobdev.h"

#define CLASS_NAME "mobica"
#define DEVICE_NAME "mobdev_irq"

#define IRQ_NO 11
// #define IO_MEM_SIZE 8

static const unsigned int mob_devs_num = 1;
static int dev_minor_start = 0;
static int dev_major = 0;

static struct mob_dev *mob_devices = NULL;
static struct class *mob_class = NULL;

void bottom_handler(struct tasklet_struct *t);
DECLARE_TASKLET(mob_tl, bottom_handler);

//Interrupt handler for IRQ_NO.
static irqreturn_t irq_handler(int irq, void *dev_id)
{
	pr_info("Shared IRQ: Interrupt Occurred\n");
	tasklet_schedule(&mob_tl);

	return IRQ_HANDLED;
}

void bottom_handler(struct tasklet_struct *t)
{
	pr_info("Handling bottom half in tasklet\n");
}

static int mobdev_open(struct inode *inode, struct file *file)
{
	pr_info("device file open...!!!\n");
	return nonseekable_open(inode, file);
}

static int mobdev_release(struct inode *inode, struct file *file)
{
	pr_info("device file released...!!!\n");
	return 0;
}

#define IRQ_VEC_NO (FIRST_EXTERNAL_VECTOR + 0x10 + IRQ_NO) // 0x3B
static ssize_t mobdev_read(struct file *filp, char __user *buf, size_t len,
			   loff_t *off)
{
	struct irq_desc *desc;
	pr_debug("mobdev_read, len = %zu, off = %lld\n", len, *off);

	// desc = irq_to_desc(IRQ_NO);
	desc = irq_data_to_desc(irq_get_irq_data(IRQ_NO));
	if (!desc)
		return -EINVAL;

	__this_cpu_write(vector_irq[IRQ_VEC_NO], desc);
	asm("int $0x3B"); // Corresponding to irq 11

	return 0;
}

static ssize_t mobdev_write(struct file *filp, const char __user *buf,
			    size_t len, loff_t *off)
{
	pr_info("mobdev_write, len = %zu, off = %lld\n", len, *off);

	return len;
}

/*
** File Operations structure
*/
static struct file_operations fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = mobdev_read,
	.write = mobdev_write,
	.open = mobdev_open,
	.release = mobdev_release,
};

static void mobdev_clean_cdev(struct mob_dev *mdev)
{
	device_destroy(mob_class, mdev->c_dev.dev);
	cdev_del(&mdev->c_dev);
}

static int mobdev_init_cdev(struct mob_dev *mdev, size_t index)
{
	int result = 0;
	struct device *mob_device = NULL;
	dev_t dev = MKDEV(dev_major, dev_minor_start + index);

	cdev_init(&mdev->c_dev, &fops);
	mdev->c_dev.owner = THIS_MODULE;

	result = cdev_add(&mdev->c_dev, dev, 1);
	if (result < 0) {
		pr_warn("failed to add cdev\n");
		return result;
	}

	mob_device = device_create(mob_class, NULL, dev, NULL, "%s_%zu",
				   DEVICE_NAME, index);
	if (IS_ERR(mob_device)) {
		pr_warn("failed to create device\n");
		return PTR_ERR(mob_device);
	}

	return 0;
}

/*
** Module Init function
*/
static __init int mobdev_init(void)
{
	int result = 0;
	size_t i;
	dev_t dev = 0;

	result = alloc_chrdev_region(&dev, dev_minor_start, mob_devs_num,
				     DEVICE_NAME);
	if (result < 0) {
		pr_warn("Cannot allocate major number\n");
		return result;
	}

	dev_major = MAJOR(dev);

	mob_devices = kcalloc(mob_devs_num, sizeof(struct mob_dev), GFP_KERNEL);
	if (!mob_devices) {
		pr_warn("kcalloc failed for mob devices\n");
		result = -ENOMEM;
		goto clean_chrdev;
	}

	mob_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(mob_class)) {
		pr_warn("failed to create class\n");
		result = PTR_ERR(mob_class);
		goto clean_mobdevs;
	}

	/* Initialize each cdevice */
	for (i = 0; i < mob_devs_num; i++) {
		result = mobdev_init_cdev(&mob_devices[i], i);
		if (result < 0) {
			pr_warn("Error adding mob device %zu\n", i);
			goto clean_cdevs;
		}
	}

	if (request_irq(IRQ_NO, irq_handler, IRQF_SHARED, DEVICE_NAME, (void *)(irq_handler))) {
		pr_warn(DEVICE_NAME ": cannot register IRQ\n");
		result = -EFAULT;
		goto clean_cdevs;
    }

	pr_info("mobdev_init done\n");
	return 0;

clean_cdevs:
	for (i = 0; i < mob_devs_num; i++) {
		mobdev_clean_cdev(&mob_devices[i]);
	}
	class_destroy(mob_class);
clean_mobdevs:
	kfree(mob_devices);
clean_chrdev:
	unregister_chrdev_region(dev, mob_devs_num);
	return result;
}

/*
** Module exit function
*/
static __exit void mobdev_exit(void)
{
	size_t i;
	pr_info("mobdev_exit\n");
	tasklet_kill(&mob_tl);
	free_irq(IRQ_NO, (void *)(irq_handler));
	if (mob_devices) {
		for (i = 0; i < mob_devs_num; i++) {
			mobdev_clean_cdev(&mob_devices[i]);
		}
		kfree(mob_devices);
	}
	class_destroy(mob_class);
	unregister_chrdev_region(MKDEV(dev_major, dev_minor_start),
				 mob_devs_num);
}

module_init(mobdev_init);
module_exit(mobdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kamil Wiatrowski");
MODULE_DESCRIPTION("Character driver with IRQ.");
MODULE_VERSION("0.1.3");
