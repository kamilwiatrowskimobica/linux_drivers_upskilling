#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/rmap.h>
#include <linux/seq_file.h>
#include <linux/version.h>

#define DEV_NUMBER 2
#define MAX_DEV_BUFFER_SIZE 255
#define DEVICE_NAME "chardevice"
#define CLASS_NAME "charclass"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A B");
MODULE_DESCRIPTION("Char driver");
MODULE_VERSION("1.0");

#define NPAGES 16
#define PROC_ENTRY_NAME "mmap_driver_seq"

static int dev_major;
static int dev_minor = 0;
static struct cdev cdev;
static void* kmalloc_ptr;
static char* kmalloc_area;

static struct device_data* devices = NULL;
static struct class* dev_class = NULL;

static ssize_t dev_read(struct file* file, char* __user buffer, size_t size, loff_t* offset);
static ssize_t dev_write(struct file* file, const char* __user buffer, size_t size, loff_t* offset);
static int dev_open(struct inode* inode, struct file* file);
static int dev_release(struct inode* inode, struct file* file);
static int dev_proc_show(struct seq_file* seq_file, void* v);
static int dev_proc_open(struct inode* inode, struct file* file);
static int dev_mmap(struct file* file, struct vm_area_struct* vm);

static struct file_operations fops = 
{
    .owner = THIS_MODULE,
    .mmap = dev_mmap,
    .open = dev_open,
    .read = dev_read,
    .release = dev_release,    
    .write = dev_write
};

static const struct proc_ops pops = 
{
    .proc_open = dev_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release
};

static int dev_proc_show(struct seq_file* seq, void* v)
{
    printk(KERN_INFO "CharDriver: Proc file show");

    struct mm_struct* mm;
    struct vm_area_struct* vma_iterator;
    unsigned long total = 0;

    mm = get_task_mm(current);
    vma_iterator = mm->mmap;

    while (vma_iterator)
    {
        printk("%lx %lx\n", vma_iterator->vm_start, vma_iterator->vm_end);
        total += vma_iterator->vm_end - vma_iterator->vm_start;
        vma_iterator = vma_iterator->vm_next;
    }

    mmput(mm);

    seq_printf(seq, "%lu\n", total);

    return 0;
}

static int dev_proc_open(struct inode* inode, struct file* file)
{
    printk(KERN_INFO "CharDriver: Proc file open");
    return single_open(file, dev_proc_show, NULL);
}

static ssize_t dev_read(struct file* file, char* __user buffer, size_t size, loff_t* offset)
{
    printk(KERN_INFO "CharDriver: Read\n");

    if (size > NPAGES * PAGE_SIZE)
        size = NPAGES * PAGE_SIZE;
    
    if (copy_to_user(buffer, kmalloc_area, size))
        return -EFAULT;

    return size;
}

static ssize_t dev_write(struct file* file, const char* __user buffer, size_t size, loff_t* offset)
{
    printk(KERN_INFO "CharDriver: Write\n");

    if (size > NPAGES * PAGE_SIZE)
        size = NPAGES * PAGE_SIZE;
    
    memset(kmalloc_area, 0, NPAGES * PAGE_SIZE);

    if (copy_from_user(kmalloc_area, buffer, size))
        return -EFAULT;

    return size;
}

static int dev_mmap(struct file* file, struct vm_area_struct* vma)
{
    int ret;
    long length = vma->vm_end - vma->vm_start;

    printk("CharDriver: MMap length = %lu, vm_start = %lx, vm_end = %lx, kmalloc_area = %p\
            PAGE_SHIFT = %d, physical address = %llu", length, vma->vm_start, vma->vm_end,
            kmalloc_area, PAGE_SHIFT, virt_to_phys((void*) kmalloc_area));

    if (length > NPAGES * PAGE_SIZE)
        return -EIO;

    ret = remap_pfn_range(vma, vma->vm_start,
                    virt_to_phys((void*) kmalloc_area) >> PAGE_SHIFT,
                    length, vma->vm_page_prot);

    if (ret < 0)
    {
        printk(KERN_INFO "CharDriver: Mapping failed\n");
        return ret;
    }

    return 0;
}

static int dev_open(struct inode* inode, struct file* file)
{
    printk(KERN_INFO "CharDriver: Open");

    return 0;
}

static int dev_release(struct inode* inode, struct file* file)
{
    printk(KERN_INFO "CharDriver: Release");

    return 0;
}

static int dev_init(void)
{
    int i, result;
    dev_t dev;
    struct proc_dir_entry* entry;

    printk(KERN_DEBUG "CharDriver: Dynamic device registration\n");
    result = alloc_chrdev_region(&dev, dev_minor, DEV_NUMBER, DEVICE_NAME);

    dev_major = MAJOR(dev);
    printk(KERN_DEBUG "CharDriver: Major: %i, minor %i\n", dev_major, dev_minor);
    
    if (result)
    {
        printk(KERN_ERR "CharDriver: Registration error\n");
        return result;
    }

    cdev_init(&cdev, &fops);
    result = cdev_add(&cdev, MKDEV(dev_major, 0), 1);

    if (result)
    {
        printk(KERN_ERR "CharDriver: Device not initialized\n");
        unregister_chrdev_region(dev, 1);
        return result;
    }

    kmalloc_ptr = kmalloc((NPAGES + 2) * PAGE_SIZE, GFP_KERNEL);
    if (!kmalloc_ptr)
    {
        printk(KERN_ERR "CharDriver: Memory allocation error\n");
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(kmalloc_ptr);
    }

    printk(KERN_INFO "CharDriver: Allocation successful, size = %lu, page size = %lu, pages = %d\n",
                     kmalloc_ptr, (NPAGES + 2) * PAGE_SIZE, PAGE_SIZE, (NPAGES + 2));

    kmalloc_area = (char*) PAGE_ALIGN((unsigned long) kmalloc_ptr);

    for (i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE)
        SetPageReserved(virt_to_page((unsigned long) kmalloc_area) + i);

    for (i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE)
    {
        kmalloc_area[i] = 0xAA;
        kmalloc_area[i + 1] = 0xBB;
        kmalloc_area[i + 2] = 0xCC;
        kmalloc_area[i + 3] = 0xDD;
    }

    entry = proc_create(PROC_ENTRY_NAME, 0, NULL, &pops);
    if (!entry)
    {
        kfree(kmalloc_ptr);
        unregister_chrdev_region(dev, 1);
        return -EFAULT;
    }

    return 0;
}

static void dev_exit(void)
{
    printk(KERN_INFO "CharDriver: Exit\n");
    int i;
    dev_t dev = MKDEV(dev_major, dev_minor);
    cdev_del(&cdev);
    
    for (i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE)
        ClearPageReserved(virt_to_page((unsigned long) kmalloc_area + i));

    kfree(kmalloc_ptr);
    unregister_chrdev_region(dev, 1);
    remove_proc_entry(PROC_ENTRY_NAME, NULL);
}

module_init(dev_init);
module_exit(dev_exit);
