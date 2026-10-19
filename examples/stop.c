/*
 * stop.c - Illustration of multi filed modules
 */

#include <linux/kernel.h> /* We are doing kernel work */
#include <linux/module.h> /* Specifically, a module  */
#include <linux/init.h> /* Needed for the macros */

static void __exit exit_module(void)
{
    pr_info("Short is the life of a kernel module\n");
}

module_exit(exit_module);

MODULE_LICENSE("GPL");
