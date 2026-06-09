# EdgeGuard UI 移植与登录系统方案

> **状态**：规划中 | 2026-06-09

---

## 0. 背景

当前分支（main）上的代码已包含上一轮触摸修复，但滑动切页在板子上不触发（需 `journalctl` 日志进一步定位）。用户要求：

1. **保持当前 raw evdev 触摸逻辑不变**（后续再处理触摸问题）
2. **新增登录界面**：虚拟键盘输入用户名密码，登录后进入现有 UI
3. **从野火 qt_demo 移植有价值组件**，丰富项目

---

## 0.5 编译与部署约束

**所有修改限定在 `ui/` 目录内**，最终在 `ui/` 下 `qmake && make` 输出单个 `edgeguard-ui` 可执行文件。

| 维度 | 说明 |
|------|------|
| 编译 | `ui/` 目录内 `qmake EdgeGuardUI.pro && make`，不依赖 `app/` 或其他目录 |
| 产物 | 单个 `edgeguard-ui` 二进制 |
| 配置 | 用户凭据文件 `/etc/edgeguard/users.json` 是板子端部署文件，不在源码目录 |
| 依赖 | 仅 Qt5 库 + libc + pthread，与当前完全一致 |
| 其他进程 | 不动 sensor_hubd / edgeguard_httpd / edgeguard_mqttd |

**不需要改动**：`app/`、`drivers/`、`dts/`、`scripts/` 目录。

---

## 1. 移植清单（从 qt_demo → EdgeGuard）

### 1.1 必移植（P0，登录界面直接依赖）

| 组件 | 来源 | 移植内容 | 用途 |
|------|------|---------|------|
| **xyinput 输入法** | 板子 `/usr/lib/qt/plugins/platforminputcontexts/` | 验证插件可用，`qputenv("QT_IM_MODULE", "xyinput")` 激活 | 虚拟键盘弹出，QLineEdit 点击即可输入 |

**xyinput 验证**（在板子上执行）：
```bash
# 检查输入法插件是否存在
find /usr/lib/qt -name "*xyinput*" -o -name "*imx6*input*" -o -name "*qtkeyboard*" 2>/dev/null

# 检查 Qt 插件目录
ls /usr/lib/qt/plugins/platforminputcontexts/ 2>/dev/null
ls /usr/lib/plugins/platforminputcontexts/ 2>/dev/null
```

- 如果存在 → 直接用 `qputenv()` 激活，零开发量
- 如果不存在 → 需要从 VM 交叉编译或手写 VirtualKeyboard Widget（见 P1 fallback）

### 1.2 推荐移植（P1，页面动画 + 交互增强）

| 组件 | 来源文件 | 移植方式 | 用途 |
|------|---------|---------|------|
| **QtAnimationWidget** | `qt_demo/.../QtUi/src/qtwidgetbase.h:106-138` | 提取动画逻辑到 `ui/widget/`，不改继承关系 | 页面切入切出动画（200ms 滑入） |
| **QtSwitchButton** | `qt_demo/.../QtUi/src/qtswitchbutton.h` | 复制 `ui/widget/qtswitchbutton.h/.cpp` | Settings 页开关控件（LED 开关、蜂鸣器开关等） |
| **QtListWidget** | `qt_demo/.../QtUi/src/qtlistwidget.h` | 参考绘制逻辑，简化移植 | Alarms 页可滚动报警列表 |
| **SplashScreen** | `qt_demo/.../QtUi/src/splashscreen.h` | 参考实现 | 启动画面（加载中动画） |

### 1.3 远期移植（P2，摄像头集成时）

| 组件 | 来源文件 | 用途 |
|------|---------|------|
| **v4l2Cam** | `qt_demo/.../v4l2Cam/v4l2cam.h/.cpp` | USB 摄像头 V4L2 捕获（但会先做成 C daemon 版本） |
| **QtCustomPlot** | `qt_demo/.../QtUi/src/qtcustomplot.h` | 传感器实时折线图 |

### 1.4 不移植

| 组件 | 原因 |
|------|------|
| QtPageListWidget / Launcher | 启动器模式不匹配，当前 QStackedWidget 更简单 |
| 触摸处理 (mousePressEvent 方案) | 需要 tslib/Qt evdevtouch 中间层，保持 raw evdev |
| Skin 皮肤系统 | 当前 CSS stylesheet 已足够 |
| CameraWidget (QCamera+gst) | QCamera 依赖过多，用 raw V4L2 更轻量 |

