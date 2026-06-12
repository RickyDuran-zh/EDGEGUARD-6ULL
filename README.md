# EdgeGuard-6ULL

基于 i.MX6ULL-S1 Pro 开发板的边缘安全监控系统。

---

## 项目定位

EdgeGuard-6ULL 是一个嵌入式边缘安全节点，通过 I2C 传感器（MPU6050 加速度计/陀螺仪 + AP3216C 光线/接近传感器）实时监测环境状态，经状态机评估后驱动 LED 和蜂鸣器报警，同时提供：

- **本地 LCD 触摸屏界面**（Qt5, 800×480）
- **Web 远程仪表板**（HTTP :8080, 嵌入式 HTML）
- **SSE 实时数据推送**（无需轮询）
- **SQLite 报警历史查询**
- **MQTT 遥测上报**（IoT 标准协议）
- **USB 摄像头视觉监控**（运动检测 + 快照留存，可选 AI 人脸检测）

PC 通过网线直连板子即可监控和控制。

---

## 目录结构

```
EdgeGuard-6ULL/
├── app/                        # 用户态守护进程（C）
│   ├── sensor_hubd.c           # 核心守护进程：传感器读取、状态机、LED/蜂鸣器、JSON输出、SQLite写入
│   ├── edgeguard_httpd.c       # HTTP 服务器：Web仪表板、REST API、SSE推送、SQLite查询、Basic Auth
│   ├── edgeguard_mqttd.c       # MQTT客户端：遥测上报、断线重连
│   ├── sqlite3.c / sqlite3.h   # SQLite 3.53.1 amalgamation（嵌入式数据库，编译进二进制）
│   ├── camera_v4l2.h / .c      # V4L2 摄像头捕获封装（MJPEG 640×480, mmap 双缓冲）
│   ├── edgeguard_visiond.c     # 视觉守护进程：定时拍照、运动检测、写 vision.json
│   └── Makefile                # 交叉编译 makefile（4个target）
│
├── ui/                         # Qt5 本地触摸屏界面（C++）
│   ├── main.cpp                # 入口：QApplication + showFullScreen()
│   ├── mainwindow.h / .cpp     # 主窗口：6页 QStackedWidget + 触摸滑动 + JSON解析 + 登录
│   ├── EdgeGuardUI.pro         # qmake 工程文件
│   ├── run_linuxfb.sh          # 板子启动脚本
│   └── status_sample.json      # 测试用示例 JSON
│
├── drivers/                    # Linux 内核驱动（C）
│   ├── i2c_sensors/            # MPU6050 + AP3216C I2C misc 字符设备驱动
│   ├── input/                  # edge_leds + edge_buzzer + edge_keys（GPIO/platform驱动）
│   └── touch_sceen/            # 触摸屏/帧缓冲测试程序
│
├── dts/                        # 设备树
│   └── imx6ull-mmc-npi.dts     # 主设备树文件（权威源）
│
├── docs/                       # 文档
│   └── board_resource_map.md   # 板子引脚资源映射
│
├── scripts/                    # 部署与配置脚本
│   ├── sync_to_vm.sh           # Windows → Ubuntu VM 代码同步
│   ├── install_services.sh     # 一键安装所有 systemd 服务
│   ├── edgeguard.service       # sensor_hubd 服务单元
│   ├── edgeguard-ui.service    # Qt UI 服务单元
│   ├── edgeguard-httpd.service # HTTP 服务器服务单元
│   ├── edgeguard-mqttd.service # MQTT 客户端服务单元
│   └── edgeguard-visiond.service # 视觉守护进程服务单元
│
├── CLAUDE.md                   # Claude Code 项目指令
└── README.md                   # 本文件
```

---

