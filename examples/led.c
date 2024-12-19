/*
 * led.c - Using GPIO to control the LED on/off
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include <asm/errno.h>

#define SUCCESS 0
#define DEVICE_NAME "gpio_led"

/* The major device number. We can not rely on dynamic registration
 * any more.
 */
#define MAJOR_NUM 100
#define BUF_LEN 2

static char control_signal[BUF_LEN];
static unsigned long device_buffer_size = 0;

static struct class *cls;
static struct device *dev;

/* Define GPIOs for LEDs.
 * TODO: Change the numbers for the GPIO on your board.
 */
static struct gpio leds[] = { { 4, GPIOF_OUT_INIT_LOW, "LED 1" } };

/* This is called whenever a process attempts to open the device file */
static int device_open(struct inode *inode, struct file *file)
{
    pr_info("device_open(%p)\n", file);

    try_module_get(THIS_MODULE);
    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
    pr_info("device_release(%p,%p)\n", inode, file);

    module_put(THIS_MODULE);
    return SUCCESS;
}

/* called when somebody tries to write into our device file. */
static ssize_t device_write(struct file *file, const char __user *buffer,
                            size_t length, loff_t *offset)
{
    pr_info("device_write(%p,%p,%ld)", file, buffer, length);

    device_buffer_size = min(BUF_LEN, length);

    if (copy_from_user(control_signal, buffer, device_buffer_size)) {
        return -EFAULT;
    }

    /* Determine the received signal to decide the LED on/off state. */
    switch (control_signal[0]) {
    case '0':
        gpio_set_value(leds[0].gpio, 0);
        pr_info("LED OFF");
        break;
    case '1':
        gpio_set_value(leds[0].gpio, 1);
        pr_info("LED ON");
        break;
    default:
        pr_warn("Invalid value!\n");
        break;
    }

    *offset += device_buffer_size;

    /* Again, return the number of input characters used. */
    return device_buffer_size;
}

static struct file_operations fops = {
    .write = device_write,
    .open = device_open,
    .release = device_release, /* a.k.a. close */
};

/* Initialize the module - Register the character device */
static int __init led_init(void)
{
    int ret = 0;

    /* Register the character device (at least try) */
    ret = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);

    /* Negative values signify an error */
    if (ret < 0) {
        pr_alert("%s failed with %d\n",
                 "Sorry, registering the character device ", ret);
        return ret;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create(DEVICE_NAME);
#else
    cls = class_create(THIS_MODULE, DEVICE_NAME);
#endif
    if (IS_ERR(cls)) {
        pr_err("Failed to create class for device\n");
        ret = PTR_ERR(cls);
        goto fail1;
    }

    dev = device_create(cls, NULL, MKDEV(MAJOR_NUM, 0), NULL, DEVICE_NAME);
    if (IS_ERR(dev)) {
        pr_err("Failed to create the device file\n");
        ret = PTR_ERR(dev);
        goto fail2;
    }

    pr_info("Device created on /dev/%s\n", DEVICE_NAME);

    ret = gpio_request(leds[0].gpio, leds[0].label);

    if (ret) {
        pr_err("Unable to request GPIOs for LEDs: %d\n", ret);
        goto fail3;
    }

    ret = gpio_direction_output(leds[0].gpio, leds[0].flags);

    if (ret) {
        pr_err("Failed to set GPIO %d direction\n", leds[0].gpio);
        goto fail4;
    }

    return 0;

fail4:
    gpio_free(leds[0].gpio);

fail3:
    device_destroy(cls, MKDEV(MAJOR_NUM, 0));

fail2:
    class_destroy(cls);

fail1:
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);

    return ret;
}

/* Cleanup - unregister the appropriate file from /proc */
static void __exit led_exit(void)
{
    gpio_set_value(leds[0].gpio, 0);
    gpio_free(leds[0].gpio);

    device_destroy(cls, MKDEV(MAJOR_NUM, 0));
    class_destroy(cls);
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
