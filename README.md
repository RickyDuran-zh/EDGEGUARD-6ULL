# EdgeGuard-6ULL

基于 i.MX6ULL-S1 Pro 开发板的边缘安全监控系统。

> **当前版本**：P2 JPEG 解码已修复（libjpeg-turbo） | P3 人脸识别开发中（MobileFaceNet） | Web UI SVG 图标 + Toggle 开关 | 一键部署 deploy.sh | 最后更新：2026-06-26

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
│   ├── camera_v4l2.h / .c      # V4L2 摄像头捕获封装（MJPEG 640×480, mmap 4缓冲, select超时3s, flush前2帧）
│   ├── edgeguard_visiond.c     # 视觉守护进程：3模式(monitor/facelogin/tamper) + cmd监听 + 运动检测 + 遮挡检测 + 人脸检测 + vision.json输出
│   ├── face_detect.h           # 人脸检测+识别 C/C++ 桥接接口（extern "C"，5个公开API）
│   ├── face_detect.c           # 纯 C stub 实现（零 AI 依赖，总是返回 face_count=0，保证无 ncnn 时可编译）
│   ├── face_detect.cpp         # ncnn 完整推理实现（-DEDGEGUARD_USE_NCNN）：libjpeg-turbo JPEG解码 → ultra_face 检测 → MobileFaceNet 128维识别 → cosine 相似度比对 → face_db.json 读写
│   ├── face_register.c         # 人脸注册 CLI 工具：./face_register <用户名> <JPEG路径> → 提取特征 → 写入 face_db.json
│   └── Makefile                # 交叉编译 makefile（7个target + C/C++ 分离编译 + edgeguard_visiond_face + face_register）
│
├── ui/                         # Qt5 本地触摸屏界面（C++）
│   ├── main.cpp                # 入口：QApplication + showFullScreen()
│   ├── loginpage.h / .cpp      # 登录页：虚拟键盘 + 3次失败锁定30s + 密码/人脸/演示三入口
│   ├── faceloginpage.h / .cpp  # 人脸登录页：摄像头实时预览(250ms刷新) + 30s超时 + 占位提示(P2修复后自动生效)
│   ├── mainwindow.h / .cpp     # 主窗口：8页 QStackedWidget (Login→FaceLogin→Dashboard→...→Chart) + 触摸滑动 + 顶栏Logout + JSON解析
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
│   ├── build_deploy.sh         # VM 端一键编译 + 打包到 sharedir（支持参数化：httpd|visiond|ui|...）
│   ├── deploy.sh               # 开发板端一键部署（stop→cp→restart→clean，支持参数化）
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
│      │  │  │  ┌───────────┴──────────────────┐    │               │
│      │  │  │  │ edgeguard_visiond              │    │               │
│      │  │  │  │ (USB摄像头守护进程, 3模式)       │    │               │
│      │  │  │  │                                │    │               │
│      │  │  │  │ monitor: 2s运动检测+人脸触发    │    │               │
│      │  │  │  │ facelogin: 300ms预览+人脸检测  │    │               │
│      │  │  │  │ tamper: 2s运动+遮挡检测        │    │               │
│      │  │  │  │ 读cmd.json, 写preview.jpg     │    │               │
│      │  │  │  └──────────┬────────────────────┘    │               │
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
| `/tmp/edgeguard_cmd.json` | HTTP / UI / 手动 | sensor_hubd / edgeguard_visiond | 命令（mute/ack/demo/set_config/delete/vision_mode，读取后删除） |
| `/tmp/edgeguard_vision.json` | edgeguard_visiond | sensor_hubd / HTTP / UI | 摄像头状态 + 运动/人脸/遮挡检测结果（monitor/tamper: 2s刷新, facelogin: 300ms刷新, 原子写） |
| `/tmp/edgeguard_camera_preview.jpg` | edgeguard_visiond | Qt UI (FaceLoginPage) | facelogin 模式实时预览帧（300ms刷新, 原子写: .tmp → rename） |
| `/var/log/edgeguard/alarms.db` | sensor_hubd | HTTP | 报警历史（SQLite WAL模式） |
| `/var/log/edgeguard/snapshots/` | edgeguard_visiond | HTTP | 报警 JPEG 快照（最多50张，超出自动清理） |
| `/etc/edgeguard/config.json` | sensor_hubd | sensor_hubd | 传感器阈值配置（首次启动自动生成，Web Settings 即时生效） |
| `/etc/edgeguard/face_db.json` | face_register | edgeguard_visiond | 人脸数据库（128维embedding，cosine相似度比对） |
| `/var/log/edgeguard/face_count.dat` | edgeguard_visiond | edgeguard_visiond | 累计人脸数量持久化（重启不丢失） |
| `/etc/edgeguard/users.json` | 管理员 | Qt UI | LCD 登录用户凭证（可选，不存在则用默认值） |

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

