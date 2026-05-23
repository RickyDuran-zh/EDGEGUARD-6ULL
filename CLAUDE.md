# EdgeGuard imx6ull Driver Project

这是一个基于 imx6ull-S1 Pro 开发板的 Linux 驱动开发项目。

## 环境约束

- 目标板：imx6ull
- 内核版本：Linux 4.19.35-imx6
- 虚拟机：Ubuntu 18.04.4
- 交叉编译工具链：arm-linux-gnueabihf-gcc
- Agent 不直接在虚拟机中运行，只在 Windows 主机上修改代码
- 编译必须通过 SSH 到 Ubuntu 18.04 虚拟机完成

## 工作流

1. 在 Windows 主机中修改代码。
2. 执行 scripts/sync_to_vm.ps1 同步代码到虚拟机。
3. 执行 scripts/build_mpu6050.ps1 在虚拟机中交叉编译。
4. 执行 scripts/deploy_mpu6050.ps1 部署 ko 到 imx6ull 板子。
5. 根据 dmesg 日志分析驱动问题。

## 要求

- 不要修改整个 Linux 内核源码。
- 只修改 drivers、apps、dts、scripts 中的文件。
- 驱动代码需要兼容 Linux 4.19.35。
- 内核驱动代码不要使用过新的内核 API。
- 修改驱动后必须检查 Makefile、设备树匹配 compatible、MODULE_DEVICE_TABLE、probe/remove 逻辑。