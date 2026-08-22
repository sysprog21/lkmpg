#include <linux/module.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/delay.h>

struct my_data {
    int value;
};

static struct my_data __rcu *shared_data;
static DEFINE_SPINLOCK(data_lock);
static struct task_struct *reader_thread;

static int reader_thread_fn(void *data)
{
    while (!kthread_should_stop()) {
        struct my_data *p;
        int val;
        bool valid = false;

        rcu_read_lock();
        p = rcu_dereference(shared_data);
        if (p) {
            val = p->value;
            valid = true;
        }
        rcu_read_unlock();

        if (valid)
            pr_info("RCU Reader: value = %d\n", val);
        else
            pr_info("RCU Reader: data is NULL\n");

        msleep(500);
    }
    return 0;
}

static void update_data(int new_value)
{
    struct my_data *new_p, *old_p;

    new_p = kmalloc(sizeof(*new_p), GFP_KERNEL);
    if (!new_p)
        return;
    new_p->value = new_value;

    spin_lock(&data_lock);

    old_p = rcu_dereference_protected(shared_data, lockdep_is_held(&data_lock));
    rcu_assign_pointer(shared_data, new_p);

    spin_unlock(&data_lock);

    if (old_p) {
        synchronize_rcu();
        kfree(old_p);
        pr_info("RCU Updater: old memory freed\n");
    }
}

static int __init example_rcu_init(void)
{
    pr_info("Loading RCU example module\n");

    update_data(42);

    reader_thread = kthread_run(reader_thread_fn, NULL, "rcu_reader");
    if (IS_ERR(reader_thread))
        return PTR_ERR(reader_thread);

    msleep(1000);
    update_data(100);
    msleep(1000);

    return 0;
}

static void __exit example_rcu_exit(void)
{
    struct my_data *old_p;

    pr_info("Unloading RCU example module\n");

    kthread_stop(reader_thread);

    spin_lock(&data_lock);
    old_p = rcu_dereference_protected(shared_data, lockdep_is_held(&data_lock));
    RCU_INIT_POINTER(shared_data, NULL);
    spin_unlock(&data_lock);

    if (old_p) {
        synchronize_rcu();
        kfree(old_p);
    }
}

module_init(example_rcu_init);
module_exit(example_rcu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kuan-Wei Chiu <visitorckw@gmail.com>");
MODULE_DESCRIPTION("RCU Example Module");
