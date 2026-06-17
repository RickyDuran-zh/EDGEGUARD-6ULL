# P3: 人脸识别登录 + 摄像头实时预览 + 遮挡检测

## 首先回答核心问题

### Q: LCD 能否实时显示摄像头画面？（是不是必须先存 snapshot 再刷新？）

**答：确实必须先存文件再刷新，但可以做到接近实时。**

原因：V4L2 `/dev/video0` 同一时间只能被一个进程打开。`edgeguard_visiond` 已经占用了摄像头。Qt UI 不能直接读摄像头，必须通过 visiond 作为"帧中继"。

实现方式：
```
visiond 捕获 JPEG 帧 → 写入 /tmp/edgeguard_camera_preview.jpg → Qt 每 250ms 读文件刷新 QLabel
```

这样能做到 **3-4 fps**（每帧延迟 ~300-500ms），看起来像慢速视频，但足以让人对准脸部。这和现有的 snapshot 机制原理一样，只是频率提高、路径固定。

---

## 可行性分析总结

| 功能 | 可行性 | 复杂度 | 风险 |
|------|--------|--------|------|
| 登录页增加「人脸登录」按钮 | ✅ 高 | 低 | 无 |
| LCD 实时摄像头预览 | ✅ 高 | 中 | 3-4 fps 体验可接受 |
| 人脸识别（匹配已知用户） | ⚠️ 条件可行 | **高** | 依赖 P2 人脸检测先修好 |
| 登录后摄像头遮挡检测 | ✅ 高 | 低 | 复用现有 motion 检测 |
| visiond 模式切换（facelogin/tamper/monitor） | ✅ 高 | 中 | 通过 cmd 文件 IPC |

### 关于人脸识别的关键风险

人脸识别 = 人脸检测 + 特征提取 + 相似度匹配。检测是前提——检测不到脸，识别无从谈起。P2 检测效果一直有问题，所以 P3 的人脸识别功能有**硬依赖**。

**建议策略**：P3 先搭好完整框架（预览 + 模式切换 + UI 流程），用简单的"检测到人脸即放行"作为过渡。等 P2 检测修好后，再接入真正的特征比对模型。

---

## 整体架构

```
┌──────────────────────────────────────────────────────────┐
│                    LoginPage (改造)                       │
│  ┌──────────┐  ┌──────────────┐  ┌──────────┐          │
│  │ 密码登录  │  │ 人脸识别登录  │  │ 演示模式   │          │
│  └──────────┘  └──────────────┘  └──────────┘          │
│       │               │                                  │
│       ▼               ▼                                  │
│  现有键盘UI      FaceLoginPage (新增页)                   │
│                 ┌────────────────────┐                   │
│                 │  摄像头实时画面      │  QLabel          │
│                 │  (3-4 fps 刷新)    │  QTimer 250ms    │
│                 │                    │                   │
│                 │  ╭──────────────╮  │                   │
│                 │  │  正在检测...   │  │  状态提示        │
│                 │  ╰──────────────╯  │                   │
│                 │                    │                   │
│                 │  [取消] 返回密码登录 │                   │
│                 └────────────────────┘                   │
│                         │                                │
│                    检测到人脸?                             │
│                    ┌────┴────┐                           │
│                    ▼         ▼                           │
│              识别通过→菜单   超时/失败→提示+回退           │
└──────────────────────────────────────────────────────────┘

登录后:
┌──────────────────────────────────────────────────────────┐
│  MainWindow (菜单页面)                                    │
│                                                          │
│  visiond 切换到 "tamper" 模式:                            │
│  - 每 2s 检测 motion (已有)                               │
│  - 新增: 检测摄像头是否被遮挡                               │
│  - 遮挡→ 写入 alarm 状态                                  │
│  - sensor_hubd → 蜂鸣器/LED 报警                         │
└──────────────────────────────────────────────────────────┘
```

---

## 实施步骤

### 步骤 1: visiond 增加模式切换 + 预览帧输出

**文件**: `app/edgeguard_visiond.c`

