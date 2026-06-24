// GBufferRenderer.cpp - Phase 9.3.8-9.3.11 GBuffer MRT + Deferred Lighting 实现
//
// 初始化流程：
//   1. CheckDeviceCapabilities：检查 D3D9 device 是否支持 4 MRT + A16B16G16F
//   2. CreateGBufferResources：创 4 RT (texture) + 4 surface + 1 depth surface
//   3. EnsureShaders：编译 GBuffer VS / PS / Deferred PS
//   4. 任何步骤失败 → 内部 m_available = false, RenderFrame 退化为 no-op
//
// 渲染流程 (RenderFrame)：
//   1. BindGBufferAsRenderTarget：保存旧 RT/depth, 绑 4 MRT
//   2. ClearRenderTargetView：清 GBuffer 4 RT + depth (黑)
//   3. RenderGBufferPass：跑 GBuffer VS/PS (Phase 9.3.11b 实现细节, 当前占位)
//   4. UnbindGBuffer：恢复旧 RT
//   5. RenderDeferredPass：跑 Deferred PS (全屏 quad, 采样 4 GBuffer)
//
// 失败容错：每步返 HRESULT, RenderFrame 包裹 try-style 清理。
//   任何中间步骤失败 → 调 UnbindGBuffer + 返 E_FAIL, 不影响原始 Execute。

#include "GBufferRenderer.h"
#include "../Common/Logging.h"
#include "../Config/ConfigManager.h"

// Phase 9.3.8: GBuffer VS/PS + Deferred PS 编译产物
//   fxc 生成的 .h 路径: ${CMAKE_CURRENT_BINARY_DIR}/<shader>HLSLC.h (= build/ddfix/)
//   CMake 已把 ddfix binary 目录加到 AdditionalIncludeDirectories,
//     所以这里直接用短名 (不带 ../ 前缀), 与 D3D9Context.cpp 引用 ColorKeyHLSLC.h 风格一致.
//   包含 g_gBufferVSHLSLC[] / g_gBufferPSHLSLC[] / g_gDeferredPSHLSLC[] 数组
//   C++ const 全局数组 default linkage 是 internal, 多 TU include 不会冲突。
#include "GBufferVSHLSLC.h"
#include "GBufferPSHLSLC.h"
#include "DeferredPSHLSLC.h"

#include <d3dx9.h>
#include <cstring>

