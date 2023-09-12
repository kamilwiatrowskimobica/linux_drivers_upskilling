#ifndef _TWDRV_FOPS_H_
#define _TWDRV_FOPS_H_

#include <linux/fs.h> 

int twdrv_fops_init(struct file_operations *fops);

#endif /* _TWDRV_FOPS_H_ */