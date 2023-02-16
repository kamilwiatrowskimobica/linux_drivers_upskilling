#include <linux/kernel.h> /* We're doing kernel work */
#include <linux/module.h> /* Specifically, a module */
#include <linux/proc_fs.h> /* Necessary because we use proc fs */
#include <linux/seq_file.h> /* for seq_file */
#include <linux/slab.h>
#include "mob_seq_file.h"
#include "mob_debug.h"

#define PROC_NAME "mob_char"

static void *mob_seq_start(struct seq_file *s, loff_t *pos);
static void *mob_seq_next(struct seq_file *s, void *v, loff_t *pos);
static void mob_seq_stop(struct seq_file *s, void *v);
static int mob_seq_show(struct seq_file *s, void *v);
static int mob_seq_file_open(struct inode *inode, struct file *filp);

static struct seq_operations mob_seq_ops = { .start = mob_seq_start,
					     .stop = mob_seq_stop,
					     .show = mob_seq_show,
					     .next = mob_seq_next,
};


static struct proc_ops mob_proc_ops = { .proc_open = mob_seq_file_open,
					.proc_read = seq_read,
					.proc_lseek = seq_lseek,
					.proc_release = seq_release };

int __init mob_seq_file_init(void)
{
	int ret = 0;
	struct proc_dir_entry *p_proc_read;
	p_proc_read = proc_create(PROC_NAME, 0, NULL, &mob_proc_ops);

    if (NULL == p_proc_read)
        ret = PTR_ERR(p_proc_read);

	return ret;
}

void __exit mob_seq_file_exit(void)
{
	remove_proc_entry(PROC_NAME, NULL);
}

static void *mob_seq_start(struct seq_file *s, loff_t *pos)
{
    loff_t *next;
    MOB_PRINT("%s: pos = %llu", __FUNCTION__, *pos);

    if (*pos >= MOB_PROC_MAX)
        return NULL;

    *next = *pos;
    return next;
}

static void *mob_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    loff_t *next = v;
    *pos = ++*next;
    MOB_PRINT("%s: pos = %llu", __FUNCTION__, *pos);

    if (*pos >= MOB_PROC_MAX)
        return NULL;
    return next;
}

static void mob_seq_stop(struct seq_file *s, void *v)
{
    kfree(v);
}

static int mob_seq_show(struct seq_file *s, void *v)
{
    loff_t *curr = (loff_t*)v;
    seq_printf(s, "Msg from driver %llu\n", *curr);

    return 0;
}

static int mob_seq_file_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &mob_seq_ops);
}