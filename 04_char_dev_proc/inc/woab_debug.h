#ifndef _WOAB_DEBUG_H_
#define _WOAB_DEBUG_H_

#undef WDEBUG
#ifdef WOAB_DEBUG
#	define WDEBUG(fmt, args...) printk( KERN_DEBUG "WOABChar: " fmt, ## args)
#else
#	define PDEBUG(fmt, args...)
#endif /* WOAB_DEBUG */

#define SEQ_FILE_TRACE
#ifdef SEQ_FILE_TRACE
#	define WSEQDEBUG(fmt, args...) WDEBUG(fmt, ## args)
#else
#	define WSEQDEBUG(fmt, args...)
#endif /* SEQ_FILE_TRACE */

#endif /* _WOAB_DEBUG_H_ */