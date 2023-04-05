#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/bio.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("GPL");

//#define DEBUG
//#define TRACE

#define WOAB_BLOCK_MAJOR 240
#define WOAB_BLOCK_DEV_NAME "woabdev"
#define WOAB_BLOCK_MINORS 1
#define WOAB_BLOCK_NR_SECTORS 128

#define KERNEL_SECTOR_SIZE 512

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

static struct woab_block_dev {
	struct blk_mq_tag_set tag_set;
	struct request_queue *queue;
	struct gendisk *gd;
	u8 *data;
	size_t size;
} g_dev;

static int woab_block_open(struct block_device *bdev, fmode_t mode)
{
	WTRACE();

	return 0;
}

static void woab_block_release(struct gendisk *gd, fmode_t mode)
{
	WTRACE();
}

static const struct block_device_operations my_block_ops = {
	.owner = THIS_MODULE,
	.open = woab_block_open,
	.release = woab_block_release
};

static void woab_block_transfer(struct woab_block_dev *dev, sector_t sector,
				unsigned long len, char *buffer, int dir)
{
	unsigned long offset = sector * KERNEL_SECTOR_SIZE;
	int i;

	WTRACE();

	if ((offset + len) > dev->size) {
		WTRACE();
		return;
	}

	

	if (dir == 1) { // write
		WTRACE();
		WDEBUG("%s\n", "Print write buffer");
		for (i = 0; i < len; i += 16) {
			WDEBUG("%d %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			       i, (u8)buffer[i], (u8)buffer[i + 1],
			       (u8)buffer[i + 2], (u8)buffer[i + 3],
			       (u8)buffer[i + 4], (u8)buffer[i + 5],
			       (u8)buffer[i + 6], (u8)buffer[i + 7],
			       (u8)buffer[i + 8], (u8)buffer[i + 9],
			       (u8)buffer[i + 10], (u8)buffer[i + 11],
			       (u8)buffer[i + 2], (u8)buffer[i + 13],
			       (u8)buffer[i + 14], (u8)buffer[i + 15]);
		}

		memcpy(dev->data + offset, buffer, len);

		WDEBUG("%s\n", "Printe write device buffer");
		for (i = 0; i < len; i += 16) {
			WDEBUG("%d %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			       i, dev->data[offset + i],
			       dev->data[offset + i + 1],
			       dev->data[offset + i + 2],
			       dev->data[offset + i + 3],
			       dev->data[offset + i + 4],
			       dev->data[offset + i + 5],
			       dev->data[offset + i + 6],
			       dev->data[offset + i + 7],
			       dev->data[offset + i + 8],
			       dev->data[offset + i + 9],
			       dev->data[offset + i + 10],
			       dev->data[offset + i + 11],
			       dev->data[offset + i + 12],
			       dev->data[offset + i + 13],
			       dev->data[offset + i + 14],
			       dev->data[offset + i + 15]);
		}
	} else {
		WTRACE();
		WDEBUG("%s\n", "Print read device buffer");
		for (i = 0; i < len; i += 16) {
			WDEBUG("%d %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			       i, dev->data[offset + i],
			       dev->data[offset + i + 1],
			       dev->data[offset + i + 2],
			       dev->data[offset + i + 3],
			       dev->data[offset + i + 4],
			       dev->data[offset + i + 5],
			       dev->data[offset + i + 6],
			       dev->data[offset + i + 7],
			       dev->data[offset + i + 8],
			       dev->data[offset + i + 9],
			       dev->data[offset + i + 10],
			       dev->data[offset + i + 11],
			       dev->data[offset + i + 12],
			       dev->data[offset + i + 13],
			       dev->data[offset + i + 14],
			       dev->data[offset + i + 15]);
		}

		memcpy(buffer, dev->data + offset, len);

		WDEBUG("%s\n", "Print read buffer");
		for (i = 0; i < len; i += 16) {
			WDEBUG("%d %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			       i, (u8)buffer[i], (u8)buffer[i + 1],
			       (u8)buffer[i + 2], (u8)buffer[i + 3],
			       (u8)buffer[i + 4], (u8)buffer[i + 5],
			       (u8)buffer[i + 6], (u8)buffer[i + 7],
			       (u8)buffer[i + 8], (u8)buffer[i + 9],
			       (u8)buffer[i + 10], (u8)buffer[i + 11],
			       (u8)buffer[i + 2], (u8)buffer[i + 13],
			       (u8)buffer[i + 14], (u8)buffer[i + 15]);
		}
	}
}

