# EdgeGuard AI 视觉 + 摄像头集成方案

> **状态**：规划中 | 最后更新：2026-06-08

## 0. 可行性评估

### 0.1 总体结论：**可行，但需分级推进，P2 以上有中等风险**

### 0.2 逐项评估

| 评估项 | 可行性 | 风险等级 | 说明 |
|--------|--------|---------|------|
| USB UVC 摄像头驱动 | :white_check_mark: 高 | :green_circle: 低 | OTG2 已配 Host，Linux 4.19 uvcvideo 成熟，内核开启 3 个 CONFIG_ 即可 |
| V4L2 单帧捕获 | :white_check_mark: 高 | :green_circle: 低 | 标准 Linux API，纯 C 调用，~200 行代码，无数依赖 |
| 运动检测（像素差分） | :white_check_mark: 高 | :green_circle: 低 | 纯算法无模型，~10ms CPU，10 行核心代码 |
| ncnn 交叉编译 | :yellow_circle: 中 | :yellow_circle: 中 | 官方支持 arm-linux-gnueabihf，但 Ubuntu 18.04 的 CMake/gcc 版本可能偏旧，需验证 |
| 人脸检测 300-500ms | :yellow_circle: 中 | :yellow_circle: 中 | 理论可行，实际受 NEON 优化程度、内存带宽影响，可能 500-800ms，建议实测 |
| 512MB RAM 容纳模型 | :white_check_mark: 高 | :green_circle: 低 | Ultra-Light 模型 int8 量化 ~300KB，visiond 进程整体 ~15MB |
| 人脸识别（P3） | :red_circle: 低 | :red_circle: 高 | 单核 Cortex-A7 做人脸特征提取+比对约 1-3 秒，实用性差；建议走云边协同 |
| YOLO-Tiny 目标检测（P3） | :yellow_circle: 中 | :yellow_circle: 中 | 模型 2-8MB，推理 ~1-2 秒/帧，勉强可用但不流畅 |
| 单核 CPU 并发影响 | :yellow_circle: 中 | :yellow_circle: 中 | 推理时 sensor_hubd 采集仍可运行（中断驱动），但 HTTP 响应可能延迟 |
| CSI 摄像头方案 | :red_circle: 不可行 | :red_circle: 高 | CSI 引脚与 RGB LED 冲突，需硬件改造，不推荐 |

### 0.3 关键风险点

**风险 1：ncnn 与 ARM Linux 4.19 + Ubuntu 18.04 交叉编译兼容性**
- ncnn 要求 C++11/C++14 编译器，Ubuntu 18.04 自带 GCC 7.x 满足
- 但 arm-linux-gnueabihf-gcc 可能是 GCC 6.x 或 7.x 版本，需确认 `__ARM_NEON` 宏正确定义
- **缓解**：先用 `arm-linux-gnueabihf-g++ -dM -E - < /dev/null | grep NEON` 验证 NEON 支持

**风险 2：实际推理速度可能低于预期**
- 论文中的 300-500ms 通常在 Cortex-A53@1.2GHz 等更强核心上测得
- Cortex-A7@800MHz 单核，实际可能在 500-1000ms
- **缓解**：(a) 降低输入分辨率到 160×120；(b) 模型 int8 量化；(c) 2-5 秒周期可接受

**风险 3：USB 摄像头在 OTG2 口的供电**
- OTG2 USB 口供电能力有限（通常 500mA）
- 部分 USB 摄像头启动电流可能超过 500mA
- **缓解**：使用有源 USB Hub 或选低功耗摄像头

**风险 4：ncnn 模型格式转换**
- 需要将预训练模型（PyTorch/ONNX）转为 ncnn 格式（.param + .bin）
- 此步骤需在 PC 上完成（需安装 onnx2ncnn 工具）
- **缓解**：可直接使用 ncnn 官方已转换好的 Ultra-Light Face 模型

### 0.4 推荐策略

```
P1 (安全区) → 立即开始，无风险
P2 (探索区) → 先验证 ncnn 交叉编译，再写 face_detect 代码
P3 (实验区) → 等 P2 稳定后按需推进
```

---

## 定位

在 EdgeGuard-6ULL 边缘安全监控项目中加入摄像头和 AI 推理能力，使项目从"传感器数据采集+报警"升级为"视觉+传感器融合的智能安防节点"。

