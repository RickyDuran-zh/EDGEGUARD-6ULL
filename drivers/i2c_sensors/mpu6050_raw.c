// mpu6050_raw.c
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#define MPU6050_REG_PWR_MGMT_1    0x6B
#define MPU6050_REG_SMPLRT_DIV    0x19
#define MPU6050_REG_CONFIG        0x1A
#define MPU6050_REG_GYRO_CONFIG   0x1B
#define MPU6050_REG_ACCEL_CONFIG  0x1C
#define MPU6050_REG_ACCEL_XOUT_H  0x3B
#define MPU6050_REG_WHO_AM_I      0x75

struct mpu6050_dev {
    struct i2c_client *client;
    struct miscdevice miscdev;
    struct mutex lock;
};

static s16 mpu6050_be16_to_s16(u8 high, u8 low)
{
    return (s16)((high << 8) | low);
}

static int mpu6050_read_block(struct i2c_client *client, u8 reg, u8 *buf, int len)
{
    int ret;

    ret = i2c_smbus_read_i2c_block_data(client, reg, len, buf);
    if (ret < 0)
        return ret;

    if (ret != len)
        return -EIO;

    return 0;
}

static ssize_t mpu6050_read(struct file *file, char __user *ubuf,
                            size_t count, loff_t *ppos)
{
    struct mpu6050_dev *dev = file->private_data;
    u8 data[14];
    s16 ax, ay, az, temp, gx, gy, gz;
    char kbuf[256];
    int len;
    int ret;

    mutex_lock(&dev->lock);

    ret = mpu6050_read_block(dev->client, MPU6050_REG_ACCEL_XOUT_H,
                             data, sizeof(data));
    if (ret) {
        mutex_unlock(&dev->lock);
        return ret;
    }

    ax   = mpu6050_be16_to_s16(data[0],  data[1]);
    ay   = mpu6050_be16_to_s16(data[2],  data[3]);
    az   = mpu6050_be16_to_s16(data[4],  data[5]);
    temp = mpu6050_be16_to_s16(data[6],  data[7]);
    gx   = mpu6050_be16_to_s16(data[8],  data[9]);
    gy   = mpu6050_be16_to_s16(data[10], data[11]);
    gz   = mpu6050_be16_to_s16(data[12], data[13]);

    mutex_unlock(&dev->lock);

    len = scnprintf(kbuf, sizeof(kbuf),
                    "accel_raw: %d %d %d\n"
                    "temp_raw: %d\n"
                    "gyro_raw: %d %d %d\n",
                    ax, ay, az, temp, gx, gy, gz);

    return simple_read_from_buffer(ubuf, count, ppos, kbuf, len);
}

static int mpu6050_open(struct inode *inode, struct file *file)
{
    struct miscdevice *miscdev = file->private_data;
    struct mpu6050_dev *dev;

    dev = container_of(miscdev, struct mpu6050_dev, miscdev);
    file->private_data = dev;

    return 0;
}

static const struct file_operations mpu6050_fops = {
    .owner = THIS_MODULE,
    .open  = mpu6050_open,
    .read  = mpu6050_read,
};

static int mpu6050_hw_init(struct i2c_client *client)
{
    int ret;
    int whoami;

    whoami = i2c_smbus_read_byte_data(client, MPU6050_REG_WHO_AM_I);
    if (whoami < 0)
        return whoami;

    dev_info(&client->dev, "WHO_AM_I = 0x%02x\n", whoami);

    /*
     * Wake up MPU6050.
     * PWR_MGMT_1 = 0x00: use internal 8MHz clock, clear sleep bit.
     */
    ret = i2c_smbus_write_byte_data(client, MPU6050_REG_PWR_MGMT_1, 0x00);
    if (ret)
        return ret;

    msleep(100);

    /*
     * Basic configuration:
     * sample divider = 0
     * DLPF config = 3
     * gyro range = ±250 dps
     * accel range = ±2g
     */
    ret = i2c_smbus_write_byte_data(client, MPU6050_REG_SMPLRT_DIV, 0x00);
    if (ret)
        return ret;

    ret = i2c_smbus_write_byte_data(client, MPU6050_REG_CONFIG, 0x03);
    if (ret)
        return ret;

    ret = i2c_smbus_write_byte_data(client, MPU6050_REG_GYRO_CONFIG, 0x00);
    if (ret)
        return ret;

    ret = i2c_smbus_write_byte_data(client, MPU6050_REG_ACCEL_CONFIG, 0x00);
    if (ret)
        return ret;

    return 0;
}

static int mpu6050_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
    struct mpu6050_dev *dev;
    int ret;

    dev_info(&client->dev, "mpu6050 probe, addr=0x%02x\n", client->addr);

    if (!i2c_check_functionality(client->adapter,
                                 I2C_FUNC_I2C |
                                 I2C_FUNC_SMBUS_BYTE_DATA |
                                 I2C_FUNC_SMBUS_I2C_BLOCK)) {
        dev_err(&client->dev, "i2c functionality not supported\n");
        return -ENODEV;
    }

    ret = mpu6050_hw_init(client);
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
    dev->miscdev.name  = "mpu6050_raw";
    dev->miscdev.fops  = &mpu6050_fops;
    dev->miscdev.parent = &client->dev;

    i2c_set_clientdata(client, dev);

    ret = misc_register(&dev->miscdev);
    if (ret) {
        dev_err(&client->dev, "misc register failed: %d\n", ret);
        return ret;
    }

    dev_info(&client->dev, "/dev/mpu6050_raw created\n");

    return 0;
}

static int mpu6050_remove(struct i2c_client *client)
{
    struct mpu6050_dev *dev = i2c_get_clientdata(client);

    misc_deregister(&dev->miscdev);
    dev_info(&client->dev, "mpu6050 removed\n");
    return 0;
}

static const struct of_device_id mpu6050_of_match[] = {
    { .compatible = "rickyduran,i2c_mpu6050" },
    { }
};
MODULE_DEVICE_TABLE(of, mpu6050_of_match);

static const struct i2c_device_id mpu6050_id[] = {
    { "mpu6050_raw", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, mpu6050_id);

static struct i2c_driver mpu6050_driver = {
    .driver = {
        .name = "mpu6050_raw",
        .of_match_table = mpu6050_of_match,
    },
    .probe    = mpu6050_probe,
    .remove   = mpu6050_remove,
    .id_table = mpu6050_id,
};

module_i2c_driver(mpu6050_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("RickyDuran");
MODULE_DESCRIPTION("Raw I2C driver for MPU6050 on EBF6ULL S1 Pro");