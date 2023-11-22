#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/blk_types.h>
#include <linux/blk-mq.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

#define NR_SECTORS 128
#define KERNEL_SECTOR_SIZE 512
#define DISK_NAME "block_disk"
#define DEV_NAME "block_device"

int dev_major = 240;
int dev_minor = 1;
int dev_minors = 1;

static struct block_dev
{
    struct gendisk* gd;
    struct request_queue* q;
    struct blk_mq_tag_set tag_set;
    u8* data;
    int size;
} blockdevice;

static int block_open(struct block_device* dev, fmode_t mode);
static void block_release(struct gendisk* gd, fmode_t mode);
static void block_transfer(struct block_dev* dev, sector_t sector, unsigned long len, char* buffer, int dir);
static blk_status_t block_request(struct blk_mq_hw_ctx* hctx, const struct blk_mq_queue_data* bd);

static const struct block_device_operations bops =
{
    .owner = THIS_MODULE,
    .open = block_open,
    .release = block_release
};

static struct blk_mq_ops block_queued_ops =
{
    .queue_rq = block_request
};

static int block_open(struct block_device* dev, fmode_t mode)
{
    printk(KERN_INFO "BlockDriver: Open\n");
    return 0;
}

static void block_release(struct gendisk* gd, fmode_t mode)
{
    printk(KERN_INFO "BlockDriver: Release\n");
}

static void block_transfer(struct block_dev* dev, sector_t sector, unsigned long len, char* buffer, int dir)
{
    printk(KERN_INFO "BlockDriver: Transfer\n");

    unsigned long offset = sector * KERNEL_SECTOR_SIZE;

    if ((offset + len) > dev->size)
    {
        printk(KERN_INFO "BlockDriver: Transfer request exceeds boundaries\n");
        return;
    }

    printk(KERN_INFO "BlockDriver: dir = %d\n", dir);

    if (dir) //write
    {
        printk(KERN_INFO "BlockDriver: Write");    
        memcpy(dev->data + offset, buffer, len);
    }
    else    //read
    {
        printk(KERN_INFO "BlockDriver: Read");    
        memcpy(buffer, dev->data + offset, len);
    }
}

static blk_status_t block_request(struct blk_mq_hw_ctx* hctx, const struct blk_mq_queue_data* bd)
{
    printk(KERN_INFO "BlockDriver: Request\n");

    struct request* req = bd->rq;
    struct block_dev* dev = hctx->queue->queuedata;

    blk_mq_start_request(req);

    if (blk_rq_is_passthrough(req))
    {
        printk(KERN_INFO "BlockDriver: Skip non-fs request\n");
        blk_mq_end_request(req, BLK_STS_IOERR);
        return BLK_STS_IOERR;    
    }

    printk(KERN_INFO "BlockDriver: pos=%llu bytes=%u cur_bytes=%u dir=%c\n",
                     blk_rq_pos(req), blk_rq_bytes(req), blk_rq_cur_bytes(req),
                     rq_data_dir(req) ? "write" : "read");

    block_transfer(dev, blk_rq_pos(req), blk_rq_bytes(req), bio_data(req->bio), rq_data_dir(req));
    blk_mq_end_request(req, BLK_STS_OK);

    return BLK_STS_OK;
}

static int create_block_dev(struct block_dev* dev)
{
    printk(KERN_INFO "BlockDriver: Create\n");    

    int err;

    memset(dev, 0, sizeof(struct block_dev));
    dev->size = NR_SECTORS * KERNEL_SECTOR_SIZE;
    dev->size = vmalloc(dev->size);

    if (!dev->data)
    {
        printk(KERN_INFO "BlockDriver: Memory allocation error\n");
        return -ENOMEM;    
    }

    dev->tag_set.ops = &block_queued_ops;
    dev->tag_set.nr_hw_queues = 1;
    dev->tag_set.queue_depth = 128;
    dev->tag_set.numa_node = NUMA_NO_NODE;
    dev->tag_set.cmd_size = 0;
    dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
    err = blk_mq_alloc_tag_set(&dev->tag_set);

    if (err)
    {
        printk(KERN_INFO "BlockDriver: Tag set allocation error\n");
        vfree(dev->data);
        return err;
    }

    dev->q = blk_mq_init_queue(&dev->tag_set);
    if (IS_ERR(dev->q))
    {
        printk(KERN_INFO "BlockDriver: Request queue initialization error\n");
        blk_mq_free_tag_set(&dev->tag_set);
        vfree(dev->data);
        return -ENOMEM;
    }

    dev->gd->major = dev_major;
    dev->gd->first_minor = dev_minor;
    dev->gd->minors = dev_minors;
    dev->gd->fops = &bops;
    dev->gd->queue = dev->q;
    dev->gd->private_data = dev;
    snprintf(dev->gd->disk_name, 32, DISK_NAME);
    set_capacity(dev->gd, NR_SECTORS);

    add_disk(dev->gd);

    return 0;
}

static int block_init(void)
{
    printk(KERN_INFO "BlockDriver: Init\n");

    if (register_blkdev(dev_major, DEV_NAME) < 0)
    {
        printk(KERN_INFO "BlockDriver: Device registration failed\n");
        return -EBUSY;
    }

    printk(KERN_INFO "BlockDriver: dev_major = %d\n", dev_major);

    if (create_block_dev(&blockdevice) < 0)
    {
        printk(KERN_INFO "BlockDriver: Device creation failed\n");
        unregister_blkdev(dev_major, DEV_NAME);
        return -EFAULT;
    }

    return 0;
}

static void delete_block_dev(struct block_dev* dev)
{
    printk(KERN_INFO "BlockDriver: Device cleanup\n");
    if (dev->gd)
        del_gendisk(dev->gd);

    if (dev->q)
        blk_cleanup_queue(dev->q);

    if (dev->tag_set.tags)
        blk_mq_free_tag_set(&dev->tag_set);
    
    if (dev->data)
        vfree(dev->data);
}

static void block_exit(void)
{
    printk(KERN_INFO "BlockDriver: Exit\n");
    
    delete_block_dev(&blockdevice);
    unregister_blkdev(dev_major, DEV_NAME);
}

module_init(block_init);
module_exit(block_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A B");
MODULE_DESCRIPTION("Block driver");
