/**
 * @file   scull.c
 * @author Alicja Siemieniako-Maksimiuk
 * @date   15 March 2023
 * @version 0.1
 * @brief   An introductory character driver to support the second article of my series on
 * Linux loadable kernel module (LKM) development. This module maps to /dev/scull and
 * comes with a helper C program that can be run in Linux user space to communicate with
 * this the LKM.
 */

#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <linux/uaccess.h>          // Required for the copy to user function#include <linux/uaccess.h> // Required for the copy to user function

#include <linux/proc_fs.h> /* Necessary because we use proc fs */
#include <linux/seq_file.h> /* for seq_file */

#define  DEVICE_NAME "SCULL"    ///< The device will appear at /dev/scull using this value
#define  CLASS_NAME  "charDEV"        ///< The device class -- this is a character device driver
#define  NUMBER_DEVICES 2

MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Alicja Siemieniako-Maksimiuk");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple Linux driver for procFile usage");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users

static int    majorNumber;                  ///< Stores the device number -- determined automatically
static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class*  devCharClass  = NULL; ///< The device-driver class struct pointer
static struct device* devCharDevice = NULL; ///< The device-driver device struct pointer


// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static int     dev_owner(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);


char chr_arr [40]="Welcome to the procFile module";
static int len  =1;

// The prototype functions for the procfile -- must come before the struct definition
static int     proc_open(struct inode *inode, struct file *file);
static int     proc_release(struct inode *inode, struct file *file);
static int     proc_read(struct file *filp, char *__user*buffer, size_t length, loff_t *offset);
static int     proc_write(struct file * filp, const char * buff, size_t len, loff_t *off);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
//   .owner = dev_owner,
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
//   .llseek = dev_seek,
};



/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */

static const struct proc_ops proc_fops = {
  .proc_open = proc_open,
  .proc_read = seq_read,
  .proc_lseek = seq_lseek,
  .proc_release = single_release,
};


static int proc_open(struct inode *inode, struct file *file){
  printk(KERN_INFO "File procFile is open...\n");
  return 0;
}

static int proc_release(struct inode *inode, struct file *file){
  printk(KERN_INFO "File procFile is released...\n");
  return 0;

}
static int proc_read(struct file *filp, char *__user*buffer, size_t length, loff_t *offset){
  printk(KERN_INFO "File procFile is in read mode...\n");
  if (len)
	len = 0;
  else{
	len=1;
	 return 0;
  }

  copy_to_user(buffer,chr_arr,40);
  return length;

}
static int proc_write(struct file * filp, const char * buff, size_t len, loff_t *off){
  printk(KERN_INFO "File procFile is in write mode...\n");
  copy_from_user(chr_arr,buff,len);
  return len;

}






//TODO Function to print log to kernel info
/*static void print_log_from_device(void){
  printk(KERN_INFO "%s:",DEVICE_NAME);
}*/

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init dev_init(void){
   printk(KERN_INFO "%s: Initializing the EBBChar LKM\n",DEVICE_NAME);

   struct proc_dir_entry *entry;

   // Try to dynamically allocate a major number for the device -- more difficult but worth it

   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "%s failed to register a major number\n",DEVICE_NAME);
      return majorNumber;
   }
   printk(KERN_INFO "%s: registered correctly with major number %d\n", DEVICE_NAME,majorNumber);

   // Register the device class
   devCharClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(devCharClass)){                // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(devCharClass);          // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "%s: device class registered correctly\n",DEVICE_NAME);

   // Register the device driver
   devCharDevice = device_create(devCharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(devCharDevice)){               // Clean up if there is an error
      class_destroy(devCharClass);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(devCharDevice);
   }
   printk(KERN_INFO "%s: device class created correctly\n",DEVICE_NAME); // Made it! device was initialized
   return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit dev_exit(void){
   device_destroy(devCharClass, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(devCharClass);                          // unregister the device class
   class_destroy(devCharClass);                             // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
   printk(KERN_INFO "%s: Goodbye from the LKM!\n",DEVICE_NAME);
}


/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_owner(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "%s: Device owner is:%s\n",DEVICE_NAME,THIS_MODULE);
   return 0;
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
   numberOpens++;
   printk(KERN_INFO "%s: Device has been opened %d time(s)\n", DEVICE_NAME, numberOpens);
   return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count = 0;
   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
   error_count = copy_to_user(buffer, message, size_of_message);

   if (error_count==0){            // if true then have success
      printk(KERN_INFO "%s: Sent %d characters to the user\n",DEVICE_NAME, size_of_message);
      return (size_of_message=0);  // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "%s: Failed to send %d characters to the user\n",DEVICE_NAME, error_count);
      return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
   }
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
   sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
   size_of_message = strlen(message);                 // store the length of the stored message
   printk(KERN_INFO "%s: Received %zu characters from the user\n",DEVICE_NAME, len);
   return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "%s: Device successfully closed\n",DEVICE_NAME);
   return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(dev_init);
module_exit(dev_exit);
