#include <linux/init.h>          // Macros used to mark up functions e.g. __init __exit
#include <linux/kernel.h>        // printk
#include <linux/module.h>
#include <linux/fs.h>            // register_chrdev_region
#include <linux/device.h>        // class_create
#include <linux/cdev.h>
#include <linux/rmap.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rafal Zielinski");
MODULE_DESCRIPTION("mmap char driver");
MODULE_VERSION("0.1");

struct char_dev {
	void* kmalloc_ptr;
	char* kmalloc_area;
	struct cdev cdev;
};

#define CLASS_NAME  "rz_class"
#define DEVICE_NAME "rzechar"
#define NPAGES 16
static struct class*  rzecharClass  = NULL;
static struct device* rzecharDevice = NULL;
static struct char_dev* my_dev = NULL;

static int dev_mmap(struct file* file, struct vm_area_struct* vm);
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static int rzechar_major = 0;

static struct file_operations fops =
{
        .owner = THIS_MODULE,
        .mmap = dev_mmap,
        .open = dev_open,
        .read = dev_read,
        .write = dev_write,
        .release = dev_release,
};

static int dev_mmap(struct file* filp, struct vm_area_struct* vma)
{
	int ret;
	long length = vma->vm_end - vma->vm_start;
	struct char_dev *dev = filp->private_data;
	printk(KERN_INFO "rzechar: mapping memory\n" );

	if (!dev){
		printk(KERN_WARNING "rzechar: no device data \n");
		return -EFAULT;
	}

	printk("rzechar: MMap length = %lu, vm_start = %lx, vm_end = %lx", length, vma->vm_start, vma->vm_end);
	printk("rzechar: kmalloc_area = %p PAGE_SHIFT = %d, physical address = %llu",
		dev->kmalloc_area, PAGE_SHIFT, virt_to_phys((void*) dev->kmalloc_area));

	if (length > NPAGES * PAGE_SIZE)	return -EIO;

	ret = remap_pfn_range(vma, vma->vm_start,
		    virt_to_phys((void*) dev->kmalloc_area) >> PAGE_SHIFT,
		    length, vma->vm_page_prot);

	if (ret < 0){
		printk(KERN_INFO "CharDriver: Mapping failed\n");
		return ret;
	}

    return 0;
}

static int dev_open(struct inode *inodep, struct file *filep){

	struct char_dev *dev;
	printk(KERN_INFO "rzechar: opening Device\n" );
        dev = container_of(inodep->i_cdev, struct char_dev, cdev);
        filep->private_data = dev;
	

        printk(KERN_INFO "rzechar: Device has been opened\n" );
        return 0;
}

static int dev_release(struct inode *inodep, struct file *filep){

        printk(KERN_INFO "rzechar:: Device successfully closed\n");
        return 0;
}

ssize_t dev_read(struct file *filp, char __user *buf, size_t size, loff_t *f_pos)
{
        struct char_dev *dev = filp->private_data;
        printk(KERN_INFO "rzechar: dev_read count: %zu f_pos: %lld \n", size, *f_pos);
        
        if (!dev) {
                printk(KERN_WARNING "rzechar: no device data \n");
                return -EFAULT;
        }

        if (size > NPAGES * PAGE_SIZE)	size = NPAGES * PAGE_SIZE;
    
    	if (copy_to_user(buf, dev->kmalloc_area, size))	return -EFAULT;
    
        printk(KERN_INFO "rzechar: READ finished\n");
        return size;
}

ssize_t dev_write(struct file *filp, const char __user *buf, size_t size, loff_t *f_pos)
{    
        struct char_dev *dev = filp->private_data;
        printk(KERN_INFO "rzechar: dev_write count: %zu f_pos: %lld \n", size, *f_pos);

        if (!dev) {
                printk(KERN_WARNING "rzechar: no device data \n");
                return -EFAULT;
        }
        
        if (size > NPAGES * PAGE_SIZE)	size = NPAGES * PAGE_SIZE;
    
	memset(dev->kmalloc_area, 0, NPAGES * PAGE_SIZE);

	if (copy_from_user(dev->kmalloc_area, buf, size))	return -EFAULT;
    
        printk(KERN_INFO "rzechar: WRITE finished\n");
        return size;
}

