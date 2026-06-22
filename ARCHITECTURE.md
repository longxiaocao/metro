# MeteorBladeEnhancer 架构文档

> 完整分析见 [`PROJECT_ANALYSIS.md`](PROJECT_ANALYSIS.md)（13000+ 行 C++ 逐文件通读）。
> 本文档是 PROJECT_ANALYSIS 的**精简单版本 + Phase 1-6 演进说明**。

---

## 0. 一句话总览

把游戏《流星蝴蝶剑.net》的 **DirectDraw 6/7 + Direct3D 6** 调用全部拦截并转译到 **D3D9** 上执行，让老游戏在 Win10/11 现代硬件上能跑起来。本质是一个 **DX6/DX7 → DX9 翻译层 + DInput/DShow 桥接层**，由一个伪装成 `ddraw.dll` 的 DLL + 三个 MinHook 钩子组成。

---

## 1. 顶层架构图

```
                        ┌───────────────────────┐
                        │   game.exe (DX6/7)    │
                        └──────────┬────────────┘
                                   │ LoadLibraryA("ddraw.dll")
                                   │ DirectDrawCreate(...)
                                   │ DirectInputCreateA(...)
                                   │ CoCreateInstance(CLSID_FilterGraph, ...)
                                   ▼
                        ┌───────────────────────┐
                        │      ddfix.dll        │
                        │                       │
                        │  ┌─ DDRAW namespace ─┐│
                        │  │ m_IDirectDraw     ││ → QueryInterface 转 v4
                        │  │ m_IDirectDraw4    ││ → 创 D3D9 device
                        │  │ m_IDirectDraw*    ││ → 壳（v1/2/3/5/6/7）
                        │  │ m_IDirectDrawSurf4││ → ★ 核心实现
                        │  │ m_IDirect3D3      ││ → 真实入口
                        │  │ m_IDirect3DDevice3││ → D3D9 全面实现
                        │  └───────────────────┘│
                        │  ┌─ DINPUT namespace ─┐│
                        │  │ m_IDirectInputA    ││ → 包装系统 dinput
                        │  │ m_IDirectInputDevA ││ → 包装设备
                        │  └───────────────────┘│
                        │  ┌─ DSHOW namespace ──┐│
                        │  │ m_IGraphBuilder    ││ → RenderFile + VMR9
                        │  │ m_IVideoWindow     ││ → 阻断 DShow 建窗口
                        │  └───────────────────┘│
                        │  ┌─ 公共服务 ────────┐ │
                        │  │ D3D9Context        │ │ → D3D9 设备单例
                        │  │ Wrapper.h 表      │ │ → AddressLookupTable
                        │  │ ConfigManager      │ │ → ddfix.ini
                        │  │ HudRenderer        │ │ → F12 调试 HUD
                        │  │ PerfCounter        │ │ → 1秒滑窗 FPS
                        │  │ Log (LogLevel)     │ │ → 线程安全日志
                        │  └───────────────────┘ │
                        └──────────┬────────────┘
                                   │ D3D9 / DInput8 / Ole32
                                   ▼
                        ┌───────────────────────┐
                        │   系统 DLL 层         │
                        │   d3d9.dll            │
                        │   dinput8.dll         │
                        │   ole32.dll           │
                        └───────────────────────┘
```

---

## 2. 文件结构

