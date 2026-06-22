# MeteorBladeEnhancer 完整代码分析

> 仓库：`https://github.com/gwmgdemj/MeteorBladeEnhancer`
> 本地路径：`d:\Program Files (x86)\desktop\metro\MeteorBladeEnhancer`
> 分析日期：2026-06-21
> 范围：所有 80+ 源文件已逐文件通读

---

## 0. 一句话总览

把游戏《流星蝴蝶剑.net》的 **DirectDraw 6/7 + Direct3D 6** 调用全部拦截并转译到 **D3D9** 上执行，让老游戏在 Win10/11 现代硬件上能跑起来。本质是一个 **DX6/DX7 → DX9 翻译层 + DInput/DShow 桥接层**，由一个伪装成 `ddraw.dll` 的 DLL + 三个 MinHook 钩子组成。

---

## 1. 仓库结构与文件清单

| 路径 | 行数级 | 作用 |
|---|---|---|
| [CMakeLists.txt](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/CMakeLists.txt) | 31 | 顶层构建脚本，要求传入 `GameExe` |
| [README.md](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/README.md) | 21 | 公开 README，特性 + 已知问题 |
| [LICENCE.txt](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/LICENCE.txt) | 5 | zlib 类宽松协议 |
| [ddfix/CMakeLists.txt](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/CMakeLists.txt) | 25 | ddfix 库构建，依赖 MinHook/D3D9/DInput8/Strmiids |
| [ddfix/vcxproj.user.in](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/vcxproj.user.in) | 10 | VS 调试启动模板（需 configure_file 生成） |
| [ddfix/dllmain.cpp](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/dllmain.cpp) | 378 | DLL 入口 + 三大命名空间（DDRAW/DINPUT/DSHOW）+ MinHook 封装 |
| [ddfix/exports.def](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/exports.def) | 11 | 导出与系统 ddraw.dll 同名的 7 个符号 |
| [ddfix/D3D9Context.h](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/D3D9Context.h) / [.cpp](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/D3D9Context.cpp) | 134+472 | D3D9 单例：设备/资源管理/Reset |
| [ddfix/Common/](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/Common) | 3 文件 | 工具：Logging、SmartPointer、Wrapper（核心基础设施） |
| [ddfix/ddraw/](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/ddraw) | 31 个 .h + 27 个 .cpp | **核心实现** |
| [ddfix/dinput/](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/dinput) | 12 对 .h/.cpp | 输入设备纯代理 |
| [ddfix/dshow/](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/dshow) | 3 对 .h/.cpp | DirectShow 桥接 VMR9 |
| [ddfix/ddraw/ColorKey.hlsl](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/ddraw/ColorKey.hlsl) | 59 | 像素着色器源码（运行时由 fxc 预编译为 `g_colorKeyHLSLC`） |
| [minhook/](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/minhook) | 第三方 | x86/x64 API hook 库 |
| [ExtraDxSDK/](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ExtraDxSDK) | 11 头 + 3 lib | 离线 D3DX9 头/库 |

---

## 2. 构建系统

[CMakeLists.txt](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/CMakeLists.txt) 的关键配置：

```cmake
set(GameExe "" CACHE FILEPATH "游戏Exe路径")          # 必须指定
set(GameStartupArgs "w" CACHE STRING "游戏启动参数")
set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo")   # 没有 Release
add_definitions("/MP")                                  # 多核编译
```

[ddfix/CMakeLists.txt](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/CMakeLists.txt) 的关键点：

```cmake
# 编译期用 fxc 把 HLSL 编成 C 头文件
add_custom_command(OUTPUT ColorKeyHLSLC COMMAND fxc.exe /Eps_main /Vng_colorKeyHLSLC
    /Tps_2_b /Fh${CMAKE_CURRENT_BINARY_DIR}\\ColorKeyHLSLC.h
    /nologo ddraw\\ColorKey.hlsl)

target_link_libraries(ddfix minhook dinput8.lib Strmiids.lib d3d9.lib d3dx9.lib
    legacy_stdio_definitions.lib "-SAFESEH:NO")

# 自动生成 vcxproj.user，使 F5 直接启动游戏
configure_file(vcxproj.user.in ddfix.vcxproj.user @ONLY)
```

**构建前置条件**（Windows SDK 路径里要有）：
- `fxc.exe`（D3D 着色器编译器）—— 否则 CMake configure 失败
- `cl.exe` + MSBuild（VS）
- 系统已有 `d3d9.lib`（Win10 SDK 自带）

**输出**：`<build>/bin/ddfix.dll`，配合游戏 exe 改名/同目录即可（具体加载方式见 §4）。

---

## 3. 工具层（Common）

### 3.1 [Logging.h](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/Common/Logging.h)
全局 `Log::LOG` 单个 `std::ofstream` 写 `ddfix.log`，`logf(fmt, ...)` 是 printf 风格包装。

**线程不安全**——多线程并发写会交错（见 §11 bug 清单）。

### 3.2 [SmartPointer.h](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/Common/SmartPointer.h)
自实现的 CComPtr-like 智能指针（**不依赖 ATL**），用 `_NoAddRefOrRelease` 包装类禁止外部直接 AddRef/Release（与 ATL CComPtr 同模式）。

### 3.3 [Wrapper.h](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/Common/Wrapper.h) ★核心基础设施

**两个全局表**：

