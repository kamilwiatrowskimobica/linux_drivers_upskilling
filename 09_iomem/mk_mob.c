/**
 * @file   mk_mob.c
 * @author Michal Kozikowski
 * @date   11 June 2024
 * @version 0.1
 * @brief   A simple character driver using preallocated RAM memory. 
 * Implementation is based on the book "Linux Device Drivers" 
 * by Alessandro Rubini and Jonathan Corbet, published by O'Reilly & Associates.
 */

#include "mk_mob.h"

#include <asm/io.h>
#include <linux/init.h> // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h> // Core header for loading LKMs into the kernel
#include <linux/kernel.h> // Contains types, macros, functions for the kernel
#include <linux/fs.h> // Header for the Linux file system support
#include <linux/slab.h>
#include <linux/uaccess.h> // Required for the copy to user function

#define DEVICE_NAME "mk_mob" ///< The device will appear at /dev/mk_mob using this value
#define CLASS_NAME  "mob" ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL"); ///< The license type -- this affects available functionality
MODULE_AUTHOR("Michal Kozikowski"); ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple Linux char driver"); ///< The description -- see modinfo
MODULE_VERSION("0.1"); ///< A version number to inform users

#define RPI4B_GPIO_ADDRESS_BASE 0xfe200000
#define GPIO_ON_REG_OFFSET   0x1c
#define GPIO_OFF_REG_OFFSET  0x28
#define GPIO_PIN			 21

static int majorNumber; ///< Stores the device number -- determined automatically
static const int minorNumberStart = 0; ///< Minor number of first device
static const int nr_devs = 4; ///< Number of bare devices

static struct mk_mob_dev *mk_mob_devices; ///< allocated in mk_mob_init_module
static struct class *mk_mob_class = NULL;

static unsigned int *gpio_registers = NULL;

// The prototype functions for the character driver -- must come before the struct definition
static int mk_mob_dev_open(struct inode *, struct file *);
static int mk_mob_dev_release(struct inode *, struct file *);
static ssize_t mk_mob_dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t mk_mob_dev_write(struct file *, const char *, size_t, loff_t *);
static loff_t mk_mob_dev_llseek(struct file *filp, loff_t off, int whence);
static long mk_mob_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);


static void gpio_pin_on(unsigned int pin)
{
	unsigned int fsel_bitpos = pin%10;

	unsigned int gpio_fsel = ioread32(gpio_registers + pin/10); // read proper GPIO function select register
	gpio_fsel &= ~(7 << (fsel_bitpos*3));
	gpio_fsel |= (1 << (fsel_bitpos*3));
	iowrite32(gpio_fsel, gpio_registers + pin/10); // write proper GPIO function select register

	unsigned int gpio_on_register = ioread32((unsigned int*)((char*)gpio_registers + GPIO_ON_REG_OFFSET)); // read GPIO ON register
	gpio_on_register |= (1 << pin);
	iowrite32(gpio_on_register, (unsigned int*)((char*)gpio_registers + GPIO_ON_REG_OFFSET)); // write GPIO ON register

	return;
}

static void gpio_pin_off(unsigned int pin)
{
	unsigned int gpio_on_register = ioread32((unsigned int*)((char*)gpio_registers + GPIO_OFF_REG_OFFSET)); // read GPIO OFF register
	gpio_on_register |= (1 << pin);
	iowrite32(gpio_on_register, (unsigned int*)((char*)gpio_registers + GPIO_OFF_REG_OFFSET)); // write GPIO OFF register

	return;
}

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = mk_mob_dev_open,
	.read = mk_mob_dev_read,
	.write = mk_mob_dev_write,
	.release = mk_mob_dev_release,
	.llseek = mk_mob_dev_llseek,
	.unlocked_ioctl = mk_mob_dev_ioctl,
};

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
static void mk_mob_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(majorNumber, minorNumberStart);

	/* Get rid of our char dev entries */
	if (mk_mob_devices) {
		for (i = 0; i < nr_devs; i++) {
			if (mk_mob_devices[i].data) {
				kfree(mk_mob_devices[i].data);
			}
			device_destroy(mk_mob_class,
				       mk_mob_devices[i].cdev.dev);
			cdev_del(&mk_mob_devices[i].cdev);
		}
		kfree(mk_mob_devices);
	}

	class_destroy(mk_mob_class);

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, nr_devs);

	iounmap(gpio_registers);
}