```
MeteorBladeEnhancer/
├── CMakeLists.txt                        # 顶层构建
├── README.md                             # 用户文档（Phase 6 重写）
├── ARCHITECTURE.md                       # 架构文档（本文件）
├── PROJECT_ANALYSIS.md                   # 深度分析（1300+ 行）
├── ddfix.ini.example                     # 配置模板
├── LICENCE.txt                           # zlib 协议
├── tests/                                # ★ Phase 6 新增
│   ├── SingleTest.h                      # 自包含测试框架
│   ├── main.cpp                          # 测试入口
│   ├── test_ini_parser.cpp               # 配置解析测试
│   ├── test_wrapper.cpp                  # Wrapper 表测试
│   ├── test_perf_counter.cpp             # 性能计数器测试
│   ├── test_hlsl.cpp                     # HLSL 产物验证
│   └── CMakeLists.txt
├── .github/workflows/build.yml           # ★ Phase 6 新增
├── ExtraDxSDK/                           # 离线 D3DX9 头/库
├── minhook/                              # 第三方 API hook 库
└── ddfix/                                # ★ 核心实现
    ├── CMakeLists.txt
    ├── dllmain.cpp                       # DLL 入口 + 3 大 hook namespace
    ├── D3D9Context.h / .cpp              # D3D9 单例 + 6 个 Resource9Factory
    ├── exports.def                       # 导出 7 个 ddraw 符号
    ├── Common/                           # 工具
    │   ├── Logging.h                     # ★ Phase 5.1 重写
    │   ├── SmartPointer.h                # CComPtr-like
    │   ├── Wrapper.h                     # ★ 核心：两个查找表
    │   └── NamespaceDDraw.h              # ★ Phase 5.4 占位
    ├── Config/                           # ★ Phase 3 新增
    │   ├── IniParser.h / .cpp            # 单头 INI 解析器
    │   └── ConfigManager.h / .cpp        # 单例 + 3 段配置
    ├── Debug/                            # ★ Phase 4 新增
    │   ├── HudRenderer.h / .cpp          # 自绘 HUD
    │   ├── ImGuiBackend.h / .cpp         # 桩文件（Phase 4.1 决策）
    │   └── PerfCounter.h / .cpp          # 性能计数器
    ├── ddraw/                            # ★ 核心实现 (27 cpp + 31 h)
    │   ├── ColorKey.hlsl                 # 像素着色器源
    │   └── IDirectDraw*.cpp/.h           # v1/2/3/4/7 + Surface + Device
    ├── dinput/                           # 12 对纯代理
    └── dshow/                            # 3 对 VMR9 桥接
```

---

## 3. 三大子系统

### 3.1 DDraw 代理层（核心）

#### 3.1.1 入口

`dllmain.cpp` 用 `exports.def` 导出与系统 ddraw.dll 同名的 7 个符号：
- `DirectDrawCreate` → `FakeDirectDrawCreate`（**关键拦截**：不调系统函数，直接 `new m_IDirectDraw`）
- `DirectDrawEnumerateA` / `AcquireDDThreadLock` / `ReleaseDDThreadLock` / `D3DParseUnknownCommand` / `DDInternalLock` / `DDInternalUnlock` → 透明转发到系统 ddraw

`FakeDirectDrawCreate` 是整个 ddfix 的"心脏"：
```cpp
HRESULT WINAPI FakeDirectDrawCreate(GUID *lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter) {
    auto WrapperAddressLookupTable = std::make_shared<WrapperLookupTable<void>>(nullptr);
    *lplpDD = WrapperAddressLookupTable->FindWrapper<m_IDirectDraw>(IID_IDirectDraw);
    return S_OK;
}
```

它不调用系统的 DirectDrawCreate，而是直接 new 一个 m_IDirectDraw 包装。从此游戏所有 DDraw 调用进入我们的代理链。

#### 3.1.2 关键 wrapper

| 类 | 行数 | 状态 | 说明 |
|---|---|---|---|
| `m_IDirectDraw` (v1) | ~250 | ✅ 真实现 | 入口，GetCaps 合成表，QueryInterface 转 v4 |
| `m_IDirectDraw2/3/7` | ~120 each | 🟡 壳 | ProxyInterface 转发（兜底） |
| `m_IDirectDraw4` | ~470 | ✅✅ 核心 | 真正接管游戏窗口，触发 D3D9 初始化 |
| `m_IDirectDrawSurface*` (v1-3,7) | ~100 each | 🟡 壳 | ProxyInterface 转发（部分会崩，见 Phase 1.6 修复） |
| **`m_IDirectDrawSurface4`** | **~1750** | ✅✅✅ 核心 | **整个项目最复杂类**，3 种 wrapper + Blit/Present/Lock |
| `m_IDirectDrawClipper` | ~200 | ✅ | 自管 HWND，但 GetClipList/SetClipList 是壳 |
| `m_IDirect3D3` | ~280 | ✅ | EnumDevices 真实枚举 |
| `m_IDirect3DDevice3` | ~900 | ✅✅ 核心 | D3D9 设备全面实现（DrawPrimitive/SetTexture/SetRenderState） |
| `m_IDirect3DTexture2` | ~250 | ✅ | D3D9 texture 持有，Load 用 CPU 拷贝 |
| `m_IDirect3DViewport3` | ~150 | ✅ | D3DVIEWPORT9 + 8 lights |
| `m_IDirect3DLight` | ~150 | ✅ | D3DLIGHT 完整翻译 |
| `m_IDirect3DMaterial3` | ~120 | ✅ | GetMaterial9 翻译 |

