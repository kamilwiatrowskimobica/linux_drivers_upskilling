#include <linux/init.h>          // Macros used to mark up functions e.g. __init __exit
#include <linux/kernel.h>        // printk
#include <linux/module.h>

#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/bio.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rafal Zielinski");
MODULE_DESCRIPTION("example block driver");
MODULE_VERSION("0.1");

static struct blk_dev {
        int size;                       /* Device size in sectors */
        u8 *data;                       /* The data array */
	struct blk_mq_tag_set tag_set;	/* tag_set added */
	struct request_queue *queue;    /* The device request queue */
	struct gendisk *gd;             /* The gendisk structure */
};

#define MY_BLKDEV_NAME		"rzeblk"
#define NR_SECTORS		128
#define KERNEL_SECTOR_SIZE	512
#define MY_BLOCK_MINORS	1


static struct blk_dev* my_dev = NULL;
static int rzeblk_major = 0;


static int blk_open(struct block_device *bdev, fmode_t mode){
	printk(KERN_INFO "rzeblk: open\n" );
        return 0;
}

static void blk_release(struct gendisk *gd, fmode_t mode){
        printk(KERN_INFO "rzeblk: release\n");
        return;
}

static struct block_device_operations my_block_ops = {
        .owner = THIS_MODULE,
        .open = blk_open,
        .release = blk_release,
};

static void my_block_transfer(struct blk_dev *dev, sector_t sector,
		unsigned long len, char *buffer, int dir)
{
	unsigned long offset = sector * KERNEL_SECTOR_SIZE;

	if ((offset + len) > dev->size)
		return;

	if (dir == 1)		/* write */
		memcpy(dev->data + offset, buffer, len);
	else
		memcpy(buffer, dev->data + offset, len);
}

static void do_simple_request (struct request * rq, struct blk_dev *dev)
{
	struct bio_vec bvec;
	struct req_iterator iter;

	rq_for_each_segment(bvec, rq, iter) {
		sector_t sector = iter.iter.bi_sector;
		unsigned long offset = bvec.bv_offset;
		size_t len = bvec.bv_len;
		int dir = bio_data_dir(iter.bio);
		char *buffer = kmap_atomic(bvec.bv_page);
		printk(KERN_INFO "rzeblk: bufp: %8p offset: %lu len: %zu dir: %d\n", buffer, offset, len, dir);

		my_block_transfer(dev, sector, len, buffer + offset, dir);
		kunmap_atomic(buffer);
	}

    return;
}


static blk_status_t my_block_request(struct blk_mq_hw_ctx *hctx,
                                     const struct blk_mq_queue_data *bd){
                                     
	struct request *rq = bd->rq;
	struct blk_dev *dev = hctx->queue->queuedata;
	blk_status_t status = BLK_STS_OK;

	blk_mq_start_request(rq);

	if (blk_rq_is_passthrough(rq)) {
		printk (KERN_NOTICE "Skip non-fs request\n");
		status = BLK_STS_IOERR;
		goto out;
	}

	printk(KERN_INFO 
		"request received: pos=%llu bytes=%u "
		"cur_bytes=%u dir=%c\n",
		(unsigned long long) blk_rq_pos(rq),
		blk_rq_bytes(rq), blk_rq_cur_bytes(rq),
		rq_data_dir(rq) ? 'W' : 'R');
	
        do_simple_request(rq, dev);
out:
	blk_mq_end_request(rq, status);
	return status;                                   
};

static struct blk_mq_ops my_queue_ops = {
	.queue_rq = my_block_request,
};

static int setup_device(struct blk_dev *dev)
{
	int err;
	static struct lock_class_key lkclass;

	dev->size = NR_SECTORS * KERNEL_SECTOR_SIZE;
	dev->data = vmalloc(dev->size);
	if (dev->data == NULL) {
		printk(KERN_ERR "vmalloc: out of memory\n");
		err = -ENOMEM;
		goto out_vmalloc;
	}

	/* Initialize tag set. */
	dev->tag_set.ops = &my_queue_ops;
	dev->tag_set.nr_hw_queues = 1;
	dev->tag_set.queue_depth = 128;
	dev->tag_set.numa_node = NUMA_NO_NODE;
	dev->tag_set.cmd_size = 0;
	dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	err = blk_mq_alloc_tag_set(&dev->tag_set);
	if (err) {
	    printk(KERN_ERR "blk_mq_alloc_tag_set: can't allocate tag set\n");
	    goto out_alloc_tag_set;
	}

	/* Allocate multiqueue. */
	dev->queue = blk_mq_init_queue(&dev->tag_set);
	if (IS_ERR(dev->queue)) {
		printk(KERN_ERR "blk_mq_init_queue: out of memory\n");
		err = -ENOMEM;
		goto out_blk_init;
	}
	blk_queue_logical_block_size(dev->queue, KERNEL_SECTOR_SIZE);
	dev->queue->queuedata = dev;

	/* initialize the gendisk structure */
	dev->gd = __alloc_disk_node(dev->queue, MY_BLOCK_MINORS, &lkclass);
	if (!dev->gd) {
		printk(KERN_ERR "alloc_disk: failure\n");
		err = -ENOMEM;
		goto out_alloc_disk;
	}

	dev->gd->major = rzeblk_major;
	dev->gd->first_minor = 0;
	dev->gd->minors = MY_BLOCK_MINORS;
	dev->gd->fops = &my_block_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf(dev->gd->disk_name, DISK_NAME_LEN, MY_BLKDEV_NAME);
	set_capacity(dev->gd, NR_SECTORS);

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

static int __init rzechardrv_init(void){
        
        rzeblk_major = register_blkdev(rzeblk_major, MY_BLKDEV_NAME);
	if (rzeblk_major <= 0) {
		printk(KERN_WARNING "rzeblk: unable to get major number\n");
		return -EBUSY;
	}
        else printk(KERN_INFO "rzeblk: LKM registered with major:%d\n", rzeblk_major);

        my_dev = kmalloc(sizeof (struct blk_dev), GFP_KERNEL);
	if (my_dev == NULL){
		printk(KERN_WARNING "rzeblk: error allocating mem for device\n");
		goto out_unregister;
	}
	
	setup_device(my_dev);
    
	return 0;

out_unregister:
	unregister_blkdev(rzeblk_major, MY_BLKDEV_NAME);
	return -ENOMEM;
}

static void delete_device(struct blk_dev *dev)
{
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

static void __exit rzechardrv_exit(void){
	delete_device(my_dev);
	unregister_blkdev(rzeblk_major, MY_BLKDEV_NAME);
	kfree(my_dev);

	printk(KERN_INFO "rzeblk LKM unloaded!\n");
}

module_init(rzechardrv_init);
module_exit(rzechardrv_exit);