**Web Dashboard（多页面 SPA）**：4 个页面纯前端路由切换，零外部依赖，SVG 矢量图标替代 emoji。

| 页面 | 内容 |
|------|------|
| **Dashboard** | 4 卡片概览栏（状态/报警/运行/IP，SVG 图标前缀）+ MPU6050 + AP3216C + 设备控制（静音/确认/测试按钮含 SVG 图标）+ 最近 5 条报警 |
| **Alarms** | SQLite 报警历史表格 + 分页 + 清理早期记录 |
| **Camera** | 实时快照预览 + 6 卡片（摄像头/模式/运动/当前人脸/累计人脸/推理耗时）+ 最新快照路径 + 人脸快照路径 |
| **Settings** | 6 项传感器阈值输入（卡片式 hover 高亮）+ 3 个 Toggle 开关（蜂鸣器/LED/日志）+ 系统信息 |

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

- **8 页**：Login / FaceLogin / Dashboard / Sensors / Alarms / System / Vision / Chart
- **登录页**：3 入口——密码登录（虚拟键盘 + 3次失败锁定30s）、人脸识别登录（摄像头实时预览 + 占位提示）、演示模式
- **人脸登录页（P3 新增）**：实时摄像头预览（QTimer 250ms 刷新 `/tmp/edgeguard_camera_preview.jpg`）、30s 超时自动返回、"功能开发中"占位提示（P2 修复后自动生效）
- **布局**：左侧166px侧边栏（6个导航按钮 + Brand）+ 顶栏Logout按钮 + 右侧内容区
- **顶栏**：登录页和FaceLogin页隐藏，认证后显示 "EdgeGuard 6ULL" 品牌 + Logout 按钮
- **触摸**：通过 Qt linuxfb QPA 的 QMouseEvent 实现上下滑动切页、点击侧边栏切页
- **登录/人脸页**：滑动被完全禁用，只能通过按钮操作
- **虚拟键盘**：QWERTY 全键盘 + Shift/Backspace/Toggle/Space/.com + Enter 自动跳转密码框
- **键盘**：按键 1-6 切页（认证页）、Esc 退出
- **Demo 模式**：`--demo` 参数模拟传感器数据（含视觉 mock 数据 + tamper 模拟）
- **登录认证**：用户名/密码 + 虚拟键盘，3 次失败锁定 30 秒，支持自定义 `/etc/edgeguard/users.json`
- **登录后自动切换**：密码/演示登录成功后发送 `vision_mode: tamper`，登出时发送 `vision_mode: monitor`
- **Vision 页**：显示 Camera 在线/遮挡状态 / Motion 检测 / Face Count / Inference 耗时 / 最新快照路径

### edgeguard_visiond — USB 摄像头视觉守护进程（P3 ✅ 已部署）

- **3 种运行模式**，通过 cmd.json 动态切换：
  - **monitor**（默认）：2000ms 间隔，运动检测 + 运动触发人脸检测，快照留存
  - **facelogin**：300ms 间隔，每帧写预览帧 `/tmp/edgeguard_camera_preview.jpg`（固定路径覆盖），每帧跑人脸检测，不存快照
  - **tamper**（登录后）：2000ms 间隔，运动检测 + **遮挡检测** + 运动触发人脸检测
- **模式切换**：监听 `/tmp/edgeguard_cmd.json` 解析 `{"cmd":"vision_mode","mode":"..."}` 命令（和 sensor_hubd 共用同一个 cmd 通道，visiond 只处理 vision_mode 命令）
- **摄像头**：USB UVC (Logitech B525)，V4L2 MJPEG 640×480 捕获，mmap 4 缓冲，select 3s 超时，前 2 帧 flush
- **运动检测**：JPEG 帧文件大小变化启发式检测（阈值 15%），零 AI 依赖
- **遮挡检测（P3 新增）**：连续 3 帧 JPEG size < 5KB（纯黑/纯白画面）→ `tamper_detected: true`
- **快照留存**：monitor/tamper 模式下每帧保存到 `/var/log/edgeguard/snapshots/`，**最多保留 50 张**（超出自动删最旧）；facelogin 模式不存快照
- **累计人脸**：`g_total_face_count` 跨帧累计，持久化到 `/var/log/edgeguard/face_count.dat`（重启不丢失）
- **自动恢复**：摄像头拔出/插入自动重连（hotplug），掉线期间写 `camera_online: false`
- **内核依赖**：`CONFIG_USB_VIDEO_CLASS=m`（uvcvideo.ko），B525 即插即用
- **详细方案**：参见 `plan/AI_CAMERA_PLAN.md`、`plan/p3-face-login-plan.md`

