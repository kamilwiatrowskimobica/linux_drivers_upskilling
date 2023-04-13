#include <linux/init.h> // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h> // Core header for loading LKMs into the kernel
#include <linux/device.h> // Header to support the kernel Driver Model
#include <linux/kernel.h> // Contains types, macros, functions for the kernel
#include <linux/fs.h> // Header for the Linux file system support
#include <linux/uaccess.h> // Required for the copy to user function
#include <linux/slab.h> // kmalloc()
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/hw_irq.h>
#include <linux/version.h>
#include <linux/spinlock.h>

#include "mobi_module.h"

///< The device will appear at /dev/mob using this value
#define MOB_DEVICE_NAME "mob"
///< The device class -- this is a character device driver
#define CLASS_NAME "lukr"
#define DEVICE_NAME_FORMAT "%s_%zu"

#define MOB_MESSAGE_LEN 256
#define MOB_DEFAULT_DEV_COUNT 1

#define IRQ_NO 11

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lukasz Krakowiak");
MODULE_DESCRIPTION("A simple Linux driver with IRQ handling");
MODULE_VERSION("0.1");

static uint dev_count = MOB_DEFAULT_DEV_COUNT;
// module_param(dev_count, uint, S_IRUGO);
// MODULE_PARM_DESC(dev_count, "Number of creating mob devices");

static int major_number = 0;
///< The device-driver class struct pointer
static struct class *mob_class = NULL;
static struct mob_dev *mob_devices = NULL;
static unsigned long IRQ_counter = 0;
static unsigned long tasklet_counter = 0;

#if LINUX_VERSION_CODE == KERNEL_VERSION(5,10,14) //kernel version with QEMU
void bottom_handler(struct tasklet_struct *t);
DECLARE_TASKLET(mob_tl, bottom_handler);
#else
void bottom_handler(unsigned long);
DECLARE_TASKLET(mob_tl, bottom_handler, 0);
#endif

DEFINE_SPINLOCK(mobi_spinlock);

// The prototype functions for the character driver -- must come before the struct definition
static int mobdev_open(struct inode *, struct file *);
static int mobdev_release(struct inode *, struct file *);
static ssize_t mobdev_read(struct file *, char *, size_t, loff_t *);
static ssize_t mobdev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
	.open = mobdev_open,
	.read = mobdev_read,
	.write = mobdev_write,
	.release = mobdev_release,
};

typedef enum {
	CLEANUP_IRQ	= 1,
	CLEANUP_U_REGION = 2,
	CLEANUP_CLASS_DESTROY = 4,
	CLEANUP_KFREE = 8,
	CLEANUP_DEVS = 16,
	CLEANUP_ALL = 31
} type_of_cleanup_t;

//Interrupt handler for IRQ 11.
static irqreturn_t irq_handler(int irq,void *dev_id)
{
	spin_lock(&mobi_spinlock);
	printk(KERN_INFO "MOBChar: Shared Interrupt Occurred, t_counter:%ld\n",
		tasklet_counter);
	tasklet_schedule(&mob_tl);

	IRQ_counter++;
	spin_unlock(&mobi_spinlock);

	return IRQ_HANDLED;
}

#if LINUX_VERSION_CODE == KERNEL_VERSION(5,10,14) //kernel version with QEMU
void bottom_handler(struct tasklet_struct *t)
#else
void bottom_handler(unsigned long l)
#endif
{
	unsigned long state = 0;
	spin_lock_irqsave(&mobi_spinlock, state);
	pr_info("MOBChar: Handling bottom half in tasklet, IRQ_counter%ld\n",
		IRQ_counter);
	tasklet_counter ++;
	spin_unlock_irqrestore(&mobi_spinlock, state);
}

static void mobdev_clean_cdev(struct mob_dev *mdev)
{
	if (mdev == NULL)
		return;

	device_destroy(mob_class, mdev->cdev.dev);
	cdev_del(&mdev->cdev);
}

void exit_handler(uint type_c)
{
	int i = 0;
	dev_t dev = MKDEV(major_number, 0);

	if (type_c & CLEANUP_IRQ) {
		tasklet_kill(&mob_tl);
		free_irq(IRQ_NO,(void *)(irq_handler));
	}

	if ((type_c & CLEANUP_DEVS) && mob_devices) {
		for (i = 0; i < dev_count; i++) {
			mobdev_clean_cdev(&mob_devices[i]);
		}
		printk(KERN_INFO "mobdev_clean_cdev\n");
	}

	if ((type_c & CLEANUP_KFREE) && mob_devices) {
		kfree(mob_devices);
		printk(KERN_INFO "kfree\n");
	}

	if (type_c & CLEANUP_CLASS_DESTROY) {
		class_destroy(mob_class);
		printk(KERN_INFO "class_destroy\n");
	}

	if (type_c & CLEANUP_U_REGION) {
		unregister_chrdev_region(dev, dev_count);
		printk(KERN_INFO "unregister_chrdev_region\n");
	}
}