#### 3.1.3 ISurface9Wrapper 抽象

`m_IDirectDrawSurface4.cpp:74-84` 定义的核心抽象：
```cpp
struct ISurface9Wrapper {
    virtual ~ISurface9Wrapper() = default;
    virtual SmartPtr<IDirect3DSurface9> GetSurface9() const = 0;
    virtual HRESULT Blt(ISurface9Wrapper* src, LPRECT srcRect, LPRECT destRect,
                        D3DCOLOR* srcColorKey, D3DCOLOR* destColorKey) = 0;
    virtual HRESULT BltFast(ISurface9Wrapper* src, LPRECT srcRect, DWORD dx, DWORD dy,
                            D3DCOLOR* srcColorKey, D3DCOLOR* destColorKey) = 0;
    virtual HRESULT FillColor(LPRECT rect, D3DCOLOR color) = 0;
    virtual HRESULT GetDC(HDC*) = 0;
    virtual HRESULT ReleaseDC(HDC) = 0;
    virtual std::string GetImplClassName() const = 0;
};
```

4 种实现：

| 类 | 文件位置 | 状态 | 用途 |
|---|---|---|---|
| `ZBuffer9Wrapper` | IDirectDrawSurface4.cpp:86-139 | ✅ | 深度模板 |
| `SoftwareSurface9Wrapper` | IDirectDrawSurface4.cpp:142-426 | ✅ | CPU 路径（fallback） |
| `HardwareSurface9Wrapper` | IDirectDrawSurface4.cpp:428-699 | ✅✅ 默认 | D3DXSprite + ColorKey PS |
| `Overlay9Wrapper` | IDirectDrawSurface4.cpp:961+ (Phase 2.1) | ✅ | 兜底 surface |

#### 3.1.4 DrawSprite 渲染管线

所有 Blt 操作最终走 `HardwareSurface9Wrapper::DrawSprite`：

```
1. 保存当前 PixelShader / RenderTarget / ZENABLE
2. SetRenderTarget(0, this)
3. SetRenderState(ZENABLE, FALSE)
4. device->BeginScene + sprite->Begin
5. sprite->SetTransform(2D matrix with dest offset + scale)
6. SetPixelShader(m_colorKeyShader)
7. 推 ColorKey 常量（srcColorKey / haveColorKey / checkAlpha）
8. 3 个采样器都设 D3DTEXF_POINT（避免线性插值）
9. sprite->Draw(srcTex9, srcRect, nullptr, nullptr, 0xffffffff)
10. sprite->Flush/End + device->EndScene
11. 恢复所有状态
```

### 3.2 DInput 代理层（桥接）

- `dllmain.cpp` 的 `DINPUT_HOOK` 命名空间用 MinHook 钩 `dinput.dll!DirectInputCreateA` / `DirectInputCreateEx`
- 钩子把返回的设备指针包成 `m_IDirectInputDeviceA` 等
- `DirectInputCreateEx` **不调系统函数**，而是改用 `MyDirectInput8Create`（DInput8 升级）—— 自动支持 XInput 设备
- 所有 12 对 dinput 类都是**纯代理**：QueryInterface / AddRef / Release 透传到系统对象

### 3.3 DShow 代理层（VMR9 桥接）

- 钩 `Ole32.dll!CoCreateInstance`
- 当游戏建 `CLSID_FilterGraph` 时，把 `IFilterGraph` / `IGraphBuilder` 包成 `m_` 代理
- 注释："**避免DShow里使用DX6的接口**" —— DShow 内部视频渲染会拉起 DX6 surface，我们提前截胡
- `m_IGraphBuilder::RenderFile` 主动建 `CLSID_VideoMixingRenderer9` filter（windowless 模式）
- `m_IVideoWindow::put_Visible` / `put_Owner` / `put_FullScreenMode` / `put_Caption` 全部返 S_OK 不转发 → 阻止 DShow 建独立视频窗口

