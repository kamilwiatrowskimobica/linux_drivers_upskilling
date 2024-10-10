#ifndef SCULL_H
#define SCULL_H


#define DEVICE_NAME "scull"
#define CLASS_NAME  "scull_class"


/* Linked list data structures sizes */
#define SCULL_QUANTUM_DEFAULT_NB 	4000
#define SCULL_QSET_DEFULT_NB    	1000

#define MAJOR_DEFAULT_NB			0
#define MINOR_BASE_DEFAUL_NB		0
#define DEVS_NB_DEFAULT_VAL			4
#define FLAG_DEFAULT_VAL			0

/* Linked list data representation */
struct scull_qset
{
   void **data;
   struct scull_qset *next;
};

/* Device data structure */
struct scull_dev
{
   struct scull_qset *data;   /* pointer to first quantum set */
   int quantum;               /* the current quantum size */
   int qset;                  /* the current array size */
   unsigned long size;        /* amount of data stored here */
   unsigned int access_key;   /* used by sculluid and scullpriv */
   struct mutex lock;         /* mutual exclusion semaphore */
   struct cdev cdev;          /* char device structure */
};

/*
 * Ioctl definitions
 */

/* Use 'p' as magic number */
#define SCULL_IOC_MAGIC  'p'

#define SCULL_IOCRESET    _IO(SCULL_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */
#define SCULL_IOCSQUANTUM _IOW(SCULL_IOC_MAGIC,  1, int)
#define SCULL_IOCSQSET    _IOW(SCULL_IOC_MAGIC,  2, int)
#define SCULL_IOCTQUANTUM _IO(SCULL_IOC_MAGIC,   3)
#define SCULL_IOCTQSET    _IO(SCULL_IOC_MAGIC,   4)
#define SCULL_IOCGQUANTUM _IOR(SCULL_IOC_MAGIC,  5, int)
#define SCULL_IOCGQSET    _IOR(SCULL_IOC_MAGIC,  6, int)
#define SCULL_IOCQQUANTUM _IO(SCULL_IOC_MAGIC,   7)
#define SCULL_IOCQQSET    _IO(SCULL_IOC_MAGIC,   8)
#define SCULL_IOCXQUANTUM _IOWR(SCULL_IOC_MAGIC, 9, int)
#define SCULL_IOCXQSET    _IOWR(SCULL_IOC_MAGIC,10, int)
#define SCULL_IOCHQUANTUM _IO(SCULL_IOC_MAGIC,  11)
#define SCULL_IOCHQSET    _IO(SCULL_IOC_MAGIC,  12)

#define SCULL_IOC_MAXNR 12

/*
 * Macros for debugging purpose
 */
#undef PDEBUG             /* undef it, just in case */
#ifdef SCULL_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "scull: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */


#endif /* SCULL_H */



