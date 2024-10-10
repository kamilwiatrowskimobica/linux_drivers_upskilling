#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/cdev.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>


#include "scull.h"
#include "proc_ops_version.h"    /* wrapper - to keep compatibility with older kernels */
#include "access_ok_version.h"   /* wrapper - to keep compatibility with older kernels */


static int scull_open(struct inode *inode, struct file *filp);
static int scull_release(struct inode *inode, struct file *filp);
static ssize_t scull_read(struct file *filp, char __user *buf, size_t devs_nb, loff_t *f_pos);
static ssize_t scull_write(struct file *filp, const char __user *buf, size_t devs_nb, loff_t *f_pos);
static loff_t scull_llseek(struct file *filp, loff_t off, int whence);
static long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

static int scull_setup_cdev(struct scull_dev *dev, int index);
static void scull_cleanup_module(void);

static int scull_trim(struct scull_dev *dev);
static struct scull_qset *scull_follow(struct scull_dev *dev, int n);

static struct scull_dev *scull_devices;
static struct class *dev_class;

/* PROC RELATED FEATURES ARE USED ONLY WHEN DEBUG IS ON (DEBUG=y) */
#ifdef SCULL_DEBUG
static void scull_create_proc(void);
static void scull_remove_proc(void);

static int scullmem_proc_open(struct inode *inode, struct file *file);
static int scullseq_proc_open(struct inode *inode, struct file *file);

static int scull_read_procmem(struct seq_file *s, void *v);
static void *scull_seq_start(struct seq_file *s, loff_t *pos);
static void *scull_seq_next(struct seq_file *s, void *v, loff_t *pos);
static void scull_seq_stop(struct seq_file *s, void *v);
static int scull_seq_show(struct seq_file *s, void *v);
#endif /* SCULL_DEBUG - PROC RELATED FEATURES ARE USED ONLY WHEN DEBUG IS ON (DEBUG=y) */


static int major = MAJOR_DEFAULT_NB;
static int minor_base = MINOR_BASE_DEFAUL_NB;
static int devs_nb = DEVS_NB_DEFAULT_VAL;
static int flag = FLAG_DEFAULT_VAL;

static int quantum_sz = SCULL_QUANTUM_DEFAULT_NB;
static int qset_sz =    SCULL_QSET_DEFULT_NB;

module_param(major, int, S_IRUGO);
module_param(minor_base, int, S_IRUGO);
module_param(devs_nb, int, S_IRUGO);
module_param(flag, int, S_IRUGO);
module_param(quantum_sz, int, S_IRUGO);
module_param(qset_sz, int, S_IRUGO);


/* scull device file operations */
static struct file_operations scull_fops =
{
   .owner = THIS_MODULE,
   .open = scull_open,
   .release = scull_release,
   .read = scull_read,
   .write = scull_write,
   .llseek = scull_llseek,
   .unlocked_ioctl = scull_ioctl
};


/* PROC RELATED FEATURES ARE USED ONLY WHEN DEBUG IS ON (DEBUG=y) */
#ifdef SCULL_DEBUG
/* proc filesystem single operations */
static struct file_operations scullmem_proc_ops =
{
	.owner   = THIS_MODULE,
	.open    = scullmem_proc_open,	/* implemented in scull driver, the rest of methods is canned */
	.read    = seq_read, 			/* coming from a set of canned seq file operations */
	.llseek  = seq_lseek, 			/* coming from a set of canned seq file operations */
	.release = single_release 		/* coming from a set of canned seq file operations */
};

/* proc filesystem sequence operations */
static struct file_operations scullseq_proc_ops =
{
	.owner   = THIS_MODULE,
	.open    = scullseq_proc_open,	/* implemented in scull driver, the rest of methods is canned */
	.read    = seq_read,			/* coming from a set of canned seq file operations */
	.llseek  = seq_lseek,			/* coming from a set of canned seq file operations */
	.release = seq_release			/* coming from a set of canned seq file operations */
};

