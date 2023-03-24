#ifndef _LOCK_API_H_
#define _LOCK_API_H_
#include "mobi_module.h"
#define LOCK_EN

void lock_init(struct mob_dev* mdev, int type)
{
#ifdef LOCK_EN
    if (mdev) {
        if (type & lock_rw_spinlock)
            rwlock_init(&mdev->rw_lock);

        if (type & lock_mutex)
            mutex_init(&mdev->m_lock);
    }
#endif
}

void lock_deinit(struct mob_dev* mdev, int type)
{
#ifdef LOCK_EN
    if (mdev)
        if (type & lock_mutex)
            mutex_destroy(&mdev->m_lock);
#endif
}

int lock(struct mob_dev* mdev, lock_action_t type)
{
#ifdef LOCK_EN
    if (mdev) {
        if (type == lock_action_read)
            read_lock(&mdev->rw_lock);
        else if (type == lock_action_write)
            write_lock(&mdev->rw_lock);
        else if (type == lock_action_mutex) {
            if (mutex_lock_interruptible(&mdev->m_lock) != 0)
                return -EINTR;
        }
    }
#endif
    return 0;
}

int unlock(struct mob_dev* mdev, lock_action_t type)
{
#ifdef LOCK_EN
    if (mdev) {
        if (type == lock_action_read)
            read_unlock(&mdev->rw_lock);
        else if (type == lock_action_write)
            write_unlock(&mdev->rw_lock);
        else if (type == lock_action_mutex) {
            mutex_unlock(&mdev->m_lock);
        }
    }
#endif
    return 0;
}

#endif /* _LOCK_API_H_ */