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
   struct semaphore sem;      /* mutual exclusion semaphore */
   struct cdev cdev;          /* char device structure */
};

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



