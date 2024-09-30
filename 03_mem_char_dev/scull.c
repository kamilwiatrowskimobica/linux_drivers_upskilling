#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/cdev.h>

#define DEVICE_NAME "scull"

#define SCULL_QUANTUM 4000
#define SCULL_QSET    1000

struct scull_qset
{
   void **data;
   struct scull_qset *next;
};

struct scull_dev
{
   struct scull_qset *data;   /* pointer to first quantum set */
   int quantum;               /* the current quantum size */
   int qset;                  /* the current array size */
   unsigned long size;        /* amount of data stored here */
   unsigned int access_key;   /* used by sculluid and scullpriv */
   struct semaphore sem;      /* mutual exclusion semaphore */
   struct cdev cdev;          /* char device structure */
};

static int scull_open(struct inode *inode, struct file *filp);
static int scull_release(struct inode *inode, struct file *filp);
static ssize_t scull_read(struct file *filp, char __user *buf, size_t devs_nb, loff_t *f_pos);
static ssize_t scull_write(struct file *filp, const char __user *buf, size_t devs_nb, loff_t *f_pos);

static int scull_trim(struct scull_dev *dev);
static struct scull_qset *scull_follow(struct scull_dev *dev, int n);
static int scull_setup_cdev(struct scull_dev *dev, int index);
static void scull_cleanup_module(void);

static struct file_operations scull_fops =
{
   .owner = THIS_MODULE,
   .open = scull_open,
   .release = scull_release,
   .read = scull_read,
   .write = scull_write,
};

static int major = 0;
static int minor_base = 0;
static int devs_nb = 4;
static int flag = 0;

static int scull_quantum = SCULL_QUANTUM;
static int scull_qset =    SCULL_QSET;

static struct scull_dev *scull_devices;
static struct class *dev_class;

module_param(major, int, S_IRUGO);
module_param(minor_base, int, S_IRUGO);
module_param(devs_nb, int, S_IRUGO);
module_param(flag, int, S_IRUGO);

/* follow the list */
static struct scull_qset *scull_follow(struct scull_dev *dev, int n)
{
   struct scull_qset *qs = dev->data;

   /* Allocate first qset explicitly if need be */
   if (!qs)
   {
      printk(KERN_INFO "[scull] scull_follow: allocating first qset with kmalloc\n");
      qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);

      if (qs == NULL)
      {
         printk(KERN_ALERT "[scull] scull_follow: failed to allocate first qset with kmalloc\n");
         return NULL;  /* Never mind */
      }

      memset(qs, 0, sizeof(struct scull_qset));
   }

   /* Then follow the list */
   while (n--)
   {
      if (!qs->next)
      {
         printk(KERN_INFO "[scull] scull_follow: allocating next qset with kmalloc\n");
         qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);

         if (qs->next == NULL)
         {
            printk(KERN_ALERT "[scull] scull_follow: failed to allocate next qset with kmalloc\n");
            return NULL;  /* Never mind */
         }

         memset(qs->next, 0, sizeof(struct scull_qset));
      }

      qs = qs->next;
      continue;
   }

   return qs;
}

static int scull_trim(struct scull_dev *dev)
{
   struct scull_qset *next, *dptr;
   int qset = dev->qset; /* dev is not null */
   int i;

   for(dptr = dev->data; dptr; dptr = next)
   {
      if(dptr->data)
      {
         for (i = 0; i < qset; i++)
         {
            kfree(dptr->data[i]);
         }

         kfree(dptr->data);
         dptr->data = NULL;
      }
      next = dptr->next;
      kfree(dptr);
   }

   dev->size = 0;
   dev->quantum = scull_quantum;
   dev->qset = scull_qset;
   dev->data = NULL;

   return 0;
}

