// edge_buzzer.c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/string.h>

#define EDGE_BUZZER_BUF_SIZE 64

struct edge_buzzer_dev {
    struct gpio_desc *buzzer;
    struct miscdevice miscdev;
    struct mutex lock;
};

static ssize_t edge_buzzer_write(struct file *file,
                                 const char __user *ubuf,
                                 size_t count,
                                 loff_t *ppos)
{
    struct edge_buzzer_dev *bdev = file->private_data;
    char kbuf[EDGE_BUZZER_BUF_SIZE];
    char *cmd;

    if (count >= sizeof(kbuf))
        count = sizeof(kbuf) - 1;

    if (copy_from_user(kbuf, ubuf, count))
        return -EFAULT;

    kbuf[count] = '\0';
    cmd = strim(kbuf);

    mutex_lock(&bdev->lock);

    if (!strcmp(cmd, "on")) {
        gpiod_set_value_cansleep(bdev->buzzer, 1);
    } else if (!strcmp(cmd, "off")) {
        gpiod_set_value_cansleep(bdev->buzzer, 0);
    } else if (!strcmp(cmd, "beep")) {
        gpiod_set_value_cansleep(bdev->buzzer, 1);
        msleep(200);
        gpiod_set_value_cansleep(bdev->buzzer, 0);
    } else {
        mutex_unlock(&bdev->lock);
        pr_info("edge_buzzer: unknown command: %s\n", cmd);
        return -EINVAL;
    }

    mutex_unlock(&bdev->lock);
    return count;
}

static int edge_buzzer_open(struct inode *inode, struct file *file)
{
    struct miscdevice *miscdev = file->private_data;
    struct edge_buzzer_dev *bdev;

    bdev = container_of(miscdev, struct edge_buzzer_dev, miscdev);
    file->private_data = bdev;

    return 0;
}

static const struct file_operations edge_buzzer_fops = {
    .owner = THIS_MODULE,
    .open  = edge_buzzer_open,
    .write = edge_buzzer_write,
};

static int edge_buzzer_probe(struct platform_device *pdev)
{
    struct edge_buzzer_dev *bdev;
    int ret;

    bdev = devm_kzalloc(&pdev->dev, sizeof(*bdev), GFP_KERNEL);
    if (!bdev)
        return -ENOMEM;

    mutex_init(&bdev->lock);

    bdev->buzzer = devm_gpiod_get(&pdev->dev, "buzzer", GPIOD_OUT_LOW);
    if (IS_ERR(bdev->buzzer)) {
        dev_err(&pdev->dev, "failed to get buzzer gpio\n");
        return PTR_ERR(bdev->buzzer);
    }

    bdev->miscdev.minor = MISC_DYNAMIC_MINOR;
    bdev->miscdev.name = "edge_buzzer";
    bdev->miscdev.fops = &edge_buzzer_fops;
    bdev->miscdev.parent = &pdev->dev;

    platform_set_drvdata(pdev, bdev);

    ret = misc_register(&bdev->miscdev);
    if (ret)
        return ret;

    dev_info(&pdev->dev, "/dev/edge_buzzer created\n");
    return 0;
}

static int edge_buzzer_remove(struct platform_device *pdev)
{
    struct edge_buzzer_dev *bdev = platform_get_drvdata(pdev);

    gpiod_set_value_cansleep(bdev->buzzer, 0);
    misc_deregister(&bdev->miscdev);
    return 0;
}

static const struct of_device_id edge_buzzer_of_match[] = {
    { .compatible = "rickyduran,edge_buzzer" },
    { }
};
MODULE_DEVICE_TABLE(of, edge_buzzer_of_match);

static struct platform_driver edge_buzzer_driver = {
    .probe  = edge_buzzer_probe,
    .remove = edge_buzzer_remove,
    .driver = {
        .name = "edge_buzzer",
        .of_match_table = edge_buzzer_of_match,
    },
};

module_platform_driver(edge_buzzer_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("RickyDuran");
MODULE_DESCRIPTION("EdgeGuard active buzzer GPIO driver");