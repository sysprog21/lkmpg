/*
 * kmem_cache.c - Demonstrates the use of the slab allocator.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>

struct my_custom_struct {
    int id;
    char name[32];
};

static struct kmem_cache *my_cachep;
static struct my_custom_struct *obj1, *obj2;

static int __init kmem_cache_example_init(void)
{
    pr_info("kmem_cache_example: Initializing module\n");

    my_cachep =
        kmem_cache_create("my_custom_cache", sizeof(struct my_custom_struct), 0,
                          SLAB_HWCACHE_ALIGN, NULL);

    if (!my_cachep) {
        pr_err("kmem_cache_example: Failed to create cache\n");
        return -ENOMEM;
    }

    obj1 = kmem_cache_alloc(my_cachep, GFP_KERNEL);
    obj2 = kmem_cache_alloc(my_cachep, GFP_KERNEL);

    if (!obj1 || !obj2) {
        pr_err("kmem_cache_example: Failed to allocate objects\n");
        if (obj1)
            kmem_cache_free(my_cachep, obj1);
        if (obj2)
            kmem_cache_free(my_cachep, obj2);
        kmem_cache_destroy(my_cachep);
        return -ENOMEM;
    }

    obj1->id = 1;
    obj2->id = 2;
    pr_info("kmem_cache_example: Allocated obj1 at %p (id: %d)\n", obj1,
            obj1->id);
    pr_info("kmem_cache_example: Allocated obj2 at %p (id: %d)\n", obj2,
            obj2->id);

    return 0;
}

static void __exit kmem_cache_example_exit(void)
{
    if (obj1)
        kmem_cache_free(my_cachep, obj1);
    if (obj2)
        kmem_cache_free(my_cachep, obj2);

    if (my_cachep)
        kmem_cache_destroy(my_cachep);

    pr_info("kmem_cache_example: Cleaned up and exited\n");
}

module_init(kmem_cache_example_init);
module_exit(kmem_cache_example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kuan-Wei Chiu <visitorckw@gmail.com>");
MODULE_DESCRIPTION("A simple example of kmem_cache");