---

## 2. 登录界面设计

### 2.1 页面结构

```
QStackedWidget (扩到 6 页)
  ├── page 0: LoginPage   ★ 新增
  ├── page 1: Dashboard   (原 page 0)
  ├── page 2: Sensors     (原 page 1)
  ├── page 3: Alarms      (原 page 2)
  ├── page 4: Settings    (原 page 3)
  └── page 5: System      (原 page 4)
```

### 2.2 登录页面 UI 布局

```
┌──────────────────────────────────────────┐
│                                          │
│          ┌──────────────────┐            │
│          │   EdgeGuard 6ULL  │            │  品牌标识
│          │   边缘安防节点     │            │
│          └──────────────────┘            │
│                                          │
│  ┌────────────────────────────────┐     │
│  │  👤 [________________]        │     │  用户名输入框 (QLineEdit)
│  └────────────────────────────────┘     │
│  ┌────────────────────────────────┐     │
│  │  🔒 [________________]        │     │  密码输入框 (QLineEdit, echo=Password)
│  └────────────────────────────────┘     │
│                                          │
│  ┌────────────────────────────────┐     │
│  │          登  录                 │     │  登录按钮 (QPushButton)
│  └────────────────────────────────┘     │
│                                          │
│         ┌──────────────┐                │
│         │  离线 Demo   │                │  跳过认证入口
│         └──────────────┘                │
│                                          │
│       登录失败提示: 红色文字 3秒消失      │  错误提示
└──────────────────────────────────────────┘
```

**默认凭据**：

| 字段 | 值 |
|------|-----|
| 用户名 | `rickyduran` |
| 密码 | `123456` |
```

### 2.3 虚拟键盘弹出流程

```
方案 A（xyinput 插件可用）—— 推荐：
  QLineEdit 获得焦点 → Qt 检测 QT_IM_MODULE=xyinput
  → xyinput 插件自动弹出虚拟键盘（屏幕下半部分）
  → 用户输入完成 → QLineEdit 失去焦点 → 键盘收起
  开发量：0（只需 1 行 qputenv）

方案 B（xyinput 不可用）—— fallback：
  自建 VirtualKeyboard Widget
  - 点击 QLineEdit → 从底部滑入键盘（QPropertyAnimation）
  - 键盘布局：数字+字母 QWERTY（参考 qt_demo Calculator 按钮布局）
  - 点击外部区域 → 键盘滑出收起
  开发量：~400 行（参考之前 UI_REDESIGN_PLAN.md 中的 VirtualKeyboard 设计）
```

### 2.4 认证逻辑

```cpp
// LoginPage 中
void LoginPage::onLoginClicked() {
    QString user = m_userEdit->text();
    QString pass = m_passEdit->text();

    // 1. 先读 /etc/edgeguard/users.json（如果存在）
    // 2. fallback：硬编码默认账户 admin / edgeguard
    // 3. 3 次失败 → 锁定 30 秒 + 蜂鸣器告警

    if (authenticate(user, pass)) {
        m_authSuccess = true;
        emit loginSuccess();  // MainWindow 收到 → switchPage(1)
    } else {
        m_attempts++;
        m_errorLabel->setText("Authentication failed");
        if (m_attempts >= 3) {
            m_loginBtn->setEnabled(false);
            QTimer::singleShot(30000, [this]{ m_loginBtn->setEnabled(true); m_attempts = 0; });
        }
    }
}
```

用户配置文件 `/etc/edgeguard/users.json`（板子端部署时创建，不在源码目录）：

```json
{
  "users": [
    {"name": "rickyduran", "pass": "123456", "role": "admin"}
  ],
  "lock_timeout_sec": 60,
  "max_attempts": 3
}
```

如果 `/etc/edgeguard/users.json` 不存在，UI 直接使用硬编码默认值 `rickyduran` / `123456`。

### 2.5 自动锁定

```cpp
// MainWindow 中
m_lockTimer = new QTimer(this);
m_lockTimer->setInterval(60000);  // 60 秒无操作
connect(m_lockTimer, &QTimer::timeout, [this]{
    switchPage(0);  // 回到登录页
    m_lockTimer->stop();
});

