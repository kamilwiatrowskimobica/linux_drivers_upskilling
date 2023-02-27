#ifndef _MOB_SYNC_H_
#define _MOB_SYNC_H_

#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/poll.h>

#define SYNC_COMPLT_TIMEOUT (1000u)

typedef enum {
	SYNC_MODE_NONE,
	SYNC_MODE_SMPHR,
	SYNC_MODE_CMPLT,
	SYNC_MODE_SPNLCK,
	SYNC_MODE_POLL,
} sync_mode;

struct sync_poll {
	wait_queue_head_t queue;
	int data;
};

// Spinlock functions
void spinlock_dynamic_init(spinlock_t *p_sl);
void spinlock_dynamic_take(spinlock_t *p_sl);
void spinlock_dynamic_give(spinlock_t *p_sl);

// Semaphore functions
void mob_sync_semphr_init(struct semaphore *sem);
int mob_sync_semphr_take(struct semaphore *sem);
void mob_sync_semphr_give(struct semaphore *sem);

// Completition
void mob_sync_completition_init(struct completion * compl );
int mob_sync_wait_for_completion(struct completion *comp,
				 unsigned long timeout);
void mob_sync_complete(struct completion *comp);

// Poll
void mob_sync_poll_queue_init(struct sync_poll *poll_str);
void mob_sync_poll_queue_wakeup(struct sync_poll *poll_struct);
void mob_sync_poll_queue_wait(struct sync_poll *poll_str,
			      struct file *p_fl,
			      struct poll_table_struct *p_wait);
void mob_sync_set_data(struct sync_poll *poll_str, int data);
int mob_sync_get_data(struct sync_poll *poll_str);




#endif //_MOB_SYNC_H_