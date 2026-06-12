# EdgeGuard AI 视觉 + 摄像头集成方案

> **状态**：规划完成，待采购 B525 后开始实施 | 最后更新：2026-06-09

---

## 0. 可行性评估

### 0.1 总体结论：**可行，推荐分级推进**

### 0.2 逐项评估

| 评估项 | 可行性 | 风险等级 | 说明 |
|--------|:------:|:--------:|------|
| USB UVC 摄像头驱动 | ✅ 高 | 🟢 已消除 | Logitech B525 经客服在 i.MX6ULL 上实测验证通过 |
| V4L2 单帧捕获 | ✅ 高 | 🟢 低 | 标准 Linux API，纯 C 调用，~200 行代码，零外部依赖 |
| 运动检测（像素差分） | ✅ 高 | 🟢 低 | 纯算法无模型，~10ms CPU |
| ncnn 交叉编译 | 🟡 中 | 🟡 中 | 官方支持 arm-linux-gnueabihf，需验证 NEON 宏 |
| 人脸检测 300-600ms | 🟡 中 | 🟡 中 | Ultra-Light Face Detector，理论可行，建议实测 |
| 512MB RAM 容纳模型 | ✅ 高 | 🟢 低 | visiond 峰值 ~12MB，占总量 2.3% |
| 人脸识别登录（P3） | 🟡 中 | 🟡 中 | 登录场景用户配合、2-3秒可接受，与监控场景需求不同 |
| YOLO-Tiny 目标检测（P4） | 🟡 中 | 🟡 中 | 模型 2-8MB，推理 ~1-2秒/帧，勉强可用 |
| 单核 CPU 并发影响 | 🟡 中 | 🟡 中 | 推理时传感器采集不受影响（中断驱动），HTTP 响应可能轻微延迟 |
| CSI 摄像头方案 | ❌ 不可行 | 🔴 高 | CSI 引脚与 RGB LED 冲突，需硬件改造，不推荐 |

### 0.3 推荐策略

```
P1 (安全区) → 零 AI 依赖，纯 C + V4L2，立即开始
P2 (探索区) → 先验证 ncnn 交叉编译，再写 face_detect 代码
P3 (增值区) → P2 完成后，用人脸识别登录增强用户体验
P4 (扩展区) → 按需推进
```

---

## 1. 硬件资源评估

### 1.1 i.MX6ULL-S1 Pro 关键参数

| 参数 | 数值 | 对 AI 的影响 |
|------|------|-------------|
| CPU | Cortex-A7 单核 @ 800MHz | 单帧推理 300-800ms，非实时视频 |
| NEON | VFPv4-D32 (ARMv7 SIMD) | AI 推理核心加速器，比纯 ARM 快 3-5x |
| RAM | 512MB DDR3 | 够用小模型（<10MB），内存敏感 |
| CMA | 320MB（设备树已配置） | 摄像头 framebuffer 充裕 |
| USB OTG2 | `dr_mode = "host"`（DTS 已配好） | USB UVC 摄像头即插即用 |
| PXP | 硬件像素管线（DTS 已启用） | 可做格式转换加速 |
| CSI | CSI_HSYNC/VSYNC 被 RGB LED 占用 | CSI 不可用，除非硬件改造 |

### 1.2 摄像头选择：Logitech B525 ✅（已确认兼容）

客服已在 i.MX6ULL 板卡上实测通过。B525 关键特性：

| 参数 | 数值 | 影响 |
|------|------|------|
| 接口 | USB 2.0 UVC 标准 | 免驱，uvcvideo.ko 即可，零驱动开发 |
| 分辨率 | 最高 720p (1280×720) | 推荐用 640×480 降低 CPU 负担 |
| 编码格式 | MJPEG + YUYV | MJPEG 硬件直出 JPEG，无需软件编码 |
| 对焦 | 自动对焦 | 人脸登录时画面始终清晰 |
| 麦克风 | 双麦克风 | 后期可做声音检测（P4 可选扩展） |
| 供电 | USB 总线供电 (~350mA) | OTG2 口 500mA 设计余量充足 |
| 物理设计 | 可折叠 360° 旋转 | 方便嵌入部署时调整拍摄角度 |

### 1.3 物理连接