static blk_status_t woab_block_request(struct blk_mq_hw_ctx *hctx,
				       const struct blk_mq_queue_data *bd)
{
	struct request *rq;
	struct woab_block_dev *dev = hctx->queue->queuedata;

	WTRACE();

	rq = bd->rq;
	blk_mq_start_request(rq);

	if (blk_rq_is_passthrough(rq)) {
		WDEBUG("Skip non-fs request\n");
		blk_mq_end_request(rq, BLK_STS_IOERR);
		goto out;
	}

	WDEBUG("pos=%llu bytes=%u cur_bytes=%u dir=%c\n",
	       (unsigned long long)blk_rq_pos(rq), blk_rq_bytes(rq),
	       blk_rq_cur_bytes(rq), rq_data_dir(rq) ? 'W' : 'R');

	woab_block_transfer(dev, blk_rq_pos(rq), blk_rq_bytes(rq),
			    bio_data(rq->bio), rq_data_dir(rq));

	blk_mq_end_request(rq, BLK_STS_OK);

out:
	return BLK_STS_OK;
}

static struct blk_mq_ops woab_queue_ops = {
	.queue_rq = woab_block_request,
};

static int create_block_device(struct woab_block_dev *dev)
{
	int err;

	WTRACE();

	dev->size = WOAB_BLOCK_NR_SECTORS * KERNEL_SECTOR_SIZE;
	dev->data = vmalloc(dev->size);
	if (dev->data == NULL) {
		WDEBUG("%s\n", "can not allocate buffer");
		err = -ENOMEM;
		goto out_vmalloc;
	}
#ifdef DEBUG
	memset(dev->data, 0xBB, dev->size);
#endif

	dev->tag_set.ops = &woab_queue_ops;
	dev->tag_set.nr_hw_queues = 1;
	dev->tag_set.queue_depth = 128;
	dev->tag_set.numa_node = NUMA_NO_NODE;
	dev->tag_set.cmd_size = 0;
	dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	err = blk_mq_alloc_tag_set(&dev->tag_set);
	if (err) {
		WDEBUG("%s\n", "can not allocate tag set");
		goto out_alloc_tag_set;
	}

	dev->queue = blk_mq_init_queue(&dev->tag_set);
	if (IS_ERR(dev->queue)) {
		WDEBUG("%s\n", "out of memory");
		err = -ENOMEM;
		goto out_blk_init;
	}
	blk_queue_logical_block_size(dev->queue, KERNEL_SECTOR_SIZE);
	dev->queue->queuedata = dev;

	static struct lock_class_key lkclass; // FIXME
	dev->gd = __alloc_disk_node(dev->queue, WOAB_BLOCK_MINORS, &lkclass);
	if (!dev->gd) {
		WDEBUG("%s\n", "failed to allocate disk");
		err = -ENOMEM;
		goto out_alloc_disk;
	}

	dev->gd->major = WOAB_BLOCK_MAJOR;
	dev->gd->minors =
		WOAB_BLOCK_MINORS; // configure minors, otherwise kernel complain
	dev->gd->first_minor = 0;
	dev->gd->fops = &my_block_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf(dev->gd->disk_name, DISK_NAME_LEN, WOAB_BLOCK_DEV_NAME);
	set_capacity(dev->gd, WOAB_BLOCK_NR_SECTORS);

	add_disk(dev->gd);

	return 0;

out_alloc_disk:
	blk_cleanup_queue(dev->queue);
out_blk_init:
	blk_mq_free_tag_set(&dev->tag_set);
out_alloc_tag_set:
	vfree(dev->data);
out_vmalloc:
	return err;
}

static int __init woab_block_init(void)
{
	int err = 0;

	WTRACE();

	err = register_blkdev(WOAB_BLOCK_MAJOR, WOAB_BLOCK_DEV_NAME);
	if (err < 0) {
		printk(KERN_ERR "register_blkdev: unable to register\n");
		return err;
	}

	err = create_block_device(&g_dev);
	if (err < 0)
		goto out;

	return 0;

out:
	unregister_blkdev(WOAB_BLOCK_MAJOR, WOAB_BLOCK_DEV_NAME);
	return err;
}

static void delete_block_device(struct woab_block_dev *dev)
{
	WTRACE();

	if (dev->gd) {
		del_gendisk(dev->gd);
		put_disk(dev->gd);
	}

	if (dev->queue)
		blk_cleanup_queue(dev->queue);
	if (dev->tag_set.tags)
		blk_mq_free_tag_set(&dev->tag_set);
	if (dev->data)
		vfree(dev->data);
}

static void __exit woab_block_exit(void)
{
	WTRACE();

	delete_block_device(&g_dev);

	unregister_blkdev(WOAB_BLOCK_MAJOR, WOAB_BLOCK_DEV_NAME);
}

module_init(woab_block_init);
module_exit(woab_block_exit);