---

## 4. 核心基础设施

### 4.1 D3D9Context（D3D9 后端中枢）

`ND3D9::D3D9Context` 是单例，仅在 `m_IDirectDraw4` 构造时 `Initialize()`。

**关键 Present Parameters**（`D3D9Context.cpp:390-403`）：
- `Windowed = TRUE`（窗口模式）
- `BackBufferFormat = D3DFMT_X8R8G8B8`
- `SwapEffect = D3DSWAPEFFECT_DISCARD`（最快）
- `Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER`（**关键** —— 让老游戏能 Lock 后台缓冲）
- `PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE`（**关键** —— 不等 vsync，理论上解锁帧率）
- `EnableAutoDepthStencil = FALSE`（Z 缓冲由游戏显式 AddAttachedSurface 注入）

**资源工厂体系**（每种 D3D9 资源有 `IResource9Factory` 子类）：
- `OffscreenSurface9Factory` → `CreateOffscreenPlainSurface(SYSTEMMEM)`
- `ZBufferSurface9Factory` → `CreateDepthStencilSurface(DEFAULT, D16)`
- `BackBuffer9Factory` → `GetBackBuffer(0, 0, MONO)`
- `Texture9Factory` → `CreateTexture(MANAGED/DEFAULT)`
- `RenderTarget9Factory` → `CreateRenderTarget(DEFAULT)`
- `Sprite9Factory` → `D3DXCreateSprite`

**Resource9Handle 模型**：单调递增的 `int`（从 1 开始），`GetResource9<T>(handle)` 用 `*(void**)&result = ...` 转型返回，**带 AddRef**。

**Device Lost 处理**（`D3D9Context::ResetDevice`）：
1. `TestCooperativeLevel` → 若不是 `DEVICENOTRESET` 直接退出
2. 释放所有 `IsCreateInVideoMemory()` 资源（**Phase 1.4 修复**：原 `assert(refs == 0)` 会触发）
3. `BuildD3DPresentParameters` + `m_d3dDev9->Reset`
4. 对所有视频内存资源调 `RebuildResource9`（用原 factory 重新 Create）

### 4.2 Wrapper.h（两个查找表基础设施）

```cpp
// 全局：把任意 COM 指针反查到 m_ 包装
AddressLookupTable<void> ProxyAddressLookupTable;

// Per-DDraw：每个 IID 永远返同一对象（避免重复包装）
WrapperLookupTable<void> WrapperAddressLookupTable;  // 每次 DirectDrawCreate 重建
```

| 类 | 键 | 值 | 作用 |
|---|---|---|---|
| `AddressLookupTable<D>` | 真实 COM 指针（proxy/原始） | 包装对象指针 | 跨接口互转 |
| `WrapperLookupTable<D>` | 接口 IID 字符串 | 单例包装对象 | 同一 IID 始终返同一对象 |

### 4.3 ConfigManager（Phase 3）

- 单例 `ConfigManager::Instance()`
- 加载 `ddfix.ini`（dll 目录 → game exe 目录 → cwd 三级探测）
- 暴露 `renderConfig` / `logConfig` / `debugConfig` 结构体
- 支持 `[Game.<exe_name>]` profile 覆盖

### 4.4 HudRenderer（Phase 4）

- 单例 `HudRenderer::Instance()`
- `ID3DXFont` + `DrawPrimitiveUP` 自绘（不引 ImGui）
- 设备丢失时静默 no-op，等 OnDeviceReset 后重建 D3DXFont
- 在 `m_IDirectDrawSurface4::Blt` Primary 路径（Present 之后）和 Flip（Present 之后）调 Render()

### 4.5 PerfCounter（Phase 4）

- 全局静态计数 + 1 秒滑动平均 FPS
- `PERF_SCOPE(name)` 宏：RAII 埋点
- 线程安全：累计计数 `std::atomic<>`，滑动窗口 `std::mutex`

