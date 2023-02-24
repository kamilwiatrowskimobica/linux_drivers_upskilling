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

void mob_sync_completition_init(struct completion * compl )
{
	init_completion(compl);
}

int mob_sync_wait_for_completion(struct completion *comp, unsigned long timeout)
{
	long wait_for_completion_ret = 0;

	wait_for_completion_ret =
		wait_for_completion_interruptible_timeout(comp, timeout);

	if (0 == wait_for_completion_ret) {
		MOB_PRINT("%s - Timeout!\n", current->comm);
		goto no_data;
	} else if (-ERESTARTSYS == wait_for_completion_ret) {
		MOB_PRINT("%s - Interruped\n", current->comm);
		goto no_data;
	} else {
		MOB_PRINT(
			"Process %d named %s succesfuly notified. Time left = %ld\n",
			current->pid, current->comm, wait_for_completion_ret);
	}

	return 0;

no_data:
	return -1;
}

void mob_sync_complete(struct completion *comp)
{
	MOB_PRINT("Completition from process %s\n", current->comm);
	complete(comp);
}