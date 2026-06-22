#pragma once

// ======================================================================
// Phase 5.4: 命名空间解耦 —— DDRAW:: / ND3D9:: 包装（保守版）
// ======================================================================
//
// 设计目标
// --------
//   把项目里两套互不相干的类型集拆开，避免 "IDirectDraw4" 在 D3D9 命名空间
//   和 ddraw.h 全局命名空间里出现歧义：
//
//     namespace DDRAW  { /* DX6/7 头：ddraw.h, ddrawex.h, d3d.h */ }
//     namespace ND3D9  { /* D3D9 头：d3d9.h, d3dx9.h + D3D9Context */ }
//
//   这样所有 COM 接口、IID GUID、结构体都明确归属，编译器一报错就能立刻定位。
//
// 现状
// ----
//   1. ddraw.h / ddrawex.h / d3d.h 这些 **第三方头** 没有也无法用 namespace 包起来
//      （系统头会展开宏、写 typedef 到全局）。强行包会引入上千个编译错误。
//   2. ND3D9 命名空间在 D3D9Context.h 已经成立，里面有 d3d9.h / d3dx9.h 和
//      D3D9Context / IResource9Factory / 资源句柄类型。**这一半已经完成**。
//   3. DDRAW:: 命名空间尚未建立；当前项目依赖 ddraw.h 把所有类型直接暴露到
//      全局命名空间。
//
// 本文件的作用
// ------------
//   1. **不包含任何 DX6 头**（不引入 ddraw.h 之类），保证不破坏现有编译。
//   2. 声明 `namespace DDRAW` 作为命名空间占位（marker）。
//   3. 提供 `namespace DDRAW = ::DDRAW;` 别名兼容旧代码（虽然当前实际未启用，
//      但保留入口给未来彻底迁移时用）。
//   4. 详细记录"为什么保守"、"风险点"、"分阶段实施计划"。
//
// ======================================================================
// 风险评估（为什么这一版只做占位）
// ======================================================================
//
// A. ddraw.h 内的宏（DDSD_CAPS / DDPF_* 等）会展开成 ::DWORD 等全局名，
//    包到 namespace DDRAW 里会让所有 "DWORD dwFlags" 报 "未声明标识符"。
//    要彻底解决，必须改写一份本地 ddraw_local.h 重新声明所有宏 + 类型，
//    工作量约 1-2 人天。
//
// B. IID_IDirectDraw* 等 GUID 用 `extern "C"` 链接，包到 namespace 会破坏
//    __declspec(uuid(...)) 属性，影响 QueryInterface 的 IID 比较。
//
// C. m_IDirectDraw4::QueryInterface 现在依赖宏比较：
//      if (riid == IID_IDirectDraw4) ...
//    改 namespace 后要么全用 `DDRAW::IID_IDirectDraw4`，要么 `using` 拉回
//    全局；前者工作量大，后者等于没改。
//
// 因此 Phase 5.4 决定：
//   * 仅落"命名空间占位"文件，不替换 #include "ddraw.h"
//   * 把"未来 1-2 人天的全套改造"列入 PROJECT_ANALYSIS.md §21.5
//   * 任何新增的、与 DX6 无关的代码必须显式进 ND3D9（已落地）
//
// ======================================================================

// 命名空间占位（marker）。当前为空实现。
// 未来若要彻底迁移，把 ddraw.h 替换成本地 ddraw_local.h 并把
// 全部类型重新声明到 namespace DDRAW 内即可，所有 #include "ddraw.h"
// 的源文件再 `#include "Common/NamespaceDDraw.h"` 即可。
namespace DDRAW
{
	// 未来迁移完成后，下面这些 forward declarations 会替代 ddraw.h：
	//
	//   struct IDirectDraw;
	//   struct IDirectDraw2;
	//   struct IDirectDraw4;
	//   struct IDirectDraw7;
	//   struct IDirectDrawSurface;
	//   struct IDirectDrawSurface4;
	//   struct IDirectDrawSurface7;
	//   struct IDirect3D;
	//   struct IDirect3D3;
	//   struct IDirect3D7;
	//   struct IDirect3DDevice;
	//   struct IDirect3DDevice3;
	//   struct IDirect3DDevice7;
	//   struct IDirect3DTexture;
	//   struct IDirect3DTexture2;
	//   struct IDirect3DVertexBuffer;
	//   struct IDirect3DVertexBuffer7;
	//   struct IDirect3DLight;
	//   struct IDirect3DMaterial;
	//   struct IDirect3DMaterial3;
	//   struct IDirect3DViewport;
	//   struct IDirect3DViewport3;
	//   struct IDirect3DExecuteBuffer;
	//   struct IDirectDrawClipper;
	//   struct IDirectDrawPalette;
	//   struct IDirectDrawGammaControl;
	//   struct IDirectDrawColorControl;
	//   struct IDirectDrawFactory;
	//   struct DDSURFACEDESC2;
	//   struct DDPIXELFORMAT;
	//   struct D3DDEVICEDESC;
	//   ... (约 80+ 个类型)
}

// 别名兼容层说明：
//   当前 DX6 头（ddraw.h 等）仍把所有类型暴露在全局命名空间（::），
//   所以"全局 ::DDRAW" 实际是空的，namespace DDRAW { } 是给未来用的 marker。
//   不要写 `namespace DDRAW = ::DDRAW;` 这种自指别名（C++ 禁止）。
//
//   未来彻底迁移的步骤（不在 Phase 5.4 范围内）：
//     1) 准备 ddraw_local.h，在 namespace DDRAW { } 里手写所有 typedef / 宏 / GUID
//     2) 把所有 #include "ddraw.h" 改成 #include "Common/NamespaceDDraw.h"
//        并 #include "dddraw_local.h"
//     3) 全局替换 QueryInterface 里的 IID 比较为 DDRAW::IID_*
//     4) 编译 + 跑全套 IID 映射测试

namespace NDDFIX
{
	// 占位：项目自己的根命名空间（Phase 6 之后逐步把所有 wrapper 类迁入）。
	// 当前 IniParser.h、ConfigManager.h、HudRenderer.h 等已使用 NDDFIX::*。
	// 这里声明 namespace 是为了把"ddfix 全局符号"和"D3D9 上下文符号"区分开。
}
