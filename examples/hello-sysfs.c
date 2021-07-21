/*
 * hello-sysfs.c sysfs example
 */

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bob Mottram");

static struct kobject *mymodule;

/* the variable you want to be able to change */
static int myvariable = 0;

static ssize_t myvariable_show(struct kobject *kobj,
                               struct kobj_attribute *attr,
                               char *buf)
{
    return sprintf(buf, "%d\n", myvariable);
}

static ssize_t myvariable_store(struct kobject *kobj,
                                struct kobj_attribute *attr,
                                char *buf, size_t count)
{
    sscanf(buf, "%du", &myvariable);
    return count;
}


static struct kobj_attribute myvariable_attribute =
    __ATTR(myvariable, 0660, myvariable_show,
           (void*)myvariable_store);

static int __init mymodule_init (void)
{
    int error = 0;

    pr_info("mymodule: initialised\n");

    mymodule =
        kobject_create_and_add("mymodule", kernel_kobj);
    if (!mymodule)
        return -ENOMEM;

    error = sysfs_create_file(mymodule, &myvariable_attribute.attr);
    if (error) {
        pr_info("failed to create the myvariable file " \
                "in /sys/kernel/mymodule\n");
    }

    return error;
}

static void __exit mymodule_exit (void)
{
    pr_info("mymodule: Exit success\n");
    kobject_put(mymodule);
}

module_init(mymodule_init);
module_exit(mymodule_exit);