**核心理念**：边缘设备做前端采集+轻量 AI 粗筛，云端/PC 做精细分析。

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

### 1.2 摄像头接口对比

| 方案 | 硬件改动 | 驱动 | 风险 |
|------|---------|------|------|
| **USB UVC** :white_check_mark: | 零 | uvcvideo 主线驱动 | 无 |
| CSI 并行 | 需焊接+LED 迁移 | 需移植 NXP BSP | 高 |

> **结论**：USB UVC 是唯一可行方案。CSI_HSYNC/VSYNC 已被 RGB 绿灯(GPIO4_IO20)和蓝灯(GPIO4_IO19)占用，硬件改造代价太大。

---

## 2. AI 框架选型

### 2.1 ncnn（腾讯，MIT License）—— 首选

| 维度 | 评估 |
|------|------|
| 定位 | 专为 ARM NEON 优化的移动端推理框架 |
| 依赖 | **零外部依赖**，纯 C++ 实现 |
| 交叉编译 | 官方提供 arm-linux-gnueabihf toolchain 文件 |
| 体积 | 精简编译后 ~2MB |
| Cortex-A7 性能 | 轻量人脸检测模型 ~300-500ms/帧 |
| 模型生态 | 大量移动端预训练模型可用 |

### 2.2 备选对比

| 框架 | 优点 | 缺点 |
|------|------|------|
| **ncnn** :white_check_mark: | 零依赖、ARM 优化最佳、轻量 | 需 C++11 |
| TensorFlow Lite | Google 支持 | 依赖复杂、ARMv7 比 ncnn 慢 |
| OpenCV DNN | 功能全 | 编译后 10-50MB，太大 |
| MCUNet | 极致内存 | 面向裸机 MCU，不跑 Linux |

---

## 3. AI 模型分级

### Level 1：运动检测（无模型，零依赖）

- 相邻帧像素差分算法
- 检测画面中是否有物体移动
- CPU 占用极低（~10ms），纯 C 实现
- 作为"区域入侵检测"基础功能

### Level 2：人脸检测（Ultra-Light-Fast-Generic-Face-Detector-1MB）

- 模型大小：1.04MB（int8 量化后 ~300KB）
- 推理时间：~300-500ms/帧（Cortex-A7@800MHz）
- 检测画面中是否有面部（**不识别身份**）
- 与项目契合：未授权人员出现 → 触发 ALARM

### Level 3：通用目标检测（MobileNet-SSD / YOLO-Tiny）

- 模型大小：~6-8MB（int8 ~2MB）
- 推理时间：~800-1500ms/帧
- 检测人、车、动物等多种目标
- 对 CPU/内存要求更高

### Level 4：人脸身份识别（可选，挑战大）

- 模型：MobileFaceNet（~5MB int8）
- 推理时间：~1-2 秒
- 陌生人 vs 白名单人员判别
- 适合 2-3 秒周期的"门禁识别"场景

---

## 4. 实施计划

### 4.1 P1 — USB 摄像头 + 运动检测（无 AI 依赖）

**硬件**：购买 USB UVC 摄像头（Logitech C270 或任意免驱，30-80 元）

**内核**：开启 `CONFIG_MEDIA_SUPPORT`、`CONFIG_USB_VIDEO_CLASS`、`CONFIG_V4L`

**新增文件**：

| 文件 | 说明 |
|------|------|
| `app/camera_v4l2.h` | V4L2 封装头文件（struct camera_ctx） |
| `app/camera_v4l2.c` | V4L2 标准 API 捕获：open → set_fmt → mmap → capture → JPEG（~200 行） |
| `app/edgeguard_visiond.c` | 视觉守护进程：周期拍照 + 运动检测 + 写 vision.json（~300 行） |
| `scripts/edgeguard-visiond.service` | systemd 服务 |

**修改文件**：

| 文件 | 改动 |
|------|------|
| `app/sensor_hubd.c` | 读取 vision.json，motion_detected 纳入状态机 |
| `app/edgeguard_httpd.c` | 新增 `GET /api/snapshot`（JPEG 图片）、`GET /api/vision`（检测结果 JSON） |
| `ui/mainwindow.cpp` | 新增第 6 页 "Vision"：快照显示 + 检测结果 |
| `app/Makefile` | 新增 edgeguard_visiond target |
| `README.md` | 补充视觉模块文档 |

**数据格式** `/tmp/edgeguard_vision.json`：

