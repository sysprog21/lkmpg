/*
    procfs3.c
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

#define PROCFS_MAX_SIZE         2048
#define PROCFS_ENTRY_FILENAME   "buffer2k"

struct proc_dir_entry *Our_Proc_File;
static char procfs_buffer[PROCFS_MAX_SIZE];
static unsigned long procfs_buffer_size = 0;

static ssize_t procfs_read(struct file *filp, char *buffer,
                           size_t length, loff_t *offset)
{
    static int finished = 0;
    if(finished)
    {
        pr_debug("procfs_read: END\n");
        finished = 0;
        return 0;
    }
    finished = 1;
    if(copy_to_user(buffer, procfs_buffer, procfs_buffer_size))
        return -EFAULT;
    pr_debug("procfs_read: read %lu bytes\n", procfs_buffer_size);
    return procfs_buffer_size;
}
static ssize_t procfs_write(struct file *file,  const char *buffer,
                            size_t len, loff_t *off)
{
    if(len>PROCFS_MAX_SIZE)
        procfs_buffer_size = PROCFS_MAX_SIZE;
    else
        procfs_buffer_size = len;
    if(copy_from_user(procfs_buffer, buffer, procfs_buffer_size))
        return -EFAULT;
    pr_debug("procfs_write: write %lu bytes\n", procfs_buffer_size);
    return procfs_buffer_size;
}
int procfs_open(struct inode *inode, struct file *file)
{
    try_module_get(THIS_MODULE);
    return 0;
}
int procfs_close(struct inode *inode, struct file *file)
{
    module_put(THIS_MODULE);
    return 0;
}

static struct proc_ops File_Ops_4_Our_Proc_File = {
    .proc_read       = procfs_read,
    .proc_write      = procfs_write,
    .proc_open       = procfs_open,
    .proc_release    = procfs_close,
};

int init_module()
{
    Our_Proc_File = proc_create(PROCFS_ENTRY_FILENAME, 0644, NULL,&File_Ops_4_Our_Proc_File);
    if(Our_Proc_File == NULL)
    {
        remove_proc_entry(PROCFS_ENTRY_FILENAME, NULL);
        pr_debug("Error: Could not initialize /proc/%s\n", PROCFS_ENTRY_FILENAME);
        return -ENOMEM;
    }
    proc_set_size(Our_Proc_File, 80);
    proc_set_user(Our_Proc_File,  GLOBAL_ROOT_UID, GLOBAL_ROOT_GID);

    pr_debug("/proc/%s created\n", PROCFS_ENTRY_FILENAME);
    return 0;
}
void cleanup_module()
{
    remove_proc_entry(PROCFS_ENTRY_FILENAME, NULL);
    pr_debug("/proc/%s removed\n", PROCFS_ENTRY_FILENAME);
}

MODULE_LICENSE("GPL");
