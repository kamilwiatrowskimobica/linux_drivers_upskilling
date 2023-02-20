#ifndef _WOAB_SYNC_H_
#define _WOAB_SYNC_H_

#define SYNC_MODE_COMPLETION_TIMEOUT 1000

typedef enum {
	SYNC_MODE_NONE = 0,
	SYNC_MODE_COMPLETION,
	SYNC_MODE_SEMAPHORE
} sync_mode;

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

#endif /* _WOAB_SYNC_H_ */