**输出格式** (`/tmp/edgeguard_vision.json`)：
```json
{
  "mode": "monitor",
  "camera_online": true,
  "motion_detected": false,
  "face_count": 1,
  "total_face_count": 42,
  "last_face_snapshot": "/tmp/edgeguard_camera_preview.jpg",
  "snapshot_path": "/var/log/edgeguard/snapshots/20260626_120000.jpg",
  "preview_path": "null",
  "tamper_detected": false,
  "inference_ms": 85,
  "timestamp": "2026-06-26 12:00:00",
  "face_verify_result": null
}
```

**各模式触发状态机**：
```
monitor: motion_detected=true → sensor_hubd WARNING（黄灯闪烁）
         face_count>0         → sensor_hubd ALARM（红灯+蜂鸣器）【P2修复后生效】

facelogin: 写preview.jpg → Qt UI FaceLoginPage 实时显示
           face_count>0 → face_verify_result.matched=true → 登录成功【P2修复后生效】

tamper:   motion_detected=true → sensor_hubd WARNING
          tamper_detected=true → sensor_hubd ALARM（"摄像头被遮挡"）
          face_count>0         → sensor_hubd ALARM【P2修复后生效】
```

**手动模式切换命令**：
```bash
echo '{"cmd":"vision_mode","mode":"facelogin"}' > /tmp/edgeguard_cmd.json  # UI 人脸登录预览
echo '{"cmd":"vision_mode","mode":"tamper"}'    > /tmp/edgeguard_cmd.json  # 登录后遮挡检测
echo '{"cmd":"vision_mode","mode":"monitor"}'   > /tmp/edgeguard_cmd.json  # 恢复默认
```

### face_detect — 人脸检测+识别模块（P2 ✅ 完成, P3 开发中）

- **双模式架构**：
  - **Stub 模式**（默认）：`face_detect.c` 纯 C，零依赖，总是返回 `face_count=0`，保证无 ncnn 时可编译运行
  - **NCNN 模式**：`face_detect.cpp`（`-DEDGEGUARD_USE_NCNN`），两个 AI 模型协同工作
- **接口**：纯 C 接口（`extern "C"`），5 个公开 API：
  | 函数 | 用途 |
  |------|------|
  | `face_detect_init()` | 加载 ultra_face 模型 + 预计算 4420 个 SSD 先验框 |
  | `face_detect_run()` | JPEG → libjpeg 解码 → 检测 → 返回 face_count |
  | `face_recog_init()` | 加载 MobileFaceNet 模型 + face_db.json |
  | `face_verify_run()` | 完整管线：检测 → 对齐(112×96) → 提取 128 维 → cosine 比对 → 匹配结果 |
  | `face_register_user()` | 注册新用户：检测 → 提取特征 → 写入 face_db.json |
- **检测模型**：Ultra-Light-Face-Detector-1MB (RFB-320)，输入 320×240 RGB，输出 4420 候选框 → NMS(IoU=0.4)→ 置信度≥0.7
- **识别模型**：MobileFaceNet，输入 112×96 RGB（模型自带归一化），输出 128 维 L2 归一化向量 → cosine 相似度≥0.55 匹配
- **JPEG 解码**：libjpeg-turbo（`jpeg_mem_src()` + setjmp 错误处理），已替换 stb_image ✅
- **模型文件**（`/etc/edgeguard/models/`）：`ultra_face.param/.bin` + `mobilefacenet.param/.bin`
- **人脸数据库**：`/etc/edgeguard/face_db.json`，最多 10 用户，128 维 float embedding
- **注册工具**：`./face_register <用户名> <JPEG路径>` — 离线录入新用户
- **编译命令**：`make edgeguard_visiond_face NCNN_DIR=/path/to/ncnn JPEG_DIR=/opt/libjpeg-arm`

