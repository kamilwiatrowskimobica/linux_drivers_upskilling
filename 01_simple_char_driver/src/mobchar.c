#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <linux/uaccess.h>          // Required for the copy to user function
#define  DEVICE_NAME "mobchar"    ///< The device will appear at /dev/mobchar using this value
#define  CLASS_NAME  "mob"        ///< The device class -- this is a character device driver
#define  MSG_LEN     (256u)

MODULE_LICENSE("GPL");           
MODULE_AUTHOR("AKP");    
MODULE_DESCRIPTION("Simple Linux char driver");  
MODULE_VERSION("0.1");          



static int maj_number;
static char msg[MSG_LEN] = {0};
static short size_of_msg;
static int open_nums = 0;
static struct class* mobcharclass = NULL;
static struct device* mobchardevice = NULL;

// Prototypes
static int dev_open(struct inode*, struct file*);
static int dev_release(struct inode*, struct file*);
static ssize_t dev_read(struct file*, char*, size_t, loff_t*);
static ssize_t dev_write(struct file*, const char*, size_t, loff_t*);

static struct file_operations fops = 
{
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};


static int __init mobchar_init(void)
{
        printk(KERN_INFO "Mobchar: Init the Mobchar LKM\n");

        maj_number = register_chrdev(0, DEVICE_NAME, &fops);
        if (maj_number < 0){
                printk(KERN_ALERT "Mobchar failed to register a major number\n");
                return maj_number;
        }

        printk(KERN_INFO "Mobchar: registered correctly with major number %d\n", maj_number);

        // Reg class
        mobcharclass = class_create(THIS_MODULE, CLASS_NAME);
        if (IS_ERR(mobcharclass)){
                unregister_chrdev(maj_number, DEVICE_NAME);
                printk(KERN_ALERT "Failed to register device class\n");
                return PTR_ERR(mobcharclass);
        }
        printk(KERN_INFO "Mobchar: device class registered correctly\n");

        // Reg dev
        mobchardevice = device_create(mobcharclass, NULL, MKDEV(maj_number, 0), NULL, DEVICE_NAME);
        if (IS_ERR(mobchardevice)){               
                class_destroy(mobcharclass);           
                unregister_chrdev(maj_number, DEVICE_NAME);
                printk(KERN_ALERT "Failed to create the device\n");
                return PTR_ERR(mobchardevice);
        }
        printk(KERN_INFO "Mobchar: device class created correctly\n"); 
        return 0;
}

static void __exit mobchar_exit(void){
        device_destroy(mobcharclass, MKDEV(maj_number, 0));     
        //class_unregister(ebbcharClass);                         
        class_destroy(mobcharclass);                           
        unregister_chrdev(maj_number, DEVICE_NAME);            
        printk(KERN_INFO "Mobchar: Goodbye from the LKM!\n");
}

static int dev_open(struct inode *inodep, struct file *filep){
        open_nums++;
        printk(KERN_INFO "Mobchar: Device has been opened %d time(s)\n", open_nums);
        return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
        int error_count = 0;
        // copy_to_user has the format ( * to, *from, size) and returns 0 on success
        error_count = copy_to_user(buffer, msg, size_of_msg);

        // Success
        if (0 == error_count){
            printk(KERN_INFO "Mobchar: Sent %d characters to the user\n", size_of_msg);
            return (size_of_msg = 0); 
        } else {
            printk(KERN_INFO "Mobchar: Failed to send %d characters to the user\n", error_count);
            // Failed -- return a bad address message (i.e. -14)
            return -EFAULT;
        }
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
        //sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
        copy_from_user(msg, buffer, len);
        //snprintf(msg+size_of_msg, len, buffer);
        size_of_msg = strlen(msg);                 // store the length of the stored message
        printk(KERN_INFO "Mobchar: Received %zu characters from the user\n", len);
        return len;
}

static int dev_release(struct inode *inodep, struct file *filep){
        printk(KERN_INFO "Mobchar: Device successfully closed\n");
        return 0;
}


module_init(mobchar_init);
module_exit(mobchar_exit);