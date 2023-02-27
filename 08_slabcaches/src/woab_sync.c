#include <linux/completion.h>
#include <linux/semaphore.h>
#include <linux/sched.h>

#include <woab_debug.h>
#include <woab_sync.h>

int woab_sync_wait_for_completion(struct completion *comp,
				  unsigned long timeout)
{
	long wait_for_completion_ret = 0;

	WSYNDEBUG("Process %d named %s is waiting for completion\n",
		  current->pid, current->comm);
	wait_for_completion_ret =
		wait_for_completion_interruptible_timeout(comp, timeout);
	if (0 == wait_for_completion_ret) {
		WSYNDEBUG("Process %d named %s timeouted\n", current->pid,
			  current->comm);
		goto no_data;
	} else if (-ERESTARTSYS == wait_for_completion_ret) {
		WSYNDEBUG("Process %d named %s interruped\n", current->pid,
			  current->comm);
		goto no_data;
	} else {
		WSYNDEBUG(
			"Process %d named %s succesfuly notified. Time left = %ld\n",
			current->pid, current->comm, wait_for_completion_ret);
	}

	return 0;

no_data:
	return -1;
}

void woab_sync_complete(struct completion *comp)
{
	WSYNDEBUG("Process %d named %s completed writting\n", current->pid,
		  current->comm);
	complete(comp);
}

void woab_sync_init_completion(struct completion *comp)
{
	init_completion(comp);
}

void woab_sync_init_semaphore(struct semaphore *sem)
{
	sema_init(sem, 1);
}

int woab_sync_down(struct semaphore *sem)
{
	WSYNDEBUG("Process %d named %s is waiting for resource\n", current->pid,
		  current->comm);
	if (down_interruptible(sem)) {
		WSYNDEBUG("Process %d named %s interruped\n", current->pid,
			  current->comm);
		return -ERESTARTSYS;
	}
	WSYNDEBUG("Process %d named %s is acquiring resource\n", current->pid,
		  current->comm);

	return 0;
}

void woab_sync_up(struct semaphore *sem)
{
	WSYNDEBUG("Process %d named %s is releasing resource\n", current->pid,
		  current->comm);
	up(sem);
}

void woab_sync_spin_lock_init(spinlock_t *spinlock)
{
	spin_lock_init(spinlock);
}

void woab_sync_spin_lock(spinlock_t *spinlock)
{
	spin_lock(spinlock);
}

void woab_sync_spin_unlock(spinlock_t *spinlock)
{
	spin_unlock(spinlock);
}

void woab_sync_poll_wait_queue_init(struct sync_mode_poll *poll_struct)
{
	init_waitqueue_head(&poll_struct->poll_queue);
	poll_struct->data_readable = 0;
}

void woab_sync_poll_wait_queue_wake_up(struct sync_mode_poll *poll_struct)
{
	wake_up(&poll_struct->poll_queue);
	poll_struct->data_readable = 1;
}

void woab_sync_poll_wait_queue_wait(struct sync_mode_poll *poll_struct,
				    struct file *filep,
				    struct poll_table_struct *wait)
{
	poll_wait(filep, &poll_struct->poll_queue, wait);
}

void woab_sync_poll_set_data_readable(struct sync_mode_poll *poll_struct,
				      int value)
{
	poll_struct->data_readable = value;
}

int woab_sync_poll_get_data_readable(struct sync_mode_poll *poll_struct)
{
	return poll_struct->data_readable;
}
