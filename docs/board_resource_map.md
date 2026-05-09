# EBF6ULL S1 Pro 引脚映射

## 外设资源物理引脚映射
### 板载/外接传感器
- MPU6050 陀螺仪                   I2C1_SDA I2C1_SCL    I2C地址：0xD0 -> 0x68 引脚映射：MX6UL_PAD_UART4_TX_DATA__I2C1_SCL(功能测试完毕)
- AP3216C IR(红外LED) + ALS(环境光传感) + PS(接近传感器) 传感器组合  I2C1_SDA I2C1_SCL    I2C地址：0x1E         引脚映射：MX6UL_PAD_UART4_RX_DATA__I2C1_SDA(功能测试完毕)
- DS18B20(需要购买) 温湿度传感器接口   GPIO1_2 
### 本地输入输出
- 用户LED
- RGB LED
- 按键
- 可调电阻