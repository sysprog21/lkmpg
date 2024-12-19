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

#define DEVICE_NAME "gpio_led"
#define DEVICE_CNT 1
#define BUF_LEN 2

static char control_signal[BUF_LEN];
static unsigned long device_buffer_size = 0;

struct LED_dev {
    dev_t dev_num;
    int major_num, minor_num;
    struct cdev cdev;
    struct class *cls;
    struct device *dev;
};

static struct LED_dev led_device;

/* Define GPIOs for LEDs.
 * TODO: According to the requirements, search /sys/kernel/debug/gpio to 
 * find the corresponding GPIO location.
 */
static struct gpio leds[] = { { 4, GPIOF_OUT_INIT_LOW, "LED 1" } };

/* This is called whenever a process attempts to open the device file */
static int device_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t device_write(struct file *file, const char __user *buffer,
                            size_t length, loff_t *offset)
{
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
    .owner = THIS_MODULE,
    .write = device_write,
    .open = device_open,
    .release = device_release,
};

/* Initialize the module - Register the character device */
static int __init led_init(void)
{
    int ret = 0;

    /* Determine whether dynamic allocation of the device number is needed. */
    if (led_device.major_num) {
        led_device.dev_num = MKDEV(led_device.major_num, led_device.minor_num);
        ret =
            register_chrdev_region(led_device.dev_num, DEVICE_CNT, DEVICE_NAME);
    } else {
        ret = alloc_chrdev_region(&led_device.dev_num, 0, DEVICE_CNT,
                                  DEVICE_NAME);
    }

    /* Negative values signify an error */
    if (ret < 0) {
        pr_alert("Failed to register character device, error: %d\n", ret);
        return ret;
    }

    pr_info("Major = %d, Minor = %d\n", MAJOR(led_device.dev_num),
            MINOR(led_device.dev_num));

    /* Prevents module unloading while operations are in use */
    led_device.cdev.owner = THIS_MODULE;

    cdev_init(&led_device.cdev, &fops);
    ret = cdev_add(&led_device.cdev, led_device.dev_num, 1);
    if (ret) {
        pr_err("Failed to add the device to the system\n");
        goto fail1;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    led_device.cls = class_create(DEVICE_NAME);
#else
    led_device.cls = class_create(THIS_MODULE, DEVICE_NAME);
#endif
    if (IS_ERR(led_device.cls)) {
        pr_err("Failed to create class for device\n");
        ret = PTR_ERR(led_device.cls);
        goto fail2;
    }

    led_device.dev = device_create(led_device.cls, NULL, led_device.dev_num,
                                   NULL, DEVICE_NAME);
    if (IS_ERR(led_device.dev)) {
        pr_err("Failed to create the device file\n");
        ret = PTR_ERR(led_device.dev);
        goto fail3;
    }

    pr_info("Device created on /dev/%s\n", DEVICE_NAME);

    ret = gpio_request(leds[0].gpio, leds[0].label);

    if (ret) {
        pr_err("Unable to request GPIOs for LEDs: %d\n", ret);
        goto fail4;
    }

    ret = gpio_direction_output(leds[0].gpio, leds[0].flags);

    if (ret) {
        pr_err("Failed to set GPIO %d direction\n", leds[0].gpio);
        goto fail5;
    }

    return 0;

fail5:
    gpio_free(leds[0].gpio);

fail4:
    device_destroy(led_device.cls, led_device.dev_num);

fail3:
    class_destroy(led_device.cls);

fail2:
    cdev_del(&led_device.cdev);

fail1:
    unregister_chrdev_region(led_device.dev_num, DEVICE_CNT);

    return ret;
}

static void __exit led_exit(void)
{
    gpio_set_value(leds[0].gpio, 0);
    gpio_free(leds[0].gpio);

    device_destroy(led_device.cls, led_device.dev_num);
    class_destroy(led_device.cls);
    cdev_del(&led_device.cdev);
    unregister_chrdev_region(led_device.dev_num, DEVICE_CNT);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
