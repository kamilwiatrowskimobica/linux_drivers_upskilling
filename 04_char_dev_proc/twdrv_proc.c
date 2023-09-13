#include "twdrv_proc.h"
#include "twdrv_data.h"
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>



extern struct twdrv_dev *twdrv_devices;
extern int twdrv_nr_devs;

static void *twdrv_seq_start(struct seq_file *s, loff_t *pos)
{
    printk("[twdrv_proc] seq_start\n");
    printk("[twdrv_proc] seq_start *pos = %lld\n", *pos);
    if (*pos >= twdrv_nr_devs)
        return NULL; /* No more to read */
    return twdrv_devices + *pos; 
}


static void *twdrv_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    (*pos)++;
    if (*pos >= twdrv_nr_devs) {
        printk("[twdrv_proc] seq_next - no next pos\n");
        return NULL;
    }
    printk("[twdrv_proc] seq_next *pos = %lld\n", *pos);

    return twdrv_devices + *pos;
}


static void twdrv_seq_stop(struct seq_file *s, void *v) 
{
    printk("[twdrv_proc] seq_stop\n");
}


static int scull_seq_show(struct seq_file *s, void *v)
{
    struct twdrv_dev *dev = (struct twdrv_dev *)v;
    printk("[twdrv_proc] seq_show\n");
    seq_printf(s, "twdrv device %d:  %20s\n", dev->id, dev->p_data);
    return 0;
}


static struct seq_operations twdrv_seq_ops = {
    .start = twdrv_seq_start,
    .next = twdrv_seq_next,
    .stop = twdrv_seq_stop,
    .show = scull_seq_show
};


static int twdrv_proc_open(struct inode *inode, struct file *file)
{
    printk("[twdrv_proc] Open\n");
    return seq_open(file, &twdrv_seq_ops);
}


static struct proc_ops twdrv_seq_pops = {
	.proc_open = twdrv_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};


int twdrv_proc_init()
{
    struct proc_dir_entry *dir_entry;    

    dir_entry = proc_create("twdrv_seq", 0, NULL, &twdrv_seq_pops);
    if (!dir_entry) {
        printk("[twdrv_proc] Init failed\n");
        return -ENOENT;
    }
        
    printk("[twdrv_proc] Init successful\n");
    return 0;
}


void twdrv_proc_release()
{
    remove_proc_entry("twdrv_seq", NULL);
}