**当前状态**：
- ✅ 人脸检测（ultra_face + libjpeg-turbo）：测试通过，`face_count` 正常
- ✅ 模型加载：ultra_face + MobileFaceNet 均加载成功
- 🔧 MobileFaceNet extract 调试中：`ex.extract("fc1")` 返回非 0，待确认输出层名
- 📍 相关文件：`app/face_detect.cpp`（928行核心引擎）、`app/face_register.c`（CLI 注册工具）

### P3 人脸登录入口 + 遮挡检测（✅ 已实现）

**登录页 3 入口**：
```
LoginPage:  [密码登录]  [人脸识别登录]  [演示]
                │            │             │
                ▼            ▼             ▼
           密码验证成功   FaceLoginPage   演示模式
                │       (实时预览+占位)      │
                │            │             │
                └────────────┼─────────────┘
                             ▼
                    认证成功 → vision_mode: tamper
                             → Dashboard
```

**FaceLoginPage 数据流**：
```
UI 点击"人脸识别登录"
  → sendCommand({"cmd":"vision_mode","mode":"facelogin"})
  → visiond 切换到 300ms 快速采集
  → visiond 每帧写入 /tmp/edgeguard_camera_preview.jpg (覆盖)
  → FaceLoginPage QTimer 250ms 读取 → QLabel.setPixmap() 显示
  → 用户看到实时画面 (3-4 fps)
  → 点击"返回密码登录"或 30s 超时 → sendCommand({"cmd":"vision_mode","mode":"monitor"})
  → 回到 LoginPage
```

**登录后遮挡检测**：
```
密码/演示登录成功
  → sendCommand({"cmd":"vision_mode","mode":"tamper"})
  → visiond tamper 模式: 2s 间隔 + 运动检测 + 遮挡判断
  → 连续3帧 JPEG size < 5KB → tamper_detected: true
  → sensor_hubd 读取 → STATE_ALARM → 蜂鸣器 + 红灯
  → UI Vision 页显示 "被遮挡" (红色加粗)
```

**设计要点**：
- FaceLoginPage 的预览功能完全独立于 P2 人脸检测 —— visiond 直接转发 JPEG 不做解码
- `faceLoginSuccess` 信号已预留，P2 JPEG 修复后读取 vision JSON 的 `face_count>0` 即可自动触发
- visiond 和 sensor_hubd 共用 `/tmp/edgeguard_cmd.json`，各处理自己的命令，互不干扰
- 登出时自动发送 `vision_mode: monitor` 恢复正常监控模式

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
#   sensor_hubd               — 核心守护进程（含 SQLite）
#   edgeguard_httpd           — HTTP 服务器（含 SQLite）
#   edgeguard_mqttd           — MQTT 客户端
#   edgeguard_visiond         — 视觉守护进程（含 face_detect stub）

# 人脸识别版（需要先交叉编译 ncnn + libjpeg-turbo）：
# make edgeguard_visiond_face NCNN_DIR=/home/user/ncnn JPEG_DIR=/opt/libjpeg-arm
# make face_register          NCNN_DIR=/home/user/ncnn JPEG_DIR=/opt/libjpeg-arm
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
# 方式一：一键部署（推荐）
# 将编译产物拷贝到 VM 的 sharedir，然后在开发板上执行：
sudo sh /imx6ull/scripts/deploy.sh            # 全部部署
sudo sh /imx6ull/scripts/deploy.sh httpd      # 只部署 Web 服务
sudo sh /imx6ull/scripts/deploy.sh ui visiond # 只部署 UI + 摄像头

