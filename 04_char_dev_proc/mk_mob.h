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

#include <linux/cdev.h>

#define MK_DEV_MEMALLOC_MAX_SIZE 255

struct mk_mob_dev {
	char *data; ///< Pointer to device memory
	unsigned long size; ///< actual size of written @a data

	struct cdev cdev; ///< Char device structure assigned to mk_mob_dev instance
};

#endif // _MK_MOB_H_