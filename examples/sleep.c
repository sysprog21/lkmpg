/*
 *  sleep.c - create a /proc file, and if several processes try to open it at
 *  the same time, put all but one to sleep
 */

#include <linux/kernel.h>       /* We're doing kernel work */
#include <linux/module.h>       /* Specifically, a module */
#include <linux/proc_fs.h>      /* Necessary because we use proc fs */
#include <linux/sched.h>        /* For putting processes to sleep and
                                   waking them up */
#include <linux/uaccess.h>      /* for get_user and put_user */

/*
 * The module's file functions
 */

/*
 * Here we keep the last message received, to prove that we can process our
 * input
 */
#define MESSAGE_LENGTH 80
static char Message[MESSAGE_LENGTH];

static struct proc_dir_entry *Our_Proc_File;
#define PROC_ENTRY_FILENAME "sleep"

/*
 * Since we use the file operations struct, we can't use the special proc
 * output provisions - we have to use a standard read function, which is this
 * function
 */
static ssize_t module_output(struct file *file, /* see include/linux/fs.h   */
                             char *buf, /* The buffer to put data to
                                           (in the user segment)    */
                             size_t len,        /* The length of the buffer */
                             loff_t * offset)
{
    static int finished = 0;
    int i;
    char message[MESSAGE_LENGTH + 30];

    /*
     * Return 0 to signify end of file - that we have nothing
     * more to say at this point.
     */
    if (finished) {
        finished = 0;
        return 0;
    }

    /*
     * If you don't understand this by now, you're hopeless as a kernel
     * programmer.
     */
    sprintf(message, "Last input:%s\n", Message);
    for (i = 0; i < len && message[i]; i++)
        put_user(message[i], buf + i);

    finished = 1;
    return i;               /* Return the number of bytes "read" */
}

/*
 * This function receives input from the user when the user writes to the /proc
 * file.
 */
static ssize_t module_input(struct file *file,  /* The file itself */
                            const char *buf,    /* The buffer with input */
                            size_t length,      /* The buffer's length */
                            loff_t * offset)    /* offset to file - ignore */
{
    int i;

    /*
     * Put the input into Message, where module_output will later be
     * able to use it
     */
    for (i = 0; i < MESSAGE_LENGTH - 1 && i < length; i++)
        get_user(Message[i], buf + i);
    /*
     * we want a standard, zero terminated string
     */
    Message[i] = '\0';

    /*
     * We need to return the number of input characters used
     */
    return i;
}

/*
 * 1 if the file is currently open by somebody
 */
int Already_Open = 0;

/*
 * Queue of processes who want our file
 */
DECLARE_WAIT_QUEUE_HEAD(WaitQ);
/*
 * Called when the /proc file is opened
 */
static int module_open(struct inode *inode, struct file *file)
{
    /*
     * If the file's flags include O_NONBLOCK, it means the process doesn't
     * want to wait for the file.  In this case, if the file is already
     * open, we should fail with -EAGAIN, meaning "you'll have to try
     * again", instead of blocking a process which would rather stay awake.
     */
    if ((file->f_flags & O_NONBLOCK) && Already_Open)
        return -EAGAIN;

    /*
     * This is the correct place for try_module_get(THIS_MODULE) because
     * if a process is in the loop, which is within the kernel module,
     * the kernel module must not be removed.
     */
    try_module_get(THIS_MODULE);

    /*
     * If the file is already open, wait until it isn't
     */

    while (Already_Open) {
        int i, is_sig = 0;

        /*
         * This function puts the current process, including any system
         * calls, such as us, to sleep.  Execution will be resumed right
         * after the function call, either because somebody called
         * wake_up(&WaitQ) (only module_close does that, when the file
         * is closed) or when a signal, such as Ctrl-C, is sent
         * to the process
         */
        wait_event_interruptible(WaitQ, !Already_Open);

        /*
         * If we woke up because we got a signal we're not blocking,
         * return -EINTR (fail the system call).  This allows processes
         * to be killed or stopped.
         */

        /*
         * Emmanuel Papirakis:
         *
         * This is a little update to work with 2.2.*.  Signals now are contained in
         * two words (64 bits) and are stored in a structure that contains an array of
         * two unsigned longs.  We now have to make 2 checks in our if.
         *
         * Ori Pomerantz:
         *
         * Nobody promised me they'll never use more than 64 bits, or that this book
         * won't be used for a version of Linux with a word size of 16 bits.  This code
         * would work in any case.
         */
        for (i = 0; i < _NSIG_WORDS && !is_sig; i++)
            is_sig =
                current->pending.signal.sig[i] & ~current->
                blocked.sig[i];

        if (is_sig) {
            /*
             * It's important to put module_put(THIS_MODULE) here,
             * because for processes where the open is interrupted
             * there will never be a corresponding close. If we
             * don't decrement the usage count here, we will be
             * left with a positive usage count which we'll have no
             * way to bring down to zero, giving us an immortal
             * module, which can only be killed by rebooting
             * the machine.
             */
            module_put(THIS_MODULE);
            return -EINTR;
        }
    }

    /*
     * If we got here, Already_Open must be zero
     */

    /*
     * Open the file
     */
    Already_Open = 1;
    return 0;               /* Allow the access */
}

/*
 * Called when the /proc file is closed
 */
int module_close(struct inode *inode, struct file *file)
{
    /*
     * Set Already_Open to zero, so one of the processes in the WaitQ will
     * be able to set Already_Open back to one and to open the file. All
     * the other processes will be called when Already_Open is back to one,
     * so they'll go back to sleep.
     */
    Already_Open = 0;

    /*
     * Wake up all the processes in WaitQ, so if anybody is waiting for the
     * file, they can have it.
     */
    wake_up(&WaitQ);

    module_put(THIS_MODULE);

    return 0;               /* success */
}

/*
 * Structures to register as the /proc file, with pointers to all the relevant
 * functions.
 */

/*
 * File operations for our proc file. This is where we place pointers to all
 * the functions called when somebody tries to do something to our file. NULL
 * means we don't want to deal with something.
 */
static struct proc_ops File_Ops_4_Our_Proc_File = {
    .proc_read = module_output,  /* "read" from the file */
    .proc_write = module_input,  /* "write" to the file */
    .proc_open = module_open,    /* called when the /proc file is opened */
    .proc_release = module_close,        /* called when it's closed */
};

/*
 * Module initialization and cleanup
 */

/*
 * Initialize the module - register the proc file
 */

int init_module()
{
    Our_Proc_File = proc_create(PROC_ENTRY_FILENAME, 0644, NULL, &File_Ops_4_Our_Proc_File);
    if(Our_Proc_File == NULL)
    {
        remove_proc_entry(PROC_ENTRY_FILENAME, NULL);
        pr_debug("Error: Could not initialize /proc/%s\n", PROC_ENTRY_FILENAME);
        return -ENOMEM;
    }
    proc_set_size(Our_Proc_File, 80);
    proc_set_user(Our_Proc_File,  GLOBAL_ROOT_UID, GLOBAL_ROOT_GID);

    pr_info("/proc/test created\n");

    return 0;
}

/*
 * Cleanup - unregister our file from /proc.  This could get dangerous if
 * there are still processes waiting in WaitQ, because they are inside our
 * open function, which will get unloaded. I'll explain how to avoid removal
 * of a kernel module in such a case in chapter 10.
 */
void cleanup_module()
{
    remove_proc_entry(PROC_ENTRY_FILENAME, NULL);
    pr_debug("/proc/%s removed\n", PROC_ENTRY_FILENAME);
}

MODULE_LICENSE("GPL");