## 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│  板子 i.MX6ULL                                               │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ sensor_hubd  │  │edgeguard_httpd│  │edgeguard_mqttd│     │
│  │ (核心进程)    │  │ (HTTP :8080) │  │ (MQTT client) │      │
│  │              │  │              │  │               │      │
│  │ 读传感器      │  │ Web仪表板     │  │ 状态上报       │      │
│  │ 状态机评估    │  │ REST API     │  │ 报警推送       │      │
│  │ LED/蜂鸣器   │  │ SSE推送      │  │ 遥测数据       │      │
│  │ 写JSON/DB    │  │ SQLite查询   │  │               │      │
│  │ 读vision.json│  │ 返回快照      │  │               │      │
│  └───┬──┬──┬────┘  └──────┬───────┘  └──────┬────────┘      │
│      │  │  │              │                  │               │
│      │  │  │  ┌───────────┴──────────┐       │               │
│      │  │  │  │ edgeguard_visiond    │       │               │
│      │  │  │  │ (USB摄像头守护进程)    │       │               │
│      │  │  │  │ 拍照+运动检测+快照    │       │               │
│      │  │  │  └──────────┬──────────┘       │               │
│      │  │  │              │                  │               │
│      ↓  ↓  ↓              ↓                  ↓               │
│  ┌───────────────────────────────────────────────────────┐   │
│  │        /tmp/edgeguard_status.json    (500ms刷新)       │   │
│  │        /tmp/edgeguard_cmd.json       (命令通道)        │   │
│  │        /tmp/edgeguard_vision.json    (摄像头JSON)     │   │
│  │        /var/log/edgeguard/alarms.db  (SQLite历史)     │   │
│  │        /var/log/edgeguard/alarm.log  (文本日志)        │   │
│  │        /var/log/edgeguard/snapshots/ (报警快照)        │   │
│  │        /etc/edgeguard/config.json    (阈值配置)        │   │
│  └───────────────────────────────────────────────────────┘   │
│                              ↑                               │
│  ┌──────────────┐           │                               │
│  │ edgeguard-ui │───────────┘                               │
│  │ (Qt5触摸屏)   │  读status.json, 写cmd.json               │
│  └──────────────┘                                           │
└─────────────────────────────────────────────────────────────┘
         │ 网线                                   │ 网线
         ↓                                        ↓