static ssize_t scull_read(struct file *filp, char __user *buf, size_t devs_nb, loff_t *f_pos)
{
   struct scull_dev *dev = filp->private_data;
   struct scull_qset *dptr;	/* the first listitem */
   int quantum = dev->quantum, qset = dev->qset;
   int itemsize = quantum * qset; /* how many bytes in the listitem */
   int item, s_pos, q_pos, rest;
   ssize_t retval = 0;

   if (*f_pos >= dev->size)
   {
      goto out;
   }

   if (*f_pos + devs_nb > dev->size)
   {
      devs_nb = dev->size - *f_pos;
   }

   /* find listitem, qset index, and offset in the quantum */
   item = (long)*f_pos / itemsize;
   rest = (long)*f_pos % itemsize;
   s_pos = rest / quantum; q_pos = rest % quantum;

   /* follow the list up to the right position (defined elsewhere) */
   dptr = scull_follow(dev, item);

   if (dptr == NULL || !dptr->data || ! dptr->data[s_pos])
   {
      goto out; /* don't fill holes */
   }

   /* read only up to the end of this quantum */
   if (devs_nb > quantum - q_pos)
   {
      devs_nb = quantum - q_pos;
   }

   if (copy_to_user(buf, dptr->data[s_pos] + q_pos, devs_nb))
   {
      retval = -EFAULT;
      goto out;
   }

   *f_pos += devs_nb;
   retval = devs_nb;

out:
   return retval;
}

static ssize_t scull_write(struct file *filp, const char __user *buf, size_t devs_nb, loff_t *f_pos)
{
   struct scull_dev *dev = filp->private_data;
   struct scull_qset *dptr;
   int quantum = dev->quantum, qset = dev->qset;
   int itemsize = quantum * qset;
   int item, s_pos, q_pos, rest;
   ssize_t retval = -ENOMEM; /* value used in "goto out" statements */

   /* find listitem, qset index and offset in the quantum */
   item = (long)*f_pos / itemsize;
   rest = (long)*f_pos % itemsize;
   s_pos = rest / quantum; q_pos = rest % quantum;

   /* follow the list up to the right position */
   dptr = scull_follow(dev, item);
   if (dptr == NULL)
   {
      goto out;
   }

   if (!dptr->data)
   {
      dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);

      if (!dptr->data)
      {
         goto out;
      }

      memset(dptr->data, 0, qset * sizeof(char *));
   }

   if (!dptr->data[s_pos])
   {
      dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);

      if (!dptr->data[s_pos])
      {
         goto out;
      }
   }

   /* write only up to the end of this quantum */
   if (devs_nb > quantum - q_pos)
   {
      devs_nb = quantum - q_pos;
   }

   if (copy_from_user(dptr->data[s_pos] + q_pos, buf, devs_nb))
   {
      retval = -EFAULT;
      goto out;
   }

   *f_pos += devs_nb;
   retval = devs_nb;

   /* update the size */
   if (dev->size < *f_pos)
   {
      dev->size = *f_pos;
   }

out:
   return retval;
}

static int scull_open(struct inode *inode, struct file *filp)
{
   struct scull_dev *dev; /* device information */

   printk(KERN_INFO "[scull] performing 'open' operation\n");

   dev = container_of(inode->i_cdev, struct scull_dev, cdev);
   filp->private_data = dev; /* for other methods */

   /* now trim to 0 the length of the device if open was write-only */
   if( (filp->f_flags & O_ACCMODE) == O_WRONLY)
   {
      printk(KERN_INFO "[scull] trimming from scull_open\n");
      scull_trim(dev); /* ignore errors */
   }

   return 0;
}

static int scull_release(struct inode *inode, struct file *filp)
{
   printk(KERN_INFO "[scull] performing 'release' operation\n");

   return 0;
}

static int scull_setup_cdev(struct scull_dev *dev, int index)
{
   int retVal;
   struct device *scull_device = NULL;
   dev_t devno = MKDEV(major, minor_base + index);

   cdev_init(&dev->cdev, &scull_fops);
   dev->cdev.owner = THIS_MODULE;
   dev->cdev.ops = &scull_fops; /* cdev.ops is set inside cdev_init(), there is no need to repeat that */
   retVal = cdev_add(&dev->cdev, devno, 1);

   if(0 > retVal)
   {
      printk(KERN_ALERT "[scull]: Error %d adding scull%d", retVal, index);
      return retVal;
   }

   if(1 == flag)
   {
      scull_device = device_create(dev_class, NULL, devno, NULL, "%s%d", DEVICE_NAME, index);

      if(IS_ERR(scull_device))
      {
         printk(KERN_ALERT "[scull]: Failed to created node scull%d\n", index);
         return PTR_ERR(scull_device);
      }

      printk(KERN_INFO "[scull]: Node scull%d created", index);
   }

   printk(KERN_INFO "[scull] cdev initialized\n");

   return 0;
}

