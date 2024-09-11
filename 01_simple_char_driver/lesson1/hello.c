#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pawel");
MODULE_DESCRIPTION("A simple Linux driver for the Ubuntu.");
MODULE_VERSION("0.1");
 
static char *name = "world";
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "The name to display in /var/log/kern.log");
 
static int __init helloUbuntu_init(void){
   printk(KERN_INFO "EBB: Hello %s from the Ubuntu LKM!\n", name);
   return 0;
}
 
static void __exit helloUbuntu_exit(void){
   printk(KERN_INFO "EBB: Goodbye %s from the Ubuntu LKM!\n", name);
}
 
module_init(helloUbuntu_init);
module_exit(helloUbuntu_exit);
