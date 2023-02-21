#ifndef _MOB_SYNC_H_
#define _MOB_SYNC_H_

#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/spinlock.h> 

typedef enum {
    SYNC_MODE_NONE,
    SYNC_MODE_SMPHR,
    SYNC_MODE_SPNLCK,
} sync_mode;

// Spinlock functions
void spinlock_dynamic_init(spinlock_t *p_sl);
void spinlock_dynamic_take(spinlock_t *p_sl);
void spinlock_dynamic_give(spinlock_t *p_sl);

// Semaphore functions
void mob_sync_semphr_init(struct semaphore* sem);
int mob_sync_semphr_take(struct semaphore* sem);
void mob_sync_semphr_give(struct semaphore* sem);

#endif //_MOB_SYNC_H_