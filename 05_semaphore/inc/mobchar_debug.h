#ifndef _MOBCHAR_DEBUG_H_
#define _MOBCHAR_DEBUG_H_

#define PRINT_PREFIX "MobChar: "

#undef DEBUG_PRINT
#ifdef DEBUG_MODE_PRINT
        #ifdef   __KERNEL__
                #define DEBUG_PRINT(fmt, args...) printk(KERN_DEBUG PRINT_PREFIX fmt, ## args)
        #else
                #define DEBUG_PRINT(fmt, args...) fprintf(stderr, fmt, ## args)
        #endif
#else
        #define DEBUG_PRINT(fmt, args...) 
#endif

#endif //_MOBCHAR_DEBUG_H_