#ifndef _MOB_CHAR_H_
#define _MOB_CHAR_H_

#include "mob_sync.h"

#define DEVICE_MAX_SIZE (20u)
#define CACHE_SIZE		(1024u)

struct mob_dev {
	char *p_data; /* pointer to the memory allocated */
	int data_len; /* Data in buffer */
	int buffer_len; /* max buffer len */
	int open_num;  
	int index_of_dev;
	int ioctl_var;
	struct cdev cdev; /* Char device structure		*/

	// Sync
	sync_mode mode;
	union {
		struct semaphore semphr;
		struct completion compl;
		struct sync_poll poll;
	};
	spinlock_t spn_lck;

	char *cache;
};

#endif /* _MOB_CHAR_H_ */