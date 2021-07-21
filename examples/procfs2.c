/**
 *  procfs2.c -  create a "file" in /proc
 *
 */

#include <linux/kernel.h>  /* We're doing kernel work */
#include <linux/module.h>  /* Specifically, a module */
#include <linux/proc_fs.h> /* Necessary because we use the proc fs */
#include <linux/uaccess.h> /* for copy_from_user */

#define PROCFS_MAX_SIZE 1024
#define PROCFS_NAME "buffer1k"

/**
 * This structure hold information about the /proc file
 *
 */
static struct proc_dir_entry *Our_Proc_File;

/**
 * The buffer used to store character for this module
 *
 */
static char procfs_buffer[PROCFS_MAX_SIZE];

/**
 * The size of the buffer
 *
 */
static unsigned long procfs_buffer_size = 0;

/**
 * This function is called then the /proc file is read
 *
 */
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


/**
 * This function is called with the /proc file is written
 *
 */
static ssize_t procfile_write(struct file *file,
                              const char *buff,
                              size_t len,
                              loff_t *off)
{
    procfs_buffer_size = len;
    if (procfs_buffer_size > PROCFS_MAX_SIZE)
        procfs_buffer_size = PROCFS_MAX_SIZE;

    if (copy_from_user(procfs_buffer, buff, procfs_buffer_size))
        return -EFAULT;

    procfs_buffer[procfs_buffer_size] = '\0';
    return procfs_buffer_size;
}

static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
    .proc_write = procfile_write,
};

/**
 *This function is called when the module is loaded
 *
 */
int init_module()
{
    Our_Proc_File = proc_create(PROCFS_NAME, 0644, NULL, &proc_file_fops);
    if (NULL == Our_Proc_File) {
        proc_remove(Our_Proc_File);
        pr_alert("Error:Could not initialize /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }

    pr_info("/proc/%s created\n", PROCFS_NAME);
    return 0;
}

/**
 *This function is called when the module is unloaded
 *
 */
void cleanup_module()
{
    proc_remove(Our_Proc_File);
    pr_info("/proc/%s removed\n", PROCFS_NAME);
}

MODULE_LICENSE("GPL");