static void scull_cleanup_module(void)
{
   int i;
   dev_t devno = MKDEV(major, minor_base);

   if((1 == flag) && dev_class)
   {
      for(i = 0; i < devs_nb; i++)
      {
         device_destroy(dev_class, devno + i);
         printk(KERN_INFO "[scull]: device%d destroyed.\n", i);
      }

      class_destroy(dev_class);
      printk(KERN_INFO "[scull]: device class destroyed\n");
   }

   if(scull_devices)
   {
      for(i = 0; i < devs_nb; i++)
      {
         scull_trim(scull_devices + i);
         cdev_del(&(scull_devices[i].cdev));
         printk(KERN_INFO "[scull]: cdev%d deleted.\n", i);
      }

      kfree(scull_devices);
      printk(KERN_INFO "[scull]: kfree for scull devices executed.\n");
   }

   unregister_chrdev_region(devno, devs_nb);
   printk(KERN_INFO "[scull]: chrdev region unregistred.\n");

   printk(KERN_INFO "[scull]: --------- Cleanup completed ---------\n");
}

static int __init scull_module_init(void)
{
   int retVal, i;
   dev_t devno = 0;

   printk(KERN_INFO "[scull]: Dev init started with major: (%d) and minor_base: (%d)\n", major, minor_base);

   /* If major is different than 0 (major requested by module's param) perfomr static allocation. */
   if(major)
   {
      printk(KERN_INFO "[scull]: Static allocation of major number (%d)\n", major);
      devno = MKDEV(major, minor_base);
      retVal = register_chrdev_region(devno, devs_nb, DEVICE_NAME);
   }
   /* If major is 0 (default value) perform dynamic allocation. */
   else
   {
      printk(KERN_INFO "[scull]: Dynamic allocation of major number\n");
      retVal = alloc_chrdev_region(&devno, minor_base, devs_nb, DEVICE_NAME);
      major = MAJOR(devno);
      minor_base = MINOR(devno);
   }

   if(0 > retVal)
   {
      printk(KERN_ALERT "[scull]: Registration of the chardev region failed\n");
      return retVal;
   }

   scull_devices = kmalloc(devs_nb * sizeof(struct scull_dev), GFP_KERNEL);
   if(!scull_devices)
   {
      retVal = -ENOMEM;
      printk(KERN_ALERT "[scull]: ERROR kmalloc\n");
      goto fail;
   }
   else
   {
      printk(KERN_INFO "[scull]: kmalloc executed with no error\n");
   }

   memset(scull_devices, 0, devs_nb * sizeof(struct scull_dev));

   if(1 == flag)
   {
      dev_class = class_create("scull_class");

      if(IS_ERR(dev_class))
      {
         printk(KERN_ALERT "[scull]: Failed to create device class.\n");
         retVal = PTR_ERR(dev_class);
         goto fail;
      }
   }

   for(i = 0; i < devs_nb; i++)
   {
      scull_devices[i].quantum = scull_quantum;
      scull_devices[i].qset = scull_qset;

      retVal = scull_setup_cdev(&scull_devices[i], i);

      if(0 > retVal)
      {
         printk(KERN_ALERT "[scull]: Failed adding device scull%d.\n", i);
         goto fail;
      }
   }

   printk(KERN_INFO "[scull]: Dev init ended.\nCreated device with major: (%d), minor_base: (%d) and (%d) node(s)\n", major, minor_base, devs_nb);

   return 0;

fail:
   scull_cleanup_module();
   return retVal;
}

static void __exit scull_module_exit(void)
{
   printk(KERN_INFO "[scull] ----- Calling cleanup function on module unload -----\n");
   scull_cleanup_module();
}

module_init(scull_module_init);
module_exit(scull_module_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("MS");
MODULE_DESCRIPTION("LDD3 book - scull driver exercise - chapter 03.");
MODULE_VERSION("0.1");
