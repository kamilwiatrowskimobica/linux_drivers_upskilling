#include <linux/sched.h>
#include <linux/sched.h>
#include "mob_sync.h"
#include "mob_debug.h"

void mob_sync_semphr_init(struct semaphore *sem)
{
	sema_init(sem, 1);
}

int mob_sync_semphr_take(struct semaphore *sem)
{
	MOB_PRINT("Process %d/%s is waiting for resource\n", current->pid,
		  current->comm);

	if (down_interruptible(sem)) {
		MOB_PRINT("Process %d/%s interrupted\n", current->pid,
			  current->comm);
		return -ERESTARTSYS;
	}

	MOB_PRINT("Process %d /%s is acquiring resource\n", current->pid,
		  current->comm);

	return 0;
}

void mob_sync_semphr_give(struct semaphore *sem)
{
	MOB_PRINT("Process %d named %s is releasing resource\n", current->pid,
		  current->comm);
	up(sem);
}

void spinlock_dynamic_init(spinlock_t *p_sl)
{
	spin_lock_init(p_sl);
}

void spinlock_dynamic_take(spinlock_t *p_sl)
{
	spin_lock(p_sl);
}

void spinlock_dynamic_give(spinlock_t *p_sl)
{
	spin_unlock(p_sl);
}
