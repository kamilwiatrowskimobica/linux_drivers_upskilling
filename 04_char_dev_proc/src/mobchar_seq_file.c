/**
 * @file    main.c
 * @author  Drost Krzysztof
 * @date    16 February 2023
 * @version 0.1
 * @brief   Simple character driver with support for read and write to device.
 *          Tested on linux version: 5.15.0-58-generic
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include "mobchar_debug.h"

#define PROC_NAME "mobchar_seq_file"
#define MAX_LINE_PRINTED (5)

static void *mobchar_seq_start(struct seq_file *s, loff_t *pos);
static void *mobchar_seq_next(struct seq_file *s, void *v, loff_t *pos);
static void mobchar_seq_stop(struct seq_file *s, void *v);
static int mobchar_seq_show(struct seq_file *s, void *v);
static int mobchar_seq_file_open(struct inode *inode, struct file *filp);

static int current_pos = 0;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Drost Krzysztof");

static struct seq_operations mobchar_seq_ops = { .start = mobchar_seq_start,
						 .next = mobchar_seq_next,
						 .stop = mobchar_seq_stop,
						 .show = mobchar_seq_show };

static struct proc_ops mobchar_proc_ops = { .proc_open = mobchar_seq_file_open,
					    .proc_read = seq_read,
					    .proc_lseek = seq_lseek,
					    .proc_release = seq_release };

int __init mobchar_seq_file_init(void)
{
	struct proc_dir_entry *entry;
	DEBUG_PRINT("%s: Start\n", __FUNCTION__);
	entry = proc_create(PROC_NAME, 0, NULL, &mobchar_proc_ops);
	if (entry == NULL) {
		DEBUG_PRINT("%s: End\n", __FUNCTION__);
		return -ENOMEM;
	}

	DEBUG_PRINT("%s: End\n", __FUNCTION__);
	return 0;
}

static void *mobchar_seq_start(struct seq_file *s, loff_t *pos)
{
	loff_t *counter;
	DEBUG_PRINT("%s: Start\n", __FUNCTION__);
	counter = kmalloc(sizeof(loff_t), GFP_KERNEL);

	if (*pos >= MAX_LINE_PRINTED) {
		*pos = 0;
		DEBUG_PRINT("%s: return null\n", __FUNCTION__);
		return NULL;
	}
	*counter = *pos;
	DEBUG_PRINT("*pos = %llu, *counter = %llu \n", *pos, *counter);
	DEBUG_PRINT("%s: return pos\n", __FUNCTION__);
	return counter;
}

static void *mobchar_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	loff_t *tmp_v = (loff_t *)v;
	DEBUG_PRINT("%s: Start\n", __FUNCTION__);
	DEBUG_PRINT("%s: BEFORE *pos = %llu \n", __FUNCTION__, *pos);
	*pos = ++*tmp_v;
	if (*pos >= MAX_LINE_PRINTED) {
		DEBUG_PRINT("%s: pos = %llu\n", __FUNCTION__, *pos);
		return NULL;
	}
	DEBUG_PRINT("%s: END  *pos = *tmp_v = %llu \n", __FUNCTION__, *pos);
	DEBUG_PRINT("%s: return tmp_v\n", __FUNCTION__);
	return tmp_v;
}

static void mobchar_seq_stop(struct seq_file *s, void *v)
{
	DEBUG_PRINT("%s: Start\n", __FUNCTION__);
	kfree(v);
	DEBUG_PRINT("%s: End\n", __FUNCTION__);
}

static int mobchar_seq_show(struct seq_file *s, void *v)
{
	loff_t *spos = (loff_t *)v;
	DEBUG_PRINT("%s: Start\n", __FUNCTION__);
	seq_printf(s, "proc_seq_file print nr %Ld\n", *spos);
	DEBUG_PRINT("%s: End\n", __FUNCTION__);
	return 0;
}

static int mobchar_seq_file_open(struct inode *inode, struct file *file)
{
	DEBUG_PRINT("%s: Start\n", __FUNCTION__);
	DEBUG_PRINT("%s: return seq_open\n", __FUNCTION__);
	return seq_open(file, &mobchar_seq_ops);
};

void __exit mobchar_seq_file_exit(void)
{
	DEBUG_PRINT("%s: Start\n", __FUNCTION__);
	remove_proc_entry(PROC_NAME, NULL);
	DEBUG_PRINT("%s: End\n", __FUNCTION__);
}