// 任何触摸/按键事件重置定时器
// 在 processTouchEvents() 和 keyPressEvent() 中 m_lockTimer->start()
```

---

## 3. 实施计划

### 3.1 P0 — 登录页面（1-2 天）

| 步骤 | 内容 | 文件 |
|------|------|------|
| 1 | 在板子上验证 xyinput 插件是否存在 | 板子终端 |
| 2 | 新建 `LoginPage` 类 | `ui/loginpage.h/.cpp` |
| 3 | 修改 `MainWindow`：QStackedWidget 插登录页到 index 0 | `ui/mainwindow.h/.cpp` |
| 4 | 实现认证逻辑（读 users.json / fallback 硬编码） | `ui/loginpage.cpp` |
| 5 | 实现登录失败限制（3 次锁 30 秒） | `ui/loginpage.cpp` |
| 6 | 实现自动锁定定时器 | `ui/mainwindow.cpp` |
| 7 | 更新 .pro 文件 | `ui/EdgeGuardUI.pro` |
| 8 | 验证：启动 → 登录页 → 输入 → 进入 Dashboard |

### 3.2 P1 — 页面动画 + 开关控件（1 天）

| 步骤 | 内容 | 文件 |
|------|------|------|
| 1 | 提取 `QtAnimationWidget` 动画逻辑 | `ui/mainwindow.cpp`（直接加 QPropertyAnimation） |
| 2 | 移植 `QtSwitchButton` | `ui/widget/qtswitchbutton.h/.cpp` |
| 3 | 页面切换增加 200ms 滑入动画 | `ui/mainwindow.cpp` switchPage() |
| 4 | Settings 页增加功能开关（LED/Buzzer 开关） | `ui/mainwindow.cpp` Settings 页 |

### 3.3 P2 — 列表 + 摄像头预留（1-2 天）

| 步骤 | 内容 | 文件 |
|------|------|------|
| 1 | 移植 `QtListWidget` 到 Alarms 页 | `ui/widget/qtlistwidget.h/.cpp` |
| 2 | Alarms 页改为可滚动列表 | `ui/mainwindow.cpp` |
| 3 | 预留 Camera 页框架（空白页 + 占位） | `ui/mainwindow.cpp` |

---

## 4. 文件变更总览

### 4.1 新建文件

| 文件 | 行数 | 说明 |
|------|------|------|
| `ui/loginpage.h` | ~40 | 登录页头文件 |
| `ui/loginpage.cpp` | ~250 | 登录页实现：UI + 认证 + 锁定 |
| `ui/widget/qtswitchbutton.h` | ~50 | 开关按钮（从 qt_demo 移植） |
| `ui/widget/qtswitchbutton.cpp` | ~150 | 开关按钮实现 |
| `ui/widget/qtlistwidget.h` | ~60 | 自绘列表（从 qt_demo 移植简化） |
| `ui/widget/qtlistwidget.cpp` | ~200 | 列表实现 |
| `ui/widget/virtualkeyboard.h` | ~40 | 虚拟键盘（仅 xyinput 不可用时） |
| `ui/widget/virtualkeyboard.cpp` | ~350 | 虚拟键盘实现 |

### 4.2 修改文件

| 文件 | 改动 |
|------|------|
| `ui/mainwindow.h` | 新增：LoginPage 指针、锁屏定时器、认证状态标志 |
| `ui/mainwindow.cpp` | 新增：buildLoginPage()、login 成功切换、锁屏逻辑、页面动画 |
| `ui/EdgeGuardUI.pro` | 新增 .cpp/.h 到 SOURCES/HEADERS |

---

## 5. 特殊情况处理

### 5.1 xyinput 插件不可用时的 VirtualKeyboard

参考 `qt_demo/Calculator/src/calculator.cpp` 的按钮布局方式和 `qt_demo/NotePad/src/notepadwidget.cpp` 的输入法使用模式。键盘从底部弹出，半屏覆盖，键盘弹出时禁用底层页面滑动。

### 5.2 离线 Demo 模式

登录页提供 "离线 Demo" 按钮：
- 跳过认证，直接进入 Dashboard
- `--demo` CLI 参数也跳过登录页
- Demo 模式下所有命令（mute/ack）仍生效但不写 cmd.json

### 5.3 向后兼容

- `--demo` 参数：启动后跳过登录页，直接进 Dashboard（保持当前行为）
- 现有 5 页功能和布局不变
- 登录页是"追加"而非"重构"
