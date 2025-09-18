/*
 *  dht11.c - Using GPIO to read temperature and humidity from DHT11 sensor.
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

#define GPIO_PIN_4 575
#define DEVICE_NAME "dht11"
#define DEVICE_CNT 1

static char msg[64];

struct dht11_dev {
    dev_t dev_num;
    int major_num, minor_num;
    struct cdev cdev;
    struct class *cls;
    struct device *dev;
};

static struct dht11_dev dht11_device;

/* Define GPIOs for LEDs.
 * TODO: According to the requirements, search /sys/kernel/debug/gpio to 
 * find the corresponding GPIO location.
 */
static struct gpio dht11[] = { { GPIO_PIN_4, GPIOF_OUT_INIT_HIGH, "Signal" } };

static int dht11_read_data(void)
{
    int timeout;
    uint8_t sensor_data[5] = { 0 };
    uint8_t i, j;

    gpio_set_value(dht11[0].gpio, 0);
    mdelay(20);
    gpio_set_value(dht11[0].gpio, 1);
    udelay(30);
    gpio_direction_input(dht11[0].gpio);
    udelay(2);

    timeout = 300;
    while (gpio_get_value(dht11[0].gpio) && timeout--)
        udelay(1);

    if (timeout == -1)
        return -ETIMEDOUT;

    timeout = 300;
    while (!gpio_get_value(dht11[0].gpio) && timeout--)
        udelay(1);

    if (timeout == -1)
        return -ETIMEDOUT;

    timeout = 300;
    while (gpio_get_value(dht11[0].gpio) && timeout--)
        udelay(1);

    if (timeout == -1)
        return -ETIMEDOUT;

    for (j = 0; j < 5; j++) {
        uint8_t byte = 0;
        for (i = 0; i < 8; i++) {
            timeout = 300;
            while (gpio_get_value(dht11[0].gpio) && timeout--)
                udelay(1);

            if (timeout == -1)
                return -ETIMEDOUT;

            timeout = 300;
            while (!gpio_get_value(dht11[0].gpio) && timeout--)
                udelay(1);

            if (timeout == -1)
                return -ETIMEDOUT;

            udelay(50);
            byte <<= 1;
            if (gpio_get_value(dht11[0].gpio))
                byte |= 0x01;
        }
        sensor_data[j] = byte;
    }

    if (sensor_data[4] != (uint8_t)(sensor_data[0] + sensor_data[1] +
                                    sensor_data[2] + sensor_data[3]))
        return -EIO;

    gpio_direction_output(dht11[0].gpio, 1);
    sprintf(msg, "Humidity: %d%%\nTemperature: %d deg C\n", sensor_data[0],
            sensor_data[2]);

    return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
    int ret, retry;

    for (retry = 0; retry < 5; ++retry) {
        ret = dht11_read_data();
        if (ret == 0)
            return 0;
        msleep(10);
    }
    gpio_direction_output(dht11[0].gpio, 1);

    return ret;
}

static int device_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t device_read(struct file *filp, char __user *buffer,
                           size_t length, loff_t *offset)
{
    int msg_len = strlen(msg);

    if (*offset >= msg_len)
        return 0;

    size_t remain = msg_len - *offset;
    size_t bytes_read = min(length, remain);

    if (copy_to_user(buffer, msg + *offset, bytes_read))
        return -EFAULT;

    *offset += bytes_read;

    return bytes_read;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .read = device_read,
};

/* Initialize the module - Register the character device */
static int __init dht11_init(void)
{
    int ret = 0;

    /* Determine whether dynamic allocation of the device number is needed. */
    if (dht11_device.major_num) {
        dht11_device.dev_num =
            MKDEV(dht11_device.major_num, dht11_device.minor_num);
        ret = register_chrdev_region(dht11_device.dev_num, DEVICE_CNT,
                                     DEVICE_NAME);
    } else {
        ret = alloc_chrdev_region(&dht11_device.dev_num, 0, DEVICE_CNT,
                                  DEVICE_NAME);
    }

    /* Negative values signify an error */
    if (ret < 0) {
        pr_alert("Failed to register character device, error: %d\n", ret);
        return ret;
    }

    pr_info("Major = %d, Minor = %d\n", MAJOR(dht11_device.dev_num),
            MINOR(dht11_device.dev_num));

    /* Prevents module unloading while operations are in use */
    dht11_device.cdev.owner = THIS_MODULE;

    cdev_init(&dht11_device.cdev, &fops);
    ret = cdev_add(&dht11_device.cdev, dht11_device.dev_num, 1);
    if (ret) {
        pr_err("Failed to add the device to the system\n");
        goto fail1;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    dht11_device.cls = class_create(DEVICE_NAME);
#else
    dht11_device.cls = class_create(THIS_MODULE, DEVICE_NAME);
#endif
    if (IS_ERR(dht11_device.cls)) {
        pr_err("Failed to create class for device\n");
        ret = PTR_ERR(dht11_device.cls);
        goto fail2;
    }

    dht11_device.dev = device_create(dht11_device.cls, NULL,
                                     dht11_device.dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(dht11_device.dev)) {
        pr_err("Failed to create the device file\n");
        ret = PTR_ERR(dht11_device.dev);
        goto fail3;
    }

    pr_info("Device created on /dev/%s\n", DEVICE_NAME);

    ret = gpio_request(dht11[0].gpio, dht11[0].label);

    if (ret) {
        pr_err("Unable to request GPIOs for dht11: %d\n", ret);
        goto fail4;
    }

    ret = gpio_direction_output(dht11[0].gpio, 1);

    if (ret) {
        pr_err("Failed to set GPIO %d direction\n", dht11[0].gpio);
        goto fail5;
    }

    return 0;

fail5:
    gpio_free(dht11[0].gpio);

fail4:
    device_destroy(dht11_device.cls, dht11_device.dev_num);

fail3:
    class_destroy(dht11_device.cls);

fail2:
    cdev_del(&dht11_device.cdev);

fail1:
    unregister_chrdev_region(dht11_device.dev_num, DEVICE_CNT);

    return ret;
}

static void __exit dht11_exit(void)
{
    gpio_set_value(dht11[0].gpio, 0);
    gpio_free(dht11[0].gpio);

    device_destroy(dht11_device.cls, dht11_device.dev_num);
    class_destroy(dht11_device.cls);
    cdev_del(&dht11_device.cdev);
    unregister_chrdev_region(dht11_device.dev_num, DEVICE_CNT);
}

module_init(dht11_init);
module_exit(dht11_exit);

MODULE_LICENSE("GPL");