/* proc filesystem sequence operations: start, next, stop, show */
static struct seq_operations scull_seq_ops =
{
	.start = scull_seq_start,
	.next  = scull_seq_next,
	.stop  = scull_seq_stop,
	.show  = scull_seq_show
};

/* proc filesystem - single access - function to read entry */
static int scull_read_procmem(struct seq_file *s, void *v)
{
	int i, j;
	int limit = s->size - 80; /* printing limit */

	for (i = 0; i < devs_nb && s->count <= limit; i++)
	{
		struct scull_dev *d = &scull_devices[i];
		struct scull_qset *qs = d->data;

		if(!mutex_trylock(&d->lock))
		{
			PDEBUG("Failed to procmem read. Busy.\n");
			return -EBUSY;
		}

		seq_printf(s,"\nDevice %i: qset %i, q %i, sz %li\n", i, d->qset, d->quantum, d->size);

		for (; qs && s->count <= limit; qs = qs->next)
		{ /* scan the list */
			seq_printf(s, "  item at %p, qset at %p\n", qs, qs->data);

			if (qs->data && !qs->next) /* dump only the last item */
			{
				for (j = 0; j < d->qset; j++)
				{
					if (qs->data[j])
					{
						seq_printf(s, "    % 4i: %8p\n", j, qs->data[j]);
					}
				}
			}
		}
		mutex_unlock(&d->lock);
	}
	return 0;
}

/*
 * Sequence iteration methods implementation.
 * "position" is simply the device number.
 */
static void *scull_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= devs_nb)
	{
		return NULL;   /* No more to read */
	}

	return scull_devices + *pos;
}

static void *scull_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;

	if (*pos >= devs_nb)
	{
		return NULL;
	}

	return scull_devices + *pos;
}

static void scull_seq_stop(struct seq_file *s, void *v)
{
	/* Nothing to do. */
}

static int scull_seq_show(struct seq_file *s, void *v)
{
	struct scull_dev *dev = (struct scull_dev *) v;
	struct scull_qset *d;
	int i;

	if(!mutex_trylock(&dev->lock))
	{
		PDEBUG("Failed seq_show. Busy\n");
		return -EBUSY;
	}

	seq_printf(s, "\nDevice %i: qset %i, q %i, sz %li\n", (int) (dev - scull_devices), dev->qset, dev->quantum, dev->size);

	for (d = dev->data; d; d = d->next)
	{ /* scan the list */
		seq_printf(s, "  item at %p, qset at %p\n", d, d->data);

		if (d->data && !d->next) /* dump only the last item */
		{
			for (i = 0; i < dev->qset; i++)
			{
				if (d->data[i]) seq_printf(s, "    % 4i: %8p\n", i, d->data[i]);
			}
		}
	}

	mutex_unlock(&dev->lock);

	return 0;
}

/* Single /proc file open method */
static int scullmem_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, scull_read_procmem, NULL);
}

/* Sequence /proc file open method */
static int scullseq_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &scull_seq_ops);
}

/* Creating proc's file */
static void scull_create_proc(void)
{
	proc_create_data("scullmem", 0 /* default mode */,
			NULL /* parent dir */, proc_ops_wrapper(&scullmem_proc_ops, scullmem_pops),
			NULL /* client data */);

	proc_create("scullseq", 0, NULL, proc_ops_wrapper(&scullseq_proc_ops, scullseq_pops));
}

/* Removing proc's file */
static void scull_remove_proc(void)
{
	/* no problem if it was not registered */
	remove_proc_entry("scullmem", NULL /* parent dir */);
	remove_proc_entry("scullseq", NULL);
}