新增 visiond 运行模式:
- `monitor` (默认): 当前行为, 2s 间隔, motion 触发检测
- `facelogin`: 300ms 间隔, 每帧写入 `/tmp/edgeguard_camera_preview.jpg`, 每帧跑 face_detect
- `tamper` (登录后): 2s 间隔, motion 检测 + 遮挡判断

模式切换通过读取 `/tmp/edgeguard_cmd.json`:
```json
{"cmd": "vision_mode", "mode": "facelogin"}
{"cmd": "vision_mode", "mode": "tamper"}
```

预览帧路径: `/tmp/edgeguard_camera_preview.jpg` (固定路径, 每次都覆盖)

**注意**: visiond 当前只写 JSON 不读 cmd。需要新增 cmd 文件监听（和 sensor_hubd 同样的方式，监听 `/tmp/edgeguard_cmd.json` 的 mtime 变化）。visiond 只处理 `vision_mode` 命令，忽略其他。

vision JSON 输出增加字段:
```json
{
  "mode": "facelogin",
  "preview_path": "/tmp/edgeguard_camera_preview.jpg",
  "face_verify_result": {
    "detected": true,
    "matched": false,
    "user_id": "",
    "confidence": 0.0
  },
  "tamper_detected": false
}
```

### 步骤 2: face_detect 增加人脸验证接口

**文件**: `app/face_detect.h`, `app/face_detect.cpp`

新增 C 接口:
```c
// 人脸验证: 检测 jpeg 中的人脸是否匹配已知用户数据库
// matched_user[out]: 匹配到的用户名 (至少 64 字节缓冲区)
// confidence[out]: 匹配置信度 0.0-1.0
// 返回 0 成功, -1 失败
int face_verify_run(const uint8_t *jpeg_data, int len,
                    char *matched_user, int user_buf_size,
                    float *confidence);
```

**过渡阶段**（P2 检测还没修好时）:
- 调用现有的 `face_detect_run` 检测是否有人脸
- 如果有人脸 → 返回 `matched=true, user_id="detected"`
- 无人脸 → 返回 `matched=false`
- 这个阶段本质是"有人脸就放行"，安全等级低，但能跑通完整流程

**完整阶段**（P2 修好后）:
- 下载 MobileFaceNet ncnn 模型
- 已知用户的人脸特征预存储在 `/etc/edgeguard/faces/<username>.embed`
- 注册工具: `edgeguard_face_register <username> <jpeg_path>`
- 检测到人脸 → 裁剪人脸区域 → 提取 128 维特征 → 和数据库中所有用户做余弦相似度比较 → 最高分 > 阈值则匹配

### 步骤 3: UI 新增 FaceLoginPage

**新增文件**: `ui/faceloginpage.h`, `ui/faceloginpage.cpp`

功能:
- 一个 QLabel 显示实时摄像头画面 (QTimer 250ms 刷新 `/tmp/edgeguard_camera_preview.jpg`)
- 状态提示: "正在检测人脸..."/"识别成功"/"识别失败"
- 取消按钮: 返回密码登录页
- 超时机制: 30 秒未检测到人脸 → 提示超时

信号:
- `faceLoginSuccess()` → 认证通过, 跳转菜单
- `faceLoginCancel()` → 返回登录页

### 步骤 4: 登录页改造

**修改文件**: `ui/loginpage.h`, `ui/loginpage.cpp`

在现有 UI 上增加:
- "人脸识别登录" 按钮（在"登录"和"演示"按钮旁边）
- 点击 → 发送 `{"cmd":"vision_mode","mode":"facelogin"}` → 切换到 FaceLoginPage
- 同时显示密码登录区域（折叠/展开），让用户可以随时切换回来

### 步骤 5: MainWindow 集成 FaceLoginPage

**修改文件**: `ui/mainwindow.h`, `ui/mainwindow.cpp`

