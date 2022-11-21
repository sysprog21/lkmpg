/**
 * syscall-ftrace.c
 * 
 * System call "stealing" with Ftrace
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/unistd.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
/** This is what we're using here. */
#include <linux/ftrace.h>

MODULE_LICENSE("GPL");

#define MAX_FILENAME_SIZE 200

/* UID we want to spy on - will be filled from the command line. */
static int uid;
module_param(uid, int, 0644);

/**
 * This is a helper structure that housekeeps all information
 * needed for hooking. Usage with `PREPARE_HOOK` is recommended.
 * 
 * Example:
 * static ftrace_hook_t sys_clone_hook = PREPARE_HOOK(__NR_openat, my_sys_clone, &orig_sys_clone)
 */
typedef struct ftrace_hook {
    unsigned long nr;      // syscall name
    void* new;             // hook function
    void* orig;            // original function

    unsigned long address; // address to the original function
    struct ftrace_ops ops; // ftrace structure
} ftrace_hook_t;

#define PREPARE_HOOK(_nr, _hook, _orig) { \
    .nr   = (_nr),   \
    .new  = (_hook), \
    .orig = (_orig)  \
}

unsigned long **sys_call_table;

/**
 * For the sake of simplicity, only the kprobe method is included.
 * If you want to know more about different methods to get
 * kallsyms_lookup_name, see syscall.c.
 */
static int resolve_address(ftrace_hook_t *hook)
{
    static struct kprobe kp = {
        .symbol_name = "kallsyms_lookup_name"
    };
    unsigned long (*kallsyms_lookup_name)(const char *name);
    register_kprobe(&kp);
    kallsyms_lookup_name = (unsigned long (*)(const char *))kp.addr;
    unregister_kprobe(&kp);

    if (kallsyms_lookup_name) pr_info("[syscall-ftrace] kallsyms_lookup_name is found at 0x%lx\n", (unsigned long)kallsyms_lookup_name);
    else {
        pr_err("[syscall-ftrace] kallsyms_lookup_name is not found!\n");
        return -1;
    }

    sys_call_table = (unsigned long **)kallsyms_lookup_name("sys_call_table");
    if (sys_call_table) pr_info("[syscall-ftrace] sys_call_table is found at 0x%lx\n", (unsigned long)sys_call_table);
    else {
        pr_err("[syscall-ftrace] sys_call_table is not found!\n");
        return -1;
    }

    hook->address = (unsigned long)sys_call_table[hook->nr];
    *((unsigned long*) hook->orig) = hook->address;
    return 0;
}

/**
 * This is where the magic happens.
 * 
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
static void notrace ftrace_thunk(unsigned long ip, unsigned long parent_ip, struct ftrace_ops *ops, struct ftrace_regs *fregs)
{
    ftrace_hook_t *hook = container_of(ops, ftrace_hook_t, ops);
    if (!within_module(parent_ip, THIS_MODULE)) fregs->regs.ip = (unsigned long) hook->new;
}

#else
static void notrace ftrace_thunk(unsigned long ip, unsigned long parent_ip, struct ftrace_ops *ops, struct pt_regs *regs)
{
    ftrace_hook_t *hook = container_of(ops, ftrace_hook_t, ops);
    if (!within_module(parent_ip, THIS_MODULE)) regs->ip = (unsigned long) hook->new;
}

#endif /** Version >= v5.11 */

int install_hook(ftrace_hook_t *hook)
{
    int err;
    err = resolve_address(hook);
    if (err) return err;

    hook->ops.func = ftrace_thunk;
    hook->ops.flags = FTRACE_OPS_FL_SAVE_REGS | FTRACE_OPS_FL_IPMODIFY;
    err = ftrace_set_filter_ip(&hook->ops, hook->address, 0, 0);
    if (err) {
        pr_err("[syscall-ftrace] ftrace_set_filter_ip() failed: %d\n", err);
        return err;
    }

    err = register_ftrace_function(&hook->ops);
    if (err) {
        pr_err("[syscall-ftrace] register_ftrace_function() failed: %d\n", err);
        return err;
    }

    return 0;
}

void remove_hook(ftrace_hook_t *hook)
{
    int err;
    err = unregister_ftrace_function(&hook->ops);
    if (err) pr_err("[syscall-ftrace] unregister_ftrace_function() failed: %d\n", err);

    err = ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0);
    if (err) pr_err("[syscall-ftrace] ftrace_set_filter_ip() failed: %d\n", err);
}

/** For some reason the kernel segfaults when the arguments are expanded. */
static asmlinkage long (*original_call)(struct pt_regs *regs);
static asmlinkage long our_sys_openat(struct pt_regs *regs)
{
    char *kfilename;
    kfilename = kmalloc(GFP_KERNEL, MAX_FILENAME_SIZE*sizeof(char));
    if (!kfilename) return original_call(regs);

    if (copy_from_user(kfilename, (char __user *)regs->si, MAX_FILENAME_SIZE) < 0) {
        kfree(kfilename);
        return original_call(regs);
    }

    pr_info("[syscall-ftrace] File opened by UID %d: %s\n", uid, kfilename);
    kfree(kfilename);

    return original_call(regs);
}

static ftrace_hook_t sys_openat_hook = PREPARE_HOOK(__NR_openat, our_sys_openat, &original_call);

static int __init syscall_ftrace_start(void)
{
    int err;
    err = install_hook(&sys_openat_hook);
    if (err) return err;
    pr_info("[syscall-ftrace] hooked, spying on uid %d\n", uid);
    return 0;
}

static void __exit syscall_ftrace_end(void)
{
    remove_hook(&sys_openat_hook);
    pr_info("[syscall-ftrace] removed\n");
}

module_init(syscall_ftrace_start);
module_exit(syscall_ftrace_end);