#endif /* SCULL_DEBUG - PROC RELATED FEATURES ARE USED ONLY WHEN DEBUG IS ON (DEBUG=y) */

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
	  PDEBUG("0x%p\n", (void*)dptr);
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
   dev->quantum = quantum_sz;
   dev->qset = qset_sz;
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

   if(mutex_lock_interruptible(&dev->lock))
   {
	   PDEBUG("Device read failed. Busy\n");
	   return -ERESTARTSYS;
   }

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
   mutex_unlock(&dev->lock);
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

   if(mutex_lock_interruptible(&dev->lock))
   {
	   PDEBUG("Failed device write. Busy.\n");
	   return -ERESTARTSYS;
   }

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
   mutex_unlock(&dev->lock);
   return retval;
}

loff_t scull_llseek(struct file *filp, loff_t off, int whence)
{
   struct scull_dev *dev = filp->private_data;
   loff_t newpos;

   switch(whence)
   {
      case 0: /* SEEK_SET */
      {
         newpos = off;
         break;
      }

      case 1: /* SEEK_CUR */
      {
         newpos = filp->f_pos + off;
         break;
      }

      case 2: /* SEEK_END */
      {
         newpos = dev->size + off;
         break;
      }

      default: /* can't happen */
      {
         return -EINVAL;
      }
   }

   if(newpos < 0)
   {
      return -EINVAL;
   }

   filp->f_pos = newpos;

   return newpos;
}

