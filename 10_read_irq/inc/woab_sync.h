#ifndef _WOAB_SYNC_H_
#define _WOAB_SYNC_H_

#include <linux/poll.h>

#define SYNC_MODE_COMPLETION_TIMEOUT 1000

typedef enum {
	SYNC_MODE_NONE = 0,
	SYNC_MODE_COMPLETION,
	SYNC_MODE_SEMAPHORE,
	SYNC_MODE_POLL
} sync_mode;

struct sync_mode_poll {
	wait_queue_head_t poll_queue;
	int data_readable;
};

int woab_sync_wait_for_completion(struct completion *comp,
				  unsigned long timeout);
void woab_sync_complete(struct completion *comp);
void woab_sync_init_completion(struct completion *comp);

void woab_sync_init_semaphore(struct semaphore *sem);
int woab_sync_down(struct semaphore *sem);
void woab_sync_up(struct semaphore *sem);

void woab_sync_spin_lock_init(spinlock_t *spinlock);
void woab_sync_spin_lock(spinlock_t *spinlock);
void woab_sync_spin_unlock(spinlock_t *spinlock);

void woab_sync_poll_wait_queue_init(struct sync_mode_poll *poll_struct);
void woab_sync_poll_wait_queue_wake_up(struct sync_mode_poll *poll_struct);
void woab_sync_poll_wait_queue_wait(struct sync_mode_poll *poll_struct,
				    struct file *filep,
				    struct poll_table_struct *wait);
void woab_sync_poll_set_data_readable(struct sync_mode_poll *poll_struct,
				      int value);
int woab_sync_poll_get_data_readable(struct sync_mode_poll *poll_struct);

#endif /* _WOAB_SYNC_H_ */