```
┌─────────────────────────────────┐
│   i.MX6ULL-S1 Pro 开发板        │
│                                 │
│  [Micro USB]  OTG1 (OTG模式)    │ ← 供电/烧录，不接摄像头
│  [Type-A USB] OTG2 (Host模式)   │ ← B525 插这里！
│                                 │
│  [RJ45 网口]                    │
│  [LCD 排线接口]                  │
└─────────────────────────────────┘
```

设备树已配置（`dts/imx6ull-mmc-npi.dts`）：
```dts
&usbotg2 {
    dr_mode = "host";       // 已是 Host 模式
    disable-over-current;
    status = "okay";        // 已启用
};
```

**B525 USB 线直接插入 Type-A（OTG2）口即可，无需 USB Hub，无需焊接。**

### 1.4 摄像头接口对比

| 方案 | 硬件改动 | 驱动 | 风险 |
|------|---------|------|------|
| **USB UVC (B525)** ✅ | 零 | uvcvideo 主线驱动 | 无 |
| CSI 并行 | 需焊接+LED 迁移 | 需移植 NXP BSP | 高 |

> 结论：USB UVC 是唯一可行方案。CSI_HSYNC/VSYNC 已被 RGB 绿灯(GPIO4_IO20)和蓝灯(GPIO4_IO19)占用。

---

## 2. AI 框架选型

### 2.1 ncnn（腾讯，MIT License）—— 首选

| 维度 | 评估 |
|------|------|
| 定位 | 专为 ARM NEON 优化的移动端推理框架 |
| 依赖 | **零外部依赖**，纯 C++ 实现 |
| 交叉编译 | 官方提供 arm-linux-gnueabihf toolchain 文件 |
| 体积 | 精简编译后 ~2MB |
| Cortex-A7 性能 | 轻量人脸检测模型 ~300-600ms/帧 |
| 模型生态 | 大量移动端预训练模型可用 |

### 2.2 备选对比

| 框架 | 优点 | 缺点 |
|------|------|------|
| **ncnn** ✅ | 零依赖、ARM 优化最佳、模型轻量 | 需 C++11 编译器 |
| TensorFlow Lite | Google 支持、文档多 | 依赖复杂、ARMv7 比 ncnn 慢 |
| OpenCV DNN | 功能全 | 编译后 10-50MB，太大 |
| MCUNet/TinyML | 极致内存优化 | 面向裸机 MCU，不跑 Linux |

---

## 3. AI 模型分级

### Level 1：运动检测（无模型，零依赖）

- 相邻帧像素差分算法
- 检测画面中是否有物体移动
- CPU 占用极低（~10ms），纯 C 实现
- 作为"区域入侵检测"基础功能

### Level 2：人脸检测（Ultra-Light-Fast-Generic-Face-Detector-1MB）

- 模型大小：1.04MB（int8 量化后 ~300KB）
- 推理时间：~300-600ms/帧（Cortex-A7@800MHz）
- 检测画面中是否有面部（**不识别身份**）
- 与项目契合：未授权人员出现 → 触发 ALARM

### Level 3：人脸身份识别（MobileFaceNet）

- 模型大小：~5MB（fp16 量化）
- 推理时间：~800-1500ms（特征提取）
- 登录场景：正脸对准 → 2-3 秒识别出是谁 → 自动登录
- 注册库：2-5 个授权用户

### Level 4：通用目标检测（MobileNet-SSD / YOLO-Tiny，可选）

- 模型大小：~6-8MB（int8 ~2MB）
- 推理时间：~800-1500ms/帧
- 检测人、车、动物等多种目标
- 对 CPU/内存要求更高

---

## 4. 实施计划

### 4.1 P1 — USB 摄像头 + 运动检测（零 AI 依赖，1-2 天）

#### 4.1.1 内核准备（用户在 VM 上手动完成）

需确保以下 3 个内核配置项为 `=y`：

```
CONFIG_MEDIA_SUPPORT=y
CONFIG_MEDIA_USB_SUPPORT=y
CONFIG_USB_VIDEO_CLASS=y
```

板子验证命令：
```bash
zcat /proc/config.gz | grep -E "CONFIG_MEDIA_SUPPORT|CONFIG_MEDIA_USB|CONFIG_USB_VIDEO"
# 三项都为 y → 插入 B525 后 /dev/video0 自动出现

dmesg | grep -i uvc
# 期望输出：uvcvideo: Found UVC 1.00 device <unnamed> (046d:0825)

ls -la /dev/video*
# 确认设备节点存在
```

#### 4.1.2 B525 V4L2 捕获配置

