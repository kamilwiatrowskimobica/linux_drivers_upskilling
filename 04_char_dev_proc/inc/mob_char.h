#ifndef _MOB_CHAR_H_
#define _MOB_CHAR_H_

#define DEVICE_MAX_SIZE (20)

struct mob_dev {
	char *p_data; /* pointer to the memory allocated */
	int data_len; /* Data in buffer */
	int buffer_len; /* max buffer len */
	int open_num;  
	struct cdev cdev; /* Char device structure		*/
};


#endif /* _MOB_CHAR_H_ */