```json
{
  "camera_online": true,
  "motion_detected": false,
  "face_count": 0,
  "snapshot_path": "/var/log/edgeguard/snapshots/2026-06-08_12-00-00.jpg",
  "inference_ms": 12
}
```

### 4.2 P2 — ncnn 人脸检测（AI 核心）

**新增/修改文件**：

| 文件 | 说明 |
|------|------|
| `app/face_detect.h` | ncnn 人脸检测接口 |
| `app/face_detect.cpp` | ncnn 封装：加载模型 + `detect_faces()`（~200 行 C++） |
| `app/edgeguard_visiond.c` | 集成 face_detect：运动检测后跑人脸检测 |
| `app/sensor_hubd.c` | 状态机新增 `face_intrusion` / `motion_detected` 告警原因 |
| `app/Makefile` | C++ 编译 + 链接 libncnn.a |
| 板子目录 | `/etc/edgeguard/models/ultra_face.param` + `ultra_face.bin` |

**ncnn 交叉编译**（用户在 VM 上操作）：

```bash
git clone https://github.com/Tencent/ncnn.git
cd ncnn && mkdir build-arm && cd build-arm
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/arm-linux-gnueabihf.toolchain.cmake \
      -DNCNN_BUILD_TOOLS=OFF -DNCNN_BUILD_EXAMPLES=OFF \
      -DNCNN_BUILD_BENCHMARK=OFF -DNCNN_VULKAN=OFF ..
make -j4
# 产物: libncnn.a (~2MB)
```

**告警逻辑**：

- 检测到人脸 → ALARM（reason: `face_intrusion`）+ 快照留存
- 检测到运动但无人脸 → WARNING（reason: `motion_detected`）
- 无运动 → NORMAL

### 4.3 P3 — 增强功能（按需选择）

- **P3.1 人脸身份识别**：MobileFaceNet，陌生人 vs 白名单
- **P3.2 云边协同**：板子采集+粗筛 → PC/云做精确识别
- **P3.3 Coral TPU 加速**：USB 硬件加速器，推理 500ms → <50ms（需二手市场）

---

## 5. 系统架构

```
USB Camera (/dev/video0)
       │
       ▼
edgeguard_visiond (V4L2 capture + Motion detect + ncnn inference)
       │
       ├── /var/log/edgeguard/snapshots/*.jpg (报警快照)
       │
       └── /tmp/edgeguard_vision.json (检测结果)
              │
              ├─ sensor_hubd 读取 → 状态机 (face_intrusion → ALARM)
              │                        └── LED + Buzzer + alarms.db
              │
              ├─ edgeguard_httpd 读取 → /api/snapshot + /api/vision
              │                        └── Dashboard 显示
              │
              ├─ edgeguard_mqttd 读取 → edgeguard/vision topic
              │
              └─ edgeguard-ui (Qt5) 读取 → 第 6 页 "Vision"
```

IPC 模式不变——所有进程通过 JSON 文件通信。

---

## 6. 性能预估

| 操作 | Cortex-A7 @ 800MHz | 备注 |
|------|-------------------|------|
| V4L2 捕获 JPEG | ~50ms | MJPEG 640×480 |
| 运动检测 | ~10ms | 320×240 降采样 |
| 人脸检测（ncnn） | ~300-500ms | Ultra-Light 模型 |
| 完整处理周期 | ~2-5 秒/帧 | 运动+人脸可切换 |
| 内存占用（visiond） | ~10-15MB | 含模型+帧缓冲 |

---

## 7. 硬件购买清单

| 物品 | 推荐 | 价格 | 用途 |
|------|------|------|------|
| USB 摄像头 | Logitech C270 / 任意 UVC | 30-80 元 | 拍照 |
| USB 延长线 | 公对母 1m | 5 元 | 灵活布置 |

总计：~50-100 元

---

## 8. 风险与缓解

| 风险 | 缓解 |
|------|------|
| USB 摄像头 UVC 兼容性 | 首选 Logitech C270，已被广泛验证 |
| ncnn 交叉编译困难 | 使用腾讯官方 ARM toolchain 文件 |
| 模型加载失败 | fallback 到纯运动检测模式 |
| Cortex-A7 推理慢 | 设计为 2-5 秒周期拍照，不做实时视频；慢但有结果 |
| V4L2 设备节点变化 | 自动探测 `/dev/video*` 并检测 UVC 设备 |
