// ap3216c_raw.c
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/delay.h>

// 寄存器地址
#define AP3216C_REG_SYSTEM_CONFIG   0x00
#define AP3216C_REG_IR_DATA_LOW     0x0A
// 命令
#define AP3216C_CMD_RESET           0x04
#define AP3216C_CMD_ENABLE_ALL      0x03

struct ap3216c_dev {
    struct i2c_client *client;
    struct miscdevice miscdev;
    struct mutex lock;
};

static int ap3216c_read_block(struct i2c_client *client, u8 reg, u8 *buf, int len)
{
    int ret;

    ret = i2c_smbus_read_i2c_block_data(client, reg, len, buf);
    if (ret < 0)
        return ret;

    if (ret != len)
        return -EIO;

    return 0;
}

static ssize_t ap3216c_read(struct file *file, char __user *ubuf,
                            size_t count, loff_t *ppos)
{
    struct ap3216c_dev *dev = file->private_data;
    u8 data[6];
    int ir, als, ps;
    char kbuf[256];
    int len;
    int ret;

    mutex_lock(&dev->lock);

    /*
     * Registers:
     * 0x0A/0x0B: IR data
     * 0x0C/0x0D: ALS data
     * 0x0E/0x0F: PS data
     */
    ret = ap3216c_read_block(dev->client, AP3216C_REG_IR_DATA_LOW,
                             data, sizeof(data));
    if (ret) {
        mutex_unlock(&dev->lock);
        return ret;
    }

    mutex_unlock(&dev->lock);

    /*
     * AP3216C data format:
     * IR:  lower 8 bits in data[0], upper 2 bits in data[1][1:0]
     * ALS: 16-bit little-endian
     * PS:  lower 4 bits in data[4][3:0], upper 6 bits in data[5][5:0]
     */
    if (data[0] & 0x80)
        ir = -1;
    else
        ir = ((data[1] & 0x03) << 8) | data[0];

    als = (data[3] << 8) | data[2];

    if (data[4] & 0x40)
        ps = -1;
    else
        ps = ((data[5] & 0x3F) << 4) | (data[4] & 0x0F);

    len = scnprintf(kbuf, sizeof(kbuf),
                    "ir_raw: %d\n"
                    "als_raw: %d\n"
                    "ps_raw: %d\n",
                    ir, als, ps);

    return simple_read_from_buffer(ubuf, count, ppos, kbuf, len);
}

static int ap3216c_open(struct inode *inode, struct file *file)
{
    struct miscdevice *miscdev = file->private_data;
    struct ap3216c_dev *dev;

    dev = container_of(miscdev, struct ap3216c_dev, miscdev);
    file->private_data = dev;

    return 0;
}

static const struct file_operations ap3216c_fops = {
    .owner = THIS_MODULE,
    .open  = ap3216c_open,
    .read  = ap3216c_read,
};

static int ap3216c_hw_init(struct i2c_client *client)
{
    int ret;

    /*
     * Reset AP3216C.
     */
    ret = i2c_smbus_write_byte_data(client,
                                    AP3216C_REG_SYSTEM_CONFIG,
                                    AP3216C_CMD_RESET);
    if (ret)
        return ret;

    msleep(50);

    /*
     * Enable ALS + PS + IR.
     */
    ret = i2c_smbus_write_byte_data(client,
                                    AP3216C_REG_SYSTEM_CONFIG,
                                    AP3216C_CMD_ENABLE_ALL);
    if (ret)
        return ret;

    msleep(100);

    return 0;
}

static int ap3216c_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
    struct ap3216c_dev *dev;
    int ret;

    dev_info(&client->dev, "ap3216c probe, addr=0x%02x\n", client->addr);

    if (!i2c_check_functionality(client->adapter,
                                 I2C_FUNC_I2C |
                                 I2C_FUNC_SMBUS_BYTE_DATA |
                                 I2C_FUNC_SMBUS_I2C_BLOCK)) {
        dev_err(&client->dev, "i2c functionality not supported\n");
        return -ENODEV;
    }

    ret = ap3216c_hw_init(client);
    if (ret) {
        dev_err(&client->dev, "hardware init failed: %d\n", ret);
        return ret;
    }

    dev = devm_kzalloc(&client->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->client = client;
    mutex_init(&dev->lock);

    dev->miscdev.minor = MISC_DYNAMIC_MINOR;
    dev->miscdev.name  = "ap3216c_raw";
    dev->miscdev.fops  = &ap3216c_fops;
    dev->miscdev.parent = &client->dev;

    i2c_set_clientdata(client, dev);

    ret = misc_register(&dev->miscdev);
    if (ret) {
        dev_err(&client->dev, "misc register failed: %d\n", ret);
        return ret;
    }

    dev_info(&client->dev, "/dev/ap3216c_raw created\n");

    return 0;
}

static int ap3216c_remove(struct i2c_client *client)
{
    struct ap3216c_dev *dev = i2c_get_clientdata(client);

    misc_deregister(&dev->miscdev);
    dev_info(&client->dev, "ap3216c removed\n");

    return 0;
}

static const struct of_device_id ap3216c_of_match[] = {
    { .compatible = "rickyduran,i2c_ap3216c" },
    { }
};
MODULE_DEVICE_TABLE(of, ap3216c_of_match);

static const struct i2c_device_id ap3216c_id[] = {
    { "ap3216c_raw", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, ap3216c_id);

static struct i2c_driver ap3216c_driver = {
    .driver = {
        .name = "ap3216c_raw",
        .of_match_table = ap3216c_of_match,
    },
    .probe    = ap3216c_probe,
    .remove   = ap3216c_remove,
    .id_table = ap3216c_id,
};

module_i2c_driver(ap3216c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("RickyDuran");
MODULE_DESCRIPTION("Raw I2C driver for AP3216C on EBF6ULL S1 Pro");