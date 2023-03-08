#ifndef _WOAB_DEBUG_H_
#define _WOAB_DEBUG_H_

#define WERROR(fmt, args...) printk(KERN_ERR "WOABChar: " fmt, ##args)
#define WINFO(fmt, args...) printk(KERN_INFO "WOABChar: " fmt, ##args)

#undef WDEBUG
#ifdef WOAB_DEBUG
#define WDEBUG(fmt, args...) printk(KERN_DEBUG "WOABChar: " fmt, ##args)
#else
#define WDEBUG(fmt, args...)
#endif /* WOAB_DEBUG */

//#define CHARDEV_TRACE
#ifdef CHARDEV_TRACE
#define WCHARDEBUG(fmt, args...) WDEBUG(fmt, ##args)
#else
#define WCHARDEBUG(fmt, args...)
#endif /* CHARDEV_TRACE */

//#define SEQ_FILE_TRACE
#ifdef SEQ_FILE_TRACE
#define WSEQDEBUG(fmt, args...) WDEBUG(fmt, ##args)
#else
#define WSEQDEBUG(fmt, args...)
#endif /* SEQ_FILE_TRACE */

//#define SYNCHRONISATION_TRACE
#ifdef SYNCHRONISATION_TRACE
#define WSYNDEBUG(fmt, args...) WDEBUG(fmt, ##args)
#else
#define WSYNDEBUG(fmt, args...)
#endif /* COMPLETION_TRACE */

#define LIST_TRACE
#ifdef LIST_TRACE
#define WLISTDEBUG(fmt, args...) WDEBUG(fmt, ##args)
#else
#define LIST_TRACE(fmt, args...)
#endif /* COMPLETION_TRACE */

#endif /* _WOAB_DEBUG_H_ */