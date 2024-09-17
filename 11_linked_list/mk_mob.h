/**
 * @file   mk_mob.h
 * @author Michal Kozikowski
 * @date   11 June 2024
 * @version 0.1
 * @brief   A simple character driver using dynamically allocated memory. 
 * Implementation is based on the book "Linux Device Drivers" 
 * by Alessandro Rubini and Jonathan Corbet, published by O'Reilly & Associates.
 */

#ifndef _MK_MOB_H_
#define _MK_MOB_H_

#include <linux/types.h>

#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/list.h>

struct my_list {
	struct list_head list;
	int data;
};

struct mk_mob_dev {
	struct cdev cdev; ///< Char device structure assigned to mk_mob_dev instance
	struct semaphore sem;     ///< mutual exclusion semaphore, protecting the access to an instance of this struct.

	struct my_list *head; ///< Head of the list of integers

};

#endif // _MK_MOB_H_