#ifndef _MOB_DEBUG_H_
#define _MOB_DEBUG_H_

#undef MOB_PRINT
#ifdef MOB_DEBUG
#   ifdef   __KERNEL__
        // kern space
#       define MOB_PRINT(fmt, args...) printk(KERN_DEBUG "MOBPRINT: " fmt, ## args)
#   else
        // user space 
#       define MOB_PRINT(fmt, args...) fprintf(stderr, fmt, ## args)
#   endif
#else
    	// Empty
#   define  MOB_PRINT(fmt, args...) 
#endif

#endif //_MOB_DEBUG_H_