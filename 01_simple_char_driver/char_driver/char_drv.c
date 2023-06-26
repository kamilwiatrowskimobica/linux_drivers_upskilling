#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>

/* Module info */ 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michał Aleksiński");
MODULE_DESCRIPTION("Char driver");
MODULE_VERSION("0.1");

/* Module parameters */
static char *drv_name = "mobica";
module_param(drv_name, charp, S_IRUGO);
MODULE_PARM_DESC(drv_name, "Character device name");
static char *drv_class = "mobica";
module_param(drv_class, charp, S_IRUGO);
MODULE_PARM_DESC(drv_class, "Character device class");


/* Driver state */
static int m_char_drv_major_number;
static struct class* m_char_drv_class = NULL;
static struct device* m_char_drv_device = NULL;

/* File operations */
static int char_drv_open(struct inode *inodep, struct file *filep);
static int char_drv_release(struct inode *inodep, struct file *filep);
static ssize_t char_drv_read(struct file *filep,
			     char *buffer,
			     size_t len,
			     loff_t *offset);
static ssize_t char_drv_write(struct file *filep,
			      const char *buffer,
			      size_t len,
			      loff_t *offset);

static struct file_operations fops = {
	.open = char_drv_open,
	.read = char_drv_read,
	.write = char_drv_write,
	.release = char_drv_release,
};

#define BUFFER_SIZE 16

static char data[BUFFER_SIZE];
static unsigned data_size = 0;
static unsigned read_offset = 0;
static unsigned write_offset = 0;

/**
 * @brief Module init
 */
static int __init char_drv_init(void) {
	printk(KERN_INFO "%s: driver init\n", drv_name);

	/* Allocate device major number */
	m_char_drv_major_number = register_chrdev(0, drv_name, &fops);
	if (m_char_drv_major_number < 0) {
		printk(KERN_ALERT
		       "%s: failed to register a major number\n", drv_name);
		return m_char_drv_major_number;
	}

	/* Register the device class */
	m_char_drv_class = class_create(THIS_MODULE, drv_class);
	if (IS_ERR(m_char_drv_class)) {
		unregister_chrdev(m_char_drv_major_number, drv_name);
      		printk(KERN_ALERT "char_drv: Failed to register device class\n");
      		return PTR_ERR(m_char_drv_class);
   	}
   	printk(KERN_INFO "%s: device class registered correctly\n", drv_name);

	/* Register the device driver */
	m_char_drv_device = device_create(m_char_drv_class, NULL,
					  MKDEV(m_char_drv_major_number, 0),
					  NULL, drv_name);
	if (IS_ERR(m_char_drv_device)) {
		// class_unregister(m_char_drv_class);
		class_destroy(m_char_drv_class);
		unregister_chrdev(m_char_drv_major_number, drv_name);
		printk(KERN_ALERT "%s: Failed to create the device\n", drv_name);
		return PTR_ERR(m_char_drv_device);
	}
	printk(KERN_INFO "%s: device created correctly\n", drv_name);

	return 0;
}

/**
 * @brief Module cleanup
 */
static void __exit char_drv_exit(void) {
	device_destroy(m_char_drv_class, MKDEV(m_char_drv_major_number, 0));
	// class_unregister(m_char_drv_class);
	class_destroy(m_char_drv_class);
	unregister_chrdev(m_char_drv_major_number, drv_name);

	printk(KERN_INFO "%s: driver exit\n", drv_name);
}


static int char_drv_open(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "%s: open\n", drv_name);

	return 0;
}

static int char_drv_release(struct inode *inodep, struct file *filep)
{
	data_size = 0;

	printk(KERN_INFO "%s: release\n", drv_name);

	return 0;
}

static ssize_t char_drv_read(struct file *filep,
			     char *buffer,
			     size_t len,
			     loff_t *offset)
{
	int result;
	ssize_t size;

	if (len == 0)
		return -EINVAL;

	if (data_size == 0) {
		printk(KERN_WARNING "%s: buffer is empty\n", drv_name);
		return 0;
	}

	if (read_offset >= write_offset)
		size = BUFFER_SIZE - read_offset;
	else
		size = data_size;
	if (size > len)
		size = len;

	result = copy_to_user(buffer, data, data_size);
	if (result != 0) {
		printk(KERN_INFO "%s: Failed to send %d characters to user\n",
		       drv_name, result);
		return -EFAULT;
	}

	printk(KERN_INFO "%s: Sent %d characters to the user\n",
		       drv_name, data_size);
	read_offset = (read_offset + size) % BUFFER_SIZE;
	data_size -= size;

	return size;
}

static ssize_t char_drv_write(struct file *filep,
			      const char *buffer,
			      size_t len,
			      loff_t *offset)
{
	int result;
	ssize_t size;

	if (BUFFER_SIZE - data_size == 0) {
		printk(KERN_WARNING "%s: buffer is full\n", drv_name);
		return -EAGAIN;
	}

	if (write_offset >= read_offset)
		size = BUFFER_SIZE - write_offset;
	else
		size = read_offset - write_offset;
	if (size > len - *offset)
		size = len;

	result = copy_from_user(data, buffer + write_offset, size);
	if (result != 0) {
		printk(KERN_INFO "%s: Failed to read %d characters from user\n",
		       drv_name, result);
		return -EFAULT;
	}
	write_offset = (write_offset + size) % BUFFER_SIZE;
	data_size += size;

	printk(KERN_INFO "%s: Received %zu characters from user\n",
		       drv_name, size);

	return size;
}

module_init(char_drv_init);
module_exit(char_drv_exit);