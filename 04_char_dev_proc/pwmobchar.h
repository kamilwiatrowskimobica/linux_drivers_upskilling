#ifndef _SEQ_FILE_H_
#define _SEQ_FILE_H_

#undef PDEBUG /* undef it, just in case */
#ifdef LEO_DEBUG
#ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#define PDEBUG(fmt, args...) printk(KERN_DEBUG "[LEO] " fmt, ##args)
#else
/* This one for user space */
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#endif /* __KERNEL__ */
#else
#define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif /* LEO_DEBUG */

#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#define MAX_LINE_PRINTED (5)

#endif /* _SEQ_FILE_H_ */

#ifndef _PWMOB_H_
#define _PWMOB_H_

#define PWMOB_DATA_MAX 100

struct pwmob_dev {
	char *p_data; /* pointer to the memory allocated */
	size_t data_size;
	struct cdev cdev; /* Char device structure		*/
};

#endif /* _PWMOB_H_ */