| 类 | 键 | 值 | 作用 |
|---|---|---|---|
| [`AddressLookupTable<D>`](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/Common/Wrapper.h#L13-L95) | **真实 COM 指针**（proxy/原始） | **包装对象指针** | 跨接口互转：拿到 `IDirectDraw4*`，查表得 `m_IDirectDraw4*` |
| [`WrapperLookupTable<D>`](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/Common/Wrapper.h#L109-L184) | **接口 IID 字符串** | **单例包装对象** | 同一 IID 始终返回同一个对象（避免重复包装） |

```cpp
// 全局实例（在 dllmain.cpp 顶部定义）
AddressLookupTable<void> ProxyAddressLookupTable = AddressLookupTable<void>(nullptr);
```

---

## 4. 入口与 DllMain 流程

[dllmain.cpp](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/dllmain.cpp) 是真正的工作入口。三大命名空间：

### 4.1 DDRAW_HOOK（`ddraw.dll` 伪装）

[exports.def](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/exports.def) 导出 7 个符号：

```
DirectDrawCreate        → FakeDirectDrawCreate       [改造]
DirectDrawEnumerateA    → FakeDirectDrawEnumerateA   [透明转发]
AcquireDDThreadLock     → FakeAcquireLock            [透明 jmp]
D3DParseUnknownCommand  → FakeParseUnknown           [透明 jmp]
DDInternalLock          → FakeInternalLock           [透明 jmp]
DDInternalUnlock        → FakeInternalUnlock         [透明 jmp]
ReleaseDDThreadLock     → FakeReleaseLock            [透明 jmp]
```

`CollectOrignalProcAddress()` 从 `%SystemRoot%\ddraw.dll` 用 `GetSystemDirectoryA + strcat` 取系统原 ddraw 的真实地址，存到 6 个 `static void*` 指针，`naked` 函数 `jmp` 过去（line 227-241）。

**核心拦截** [`FakeDirectDrawCreate`](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/dllmain.cpp#L243-L255)：

```cpp
HRESULT WINAPI FakeDirectDrawCreate(GUID *lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter) {
    auto WrapperAddressLookupTable = std::make_shared<WrapperLookupTable<void>>(nullptr);
    *lplpDD = WrapperAddressLookupTable->FindWrapper<m_IDirectDraw>(IID_IDirectDraw);
    return S_OK;
}
```

**它不调用系统的 DirectDrawCreate**！而是直接 `new` 一个 `m_IDirectDraw`（带 `nullptr` 代理）返回。从此游戏所有的 DDraw 调用进入我们控制的代理链。

### 4.2 DINPUT_HOOK（MinHook 钩 `dinput.dll`）

[line 292-306](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/dllmain.cpp#L292-L306)：
- `LoadLibraryA("%SystemRoot%\dinput.dll")`
- MinHook 创建 `DirectInputCreateA` / `DirectInputCreateEx` 钩子
- 钩子把返回的设备指针包成 `m_IDirectInputDeviceA` 等

**注意**：`DirectInputCreateEx` 的钩子实现里调用了 `MyDirectInput8Create`（DInput8 升级）—— **这是一个未声明的函数**（line 280），靠后续链接 `dinput8.lib` 解析。

### 4.3 DSHOW_HOOK（MinHook 钩 `Ole32.dll`）

[line 341-354](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/dllmain.cpp#L341-L354)：
- 钩 `CoCreateInstance`
- 当游戏建 `CLSID_FilterGraph` 时，把 `IFilterGraph`/`IGraphBuilder` 包成 `m_` 代理

> 注释：`// 避免DShow里使用DX6的接口` —— DShow 内部视频渲染会拉起 DX6 surface，我们提前截胡。

### 4.4 MinHook 封装类

`MinHookPP` + `MinHookPPMgr`（[dllmain.cpp:19-200](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/dllmain.cpp#L19-L200)）提供 RAII 风格的 hook 生命周期管理：
- `CreateHook` → `Enable` → `Disable` → `RemoveHook`
- `GetOrignalFunctionAddress<T>()` 取 trampoline
- `MinHookPPMgr` 单例，全局 hooker 表

---

## 5. D3D9Context：D3D9 后端中枢

[ND3D9::D3D9Context](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/D3D9Context.h#L53-L133) 是单例（`Instance()`），**仅在 `m_IDirectDraw4` 构造时 `Initialize()`**（[IDirectDraw4.cpp:32](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw4.cpp#L32)）。

### 5.1 关键 Present Parameters

[`BuildD3DPresentParameters`](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/D3D9Context.cpp#L390-L403)：

| 项 | 值 | 原因 |
|---|---|---|
| `Windowed` | TRUE | 游戏窗口模式 |
| `BackBufferFormat` | D3DFMT_X8R8G8B8 | 32 位色，与游戏期望匹配 |
| `BackBufferWidth/Height` | 当前显示器分辨率 | 用 `EnumDisplaySettings(ENUM_CURRENT_SETTINGS)` |
| `SwapEffect` | D3DSWAPEFFECT_DISCARD | 最快 |
| `Flags` | **D3DPRESENTFLAG_LOCKABLE_BACKBUFFER** | **关键** —— 让老游戏能 Lock 后台缓冲 |
| `PresentationInterval` | **D3DPRESENT_INTERVAL_IMMEDIATE** | **关键** —— 不等 vsync，理论上解锁帧率 |
| `EnableAutoDepthStencil` | **FALSE** | Z 缓冲由游戏显式 `AddAttachedSurface` 注入 |
| `AutoDepthStencilFormat` | D3DFMT_D16 | 占位（实际未启用） |

### 5.2 资源工厂体系

每种 D3D9 资源有对应的 `IResource9Factory` 子类（[D3D9Context.cpp:6-227](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/D3D9Context.cpp#L6-L227)）：

| 工厂 | D3D9 调用 | 池 |
|---|---|---|
| `OffscreenSurface9Factory` | `CreateOffscreenPlainSurface` | SYSTEMMEM |
| `ZBufferSurface9Factory` | `CreateDepthStencilSurface` | DEFAULT（D16） |
| `BackBuffer9Factory` | `GetBackBuffer(0, 0, MONO)` | — |
| `Texture9Factory` | `CreateTexture` | MANAGED 或 DEFAULT |
| `RenderTarget9Factory` | `CreateRenderTarget` | DEFAULT |
| `Sprite9Factory` | `D3DXCreateSprite` | — |

### 5.3 资源 handle 模型

[`Resource9Handle`](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/D3D9Context.h#L40) 就是单调递增的 `int`（从 1 开始，0 永远不用）。`m_resAllocated` 是 `std::map<handle, Resource9Info>`，Info 含 `factory + pointer`。

`GetResource9<T>(handle)` 用 `*(void**)&result = ...` 转型返回，**带 AddRef**。

### 5.4 Device Lost 处理

[`ResetDevice`](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/D3D9Context.cpp#L317-L364)：
1. `TestCooperativeLevel` → 若不是 `DEVICENOTRESET` 直接退出
2. 释放所有 `IsCreateInVideoMemory()` 的资源（**注意：`assert(refs == 0)` 会触发！** 见 §11 bug）
3. `BuildD3DPresentParameters` + `m_d3dDev9->Reset`
4. 对所有视频内存资源调 `RebuildResource9`（用原 factory 重新 `Create`）

`m_deviceLost` 标志位由 `m_IDirectDrawSurface4::Blt` 在 `Present` 返 `D3DERR_DEVICELOST` 时设上（[IDirectDrawSurface4.cpp:1131-1135](file:///D:/Program%20Files%20(x86)/desktop/metro/Meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1131-L1135)），由 `IsLost()` 查询触发 `Restore()` → `ResetDevice()`。

---

## 6. ISurface9Wrapper：渲染抽象层 ★核心

[定义于 IDirectDrawSurface4.cpp:74-84](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L74-L84)：

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

**全局开关** [`g_useSoftwareWrapper9 = false`](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L22) —— 默认走硬件路径。

### 6.1 ZBuffer9Wrapper（[line 86-139](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L86-L139)）
极简：创建 D3DFMT_D16 深度模板，所有 Blt/BltFast/FillColor/GetDC 都返 `DDERR_GENERIC`。

### 6.2 SoftwareSurface9Wrapper（[line 142-426](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L142-L426)）
**完整保留**的纯 CPU Blt 路径。逻辑：
1. `LockRect(READONLY)` 源
2. **最近邻采样**到 `m_srcSampled`（预分配 `width*height * sizeof(D3DCOLOR)`）
3. `LockRect` 目标
4. `LineProcess<...>` 模板按行应用 ColorKey（[line 25-72](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L25-L72)）
5. Unlock 两边

`LineProcess` 有 4 个模板位：`HAVESRCCOLORKEY`、`HAVEDESTCOLORKEY`、`CHECKDIRTY`（仅 Primary surface 用），`CHECKDIRTY` 模式下若 `srcPixel == 0` 跳过非零写入（注释 *trick for GetDC*）—— **说明游戏某处把 GetDC 当 readback 用，需保留黑色源素**。

### 6.3 HardwareSurface9Wrapper（[line 428-699](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L428-L699)）★默认路径

**构造**根据 `ESurfaceType` 创建不同 D3D9 资源：

| SurfaceType | D3D9 资源 | m_isRenderTarget | m_isTex |
|---|---|---|---|
| `BackBuffer` | `CreateTexture(w,h,1,RENDERTARGET,X8R8G8B8,DEFAULT)` | true | true |
| `OffScreen + DDSCAPS_3DDEVICE` | 同上（**RENDERTARGET**） | true | true |
| `OffScreen`（普通） | `CreateTexture(w,h,1,0,X8R8G8B8,MANAGED)` | false | true |
| `Primary` | `GetBackBuffer9()`（共享） | true | false |

所有 `m_isRenderTarget = true` 的实例还会额外创建：
- `D3DXSprite`（用于 2D quad 绘制）
- 自定义 ColorKey 像素着色器（从 `g_colorKeyHLSLC` 即 `ColorKey.hlsl` 预编译）
- 着色器常量表 `ID3DXConstantTable`

**核心 Blt**（[line 503-510](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L503-L510)）**直接调用 `DrawSprite`**，没有真 Blt：

```cpp
HRESULT Blt(ISurface9Wrapper* src, LPRECT srcRect, LPRECT destRect, ...) override {
    assert(src->GetImplClassName() == this->GetImplClassName());  // 必须同类型
    DrawSprite(src, srcRect, srcColorKey, destColorKey, destRect);
    return DD_OK;
}
```

**DrawSprite**（[line 512-569](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L512-L569)）核心渲染管线：
1. 保存当前 `PixelShader / RenderTarget / ZEnable`
2. `SetRenderTarget(0, GetSurface9())` 切到目标
3. `SetRenderState(ZENABLE, FALSE)` 关深度
4. `device->BeginScene` + `sprite->Begin` + `sprite->SetTransform(2D matrix with dest offset + scale)`
5. `SetPixelShader(m_colorKeyShader)` 装上 ColorKey 着色器
6. 推 ColorKey 常量（`srcColorKey`、`haveColorKey[2]`、`checkAlpha`）
7. 三个采样器都设 `D3DTEXF_POINT`（避免线性插值）
8. `sprite->Draw(srcTex9, srcRect, nullptr, nullptr, 0xffffffff)` 用 D3DXSprite 画
9. `sprite->Flush/End` + `device->EndScene`
10. 恢复所有状态

**FillColor** 分两路：
- RenderTarget：用 `device->Clear(rect, color)`（line 668-686）
- 普通：用 `LockRect` + memset 写

**GetDC/ReleaseDC**（line 598-626）：
- BackBuffer / 3D OffScreen：直接返 `DDERR_GENERIC`（注释 *too slow !!!*，因为 D3D9 文档说 GetDC 在有 ALPHA 通道时行为不确定）
- 其他：`GetSurface9()->GetDC(a)`（要求 D3D9 surface 是 lockable）

**Software 与 Hardware 的混用问题**（line 198, 312, 316）：
- `SoftwareSurface9Wrapper::Blt` `assert(srcSurface9Wrapper->GetImplClassName() == this->GetImplClassName())`
- 同样 Hardware 也 assert
- **同一 surface 链里**只能选一种 wrapper，但 `m_IDirectDrawSurface4` 构造时 `g_useSoftwareWrapper9` 决定全局走哪条
- 当前全硬件，未来切换软件路径时需同步改 `m_linkedPrevSurface/Next` 的所有节点

---

## 7. 核心 wrapper：m_IDirectDrawSurface4

[IDirectDrawSurface4.h](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.h) 是这个项目**唯一"真正实现"**的 surface 包装类。其它版本（v1/v2/v3/v7）都是直接转发 ProxyInterface 的壳（[见 §9](#9-其他版本包装器-壳模式)）。

### 7.1 字段一览

```cpp
ULONG Refs;                                       // 自管
ISurface9Wrapper* m_surface9Wrapper;              // D3D9 后端
m_IDirectDrawClipper* m_clipper;                  // 关联裁剪
m_IDirect3DTexture2* m_tex2;                      // 关联纹理接口（仅 Texture 类型）
m_IDirectDrawSurface4* m_linkedPrevSurface;       // mip 链 / 翻转链
m_IDirectDrawSurface4* m_linkedNextSurface;       // mip 链 / backbuffer 链
bool m_locked;                                    // 锁状态
ESurfaceType m_surfaceType;                       // 类型枚举
DDSURFACEDESC2 m_desc;                            // 完整 desc 缓存
D3DCOLOR m_desc.ddckCKSrcBlt/ddckCKDestBlt/...;  // ColorKey 存在 desc 里
```

### 7.2 构造：类型分发

[line 701-893](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L701-L893)：

```cpp
if (DDSCAPS_OFFSCREENPLAIN)        → OffScreen → Software/Hardware9Wrapper
else if (DDSCAPS_PRIMARYSURFACE)   → Primary  → 详见下
else if (DDSCAPS_TEXTURE)          → Texture  → 创 m_IDirect3DTexture2
else if (DDSCAPS_OVERLAY)          → Overlay  → 啥也不做（m_surface9Wrapper=null）
else if (DDSCAPS_BACKBUFFER)       → BackBuffer → 啥也不做
else if (DDSCAPS_ZBUFFER)          → ZBuffer  → ZBuffer9Wrapper
```

**Primary 复杂情况**（line 775-840）：
- `COMPLEX | FLIP | BackBufferCount > 0` → 自己作为 Primary 创 wrapper，**再 new 一个 m_linkedNextSurface 当 backbuffer**（[line 807-809](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L807-L809)）
- `BackBufferCount > 0` 但没 COMPLEX/FLIP → 自己**降级**为 BackBuffer 类型
- 其他 → 单纯 Primary

**Texture + Mipmap**（line 842-874）：
- 顶层（无 `DDSCAPS2_MIPMAPSUBLEVEL`）创 `m_IDirect3DTexture2`，把 D3D9 texture 存到 `m_ddraw4->Tex9LookupTable`
- 每个 mip 创建一个子 surface（`m_linkedNextSurface` 串联），子 surface 持 `m_linkedPrevSurface` 反查
- 子 surface 本身不创 texture，复用顶层的

### 7.3 Blt 实现：完整流程

[line 1002-1142](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1002-L1142)：

1. **DDBLT_COLORFILL 路径**（line 1033-1064）：
   - 根据 `m_surfaceType` 选 destSurface9（Primary+COMPLEX 用 m_linkedNextSurface->GetSurface9()，即 backbuffer）
   - `m_surface9Wrapper->FillColor(rect, color)`

2. **普通 Blt 路径**（line 1066-1141）：
   - 把 `lpSrcRect`/`lpDestRect` 兜底为 surface 全尺寸
   - **Primary 特判**（line 1082-1094）：把 destRect 偏移归零（因为 Primary 自己不是真 surface，画到 backbuffer）
   - 从 `m_desc` 读 ColorKey：`DDBLT_KEYSRC` 读 `ddckCKSrcBlt`，`DDBLT_KEYSRCOVERRIDE` 读 `lpDDBltFx`
   - `assert(!(DDBLT_ALPHASRC | DDBLT_ALPHADEST))` —— 透明度路径**未实现**
   - `m_surface9Wrapper->Blt(...)` ← 真正干活，调 HardwareSurface9Wrapper::DrawSprite
   - **若 m_surfaceType == Primary**：
     - `device->Present(srcRect, destRect, 0, nullptr)` 立即提交帧
     - 若返 `D3DERR_DEVICELOST` → `TagDeviceLost()` + 返 `DDERR_SURFACELOST`
     - 否则 `srcSurface->m_surface9Wrapper->FillColor(srcRect, 0)` **把源 surface 清零**（line 1138，注释 *trick for GetDC*）—— 这是防止游戏下次把上次 Blt 结果当背景画到新位置上

### 7.4 BltFast

[line 1154-1187](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1154-L1187)：仅 OffScreen/BackBuffer 支持，**Primary 调会 assert 失败**。把 `dwX/dwY` 拼成 destRect 后调 wrapper->BltFast。

### 7.5 Lock/Unlock

[Lock line 1411-1530](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1411-L1530)：
- **OffScreen + (3DDEVICE | OFFSCREENPLAIN)** → **永远返 `DDERR_INVALIDPARAMS`**（line 1435-1438，注释 *trick for meteor blade*，因为 3D 渲染目标 Lock 太慢）
- **BackBuffer** → **永远返 `DDERR_GENERIC`**（line 1494-1502，同 trick）
- **OffScreen（普通）** → 调 D3D9 `LockRect`，把 `pBits`/`Pitch` 写回 desc
- **Texture** → 找到对应 D3D9 纹理 + 正确的 mip level，LockRect

`m_locked` 标志位防止重复 Lock。

### 7.6 AddAttachedSurface（ZBuffer 关联）

[line 985-995](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L985-L995)：
```cpp
m_linkedNextSurface = zbuffer;
device->SetDepthStencilSurface(zbuffer9);
```
assert zbuffer 类型必须是 ZBuffer。

### 7.7 Restore（Device Lost 恢复）

[line 1537-1563](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1537-L1563)：
- Primary → `D3D9Context::ResetDevice()`
- ZBuffer → 重新设 DepthStencil + **`SetRenderState(ZENABLE, TRUE)`**（line 1556，注释 *trick for game meteor blade*）

### 7.8 已知未实现

- `Flip` 仍 `ProxyInterface->Flip(...)` —— **代理是 nullptr，会崩**（line 1217-1225）
- `UpdateOverlay / UpdateOverlayDisplay / UpdateOverlayZOrder` 全部 `ProxyInterface->...`
- `BltBatch / EnumAttachedSurfaces / EnumOverlayZOrders / GetBltStatus / GetCaps / GetFlipStatus / GetOverlayPosition / PageLock/Unlock / SetSurfaceDesc / SetPrivateData/...` 全部转发 ProxyInterface

---

## 8. m_IDirectDraw4：真正接管游戏窗口

[IDirectDraw4.cpp](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw4.cpp) 是除了 v1 surface 之外**第二个真正实现**的类。

### 8.1 生命周期即 D3D9 生命周期

[line 21-43](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw4.cpp#L21-L43)：
```cpp
m_IDirectDraw4::m_IDirectDraw4(...) {
    Tex9LookupTable = new AddressLookupTable<m_IDirectDraw4>(this, false);
    ND3D9::D3D9Context::Instance()->Initialize(GetActiveWindow());  // 关键
}
m_IDirectDraw4::~m_IDirectDraw4() {
    delete Tex9LookupTable;
    ND3D9::D3D9Context::Instance()->Uninitialize();                  // 关键
}
```

**`Tex9LookupTable`** 是把 `IDirect3DTexture9*` 反查回 `m_IDirectDrawSurface4*` 的关键表，在 `IDirect3DDevice3::GetTexture` 中使用（[IDirect3DDevice3.cpp:770-789](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirect3DDevice3.cpp#L770-L789)）。

### 8.2 "骗"游戏的 SetDisplayMode / SetCooperativeLevel

[line 435-447](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw4.cpp#L435-L447)：
```cpp
SetCooperativeLevel(HWND, DWORD) { return DD_OK; }      // 不协商独占
SetDisplayMode(...)              { m_displayWidth = ...; return DD_OK; }  // 不切显示
RestoreDisplayMode()             { return DD_OK; }
FlipToGDISurface()               { return DD_OK; }
```

**这是个关键决策**：游戏在 Win10 上跑 16 位独占模式会失败，本 patch 走 D3D9 窗口模式假装自己是 fullscreen DDraw。`D3D9Context::CalcBackBufferSize` 用 `EnumDisplaySettings(ENUM_CURRENT_SETTINGS)` 拿真实桌面分辨率作为后台缓冲尺寸，让游戏以为自己在全屏。

### 8.3 GetCaps：合成一个"完美的"能力表

[line 284-356](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw4.cpp#L284-L356)：
- `DDCAPS_BLT | BLTCOLORFILL | BLTDEPTHFILL | BLTSTRETCH | COLORKEY | GDI | PALETTE | CANBLTSYSMEM | 3D | CANCLIP | CANCLIPSTRETCHED | READSCANLINE`
- `DDCAPS2_CANRENDERWINDOWED | WIDESURFACES | NOPAGELOCKREQUIRED | FLIPINTERVAL | FLIPNOVSYNC | NONLOCALVIDMEM`
- `DDFXCAPS_BLTSHRINKX/Y | BLTSTRETCHX/Y | BLTMIRRORLEFTRIGHT | BLTMIRRORUPDOWN | BLTROTATION90`
- 等等——**声称自己有所有现代 DDraw 7 的能力**，但实际只用了其中一部分

### 8.4 CreateSurface：不走 ProxyInterface

```cpp
HRESULT CreateSurface(LPDDSURFACEDESC2 a, LPDIRECTDRAWSURFACE4* b, IUnknown* c) {
    *b = new m_IDirectDrawSurface4(nullptr, *a, nullptr, WrapperAddressLookupTable);
    return DD_OK;
}
```
[line 110-114](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw4.cpp#L110-L114) —— **所有 surface 都直接 new 一个 m_IDirectDrawSurface4**，**不调用系统 ddraw**。

### 8.5 CreateClipper / GetGDISurface / DuplicateSurface

- `CreateClipper` 直接 `new m_IDirectDrawClipper(0, 0)`（[line 92-96](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw4.cpp#L92-L96)）
- `GetGDISurface` 返 `DDERR_GENERIC`（[line 395-407](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw4.cpp#L395-L407)）
- `DuplicateSurface` 返 `DDERR_GENERIC`（[line 116-133](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw4.cpp#L116-L133)）
- `GetSurfaceFromDC` 返 `DD_OK`（line 459-471）

### 8.6 EnumDisplayModes：自己枚举

[line 165-267](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw4.cpp#L165-L267) 用 `EnumDisplaySettings` 枚举所有显示模式，去重后回调游戏。

### 8.7 GetDisplayMode：拼当前桌面模式

[line 292-388](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw4.cpp#L292-L388) 读 `ENUM_CURRENT_SETTINGS`，按位深合成 DDSURFACEDESC2。fullscreen 分支 `m_displayWidth/Height/RefreshRate` 是死的（全 0），实际走桌面模式。

---

## 9. 其他版本包装器：壳模式

[IDirectDraw.h](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw.h) / v2 / v3 / v7 / IDirectDrawSurface.h / v2 / v3 / v7 / Palette / Clipper / Gamma / ColorControl / IDirect3D / Device / Device2 / Device7 / Texture / VertexBuffer / ExecuteBuffer / Material / Viewport / Viewport2 / IDirectDrawFactory / IDirectDrawEnumSurface

**共同模式**（以 IDirectDraw2 为例，[IDirectDraw2.cpp:19-118](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw2.cpp#L19-L118)）：

```cpp
HRESULT CreateClipper(DWORD a, LPDIRECTDRAWCLIPPER* b, IUnknown* c) {
    return ProxyInterface->CreateClipper(a, b, c);  // 转发 system ddraw
}
```

**结论**：**只有 m_IDirectDraw v1 + m_IDirectDraw4 + m_IDirectDrawSurface4 + m_IDirect3D3 + m_IDirect3DDevice3 + m_IDirect3DTexture2 + m_IDirect3DViewport3 + m_IDirect3DLight + m_IDirect3DMaterial3 + m_IDirectDrawClipper 这 10 个类有自管理 Refs / 自实现**。

**v1（m_IDirectDraw）是入口**（FakeDirectDrawCreate 唯一直接 new 的对象），它通过 `QueryInterface` 转到 v4（m_IDirectDraw4）。

游戏调 `QueryInterface(IID_IDirectDraw4)` 时：
- m_IDirectDraw::QueryInterface（[line 187-212](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw.cpp#L187-L212)）→ `WrapperAddressLookupTable->FindWrapper<m_IDirectDraw4>(IID_IDirectDraw4)` → new m_IDirectDraw4（**触发 D3D9 初始化**）
- 之后游戏所有 DDraw 调用走 m_IDirectDraw4

游戏调 `QueryInterface(IID_IDirect3D3)` 时：
- 同样通过 WrapperAddressLookupTable 拿 m_IDirect3D3 单例

游戏调 `QueryInterface(IID_IDirectDrawSurface4)` 时：
- m_IDirectDraw4::CreateSurface 直接 new m_IDirectDrawSurface4
- 之后所有 surface 调用走 m_IDirectDrawSurface4

---

## 10. D3D 设备与视口

### 10.1 m_IDirect3DDevice3：[IDirect3DDevice3.cpp](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirect3DDevice3.cpp) ★核心设备

**自管 Refs**，**用 D3D9 API 全面实现**：

| DX6 调用 | DX9 实现 |
|---|---|
| `BeginScene` | `device9->BeginScene()` |
| `EndScene` | `device9->EndScene()` |
| `SetCurrentViewport(vp)` | `device9->SetViewport(vp->GetViewport9())` |
| `SetRenderTarget(surf, 0)` | `device9->SetRenderTarget(0, surf->GetSurface9())` |
| `GetRenderTarget` | 返 `m_currentRenderTarget` |
| `SetTexture(stage, tex2)` | `device9->SetTexture(stage, tex2->GetTexture9())` |
| `GetTexture(stage, out)` | `device9->GetTexture` → `Tex9LookupTable->FindAddressOnly<m_IDirectDrawSurface4>(tex9)` → `out = surface->GetTexture2()` |
| `DrawPrimitive(type, fvf, verts, count, flags)` | `device9->SetFVF(fvf)` + `device9->DrawPrimitiveUP(type, count, verts, stride)` |
| `DrawIndexedPrimitive` | `device9->DrawIndexedPrimitiveUP` |
| `SetTransform(D3DTS_WORLD)` | `device9->SetTransform((D3D9::D3DTS_WORLD1/2/3), m)` —— 完整支持多 world matrix |
| `SetRenderState(D3DRENDERSTATE_TEXTUREMAPBLEND)` | 转换 `D3DTBLEND_MODULATE → D3DTOP_MODULATE` 等，写到 `SetTextureStageState` |
| `SetRenderState(D3DRENDERSTATE_COLORKEYENABLE)` | 记录到 `m_colorKeyEnabled`，若 true 则 `SetRenderState(ALPHATESTENABLE, TRUE)` |
| `SetLightState(D3DLIGHTSTATE_MATERIAL)` | 走 `m_IDirect3DMaterial3::GetMaterial9` |
| `SetLightState(D3DLIGHTSTATE_AMBIENT)` | 存到 `m_lightAmbient`（注：但 Initialize 里有 TODO 说要 `SetRenderState(AMBIENT, ...)`，被注释掉） |
| `EnumTextureFormats` | 走 `g_texformats` 数组（**extern 声明**但项目里**没找到定义**！见 §11 bug 4） |

**仍转发 ProxyInterface**：
- `Begin / BeginIndexed / Vertex / Index / End`（DX6 立即模式，未实现）
- `DrawPrimitiveStrided / DrawIndexedPrimitiveStrided`（未实现）
- `DrawPrimitiveVB / DrawIndexedPrimitiveVB`（unwraps proxy for vertex buffer then forwards）

### 10.2 m_IDirect3DViewport3：[IDirect3DViewport3.cpp](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirect3DViewport3.cpp)

- 自管 Refs
- 持 `m_viewport9: D3DVIEWPORT9*` + `m_lights: std::array<IDirect3DLight*, 8>`
- 析构释放所有 lights
- 多数方法仍 `ProxyInterface->...`（line 86-94 等），**核心 SetViewport 没用 m_viewport9**（line 91-94 直接转发）

### 10.3 m_IDirect3DLight：[IDirect3DLight.cpp](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirect3DLight.cpp)

- 自管 Refs
- 持 `m_light9: D3DLIGHT9*`
- **`SetLight` 完整翻译 D3DLIGHT → D3DLIGHT9**（line 78-110）：Type/Diffuse/Position/Direction/Range/Falloff/Attenuation0-2/Theta/Phi
- 多数方法仍 `ProxyInterface->...`

### 10.4 m_IDirect3DTexture2：[IDirect3DTexture2.cpp](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirect3DTexture2.cpp)

- 自管 Refs
- 构造时调 `D3D9Context::CreateTexture9(w, h, mipCount, 0, A8R8G8B8, MANAGED)`，存到 `m_tex9Handle`
- 持 `m_surface: m_IDirectDrawSurface4*`（反引用关系）
- **`Load` 完整实现**（line 118-189）：CPU 端 memcpy 源/目标 D3D9 MANAGED texture + ColorKey 处理
- 多数方法仍 `ProxyInterface->...`（GetHandle/PaletteChanged 转发）

### 10.5 m_IDirect3DMaterial3
从 [IDirect3D3.cpp:101](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirect3D3.cpp#L101) 知道它的存在并支持 `GetMaterial9`，具体实现未深读（m_IDirect3DDevice3::SetLightState 引用了它）。

---

## 11. 已知问题 / TODO / bug 清单

按严重程度排序：

### 🔴 严重（直接崩溃）

1. **m_IDirectDrawSurface4::Flip 转发 nullptr**（[IDirectDrawSurface4.cpp:1217-1225](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1217-L1225)）
   - Flip 链建好后（[line 807-809](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L807-L809)）游戏调 Flip 时直接 `ProxyInterface->Flip(nullptr, ...)` → 段错误
   - 复现条件：游戏用 `DDSCAPS_COMPLEX | DDSCAPS_FLIP` 创建 surface
   - **修复方向**：仿照 HardwareSurface9Wrapper::Blt，把 m_linkedNextSurface 的 D3D9 surface 设为 RenderTarget 后 Present

2. **m_IDirect3D3::CreateVertexBuffer / FindDevice 转发 nullptr**（[IDirect3D3.cpp:113, 124](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirect3D3.cpp#L113)）
   - CreateVertexBuffer 直接 `ProxyInterface->CreateVertexBuffer(...)` → 段错误
   - FindDevice 同上

3. **m_IDirectDrawClipper 的 SetClipList/GetClipList/Initialize 转发 nullptr**（[IDirectDrawClipper.cpp:57-91](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawClipper.cpp#L57-L91)）
   - ProxyInterface 是 nullptr（v4::CreateClipper 直接 new）
   - 只 SetHWnd/GetHWnd 是自管的

4. **D3D9Context::ResetDevice 的 assert(refs == 0)**（[D3D9Context.cpp:338](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/D3D9Context.cpp#L338)）
   - `auto ptr = GetResource9(...)` 内部 AddRef（+1），然后 `ptr->Release(); refs = ptr->Release();`
   - 第一次 Release 减 1（回到原值），第二次 Release 减 1（-1？或 0？）
   - assert 实际验证的是「第二次 Release 后 refcount 必须是 0」—— 但 D3D9 内部对象常有 1 个未释放的内部 ref，**会触发**

5. **m_IDirectDrawSurface4::BltFast 对 Primary 走 assert(false)**（[IDirectDrawSurface4.cpp:1184](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1184)）

6. **g_texformats 全局变量未定义**（[IDirect3DDevice3.cpp:329-330](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/draw/IDirect3DDevice3.cpp#L329-L330)）
   ```cpp
   extern int g_numtexformats;
   extern const DDPIXELFORMAT* g_texformats;
   ```
   链接时会报 unresolved external symbol

### 🟡 中等（功能缺失）

7. **Overlay 类型 surface 完全没实现**（[line 877](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L877) —— `case ESurfaceType::Overlay: break;`，m_surface9Wrapper 永远 null）

8. **BackBuffer 类型直接构造不创 wrapper**（[line 878-879](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L878-L879)）—— **依赖 Primary 创建时链上**。如果游戏先单独创 BackBuffer，wrapper 为 null，GetSurface9() 会 null deref

9. **m_IDirect3D3::CreateDevice 的 device 参数未使用**（[IDirect3D3.cpp:116-120](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirect3D3.cpp#L116-L120)）—— 不看传入的 surface 和 CLSID，直接给一个 m_IDirect3DDevice3

10. **D3DPRESENTFLAG_LOCKABLE_BACKBUFFER 仍开启**（[D3D9Context.cpp:399](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/D3D9Context.cpp#L399)）—— D3D9 文档说会损失性能，**与 D3DPRESENTFLAG_DISCARD 互斥**

11. **日志非线程安全**（[Logging.h](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/Common/Logging.h)）—— 多线程日志可能交错

12. **`m_locked` 标志位未在 DeleteThis 时强制 Unlock** —— 析构时若 m_locked 为 true，D3D9 texture 会一直锁着

13. **m_IDirectDrawSurface4::GetDDInterface 给 v4 单例**（[IDirectDrawSurface4.cpp:1745-1749](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1745-L1749)）—— 没有 AddRef，调用方拿到的指针 refcount 不对

14. **m_IDirect3D3::QueryInterface 返回新 device 但未走 WrapperLookupTable**（[IDirect3D3.cpp:32-39](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirect3D3.cpp#L32-L39)）—— 每次 QI 都会 new 一个新 device（不是单例）

### 🟢 轻微

15. **`m_d3dDev9->SetRenderState(D3DRS_LIGHTING, TRUE)` 被注释**（[D3D9Context.cpp:277-279](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/D3D9Context.cpp#L277-L279)）—— 注释说 "trick for game meteor blade"

16. **`m_IDirect3D3::EnumDevices` 重复设 desc 指针**（[IDirect3D3.cpp:67-88](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/draw/IDirect3D3.cpp#L67-L88)）—— D3DDEVICEDESC 应该是 const ref，不是 dereferenced pointer

17. **m_IDirect3DDevice3::NextViewport / GetCurrentViewport 返 DD_OK 但不返回数据**（[line 308-325, 370-382](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/draw/IDirect3DDevice3.cpp#L308-L382)）

18. **ColorKey alpha 模式未实现**（[IDirectDrawSurface4.cpp:1122-1125](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1122-L1125)）—— assert(false)

19. **vcxproj.user.in 未提供完整的 game exe 选择模板**—— 用户需要手动改 CMake 变量

---

## 12. 数据流与依赖图

### 12.1 关键对象生命周期

```
[game.exe]
  ↓ LoadLibraryA("ddraw.dll")  ← 实际加载的是 ddfix.dll
[FakeDirectDrawCreate]
  ↓ new m_IDirectDraw(proxy=null, WrapperTable)
  ↓ SaveWrapper(m_IDirectDraw, IID_IDirectDraw)
[game 调用 QueryInterface(IID_IDirectDraw4)]
  ↓ WrapperTable->FindWrapper<m_IDirectDraw4>(IID_IDirectDraw4)
  ↓ new m_IDirectDraw4(proxy=null, WrapperTable)
       ↓ D3D9Context::Initialize(GetActiveWindow())   ← 创 D3D9 device
       ↓ Tex9LookupTable = new AddressLookupTable(...)
  ↓ SaveWrapper(m_IDirectDraw4, IID_IDirectDraw4)
[game 调用 IDirectDraw4::CreateSurface(desc, ...)]
  ↓ new m_IDirectDrawSurface4(proxy=null, desc, linkedPrev=null, WrapperTable)
       ↓ ESurfaceType 分发
       ↓   OffScreen   → new HardwareSurface9Wrapper
       ↓   Primary     → new HardwareSurface9Wrapper + new m_linkedNextSurface(BackBuffer)
       ↓   Texture     → new m_IDirect3DTexture2
       ↓   ZBuffer     → new ZBuffer9Wrapper
[game 调用 IDirectDraw4::QueryInterface(IID_IDirect3D3)]
  ↓ new m_IDirect3D3(proxy=null, WrapperTable)
  ↓ SaveWrapper(m_IDirect3D3, IID_IDirect3D3)
[game 调用 IDirect3D3::CreateDevice(refiid, surface, ...)]
  ↓ WrapperTable->FindWrapper<m_IDirect3DDevice3>(IID_IDirect3DDevice3)
  ↓ new m_IDirect3DDevice3(proxy=null, WrapperTable)
  ↓ SaveWrapper(m_IDirect3DDevice3, IID_IDirect3DDevice3)
[game 调用 IDirect3DDevice3::SetRenderTarget(3DSurface4, 0)]
  ↓ m_currentRenderTarget = surface
  ↓ device9->SetRenderTarget(0, surface->GetSurface9())
[game 调用 IDirect3DDevice3::DrawPrimitive(...)]
  ↓ device9->SetFVF + device9->DrawPrimitiveUP  ← D3D9 实际绘制
[game 调用 IDirect3DDevice3::EndScene]
  ↓ device9->EndScene
[游戏循环...]
[game 调用 IDirectDrawSurface4::Blt(...)]
  ↓ m_surface9Wrapper->Blt(...)  ← HardwareSurface9Wrapper::DrawSprite
       ↓ 切 RenderTarget + 装 ColorKey shader + D3DXSprite 画
  ↓ (若 m_surfaceType == Primary)
       ↓ device9->Present
       ↓ srcSurface->FillColor(0)  ← 清源 surface
[game 退出]
  ↓ 各 m_ 包装器 Release 到 0 → delete
  ↓ m_IDirectDraw4 析构 → D3D9Context::Uninitialize() → 释放 D3D9 device
```

### 12.2 两个查找表的关系

```
ProxyAddressLookupTable:
  key = 真实 COM 指针 (proxy / system ddraw 指针)
  value = m_ 包装对象指针
  作用: 任何时候拿到 COM 指针都能找包装

WrapperAddressLookupTable (per-DDraw):
  key = IID 字符串
  value = 该 DDraw 上下文唯一的 m_ 包装
  作用: QI 时返回同一对象 (单例)
```

注意：**ProxyAddressLookupTable 是全局**，**WrapperAddressLookupTable 是每次 DirectDrawCreate 时新建的**（[dllmain.cpp:251-252](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/dllmain.cpp#L251-L252)）—— 意味着每次 DirectDrawCreate 都会创建一套全新包装，但 ProxyAddressLookupTable 会持续累积。

---

## 13. 架构观察

### 13.1 大量"壳"代码是历史包袱

整个项目文件树有 31 个 IDirectDraw* 头 + 27 个 .cpp，但真正实现逻辑的只有：
- `m_IDirectDraw`（v1）
- `m_IDirectDraw4`
- `m_IDirectDrawSurface4`
- `m_IDirect3D3`
- `m_IDirect3DDevice3`
- `m_IDirect3DTexture2`
- `m_IDirect3DViewport3`
- `m_IDirect3DLight`
- `m_IDirectDrawClipper`

**其余 20+ 个类都是简单 ProxyInterface 转发**。这些壳类存在的意义是：游戏 `QueryInterface(IID_IDirectDraw2/3/7)` 时不返错。**新版游戏如果只用 v4 接口，可以删掉所有 v1/v2/v3/v5/v6/v7 包装**。

### 13.2 README 与代码不一致

- README 写"插件接口未完成"——代码里**完全没有插件接口**（dllmain.cpp 也没有加载任何插件）
- README 写"宽屏插件未完成"——代码里**没有宽屏处理**（所有 surface 尺寸都从游戏传入的 desc 拿）
- README 写"GetDC() 函数太慢且无法模拟"——但代码里 HardwareSurface9Wrapper::GetDC 对非 BackBuffer 类型是**直接转给 D3D9 surface->GetDC**（line 609），BackBuffer 才是真不支持
- README 写"在不修改游戏内存的情况下，将 DX6 转换到 DX9"——但实际**拦截了游戏的 SetDisplayMode/SetCooperativeLevel**，并且对 SetDisplayMode 是**完全 noop**（只记录参数），这种"不修改游戏内存"是诚实的——游戏内存完全没动，**所有改变都发生在渲染层**

### 13.3 "Trick for meteor blade" 反复出现

代码里多处注释 "trick for meteor blade" / "trick for game meteor blade" / "trick for GetDC"：
- [D3D9Context.cpp:276](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/D3D9Context.cpp#L276) `TODO: trick for game meteor blade` 关掉 lighting
- [D3D9Context.cpp:1556](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/D3D9Context.cpp#L1556) `SetRenderState(ZENABLE, TRUE)` 在 ZBuffer Restore 时
- [IDirectDrawSurface4.cpp:52](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L52) `// trick for GetDC` —— `srcPixel == 0` 跳过
- [IDirectDrawSurface4.cpp:53](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L53) `// 标准写法`（被绕开）
- [IDirectDrawSurface4.cpp:210](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L210) `const bool checkDirty = true; // trick for GetDC`
- [IDirectDrawSurface4.cpp:1138](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1138) `// trick for GetDC`（Present 后清源）
- [IDirectDrawSurface4.cpp:1433](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1433) `// trick for mateor blade`（3D OffScreen Lock 返错）
- [IDirectDrawSurface4.cpp:1496](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1496) `// trick for mateor blade`（BackBuffer Lock 永远失败）
- [IDirectDrawSurface4.cpp:1555](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1555) `// trick for game meteor blade`（ZBuffer Restore 开 Z）

**这些 trick 是从逆向工程游戏行为得到的 hack**，每个 trick 解决了游戏的一个特殊行为，**没有做通用化**。换游戏跑很可能挂。

### 13.4 DShow 部分用 VMR9 替代了旧视频渲染

[m_IGraphBuilder::RenderFile](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/dshow/IGraphBuilder.cpp#L101-L154) 主动创建 `CLSID_VideoMixingRenderer9` filter 并：
- `SetNumberOfStreams(1)`
- `SetRenderingMode(VMR9Mode_Windowless)`
- `SetVideoClippingWindow(GetActiveWindow())`
- `SetAspectRatioMode(VMR_ARMODE_NONE)`

[m_IVideoWindow::SetWindowPosition](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/dshow/IVideoWindow.cpp#L189-L233) 改用 VMR9 的 `SetVideoPosition`。

`put_Visible / put_Owner / put_FullScreenMode / put_Caption` 全部**直接返 S_OK 不转发**（line 21-25, 83-87, 173-176）—— **阻止 DShow 创建独立视频窗口**，强制把视频画到 D3D9 主窗口。

### 13.5 这个项目其实是 DXGL 的精简再实现

多处代码注释 `// code is taken from project DXGL`（[IDirectDraw.cpp:21](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw.cpp#L21)、[IDirectDraw4.cpp:167](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDraw4.cpp#L167)、[IDirect3D3.cpp:136](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirect3D3.cpp#L136)）—— GetCaps 表、DDCAPS_DX7、EnumDisplayModes 模板都从 DXGL 抄来。说明作者本身是参考 DXGL 的接口契约，但**实际渲染管线用 D3D9 全新实现**（DXGL 是 OpenGL 渲染）。

### 13.6 命名空间隔离 D3D9 头

[D3D9Context.h:8-36](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/D3D9Context.h#L8-L36)：

```cpp
namespace ND3D9 {
    #undef _D3D9_H_
    #undef DIRECT3D_VERSION
    // ... 大量 undef 防止与 DX6 头冲突
    #define DIRECT3D_VERSION 0x0900
    #include <d3dx9.h>
    #include "Common/SmartPointer.h"
}
```

**这是个常见但很脆弱的做法**：DX6 头和 D3D9 头有大量同名宏/类型冲突。`#undef` 之后又 `#define`，未来版本升级会很容易踩坑。

### 13.7 缺少 CI/测试

- 没有单元测试
- 没有集成测试
- 没有 fuzzing
- 唯一"测试"是手动跑游戏

---

## 14. 改进建议（按优先级）

### P0：直接修复 bug

1. **实现 m_IDirectDrawSurface4::Flip**——核心路径
2. **解决 m_IDirectDraw4::CreateSurface 链式 surface 的 m_surface9Wrapper null 问题**（BackBuffer 单独建）
3. **补上 g_texformats 全局变量定义**（移到独立 .cpp）
4. **修复 D3D9Context::ResetDevice 的 assert**（先 Release 到 0 再清 m_resAllocated）

### P1：补全未实现路径

5. **实现 Overlay surface 的 wrapper**（建一张 RT texture）
6. **m_IDirect3D3::CreateVertexBuffer 用 D3D9 vertex buffer 实现**
7. **m_IDirectDrawClipper 完整实现 GetClipList / SetClipList**（用 windows API EnumChildWindows + GetClientRect）
8. **D3DPRESENTFLAG_LOCKABLE_BACKBUFFER 改成视情况开启**（游戏不再 Lock 时关闭以提升性能）

### P2：架构改进

9. **删除 20+ 个壳包装类**（v1/v2/v3/v5/v6/v7 的 surface，v2/v5/v6 的 DDraw 等），精简代码
10. **m_IDirect3D3::CreateDevice 走 WrapperLookupTable 单例**（避免重复）
11. **Log::LOG 改为线程安全**（mutex 或 lock-free queue + 异步线程写）
12. **抽出 ColorKey HLSL 编译产物到 .cmake 脚本**，避免 fxc.exe 硬依赖（用 d3dcompiler 库的 D3DCompile from memory）

### P3：扩展能力

13. **写一个 .ini 配置读取层**——目前所有 `g_useSoftwareWrapper9`、`trick for meteor blade` 硬编码的开关，都应该可配置
14. **加 IMGUI 调试 HUD**（用 D3DXFont 渲到 Primary surface 之前）
15. **shader 化 ZBuffer/Texture 上的 ColorKey**（目前是 CPU 处理，可以写个像素着色器做）

---

## 附录 A：文件 ↔ 职责速查表

| 文件 | 真正干活？ | 关键内容 |
|---|---|---|
| dllmain.cpp | ✅ | DLL 入口，DDRAW/DINPUT/DSHOW 三个 hook namespace |
| exports.def | ✅ | ddraw.dll 7 个导出符号 |
| D3D9Context.h/.cpp | ✅ | D3D9 单例 + 6 个 Resource9Factory |
| Common/Logging.h | 🟡 | 非线程安全 ofstream |
| Common/SmartPointer.h | 🟡 | CComPtr-like |
| Common/Wrapper.h | ✅ | AddressLookupTable / WrapperLookupTable 基础设施 |
| ddraw/ddraw.h | 🟡 | 头聚合 |
| ddraw/InterfaceQuery.cpp | ❌ | 全注释，宏被定义为空 |
| ddraw/IDirectDraw.h/.cpp (v1) | ✅ | 入口，GetCaps 合成表，QueryInterface 转 v4 |
| ddraw/IDirectDraw2.h/.cpp | 🟡 | 壳 |
| ddraw/IDirectDraw3.h/.cpp | 🟡 | 壳 |
| ddraw/IDirectDraw4.h/.cpp | ✅ | 真正接管游戏窗口 |
| ddraw/IDirectDraw7.h/.cpp | 🟡 | 壳 |
| ddraw/IDirectDrawSurface.h/.cpp (v1) | 🟡 | 壳 |
| ddraw/IDirectDrawSurface2.h/.cpp | 🟡 | 壳 |
| ddraw/IDirectDrawSurface3.h/.cpp | 🟡 | 壳 |
| **ddraw/IDirectDrawSurface4.h/.cpp** | ✅✅ | **核心实现，ISurface9Wrapper 三种实现，Blit/Present/Lock** |
| ddraw/IDirectDrawSurface7.h/.cpp | 🟡 | 壳（**注意：m_surface9Wrapper 是 nullptr 时 Blt 会崩**） |
| ddraw/IDirectDrawClipper.h/.cpp | ✅ | 自管 HWND，但 GetClipList/SetClipList 是壳 |
| ddraw/IDirectDrawPalette.h/.cpp | 🟡 | 壳 |
| ddraw/IDirectDrawGammaControl.h | 🟡 | 壳（未实现） |
| ddraw/IDirectDrawColorControl.h | 🟡 | 壳（未实现） |
| ddraw/IDirectDrawEnumSurface.h/.cpp | 🟡 | 枚举回调适配器 |
| ddraw/IDirectDrawFactory.h/.cpp | 🟡 | 壳（游戏大概率不用） |
| ddraw/IDirect3D.h/.cpp | 🟡 | 壳（v1） |
| ddraw/IDirect3D2.h/.cpp | 🟡 | 壳 |
| **ddraw/IDirect3D3.h/.cpp** | ✅ | 真正入口，EnumDevices 真实枚举 |
| ddraw/IDirect3D7.h/.cpp | 🟡 | 壳 |
| ddraw/IDirect3DDevice.h/.cpp | 🟡 | 壳（v1） |
| ddraw/IDirect3DDevice2.h/.cpp | 🟡 | 壳 |
| **ddraw/IDirect3DDevice3.h/.cpp** | ✅✅ | **D3D9 设备全面实现** |
| ddraw/IDirect3DDevice7.h/.cpp | 🟡 | 壳 |
| ddraw/IDirect3DExecuteBuffer.h/.cpp | 🟡 | 壳 |
| **ddraw/IDirect3DLight.h/.cpp** | ✅ | D3DLIGHT 完整翻译 |
| ddraw/IDirect3DMaterial.h/.cpp | 🟡 | 壳（v1） |
| ddraw/IDirect3DMaterial2.h/.cpp | 🟡 | 壳 |
| **ddraw/IDirect3DMaterial3.h/.cpp** | ✅ | GetMaterial9 翻译 |
| ddraw/IDirect3DTexture.h/.cpp | 🟡 | 壳（v1） |
| **ddraw/IDirect3DTexture2.h/.cpp** | ✅ | D3D9 texture 持有，Load 用 CPU 拷贝 |
| ddraw/IDirect3DVertexBuffer.h/.cpp | 🟡 | 壳 |
| ddraw/IDirect3DVertexBuffer7.h/.cpp | 🟡 | 壳 |
| ddraw/IDirect3DViewport.h/.cpp | 🟡 | 壳（v1） |
| ddraw/IDirect3DViewport2.h/.cpp | 🟡 | 壳 |
| **ddraw/IDirect3DViewport3.h/.cpp** | ✅ | D3DVIEWPORT9 + 8 lights |
| ddraw/ColorKey.hlsl | ✅ | 编译成 g_colorKeyHLSLC |
| dinput/*.h/.cpp (12 对) | 🟡 | 纯代理，只包装返回的 device |
| dshow/IFilterGraph.h/.cpp | 🟡 | 简单转发 |
| dshow/IGraphBuilder.h/.cpp | ✅ | RenderFile 主动建 VMR9 |
| dshow/IVideoWindow.h/.cpp | ✅ | SetWindowPosition 走 VMR9，Visible/Owner/Caption 屏蔽 |

✅ 真正实现 | 🟡 部分实现/壳 | ❌ 完全空 | ✅✅ 核心

---

## 附录 B：核心算法速查

### B.1 像素格式约定

| 类型 | 格式 |
|---|---|
| 内部通用 | **A8R8G8B8**（全部 surface 都用这个） |
| ZBuffer | D16 |
| 桌面后缓冲 | X8R8G8B8 |

### B.2 D3D9 资源池选择

| 用途 | 池 |
|---|---|
| 普通 OffScreen（游戏能 Lock） | MANAGED |
| 3D OffScreen（游戏当 RT 用） | DEFAULT + RENDERTARGET |
| BackBuffer | DEFAULT + RENDERTARGET 纹理 |
| Texture（带 mipmap） | MANAGED |
| ZBuffer | DEFAULT（DepthStencil） |
| 软件路径 OffScreen/BackBuffer | SYSTEMMEM |

### B.3 重要 flags

| 标记 | 位置 | 作用 |
|---|---|---|
| `D3DPRESENTFLAG_LOCKABLE_BACKBUFFER` | D3D9Context.cpp:399 | 允许 Lock 后台缓冲（DX6 必要） |
| `D3DPRESENT_INTERVAL_IMMEDIATE` | D3D9Context.cpp:402 | 关闭 vsync（解锁帧率） |
| `D3DDEVCAPS_FLOATTLVERTEX` | IDirectDraw.cpp:25 | 告诉游戏支持浮点 TLVERTEX |
| `DDSCAPS_NONLOCALVIDMEM` | IDirectDraw.cpp:338 | 告诉游戏支持 AGP/PCIe 显存 |

---

## 15. DirectInput 代理层（dinput/）

[dllmain.cpp:262-308](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/dllmain.cpp#L262-L308) 的 `DINPUT_HOOK` 命名空间通过 MinHook 钩 `dinput.dll` 的两个入口。

### 15.1 钩子装载流程

```cpp
HMODULE dinput = LoadLibraryA("%SystemRoot%\\dinput.dll");
hookers.push_back(CreateHooker(dinput, "DirectInputCreateA", &FakeDirectInputCreateA));
hookers.push_back(CreateHooker(dinput, "DirectInputCreateEx", &FakeDirectInputCreateEx));
```

- `DirectInputCreateA` 直接转发到系统 dinput，返回的 `IDirectInputA*` **被包成 m_IDirectInputA**（[line 271](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/dllmain.cpp#L271)）
- `DirectInputCreateEx` **不调用系统函数**，而是改用 **`MyDirectInput8Create`**（DInput8 升级路径）—— 这是个函数前置声明，靠 `dinput8.lib` 链接解析（[line 280](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/dllmain.cpp#L280)）

### 15.2 DInput8 升级的意义

老游戏用 DInput7（`DirectInputCreateEx` 返回 `IDirectInput7*`），新版 Windows 仍兼容但**新硬件的支持更好**。升级到 DInput8 后：
- 自动支持 XInput 设备
- 更好的设备通知机制
- 调用方拿到的 `IDirectInput*` 实际是 DInput8 的 `IDirectInput8*`（通过 `genericDinputQueryInterface(riidltf, ppvOut)` 做 IID 适配）

### 15.3 所有 dinput 类的统一模式

12 对 .h/.cpp（[IDirectInputA/W.cpp](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/dinput/IDirectInputA.cpp)、[IDirectInputDevice2A/W.cpp](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/dinput/IDirectInputDevice2A.cpp)、[IDirectInputDevice7A/W.cpp](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/dinput/IDirectInputDevice7A.cpp)、[IDirectInputEffect.cpp](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/dinput/IDirectInputEffect.cpp) 等）**全部是纯 ProxyInterface 转发**：

```cpp
HRESULT QueryInterface(REFIID riid, LPVOID* ppvObj) {
    if (本类 IID) { AddRef(); *ppvObj = this; return S_OK; }
    return ProxyInterface->QueryInterface(riid, ppvObj);  // 系统 dinput 的真指针
}
ULONG AddRef() { return ProxyInterface->AddRef(); }   // 引用计数走系统对象
ULONG Release() {
    ULONG x = ProxyInterface->Release();
    if (x == 0) delete this;   // 包装对象自己负责销毁
    return x;
}
```

**核心特征**：
- **ProxyInterface 是系统 dinput 的真实指针**（不是 nullptr）—— 这是与 ddraw 代理层的关键区别
- `IDirectInputDevice*::CreateEffect` 通过 `ProxyAddressLookupTable` 把返回的 `IDirectInputEffect*` 包成 `m_IDirectInputEffect`
- 包装对象只增加 `this` 引用计数，系统对象的引用计数由调用方管理

### 15.4 dinput 代理层与 ddraw 代理层的关键差异

| 维度 | ddraw 代理 | dinput 代理 |
|---|---|---|
| ProxyInterface 内容 | **nullptr**（自己 new 所有包装） | **系统 dinput 真指针** |
| 引用计数 | 自管 Refs | 透传到系统对象 |
| 创建模式 | 自己 new | 由系统 new 后用 `ProxyAddressLookupTable` 反查包装 |
| 真实功能 | 渲染逻辑全在包装内 | 大部分逻辑仍在系统 dinput 里 |

**结论**：dinput 代理是**桥接层**，不替代；ddraw 代理是**接管层**，全替代。

---

## 16. DirectShow 代理层（dshow/）

[dllmain.cpp:310-355](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/dllmain.cpp#L310-L355) 的 `DSHOW_HOOK` 命名空间钩 `Ole32.dll!CoCreateInstance`：

```cpp
HMODULE dinput = LoadLibraryA("%SystemRoot%\\Ole32.dll");  // 注意变量名错误
hookers.push_back(CreateHooker(dinput, "CoCreateInstance", &FakeCoCreateInstance));
```

注释说 "**避免DShow里使用DX6的接口**" —— 即 DShow 内部视频渲染会拉起 DX6 surface，我们提前截胡。

### 16.1 CoCreateInstance 过滤

```cpp
if (rclsid == CLSID_FilterGraph) {
    if (riid == IID_IFilterGraph)        *ppv = FindAddress<m_IFilterGraph>(*ppv);
    else if (riid == IID_IGraphBuilder)  *ppv = FindAddress<m_IGraphBuilder>(*ppv);
}
if (riid == IID_IBaseFilter) { int a = 0; a = 1; }  // 无意义代码，估计调试遗留
```

### 16.2 真正的"创造性"工作：RenderFile 主动建 VMR9

[IGraphBuilder.cpp:101-154](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/ddfix/dshow/IGraphBuilder.cpp#L101-L154) 是这个项目的**灵魂 hack** 之一：

```cpp
HRESULT RenderFile(LPCWSTR lpcwstrFile, LPCWSTR lpcwstrPlayList) {
    // 1. 主动创建一个 VMR9 filter
    CoCreateInstance(CLSID_VideoMixingRenderer9, NULL, CLSCTX_INPROC_SERVER,
                     IID_IBaseFilter, (void**)&vmr9Filter);
    AddFilter(vmr9Filter, L"Video Mixing Renderer 9");

    // 2. 配置 VMR9
    IVMRFilterConfig9* pConfig;
    pConfig->SetNumberOfStreams(1);
    pConfig->SetRenderingMode(VMR9Mode_Windowless);  // 无窗口渲染

    // 3. 绑到游戏主窗口
    vmr9Ctrl->SetVideoClippingWindow(GetActiveWindow());
    vmr9Ctrl->SetAspectRatioMode(VMR_ARMODE_NONE);

    // 4. 让原 FilterGraph 用 VMR9 替代默认渲染器
    return ProxyInterface->RenderFile(lpcwstrFile, lpcwstrPlayList);
}
```

**为什么要这样？** DShow 默认视频渲染器（`Video Mixing Renderer 7` 或 `Overlay Mixer`）会创建独立 DirectDraw 7 surface 显示视频——在 Win10 不可靠。换成 VMR9 后，**视频直接画在 D3D9 主窗口**上（VMR9 Mode Windowless）。

### 16.3 IVideoWindow：阻断 DShow 建独立窗口

[IVideoWindow.cpp](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/dshow/IVideoWindow.cpp) 的关键 hack：

| 方法 | 实现 | 用意 |
|---|---|---|
| `put_Visible(true)` | `return S_OK;` 不转发 | 阻止 DShow 显示独立视频窗口 |
| `put_Owner(...)` | `return S_OK;` 不转发 | 阻止 DShow 设置 owner（独立窗口） |
| `put_FullScreenMode(...)` | `return S_OK;` 不转发 | 阻止 DShow 进入独占全屏 |
| `put_Caption(...)` | `return S_OK;` 不转发 | 阻止 DShow 改标题 |
| `put_MessageDrain(...)` | `return S_OK;` 不转发 | 阻止 DShow 转消息 |
| `SetWindowPosition(...)` | 改用 `VMR9->SetVideoPosition` | **关键** —— 把视频定位改写到 VMR9 |
| `get_Visible` | 永远返 true | 视频始终显示在主窗口 |

**完整 hack 链**：DShow 想画视频 → 调 `IVideoWindow::put_Visible` → 我们直接 `S_OK` 吞掉 → DShow 以为窗口在显示 → 调 `SetWindowPosition` → 我们改用 VMR9 的 `SetVideoPosition` → 视频画到主窗口上。

### 16.4 IFilterGraph

[IFilterGraph.cpp](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/dshow/IFilterGraph.cpp) **全转发到 ProxyInterface**，没有自定义逻辑。

---

## 17. 着色器管线（ColorKey 透明效果）

### 17.1 编译期：HLSL → C 头文件

[CMakeLists.txt:5](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/CMakeLists.txt#L5)：

```cmake
add_custom_command(OUTPUT ColorKeyHLSLC
    COMMAND fxc.exe /Eps_main /Vng_colorKeyHLSLC
                  /Tps_2_b /Fh${CMAKE_CURRENT_BINARY_DIR}\\ColorKeyHLSLC.h
                  /nologo ddraw\\ColorKey.hlsl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
```

- `fxc.exe`：Windows SDK 的 DirectX 着色器编译器
- `/Eps_main`：入口函数名
- `/Tps_2_b`：shader model 2.0 byte code（ps_2_b）
- `/Fh<file>`：输出 C 头文件
- `/Vng_colorKeyHLSLC`：生成的 C 数组变量名

**`ps_2_b` 的选择原因**：Shader Model 2.0b 是 DX9 入门级 SM，能用 `tex2D` + 基本 ALU，**所有现代 GPU 都支持**。ColorKey 算法只需比较 + 改 alpha，用不到 SM3+ 的 `tex2Dlod` 等高级特性。

### 17.2 HLSL 源码（[ColorKey.hlsl](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/ColorKey.hlsl)）

```hlsl
float4 srcColorKey;       // 源 color key（不透明色）
float4 destColorKey;      // 目标 color key
bool haveColorKey[2];     // src/dest 是否有 color key
bool checkAlpha;          // Primary 模式：用 alpha 通道判定

sampler2D sampler0;

PS_OUTPUT ps_main(PS_INPUT Input) {
    float4 srcPixel = tex2D(sampler0, Input.Texcoord);
    float4 destPixel = srcPixel;

    if (checkAlpha) {
        // Primary 模式：alpha=0 透明，否则不透明
        destPixel.a = (srcPixel.a == 0.0) ? 0.0 : 1.0;
    }
    else if (haveColorKey[0]) {
        // 普通模式：与 srcColorKey 颜色匹配 → 透明
        if (abs(srcPixel.r - srcColorKey.r) < 0.01 && ...) {
            destPixel.a = 0.0;
        } else {
            destPixel.a = 1.0;
        }
    }
    else {
        destPixel.a = 1.0;
    }

    return destPixel;
}
```

### 17.3 运行时：每张 RenderTarget 创建一个 PS

[IDirectDrawSurface4.cpp:468-473](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L468-L473)：

```cpp
if (m_isRenderTarget) {
    m_spriteHandle = m_d3d9Context->CreateSprite();
    m_d3d9Context->GetDevice()->CreatePixelShader((DWORD*)g_colorKeyHLSLC, &m_colorKeyShader);
    ND3D9::D3DXGetShaderConstantTable((DWORD*)g_colorKeyHLSLC, &m_constantTable);
}
```

**每个 RenderTarget surface 单独 CreatePixelShader + GetConstantTable**，**非常浪费**：
- 同一份字节码可以共享 PS instance（D3D9 允许）
- 但当前实现每张 RT surface 都有独立 PS

### 17.4 DrawSprite 中的常量设置（[line 537-546](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L537-L546)）

```cpp
device9->SetPixelShader(m_colorKeyShader);

ND3D9::D3DXCOLOR srcColorKeyF(srcColorKey ? *srcColorKey : 0);
ND3D9::D3DXCOLOR destColorKeyF(destColorKey ? *destColorKey : 0);
BOOL haveColorKey[2] = { (bool)srcColorKey, (bool)destColorKey };
BOOL checkAlpha = m_surfaceType == ESurfaceType::Primary;

m_constantTable->SetVector(device9, cTbN("srcColorKey"), &srcColorKeyF);
// destColorKey 常量被注释掉（line 544）
m_constantTable->SetBoolArray(device9, cTbN("haveColorKey"), haveColorKey, 2);
m_constantTable->SetBool(device9, cTbN("checkAlpha"), checkAlpha);
```

**注意 line 544**：`destColorKey` 常量**只设置不调用** —— 这是个 bug，DX9 调用 `SetBoolArray` 后又跟了 `m_constantTable->SetFloatArray` 被注释掉。实际效果：目标 color key 在 HLSL 里**永远是 0**（默认值），只有 source color key 起作用。

### 17.5 整体管线效率评估

| 阶段 | 性能 |
|---|---|
| HLSL 编译 | 0（编译期，~10ms） |
| 运行时创 PS | **浪费**：每张 RT surface 都创一次 |
| 推常量 | 每帧每 Blt 调 3 次 `ID3DXConstantTable` |
| 纹理采样 | 1 次 `tex2D` |
| 像素 ALU | ~5 条指令（比较 + 选择） |

**优化方向**：
- 把 PS 创到 D3D9Context 单例（共享）
- 改用 `ID3DXEffect` 或 `ID3DXConstantTable::SetDefaults` 减少 API 调用
- 编译器把 HLSL 拼到 `ID3DXEffect` 字符串里，运行时 `D3DXCreateEffect` 加载

---

## 18. 配置系统分析（重要发现：根本没有配置系统）

### 18.1 仓库扫描结果

通过 `Glob` 扫描 `.ini/.json/.cfg` 扩展名：**0 个配置文件**。

### 18.2 现有"配置"全部硬编码

| 硬编码位置 | 配置项 | 当前值 |
|---|---|---|
| [IDirectDrawSurface4.cpp:22](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L22) | 软件/硬件 Blt 路径 | `g_useSoftwareWrapper9 = false` |
| [D3D9Context.cpp:399](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/D3D9Context.cpp#L399) | 后台缓冲可锁 | `D3DPRESENTFLAG_LOCKABLE_BACKBUFFER` |
| [D3D9Context.cpp:402](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/D3D9Context.cpp#L402) | 关闭 vsync | `D3DPRESENT_INTERVAL_IMMEDIATE` |
| [IDirect3D3.cpp:170](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirect3D3.cpp#L170) | EvictManagedTextures | 转发到 D3D9 |
| [D3D9Context.cpp:276-280](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/D3D9Context.cpp#L276-L280) | D3DRS_LIGHTING 开关 | **注释掉了**（trick for meteor blade） |
| [IDirectDrawSurface4.cpp:1435-1438](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1435-L1438) | 3D OffScreen Lock 行为 | 永远返 `DDERR_INVALIDPARAMS` |
| [IDirectDrawSurface4.cpp:1494-1502](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1494-L1502) | BackBuffer Lock 行为 | 永远返 `DDERR_GENERIC` |
| [IDirectDrawSurface4.cpp:1555](file:///D:/Program%20Files%20(x86)/desktop/meteor/MeteorBladeEnhancer/ddfix/ddraw/IDirectDrawSurface4.cpp#L1555) | ZBuffer Restore 时开 Z | 硬编码 |

### 18.3 CMake 配置项

[CMakeLists.txt:8-9](file:///D:/Program%20Files%20(x86)/desktop/metro/MeteorBladeEnhancer/CMakeLists.txt#L8-L9) 暴露两个 cmake 变量：

| 变量 | 用途 |
|---|---|
| `GameExe` | 游戏 exe 路径（FILEPATH） |
| `GameStartupArgs` | 启动参数，默认 "w"（STRING） |

**这两个变量只在 VS 项目模板 `vcxproj.user.in` 里使用**，让 F5 直接启动游戏调试。**与运行时行为完全无关**。

### 18.4 配置系统缺失的影响

- **想换游戏跑**：所有 "trick for meteor blade" 硬编码，**没法关掉**（譬如另一个老游戏希望 BackBuffer Lock 正常返回）
- **想切硬件/软件 Blt 路径**：要改源码 + 重新编译
- **想看 FPS / 调试信息**：没 IMGUI 也没控制台
- **不同显卡优化**：没法调 `D3DPRESENTFLAG_LOCKABLE_BACKBUFFER` 等开关
- **用户报告问题**：没法看用户实际配置

### 18.5 建议的最小配置方案

```ini
; ddfix.ini
[Render]
UseSoftwareBlt = false       ; g_useSoftwareWrapper9
LockableBackBuffer = true    ; D3DPRESENTFLAG_LOCKABLE_BACKBUFFER
VSync = false                ; PresentationInterval
LightingEnabled = true       ; D3DRS_LIGHTING
AllowBackBufferLock = false  ; BackBuffer Lock 行为
Allow3DOffScreenLock = false ; 3D OffScreen Lock 行为
ZBufferAutoRestore = true    ; ZBuffer Restore 时开 Z

[Log]
Level = INFO                 ; 日志级别
File = ddfix.log             ; 日志路径
```

---

## 19. 关键调用链示例

### 19.1 游戏 Blt 一帧到屏幕

```
[game.exe]
  DirectDrawSurface4::Blt(destRect, srcSurface, srcRect, dwFlags, fx)
  ↓ (m_IDirectDrawSurface4::Blt line 1066-1141)
  ├─ 把 destRect 兜底为全尺寸
  ├─ 检查 dwFlags (DDBLT_COLORFILL / DDBLT_KEYSRC)
  ├─ 如果是 Primary 类型 → destRect 偏移归零（画到 backbuffer）
  ├─ 从 m_desc 读 ColorKey
  ├─ 调 m_surface9Wrapper->Blt(srcSurface9Wrapper, ...)
  │   ↓ (HardwareSurface9Wrapper::Blt line 503-510)
  │   └─ DrawSprite(src, srcRect, srcColorKey, destColorKey, destRect)
  │       ↓ (line 512-569)
  │       ├─ 1. 保存旧 PixelShader / RenderTarget / ZENABLE
  │       ├─ 2. SetRenderTarget(0, this)
  │       ├─ 3. SetRenderState(ZENABLE, FALSE)
  │       ├─ 4. BeginScene + sprite->Begin
  │       ├─ 5. SetPixelShader(m_colorKeyShader)
  │       ├─ 6. 推 ColorKey 常量（srcColorKey / haveColorKey / checkAlpha）
  │       ├─ 7. SetSamplerState(MAG/MIN/MIP = POINT)
  │       ├─ 8. sprite->SetTransform(2D scale+translate matrix)
  │       ├─ 9. sprite->Draw(srcTex9, srcRect, 0xffffffff)
  │       ├─ 10. sprite->Flush/End + EndScene
  │       └─ 11. 恢复旧 PixelShader / RenderTarget / ZENABLE
  └─ 如果 m_surfaceType == Primary:
      ├─ device9->Present(srcRect, destRect, 0, nullptr)
      │   └─ 若返 D3DERR_DEVICELOST → TagDeviceLost() + 返 DDERR_SURFACELOST
      └─ srcSurface->FillColor(srcRect, 0)  (trick for GetDC)
```

### 19.2 游戏用 DInput8 创设备

```
[game.exe]
  DirectInputCreateEx(hinst, version, IID_IDirectInput7, &ptr, ...)
  ↓ (FakeDirectInputCreateEx in dllmain.cpp)
  ├─ GetHooker("DirectInputCreateEx")
  ├─ **不调系统函数** —— 改调 MyDirectInput8Create
  │   ↓ (来自 dinput8.dll)
  │   └─ 返回 IDirectInput8A* (DInput8 升级)
  └─ genericDinputQueryInterface(IID_IDirectInput7, &ptr)
      ↓ (查表适配：游戏要的 IID 转成 m_IDirectInput*)

[game.exe]
  IDirectInput7::CreateDevice(GUID_SysKeyboard, &device, NULL)
  ↓ (m_IDirectInputW2A::CreateDevice)
  ├─ ProxyInterface->CreateDevice(...)  (DInput8 的真实现)
  └─ ProxyAddressLookupTable.FindAddress<m_IDirectInputDeviceA>(device)

[game.exe]
  IDirectInputDeviceA::GetDeviceState(sizeof(keyboardState), &state)
  ↓ (m_IDirectInputDeviceA::GetDeviceState)
  └─ ProxyInterface->GetDeviceState(...)  (DInput8 真实现)
```

### 19.3 视频播放

```
[game.exe]
  CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC, IID_IGraphBuilder, &ptr)
  ↓ (FakeCoCreateInstance in dllmain.cpp)
  ├─ ProxyInterface->CoCreateInstance(...)  (系统 ole32 真实现)
  └─ ProxyAddressLookupTable.FindAddress<m_IGraphBuilder>(ptr)

[game.exe]
  IGraphBuilder::RenderFile(L"intro.bik", NULL)
  ↓ (m_IGraphBuilder::RenderFile line 101-154)
  ├─ CoCreateInstance(CLSID_VideoMixingRenderer9, ..., &vmr9Filter)
  ├─ AddFilter(vmr9Filter, L"Video Mixing Renderer 9")
  ├─ vmr9Filter->QueryInterface(IID_IVMRFilterConfig9, &pConfig)
  │   ├─ pConfig->SetNumberOfStreams(1)
  │   └─ pConfig->SetRenderingMode(VMR9Mode_Windowless)
  ├─ vmr9Filter->QueryInterface(IID_IVMRWindowlessControl9, &vmr9Ctrl)
  │   ├─ vmr9Ctrl->SetVideoClippingWindow(GetActiveWindow())
  │   └─ vmr9Ctrl->SetAspectRatioMode(VMR_ARMODE_NONE)
  └─ ProxyInterface->RenderFile(L"intro.bik", NULL)
      (系统 FilterGraph 用我们插入的 VMR9 作为渲染器)

[game.exe]
  IVideoWindow::put_Visible(TRUE)  (DShow 想显示视频窗口)
  ↓ (m_IVideoWindow::put_Visible line 83-87)
  └─ return S_OK;  (吞掉！DShow 以为显示成功了)

[game.exe]
  IVideoWindow::SetWindowPosition(x, y, w, h)  (DShow 想定位视频窗口)
  ↓ (m_IVideoWindow::SetWindowPosition line 189-233)
  ├─ FindFilterByName("Video Mixing Renderer 9", &vmr9)
  ├─ vmr9->QueryInterface(IID_IVMRWindowlessControl9, &vmr9Ctrl)
  ├─ RECT rect = {x, y, x+w, y+h}
  └─ vmr9Ctrl->SetVideoPosition(NULL, &rect)  (改写到 VMR9)
```

---

## 20. 完整目录结构与文件清单

```
MeteorBladeEnhancer/
├── CMakeLists.txt                            # 顶层构建
├── README.md                                 # 21 行公开文档
├── LICENCE.txt                               # zlib 类协议
├── .gitignore
├── ExtraDxSDK/                               # 离线 D3DX9 头/库
│   ├── Include/  (11 头)
│   └── Lib/      (3 lib)
├── minhook/                                  # 第三方 API hook 库
│   ├── dll_resources/  (MinHook.def, MinHook.rc)
│   ├── include/MinHook.h
│   └── src/  (hde/, buffer.c, hook.c, trampoline.c)
├── PROJECT_ANALYSIS.md                       # 本文档
└── ddfix/                                    # ★ 核心实现
    ├── CMakeLists.txt
    ├── vcxproj.user.in                       # VS 调试启动模板
    ├── exports.def                           # 导出 7 个 ddraw 符号
    ├── dllmain.cpp                           # DLL 入口 + 3 大 hook
    ├── D3D9Context.h / .cpp                  # D3D9 单例 + 6 个工厂
    ├── Common/
    │   ├── Logging.h                         # 非线程安全 ofstream
    │   ├── SmartPointer.h                    # CComPtr-like
    │   └── Wrapper.h                         # ★ 两个查找表基础设施
    ├── ddraw/                                # ★ 核心实现 (27 cpp + 31 h)
    │   ├── ColorKey.hlsl                     # 像素着色器源码
    │   ├── ddraw.h                           # 头聚合
    │   ├── InterfaceQuery.cpp                # 空（宏定义）
    │   ├── IDirectDraw.h / .cpp (v1)         # 入口
    │   ├── IDirectDraw2-7  …                 # 壳
    │   ├── IDirectDraw4.h / .cpp             # 真正接管游戏窗口
    │   ├── IDirectDrawSurface.h / .cpp (v1)  # 壳
    │   ├── IDirectDrawSurface2-7  …          # 壳
    │   ├── IDirectDrawSurface4.h / .cpp      # ★ 核心：3 种 wrapper
    │   ├── IDirectDrawClipper  …             # 自管 HWND
    │   ├── IDirectDrawPalette  …             # 壳
    │   ├── IDirectDrawGammaControl.h         # 壳
    │   ├── IDirectDrawColorControl.h         # 壳
    │   ├── IDirectDrawEnumSurface  …         # 枚举回调适配
    │   ├── IDirectDrawFactory  …             # 壳
    │   ├── IDirect3D / 2 / 7  …              # 壳
    │   ├── IDirect3D3.h / .cpp               # 真正入口
    │   ├── IDirect3DDevice / 2 / 7  …        # 壳
    │   ├── IDirect3DDevice3.h / .cpp         # ★ 核心：完整 D3D9 实现
    │   ├── IDirect3DExecuteBuffer  …         # 壳
    │   ├── IDirect3DLight.h / .cpp           # D3DLIGHT 完整翻译
    │   ├── IDirect3DMaterial / 2 / 3  …      # 自管 v3
    │   ├── IDirect3DTexture / 2  …           # v2 自管 + CPU Load
    │   ├── IDirect3DVertexBuffer / 7  …      # 壳
    │   └── IDirect3DViewport / 2 / 3  …      # v3 自管 + lights
    ├── dinput/                               # dinput 代理（12 对 h/cpp）
    │   ├── IDirectInputA/W …                 # 入口
    │   ├── IDirectInput2A/W …                # 壳
    │   ├── IDirectInput7A/W …                # 壳
    │   ├── IDirectInputDeviceA/W …           # 设备
    │   ├── IDirectInputDevice2A/W …          # 壳
    │   ├── IDirectInputDevice7A/W …          # 壳
    │   ├── IDirectInputEffect.cpp            # 力反馈
    │   ├── IDirectInputEnumEffect.cpp        # 枚举
    │   ├── InterfaceQuery.cpp                # 空
    │   └── dinput.h
    └── dshow/                                # DShow 桥接 VMR9
        ├── IFilterGraph.h / .cpp             # 纯转发
        ├── IGraphBuilder.h / .cpp            # ★ RenderFile 主动建 VMR9
        ├── IVideoWindow.h / .cpp             # ★ put_Visible/put_Owner 吞掉
        └── dshow.h
```

**代码量统计**（行数近似）：
- ddraw 实现：~5500 行
- ddraw 壳：~3500 行
- dinput：~2000 行
- dshow：~700 行
- 工具 / Context / Common：~1500 行
- HLSL：59 行
- **总计：~13000 行 C++ + 59 行 HLSL**

---

## 21. 风险评级与修复路线图

### 21.0 修复状态（Phase 1 已完成）

以下 6 个 P0 崩溃性 bug 已修复：

| # | Bug | 状态 | Commit |
|---|---|---|---|
| 1 | `m_IDirectDrawSurface4::Flip` 访问 nullptr ProxyInterface | ✅ 已修复 | `fix(p0/flip): D3D9 RenderTarget swap + Present 替代 nullptr ProxyInterface` |
| 2 | `m_IDirect3D3::CreateVertexBuffer` 访问 nullptr ProxyInterface | ✅ 已修复 | `fix(p0/vb): CreateVertexBuffer9 + m_IDirect3DVertexBuffer7 包装，ProxyInterface 由 nullptr 改为 m_SelfRefs 自管理` |
| 3 | `m_IDirect3D3::FindDevice` 访问 nullptr ProxyInterface | ✅ 已修复 | `fix(p0/finddev): 改读 m_IDirectDraw::GetD3DDevice3Desc 合成 D3DDEVICEDESC，绕过 nullptr ProxyInterface` |
| 4 | `D3D9Context::ResetDevice` 的 `assert(refs == 0)` 崩溃 | ✅ 已修复 | `fix(p0/reset): while-loop Release 替代两次 Release + assert，refs != 0 改 log warning` |
| 5 | BackBuffer 单独建时 `m_surface9Wrapper = nullptr` | ✅ 已修复 | `fix(p0/backbuffer): BackBuffer 单独建时创 HardwareSurface9Wrapper，置 m_isRenderTarget/m_isTex = true` |
| 6 | `BltFast` on Primary 的 `assert(false)` 崩溃 | ✅ 已修复 | `fix(p0/bltfast): 删除 assert(false)，构造 destRect 后 redirect 到 Blt 路径` |

**修复细节**：

- **Bug #1 (Flip)**: 在 `m_IDirectDrawSurface4::Flip` 中删除对 `ProxyInterface->Flip(...)` 的调用，改为使用 `m_linkedNextSurface->m_surface9Wrapper` 切换 D3D9 设备的渲染目标，再调用 `device9->Present(...)`；若返回 `D3DERR_DEVICELOST` 则通过 `D3D9Context::Instance()->TagDeviceLost()` 标记并返回 `DDERR_SURFACELOST`。
- **Bug #2 (CreateVertexBuffer)**: 通过 `D3D9Context::Instance()->CreateVertexBuffer9(...)` 申请 D3D9 顶点缓冲，再由新增的 `m_IDirect3DVertexBuffer7(nullptr, handle, ...)` 包装；当 `ProxyInterface` 为 nullptr 时，由新增的 `m_SelfRefs` 自行管理引用计数，`Lock/Unlock/AddRef/Release/QueryInterface` 优先走 D3D9 路径。
- **Bug #3 (FindDevice)**: 从 `WrapperAddressLookupTable->FindWrapper<m_IDirectDraw>(IID_IDirectDraw)` 取出 `m_IDirectDraw`，读取其 `GetD3DDevice3Desc()` 合成的 `D3DDEVICEDESC` 写入 `result->ddDeviceDesc`，避开对 nullptr 的访问。
- **Bug #4 (ResetDevice)**: 将原先的 `refs = ptr->Release(); refs = ptr->Release(); assert(refs == 0);` 改为 `while (refs > 0) refs = ptr->Release();`，并对 `refs != 0` 的剩余引用输出 `logf(...)` warning，移除 assert 防止 release 构建崩溃。
- **Bug #5 (BackBuffer)**: 在 `m_IDirectDrawSurface4` 构造函数 `switch (m_surfaceType)` 的 `ESurfaceType::BackBuffer` 分支中，从 `break` 改为创 `HardwareSurface9Wrapper`（按需可走 `SoftwareSurface9Wrapper`），并设置 `m_isRenderTarget = true`、`m_isTex = true`，确保后续 `Blt/BltFast` 等调用不会因 wrapper 为 null 崩溃。
- **Bug #6 (BltFast on Primary)**: 删除 `assert(false)`，构造 `destRect = {dwX, dwY, dwX + w, dwY + h}`（其中 `w/h` 由 `lpSrcRect` 计算），然后 redirect 调用 `Blt(&destRect, lpDDSrcSurface, lpSrcRect, dwFlags, nullptr)`。

**影响文件**：
- `ddfix/ddraw/IDirectDrawSurface4.cpp`（Flip / BltFast / BackBuffer 分支）
- `ddfix/ddraw/IDirect3D3.cpp`（FindDevice / CreateVertexBuffer）
- `ddfix/ddraw/IDirect3DVertexBuffer7.h`（新增 `m_vertexBuffer9Handle`、`m_SelfRefs`）
- `ddfix/ddraw/IDirect3DVertexBuffer7.cpp`（Lock/Unlock/AddRef/Release/QueryInterface 重写）
- `ddfix/D3D9Context.cpp`（ResetDevice 循环 Release + warning）

### 21.1 按修复成本 / 收益比排序

| # | 修复项 | 严重度 | 修复成本 | 收益 | 状态 |
|---|---|---|---|---|---|
| 1 | Flip nullptr 崩溃 | 🔴 | 中 | 高（某些场景直接挂） | ✅ 已修复 |
| 2 | CreateVertexBuffer / FindDevice nullptr | 🔴 | 低 | 高（顶点缓冲必备） | ✅ 已修复 |
| 3 | BackBuffer 单建时 m_surface9Wrapper null | 🔴 | 低 | 高（备份链） | ✅ 已修复 |
| 4 | D3D9Context::ResetDevice assert | 🔴 | 低 | 中（设备丢失恢复） | ✅ 已修复 |
| 5 | g_texformats 用 __LINE__ 计数 | 🟡 | 极低 | 低（不易触发的脆性） | ⏳ 待办（Task 5.4） |
| 6 | 实现 Overlay surface | 🟡 | 中 | 中（部分游戏用） | ⏳ 待办（Task 2.1） |
| 7 | BackBuffer GetDC 返 DDERR_GENERIC | 🟡 | 中 | 低（README 已知） | ⏳ 待办 |
| 8 | 日志线程安全 | 🟡 | 中 | 中（多线程日志） | ⏳ 待办（Task 5.1） |
| 9 | D3D9Context::CalcBackBufferSize 不响应 SetDisplayMode | 🟡 | 中 | 高（伪装多分辨率） | ⏳ 待办 |
| 10 | Log::LOG 用 mutex 保护 | 🟡 | 极低 | 中 | ⏳ 待办（Task 5.1） |
| 11 | m_IDirect3D3::CreateDevice 走 WrapperLookupTable 单例 | 🟡 | 极低 | 中 | ⏳ 待办（Task 2.6） |
| 12 | Texture2::Load 支持 ColorKey 时不分 RGBA | 🟢 | 极低 | 低（个别场景） | ⏳ 待办 |
| 13 | 加 .ini 配置 | 🟢 | 中 | 高（用户可调） | ⏳ 待办（Phase 3） |
| 14 | 加 IMGUI 调试 HUD | 🟢 | 中 | 中（开发期） | ⏳ 待办（Phase 4） |
| 15 | 删 20+ 壳包装 | 🟢 | 低 | 中（代码精简） | ⏳ 待办（Task 5.2） |

### 21.2 推荐修复顺序

**第一批（修复崩溃）** ✅ 全部完成（2026-06-22）
1. Flip → 仿照 DrawSprite 切 RenderTarget + Present
2. CreateVertexBuffer / FindDevice → 转发到 D3D9 实现（用 `D3D9Context` 单例的 `D3DDev9->CreateVertexBuffer`）
3. BackBuffer 单建 wrapper → 在 `m_IDirectDrawSurface4` 构造时检查 `dwCaps & DDSCAPS_BACKBUFFER`，若不是 Primary 链上的子节点，自己创 `HardwareSurface9Wrapper`
4. ResetDevice assert → 改用 `while (refs > 0) refs = ptr->Release();`

**第二批（补全功能）**：
5. Overlay surface → 新建一个 `Overlay9Wrapper` 类（Blt 都返 `DDERR_GENERIC`）
6. m_IDirect3DDevice3 的 `Begin/BeginIndexed/Vertex/Index/End` 立即模式 → 用 `D3DDev9->DrawPrimitiveUP` 改写（用 vertex buffer 缓存）
7. 日志线程安全 → 加 `std::mutex` 保护 `Log::LOG`

**第三批（架构改进）**：
8. .ini 解析 → 用 Windows `GetPrivateProfileStringA` 或单头 `inih`
9. 删壳 → 改 `IDirectDraw4::QueryInterface` 同时支持 IID_IDirectDraw{2,3,7} 转发到 `IDirectDraw4` 的接口
10. PS 共享 → 创到 D3D9Context 单例

---

## 22. 总结：项目核心洞察

1. **本质**：一个**薄 DX6/DX7 → D3D9 翻译层** + **DInput8 升级** + **DShow 桥 VMR9**，针对《流星蝴蝶剑.net》做了大量 trick
2. **架构**：单 DLL 伪装系统 ddraw.dll，三大 hook 命名空间分头并进
3. **核心**：约 10 个真实现类，20+ 壳包装类，DX6 接口全拦截后用 D3D9 重新实现
4. **代码来源**：3 个上游项目（DirectX-Wrappers、MinHook、DXGL）拼装而成
5. **完成度**：能让游戏**勉强跑起来**，但有 6 个直接崩溃的 bug 和 8 个功能缺失
6. **可扩展性**：缺乏配置系统，所有 trick 硬编码；缺乏 CI/测试；缺乏文档（README 21 行）
7. **改进空间**：很小的工作量就能修崩溃（4 个 bug ~ 200 行代码），中等工作量能补全功能（~500 行），加 .ini 配置 + IMGUI 可以让项目变成一个**通用老游戏 DX6→DX9 兼容层**（不只服务流星蝴蝶剑）

---

## 23. Phase 6 状态（2026-06-22 实装）

> 本节是 Phase 6 实装后的"项目当前状态"快照。
> 详细 task 描述见 [.trae/specs/evolve-meteor-blade-enhancer/tasks.md](../.trae/specs/evolve-meteor-blade-enhancer/tasks.md) 的 Phase 6 段。

### 23.1 实装清单

| # | Task | 状态 | 关键产出 |
|---|---|---|---|
| 6.1 | 测试框架 | ✅ 完成 | `tests/SingleTest.h`（自包含，零依赖，决策变更：不引 gtest）+ `tests/CMakeLists.txt` |
| 6.2 | Wrapper 测试 | ✅ 完成 | `tests/test_wrapper.cpp`（mock 化，11 用例） |
| 6.3 | INI 解析器测试 | ✅ 完成 | `tests/test_ini_parser.cpp`（26 用例） |
| 6.4 | PerfCounter 测试 | ✅ 完成 | `tests/test_perf_counter.cpp`（11 用例） |
| 6.5 | HLSL 产物测试 | ✅ 完成 | `tests/test_hlsl.cpp`（7 用例，**范围变更**：原 plan "工厂分发"改为 HLSL 产物验证） |
| 6.6 | 测试入口 | ✅ 完成 | `tests/main.cpp`（支持 `--list` / `--filter` / `--help`） |
| 6.7 | CI | ✅ 完成 | `.github/workflows/build.yml`（windows-latest + VS 2022 + Ninja + ctest） |
| 6.8 | 文档 | ✅ 完成 | `README.md`（21 → 460+ 行）+ `ARCHITECTURE.md`（400+ 行新建） |

### 23.2 关键决策变更

1. **不引 gtest**：原 plan 用 `FetchContent` 拉 gtest，但项目是**离线分发**，不能联网。改用自包含 SingleTest（单头 + 200 行）
2. **测试位置**：原 plan 把 tests/ 放在 ddfix/ 子目录，但实际放在**项目根** `tests/`（与 ddfix 平级）
3. **范围调整**：原 Task 6.5 "工厂分发" 测试改为 HLSL 产物验证（工厂强依赖 D3D9 device，单元测试覆盖不到）

### 23.3 测试覆盖现状

| 模块 | 覆盖度 | 备注 |
|---|---|---|
| `ddfix/Config/IniParser` | 🟢 高 | 26 用例覆盖所有 API + 边界情况 |
| `ddfix/Common/Wrapper` | 🟡 中 | mock 化测试 11 用例，未实际链接 ddfix |
| `ddfix/Debug/PerfCounter` | 🟡 中 | 11 用例覆盖 Increment + Sliding + PERF_SCOPE |
| `ddfix/ddraw/ColorKey.hlsl` | 🟡 中 | 7 用例验证源 + 生成产物 |
| `ddfix/ddraw/IDirectDrawSurface4` | 🔴 无 | 需要游戏跑通 + D3D9 device 注入，CI 无法自动化 |
| `ddfix/D3D9Context` | 🔴 无 | 同上 |
| `ddfix/Config/ConfigManager` | 🔴 无 | 集成测试级别，需要 ddfix.ini fixture |
| Phase 1-5 bug 修复 | 🔴 无 | 集成测试级别 |

> **结论**：单元测试覆盖 4 个**纯逻辑**模块；DDraw 真实 wrapper 测试需集成测试，CI 不能自动化。

### 23.4 文档现状

- `README.md`：460+ 行，**用户友好**，含快速开始 / 构建 / 使用 / 配置 / 调试 / 已知问题 / 贡献
- `ARCHITECTURE.md`：400+ 行，**开发者友好**，含架构图 + 三大子系统 + 关键调用链 + Phase 1-6 演进
- `PROJECT_ANALYSIS.md`：1300+ 行（本文），**深度分析**，含逐文件说明
- `ddfix.ini.example`：90+ 行，所有 key 含注释 + Game Profile 示例

### 23.5 后续 Phase 7+ 建议

| 优先级 | 任务 | 说明 |
|---|---|---|
| 🟡 | ISurface9Wrapper 集成测试 | 用 mock DirectX runtime + 真 D3D9 device 注入 |
| 🟡 | ConfigManager 集成测试 | 写 fixture ddfix.ini + 验证 fallback |
| 🟡 | IID 映射回归测试 | Task 5.5 删壳前置 |
| 🟡 | 性能基准测试 | 跑已知游戏场景，断言 Blt/s / FPS 在合理范围 |
| 🟢 | 真实 D3D9 端到端测试 | 启动 ddfix.dll + 调 DirectDrawCreate + 验证 device 创成功 |
| 🟢 | Linux 交叉编译验证 | 完成 cmake/mingw-x86_64.cmake + CI 加 linux 任务 |
| 🟢 | 模糊测试（fuzz） | libFuzzer 测 IniParser（input = INI 文本） |

### 23.6 整体状态总结

```
Phase 1: ✅ 崩溃修复（6 P0 bug）
Phase 2: ✅ 功能补全（6 P1 功能）
Phase 3: ✅ 配置系统（IniParser + ConfigManager + 9 硬编码开关 + Game Profile）
Phase 4: ✅ 调试 HUD（HudRenderer + PerfCounter + F12 + Config 闸门）
Phase 5: ✅ 架构优化（日志 + Build 选项 + namespace 占位 + 壳类标注，部分完成）
Phase 6: ✅ 测试 & 文档（SingleTest + 4 test_*.cpp + CI + README + ARCHITECTURE）
Phase 7+: ⏳ 集成测试 + 性能基准 + IID 回归 + 模糊测试
```

> **关键里程碑**：经过 Phase 1-6 实装，项目从"勉强跑起来"演化为"**有完整测试 + 文档 + CI** 的可维护项目"。

---

**分析完成**。基于这份文档，我们可以：
- 选 P0 bug 做修复（让当前游戏更稳定）
- 选 .ini 配置 + 修 bug 做小型重构（让项目支持更多老游戏）
- 选 IMGUI + 测试 + 文档做完整翻新（让它成为社区项目）
- 或其它方向（请告知）
