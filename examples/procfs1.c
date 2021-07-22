/*
 *  procfs1.c
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif

#define procfs_name "helloworld"

struct proc_dir_entry *Our_Proc_File;


ssize_t procfile_read(struct file *filePointer,
                      char *buffer,
                      size_t buffer_length,
                      loff_t *offset)
{
    int ret = 0;
    if (strlen(buffer) == 0) {
        pr_info("procfile read %s\n", filePointer->f_path.dentry->d_name.name);
        ret = copy_to_user(buffer, "HelloWorld!\n", sizeof("HelloWorld!\n"));
        ret = sizeof("HelloWorld!\n");
    }
    return ret;
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
};
#else
static const struct file_operations proc_file_fops = {
    .read = procfile_read,
};
#endif

int init_module()
{
    Our_Proc_File = proc_create(procfs_name, 0644, NULL, &proc_file_fops);
    if (NULL == Our_Proc_File) {
        proc_remove(Our_Proc_File);
        pr_alert("Error:Could not initialize /proc/%s\n", procfs_name);
        return -ENOMEM;
    }

    pr_info("/proc/%s created\n", procfs_name);
    return 0;
}

void cleanup_module()
{
    proc_remove(Our_Proc_File);
    pr_info("/proc/%s removed\n", procfs_name);
}

MODULE_LICENSE("GPL");