static int mobdev_open(struct inode *inodep, struct file *filep)
{
	struct mob_dev *mdev;
	mdev = container_of(inodep->i_cdev, struct mob_dev, cdev);
	if (!mdev) {
		printk(KERN_ALERT "MOBChar: %s error accessing mob_dev\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	filep->private_data = mdev; /* for other methods */

	pr_info("MOBChar: Device [%ld] has been opened\n", mdev->id);
	return nonseekable_open(inodep, filep);
}

static int mobdev_release(struct inode *inodep, struct file *filep)
{
	struct mob_dev *mdev;
	mdev = container_of(inodep->i_cdev, struct mob_dev, cdev);
	if (mdev)
		pr_info("MOBChar: Device [%ld] successfully closed\n", mdev->id);
	return 0;
}

#define IRQ_VEC_NO (FIRST_EXTERNAL_VECTOR + 0x10 + IRQ_NO) // 0x3B
// if missing vector_irq, modify arch/x86/kernel/irq.c, add line:
// EXPORT_SYMBOL(vector_irq);
// and recompile kernel
static ssize_t mobdev_read(struct file *filep, char *buffer, size_t len,
			   loff_t *offset)
{
	struct irq_desc *desc;
	pr_info("MOBChar: mobdev_read\n");

	desc = irq_data_to_desc(irq_get_irq_data(IRQ_NO));
	if (!desc)
		return -EINVAL;

	__this_cpu_write(vector_irq[IRQ_VEC_NO], desc);
	asm("int $0x3B"); // Corresponding to irq 11

	return 0;
}

static ssize_t mobdev_write(struct file *filep, const char *buffer, size_t len,
			    loff_t *offset)
{
	struct mob_dev *mdev = (struct mob_dev *)filep->private_data;

	if (mdev == NULL) {
		printk(KERN_ALERT "MOBChar: %s error accessing mob_dev\n",
		       __FUNCTION__);
		return -EFAULT;
	}

	pr_info("MOBChar: %s_%d len = %zu, off = %lld\n",
		MOB_DEVICE_NAME, MINOR(mdev->cdev.dev), len, *offset);

	return 0;
}

static int mobdev_init_cdev(struct mob_dev *mdev, size_t index)
{
	int result = 0;
	dev_t dev = MKDEV(major_number, index);
	struct device *mob_device = NULL;

	cdev_init(&mdev->cdev, &fops);
	mdev->cdev.owner = THIS_MODULE;

	result = cdev_add(&mdev->cdev, dev, 1);
	if (result < 0) {
		printk(KERN_ALERT "MOBChar: failed to add cdev\n");
		return result;
	}

	mdev->id = index;

	// Register the device driver
	mob_device = device_create(mob_class, NULL, dev, NULL,
				   DEVICE_NAME_FORMAT, MOB_DEVICE_NAME, index);
	if (IS_ERR(mob_device)) {
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(mob_device);
	}

	printk(KERN_INFO "MOBChar: created %s_%d\n", MOB_DEVICE_NAME,
	       MINOR(dev));

	return 0;
}

static int __init mobdev_init(void)
{
	dev_t dev = 0;
	int result = 0;
	int i = 0;

	result = alloc_chrdev_region(&dev, 0, dev_count, MOB_DEVICE_NAME);
	if (result < 0) {
		printk(KERN_ALERT "MOBChar: failed to alloc char device\n");
		return result;
	}

	// get allocated dev major number from dev_t
	major_number = MAJOR(dev);

	mob_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(mob_class)) {
		printk(KERN_ALERT "MOBChar: Failed to reg device class\n");
		exit_handler(CLEANUP_U_REGION);
		return PTR_ERR(mob_class);
	}

	/* Initialize the device. */
	mob_devices = kcalloc(dev_count, sizeof(struct mob_dev), GFP_KERNEL);
	if (IS_ERR(mob_devices)) {
		printk(KERN_ALERT "MOBChar: Failed kcalloc\n");
		exit_handler(CLEANUP_U_REGION | CLEANUP_CLASS_DESTROY);
		return PTR_ERR(mob_devices);
	}

	for (i = 0; i < dev_count; i++) {
		result = mobdev_init_cdev(&mob_devices[i], i);
		if (result < 0) {
			exit_handler(CLEANUP_ALL & ~CLEANUP_IRQ);
			printk(KERN_ALERT "MOBChar: Error init device %d\n", i);
			return result;
		}
	}

	if (request_irq(IRQ_NO, irq_handler, IRQF_SHARED, MOB_DEVICE_NAME, (void *)(irq_handler)))
	{
		printk(KERN_INFO "MOBChar: cannot register IRQ: %d", IRQ_NO);
		exit_handler(CLEANUP_ALL);
	}

	printk(KERN_INFO "MOBChar: Init LKM correctly with major number %d\n",
	       major_number);
	return 0;
}

static void __exit mobdev_exit(void)
{
	exit_handler(CLEANUP_ALL);
	printk(KERN_INFO "MOBChar: Goodbye from the LKM!\n");
}

module_init(mobdev_init);
module_exit(mobdev_exit);
