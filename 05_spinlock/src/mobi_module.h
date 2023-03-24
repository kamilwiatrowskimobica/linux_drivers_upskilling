#ifndef _MOBI_MODULE_H_
#define _MOBI_MODULE_H_

#include <linux/cdev.h>
#include <linux/rwlock.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>

struct mob_dev {
    size_t id;
    size_t msg_size;
    struct mutex m_lock;
    rwlock_t rw_lock;
    spinlock_t s_lock;
    struct cdev cdev;
    char *msg;
};

typedef enum {
    lock_none = 0,
    lock_mutex = 0x01,
    lock_rw_spinlock = 0x02,
    lock_spinlock = 0x04
} lock_type_t;

typedef enum {
    lock_action_none = 0,
    lock_action_read,
    lock_action_write,
    lock_action_mutex,
    lock_action_spinlock
} lock_action_t;

// void lock_init(struct mob_dev*);
// void lock(struct mob_dev*, lock_action_t);
// void unlock(struct mob_dev*, lock_action_t);

#endif /* _MOBI_MODULE_H_ */