static int __init rzechardrv_init(void){

        int result = 0;
	dev_t devt = 0;
        size_t i = 0;

        result = alloc_chrdev_region(&devt, 0, 1, DEVICE_NAME);
        rzechar_major = MAJOR(devt);

        if (result < 0){
        	printk(KERN_WARNING "rzechar: can't allocate region for device. major: %d\n", rzechar_major);
        	return 0;
        }
        else printk(KERN_INFO "rzechar: LKM registered with major:%d\n", rzechar_major);

        // Register the device class
        rzecharClass = class_create(THIS_MODULE, CLASS_NAME);
        if (IS_ERR(rzecharClass)){
                printk(KERN_ALERT "Failed to register device class\n");
                result = PTR_ERR(rzecharClass);
                goto clean_chrdev;
        }
        printk(KERN_INFO "rzechar: device class registered correctly\n");

        rzecharDevice = device_create(rzecharClass, NULL, devt, NULL, DEVICE_NAME);
        if (IS_ERR(rzecharDevice)){
                printk(KERN_ALERT "Failed to create the device\n");
                result = PTR_ERR(rzecharDevice);
                goto clean_class;
        }
        printk(KERN_INFO "rzechar: device class created correctly\n");

        my_dev = kmalloc(sizeof(struct char_dev), GFP_KERNEL);
        if (!my_dev) {
		result = -ENOMEM;
		printk(KERN_WARNING " ERROR kmalloc dev struct\n");
		goto clean_dev;
	}

        memset(my_dev, 0, sizeof(struct char_dev));
 
        my_dev->kmalloc_ptr = kmalloc((NPAGES + 2) * PAGE_SIZE, GFP_KERNEL);
        if (!my_dev->kmalloc_ptr) {
		result = PTR_ERR(my_dev->kmalloc_ptr);
		printk(KERN_WARNING "ERROR kmalloc kmalloc_ptr\n");
		goto clean_dev;
        }
        my_dev->kmalloc_area = (char*) PAGE_ALIGN((unsigned long) my_dev->kmalloc_ptr);
        
        for (i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE)	SetPageReserved(virt_to_page((unsigned long) my_dev->kmalloc_area) + i);
        
        for (i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE){
		my_dev->kmalloc_area[i] = 0xAA;
		my_dev->kmalloc_area[i + 1] = 0xBB;
		my_dev->kmalloc_area[i + 2] = 0xCC;
		my_dev->kmalloc_area[i + 3] = 0xDD;
	}
        
        cdev_init(&my_dev->cdev, &fops);
        my_dev->cdev.owner = THIS_MODULE;
        my_dev->cdev.ops = &fops;

        result = cdev_add(&my_dev->cdev, devt, 1);
        
        if (result){
                printk(KERN_WARNING "rzechar: Error %d adding cdev", result);
                goto clean_mem;
	}
        
	return 0;
	
	clean_mem:
		for (i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE)	ClearPageReserved(virt_to_page((unsigned long) my_dev->kmalloc_area + i));
		kfree(my_dev->kmalloc_ptr);
	clean_dev:
		device_destroy(rzecharClass, MKDEV(rzechar_major, 0));
	clean_class:
		class_destroy(rzecharClass);
	clean_chrdev:
		unregister_chrdev(rzechar_major, DEVICE_NAME);
		
	return result;
}

static void __exit rzechardrv_exit(void){
	int i=0;
        cdev_del(&my_dev->cdev);
        if ((my_dev->kmalloc_ptr) != 0) {
	        for (i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE)	ClearPageReserved(virt_to_page((unsigned long) my_dev->kmalloc_area + i));
		kfree(my_dev->kmalloc_ptr);
	        printk(KERN_INFO "kfree the string-memory\n");
        }

        device_destroy(rzecharClass, MKDEV(rzechar_major, 0)); 
        class_destroy(rzecharClass);
        
	if ((my_dev) != 0) {
		kfree(my_dev);
		printk(KERN_INFO " kfree device\n");
	}
        unregister_chrdev(rzechar_major, "rzechar");

        printk(KERN_INFO "rzechar LKM unloaded!\n");
}

module_init(rzechardrv_init);
module_exit(rzechardrv_exit);
