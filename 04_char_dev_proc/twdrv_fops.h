#ifndef _TWDRV_FOPS_H_
#define _TWDRV_FOPS_H_

struct file_operations;

int twdrv_fops_init(struct file_operations *fops);

#endif /* _TWDRV_FOPS_H_ */