### 4.6 Logging（Phase 5）

- `enum class LogLevel { DEBUG, INFO, WARN, ERROR, FATAL }`
- `class Log` 用**函数内 static** 持 mutex + LogLevel（避免 SIOF）
- `LOG_DEBUG / LOG_INFO / LOG_WARN / LOG_ERROR / LOG_FATAL` 宏简写
- 保留旧 `Log::LOG` / `Log() << ...` / `logf(...)` 三个旧接口（向后兼容）

---

## 5. 关键调用链

### 5.1 游戏 Blt 一帧到屏幕

```
[game.exe]
  DirectDrawSurface4::Blt(destRect, srcSurface, srcRect, dwFlags, fx)
  ↓ (m_IDirectDrawSurface4::Blt line 1066-1141)
  ├─ 把 destRect 兜底为全尺寸
  ├─ 检查 dwFlags (DDBLT_COLORFILL / DDBLT_KEYSRC)
  ├─ 如果是 Primary 类型 → destRect 偏移归零
  ├─ 从 m_desc 读 ColorKey
  └─ m_surface9Wrapper->Blt(srcSurface9Wrapper, ...)
      ↓ (HardwareSurface9Wrapper::Blt)
      └─ DrawSprite(src, srcRect, srcColorKey, destColorKey, destRect)
          ↓
          ├─ SetRenderTarget(0, this)
          ├─ SetRenderState(ZENABLE, FALSE)
          ├─ BeginScene + sprite->Begin
          ├─ SetPixelShader(m_colorKeyShader)  ★ 共享 PS
          ├─ 推 ColorKey 常量
          ├─ sprite->SetTransform(2D scale+translate)
          ├─ sprite->Draw(srcTex9, srcRect, 0xffffffff)
          └─ sprite->Flush/End + EndScene

  若 m_surfaceType == Primary:
      ├─ device9->Present(srcRect, destRect, 0, nullptr)
      │   └─ 若返 D3DERR_DEVICELOST → TagDeviceLost() + 返 DDERR_SURFACELOST
      └─ srcSurface->FillColor(srcRect, 0)  (trick for GetDC)
```

### 5.2 关键对象生命周期

```
[game.exe] LoadLibraryA("ddraw.dll")  ← 实际加载的是 ddfix.dll
[FakeDirectDrawCreate]
  ↓ new m_IDirectDraw(proxy=null, WrapperTable)
  ↓ SaveWrapper(m_IDirectDraw, IID_IDirectDraw)
[game QueryInterface(IID_IDirectDraw4)]
  ↓ WrapperTable->FindWrapper<m_IDirectDraw4>(IID_IDirectDraw4)
  ↓ new m_IDirectDraw4(proxy=null, WrapperTable)
       ↓ D3D9Context::Initialize(GetActiveWindow())  ← 创 D3D9 device
       ↓ Tex9LookupTable = new AddressLookupTable(...)
  ↓ SaveWrapper(m_IDirectDraw4, IID_IDirectDraw4)
[game IDirectDraw4::CreateSurface(desc, ...)]
  ↓ new m_IDirectDrawSurface4(proxy=null, desc, linkedPrev=null, WrapperTable)
       ↓ ESurfaceType 分发
       ↓   OffScreen   → new HardwareSurface9Wrapper
       ↓   Primary     → new HardwareSurface9Wrapper + new m_linkedNextSurface(BackBuffer)
       ↓   Texture     → new m_IDirect3DTexture2
       ↓   ZBuffer     → new ZBuffer9Wrapper
       ↓   Overlay     → new Overlay9Wrapper  (Phase 2.1)
[game IDirectDraw4::QueryInterface(IID_IDirect3D3)]
  ↓ new m_IDirect3D3(proxy=null, WrapperTable)
  ↓ SaveWrapper(m_IDirect3D3, IID_IDirect3D3)
[game IDirect3D3::CreateDevice(refiid, surface, ...)]
  ↓ WrapperTable->FindWrapper<m_IDirect3DDevice3>(IID_IDirect3DDevice3)  ★ Phase 2.6 改
  ↓ new m_IDirect3DDevice3(proxy=null, WrapperTable)
  ↓ SaveWrapper(m_IDirect3DDevice3, IID_IDirect3DDevice3)
```

