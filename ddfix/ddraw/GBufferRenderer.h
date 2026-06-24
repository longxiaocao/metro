// GBufferRenderer.h - Phase 9.3.8-9.3.11 GBuffer MRT + Deferred Lighting 渲染管线
//
// 设计目标：
//   1. 单一职责：管理 GBuffer 4 个 MRT + 关联 depth stencil + 编译/缓存 shader
//   2. 单例：dll 范围内一份，集成简单（与 PostProcess/SMAA 一致）
//   3. 失败容错：任何步骤失败（创 RT/编译 shader）→ 内部标记 unavailable，
//      Run() 退化为 no-op，绝不阻断 Execute 路径
//   4. 单元测试友好：GBuffer 大小配置纯逻辑函数暴露为 static，
//      ddfixtests 链接 ddfix-static 即可测
//
// 集成点 (Phase 9.3.11a)：
//   - IDirect3DDevice::Execute hook 调 ParseExecuteBuffer 拿到 ExtractedGeometry
//   - hook 内再调 GBufferRenderer::Instance()->RenderFrame(...) 走 GBuffer + Deferred
//   - 失败/未启用 → 旁路，原始 Execute 走原路径
//
// Phase 9.3.11a 范围（简化集成）：
//   - GBuffer 4 个 RT 创建/绑定
//   - 编译 GBuffer VS / PS / Deferred PS
//   - 占位调用 RenderFrame，参数校验 + 路径准备
//   - 实际 draw call (顶点流) 留 9.3.11b 实现
//
// 基于：GPU Gems 2 第 9 章 "Deferred Shading in S.T.A.L.K.E.R." (Oles Shishkovtsov)
//       公开在线：https://developer.nvidia.com/gpugems/gpugems2/part-02-shading-lighting-and-shadows/chapter-9-deferred-shading-stalker
//       严格 Clean Room：不参考 6.5.9 .fx 源码
//
// 内存布局约束 (Microsoft Learn "Multiple Render Targets")：
//   - 4 个 RT 必须相同尺寸
//   - 关联 depth stencil 必须相同尺寸
//   - D3D9 限定所有 RT 共享 multisample 设置
//
// GBuffer 格式选择 (经典 STALKER 配置)：
//   RT0 PosTex     : D3DFMT_A16B16G16F (世界位置.xyz = float16 x 3, 深度 = float16)
//   RT1 NormalTex  : D3DFMT_A8R8G8B8   (法线 (n*0.5+0.5).rgb + specPower.a)
//   RT2 DiffuseTex : D3DFMT_A8R8G8B8   (diffuse.rgb + alpha.a)
//   RT3 SpecularTex: D3DFMT_A8R8G8B8   (specular.rgb + shininess.a)
//   DepthStencil   : D3DFMT_D24S8      (24-bit depth + 8-bit stencil)
//
// 风险点 (Phase 9.3 关注)：
//   - D3DFMT_A16B16G16F 需要 D3D9 device caps (D3DPTEXTURECAPS_POW2 不强制 +
//     D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS 支持), 老 GPU 可能不支持
//   - 4 个 MRT 需 D3DCAPS9::NumSimultaneousRTs >= 4
//   - 失败时降级: 退化为 1 个 RT (无 deferred) 或 0 个 RT (skip 整 pass)

#pragma once

#include "../D3D9Context.h"
#include "../ddraw/ExecuteBufferParser.h"

#include <d3d9.h>
#include <d3dx9.h>

namespace NDDFIX
{
namespace Render
{

// GBuffer 格式配置 (D3D9 baseline + 公开文献推荐)
struct GBufferFormat
{
    D3DFORMAT posFormat;      // 默认 D3DFMT_A16B16G16F
    D3DFORMAT normalFormat;   // 默认 D3DFMT_A8R8G8B8
    D3DFORMAT diffuseFormat;  // 默认 D3DFMT_A8R8G8B8
    D3DFORMAT specularFormat; // 默认 D3DFMT_A8R8G8B8
    D3DFORMAT depthFormat;    // 默认 D3DFMT_D24S8

    GBufferFormat()
        // WHY: D3DFMT_A16B16G16F (3-channel float) 不存在于 D3D9 官方枚举
        //   公开 d3d9types.h 只定义 D3DFMT_A16B16G16R16F (4-channel 64-bit float)
        //   这里用 R16F 通道替代, alpha 通道暂时不存数据 (后续 phase 接真正 depth texture)
        : posFormat(D3DFMT_A16B16G16R16F)
        , normalFormat(D3DFMT_A8R8G8B8)
        , diffuseFormat(D3DFMT_A8R8G8B8)
        , specularFormat(D3DFMT_A8R8G8B8)
        , depthFormat(D3DFMT_D24S8)
    {
    }
};

// Phase 9.3.8-9.3.11 GBuffer + Deferred 渲染管线
class GBufferRenderer
{
public:
    static GBufferRenderer* Instance();

    // 初始化：编译 shader + 创建 GBuffer 4 RT + depth stencil。
    //   - 设备就绪后 (D3D9Context::Initialize 之后) 调一次
    //   - 失败 → 内部 unavailable, RenderFrame 退化为 no-op
    //   - 多次调用安全：先 Shutdown 再 Initialize
    HRESULT Initialize(IDirect3DDevice9* device, int width, int height);

    // 释放所有资源；Reset 设备 / 卸载 ddfix 时调
    void Shutdown();