| 参数 | 值 | 原因 |
|------|-----|------|
| 像素格式 | `V4L2_PIX_FMT_MJPEG` | B525 硬件 MJPEG 编码，免软件压缩 |
| 分辨率 | 640×480 | 平衡画质和 CPU 负担（B525 最高 1280×720） |
| 帧率 | 按需捕获（非连续流） | 每 2 秒拍一帧，B525 最高支持 30 FPS |
| 缓冲 | 2 个 mmap buffer | 双缓冲 VIDIOC_QBUF/DQBUF 轮转 |

#### 4.1.3 新增文件

| 文件 | 说明 | 行数 |
|------|------|------|
| `app/camera_v4l2.h` | V4L2 封装头文件：`struct camera_ctx` + `camera_open/capture/close` 接口 | ~30 |
| `app/camera_v4l2.c` | V4L2 标准 API 捕获：open → set_fmt(MJPEG 640×480) → req_bufs → mmap → QBUF/DQBUF → 拿到 JPEG 帧 | ~200 |
| `app/edgeguard_visiond.c` | 视觉守护进程：定时拍照 + 运动检测 + 写 `vision.json` + 存快照 | ~300 |
| `scripts/edgeguard-visiond.service` | systemd 服务文件 | ~15 |

#### 4.1.4 修改文件

| 文件 | 改动 |
|------|------|
| `app/sensor_hubd.c` | 读取 `/tmp/edgeguard_vision.json`，motion_detected/face_count 纳入状态机报警条件 |
| `app/edgeguard_httpd.c` | 新增 `GET /api/snapshot`（返回最新 JPEG 快照）、`GET /api/vision`（返回 vision JSON） |
| `ui/mainwindow.cpp` | 新增第 6 页 "Vision"：显示快照（QImage from JPEG）+ 检测结果（motion/face count）+ 摄像头状态 |
| `app/Makefile` | 新增 `edgeguard_visiond` target |
| `README.md` | 补充视觉模块文档 |

#### 4.1.5 IPC 数据格式

`/tmp/edgeguard_vision.json`（edgeguard_visiond 写入，其他进程读取）：
```json
{
  "camera_online": true,
  "motion_detected": false,
  "face_count": 0,
  "snapshot_path": "/var/log/edgeguard/snapshots/2026-06-09_12-00-00.jpg",
  "inference_ms": 12,
  "face_verify_result": null
}
```

#### 4.1.6 P1 预期效果

- PC 浏览器打开 Dashboard 可看到摄像头画面（`/api/snapshot`）
- 有人经过 → 运动检测触发 → WARNING 状态
- 报警自动保存带时间戳的快照到 `/var/log/edgeguard/snapshots/`
- LCD 第 6 页 "Vision" 实时显示摄像头状态和最新快照

---

### 4.2 P2 — ncnn 人脸检测（AI 核心，2-3 天）

#### 4.2.1 ncnn 交叉编译（用户在 VM 上操作）

```bash
# 前置验证：确认交叉编译器支持 NEON
arm-linux-gnueabihf-g++ -dM -E - < /dev/null | grep NEON
# 期望输出包含 __ARM_NEON 宏定义

# 下载并编译 ncnn
git clone https://github.com/Tencent/ncnn.git
cd ncnn
mkdir build-arm && cd build-arm
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/arm-linux-gnueabihf.toolchain.cmake \
      -DNCNN_BUILD_TOOLS=OFF -DNCNN_BUILD_EXAMPLES=OFF \
      -DNCNN_BUILD_BENCHMARK=OFF -DNCNN_VULKAN=OFF ..
make -j4
# 产物：src/libncnn.a (~2MB)
```

#### 4.2.2 新增/修改文件

| 文件 | 说明 |
|------|------|
| `app/face_detect.h` | 人脸检测接口：`struct face_result_t` + `detect_faces()` 声明 |
| `app/face_detect.cpp` | ncnn 封装：加载 Ultra-Light Face 模型 + 推理 + 返回人脸数量和边界框（~200 行 C++） |
| 板子目录 | `/etc/edgeguard/models/ultra_face.param` + `ultra_face.bin`（int8 量化 ~300KB） |
| `app/edgeguard_visiond.c` | 集成 face_detect：motion_detected → 跑人脸检测 → 有人脸则触发 face_intrusion |
| `app/sensor_hubd.c` | 状态机告警原因新增 `"face_intrusion"` / `"motion_detected"` |
| `app/Makefile` | C++ 编译支持 + 链接 libncnn.a |