- m_stack 新增 FaceLoginPage (page index 在 Login 和 Dashboard 之间)
- 登录成功 → switchPage 到 Dashboard，同时发送 `{"cmd":"vision_mode","mode":"tamper"}`
- 在 tamper 模式下读取 vision JSON 的 `tamper_detected` 字段 → Dashboard 报警提示

### 步骤 6: 遮挡检测逻辑

**修改文件**: `app/edgeguard_visiond.c`

遮挡判定 (在 tamper 模式下):
- 连续 3 帧 JPEG size < 阈值 (比如 < 5KB，表示全黑/全白画面) → `tamper_detected = true`
- 或连续 5 帧 motion_delta 接近 0 (画面完全静止) → 可能被遮挡
- tamper 状态写入 vision JSON，sensor_hubd 读取后触发报警

### 步骤 7: sensor_hubd 读取 tamper 状态

**修改文件**: `app/sensor_hubd.c`

- 定期读取 `/tmp/edgeguard_vision.json`
- 如果 `tamper_detected == true` → 切换到 ALARM 状态, 报警原因 "摄像头被遮挡"
- 用户 ACK 后清除

---

## 数据流总结

```
登录阶段:
  UI 点击"人脸登录"
    → write_cmd("vision_mode", "facelogin")
    → visiond 切换到 300ms 快速采集
    → visiond 写 /tmp/edgeguard_camera_preview.jpg
    → UI QTimer 250ms 读取并显示
    → visiond 跑 face_verify, 写 /tmp/edgeguard_vision.json
    → UI 读取 face_verify_result
    → matched=true → 跳转菜单
    → UI write_cmd("vision_mode", "tamper")

登录后:
  visiond (tamper 模式)
    → motion 检测 + 遮挡检测
    → 写 /tmp/edgeguard_vision.json
    → sensor_hubd 读取 tamper_detected
    → 报警

密码登录:
  不受影响, visiond 保持 monitor 模式
```

---

## 文件改动清单

| 类型 | 文件 | 改动内容 |
|------|------|----------|
| **新增** | `ui/faceloginpage.h` | 人脸登录页面头文件 |
| **新增** | `ui/faceloginpage.cpp` | 实时预览 + 识别状态 UI |
| **修改** | `ui/loginpage.h` | 新增人脸登录按钮成员 |
| **修改** | `ui/loginpage.cpp` | 增加人脸登录按钮 + 信号 |
| **修改** | `ui/mainwindow.h` | 新增 FaceLoginPage 指针 |
| **修改** | `ui/mainwindow.cpp` | 集成 FaceLoginPage 到页面栈, 跳转逻辑 |
| **修改** | `ui/EdgeGuardUI.pro` | 添加 faceloginpage.h/cpp |
| **修改** | `app/edgeguard_visiond.c` | 模式切换 + 预览帧输出 + 遮挡检测 |
| **修改** | `app/face_detect.h` | 新增 face_verify_run 接口 |
| **修改** | `app/face_detect.cpp` | 过渡版: 有人脸即放行; 完整版: 特征比对 |
| **修改** | `app/sensor_hubd.c` | 读取 tamper 状态, 触发报警 |

---

## 实施顺序

```
1. visiond 模式切换 + 预览帧输出  ← 基础设施, 最先做
2. face_verify 接口 (过渡版)      ← 有人脸即放行
3. FaceLoginPage UI               ← 预览 + 状态提示
4. 登录页改造 + MainWindow 集成    ← 串联完整流程
5. 遮挡检测 + sensor_hubd 联动     ← 登录后安防
6. face_verify 完整版 (MobileFaceNet) ← P2 修好后接入
```

---

## 不需要做的事

- ❌ Qt 直接打开 V4L2 `/dev/video0` — visiond 已经占用，多进程冲突
- ❌ QCamera/QMediaPlayer — 依赖 GStreamer，嵌入式太重且已用 V4L2 直驱
- ❌ OpenCV — 太庞大，交叉编译困难，ncnn + stb_image 已经够用
- ❌ 人脸识别独立 daemon — 不需要，visiond 已经负责摄像头相关一切
