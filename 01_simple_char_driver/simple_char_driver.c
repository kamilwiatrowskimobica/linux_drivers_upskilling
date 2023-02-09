/**
 * @file    main.c
 * @author  Drost Krzysztof
 * @date    8 February 2023
 * @version 0.1
 * @brief   Simple character driver with support for read and write to device.
 *          Tested on linux version: 5.15.0-58-generic
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mutex.h>

#define PRINT_PREFIX "MobChar:"         ///< Prefix shown before eache print
#define DEVICE_NAME "mobchar"           ///< The device will appear at /dev/ using this value
#define CLASS_NAME  "mob"               ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Drost Krzysztof");
MODULE_DESCRIPTION("Simple character driver with support for read and write to device.");
MODULE_VERSION("0.1");

static DEFINE_MUTEX(mobchar_mutex);

static int    deviceNumber;                     ///< Stores the device number -- determined automatically
static char   message[256] = {0};               ///< Memory for the string that is passed from userspace
static short  meseageSize;                      ///< Used to remember the size of the string stored
static int    opensCounts = 0;                  ///< Counts the number of times the device is opened
static struct class*  mobcharClass  = NULL;     ///< The device-driver class struct pointer
static struct device* mobcharDevice = NULL;     ///< The device-driver device struct pointer

static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

/** @brief The LKM initialization function
 *  @return returns 0 if successful
 */
static int __init char_write_read_init(void)
{
        printk(KERN_INFO "%s Init function start.\n", PRINT_PREFIX);

        mutex_init(&mobchar_mutex);
        // get device number
        deviceNumber = register_chrdev(0, DEVICE_NAME, &fops);
        if (deviceNumber < 0) {
                printk(KERN_ALERT "%s failed to register a device number for %s\n", PRINT_PREFIX, DEVICE_NAME);
                return deviceNumber;
        }
        printk(KERN_INFO "%s Device %s registered with device number %d\n", PRINT_PREFIX, DEVICE_NAME, deviceNumber);

        // Register the device class
        mobcharClass = class_create(THIS_MODULE, CLASS_NAME);
        // Check for error and clean up if there is
        if (IS_ERR(mobcharClass)) {
                unregister_chrdev(deviceNumber, DEVICE_NAME);
                printk(KERN_ALERT "%s Failed to register device class\n", PRINT_PREFIX);
                return PTR_ERR(mobcharClass);
        }
        printk(KERN_INFO "%s Device class registered correctly\n", PRINT_PREFIX);
        
        // Register the device driver
        mobcharDevice = device_create(mobcharClass, NULL, MKDEV(deviceNumber, 0), NULL, DEVICE_NAME);
        // Check for error and clean up if there is
        if (IS_ERR(mobcharDevice)) {
                class_destroy(mobcharClass);
                unregister_chrdev(deviceNumber, DEVICE_NAME);
                printk(KERN_ALERT "%s Failed to create the device\n", PRINT_PREFIX);
                return PTR_ERR(mobcharDevice);
        }
        printk(KERN_INFO "%s Device class created correctly\n", PRINT_PREFIX);
        return 0;
}

/** @brief The LKM cleanup function
 */
static void __exit char_write_read_exit(void)
{
        printk(KERN_INFO "%s Exit function start.\n", PRINT_PREFIX);
        mutex_destroy(&mobchar_mutex);
        device_destroy(mobcharClass, MKDEV(deviceNumber, 0));           // remove the device
        class_destroy(mobcharClass);                                    // remove the device class
        unregister_chrdev(deviceNumber, DEVICE_NAME);                   // unregister the major number
        printk(KERN_INFO "%s Exit function end.\n", PRINT_PREFIX);
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the opensCounts counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep)
{
        if (!mutex_trylock(&mobchar_mutex)) {
                printk(KERN_ALERT "%s Device in use by another process", PRINT_PREFIX);
                return -EBUSY;
        }
        opensCounts++;
        printk(KERN_INFO "%s Device has been opened %d time(s)\n", PRINT_PREFIX, opensCounts);
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
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
        int error_count = 0;
        // copy_to_user has the format ( * to, *from, size) and returns 0 on success
        error_count = copy_to_user(buffer, message, meseageSize);
        
        if (error_count == 0) {
                printk(KERN_INFO "%s Sent %d characters to the user\n", PRINT_PREFIX, meseageSize);
                return (meseageSize = 0);
        }
        else {
                printk(KERN_INFO "%s Failed to send %d characters to the user\n", PRINT_PREFIX, error_count);
                return -EFAULT;
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
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
        int error_count = 0;
        error_count = copy_from_user(message, buffer, len);

        if (error_count == 0) {
                meseageSize = strlen(message);
                printk(KERN_INFO "%s Received %zu characters from the user\n", PRINT_PREFIX, len);
                return len;
        }
        else {
                printk(KERN_INFO "%s Failed to receive characters from the user\n", PRINT_PREFIX);
                return -EFAULT;
        }
        return len;
}
 
/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep)
{
        mutex_unlock(&mobchar_mutex);
        printk(KERN_INFO "%s Device successfully closed\n", PRINT_PREFIX);
        return 0;
}

module_init(char_write_read_init);
module_exit(char_write_read_exit);
