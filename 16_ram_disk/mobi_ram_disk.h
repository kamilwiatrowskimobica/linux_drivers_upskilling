#ifndef _MOBI_RAM_DISK_H_
#define _MOBI_RAM_DISK_H_

#define MOBI_BLOCK_MAJOR	240
#define MOBI_BLOCK_MINOR	0
#define MOBI_BLKDEV_NAME	"mobi_block_dev"
#define MOBI_DISK_NAME      "my_ram_disk" //name limited to DISK_NAME_LEN
#define NR_SECTORS		    128
#define KERNEL_SECTOR_SIZE	512

#endif /* _MOBI_RAM_DISK_H_ */