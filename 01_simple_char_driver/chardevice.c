#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <linux/uaccess.h>          // Required for the copy to user function
#define  DEVICE_NAME "chardevmob"    ///< The device will appear at /dev/ebbchar using this value
#define  CLASS_NAME  "chardevclass"        ///< The device class -- this is a character device driver
 
 
MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Rafal Cieslak");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple Linux char driver");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users

static int    majorNumber;                  ///< Stores the device number -- determined automatically
static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class*  charDevClass  = NULL; ///< The device-driver class struct pointer
static struct device* charDevice = NULL; ///< The device-driver device struct pointer

static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};
static DEFINE_MUTEX(char_mutex);

static int __init char_init(void){
   printk(KERN_INFO "Initializing the device LKM\n");
 
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "registered correctly with major number %d\n", majorNumber);
 
   // Register the device class
   charDevClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(charDevClass)){                
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(charDevClass);          
   }
   printk(KERN_INFO "device class registered correctly\n");
 
   // Register the device driver
   charDevice = device_create(charDevClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(charDevice)){              
      class_destroy(charDevClass);           
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(charDevice);
   }
   printk(KERN_INFO "device class created correctly\n"); // Made it! device was initialized
   mutex_init(&char_mutex); 
   return 0;
}

static void __exit char_exit(void){
    mutex_destroy(&char_mutex); 
   device_destroy(charDevClass, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(charDevClass);                          // unregister the device class
   class_destroy(charDevClass);                             // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
   printk(KERN_INFO "Goodbye from the LKM!\n");
}

static int dev_open(struct inode *inodep, struct file *filep){

    if(!mutex_trylock(&char_mutex)){
        printk(KERN_ALERT "Device in use by another process");
        return -EBUSY;
    }
   numberOpens++;
   printk(KERN_INFO "Device has been opened %d time(s)\n", numberOpens);
   return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count = 0;

   error_count = copy_to_user(buffer, message, size_of_message);
 
   if (error_count==0){            
      printk(KERN_INFO "Sent %d characters to the user\n", size_of_message);
      return (size_of_message=0);  
   }
   else {
      printk(KERN_INFO "Failed to send %d characters to the user\n", error_count);
      return -EFAULT;              
   }
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
   sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
   size_of_message = strlen(message);                 // store the length of the stored message
   printk(KERN_INFO "Received %zu characters from the user\n", len);
   return len;
}

static int dev_release(struct inode *inodep, struct file *filep){
    mutex_unlock(&char_mutex); 
    printk(KERN_INFO "Device successfully closed\n");
    return 0;
}
 
module_init(char_init);
module_exit(char_exit);