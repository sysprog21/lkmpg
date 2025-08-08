/* devicetree.c - Demonstrates device tree interaction with kernel modules */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/version.h>

#define DRIVER_NAME "lkmpg_devicetree"

/* Structure to hold device-specific data */
struct dt_device_data {
    const char *label;
    u32 reg_value;
    u32 custom_value;
    bool has_clock;
};

/* Probe function - called when device tree node matches */
static int dt_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_node *np = dev->of_node;
    struct dt_device_data *data;
    const char *string_prop;
    u32 value;
    int ret;

    pr_info("%s: Device tree probe called for %s\n", DRIVER_NAME,
            np->full_name);

    /* Allocate memory for device data */
    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    /* Read a string property */
    ret = of_property_read_string(np, "label", &string_prop);
    if (ret == 0) {
        data->label = string_prop;
        pr_info("%s: Found label property: %s\n", DRIVER_NAME, data->label);
    } else {
        data->label = "unnamed";
        pr_info("%s: No label property found, using default\n", DRIVER_NAME);
    }

    /* Read a u32 property */
    ret = of_property_read_u32(np, "reg", &value);
    if (ret == 0) {
        data->reg_value = value;
        pr_info("%s: Found reg property: 0x%x\n", DRIVER_NAME, data->reg_value);
    }

    /* Read a custom u32 property */
    ret = of_property_read_u32(np, "lkmpg,custom-value", &value);
    if (ret == 0) {
        data->custom_value = value;
        pr_info("%s: Found custom-value property: %u\n", DRIVER_NAME,
                data->custom_value);
    } else {
        data->custom_value = 42; /* Default value */
        pr_info("%s: No custom-value found, using default: %u\n", DRIVER_NAME,
                data->custom_value);
    }

    /* Check for presence of a property */
    data->has_clock = of_property_read_bool(np, "lkmpg,has-clock");
    pr_info("%s: has-clock property: %s\n", DRIVER_NAME,
            data->has_clock ? "present" : "absent");

    /* Store device data for later use */
    platform_set_drvdata(pdev, data);

    pr_info("%s: Device probe successful\n", DRIVER_NAME);
    return 0;
}

/* Remove function - called when device is removed */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void dt_remove(struct platform_device *pdev)
{
    struct dt_device_data *data = platform_get_drvdata(pdev);

    pr_info("%s: Removing device %s\n", DRIVER_NAME, data->label);
    /* Cleanup is handled automatically by devm_* functions */
}
#else
static int dt_remove(struct platform_device *pdev)
{
    struct dt_device_data *data = platform_get_drvdata(pdev);

    pr_info("%s: Removing device %s\n", DRIVER_NAME, data->label);
    /* Cleanup is handled automatically by devm_* functions */
    return 0;
}
#endif

/* Device tree match table - defines compatible strings this driver supports */
static const struct of_device_id dt_match_table[] = {
    {
        .compatible = "lkmpg,example-device",
    },
    {
        .compatible = "lkmpg,another-device",
    },
    {} /* Sentinel */
};
MODULE_DEVICE_TABLE(of, dt_match_table);

/* Platform driver structure */
static struct platform_driver dt_driver = {
    .probe = dt_probe,
    .remove = dt_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = dt_match_table,
    },
};

/* Module initialization */
static int __init dt_init(void)
{
    int ret;

    pr_info("%s: Initializing device tree example module\n", DRIVER_NAME);

    /* Register the platform driver */
    ret = platform_driver_register(&dt_driver);
    if (ret) {
        pr_err("%s: Failed to register platform driver\n", DRIVER_NAME);
        return ret;
    }

    pr_info("%s: Module loaded successfully\n", DRIVER_NAME);
    return 0;
}

/* Module cleanup */
static void __exit dt_exit(void)
{
    pr_info("%s: Cleaning up device tree example module\n", DRIVER_NAME);
    platform_driver_unregister(&dt_driver);
}

module_init(dt_init);
module_exit(dt_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Device tree interaction example for LKMPG");