#### 4.2.3 告警逻辑

```
V4L2 捕获一帧
    │
    ▼
运动检测（像素差分）
    │
    ├── 无运动 → NORMAL
    │
    └── 有运动 → 跑人脸检测（ncnn 推理 ~400ms）
                   │
                   ├── 有人脸 → ALARM (reason: face_intrusion) + 快照留存 + MQTT
                   │
                   └── 无人脸 → WARNING (reason: motion_detected)
```

#### 4.2.4 P2 预期效果

- 检测到人脸 → ALARM 状态 + LED + Buzzer + 快照留存 + MQTT 上报
- 检测到运动但无人脸 → WARNING 状态
- 无运动 → 正常
- Web/UI 可看到检测结果和报警快照

---

### 4.3 P3 — 人脸识别登录（P2 完成后，2-3 天）

#### 4.3.1 技术流水线

```
USB Camera → 捕获一帧 JPEG（~50ms）
       │
       ▼ Step 1: 人脸检测 — Ultra-Light Face Detector（~500ms）
       │         输出：画面中是否有脸 + 边界框坐标
       │
       ▼ Step 2: 人脸对齐 — 根据关键点裁切/缩放到 112×96（~20ms，纯算法）
       │
       ▼ Step 3: 特征提取 — MobileFaceNet ncnn 推理（~1000-1500ms）
       │         输出：128 维浮点特征向量
       │
       ▼ Step 4: 比对 — 特征向量与注册库做余弦相似度（<1ms）
       │         阈值 > 0.6 → 匹配成功，返回用户名
       │
       ▼ 结果：LoginPage 收到信号 → emit loginSuccess()
```

完整流程耗时：**~2-3 秒**（登录场景完全可接受）

#### 4.3.2 新增/修改文件

| 文件 | 说明 |
|------|------|
| `app/face_recognize.h/cpp` | ncnn 人脸特征提取 + 余弦比对封装（~150 行 C++） |
| 板子目录 | `/etc/edgeguard/models/mobilefacenet.param` + `.bin`（fp16 ~5MB） |
| 板子目录 | `/etc/edgeguard/face_db.json`（注册用户特征库，2-5 人） |
| `app/edgeguard_visiond.c` | 新增 `face_verify` IPC 命令：收到请求 → 拍照 → 检测 → 提取 → 比对 → 返回结果 |
| `ui/loginpage.h/cpp` | 新增 "Face Login" 按钮 + `onFaceLoginClicked()` + 状态提示 QLabel |
| `ui/mainwindow.cpp` | 新增 Settings 页人脸注册引导 |

#### 4.3.3 IPC 命令接口

```json
// UI → visiond：/tmp/edgeguard_vision_cmd.json
{ "cmd": "face_verify", "request_id": 1 }

// visiond → UI：/tmp/edgeguard_vision.json 中扩展字段
{
  ...
  "face_verify_result": {
    "request_id": 1,
    "success": true,
    "matched_user": "rickyduran",
    "confidence": 0.87
  }
}
```

#### 4.3.4 注册库格式

`/etc/edgeguard/face_db.json`：
```json
{
  "users": [
    {
      "name": "rickyduran",
      "embedding": [0.123, -0.456, ...],  // 128 float values
      "enrolled_at": "2026-06-09"
    }
  ]
}
```

#### 4.3.5 安全说明

- 人脸登录**不能替代密码**——照片可欺骗（Cortex-A7 无法跑活体检测）
- 设计为**便捷登录方式**（类 Demo 按钮），日志标记 `login_method: "face"`
- 如需更高安全 → 人脸 + PIN 双因子（P4 可选）

---

### 4.4 P4 — 扩展功能（按需选择）

#### 4.4.1 云边协同（推荐）

板子做前端采集+人脸检测，将人脸图像通过 MQTT/HTTP 上传到 PC 或云服务器做：
- 精确人脸识别
- 车牌识别
- 行为分析

板子角色：**边缘采集 + 前端粗筛**，PC 角色：**云端精细推理**

#### 4.4.2 声音检测

B525 双麦克风可做：
- 异常响声检测（玻璃破碎、撞击声）
- 声级报警（分贝超过阈值）
- 需要开启内核 `CONFIG_SND_USB_AUDIO`

#### 4.4.3 YOLO-Tiny 通用目标检测