    // 检查 device 是否支持本 GBuffer 配置 (4 个 RT + MRT bit depth independent)
    //   失败原因：D3DFMT_A16B16G16F 不支持 / NumSimultaneousRTs < 4
    //   - 静态方法，单元测试可独立调（不依赖 device）
    //   - device = nullptr 时仅检查"理论上是否所有 D3D9 设备都应支持" (仅看 d3d9 caps)
    static HRESULT CheckDeviceCapabilities(IDirect3DDevice9* device);

    // Phase 9.3.11a: 渲染一帧（占位 + 真实 GBuffer PS 调用）
    //   参数：
    //     - geometry: ExecuteBufferParser 解析结果 (vertices + triangles)
    //     - viewMatrix / projMatrix: D3D9 视图/投影矩阵 (从 device GetTransform 拿)
    //   行为：
    //     1. 绑 GBuffer 4 RT + depth stencil
    //     2. 清 GBuffer 4 RT (黑)
    //     3. 调 GBuffer VS/PS (输入 geometry, 输出 4 RT)  // Phase 9.3.11b 真实 draw
    //     4. 解绑 GBuffer, 恢复 back buffer RT
    //     5. 清 back buffer
    //     6. 调 Deferred PS (全屏 quad, 采样 4 GBuffer 纹理)
    //   失败 (RT 失效 / shader 失效) → 返 E_FAIL, 不影响原始 Execute 路径
    HRESULT RenderFrame(IDirect3DDevice9* device,
                       const ExtractedGeometry& geometry,
                       const D3DXMATRIX& viewMatrix,
                       const D3DXMATRIX& projMatrix);

    // 模式读写（运行期可改，预留）
    void SetEnabled(bool b) { m_enabled = b; }
    bool IsEnabled() const  { return m_enabled; }

    // 单元测试用：检查 Initialize 是否成功 (true = GBuffer 可用)
    bool IsAvailable() const { return m_available; }

    // 当前 GBuffer 尺寸 (供测试 / HUD 验证)
    int GetWidth()  const { return m_width; }
    int GetHeight() const { return m_height; }

    // 拿 GBuffer 资源 (供 GBuffer PS / Deferred PS 消费)
    //   返 nullptr 表示资源未创建 (Initialize 失败)
    IDirect3DTexture9* GetPosTex()     const { return m_posTex; }
    IDirect3DTexture9* GetNormalTex()  const { return m_normalTex; }
    IDirect3DTexture9* GetDiffuseTex() const { return m_diffuseTex; }
    IDirect3DTexture9* GetSpecTex()    const { return m_specTex; }

    // Phase 9.3.11a: 公开绑/解绑 API (供 D3D9Context 薄包装层调)
    //   内部 RenderFrame 已自动 Bind/Unbind, 这里公开是为了让外部能
    //   单独控制 GBuffer RT 切换 (例如 Phase 9.3.11b 真实 draw 路径).
    //   RenderFrame 调用方无需手动 Bind/Unbind.
    HRESULT BindGBufferAsRenderTarget(IDirect3DDevice9* device);
    HRESULT UnbindGBuffer(IDirect3DDevice9* device);

private:
    GBufferRenderer();
    ~GBufferRenderer();
    GBufferRenderer(const GBufferRenderer&) = delete;
    GBufferRenderer& operator=(const GBufferRenderer&) = delete;

    // 内部：创建 GBuffer 4 个 RT + depth stencil
    HRESULT CreateGBufferResources(IDirect3DDevice9* device, int width, int height);
    void ReleaseGBufferResources();

    // 内部：编译 GBuffer VS / PS / Deferred PS (懒加载)
    HRESULT EnsureShaders(IDirect3DDevice9* device);

    // 内部：渲染 GBuffer VS/PS (Phase 9.3.11b 完整实现, 当前 9.3.11a 占位)
    HRESULT RenderGBufferPass(IDirect3DDevice9* device,
                              const ExtractedGeometry& geometry,
                              const D3DXMATRIX& viewMatrix,
                              const D3DXMATRIX& projMatrix);

    // 内部：渲染 Deferred PS (全屏 quad)
    HRESULT RenderDeferredPass(IDirect3DDevice9* device);

    // 状态
    bool m_enabled;
    bool m_available;
    int  m_width;
    int  m_height;

    // GBuffer 资源 (裸指针, 由 d3d9 device 管理; Reset 时失效)
    IDirect3DTexture9* m_posTex;
    IDirect3DTexture9* m_normalTex;
    IDirect3DTexture9* m_diffuseTex;
    IDirect3DTexture9* m_specTex;
    IDirect3DSurface9* m_posSurface;
    IDirect3DSurface9* m_normalSurface;
    IDirect3DSurface9* m_diffuseSurface;
    IDirect3DSurface9* m_specSurface;
    IDirect3DSurface9* m_depthSurface;

    // 保存/恢复 (BindGBuffer/UnbindGBuffer 配对)
    IDirect3DSurface9* m_oldRenderTarget;
    IDirect3DSurface9* m_oldDepthStencil;

    // Shader (懒加载, Reset 时失效)
    IDirect3DVertexShader9* m_gBufferVS;
    IDirect3DPixelShader9*  m_gBufferPS;
    IDirect3DPixelShader9*  m_deferredPS;
    ID3DXConstantTable*     m_gBufferVSConst;
    ID3DXConstantTable*     m_gBufferPSConst;
    ID3DXConstantTable*     m_deferredPSConst;
    bool m_gBufferVSCreated;
    bool m_gBufferPSCreated;
    bool m_deferredPSCreated;
};

} // namespace Render
} // namespace NDDFIX