---

## 6. Phase 1-6 演进

### Phase 1：崩溃修复（6 个 P0 bug）

| Bug | 修复 |
|---|---|
| `m_IDirectDrawSurface4::Flip` 访问 nullptr | 改走 D3D9 RenderTarget swap + Present |
| `m_IDirect3D3::CreateVertexBuffer` 访问 nullptr | 改用 D3D9 vertex buffer，ProxyInterface 由 nullptr 改为 m_SelfRefs 自管理 |
| `m_IDirect3D3::FindDevice` 访问 nullptr | 改读 m_IDirectDraw::GetD3DDevice3Desc 合成 D3DDEVICEDESC |
| `D3D9Context::ResetDevice` 的 assert | while-loop Release 替代两次 Release + assert，refs != 0 改 log warning |
| BackBuffer 单独建时 m_surface9Wrapper null | 创 HardwareSurface9Wrapper，置 m_isRenderTarget/m_isTex = true |
| `BltFast` on Primary 的 assert(false) | 构造 destRect 后 redirect 到 Blt 路径 |

### Phase 2：功能补全（6 个 P1 功能）

| 功能 | 实现 |
|---|---|
| Overlay surface | 新建 Overlay9Wrapper（Blt/BltFast/FillColor/GetDC 都返 DDERR_GENERIC，GetSurface9 返 RENDERTARGET texture） |
| DDBLT_ALPHASRC/ALPHADEST 路径 | 改用 ID3D9::SetRenderState(ALPHABLENDENABLE/SRCBLEND/DESTBLEND) 三路分支 |
| PS / ConstantTable 共享 | D3D9Context 增 m_colorKeyShader / m_colorKeyConstantTable，懒加载共享 |
| destColorKey HLSL 常量 | 取消 SetFloatArray(destColorKey) 注释 |
| m_IDirect3DDevice3 立即模式 | Begin/Vertex/End 改走 std::vector<BYTE> + DrawPrimitiveUP |
| m_IDirect3D3::CreateDevice 单例 | QueryInterface 改用 FindWrapper<m_IDirect3DDevice3>(IID_IDirect3DDevice3) |

### Phase 3：配置系统

- 新建 `ddfix/Config/IniParser.h/.cpp`（单头，零依赖）
- 新建 `ddfix/Config/ConfigManager.h/.cpp`（单例 + 3 段）
- 替换 9 个硬编码开关为 ConfigManager 调用
- 新建 `ddfix.ini.example` 文档
- 实现 `[Game.<exe_name>]` profile 覆盖

### Phase 4：调试 HUD

- 决策：不引 ImGui（离线分发），改用自绘 HudRenderer
- 新建 `ddfix/Debug/HudRenderer.h/.cpp`（ID3DXFont + DrawPrimitiveUP）
- 新建 `ddfix/Debug/PerfCounter.h/.cpp`（静态计数 + 1秒滑窗 + PERF_SCOPE 宏）
- 新建 `ddfix/Debug/ImGuiBackend.h/.cpp`（桩文件，forward 到 HudRenderer）
- dllmain.cpp 加 `DEBUG_HOOK` 命名空间 + WH_KEYBOARD_LL 钩 F12

### Phase 5：架构优化

- Phase 5.1：日志重写（LogLevel + 函数内 static mutex + 5 个 LOG_* 宏）
- Phase 5.2：`g_texformats` 改 `std::size(texformats)` constexpr
- Phase 5.3：CMakeLists 选项（`BUILD_TESTS=ON` / `BUILD_DDFIX_LIB=OFF` / x86+x64 检测）
- Phase 5.4：命名空间解耦（ND3D9 完整；DDRAW 仅占位 + 风险评估）
- Phase 5.5：22+ 壳类标注（仅 TODO 注释，不实际删）

### Phase 6：测试 & 文档

- **6.1** 自包含 SingleTest 框架（`tests/SingleTest.h`）
- **6.2-6.5** 4 个 test_*.cpp（Wrapper / IniParser / PerfCounter / HLSL）
- **6.6** 测试入口 `tests/main.cpp`
- **6.7** GitHub Actions CI（windows-latest + Ninja + ctest）
- **6.8** README.md 重写（21 → 460+ 行） + ARCHITECTURE.md 新建