- 模型 ~8MB，推理 ~1-2 秒/帧
- 检测人、车、动物等多种目标
- 需要更高的 CPU 和内存

#### 4.4.4 Coral TPU 加速

如果买到 Google Coral USB Accelerator：
- 推理 500ms → <50ms
- 可跑更大模型（MobileNet-SSD、EfficientDet）
- 但 Coral USB 已停产，二手市场可能找到

---

## 5. 系统架构

```
Logitech B525 (/dev/video0)
       │
       ▼
edgeguard_visiond (V4L2 capture + Motion detect + ncnn inference)
       │
       ├── /var/log/edgeguard/snapshots/*.jpg    (报警快照)
       │
       ├── /tmp/edgeguard_vision.json            (检测结果)
       │
       ├── /tmp/edgeguard_vision_cmd.json        (UI → visiond 命令)
       │
       └── ─ ─ ─ ─ ─ ─ ─ ─ IPC 消费者 ─ ─ ─ ─ ─ ─ ─ ─
              │
              ├─ sensor_hubd 读取  → 状态机 (face_intrusion → ALARM)
              │                       └── LED + Buzzer + alarms.db
              │
              ├─ edgeguard_httpd 读取 → /api/snapshot + /api/vision
              │                       └── Dashboard Web 显示
              │
              ├─ edgeguard_mqttd 读取 → edgeguard/vision topic
              │
              └─ edgeguard-ui 读取 → Vision 页面显示快照
                                   → Face Login 调用 face_verify 命令
```

IPC 模式不变——所有进程通过 JSON 文件通信，无需额外的 socket/消息队列。

---

## 6. 性能预估

### 6.1 时延估算

| 操作 | Cortex-A7 @ 800MHz | 备注 |
|------|:---:|---|
| V4L2 捕获 MJPEG 帧 | 30-50ms | 640×480，B525 硬件 MJPEG 编码 |
| JPEG 解码到 RGB | 15-20ms | libjpeg（可选 NEON 加速） |
| 运动检测（像素差分） | 8-12ms | 320×240 降采样后计算 |
| 人脸检测（ncnn） | 300-600ms | Ultra-Light Face Detector，320×240 输入 |
| 人脸特征提取（ncnn） | 800-1500ms | MobileFaceNet，112×96 输入 |
| 特征向量比对（余弦） | <1ms | 2-5 个注册用户 |
| **P1 一帧周期（运动检测）** | **~60-80ms** | |
| **P2 一帧周期（人脸检测）** | **~400-700ms** | ~1.5-2.5 FPS |
| **P3 人脸登录完整流程** | **~2-3 秒** | 检测+提取+比对 |

### 6.2 内存预估

| 组件 | 内存 | 备注 |
|------|:---:|------|
| edgeguard_visiond 基础 | ~3MB | 代码+栈+堆 |
| MJPEG 帧缓冲 (640×480) | ~300KB | V4L2 mmap buffer × 2 |
| JPEG 解码工作区 | ~500KB | libjpeg |
| Ultra-Light Face Detector 模型 | ~300KB | int8 量化 |
| MobileFaceNet 模型（登录用） | ~5MB | fp16，仅在识别时加载 |
| ncnn 运行时 | ~2MB | 推理中间张量 |
| **P1 总计** | **~4MB** | |
| **P2 总计** | **~7MB** | |
| **P3 人脸登录总计** | **~12MB** | 模型按需加载/卸载 |
| **占 512MB 比例** | **~2.3%** | 即使全部加载也充裕 |

---

## 7. 硬件购买清单

| 物品 | 推荐型号 | 状态 | 价格 | 用途 |
|------|---------|:---:|------|------|
| USB 摄像头 | **Logitech B525** | ✅ 已确认兼容 | ~100-200 元 | 720p 拍照 + 人脸检测 |
| USB 延长线 | 公对母 1m | 按需 | ~5 元 | 灵活布置摄像头位置 |

> B525 已被客服在 i.MX6ULL 板卡上实机测试通过，无需犹豫。若 B525 不易购买，其他 UVC 免驱摄像头（如 Logitech C270）也可以替代，仅失去自动对焦和麦克风。

---

## 8. 实施顺序

