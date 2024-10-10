#ifndef ACCESS_OK_VERSION_H_
#define ACCESS_OK_VERSION_H_

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
#define access_ok_wrapper(type,arg,cmd) \
   access_ok(type, arg, cmd)
#else
#define access_ok_wrapper(type,arg,cmd) \
   access_ok(arg, cmd)
#endif

#endif /* ACCESS_OK_VERSION_H_ */