/*
 * Set up the char_dev structure for this device.
 */
static void mk_mob_setup_cdev(struct mk_mob_dev *dev, size_t index)
{
	struct device *mob_device = NULL;
	int err, devno = MKDEV(majorNumber, minorNumberStart + index);

	cdev_init(&dev->cdev, &fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &fops;

	dev->data = kmalloc(MK_DEV_MEMALLOC_MAX_SIZE, GFP_KERNEL);
	dev->size = 0;

	/* Check if we fail memory allocation for the device */
	if (dev->data == NULL) {
		return;
	}

	err = cdev_add(&dev->cdev, devno, 1);

	if (err) {
		printk(KERN_NOTICE "Error %d adding mk_mob device %zu", err,
		       index);
		return;
	}

	mob_device = device_create(mk_mob_class, NULL, devno, NULL, "%s_%zu",
				   DEVICE_NAME, index);

	if (IS_ERR(mob_device)) {
		pr_warn("failed to create device\n");
	}
}

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init mk_mob_init(void)
{
	int result, i;
	dev_t dev = 0;

	printk(KERN_INFO "mk_mob: Initializing the mk_mob module LKM.\n");

	result = alloc_chrdev_region(&dev, minorNumberStart, nr_devs, "mk_mob");
	majorNumber = MAJOR(dev);

	if (result < 0) {
		printk(KERN_WARNING
		       "mk_mob: Can't get major number for devices.\n");
		return result;
	}

	/* Allocate the devices. */
	mk_mob_devices =
		kmalloc(nr_devs * sizeof(struct mk_mob_dev), GFP_KERNEL);
	if (!mk_mob_devices) {
		result = -ENOMEM;
		goto fail;
	}

	memset(mk_mob_devices, 0, nr_devs * sizeof(struct mk_mob_dev));

	mk_mob_class = class_create(CLASS_NAME);
	if (IS_ERR(mk_mob_class)) {
		pr_warn("Failed to create class\n");
		result = PTR_ERR(mk_mob_class);
		goto fail;
	}

	/* Initialize each device. */
	for (i = 0; i < nr_devs; i++) {
		sema_init(&mk_mob_devices[i].sem, 1); // init as mutex
		mk_mob_setup_cdev(&mk_mob_devices[i], i);
	}

	gpio_registers = (int*)ioremap(RPI4B_GPIO_ADDRESS_BASE, PAGE_SIZE);
	if (gpio_registers == NULL)
	{
		printk("Failed to map GPIO memory to driver\n");
		result = -ENOMEM;
		goto fail;
	}

	return 0;

fail:
	mk_mob_cleanup_module();
	return result;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit mk_mob_exit(void)
{
	mk_mob_cleanup_module();
	printk(KERN_INFO "mk_mob: Goodbye from the LKM!\n");
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inode A pointer to an inode object (defined in linux/fs.h)
 *  @param filp A pointer to a file object (defined in linux/fs.h)
 */
static int mk_mob_dev_open(struct inode *inode, struct file *filp)
{
	struct mk_mob_dev *dev; /* device information */

	dev = container_of(inode->i_cdev, struct mk_mob_dev, cdev);
	filp->private_data = dev; /* for other methods */

	printk(KERN_INFO "mk_mob: device opened!\n");

	return 0; /* success */
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user.
 *  @param filp A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t mk_mob_dev_read(struct file *filp, char __user *buffer,
			       size_t len, loff_t *offset)
{
	ssize_t retval = 0;
	struct mk_mob_dev *dev = (struct mk_mob_dev *)filp->private_data;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	if (*offset >= dev->size)
		goto out;
	if (*offset + len > dev->size)
		len = dev->size - *offset;

	if (copy_to_user(buffer, dev->data + *offset, len)) {
		retval = -EFAULT;
		printk(KERN_INFO
		       "mk_mob: Failed to send %zu characters to the user\n",
		       len);
		goto out;
	}

	printk(KERN_INFO "mk_mob: Sent %zu characters to the user\n", len);
	*offset += len;
	retval = len;

out:
	up(&dev->sem);
	return retval;
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t mk_mob_dev_write(struct file *filp, const char __user *buffer,
				size_t len, loff_t *offset)
{
	struct mk_mob_dev *dev = (struct mk_mob_dev *)filp->private_data;
	ssize_t retval = -ENOMEM; /* value used in "goto out" statements */

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	if (*offset >= MK_DEV_MEMALLOC_MAX_SIZE)
		goto out;
	if (*offset + len > MK_DEV_MEMALLOC_MAX_SIZE)
		len = MK_DEV_MEMALLOC_MAX_SIZE - *offset;

	if (copy_from_user(dev->data + *offset, buffer, len)) {
		printk(KERN_INFO
		       "mk_mob: Failed to receive %zu characters to the user\n",
		       len);
		retval = -EFAULT;
		goto out;
	}

	printk(KERN_INFO "mk_mob: Received %zu characters from the user\n",
	       len);
	*offset += len;
	retval = len;
	dev->size = *offset;
out:
	up(&dev->sem);
	return retval;
}

static loff_t mk_mob_dev_llseek(struct file *filp, loff_t off, int whence)
{
	struct mk_mob_dev *dev = (struct mk_mob_dev *)filp->private_data;
	loff_t npos;

	pr_info("mk_mob_dev_llseek, whence = %d, off = %lld\n", whence, off);

	switch (whence) {
	case SEEK_SET:
		if (off >= MK_DEV_MEMALLOC_MAX_SIZE)
			return -EFBIG;
		npos = off;
		break;

	case SEEK_CUR:
		if (filp->f_pos + off >= MK_DEV_MEMALLOC_MAX_SIZE)
			return -EFBIG;
		npos = filp->f_pos + off;
		break;

	case SEEK_END:
		if (dev->size + off >= MK_DEV_MEMALLOC_MAX_SIZE)
			return -EFBIG;
		npos = dev->size + off;
		break;

	default: /* not expected value */
		return -EINVAL;
	}
	if (npos < 0)
		return -EINVAL;
	filp->f_pos = npos;
	return npos;
}

static long mk_mob_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct mk_mob_dev *dev = (struct mk_mob_dev *)filp->private_data;
	struct mk_mob_ioctl_data ioctl_data;

	int err = 0;
	int retval = 0;
    
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != MK_MOB_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > MK_MOB_IOC_MAXNR) return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE))
		err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	if (copy_from_user(&ioctl_data, (void __user *)arg, _IOC_SIZE(cmd))) {
		pr_err("copy from user failed!\n");
		return -EFAULT;
	}


	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	switch (cmd) {
	case MK_MOB_IOC_RESET:
		memset(dev->data, 0, dev->size);
		dev->size = 0;
		break;

	case MK_MOB_IOC_SET:
		if (ioctl_data.size + ioctl_data.off > MK_DEV_MEMALLOC_MAX_SIZE) {
			retval = -EFBIG;
		} else if (copy_from_user(dev->data + ioctl_data.off, ioctl_data.data,
					  ioctl_data.size)) {
			pr_err("copy from user failed!\n");
			retval = -EFAULT;
		} else if (ioctl_data.size + ioctl_data.off > dev->size) {
			dev->size = ioctl_data.size + ioctl_data.off;
		}
		break;

	case MK_MOB_IOC_GET:
		if (ioctl_data.size + ioctl_data.off > MK_DEV_MEMALLOC_MAX_SIZE) {
			retval = -EFBIG;
		} else if (copy_to_user(ioctl_data.data, dev->data + ioctl_data.off,
					ioctl_data.size)) {
			pr_err("copy to user failed!\n");
			retval = -EFAULT;
		}
		break;
	
	case MK_MOB_IOC_LED_ON:
		gpio_pin_on(GPIO_PIN);
		break;

	case MK_MOB_IOC_LED_OFF:
		gpio_pin_off(GPIO_PIN);
		break;

	default:
		retval = -ENOTTY;
	}

	up(&dev->sem);

	return retval;

}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int mk_mob_dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "mk_mob: Device successfully closed\n");
	return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(mk_mob_init);
module_exit(mk_mob_exit);