┌──────────────────┐                    ┌──────────────────┐
│ PC 浏览器         │                    │ PC MQTT Broker   │
│ :8080            │                    │ :1883            │
│ Web Dashboard    │                    │ mosquitto_sub    │
│ SSE 实时推送      │                    │ edgeguard/#      │
└──────────────────┘                    └──────────────────┘
```

### 进程间通信

所有进程通过**文件**交换数据，进程间零直接通信：

| 文件 | 写入者 | 读取者 | 用途 |
|------|--------|--------|------|
| `/tmp/edgeguard_status.json` | sensor_hubd | HTTP / MQTT / UI | 实时状态（原子写: .tmp → rename） |
| `/tmp/edgeguard_cmd.json` | HTTP / UI | sensor_hubd | 命令（mute/ack/demo，读取后删除） |
| `/tmp/edgeguard_vision.json` | edgeguard_visiond | sensor_hubd / HTTP / UI | 摄像头状态 + 运动/人脸检测结果 |
| `/var/log/edgeguard/alarms.db` | sensor_hubd | HTTP | 报警历史（SQLite WAL模式） |
| `/var/log/edgeguard/snapshots/` | edgeguard_visiond | HTTP | 报警 JPEG 快照 |
| `/etc/edgeguard/config.json` | 管理员 | sensor_hubd | 传感器阈值配置 |

---

## 功能模块

### sensor_hubd — 核心守护进程

- **周期**：500ms（可配置）
- **传感器**：MPU6050（加速度/陀螺仪/温度）、AP3216C（红外/光线/接近）
- **状态机**：NORMAL → WARNING → ALARM → FAULT（4级，含迟滞防止抖动）
- **硬件控制**：RGB LED 颜色/闪烁、蜂鸣器开关/鸣叫
- **按键**：硬件按键一键清除报警
- **输出**：status.json、alarms.db、alarm.log
- **输入**：cmd.json（mute_buzzer / ack_alarm / demo_alarm）

### edgeguard_httpd — HTTP 服务器

**Web Dashboard（多页面 SPA）**：4 个页面纯前端路由切换，零外部依赖。

| 页面 | 内容 |
|------|------|
| **Dashboard** | 状态概览栏（State / Alarms / Uptime / IP）+ MPU6050 + AP3216C + 设备控制 + 按钮状态反馈 + 最近 5 条报警 |
| **Alarms** | SQLite 报警历史表格（时间、级别徽章、原因、传感器详情） |
| **Camera** | 实时快照预览 + 视觉检测结果（在线状态、运动、人脸计数、推理耗时） |
| **Settings** | 6 项传感器阈值展示 + 系统信息 |

**API 端点**：

| 端点 | 方法 | 认证 | 功能 |
|------|------|------|------|
| `/` | GET | 无 | 嵌入式 Web Dashboard（多页面 SPA） |
| `/api/status` | GET | 无 | 完整状态 JSON |
| `/api/stream` | GET | 无 | SSE 实时推送 |
| `/api/alarms?limit=N` | GET | 无 | SQLite 报警历史 |
| `/api/alarms/count` | GET | 无 | 报警总数 |
| `/api/snapshot` | GET | 无 | 最新 JPEG 快照（image/jpeg） |
| `/api/vision` | GET | 无 | 摄像头视觉检测 JSON |
| `/api/cmd` | GET/POST | Basic Auth | 发送命令 |

### edgeguard_mqttd — MQTT 遥测

| 主题 | 触发 | 内容 |
|------|------|------|
| `edgeguard/state` | 状态变化 | NORMAL / WARNING / ALARM / FAULT |
| `edgeguard/alarm` | 进入报警 | {"state":"ALARM","reason":"motion"} |
| `edgeguard/telemetry` | 每 10 秒 | 完整传感器 JSON |
| `edgeguard/status` | 连接建立 | {"online":true} |

### edgeguard-ui — Qt5 触摸屏界面

- **6 页**：Login / Dashboard / Sensors / Alarms / Settings / System / Vision
- **触摸**：通过 Qt linuxfb QPA 的 QMouseEvent 实现上下滑动切页、点击侧边栏切页
- **键盘**：按键 1-6 切页、Esc 退出
- **Demo 模式**：`--demo` 参数模拟传感器数据
- **登录认证**：用户名/密码 + 虚拟键盘，3 次失败锁定 30 秒

### edgeguard_visiond — USB 摄像头视觉守护进程（P1 阶段）

- **周期**：2000ms（可配置）
- **摄像头**：USB UVC (Logitech B525)，V4L2 MJPEG 640×480 捕获
- **运动检测**：JPEG 帧大小变化启发式检测（阈值 15%）
- **输出**：`/tmp/edgeguard_vision.json` + `/var/log/edgeguard/snapshots/*.jpg`
- **自动恢复**：摄像头拔出/插入自动重连
- **内核依赖**：`CONFIG_USB_VIDEO_CLASS=y`（uvcvideo.ko）
- **详细方案**：参见 `plan/AI_CAMERA_PLAN.md`

---

## 硬件资源

| 外设 | 接口 | 设备节点 |
|------|------|---------|
| MPU6050 | I2C1, 0x68 | `/dev/mpu6050_raw` |
| AP3216C | I2C1, 0x1E | `/dev/ap3216c_raw` |
| RGB LED (4路) | GPIO | `/dev/edge_leds` |
| 蜂鸣器 | GPIO | `/dev/edge_buzzer` |
| 按键 | GPIO (中断) | `/dev/input/event0` |
| GT9157 触摸屏 | I2C1, 0x14 | `/dev/input/event*` |
| LCD | LCDIF 800×480 | `/dev/fb0` |
| USB OTG2 (Host) | USB 2.0 | `/dev/video0`（摄像头） |
| 以太网 | fec1/fec2 RMII | eth0/eth1 |

---

## 快速开始

### 环境要求

- **主机**：Windows（VS Code / Claude Code 编辑代码）
- **虚拟机**：Ubuntu 18.04（交叉编译）
- **目标板**：i.MX6ULL-S1 Pro, Linux 4.19.35
- **交叉编译器**：`arm-linux-gnueabihf-gcc`
- **PC 端可选**：mosquitto（MQTT broker）

### 编译

```bash
# 1. 从 Windows 同步代码到 VM
bash scripts/sync_to_vm.sh

# 2. SSH 进入 VM，编译所有二进制
cd ~/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/app
make all
# 产物:
#   sensor_hubd        — 核心守护进程（含 SQLite）
#   edgeguard_httpd    — HTTP 服务器（含 SQLite）
#   edgeguard_mqttd    — MQTT 客户端
```

### 编译 UI（在 VM 上）

```bash
cd ../ui
qmake EdgeGuardUI.pro
make
# 产物: edgeguard-ui
```

### 部署到板子

```bash
# 拷贝二进制到板子
scp app/sensor_hubd app/edgeguard_httpd app/edgeguard_mqttd root@192.168.10.2:/usr/local/bin/
scp ui/edgeguard-ui root@192.168.10.2:/imx6ull/ui/

# 拷贝 systemd 服务文件
scp scripts/*.service root@192.168.10.2:/etc/systemd/system/

# 在板子上安装并启动所有服务
ssh root@192.168.10.2
cd /imx6ull/scripts && chmod +x install_services.sh && ./install_services.sh
```

### 网络配置（网线直连 PC）

```
板子: ifconfig eth0 192.168.10.2 netmask 255.255.255.0
PC:   IPv4 属性 → 192.168.10.1 / 255.255.255.0
验证: ping 192.168.10.2
```

### 访问

| 入口 | 地址 | 说明 |
|------|------|------|
| Web Dashboard | `http://192.168.10.2:8080` | PC 浏览器打开 |
| API 状态查询 | `curl http://192.168.10.2:8080/api/status` | JSON 格式 |
| 报警历史 | `curl http://192.168.10.2:8080/api/alarms?limit=10` | SQLite 查询 |
| LCD 触摸屏 | 板子本地 | Qt5 linuxfb 界面 |
| MQTT 订阅 | `mosquitto_sub -t 'edgeguard/#' -v` | PC 端运行 |

### 命令操作

```bash
# 静音蜂鸣器
curl -u admin:edgeguard "http://192.168.10.2:8080/api/cmd?cmd=mute_buzzer"

# 确认报警
curl -u admin:edgeguard "http://192.168.10.2:8080/api/cmd?cmd=ack_alarm"

# 触发测试报警
curl -u admin:edgeguard "http://192.168.10.2:8080/api/cmd?cmd=demo_alarm"
```

---

## 配置

配置文件 `/etc/edgeguard/config.json`（首次启动自动生成）：

```json
{
  "sample_interval_ms": 500,
  "als_low_threshold": 80,
  "ps_warning_threshold": 120,
  "ps_alarm_threshold": 220,
  "motion_warning_threshold": 8000,
  "motion_alarm_threshold": 15000,
  "buzzer_enable": true,
  "led_enable": true,
  "log_enable": true
}
```

---

## 状态机

```
                    ┌─────────────────────────────┐
                    │        NORMAL (绿灯常亮)      │
                    └───┬─────────┬─────────┬─────┘
       运动>8000        │   光<80  │  接近>120│
       接近>120         │         │         │
          ↓             ↓         ↓         │
   ┌──────────────────────────┐            │
   │  WARNING (黄灯 500ms闪烁)  │←──────────┘
   └──────────┬───────────────┘
     运动>15000│  接近>220
              ↓
   ┌──────────────────────────┐
   │  ALARM (红灯 250ms快闪+蜂鸣)│
   └──────────┬───────────────┘
              │ 连续3次传感器读取失败
              ↓
   ┌──────────────────────────┐
   │  FAULT (红灯 1000ms慢闪)    │
   └──────────────────────────┘

非 NORMAL 状态至少持续 2 秒（迟滞），防止传感器数据边界抖动。
```

---

## systemd 服务

| 服务 | 启动顺序 | 说明 |
|------|---------|------|
| `edgeguard.service` | 最先 | sensor_hubd，先加载内核模块 |
| `edgeguard-httpd.service` | After edgeguard | HTTP 服务器 :8080 |
| `edgeguard-mqttd.service` | After edgeguard | MQTT 客户端 |
| `edgeguard-ui.service` | After edgeguard + multi-user | Qt5 LCD 界面 |
| `edgeguard-visiond.service` | After multi-user | USB 摄像头视觉守护进程 |

所有服务均配置 `Restart=always`，异常退出自动重启。

---

## 开发约定

- 代码在 Windows 上编辑，不直接在 VM/板子上改代码
- 编译和部署由人工在 VM/板子上手动完成
- 修改驱动后需检查：Makefile、compatible 匹配、MODULE_DEVICE_TABLE、probe/remove
- 内核 API 需兼容 Linux 4.19.35
- 用户态代码零外部库依赖（仅 libc + pthread + SQLite amalgamation）
- 不要将 SSH 密码、API Key 写入代码
