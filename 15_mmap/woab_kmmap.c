#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/rmap.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "woab_kmmap.h"

MODULE_LICENSE("GPL");

#define DEBUG
#define TRACE

#ifdef DEBUG
#define WDEBUG(fmt, args...) printk(KERN_DEBUG "WOABMMAP: " fmt, ##args)
#else
#define WDEBUG(fmt, args...)
#endif

#ifdef TRACE
#define WTRACE() printk(KERN_INFO "WOABMMAP: %s %d\n", __FUNCTION__, __LINE__)
#else
#define WTRACE()
#endif

static struct cdev mmap_cdev;
static void *kmalloc_ptr;
static char *kmalloc_area;
static int woabchar_major;
static int woabchar_minor;

static int woab_open(struct inode *inode, struct file *filp)
{
	WTRACE();
	return 0;
}

static int woab_release(struct inode *inode, struct file *filp)
{
	WTRACE();
	return 0;
}

static ssize_t woab_read(struct file *file, char __user *user_buffer,
			 size_t size, loff_t *offset)
{
	WTRACE();

	if (size > NPAGES * PAGE_SIZE)
		size = NPAGES * PAGE_SIZE;

	if (copy_to_user(user_buffer, kmalloc_area, size))
		return -EFAULT;

	return size;
}

static ssize_t woab_write(struct file *file, const char __user *user_buffer,
			  size_t size, loff_t *offset)
{
	WTRACE();

	if (size > NPAGES * PAGE_SIZE)
		size = NPAGES * PAGE_SIZE;

	memset(kmalloc_area, 0, NPAGES * PAGE_SIZE);
	if (copy_from_user(kmalloc_area, user_buffer, size))
		return -EFAULT;

	return size;
}

static int woab_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	long length = vma->vm_end - vma->vm_start;

	WTRACE();

	WDEBUG("%s called with length = %lu, (%lx - %lx), %p, %d, %llu\n",
	       __FUNCTION__, length, vma->vm_end, vma->vm_start, kmalloc_area,
	       PAGE_SHIFT, virt_to_phys((void *)kmalloc_area));

	if (length > NPAGES * PAGE_SIZE)
		return -EIO;

	ret = remap_pfn_range(vma, vma->vm_start,
			      virt_to_phys((void *)kmalloc_area) >> PAGE_SHIFT,
			      length, vma->vm_page_prot);
	if (ret < 0) {
		pr_err("could not map address area\n");
		return ret;
	}

	return 0;
}

static const struct file_operations woab_mmap_fops = { .owner = THIS_MODULE,
						       .open = woab_open,
						       .release = woab_release,
						       .mmap = woab_mmap,
						       .read = woab_read,
						       .write = woab_write };

static int woab_seq_show(struct seq_file *seq, void *v)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma_iterator;
	unsigned long total = 0;

	WTRACE();

	mm = get_task_mm(current);

	vma_iterator = mm->mmap;
	while (vma_iterator != NULL) {
		total += vma_iterator->vm_end - vma_iterator->vm_start;
		WDEBUG("%lx %lx %lu %lu\n", vma_iterator->vm_start,
		       vma_iterator->vm_end,
		       vma_iterator->vm_end - vma_iterator->vm_start, total);
		vma_iterator = vma_iterator->vm_next;
	}

	mmput(mm);

	seq_printf(seq, "%lu\n", total);
	return 0;
}

static int woab_seq_open(struct inode *inode, struct file *file)
{
	WTRACE();
	return single_open(file, woab_seq_show, NULL);
}

static const struct proc_ops woab_proc_ops = {
	.proc_open = woab_seq_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init my_init(void)
{
	int ret = 0;
	int i;
	dev_t dev = 0;

	struct proc_dir_entry *entry;

	WTRACE();

	ret = alloc_chrdev_region(&dev, woabchar_minor, 1, MAP_NAME);
	woabchar_major = MAJOR(dev);

	if (ret < 0) {
		WDEBUG("Can't get major %d\n", woabchar_major);
		goto out;
	}

	cdev_init(&mmap_cdev, &woab_mmap_fops);
	ret = cdev_add(&mmap_cdev, MKDEV(woabchar_major, 0), 1);
	if (ret) {
		WDEBUG("Error %d adding cdev", ret);
		goto out_unreg;
	}

	entry = proc_create(PROC_ENTRY_NAME, 0, NULL, &woab_proc_ops);
	if (!entry) {
		ret = -ENOMEM;
		goto out_unreg;
	}

	kmalloc_ptr = kmalloc((NPAGES + 2) * PAGE_SIZE, GFP_KERNEL);
	if (kmalloc_ptr == NULL) {
		ret = -ENOMEM;
		pr_err("could not allocate memory\n");
		goto out_no_memory;
	}

	WDEBUG("%p allocated, size = %lu, page size = %lu, total %d pages\n",
	       kmalloc_ptr, (NPAGES + 2) * PAGE_SIZE, PAGE_SIZE, (NPAGES + 2));

	kmalloc_area = (char *)PAGE_ALIGN(((unsigned long)kmalloc_ptr));

	WDEBUG("%p aligned to %p\n", kmalloc_ptr, kmalloc_area);

	for (i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE)
		SetPageReserved(
			virt_to_page(((unsigned long)kmalloc_area) + i));

	for (i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE) {
		kmalloc_area[i] = 0xaa;
		kmalloc_area[i + 1] = 0xbb;
		kmalloc_area[i + 2] = 0xcc;
		kmalloc_area[i + 3] = 0xdd;
		WDEBUG("write data to %p %p %p %p\n", &kmalloc_area[i],
		       &kmalloc_area[i + 1], &kmalloc_area[i + 2],
		       &kmalloc_area[i + 3]);
	}

	return 0;

out_no_memory:
	remove_proc_entry(PROC_ENTRY_NAME, NULL);
out_unreg:
	unregister_chrdev_region(dev, 1);
out:
	return ret;
}

static void __exit my_exit(void)
{
	int i;
	dev_t devno;

	WTRACE();

	cdev_del(&mmap_cdev);

	for (i = 0; i < NPAGES * PAGE_SIZE; i += PAGE_SIZE)
		ClearPageReserved(
			virt_to_page(((unsigned long)kmalloc_area) + i));
	kfree(kmalloc_ptr);

	devno = MKDEV(woabchar_major, woabchar_minor);
	unregister_chrdev_region(devno, 1);

	remove_proc_entry(PROC_ENTRY_NAME, NULL);
}

module_init(my_init);
module_exit(my_exit);