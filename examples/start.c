/*
 * start.c - Illustration of multi filed modules
 */

#include <linux/kernel.h> /* We are doing kernel work */
#include <linux/module.h> /* Specifically, a module */
#include <linux/init.h> /* Needed for the macros */

static int __init start_init(void)
{
    pr_info("Hello, world - this is the kernel speaking\n");
    return 0;
}

module_init(start_init);

MODULE_LICENSE("GPL");
