#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("WOAB");
MODULE_DESCRIPTION("Hello world module");
MODULE_VERSION("0.1");

static char *name = "default";
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "The name to display");

static int __init hello_init(void)
{
	printk(KERN_INFO "WOAB: Hello %s!\n", name);
	return 0;
}

static void __exit hello_exit(void)
{
	printk(KERN_INFO "WOAB: Goodbye %s!\n", name);
}

module_init(hello_init);
module_exit(hello_exit);