// edge_leds.c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/string.h>

#define EDGE_LED_BUF_SIZE 64

struct edge_leds_dev {
    struct gpio_desc *user;
    struct gpio_desc *red;
    struct gpio_desc *green;
    struct gpio_desc *blue;
    struct miscdevice miscdev;
    struct mutex lock;
};

static void edge_rgb_set(struct edge_leds_dev *edev, int r, int g, int b)
{
    gpiod_set_value_cansleep(edev->red, r);
    gpiod_set_value_cansleep(edev->green, g);
    gpiod_set_value_cansleep(edev->blue, b);
}

static ssize_t edge_leds_write(struct file *file,
                               const char __user *ubuf,
                               size_t count,
                               loff_t *ppos)
{
    struct edge_leds_dev *edev = file->private_data;
    char kbuf[EDGE_LED_BUF_SIZE];
    char *cmd;

    if (count >= sizeof(kbuf))
        count = sizeof(kbuf) - 1;

    if (copy_from_user(kbuf, ubuf, count))
        return -EFAULT;

    kbuf[count] = '\0';
    cmd = strim(kbuf);

    mutex_lock(&edev->lock);

    if (!strcmp(cmd, "user on")) {
        gpiod_set_value_cansleep(edev->user, 1);
    } else if (!strcmp(cmd, "user off")) {
        gpiod_set_value_cansleep(edev->user, 0);
    } else if (!strcmp(cmd, "red")) {
        edge_rgb_set(edev, 1, 0, 0);
    } else if (!strcmp(cmd, "green")) {
        edge_rgb_set(edev, 0, 1, 0);
    } else if (!strcmp(cmd, "blue")) {
        edge_rgb_set(edev, 0, 0, 1);
    } else if (!strcmp(cmd, "yellow")) {
        edge_rgb_set(edev, 1, 1, 0);
    } else if (!strcmp(cmd, "cyan")) {
        edge_rgb_set(edev, 0, 1, 1);
    } else if (!strcmp(cmd, "magenta")) {
        edge_rgb_set(edev, 1, 0, 1);
    } else if (!strcmp(cmd, "white")) {
        edge_rgb_set(edev, 1, 1, 1);
    } else if (!strcmp(cmd, "off")) {
        edge_rgb_set(edev, 0, 0, 0);
        gpiod_set_value_cansleep(edev->user, 0);
    } else {
        mutex_unlock(&edev->lock);
        pr_info("edge_leds: unknown command: %s\n", cmd);
        return -EINVAL;
    }

    mutex_unlock(&edev->lock);
    return count;
}

static int edge_leds_open(struct inode *inode, struct file *file)
{
    struct miscdevice *miscdev = file->private_data;
    struct edge_leds_dev *edev;

    edev = container_of(miscdev, struct edge_leds_dev, miscdev);
    file->private_data = edev;

    return 0;
}

static const struct file_operations edge_leds_fops = {
    .owner = THIS_MODULE,
    .open  = edge_leds_open,
    .write = edge_leds_write,
};

static int edge_leds_probe(struct platform_device *pdev)
{
    struct edge_leds_dev *edev;
    int ret;

    edev = devm_kzalloc(&pdev->dev, sizeof(*edev), GFP_KERNEL);
    if (!edev)
        return -ENOMEM;

    mutex_init(&edev->lock);

    edev->user = devm_gpiod_get(&pdev->dev, "user", GPIOD_OUT_LOW);
    if (IS_ERR(edev->user)){
        int ret = PTR_ERR(edev->user);
        dev_err(&pdev->dev, "Failed to get user gpio: %d\n", ret);
        return ret;
    }


    edev->red = devm_gpiod_get(&pdev->dev, "red", GPIOD_OUT_LOW);
    if (IS_ERR(edev->red)){
        int ret = PTR_ERR(edev->red);
        dev_err(&pdev->dev, "Failed to get red gpio: %d\n", ret);
        return ret;
    }

    edev->green = devm_gpiod_get(&pdev->dev, "green", GPIOD_OUT_LOW);
    if (IS_ERR(edev->green)){
        int ret = PTR_ERR(edev->green);
        dev_err(&pdev->dev, "Failed to get green gpio: %d\n", ret);
        return ret;
    }


    edev->blue = devm_gpiod_get(&pdev->dev, "blue", GPIOD_OUT_LOW);
    if (IS_ERR(edev->blue)){
        int ret = PTR_ERR(edev->blue);
        dev_err(&pdev->dev, "Failed to get blue gpio: %d\n", ret);
        return ret;
    }

    edev->miscdev.minor = MISC_DYNAMIC_MINOR;
    edev->miscdev.name = "edge_leds";
    edev->miscdev.fops = &edge_leds_fops;
    edev->miscdev.parent = &pdev->dev;

    platform_set_drvdata(pdev, edev);

    ret = misc_register(&edev->miscdev);
    if (ret)
        return ret;

    dev_info(&pdev->dev, "/dev/edge_leds created\n");
    return 0;
}

static int edge_leds_remove(struct platform_device *pdev)
{
    struct edge_leds_dev *edev = platform_get_drvdata(pdev);

    misc_deregister(&edev->miscdev);
    return 0;
}

static const struct of_device_id edge_leds_of_match[] = {
    { .compatible = "rickyduran,edge_leds" },
    { }
};
MODULE_DEVICE_TABLE(of, edge_leds_of_match);

static struct platform_driver edge_leds_driver = {
    .probe  = edge_leds_probe,
    .remove = edge_leds_remove,
    .driver = {
        .name = "edge_leds",
        .of_match_table = edge_leds_of_match,
    },
};

module_platform_driver(edge_leds_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("RickyDuran");
MODULE_DESCRIPTION("EdgeGuard GPIO LED driver for user LED and RGB LED");