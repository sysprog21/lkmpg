/*
 *  stop.c - Illustration of multi filed modules
 */

#include <linux/kernel.h>       /* We're doing kernel work */
#include <linux/module.h>       /* Specifically, a module  */

void cleanup_module()
{
    pr_info("Short is the life of a kernel module\n");
}

MODULE_LICENSE("GPL");
