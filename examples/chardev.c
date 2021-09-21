/*
 * chardev.c: Creates a read-only char device that says how many times
 * you have read from the dev file
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>

/* Structure for every task's offset */
typedef struct element{
    int pid;
    long offset;
    struct element *next;
}list_ele_t;

/*  Prototypes - this would normally go in a .h file */
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char __user *, size_t,
                            loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "chardev" /* Dev name as it appears in /proc/devices   */
#define BUF_LEN 80 /* Max length of the message from the device */

/* Global variables are declared as static, so are global within the file. */

static int major; /* major number assigned to our device driver */
/* Is device open? Used to prevent multiple access to device */
static atomic_t already_open = ATOMIC_INIT(0);
static char msg[BUF_LEN]; /* The msg the device will give when asked */

static list_ele_t *taskList = NULL;
struct mutex etx_mutex; 

static struct class *cls;

static struct file_operations chardev_fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release,
};

static int __init chardev_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &chardev_fops);

    if (major < 0) {
        pr_alert("Registering char device failed with %d\n", major);
        return major;
    }

    pr_info("I was assigned major number %d.\n", major);

    cls = class_create(THIS_MODULE, DEVICE_NAME);
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

    pr_info("Device created on /dev/%s\n", DEVICE_NAME);
    mutex_init(&etx_mutex);

    return SUCCESS;
}

static void __exit chardev_exit(void)
{
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);

    /* Unregister the device */
    unregister_chrdev(major, DEVICE_NAME);
}

/* Methods */

/* Called when a process tries to open the device file, like
 * "sudo cat /dev/chardev"
 */
static int device_open(struct inode *inode, struct file *file)
{
    static int counter = 0;

    if (atomic_cmpxchg(&already_open, 0, 1))
        return -EBUSY;

    sprintf(msg, "I already told you %d times Hello world!\n", counter++);
    try_module_get(THIS_MODULE);

    return SUCCESS;
}

void freeTaskList(void){
    list_ele_t *tmp = taskList;
    if(taskList == NULL)
        return;
    do{
        taskList = tmp;
        tmp = tmp->next;
        kfree(taskList);
    }while(tmp);
    return;
}

/* Called when a process closes the device file. */
static int device_release(struct inode *inode, struct file *file)
{

    freeTaskList();
    atomic_set(&already_open, 0); /* We're now ready for our next caller */

    /* Decrement the usage count, or else once you opened the file, you will
     * never get get rid of the module.
     */
    module_put(THIS_MODULE);

    return SUCCESS;
}

int findTaskOffset(int currentPid){
    list_ele_t *tmp = taskList;
    if(tmp == NULL)
        return -1;
    do{
        if(currentPid == tmp->pid){
            return tmp->offset;
        }
        tmp = tmp->next;
    }while(tmp);
    return -1;
}

int insertTaskOffset(int currentPid){
    list_ele_t *tmp = taskList;
    list_ele_t *new = kmalloc(sizeof(list_ele_t), GFP_ATOMIC);
    if(new < 0)
        return false;
    new->pid = currentPid;
    new->next = NULL;

    if(!tmp)
        taskList = new;
    else{
        while(tmp->next)
            tmp = tmp->next;
        tmp->next = new;
    }
    return true;
}

bool setTaskOffset(int currentPid, int newOffset){
    list_ele_t *tmp = taskList;
    if(tmp == NULL)
        return false;
    do{
        if(currentPid == tmp->pid){
            tmp->offset = newOffset;
            printk("[DEBUG] set the new offset. PID: %d off:%d\n", currentPid, newOffset);
            return true;
        }
        tmp = tmp->next;
    }while(tmp);
    return false;
}


/* Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp, /* see include/linux/fs.h   */
                           char __user *buffer, /* buffer to fill with data */
                           size_t length, /* length of the buffer     */
                           loff_t *offset)
{
    /* Number of bytes actually written to the buffer */
    char *msg_ptr = msg;
    
    /* Now we know which task is reading the device. */
    int currentPid = task_pid_nr(current);
    int currentOffset;
    bool ret;
    int bytes_read =0;

    mutex_lock(&etx_mutex);
    currentOffset = findTaskOffset(currentPid);

    if(currentOffset == -1){
        /* A new task is reading the device, so just add a new element in the list.*/
        
        ret = insertTaskOffset(currentPid);
        printk("[DEBUG] add the pid %d entry.\n", currentPid);
        currentOffset = 0;
    }
    mutex_unlock(&etx_mutex);   

    /* If we are at the end of message, reset the offset and
     * return 0 (which signifies end of file).
     */

    if (!*(msg_ptr + currentOffset)) {
        currentOffset = 0;
        return 0;
    }

    /* Actually put the data into the buffer */
    while (length && *(msg_ptr + currentOffset)) {
        /* The buffer is in the user data segment, not the kernel
         * segment so "*" assignment won't work.  We have to use
         * put_user which copies data from the kernel data segment to
         * the user data segment.
         */
        put_user(*((msg_ptr++)+currentOffset), buffer++);

        length--;
        bytes_read++;
    }
    /* Add bytes_read to the current task offset. */
    setTaskOffset(currentPid, currentOffset+bytes_read);

    /* Most read functions return the number of bytes put into the buffer. */
    return bytes_read;
}

/* Called when a process writes to dev file: echo "hi" > /dev/hello */
static ssize_t device_write(struct file *filp, const char __user *buff,
                            size_t len, loff_t *off)
{
    pr_alert("Sorry, this operation is not supported.\n");
    return -EINVAL;
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
