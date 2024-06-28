/**
 * @file   mk_mob.h
 * @author Michal Kozikowski
 * @date   11 June 2024
 * @version 0.1
 * @brief   A simple character driver using preallocated RAM memory. 
 * Implementation is based on the book "Linux Device Drivers" 
 * by Alessandro Rubini and Jonathan Corbet, published by O'Reilly & Associates.
 */

#ifndef _MK_MOB_H_
#define _MK_MOB_H_

#include <linux/types.h>

#ifdef __KERNEL__
#include <linux/semaphore.h>
#include <linux/cdev.h>

#define MK_DEV_MEMALLOC_MAX_SIZE 255

struct mk_mob_dev {
	char *data; ///< Pointer to device memory
	unsigned long size; ///< actual size of written @a data

	struct cdev cdev; ///< Char device structure assigned to mk_mob_dev instance
	struct semaphore sem;     ///< mutual exclusion semaphore, protecting the access to an instance of this struct.
	wait_queue_head_t pollq; ///< poll wait queue
	bool wr_enable; ///< write enable flag
};

#endif // __KERNEL__

/*
 * Ioctl definitions
 */

struct mk_mob_ioctl_data {
	__u32 off;
	__u32 size;
#ifdef __KERNEL__
	__u8 __user *data;
#else
	__u8 *data;
#endif // __KERNEL__
};

#define MK_MOB_IOC_MAGIC 'M'

#define MK_MOB_IOC_RESET 		 _IO(MK_MOB_IOC_MAGIC, 0)
#define MK_MOB_IOC_SET   		 _IOW(MK_MOB_IOC_MAGIC, 1, struct mk_mob_ioctl_data)
#define MK_MOB_IOC_GET   	 	 _IOR(MK_MOB_IOC_MAGIC, 2, struct mk_mob_ioctl_data)
#define MK_MOB_IOC_ENABLE_WRITE  _IO(MK_MOB_IOC_MAGIC, 3)
#define MK_MOB_IOC_DISABLE_WRITE _IO(MK_MOB_IOC_MAGIC, 4)

#define MK_MOB_IOC_MAXNR 4

#endif // _MK_MOB_H_