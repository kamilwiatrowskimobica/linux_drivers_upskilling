#ifndef _PWMOB_H_
#define _PWMOB_H_

#define PWMOB_DATA_MAX 100

struct pwmob_dev {
	char *p_data; /* pointer to the memory allocated */
	size_t data_size;
	struct cdev cdev; /* Char device structure		*/
	rwlock_t my_rwlock;;
};

#endif /* _PWMOB_H_ */