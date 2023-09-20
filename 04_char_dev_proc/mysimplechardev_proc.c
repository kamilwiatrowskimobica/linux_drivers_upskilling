
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include "mysimplechardev_main.h"

static void *mysimplechardev_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= MY_DEV_COUNT || *pos < 0)
		return NULL;
	return my_devices + *pos;
	return NULL;
}

static void *mysimplechardev_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;
	if (*pos >= MY_DEV_COUNT)
		return NULL;
	return my_devices + *pos;
	return NULL;
}

static void mysimplechardev_seq_stop(struct seq_file *s, void *v)
{
	pr_info("SCHD: seq_stop\n");
	seq_printf(s, "Stopping proc seq access!\n");
}

static int mysimplechardev_seq_show(struct seq_file *s, void *v)
{
	struct mysimplechardev_dev *mdev = (struct mysimplechardev_dev *)v;
	if (!mdev) {
		pr_warn("SCHD: failed, no device data\n");
		return -EFAULT;
	}

	seq_printf(s, "Mysimplechardev_%i, size = %zu\n",
		   (int)(mdev - my_devices), mdev->data_size);
	return 0;
}

static struct seq_operations proc_sops = {
	.start = mysimplechardev_seq_start,
	.next = mysimplechardev_seq_next,
	.stop = mysimplechardev_seq_stop,
	.show = mysimplechardev_seq_show,
};

static int mysimplechardev_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &proc_sops);
}

static struct proc_ops seq_pops = {
	.proc_open = mysimplechardev_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

int mysimplechardev_read_procsimple(struct seq_file *s, void *v)
{
	size_t i;
	if (!my_devices) {
		pr_warn("failed, no device data\n");
		return -EFAULT;
	}

	for (i = 0; i < MY_DEV_COUNT; ++i) {
		struct mysimplechardev_dev *mdev = my_devices + i;
		seq_printf(s, "Mysimplechardev_%zu, size = %zu\n", i,
			   mdev->data_size);
	}
	seq_printf(s, "Finished mysimplechardev_read_procsimple!\n");

	return 0;
}

void mysimplechardev_create_proc(void)
{
	struct proc_dir_entry *entry;
	entry = proc_create_single("mysimplechardev_simple", 0, NULL,
				   mysimplechardev_read_procsimple);
	if (!entry)
		pr_warn("SCHD: Proc simple entry not created!\n");
	entry = proc_create("mysimplechardev_seq", 0, NULL, &seq_pops);
	if (!entry)
		pr_warn("SCHD: Proc seq entry not created!\n");
}

void mysimplechardev_remove_proc(void)
{
	remove_proc_entry("mysimplechardev_simple", NULL);
	remove_proc_entry("mysimplechardev_seq", NULL);
}