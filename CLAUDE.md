# EdgeGuard imx6ull Driver Project

这是一个基于 imx6ull-S1 Pro 开发板的 Linux 驱动、应用和 UI 开发项目。

## 环境约束

- 目标板：imx6ull-S1 Pro
- 内核版本：Linux 4.19.35-imx6
- 虚拟机系统：Ubuntu 18.04.4
- 主机系统：Windows
- 主机开发工具：VS Code / Claude Code
- Agent 只在 Windows 主机上修改代码，不直接在 Ubuntu 18.04 虚拟机中运行
- Agent 不负责编译，不负责部署到开发板
- 编译、设备树生成、驱动加载和开发板测试均由用户在 Ubuntu 虚拟机或开发板上手动完成

## 工作流

1. 在 Windows 主机中修改代码。
2. 执行 `scripts/sync_to_vm.sh` 将代码同步到 Ubuntu 18.04 虚拟机。
3. 用户进入虚拟机手动编译驱动、应用、UI 或设备树。
4. 用户将编译报错、dmesg 日志或运行结果反馈给 Agent。
5. Agent 根据日志分析问题，并继续修改主机工程中的源码。

## 目录说明

- `drivers/`：Linux 驱动代码
- `app/`：应用层程序
- `ui/`：Qt UI 或其他前端程序
- `dts/`：本地主机维护的设备树文件
- `docs/`：项目文档
- `scripts/`：同步脚本和辅助脚本

## 设备树规则

- 本地主机维护的设备树文件位于 `dts/` 目录。
- 当前主要设备树文件为 `dts/imx6ull-mmc-npi.dts`。
- 同步脚本会将该 DTS 文件复制到虚拟机内核源码对应的 `arch/arm/boot/dts/` 目录。
- 修改设备树时，需要检查 `compatible`、`reg`、`pinctrl`、`interrupts`、`status` 等字段。
- 不要随意删除板级原有节点。

## 开发要求

- 不要随意连接虚拟机。
- 每次开发完成后，由用户执行 `scripts/sync_to_vm.sh` 同步代码。
- 只修改 `drivers/`、`app/`、`ui/`、`dts/`、`docs/` 中与当前任务相关的文件，或者在这些目录下创建新的文件或目录。
- 除非用户明确要求，否则不要修改 `scripts/sync_to_vm.sh`。
- 不要修改 `kernel/` 目录中的完整内核源码。
- 驱动代码需要兼容 Linux 4.19.35。
- 内核驱动代码不要使用过新的 Linux 内核 API。
- 修改驱动后必须检查 Makefile、设备树匹配 `compatible`、`MODULE_DEVICE_TABLE`、`probe/remove` 逻辑。
- 涉及用户态数据交互时，需要正确使用 `copy_to_user` 和 `copy_from_user`。
- 涉及并发访问时，需要考虑 `mutex`、`spinlock`、`wait queue`、`atomic` 等同步机制。
- 涉及 I2C、SPI、GPIO、中断、PWM、input 子系统时，需要保证设备树节点和驱动匹配逻辑一致。

## Agent 行为要求

- 修改代码前先阅读相关目录和已有文件。
- 不要假设项目运行在普通 x86 Linux 环境中。
- 不要生成依赖过新内核 API 的代码。
- 不要在代码中写入 SSH 密码、API Key 或其他敏感信息。
- 每次修改后说明改动了哪些文件、为什么修改、后续如何编译或测试。
- 如果用户提供编译错误或 dmesg 日志，需要结合驱动代码、Makefile、设备树节点和内核版本一起分析。