namespace NDDFIX
{
namespace Render
{

namespace
{

// 全屏 quad 顶点 (D3D9 D3DFVF_XYZRHW | D3DFVF_TEX1)
//   4 个顶点覆盖 viewport 整个区域, 纹理坐标 [0,1] 范围
//   XYZRHW 语义表示"已变换屏幕坐标 + 1/w 倒数", VS 直接透传
struct FullScreenVertex
{
    float x, y, z, rhw;  // POSITION (XYZRHW)
    float u, v;          // TEXCOORD0
};

const FullScreenVertex kFullScreenQuad[4] = {
    { 0.0f,                0.0f,                0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f,                0.0f,                0.0f, 1.0f, 0.0f, 0.0f },  // 占位, RenderDeferredPass 内覆盖
    { 0.0f,                0.0f,                0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f,                0.0f,                0.0f, 1.0f, 0.0f, 0.0f },
};

} // anonymous namespace

GBufferRenderer::GBufferRenderer()
    : m_enabled(true)
    , m_available(false)
    , m_width(0)
    , m_height(0)
    , m_posTex(nullptr)
    , m_normalTex(nullptr)
    , m_diffuseTex(nullptr)
    , m_specTex(nullptr)
    , m_posSurface(nullptr)
    , m_normalSurface(nullptr)
    , m_diffuseSurface(nullptr)
    , m_specSurface(nullptr)
    , m_depthSurface(nullptr)
    , m_oldRenderTarget(nullptr)
    , m_oldDepthStencil(nullptr)
    , m_gBufferVS(nullptr)
    , m_gBufferPS(nullptr)
    , m_deferredPS(nullptr)
    , m_gBufferVSConst(nullptr)
    , m_gBufferPSConst(nullptr)
    , m_deferredPSConst(nullptr)
    , m_gBufferVSCreated(false)
    , m_gBufferPSCreated(false)
    , m_deferredPSCreated(false)
{
}

GBufferRenderer::~GBufferRenderer()
{
    Shutdown();
}

GBufferRenderer* GBufferRenderer::Instance()
{
    static GBufferRenderer inst;
    return &inst;
}

HRESULT GBufferRenderer::CheckDeviceCapabilities(IDirect3DDevice9* device)
{
    if (!device)
    {
        return E_POINTER;
    }

    D3DCAPS9 caps;
    HRESULT hr = device->GetDeviceCaps(&caps);
    if (FAILED(hr))
    {
        return hr;
    }

    // 至少 4 个同时 RT
    if (caps.NumSimultaneousRTs < 4)
    {
        logf("GBufferRenderer: NumSimultaneousRTs=%d (need >= 4), unavailable",
             caps.NumSimultaneousRTs);
        return E_FAIL;
    }

    // 独立 RT bit depth (D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS 允许不同格式)
    if (!(caps.PrimitiveMiscCaps & D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS))
    {
        logf("GBufferRenderer: D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS not set, unavailable");
        return E_FAIL;
    }

    // 检查 A16B16G16R16F RT 支持
    //   WHY: GBufferFormat 实际用的是 D3DFMT_A16B16G16R16F (D3D9 官方定义的
    //   4-channel 64-bit float, 公开 d3d9types.h 存在)
    //   原代码用 D3DFMT_A16B16G16F (3-channel float), 该枚举在 D3D9 中不存在,
    //   编译报 C2065 未声明; 这里同步成与 GBufferFormat 一致的 4 通道版本.
    IDirect3D9* d3d = nullptr;
    device->GetDirect3D(&d3d);
    if (!d3d)
    {
        return E_FAIL;
    }
    D3DDISPLAYMODE displayMode;
    hr = device->GetDisplayMode(0, &displayMode);
    if (FAILED(hr))
    {
        d3d->Release();
        return hr;
    }
    HRESULT hrPos = d3d->CheckDeviceFormat(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, displayMode.Format,
        D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F);
    d3d->Release();
    if (FAILED(hrPos))
    {
        logf("GBufferRenderer: A16B16G16R16F RT not supported, hr=0x%08X, unavailable", hrPos);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT GBufferRenderer::CreateGBufferResources(IDirect3DDevice9* device, int width, int height)
{
    if (!device || width <= 0 || height <= 0)
    {
        return E_INVALIDARG;
    }

    GBufferFormat fmt;

    // RT0: PosTex (A16B16G16F)
    HRESULT hr = device->CreateTexture(
        width, height, 1,
        D3DUSAGE_RENDERTARGET, fmt.posFormat, D3DPOOL_DEFAULT,
        &m_posTex, nullptr);
    if (FAILED(hr))
    {
        logf("GBufferRenderer: CreateTexture posTex failed, hr=0x%08X", hr);
        return hr;
    }

    // RT1: NormalTex (A8R8G8B8)
    hr = device->CreateTexture(
        width, height, 1,
        D3DUSAGE_RENDERTARGET, fmt.normalFormat, D3DPOOL_DEFAULT,
        &m_normalTex, nullptr);
    if (FAILED(hr))
    {
        logf("GBufferRenderer: CreateTexture normalTex failed, hr=0x%08X", hr);
        return hr;
    }

    // RT2: DiffuseTex (A8R8G8B8)
    hr = device->CreateTexture(
        width, height, 1,
        D3DUSAGE_RENDERTARGET, fmt.diffuseFormat, D3DPOOL_DEFAULT,
        &m_diffuseTex, nullptr);
    if (FAILED(hr))
    {
        logf("GBufferRenderer: CreateTexture diffuseTex failed, hr=0x%08X", hr);
        return hr;
    }

    // RT3: SpecularTex (A8R8G8B8)
    hr = device->CreateTexture(
        width, height, 1,
        D3DUSAGE_RENDERTARGET, fmt.specularFormat, D3DPOOL_DEFAULT,
        &m_specTex, nullptr);
    if (FAILED(hr))
    {
        logf("GBufferRenderer: CreateTexture specTex failed, hr=0x%08X", hr);
        return hr;
    }

    // 拿 surface level 0 (绑 MRT 需要 IDirect3DSurface9)
    m_posTex->GetSurfaceLevel(0, &m_posSurface);
    m_normalTex->GetSurfaceLevel(0, &m_normalSurface);
    m_diffuseTex->GetSurfaceLevel(0, &m_diffuseSurface);
    m_specTex->GetSurfaceLevel(0, &m_specSurface);

    // Depth stencil (D24S8)
    hr = device->CreateDepthStencilSurface(
        width, height, fmt.depthFormat, D3DMULTISAMPLE_NONE, 0, FALSE,
        &m_depthSurface, nullptr);
    if (FAILED(hr))
    {
        logf("GBufferRenderer: CreateDepthStencilSurface failed, hr=0x%08X", hr);
        return hr;
    }

    m_width = width;
    m_height = height;
    return S_OK;
}

void GBufferRenderer::ReleaseGBufferResources()
{
    if (m_posSurface)     { m_posSurface->Release();     m_posSurface = nullptr; }
    if (m_normalSurface)  { m_normalSurface->Release();  m_normalSurface = nullptr; }
    if (m_diffuseSurface) { m_diffuseSurface->Release(); m_diffuseSurface = nullptr; }
    if (m_specSurface)    { m_specSurface->Release();    m_specSurface = nullptr; }
    if (m_depthSurface)   { m_depthSurface->Release();   m_depthSurface = nullptr; }
    if (m_posTex)         { m_posTex->Release();         m_posTex = nullptr; }
    if (m_normalTex)      { m_normalTex->Release();      m_normalTex = nullptr; }
    if (m_diffuseTex)     { m_diffuseTex->Release();     m_diffuseTex = nullptr; }
    if (m_specTex)        { m_specTex->Release();        m_specTex = nullptr; }
    m_width = 0;
    m_height = 0;
}

HRESULT GBufferRenderer::EnsureShaders(IDirect3DDevice9* device)
{
    if (!device) return E_POINTER;

    HRESULT hr = S_OK;

    // GBuffer VS
    if (!m_gBufferVSCreated)
    {
        hr = device->CreateVertexShader(
            reinterpret_cast<const DWORD*>(g_gBufferVSHLSLC), &m_gBufferVS);
        if (SUCCEEDED(hr))
        {
            D3DXGetShaderConstantTable(
                reinterpret_cast<const DWORD*>(g_gBufferVSHLSLC), &m_gBufferVSConst);
        }
        else
        {
            logf("GBufferRenderer: CreateVertexShader (GBuffer) failed, hr=0x%08X", hr);
        }
        m_gBufferVSCreated = true;
    }

    // GBuffer PS
    if (!m_gBufferPSCreated && SUCCEEDED(hr))
    {
        hr = device->CreatePixelShader(
            reinterpret_cast<const DWORD*>(g_gBufferPSHLSLC), &m_gBufferPS);
        if (SUCCEEDED(hr))
        {
            D3DXGetShaderConstantTable(
                reinterpret_cast<const DWORD*>(g_gBufferPSHLSLC), &m_gBufferPSConst);
        }
        else
        {
            logf("GBufferRenderer: CreatePixelShader (GBuffer) failed, hr=0x%08X", hr);
        }
        m_gBufferPSCreated = true;
    }

    // Deferred PS
    if (!m_deferredPSCreated && SUCCEEDED(hr))
    {
        hr = device->CreatePixelShader(
            reinterpret_cast<const DWORD*>(g_deferredPSHLSLC), &m_deferredPS);
        if (SUCCEEDED(hr))
        {
            D3DXGetShaderConstantTable(
                reinterpret_cast<const DWORD*>(g_deferredPSHLSLC), &m_deferredPSConst);
        }
        else
        {
            logf("GBufferRenderer: CreatePixelShader (Deferred) failed, hr=0x%08X", hr);
        }
        m_deferredPSCreated = true;
    }

    return hr;
}

HRESULT GBufferRenderer::Initialize(IDirect3DDevice9* device, int width, int height)
{
    if (!device) return E_POINTER;
    if (m_available)
    {
        // 已初始化, 不重复
        return S_OK;
    }

    // 1. 检查 caps
    HRESULT hr = CheckDeviceCapabilities(device);
    if (FAILED(hr))
    {
        logf("GBufferRenderer::Initialize: CheckDeviceCapabilities failed, hr=0x%08X", hr);
        return hr;
    }

    // 2. 创 GBuffer 资源
    hr = CreateGBufferResources(device, width, height);
    if (FAILED(hr))
    {
        logf("GBufferRenderer::Initialize: CreateGBufferResources failed, hr=0x%08X", hr);
        ReleaseGBufferResources();
        return hr;
    }

    // 3. 编译 shader
    hr = EnsureShaders(device);
    if (FAILED(hr))
    {
        logf("GBufferRenderer::Initialize: EnsureShaders failed, hr=0x%08X", hr);
        ReleaseGBufferResources();
        return hr;
    }

    m_available = true;
    logf("GBufferRenderer::Initialize: %dx%d, 4 RT + depth + 3 shader, OK",
         width, height);
    return S_OK;
}

void GBufferRenderer::Shutdown()
{
    if (m_oldRenderTarget)  { m_oldRenderTarget->Release();  m_oldRenderTarget = nullptr; }
    if (m_oldDepthStencil)  { m_oldDepthStencil->Release();  m_oldDepthStencil = nullptr; }
    if (m_gBufferVS)        { m_gBufferVS->Release();        m_gBufferVS = nullptr; }
    if (m_gBufferPS)        { m_gBufferPS->Release();        m_gBufferPS = nullptr; }
    if (m_deferredPS)       { m_deferredPS->Release();       m_deferredPS = nullptr; }
    if (m_gBufferVSConst)   { m_gBufferVSConst->Release();   m_gBufferVSConst = nullptr; }
    if (m_gBufferPSConst)   { m_gBufferPSConst->Release();   m_gBufferPSConst = nullptr; }
    if (m_deferredPSConst)  { m_deferredPSConst->Release();  m_deferredPSConst = nullptr; }
    m_gBufferVSCreated = false;
    m_gBufferPSCreated = false;
    m_deferredPSCreated = false;
    ReleaseGBufferResources();
    m_available = false;
}

HRESULT GBufferRenderer::BindGBufferAsRenderTarget(IDirect3DDevice9* device)
{
    if (!device) return E_POINTER;

    // 1. 保存旧 RT
    HRESULT hr = device->GetRenderTarget(0, &m_oldRenderTarget);
    if (FAILED(hr)) return hr;
    hr = device->GetDepthStencilSurface(&m_oldDepthStencil);
    if (FAILED(hr))
    {
        m_oldRenderTarget->Release();
        m_oldRenderTarget = nullptr;
        return hr;
    }

    // 2. 绑 4 MRT
    hr = device->SetRenderTarget(0, m_posSurface);
    if (FAILED(hr)) { m_oldRenderTarget->Release(); m_oldRenderTarget = nullptr;
                      m_oldDepthStencil->Release(); m_oldDepthStencil = nullptr; return hr; }
    hr = device->SetRenderTarget(1, m_normalSurface);
    if (FAILED(hr)) goto fail;
    hr = device->SetRenderTarget(2, m_diffuseSurface);
    if (FAILED(hr)) goto fail;
    hr = device->SetRenderTarget(3, m_specSurface);
    if (FAILED(hr)) goto fail;

    // 3. 绑 depth
    hr = device->SetDepthStencilSurface(m_depthSurface);
    if (FAILED(hr)) goto fail;

    return S_OK;

fail:
    if (m_oldRenderTarget) { m_oldRenderTarget->Release(); m_oldRenderTarget = nullptr; }
    if (m_oldDepthStencil) { m_oldDepthStencil->Release(); m_oldDepthStencil = nullptr; }
    return hr;
}

HRESULT GBufferRenderer::UnbindGBuffer(IDirect3DDevice9* device)
{
    if (!device) return E_POINTER;

    // 1. 解绑 4 MRT (绑回旧 RT/depth, 或 nullptr)
    HRESULT hr = device->SetRenderTarget(0, m_oldRenderTarget);
    if (SUCCEEDED(hr)) hr = device->SetRenderTarget(1, nullptr);
    if (SUCCEEDED(hr)) hr = device->SetRenderTarget(2, nullptr);
    if (SUCCEEDED(hr)) hr = device->SetRenderTarget(3, nullptr);
    if (SUCCEEDED(hr)) hr = device->SetDepthStencilSurface(m_oldDepthStencil);

    // 2. 释放保存的旧 surface
    if (m_oldRenderTarget) { m_oldRenderTarget->Release(); m_oldRenderTarget = nullptr; }
    if (m_oldDepthStencil) { m_oldDepthStencil->Release(); m_oldDepthStencil = nullptr; }
    return hr;
}

HRESULT GBufferRenderer::RenderGBufferPass(IDirect3DDevice9* device,
                                           const ExtractedGeometry& geometry,
                                           const D3DXMATRIX& viewMatrix,
                                           const D3DXMATRIX& projMatrix)
{
    if (!device) return E_POINTER;

    // Phase 9.3.11a: 占位
    //   真正实现 (9.3.11b) 需要：
    //   1. 从 geometry.vertices + geometry.triangles 创 D3D9 vertex/index buffer
    //   2. 绑 VS input layout (POS/NORMAL/TEX0/COLOR)
    //   3. SetVertexShader(m_gBufferVS) + SetPixelShader(m_gBufferPS)
    //   4. 上传 worldMatrix + viewProjMatrix 到 VS constants
    //   5. 上传 ambientColor / lightDir / lightColor / specPower 到 PS constants
    //   6. SetTexture(s0, diffuseMap)  // 暂用默认白纹理
    //   7. DrawIndexedPrimitive(...)
    //
    // 当前 Phase 9.3.11a: 仅清 4 RT + depth, 真正 draw 留 TODO
    (void)geometry;
    (void)viewMatrix;
    (void)projMatrix;

    // 清 4 GBuffer RT (黑)
    device->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
    // 注：D3DCLEAR_TARGET 同时清所有绑定的 RT (D3D9 标准行为)

    return S_OK;
}

HRESULT GBufferRenderer::RenderDeferredPass(IDirect3DDevice9* device)
{
    if (!device) return E_POINTER;

    // Phase 9.3.11a: 占位
    //   真正实现需要：
    //   1. 关 depth test (全屏 quad, 不需要 z-test)
    //   2. 绑 4 GBuffer 纹理到 PS samplers (s0=pos, s1=normal, s2=diffuse, s3=spec)
    //   3. 上传 PS constants (ambient / lightDir / lightColor / eyePos / backBufferSize)
    //   4. SetPixelShader(m_deferredPS)
    //   5. 提交 4 顶点全屏 quad (DrawPrimitiveUP)
    //   6. 解绑 4 GBuffer 纹理
    //
    // 当前 Phase 9.3.11a: 仅清 back buffer (用深紫标记 deferred 已跑)
    //   颜色: RGB(0.3, 0.0, 0.3, 1.0) → 0xFF800080 (BGRA in D3D)
    device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF800080, 1.0f, 0);

    return S_OK;
}

HRESULT GBufferRenderer::RenderFrame(IDirect3DDevice9* device,
                                     const ExtractedGeometry& geometry,
                                     const D3DXMATRIX& viewMatrix,
                                     const D3DXMATRIX& projMatrix)
{
    if (!m_enabled || !m_available) return E_FAIL;
    if (!device) return E_POINTER;

    // 1. 绑 GBuffer
    HRESULT hr = BindGBufferAsRenderTarget(device);
    if (FAILED(hr))
    {
        logf("GBufferRenderer::RenderFrame: BindGBufferAsRenderTarget failed, hr=0x%08X", hr);
        return hr;
    }

    // 2. 跑 GBuffer VS/PS (9.3.11a 占位)
    hr = RenderGBufferPass(device, geometry, viewMatrix, projMatrix);
    if (FAILED(hr))
    {
        logf("GBufferRenderer::RenderFrame: RenderGBufferPass failed, hr=0x%08X", hr);
        UnbindGBuffer(device);
        return hr;
    }

    // 3. 解绑 GBuffer, 恢复 back buffer
    hr = UnbindGBuffer(device);
    if (FAILED(hr))
    {
        logf("GBufferRenderer::RenderFrame: UnbindGBuffer failed, hr=0x%08X", hr);
        return hr;
    }

    // 4. 跑 Deferred PS (9.3.11a 占位)
    hr = RenderDeferredPass(device);
    if (FAILED(hr))
    {
        logf("GBufferRenderer::RenderFrame: RenderDeferredPass failed, hr=0x%08X", hr);
        return hr;
    }

    return S_OK;
}

} // namespace Render
} // namespace NDDFIX
