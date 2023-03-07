/**
  * @file	hello_TR.c
  * @author	Tomasz
  * blah blah blah
  * 
  * 
  * 
  * 
  * 
  */
  
#include <linux/init.h>  
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tomasz Rodziewicz");
MODULE_DESCRIPTION("Prosty Linux driver.");
MODULE_VERSION("0.0000001");

static char *name = "world";
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "The name to display in /var/log/kern.log");

static int __init hello_TR_init(void){
    printk(KERN_INFO "EBB: Hello %s from the TR LKM!\n", name);
    return 0;
}

static void __exit hello_TR_exit(void){
   printk(KERN_INFO "EBB: Goodbye %s from the TR LKM!\n", name);
}

module_init(hello_TR_init);
module_exit(hello_TR_exit);
