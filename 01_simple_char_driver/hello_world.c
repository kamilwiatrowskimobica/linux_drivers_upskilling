#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

/* Module info */ 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michał Aleksiński");
MODULE_DESCRIPTION("Hello world");
MODULE_VERSION("0.1");

/**
 * @brief Module init
 */
static int __init hello_world_init(void) {
        printk(KERN_INFO "hello_world: driver init\n");
        return 0;
}

/**
 * @brief Module cleanup
 */
static void __exit hello_world_exit(void) {
        printk(KERN_INFO "hello_world: driver exit\n");
}

module_init(hello_world_init);
module_exit(hello_world_exit);