long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
   int err = 0;
   int retval = 0;
   int tmp;

   /*
    * Extract the type and number bitfields, and don't decode
    * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
    */
   if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC)
   {
      PDEBUG("SCULL IOC MAGIC not correct.\n");
      return -ENOTTY;
   }
   else if (_IOC_NR(cmd) > SCULL_IOC_MAXNR)
   {
      PDEBUG("IOC CMD number not correct.\n");
      return -ENOTTY;
   }
   else
   {
      /* No error. Continue ioctl handling. */
   }

   /*
    * The direction is a bitmask, and VERIFY_WRITE catches R/W transfers.
    * `Type' is user-oriented, while access_ok is kernel-oriented,
    *  so the concept of "read" and "write" is reversed.
    */
   if (_IOC_DIR(cmd) & _IOC_READ)
   {
      PDEBUG("Access verify write failed.\n");
      err = !access_ok_wrapper(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
   }
   else if (_IOC_DIR(cmd) & _IOC_WRITE)
   {
      PDEBUG("Access verify read failed.\n");
      err =  !access_ok_wrapper(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
   }
   else
   {
      /* There is no error returned by access_ok check. Continue ioctl handling. */
   }

   /* If there is an error returned by acces_ok then exit with EFAULT. */
   if (err)
   {
      return -EFAULT;
   }

   switch(cmd)
   {
     case SCULL_IOCRESET:
     {
        quantum_sz = SCULL_QUANTUM_DEFAULT_NB;
        qset_sz = SCULL_QSET_DEFULT_NB;
        break;
     }

     case SCULL_IOCSQUANTUM: /* Set: arg points to the value */
     {
        if (!capable (CAP_SYS_ADMIN))
        {
           PDEBUG("CAP_SYS_ADMIN - not capable!.\n");
           return -EPERM;
        }
        else
        {
           retval = __get_user(quantum_sz, (int __user *)arg);
        }

        break;
     }

     case SCULL_IOCTQUANTUM: /* Tell: arg is the value */
     {
        if (!capable (CAP_SYS_ADMIN))
        {
           PDEBUG("CAP_SYS_ADMIN - not capable!.\n");
           return -EPERM;
        }
        else
        {
           quantum_sz = arg;
        }

        break;
     }

     case SCULL_IOCGQUANTUM: /* Get: arg is pointer to result */
     {
        retval = __put_user(quantum_sz, (int __user *)arg);
        break;
     }

     case SCULL_IOCQQUANTUM: /* Query: return it (it's positive) */
     {
        return quantum_sz;
     }

     case SCULL_IOCXQUANTUM: /* eXchange: use arg as pointer */
     {
        if (!capable (CAP_SYS_ADMIN))
        {
           PDEBUG("CAP_SYS_ADMIN - not capable!.\n");
           return -EPERM;
        }

        tmp = quantum_sz;
        retval = __get_user(quantum_sz, (int __user *)arg);

        if (retval == 0)
        {
           retval = __put_user(tmp, (int __user *)arg);
        }

        break;
     }

     case SCULL_IOCHQUANTUM: /* sHift: like Tell + Query */
     {
        if (!capable (CAP_SYS_ADMIN))
        {
           PDEBUG("CAP_SYS_ADMIN - not capable!.\n");
           return -EPERM;
        }

        tmp = quantum_sz;
        quantum_sz = arg;
        return tmp;
     }

     case SCULL_IOCSQSET:
     {
        if (!capable (CAP_SYS_ADMIN))
        {
           PDEBUG("CAP_SYS_ADMIN - not capable!.\n");
           return -EPERM;
        }

        retval = __get_user(quantum_sz, (int __user *)arg);

        break;
     }

     case SCULL_IOCTQSET:
     {
        if (!capable (CAP_SYS_ADMIN))
        {
           PDEBUG("CAP_SYS_ADMIN - not capable!.\n");
           return -EPERM;
        }

        qset_sz = arg;

        break;
     }

     case SCULL_IOCGQSET:
     {
        retval = __put_user(qset_sz, (int __user *)arg);
        break;
     }

     case SCULL_IOCQQSET:
     {
        return qset_sz;
     }

     case SCULL_IOCXQSET:
     {
        if (!capable (CAP_SYS_ADMIN))
        {
           PDEBUG("CAP_SYS_ADMIN - not capable!.\n");
           return -EPERM;
        }

        tmp = qset_sz;
        retval = __get_user(qset_sz, (int __user *)arg);

        if (retval == 0)
        {
           retval = put_user(tmp, (int __user *)arg);
        }

        break;
     }

     case SCULL_IOCHQSET:
     {
        if (!capable (CAP_SYS_ADMIN))
        {
           PDEBUG("CAP_SYS_ADMIN - not capable!.\n");
           return -EPERM;
        }

        tmp = qset_sz;
        qset_sz = arg;
        return tmp;
     }

     default:  /* redundant, as cmd was checked against MAXNR */
     {
        return -ENOTTY;
     }

   }

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
	  if(mutex_lock_interruptible(&dev->lock))
	  {
		  PDEBUG("Failed device open. Busy.\n");
		  return -ERESTARTSYS;
	  }

      printk(KERN_INFO "[scull] trimming from scull_open\n");
      scull_trim(dev); /* ignore errors */

      mutex_unlock(&dev->lock);
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
         mutex_destroy(&scull_devices[i].lock);
      }

      kfree(scull_devices);
      printk(KERN_INFO "[scull]: kfree for scull devices executed.\n");
   }

#ifdef SCULL_DEBUG /* use proc only if debugging */
	scull_remove_proc();
#endif

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
      dev_class = class_create(CLASS_NAME);

      if(IS_ERR(dev_class))
      {
         printk(KERN_ALERT "[scull]: Failed to create device class.\n");
         retVal = PTR_ERR(dev_class);
         goto fail;
      }
   }

   for(i = 0; i < devs_nb; i++)
   {
      scull_devices[i].quantum = quantum_sz;
      scull_devices[i].qset = qset_sz;
      mutex_init(&scull_devices[i].lock);

      retVal = scull_setup_cdev(&scull_devices[i], i);

      if(0 > retVal)
      {
         printk(KERN_ALERT "[scull]: Failed adding device scull%d.\n", i);
         goto fail;
      }
   }

   printk(KERN_INFO "[scull]: Dev init ended.\nCreated device with major: (%d), minor_base: (%d) and (%d) node(s)\n", major, minor_base, devs_nb);

#ifdef SCULL_DEBUG /* only when debugging */
	scull_create_proc();
#endif

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
