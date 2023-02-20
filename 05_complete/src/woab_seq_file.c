#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include <woab_debug.h>

#define WOAB_PROC_NAME "woabchar"
#define WOAB_PROC_MAX (3)

// FIXME
int woab_device_return_opens(int index);

static void *woab_seq_start(struct seq_file *s, loff_t *pos);
static void *woab_seq_next(struct seq_file *s, void *v, loff_t *pos);
static void woab_seq_stop(struct seq_file *s, void *v);
static int woab_seq_show(struct seq_file *s, void *v);
static int woab_seq_file_open(struct inode *inode, struct file *file);

static struct seq_operations woab_seq_ops = { .start = woab_seq_start,
					      .next = woab_seq_next,
					      .stop = woab_seq_stop,
					      .show = woab_seq_show };

static const struct proc_ops woab_seq_file_ops = { .proc_open =
							   woab_seq_file_open,
						   .proc_read = seq_read,
						   .proc_lseek = seq_lseek,
						   .proc_release =
							   seq_release };

int __init woab_seq_file_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create(WOAB_PROC_NAME, 0, NULL, &woab_seq_file_ops);
	if (entry == NULL)
		return -ENOMEM;

	return 0;
}

void __exit woab_seq_file_close(void)
{
	remove_proc_entry(WOAB_PROC_NAME, NULL);
}

static int woab_seq_file_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &woab_seq_ops);
};

static void *woab_seq_start(struct seq_file *s, loff_t *pos)
{
	loff_t *spos;

	WSEQDEBUG("%s:%d pos = %llu\n", __FUNCTION__, __LINE__, *pos);

	spos = kmalloc(sizeof(loff_t), GFP_KERNEL);
	if (NULL == spos || *pos >= WOAB_PROC_MAX) {
		WSEQDEBUG("%s:%d pos = %llu\n", __FUNCTION__, __LINE__, *pos);
		return NULL;
	}

	*spos = *pos;

	WSEQDEBUG("%s:%d pos = %llu\n", __FUNCTION__, __LINE__, *pos);

	return spos;
}

static void *woab_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	loff_t *spos;
	spos = v;

	*pos += 1;

	WSEQDEBUG("%s:%d pos = %llu\n", __FUNCTION__, __LINE__, *pos);

	if (*pos >= WOAB_PROC_MAX) {
		WSEQDEBUG("%s:%d pos = %llu\n", __FUNCTION__, __LINE__, *pos);
		return NULL;
	}

	*pos = ++*spos;
	WSEQDEBUG("%s:%d pos = %llu\n", __FUNCTION__, __LINE__, *pos);

	return spos;
}

static void woab_seq_stop(struct seq_file *s, void *v)
{
	kfree(v);
	WSEQDEBUG("%s:%d\n", __FUNCTION__, __LINE__);
}

static int woab_seq_show(struct seq_file *s, void *v)
{
	loff_t *spos = (loff_t *)v;

	WSEQDEBUG("%s:%d pos = %llu\n", __FUNCTION__, __LINE__, *spos);
	seq_printf(s, "Driver %lld opened %d times\n", *spos,
		   woab_device_return_opens(*spos));

	return 0;
}