# 方式二：手动部署
scp app/sensor_hubd app/edgeguard_httpd app/edgeguard_mqttd app/edgeguard_visiond root@192.168.10.2:/imx6ull/app/
scp ui/edgeguard-ui root@192.168.10.2:/imx6ull/ui/
scp scripts/*.service root@192.168.10.2:/etc/systemd/system/

# 在板子上安装并启动所有服务
ssh root@192.168.10.2
systemctl daemon-reload
systemctl enable edgeguard edgeguard-httpd edgeguard-mqttd edgeguard-visiond edgeguard-ui
systemctl restart edgeguard edgeguard-httpd edgeguard-mqttd edgeguard-visiond edgeguard-ui
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
| Web Dashboard | `http://192.168.10.2:8080` | PC 浏览器打开，4页SPA（Dashboard/Alarms/Camera/Settings） |
| 摄像头实时画面 | `http://192.168.10.2:8080/api/snapshot` | 最新 JPEG 快照，image/jpeg 直出 |
| 视觉检测结果 | `curl http://192.168.10.2:8080/api/vision` | JSON 格式（motion/face） |
| API 状态查询 | `curl http://192.168.10.2:8080/api/status` | 完整传感器+视觉 JSON |
| 报警历史 | `curl http://192.168.10.2:8080/api/alarms?limit=10` | SQLite 查询，支持分页 |
| LCD 触摸屏 | 板子本地 | Qt5 linuxfb 界面，8页（含人脸登录页实时预览） |
| MQTT 订阅 | `mosquitto_sub -t 'edgeguard/#' -v` | PC 端运行 |

### 命令操作

```bash
# 静音蜂鸣器
curl -u admin:edgeguard "http://192.168.10.2:8080/api/cmd?cmd=mute_buzzer"

# 确认报警
curl -u admin:edgeguard "http://192.168.10.2:8080/api/cmd?cmd=ack_alarm"

# 触发测试报警
curl -u admin:edgeguard "http://192.168.10.2:8080/api/cmd?cmd=demo_alarm"

# 切换 visiond 模式（直接写 cmd.json）
echo '{"cmd":"vision_mode","mode":"facelogin"}' > /tmp/edgeguard_cmd.json  # 人脸登录预览
echo '{"cmd":"vision_mode","mode":"tamper"}'    > /tmp/edgeguard_cmd.json  # 遮挡检测
echo '{"cmd":"vision_mode","mode":"monitor"}'   > /tmp/edgeguard_cmd.json  # 默认监控

# 验证预览帧和遮挡状态
ls -la /tmp/edgeguard_camera_preview.jpg   # facelogin 模式下应 < 3s 更新
cat /tmp/edgeguard_vision.json | python -m json.tool  # 检查 mode/tamper_detected 字段
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
         视觉运动检测 ════════╝         │         │
               ↓             ↓         ↓         │
        ┌──────────────────────────┐            │
        │  WARNING (黄灯 500ms闪烁)  │←──────────┘
        └──────────┬───────────────┘
          运动>15000│  接近>220
       视觉人脸检测 ═╡  (P2: face_count>0)
        摄像头遮挡  ═╡  (P3: tamper_detected=true)
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

视觉联动（P3 已部署）：
  motion_detected=true  → WARNING（与传感器运动/接近检测同级）
  tamper_detected=true  → ALARM   （P3: 摄像头被遮挡，最高优先级）
  face_count>0          → ALARM   （P2: ncnn 修复后生效）

visiond 模式切换：
  monitor   (默认)    — 2s 间隔，运动检测，快照留存
  facelogin (登录预览) — 300ms 间隔，写预览帧，每帧人脸检测
  tamper    (登录后)   — 2s 间隔，运动检测 + 遮挡检测
```

---

## systemd 服务

| 服务 | 启动顺序 | 说明 |
|------|---------|------|
| `edgeguard.service` | 最先 | sensor_hubd，先加载内核模块 |
| `edgeguard-httpd.service` | After edgeguard | HTTP 服务器 :8080 |
| `edgeguard-mqttd.service` | After edgeguard | MQTT 客户端 |
| `edgeguard-ui.service` | After edgeguard + multi-user | Qt5 LCD 界面 |
| `edgeguard-visiond.service` | After multi-user | USB 摄像头视觉守护进程（3模式） |

所有服务均配置 `Restart=always`，异常退出自动重启。

---

## 开发约定

- 代码在 Windows 上编辑，不直接在 VM/板子上改代码
- 编译和部署由人工在 VM/板子上手动完成
- 修改驱动后需检查：Makefile、compatible 匹配、MODULE_DEVICE_TABLE、probe/remove
- 内核 API 需兼容 Linux 4.19.35
- 用户态代码零外部库依赖（仅 libc + pthread + SQLite amalgamation）
- P1 编译仅需 gcc；P2 需要 g++（C++11）+ ncnn 库 + libjpeg-turbo
- face_detect 使用 stub/full 双模式：stub 保证 P1/P3 独立可编译运行，full 模式按需激活
- 所有 IPC 文件使用原子写（.tmp → rename），避免读者读到半写数据
- visiond 和 sensor_hubd 共用 `/tmp/edgeguard_cmd.json`，各自按 `cmd` 字段过滤：sensor_hubd 处理 mute/ack/demo，visiond 只处理 vision_mode
- 不要将 SSH 密码、API Key 写入代码