```
Phase P1 — 摄像头 + 运动检测（1-2 天，零 AI 依赖）
├── 购买 Logitech B525
├── 确认内核 UVC 驱动（3 个 CONFIG_）
├── B525 插入 OTG2 Type-A 口 → 验证 /dev/video0
├── 编写 camera_v4l2.h/c（V4L2 MJPEG 捕获）
├── 编写 edgeguard_visiond.c（拍照 + 运动检测）
├── HTTP /api/snapshot + /api/vision
├── Qt UI 第 6 页 "Vision"（快照 + 状态）
├── Makefile + systemd service
└── 测试：浏览器 → 摄像头画面；人经过 → 运动告警

Phase P2 — ncnn 人脸检测（2-3 天）
├── 交叉编译 ncnn（arm-linux-gnueabihf）
├── 下载 Ultra-Light Face Detector 模型 → /etc/edgeguard/models/
├── 编写 face_detect.h/cpp（ncnn 封装）
├── 集成到 edgeguard_visiond（运动+人脸检测）
├── sensor_hubd 状态机新增 face_intrusion / motion_detected
├── Web/UI 显示人脸检测结果
└── 测试：人脸出现在摄像头前 → ALARM + 快照 + MQTT

Phase P3 — 人脸识别登录（2-3 天，P2 完成后）
├── 下载 MobileFaceNet ncnn 模型
├── 编写 face_recognize.h/cpp（特征提取 + 比对）
├── face_db.json 注册库
├── edgeguard_visiond 新增 face_verify IPC 命令
├── LoginPage 增加 "Face Login" 按钮 + 状态提示
├── Settings 增加人脸注册引导
└── 测试：正脸对准 → 识别 → 自动登录

Phase P4 — 扩展（按需）
├── 云边协同（MQTT 上传快照到 PC/云）
├── YOLO-Tiny 通用目标检测
├── 声音检测（B525 双麦克风）
└── Coral TPU 加速（如买到）
```

---

## 9. 风险与缓解

| 风险 | 等级 | 缓解 |
|------|:----:|------|
| ~~USB 摄像头兼容性~~ | 🟢 已消除 | B525 经客服在 i.MX6ULL 上实测验证通过 |
| 内核未开启 UVC 驱动 | 🟢 低 | 提供 3 个明确 CONFIG_ 宏，可直接编辑 .config 或 make menuconfig |
| B525 供电不足 | 🟢 低 | B525 典型 ~350mA，OTG2 最大 500mA，余量充足 |
| 512MB RAM 不足 | 🟢 低 | visiond 峰值 ~12MB，占总量 2.3% |
| ncnn 交叉编译困难 | 🟡 中 | 腾讯官方 ARM toolchain 文件；编译前先验证 NEON 宏 |
| 模型加载失败 | 🟡 中 | 充分错误处理，fallback 到纯运动检测模式 |
| Cortex-A7 推理太慢 | 🟡 中 | 2-5 秒周期拍照，降分辨率到 320×240，int8 量化 |
| 人脸登录照片欺骗 | 🟡 中 | 设计为便捷方式（非唯一认证），日志标记 login_method |
| 设备树节点冲突 | 🟢 低 | USB Host 已配置，无需修改 DTS |

---

## 10. 附录

### 10.1 插入 B525 后的验证步骤

```bash
# 1. 确认 USB 设备被识别
lsusb
# 期望：Bus 001 Device 002: ID 046d:0825 Logitech, Inc.

# 2. 确认内核加载 uvcvideo 驱动
dmesg | grep -i uvc
# 期望：uvcvideo: Found UVC 1.00 device <unnamed> (046d:0825)

# 3. 确认 V4L2 设备节点
ls -la /dev/video*
# 期望：crw-rw---- 1 root video 81, 0 ... /dev/video0

# 4. 确认支持的格式（需要 v4l-utils，可选）
v4l2-ctl --device=/dev/video0 --list-formats
# 期望输出包含 MJPG 和 YUYV

# 5. 快速拍照测试（需要 v4l-utils，可选）
v4l2-ctl --device=/dev/video0 --set-fmt-video=width=640,height=480,pixelformat=MJPG \
         --stream-mmap --stream-count=1 --stream-to=test.jpg
```

### 10.2 相关文档

- [ncnn 官方仓库](https://github.com/Tencent/ncnn)
- [Ultra-Light-Fast-Generic-Face-Detector-1MB](https://github.com/Linzaer/Ultra-Light-Fast-Generic-Face-Detector-1MB)
- [MobileFaceNet](https://github.com/XiaoMi/mobilefacenet)
- [Linux UVC 驱动文档](https://www.kernel.org/doc/html/v4.19/media/uapi/v4l/v4l2.html)
