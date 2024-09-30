#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MASJ");
MODULE_DESCRIPTION("Simple char device");
MODULE_VERSION("0.1");

#define DEVICE_NAME        "CharDevEx"
#define DEVICE_CLASS_NAME  "masj"

static DEFINE_MUTEX(charDevEx_mutex);

static int numberOpens = 0;
static short sizeOfMessage;
static char message[256] = {0};

static int majorNumber;
static struct class* devClass = NULL;
static struct device* devCharDevEx = NULL;

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations f_ops =
{
      .open =    dev_open,
      .read =    dev_read,
      .write =   dev_write,
      .release = dev_release,
};

static int __init dev_init(void)
{
   printk(KERN_INFO "CharDevEx: Initializing LKM\n");

   majorNumber = register_chrdev(0, DEVICE_NAME, &f_ops);
   if(0 > majorNumber)
   {
      printk(KERN_ALERT "CharDevEx: failed to register major number\n");
      return majorNumber;
   }

   printk(KERN_INFO "CharDevEx registered correctly with major number %d\n", majorNumber);

   devClass = class_create(DEVICE_CLASS_NAME);
   if(IS_ERR(devClass))
   {
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "CharDevEx: failed to register device class\n");
      return PTR_ERR(devClass);
   }

   devCharDevEx = device_create(devClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if(IS_ERR(devCharDevEx))
   {
      class_destroy(devClass);
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "CharDevEx: Failed to create the device\n");
      return PTR_ERR(devCharDevEx);
   }

   printk(KERN_INFO "CharDevEx: device created correctly\n");

   mutex_init(&charDevEx_mutex);

   return 0;
}

static void __exit dev_exit(void)
{
   mutex_destroy(&charDevEx_mutex);
   device_destroy(devClass, MKDEV(majorNumber, 0));
   class_unregister(devClass);
   class_destroy(devClass);
   unregister_chrdev(majorNumber, DEVICE_NAME);
   printk(KERN_INFO "CharDevEx: Deinitialized LKM\n");
}

static int dev_open(struct inode *inodep, struct file *filep)
{
   if(!mutex_trylock(&charDevEx_mutex))
   {
      printk(KERN_ALERT "CharDevEx: Device in use by another process");
      return -EBUSY;
   }

   numberOpens++;
   printk(KERN_INFO "CharDevEx: Device has been opened %d time(s)\n", numberOpens);
   return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
   int errorCnt = 0;

   errorCnt = copy_to_user(buffer, message, sizeOfMessage);

   if (0 == errorCnt)
   {
      printk(KERN_INFO "CharDevEx: Sent %d characters to the user\n", sizeOfMessage);
      return (sizeOfMessage = 0);
   }
   else
   {
      printk(KERN_INFO "CharDevEx: Failed to send %d characters to the user\n", errorCnt);
      return -EFAULT;
   }
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
   sprintf(message, "%s(%zu letters)", buffer, len);
   sizeOfMessage = strlen(message);
   printk(KERN_INFO "CharDevEx: Received %zu characters from the user\n", len);
   return len;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
   mutex_unlock(&charDevEx_mutex);

   printk(KERN_INFO "CharDevEx: Device successfully closed\n");
   return 0;
}

module_init(dev_init);
module_exit(dev_exit);