---

## 7. 关键设计决策

### 7.1 为什么 ddraw 代理是 nullptr 而 dinput 是真指针？

- **ddraw**：游戏期望 32 位独占模式 + 旧版本 ddraw，Win10 不再支持；我们必须**全替代**渲染管线
- **dinput**：游戏期望标准 dinput API，Win10 仍兼容；我们**只包装**返回的设备指针

### 7.2 为什么 ColorKey 走 D3D9 PS 而不是 CPU？

- DDraw6 ColorKey 是 per-pixel 比较 + alpha 替换 —— GPU 上 1 个 PS pass 完成
- CPU 路径（SoftwareSurface9Wrapper）保留作为软件 fallback（老显卡 / 调试用）
- 全硬件时 D3DXSprite + ColorKey PS 是**最优解**

### 7.3 为什么 PS 在 D3D9Context 单例共享（Phase 2.3）？

- 同一份字节码可以共享 PS instance（D3D9 允许）
- 原实现每张 RT surface 都 `CreatePixelShader` —— 浪费
- 现在用 `GetSharedColorKeyShader` 懒加载共享

### 7.4 为什么 fxc.exe 编译期生成 HLSL 头？

- 避免运行时依赖 D3DCompiler（体积 + 启动慢）
- 编译期 fxc 一次编成字节码，运行时 `CreatePixelShader((DWORD*)g_colorKeyHLSLC, &ps)`
- 代价：CI 必须有 fxc.exe（Windows SDK 自带）

### 7.5 为什么用 `trick for meteor blade` 这种硬编码？

- 从逆向工程游戏行为得到的 hack
- 每个 trick 解决了游戏的一个特殊行为
- **未做通用化**（Project Phase 3 的 Game Profile 是修补路径）

---

## 8. 数据流与依赖图

### 8.1 ddraw 代理与 dinput 代理的关键差异

| 维度 | ddraw 代理 | dinput 代理 |
|---|---|---|
| ProxyInterface 内容 | **nullptr**（自己 new 所有包装） | **系统 dinput 真指针** |
| 引用计数 | 自管 Refs | 透传到系统对象 |
| 创建模式 | 自己 new | 由系统 new 后用 ProxyAddressLookupTable 反查包装 |
| 真实功能 | 渲染逻辑全在包装内 | 大部分逻辑仍在系统 dinput 里 |
| 结论 | 接管层（全替代） | 桥接层（不替代） |

### 8.2 资源生命周期

```
D3D9Context::Instance()  ← 单例，全局
  m_d3d9                ← IDirect3D9
  m_d3dDev9             ← IDirect3DDevice9
  m_resAllocated        ← map<handle, Resource9Info>  (handle = 单调 int)
  m_colorKeyShader      ← IDirect3DPixelShader9 (Phase 2.3 共享)
  m_colorKeyConstantTable ← ID3DXConstantTable    (Phase 2.3 共享)

每次 m_IDirectDrawSurface4 构造：
  m_d3d9Context->CreateTexture9(...) → 返 handle
  m_d3d9Context->CreateSprite()     → 返 handle
  m_d3d9Context->GetSharedColorKeyShader() → 返 LPDIRECT3DPIXELSHADER9 (共享)

设备丢失 → D3D9Context::ResetDevice():
  1. 释放所有 IsCreateInVideoMemory() 资源
  2. m_d3dDev9->Reset(...)
  3. 对所有视频内存资源 RebuildResource9 (用原 factory 重新 Create)
  4. m_colorKeyShaderInited = false (触发共享 PS 重建)
```

---

## 9. 风险评估

### 9.1 Phase 1 风险（已消除）

