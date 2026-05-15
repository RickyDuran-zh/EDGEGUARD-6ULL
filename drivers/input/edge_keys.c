// edge_keys.c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>

struct edge_key_item {
    const char *name;
    struct gpio_desc *gpio;
    int irq;
    unsigned int code;
    struct edge_keys_dev *parent;
};

struct edge_keys_dev {
    struct device *dev;
    struct input_dev *input;
    struct edge_key_item key;
    unsigned int debounce_ms;
};

static irqreturn_t edge_key_irq_thread(int irq, void *data)
{
    struct edge_key_item *key = data;
    int value;

    if (key->parent->debounce_ms)
        msleep(key->parent->debounce_ms);

    /*
     * gpiod_get_value_cansleep() 返回的是逻辑值。
     * 如果 DTS 中设置 GPIO_ACTIVE_HIGH，则按下为 1。
     * 如果 DTS 中设置 GPIO_ACTIVE_LOW，则按下也会被转换为 1。
     */
    value = gpiod_get_value_cansleep(key->gpio);

    input_report_key(key->parent->input, key->code, value);
    input_sync(key->parent->input);

    dev_dbg(key->parent->dev, "%s value=%d\n", key->name, value);

    return IRQ_HANDLED;
}

static int edge_keys_probe(struct platform_device *pdev)
{
    struct edge_keys_dev *kdev;
    struct device_node *np = pdev->dev.of_node;
    unsigned int code = KEY_ENTER;
    int ret;

    kdev = devm_kzalloc(&pdev->dev, sizeof(*kdev), GFP_KERNEL);
    if (!kdev)
        return -ENOMEM;

    kdev->dev = &pdev->dev;

    of_property_read_u32(np, "debounce-ms", &kdev->debounce_ms);
    of_property_read_u32(np, "key-code", &code);

    kdev->input = devm_input_allocate_device(&pdev->dev);
    if (!kdev->input)
        return -ENOMEM;

    kdev->input->name = "edge_keys";
    kdev->input->phys = "edge_keys/input0";
    kdev->input->id.bustype = BUS_HOST;

    input_set_capability(kdev->input, EV_KEY, code);

    kdev->key.name = "edge_key";
    kdev->key.code = code;
    kdev->key.parent = kdev;

    kdev->key.gpio = devm_gpiod_get(&pdev->dev, "key", GPIOD_IN);
    if (IS_ERR(kdev->key.gpio)) {
        dev_err(&pdev->dev, "failed to get key gpio\n");
        return PTR_ERR(kdev->key.gpio);
    }

    kdev->key.irq = gpiod_to_irq(kdev->key.gpio);
    if (kdev->key.irq < 0) {
        dev_err(&pdev->dev, "failed to get key irq\n");
        return kdev->key.irq;
    }

    ret = devm_request_threaded_irq(&pdev->dev,
                                    kdev->key.irq,
                                    NULL,
                                    edge_key_irq_thread,
                                    IRQF_TRIGGER_RISING |
                                    IRQF_TRIGGER_FALLING |
                                    IRQF_ONESHOT,
                                    "edge_key_irq",
                                    &kdev->key);
    if (ret) {
        dev_err(&pdev->dev, "failed to request key irq\n");
        return ret;
    }

    ret = input_register_device(kdev->input);
    if (ret) {
        dev_err(&pdev->dev, "failed to register input device\n");
        return ret;
    }

    platform_set_drvdata(pdev, kdev);

    dev_info(&pdev->dev,
             "edge_keys registered, irq=%d, code=%u, debounce=%u ms\n",
             kdev->key.irq, kdev->key.code, kdev->debounce_ms);

    return 0;
}

static int edge_keys_remove(struct platform_device *pdev)
{
    return 0;
}

static const struct of_device_id edge_keys_of_match[] = {
    { .compatible = "rickyduran,edge_keys" },
    { }
};
MODULE_DEVICE_TABLE(of, edge_keys_of_match);

static struct platform_driver edge_keys_driver = {
    .probe  = edge_keys_probe,
    .remove = edge_keys_remove,
    .driver = {
        .name = "edge_keys",
        .of_match_table = edge_keys_of_match,
    },
};

module_platform_driver(edge_keys_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("RickyDuran");
MODULE_DESCRIPTION("EdgeGuard GPIO key input driver");