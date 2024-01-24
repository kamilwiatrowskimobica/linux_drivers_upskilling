/**
 * @file   mem_char_drv.h
 * @author Michal Binek
 * @date   23 January 2024
 * @version 0.1
 * @brief   Memory char driver
 */

#ifndef _MEM_CHAR_DRV_H_
#define _MEM_CHAR_DRV_H_

#define MCD_DEVICE_COUNT 8
#define MCD_DEVICE_MAX_SIZE 32

struct mcd_dev {
	char *data;
        size_t size;
	struct cdev cdev;
};

#endif // _MEM_CHAR_DRV_H_