- 6 个 P0 崩溃 bug 已全部修复（参考 §6）
- 剩余 P1 / P2 / P3 详见 [README §已知问题](README.md#已知问题)

### 9.2 Phase 5 风险（部分完成）

- **5.4 namespace 解耦**：仅完成 ND3D9 完整解耦 + DDRAW 占位。真实 namespace 包装会破坏 `__declspec(uuid(...))` IID 宏，需要 ddraw_local.h 重写 800 行 + IID 映射回归测试（错一个 IID 就会让游戏黑屏）
- **5.5 删壳**：仅完成 TODO 标注 + 风险分级。真实删除前必须先验证 `m_IDirectDraw4::QueryInterface` 能处理所有 IID 映射，且需要端到端 IID 映射表回归测试（需游戏跑过 → 不能在 CI 自动化）+ 灰度发布

### 9.3 Phase 6 风险

- **测试覆盖度**：当前只覆盖 IniParser / Wrapper / PerfCounter / HLSL 4 个**纯逻辑**模块；DDraw 真实 wrapper 测试需要游戏跑通 → CI 不能自动化
- **CI 环境**：GitHub Actions windows-latest runner 自带 VS 2022 + Windows SDK，理论上能跑，但首次跑可能因 fxc 路径问题失败
- **Linux 交叉编译**：仅理论可行，未在 CI 跑通

---

## 10. 扩展点

| 想加什么 | 改哪里 |
|---|---|
| 新增配置项 | `ddfix/Config/ConfigManager.h` + `ddfix.ini.example` + 单元测试 |
| 新增 HUD 显示 | `ddfix/Debug/HudRenderer.cpp` |
| 新增性能计数 | `ddfix/Debug/PerfCounter.h/.cpp` + 在埋点处调 `Increment*` |
| 新增 wrapper 拦截 | `ddfix/ddraw/IDirectDraw*.cpp` + 更新 `ddfix.h` 头包含 |
| 新增 HLSL 着色器 | `ddfix/ddraw/*.hlsl` + `ddfix/CMakeLists.txt` 加 `add_custom_command` |
| 新增单元测试 | `tests/test_*.cpp` + 加到 `tests/CMakeLists.txt` 的 `TEST_SOURCES` |

---

## 附录 A：核心算法速查

### A.1 像素格式约定

| 类型 | 格式 |
|---|---|
| 内部通用 | **A8R8G8B8**（全部 surface 都用这个） |
| ZBuffer | D16 |
| 桌面后缓冲 | X8R8G8B8 |

### A.2 D3D9 资源池选择

| 用途 | 池 |
|---|---|
| 普通 OffScreen（游戏能 Lock） | MANAGED |
| 3D OffScreen（游戏当 RT 用） | DEFAULT + RENDERTARGET |
| BackBuffer | DEFAULT + RENDERTARGET 纹理 |
| Texture（带 mipmap） | MANAGED |
| ZBuffer | DEFAULT（DepthStencil） |
| 软件路径 OffScreen/BackBuffer | SYSTEMMEM |

### A.3 重要 flags

| 标记 | 位置 | 作用 |
|---|---|---|
| `D3DPRESENTFLAG_LOCKABLE_BACKBUFFER` | D3D9Context.cpp:399 | 允许 Lock 后台缓冲（DX6 必要） |
| `D3DPRESENT_INTERVAL_IMMEDIATE` | D3D9Context.cpp:402 | 关闭 vsync（解锁帧率） |
| `D3DDEVCAPS_FLOATTLVERTEX` | IDirectDraw.cpp:25 | 告诉游戏支持浮点 TLVERTEX |
| `DDSCAPS_NONLOCALVIDMEM` | IDirectDraw.cpp:338 | 告诉游戏支持 AGP/PCIe 显存 |

---

## 附录 B：参考文档

- [README.md](README.md) - 用户文档
- [PROJECT_ANALYSIS.md](PROJECT_ANALYSIS.md) - 1300+ 行深度分析
- [ddfix.ini.example](ddfix.ini.example) - 配置模板
- [tasks.md](../.trae/specs/evolve-meteor-blade-enhancer/tasks.md) - 任务清单
- [checklist.md](../.trae/specs/evolve-meteor-blade-enhancer/checklist.md) - 验收清单
- [DirectX-Wrappers](https://github.com/elishacloud/DirectX-Wrappers) - 上游项目
- [MinHook](https://github.com/TsudaKageyu/minhook) - Hook 库
- [DXGL](https://www.dxgl.info) - DX6→OpenGL 参考
