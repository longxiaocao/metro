# MeteorBladeEnhancer

一款适用于游戏《[流星蝴蝶剑.net](https://baike.baidu.com/item/%E6%B5%81%E6%98%9F%E8%9D%B4%E8%9D%B3%E5%89%91)》的图像增强补丁，把 DirectX 6/7 调用拦截并转译到 DirectX 9，让老游戏在 Windows 10/11 现代硬件上顺畅运行。

> 仓库：[`gwmgdemj/MeteorBladeEnhancer`](https://github.com/gwmgdemj/MeteorBladeEnhancer)（基于社区版二次开发）

![MeteorBlade Logo Placeholder](docs/screenshot-placeholder.png)
> 截图占位：游戏运行截图 + 调试 HUD（`docs/screenshot-placeholder.png`）。后续补充实际截图。

---

## 目录

1. [特性](#特性)
2. [快速开始](#快速开始)
3. [构建说明](#构建说明)
4. [使用说明](#使用说明)
5. [配置参考](#配置参考)
6. [调试指南](#调试指南)
7. [架构概览](#架构概览)
8. [已知问题](#已知问题)
9. [项目状态](#项目状态)
10. [贡献指南](#贡献指南)
11. [致谢](#致谢)
12. [许可](#许可)

---

## 特性

- **DirectX 6/7 → DirectX 9 翻译层**：拦截游戏所有 DDraw / D3D 调用，**不修改游戏内存**，全部在渲染层重写。
- **完整接管 DDraw 渲染管线**：
  - Primary / BackBuffer / OffScreen / Texture / ZBuffer / Overlay 全部支持
  - ColorKey 透明效果走 D3D9 像素着色器（ps_2_b）
  - 32 位真彩色 (`A8R8G8B8`)
  - 抗锯齿（`D3DTEXF_POINT`）+ 翻转链自动管理
- **DirectInput 8 升级**：从 DirectInput7 升级到 DirectInput8，自动支持 XInput 设备
- **DirectShow VMR9 桥接**：把视频渲染器从默认的 DDraw7 切换到 VMR9 windowless 模式，避免 Win10 上的兼容性问题
- **配置系统（Phase 3）**：所有"硬编码 trick"都可通过 `ddfix.ini` 调整
- **调试 HUD（Phase 4）**：内置轻量 HUD（基于 `ID3DXFont` + `DrawPrimitiveUP` 自绘，非 ImGui），按 `F12` 切换显示
- **性能计数器（Phase 4）**：Blt / BltFast / FillColor / Flip 调用次数 + 1 秒滑动平均 FPS
- **结构化日志（Phase 5）**：`LogLevel` 过滤 + 线程安全 mutex
- **单元测试（Phase 6）**：`tests/` 自包含 SingleTest 框架，无第三方依赖

---

## 快速开始

> 前提：你的 Windows 系统已安装 **Visual Studio 2022**（含 Windows SDK + C++ 桌面开发组件）

1. **克隆仓库**
   ```bash
   git clone https://github.com/gwmgdemj/MeteorBladeEnhancer.git
   cd MeteorBladeEnhancer
   ```
2. **CMake configure**（默认开 `BUILD_TESTS=ON`）
   ```bash
   cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
   ```
3. **构建**（Debug / Release 任选）
   ```bash
   cmake --build build --config Release
   ```
4. **跑单元测试**
   ```bash
   cd build
   ctest --output-on-failure -C Release
   ```
5. **复制产物**：把 `build/bin/Release/ddfix.dll` + `ddfix.ini.example`（改名为 `ddfix.ini`）放到《流星蝴蝶剑.net》的 exe 同目录
6. **启动游戏**：`meteor.exe`，按 `F12` 看 HUD

> 详细构建见 [§3 构建说明](#构建说明)。调试/排错见 [§6 调试指南](#调试指南)。

---

## 构建说明

### Windows（原生构建，主流场景）

#### 工具链

| 工具 | 版本要求 | 用途 |
|---|---|---|
| Visual Studio | 2022（17.x），含 C++ 桌面开发 | `cl.exe` / `MSBuild` |
| Windows SDK | 10.0.19041 或更新 | `ddraw.h` / `d3d9.h` / `fxc.exe` |
| CMake | 3.11+ | 构建系统 |
| Ninja（推荐） | 任意 | 比 VS generator 快 30% |
| Git | 任意 | clone 仓库 |

> 项目自带 `ExtraDxSDK/`（离线 D3DX9 头/库），不依赖网络下载。

#### Visual Studio IDE

1. 打开 CMakeLists.txt（"Open Folder"）
2. 等 CMake configure 自动完成
3. 顶部配置选 `x86-Release` 或 `x64-Release`
4. 右键 `ddfix` → Build
5. 在 `out/build/x86-Release/bin/ddfix.dll` 找到产物

#### 命令行（推荐）

```bash
# x86（默认；游戏是 32 位）
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j

# x64（实验性；游戏是 32 位时 64 位构建仅用于测试 64 位 ddraw 链）
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

#### CMake 选项

| Option | Default | 说明 |
|---|---|---|
| `BUILD_TESTS` | `ON` | 构建 `ddfixtests` 单元测试可执行文件 |
| `BUILD_DDFIX_LIB` | `OFF` | 额外构建 ddfix 静态库（默认仅输出 `ddfix.dll`） |
| `GameExe` | `""` | 游戏 exe 绝对路径（仅 VS 调试器 `vcxproj.user.in` 用） |
| `GameStartupArgs` | `"w"` | 游戏启动参数（默认 `w` = 窗口模式） |
| `CMAKE_BUILD_TYPE` | (single-config) | `Debug` / `Release` / `RelWithDebInfo` |

### Linux 交叉编译（实验性，理论可行）

⚠️ **状态**：未在 CI 跑通。理论流程如下，不保证可用：

```bash
# 安装交叉工具链
sudo apt install mingw-w64 cmake ninja-build

# 用 mingw 生成器
cmake -S . -B build-mingw -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x86_64.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw
```

需要的 toolchain file（`cmake/mingw-x86_64.cmake`）：
```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
```

**已知问题**：
- `fxc.exe` 不在 Linux 上，需要提前在 Windows 上生成 `ColorKeyHLSLC.h`，再传到 Linux 编译
- DInput8 / Strmiids 等 Windows-only 库需用 Wine 的 stub 或 vendor

---

## 使用说明

### DLL 加载方式

ddfix 设计为**伪装系统 ddraw.dll**。游戏 `LoadLibraryA("ddraw.dll")` 时会优先找 exe 同目录的 ddraw.dll，所以：

#### 方法 A：替换/补充 ddraw.dll（推荐）

把 `ddfix.dll` 改名为 `ddraw.dll`，放到游戏 exe 同目录。游戏会加载我们的 ddraw，然后所有 DDraw 调用进入我们的代理层。

> **注意**：备份原始 `ddraw.dll`！如果想恢复，把原 `ddraw.dll` 放回去即可。

#### 方法 B：dll 注入（高级）

用 DLL 注入工具（如 [Process Hacker](https://processhacker.sourceforge.io/) / [Extreme Injector](https://github.com/master131/ExtremeInjector)）把 `ddfix.dll` 注入到已运行的游戏进程。

> 不推荐 —— 需要在游戏创建 DDraw 设备**之前**注入，否则已有 DDraw 实例不会被我们的代理层接管。

### 加载顺序

```
[game.exe] LoadLibraryA("ddraw.dll")
   ↓ 系统优先找 exe 同目录
[ddfix.dll] DLL_PROCESS_ATTACH
   ├─ 初始化 ConfigManager (加载 ddfix.ini)
   ├─ 初始化 PerfCounter
   ├─ 安装 dinput.dll MinHook 钩
   ├─ 安装 ole32!CoCreateInstance 钩
   └─ 等游戏调 DirectDrawCreate
[game.exe] DirectDrawCreate(...)
   ↓ FakeDirectDrawCreate 拦截
[ddfix.dll] new m_IDirectDraw(...) → 游戏开始走我们的代理链
```

### 第一次运行

1. 第一次启动后会在 exe 同目录生成 `ddfix.log`（按 `Debug.HudEnabled=true` 还会显示 HUD）
2. 默认行为与原始游戏一致 —— 除非有 P0 bug，否则你不会注意到差别
3. 想调参？编辑 `ddfix.ini`（可参考 `ddfix.ini.example`）

---

## 配置参考

完整配置示例：[`ddfix.ini.example`](ddfix.ini.example)

### 配置节

| Section | 说明 | 关键 key |
|---|---|---|
| `[Render]` | 渲染层开关 | `UseSoftwareBlt` / `LockableBackBuffer` / `VSync` / `LightingEnabled` / `AllowBackBufferLock` / `Allow3DOffScreenLock` / `ZBufferAutoRestore` |
| `[Log]` | 日志配置 | `Level` / `File` / `ToConsole` |
| `[Debug]` | 调试 HUD | `HudEnabled` / `HotkeyToggle` |
| `[Game.<name>]` | 游戏 Profile | 同 [Render] 字段，按 exe 名匹配覆盖 |

### 常用配置示例

#### 调试模式（开 HUD + 详细日志）

```ini
[Debug]
HudEnabled   = true
HotkeyToggle = F12

[Log]
Level     = DEBUG
File      = ddfix.log
ToConsole = true
```

#### 性能优化（关 Lock + 关 VSync）

```ini
[Render]
LockableBackBuffer = false
VSync              = false
```

#### 通用 DDraw6 老游戏（非流星蝴蝶剑）

```ini
[Game.OldGame]
UseSoftwareBlt       = true
LockableBackBuffer   = true
AllowBackBufferLock  = true
Allow3DOffScreenLock = true
LightingEnabled      = true
```

> **Game Profile 机制**：`<name>` 是 exe 文件名（不含扩展名），**大小写不敏感**。缺失的 key 继承 [Render]/[Log]/[Debug] 默认。

---

## 调试指南

### 调试 HUD（F12 切换）

按 `F12`（可在 `ddfix.ini` 改 `HotkeyToggle`）调出 HUD，显示：

- **FPS**：1 秒滑动平均（基于 `PerfCounter::Tick`）
- **D3D9 设备状态**：`TestCooperativeLevel` 结果文字化（OK / LOST / RESET）
- **主 surface 尺寸**：当前 back buffer 分辨率
- **每帧调用数**：
  - `Blt/s`（每秒 Blt 次数）
  - `BltFast/s`
  - `FillColor/s`
  - `Flip/s`
  - `Render/s`
- **显存使用**：`GetAvailableTextureMem()`
- **PERF_SCOPE hot 列表**：按出现次数排序的前 8 个作用域
- **Uptime**：自 DLL 加载以来的秒数

> HUD 是自绘的（`ID3DXFont` + `DrawPrimitiveUP`），不依赖 ImGui 第三方库。

### 日志

日志文件默认 `ddfix.log`（exe 同目录）。

| LogLevel | 用途 |
|---|---|
| `DEBUG` | 详细 trace（性能影响） |
| `INFO` | 默认（关键事件：设备创建 / DirectDrawCreate 等） |
| `WARN` | 可疑行为（不致命但需关注） |
| `ERROR` | 失败（D3D9 device lost / Hook 安装失败） |
| `FATAL` | 立即崩 |

```ini
[Log]
Level     = DEBUG   ; 开发时；release 用 INFO
File      = ddfix.log
ToConsole = false   ; release 建议 false
```

### RenderDoc 集成

ddfix 的 D3D9 设备是标准 IDirect3DDevice9，RenderDoc 可直接捕获：

1. 启动 RenderDoc，attach 到 `meteor.exe`
2. 触发可疑帧（按 F12 看 HUD 上的 Blt/s 是否异常）
3. 捕获帧 → 检查 `IDirect3DDevice9::DrawPrimitive` / `SetRenderTarget` / `SetPixelShader` 调用

> 注意：RenderDoc 会注入自己的 d3d9.dll，可能与 ddfix 的 ddraw 代理冲突。如遇问题，先关 ddfix 验证游戏在原生 ddraw 下的行为。

### 性能分析

- **看 FPS 低**：开 HUD → 看 `Blt/s` 是否过高（> 1000 = 异常）；看 `Render/s`（> 200 = 帧率可能受 3D 渲染限制）
- **看 Blt 失败**：`ddfix.log` 搜 `Blt` 找 `DDERR_SURFACELOST`（设备丢失，需要 `Restore()`）
- **看 Buffer 锁失败**：开 `Render.AllowBackBufferLock=true` 验证

---

## 架构概览

> 完整架构见 [`ARCHITECTURE.md`](ARCHITECTURE.md)。这里是 1 分钟速览。

```
                    ┌─────────────────────────┐
                    │    game.exe (DX6/DX7)   │
                    └────────────┬────────────┘
                                 │ LoadLibraryA("ddraw.dll")
                                 ▼
                    ┌─────────────────────────┐
                    │       ddfix.dll         │
                    │  ┌───────────────────┐  │
                    │  │ DDRAW namespace   │  │  → m_IDirectDraw / m_IDirectDrawSurface4 ...
                    │  │ DINPUT namespace  │  │  → m_IDirectInputA / m_IDirectInputDeviceA ...
                    │  │ DSHOW namespace   │  │  → m_IGraphBuilder (RenderFile + VMR9)
                    │  └───────────────────┘  │
                    │  ┌───────────────────┐  │
                    │  │ D3D9Context       │  │  → 设备 + 资源单例
                    │  │ Wrapper.h 表      │  │  → AddressLookupTable + WrapperLookupTable
                    │  │ ConfigManager     │  │  → ddfix.ini 解析
                    │  │ HudRenderer       │  │  → F12 调试 HUD
                    │  │ PerfCounter       │  │  → 性能计数
                    │  └───────────────────┘  │
                    └────────────┬────────────┘
                                 │ D3D9 API
                                 ▼
                    ┌─────────────────────────┐
                    │   d3d9.dll (系统)       │
                    └─────────────────────────┘
```

关键概念：
- **AddressLookupTable**：key = 真实 COM 指针，value = 包装对象
- **WrapperLookupTable**：key = IID 字符串，value = 单例包装对象（同一 IID 始终返同一对象）
- **ISurface9Wrapper**：3 种实现（Hardware / Software / ZBuffer）+ Overlay9Wrapper
- **DrawSprite**：所有 Blt 操作最终走 `D3DXSprite::Draw` + ColorKey 像素着色器

---

## 已知问题

> 来源：[`PROJECT_ANALYSIS.md §11`](PROJECT_ANALYSIS.md)

### ✅ 已修复（Phase 1）

- [x] `m_IDirectDrawSurface4::Flip` 访问 nullptr ProxyInterface（崩溃）
- [x] `m_IDirect3D3::CreateVertexBuffer` 访问 nullptr ProxyInterface
- [x] `m_IDirect3D3::FindDevice` 访问 nullptr ProxyInterface
- [x] `D3D9Context::ResetDevice` 的 `assert(refs == 0)` 崩溃
- [x] `BackBuffer` 单独建时 `m_surface9Wrapper` 为 null
- [x] `BltFast` on Primary 的 `assert(false)` 崩溃

### ⏳ 待办（按优先级）

#### P1（功能缺失）

- [ ] `DDBLT_ALPHASRC` / `DDBLT_ALPHADEST` 路径有实现但未广泛测试（Phase 2.2）
- [ ] BackBuffer GetDC 永远返 `DDERR_GENERIC`（老游戏部分需要）
- [ ] `m_IDirect3DDevice3::BeginIndexed` 临时 stub 返 `DDERR_GENERIC`

#### P2（架构改进）

- [ ] 删 20+ 壳包装类（v1/v2/v3/v5/v6/v7 包装）—— Phase 5.5 仅完成 TODO 标注，真实删除需要 IID 映射回归测试
- [ ] 把 `m_IDirectDraw` 命名空间解耦 —— Phase 5.4 仅完成占位 + 风险文档
- [ ] `fxc.exe` 硬依赖（应改 `D3DCompile` from memory）

#### P3（少量）

- [ ] 第三方 DDraw 老游戏兼容性 —— 现在很多 "trick for meteor blade" 硬编码，需要走 Game Profile
- [ ] HUD 截图文档（等实际游戏跑通后补）

> 完整 bug 清单 + 状态见 [`PROJECT_ANALYSIS.md §11`](PROJECT_ANALYSIS.md)

---

## 项目状态

| Phase | 状态 | 内容 |
|---|---|---|
| Phase 1: 崩溃修复 | ✅ 完成 | 6 个 P0 崩溃 bug |
| Phase 2: 功能补全 | ✅ 完成 | 6 个 P1 功能（Overlay / Alpha / PS 共享 / DestColorKey / 立即模式 / Device 单例） |
| Phase 3: 配置系统 | ✅ 完成 | IniParser + ConfigManager + 9 个硬编码开关 + Game Profile |
| Phase 4: 调试 HUD | ✅ 完成 | 自绘 HUD + PerfCounter + F12 切换 + Config 闸门 |
| Phase 5: 架构优化 | ✅ 完成 | 日志重写 + constexpr 数组 + Build 选项 + namespace 占位 + 壳类标注 |
| Phase 6: 测试 & 文档 | ✅ 完成 | SingleTest 框架 + 4 个 test_*.cpp + 单元测试集成 + GitHub Actions CI + 文档重写 |

---

## 贡献指南

### 报告 Bug

1. 搜索 [Issues](https://github.com/gwmgdemj/MeteorBladeEnhancer/issues) 确认未有人报
2. 提供：
   - 操作系统版本 + 显卡型号
   - 游戏版本
   - `ddfix.log`（如可复现）
   - HUD 截图（如开了 HUD）
   - 复现步骤

### 提交 PR

1. Fork 仓库
2. 创建分支：`git checkout -b feat/your-feature`
3. 写代码（参考 [贡献代码规范](#贡献代码规范)）
4. 跑单元测试：`ctest -C Release`（必须全绿）
5. 更新相关文档（README / ARCHITECTURE / ddfix.ini.example）
6. 提交 PR

### 贡献代码规范

- **C++ 标准**：C++14（项目用 `nullptr` / `auto` / 范围 for / lambda，但未启用 C++17 `std::filesystem`）
- **命名**：
  - 类 / 函数：`PascalCase`
  - 局部变量 / 参数：`camelCase`
  - 成员变量：`m_camelCase`
  - 静态变量：`s_camelCase` 或全大写宏
  - 命名空间：`PascalCase`，项目用 `NDDFIX::Debug` / `NDDFIX::Config` / `ND3D9::`
- **注释**：复杂逻辑加 `// WHY: [解释为什么]` 注释
- **错误处理**：所有 IO / COM / 文件操作必须有错误分支，**禁止**直接吞错
- **新代码必须配测试**（Phase 6 之后）：业务逻辑改动要在 `tests/test_*.cpp` 加测试用例

### 扩展点

| 想加什么 | 改哪里 |
|---|---|
| 新增配置项 | `ddfix/Config/ConfigManager.h` + `ddfix.ini.example` + 单元测试 |
| 新增 HUD 显示 | `ddfix/Debug/HudRenderer.cpp` |
| 新增性能计数 | `ddfix/Debug/PerfCounter.h/.cpp` + 在埋点处调 `Increment*` |
| 新增 wrapper 拦截 | `ddfix/ddraw/IDirectDraw*.cpp` + 更新 `ddfix.h` 头包含 |
| 新增 HLSL 着色器 | `ddfix/ddraw/*.hlsl` + `ddfix/CMakeLists.txt` 加 `add_custom_command` |

---

## 致谢

本项目使用到了以下项目的代码 / 思路：

- [DirectX-Wrappers](https://github.com/elishacloud/DirectX-Wrappers) — elishacloud 的 DX wrapper 框架
- [MinHook](https://github.com/TsudaKageyu/minhook) — TsudaKageyu 的 x86/x64 API hook 库
- [DXGL](https://www.dxgl.info) — DXGL 的 DX6 → OpenGL 翻译层（接口契约参考）
- [D3DX9](https://learn.microsoft.com/en-us/windows/win32/direct3d9/dx9-graphics-reference-d3dx) — Microsoft D3DX9 实用库（离线分发在 `ExtraDxSDK/`）

特别感谢：
- 《流星蝴蝶剑.net》玩家社区的反馈
- 早期版本贡献者（参考 git log）

---

## 许可

本项目使用 **zlib 类宽松协议**，详见 [`LICENCE.txt`](LICENCE.txt)。

允许：
- 自由使用、修改、再分发
- 商业用途

要求：
- 保留版权声明
- 不得用作者名义为衍生作品背书

> 注意：项目使用 MinHook（BSD-2-Clause）和 D3DX9（Microsoft 专属，参见 [Microsoft DirectX SDK EULA](https://learn.microsoft.com/en-us/windows/win32/direct3d9/dx9-graphics-reference-d3dx)）等第三方组件，遵守各自协议。
