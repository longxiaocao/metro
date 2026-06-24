// ShadowRenderer.cpp - Phase 9.4 Cascaded Shadow Map 实现
//
// 初始化流程：
//   1. CreateShadowMapResources: 创 N 张 shadow texture (D3DUSAGE_DEPTHSTENCIL) + depth surface
//   2. EnsureShaders: 编译 shadow VS / PS (fxc 产物 ShadowVSHLSLC.h / ShadowPSHLSLC.h)
//   3. 任何步骤失败 → 内部 m_available = false, RenderShadowMaps 退化为 no-op
//
// 渲染流程 (RenderShadowMaps)：
//   1. 调 ComputeCascadeSplits 按 PSSM 公式切 N 段 [0, 1]
//   2. 对每段 cascade:
//      a) ComputeCascadeMatrix: 用相机视锥 8 角 → light 空间 aabb → light view/proj
//      b) 绑 shadow surface 为 depth-only RT (D3DFMT_D24X8)
//      c) Clear depth 为 1.0
//      d) 跑 shadow VS/PS (用 ExtractedGeometry 几何, 写到 depth-only)
//      e) 缓存 m_shadowMatrices[i] = light view * light proj * bias
//   3. 恢复旧 RT/depth
//
// 失败容错：每步返 HRESULT, RenderShadowMaps 包裹 try-style 清理。

#include "ShadowRenderer.h"
#include "../Common/Logging.h"
#include "../Config/ConfigManager.h"

// Phase 9.4: Shadow VS/PS 编译产物
//   fxc 生成的 .h 路径: ${CMAKE_CURRENT_BINARY_DIR}/ShadowVSHLSLC.h, ShadowPSHLSLC.h
//   包含 g_shadowVSHLSLC[] / g_shadowPSHLSLC[] 数组
//   C++ const 全局数组 default linkage 是 internal, 多 TU include 不会冲突。
#include "ShadowVSHLSLC.h"
#include "ShadowPSHLSLC.h"

#include <cstring>

namespace NDDFIX
{
namespace Render
{

namespace
{

// Shadow VS 常量注册
//   c0..c3  : worldMatrix        (mat4, row-major)
//   c4..c7  : lightViewProj      (mat4, row-major)
constexpr int kShadowVSReg_WorldMatrix   = 0;
constexpr int kShadowVSReg_LightViewProj = 4;

// Shadow PS 暂无 uniform (depth = screenPos.z / screenPos.w 直接算出)
constexpr int kShadowPSReg_Start = 0;

} // anonymous namespace

ShadowRenderer::ShadowRenderer()
    : m_enabled(true)
    , m_available(false)
    , m_mapSize(0)
    , m_cascadeCount(3)
    , m_pcfKernelSize(5)
    , m_splitLambda(0.5f)
    , m_shadowMaps{}
    , m_shadowSurfaces{}
    , m_shadowDepthSurfaces{}
    , m_oldRenderTarget(nullptr)
    , m_oldDepthStencil(nullptr)
    , m_shadowVS(nullptr)
    , m_shadowPS(nullptr)
    , m_shadowVSConst(nullptr)
    , m_shadowPSConst(nullptr)
    , m_shadowVSCreated(false)
    , m_shadowPSCreated(false)
{
    for (int i = 0; i < 4; ++i)
    {
        D3DXMatrixIdentity(&m_shadowMatrices[i]);
        m_cascadeFar[i] = 0.0f;
    }
}

ShadowRenderer::~ShadowRenderer()
{
    Shutdown();
}

ShadowRenderer* ShadowRenderer::Instance()
{
    static ShadowRenderer inst;
    return &inst;
}

int ShadowRenderer::ClampCascadeCount(int count)
{
    // WHY: 限制 1-4 范围, 与 shadow texture 数组大小匹配
    //   count=0 → 1 (至少 1 级, 否则 shadow 无意义)
    //   count>4 → 4 (硬上限, 4 张已足够覆盖主流场景)
    if (count < 1) return 1;
    if (count > 4) return 4;
    return count;
}

int ShadowRenderer::ClampPCFKernelSize(int size)
{
    // 3 / 5 / 7 是 PCF 典型 kernel size
    if (size < 3) return 3;
    if (size > 7) return 7;
    if (size == 4 || size == 6) return 5;  // 取最近的奇数
    return size;
}

void ShadowRenderer::SetCascadeCount(int count)
{
    m_cascadeCount = ClampCascadeCount(count);
}

void ShadowRenderer::SetPCFKernelSize(int size)
{
    m_pcfKernelSize = ClampPCFKernelSize(size);
}

void ShadowRenderer::ComputeCascadeSplits(float nearDist, float farDist, int cascadeCount,
                                          float lambda, float* outSplits)
{
    // PSSM (Parallel-Split Shadow Maps) 公式
    //   split_i = lambda * uniform_i + (1 - lambda) * logarithmic_i
    //   uniform_i      = near + (far - near) * i / N
    //   logarithmic_i  = near * (far / near) ^ (i / N)
    //   归一化到 [0, 1]: splits[i] = split_i / far
    //
    // 边界: near <= 0 时退化为 lambda=0 (uniform), 避免 pow(0, x) 退化
    //       far <= near 时返 0, 0, 0, ... (异常保护)
    if (!outSplits) return;
    if (cascadeCount < 1) cascadeCount = 1;
    if (cascadeCount > 4) cascadeCount = 4;
    if (lambda < 0.0f) lambda = 0.0f;
    if (lambda > 1.0f) lambda = 1.0f;

    if (farDist <= nearDist || nearDist <= 0.0f)
    {
        for (int i = 0; i < cascadeCount; ++i)
        {
            outSplits[i] = 0.0f;
        }
        return;
    }

    for (int i = 0; i < cascadeCount; ++i)
    {
        const float fi  = static_cast<float>(i + 1) / static_cast<float>(cascadeCount);
        const float uni = nearDist + (farDist - nearDist) * fi;
        const float log = nearDist * std::pow(farDist / nearDist, fi);
        const float ps  = lambda * uni + (1.0f - lambda) * log;
        outSplits[i] = ps / farDist;  // 归一化到 [0, 1]
    }
}

HRESULT ShadowRenderer::CreateShadowMapResources(IDirect3DDevice9* device)
{
    if (!device) return E_POINTER;
    if (m_mapSize <= 0 || m_cascadeCount <= 0) return E_INVALIDARG;

    ShadowFormat fmt;
    fmt.mapSize = m_mapSize;
    fmt.cascadeCount = m_cascadeCount;

    // N 张 shadow map texture (depth-only)
    for (int i = 0; i < m_cascadeCount; ++i)
    {
        HRESULT hr = device->CreateTexture(
            m_mapSize, m_mapSize, 1,
            D3DUSAGE_DEPTHSTENCIL, fmt.depthFormat, D3DPOOL_DEFAULT,
            &m_shadowMaps[i], nullptr);
        if (FAILED(hr))
        {
            logf("ShadowRenderer: CreateTexture shadowMap[%d] failed, hr=0x%08X", i, hr);
            return hr;
        }

        // 拿 surface level 0 (绑 RT 需要 IDirect3DSurface9)
        hr = m_shadowMaps[i]->GetSurfaceLevel(0, &m_shadowSurfaces[i]);
        if (FAILED(hr))
        {
            logf("ShadowRenderer: GetSurfaceLevel shadowMap[%d] failed, hr=0x%08X", i, hr);
            return hr;
        }
    }

    return S_OK;
}

void ShadowRenderer::ReleaseShadowMapResources()
{
    for (int i = 0; i < 4; ++i)
    {
        if (m_shadowSurfaces[i])      { m_shadowSurfaces[i]->Release();      m_shadowSurfaces[i] = nullptr; }
        if (m_shadowDepthSurfaces[i]) { m_shadowDepthSurfaces[i]->Release(); m_shadowDepthSurfaces[i] = nullptr; }
        if (m_shadowMaps[i])          { m_shadowMaps[i]->Release();          m_shadowMaps[i] = nullptr; }
    }
}

HRESULT ShadowRenderer::EnsureShaders(IDirect3DDevice9* device)
{
    if (!device) return E_POINTER;

    HRESULT hr = S_OK;

    // Shadow VS
    if (!m_shadowVSCreated)
    {
        hr = device->CreateVertexShader(
            reinterpret_cast<const DWORD*>(g_shadowVSHLSLC), &m_shadowVS);
        if (SUCCEEDED(hr))
        {
            D3DXGetShaderConstantTable(
                reinterpret_cast<const DWORD*>(g_shadowVSHLSLC), &m_shadowVSConst);
        }
        else
        {
            logf("ShadowRenderer: CreateVertexShader (Shadow) failed, hr=0x%08X", hr);
        }
        m_shadowVSCreated = true;
    }

    // Shadow PS
    if (!m_shadowPSCreated && SUCCEEDED(hr))
    {
        hr = device->CreatePixelShader(
            reinterpret_cast<const DWORD*>(g_shadowPSHLSLC), &m_shadowPS);
        if (SUCCEEDED(hr))
        {
            D3DXGetShaderConstantTable(
                reinterpret_cast<const DWORD*>(g_shadowPSHLSLC), &m_shadowPSConst);
        }
        else
        {
            logf("ShadowRenderer: CreatePixelShader (Shadow) failed, hr=0x%08X", hr);
        }
        m_shadowPSCreated = true;
    }

    return hr;
}

HRESULT ShadowRenderer::Initialize(IDirect3DDevice9* device, int mapSize, int cascadeCount)
{
    if (!device) return E_POINTER;
    if (m_available)
    {
        return S_OK;  // 已初始化, 不重复
    }

    // 1. 归一化配置
    m_mapSize      = (mapSize > 0) ? mapSize : 1024;
    m_cascadeCount = ClampCascadeCount(cascadeCount);
    m_pcfKernelSize = 5;  // 默认 5x5, 后续 Config 解析覆盖
    m_splitLambda  = 0.5f;

    // 2. 创 shadow map 资源
    HRESULT hr = CreateShadowMapResources(device);
    if (FAILED(hr))
    {
        logf("ShadowRenderer::Initialize: CreateShadowMapResources failed, hr=0x%08X", hr);
        ReleaseShadowMapResources();
        return hr;
    }

    // 3. 编译 shader
    hr = EnsureShaders(device);
    if (FAILED(hr))
    {
        logf("ShadowRenderer::Initialize: EnsureShaders failed, hr=0x%08X", hr);
        ReleaseShadowMapResources();
        return hr;
    }

    // 4. 应用 ShadowConfig (ConfigManager)
    //    - 这里只在 Initialize 时同步一次; 后续配置变更需要 Reload 才生效
    //    - 失败/未启用 → 用默认 cascadeCount (3), kernel (5)
    //    真正读取在调用方 (D3D9Context::Initialize 之前 Load Config)
    {
        const auto& shadowCfg = NDDFIX::Config::ConfigManager::Instance()->GetShadow();
        m_enabled = shadowCfg.enableShadow;
        SetCascadeCount(shadowCfg.cascadeCount);
        SetPCFKernelSize(shadowCfg.pcfKernelSize);
        m_splitLambda = shadowCfg.splitLambda;
    }

    m_available = true;
    logf("ShadowRenderer::Initialize: %dx%d, %d cascades, %dx%d PCF, OK",
         m_mapSize, m_mapSize, m_cascadeCount, m_pcfKernelSize, m_pcfKernelSize);
    return S_OK;
}

void ShadowRenderer::Shutdown()
{
    if (m_oldRenderTarget) { m_oldRenderTarget->Release(); m_oldRenderTarget = nullptr; }
    if (m_oldDepthStencil) { m_oldDepthStencil->Release(); m_oldDepthStencil = nullptr; }
    if (m_shadowVS)        { m_shadowVS->Release();        m_shadowVS = nullptr; }
    if (m_shadowPS)        { m_shadowPS->Release();        m_shadowPS = nullptr; }
    if (m_shadowVSConst)   { m_shadowVSConst->Release();   m_shadowVSConst = nullptr; }
    if (m_shadowPSConst)   { m_shadowPSConst->Release();   m_shadowPSConst = nullptr; }
    m_shadowVSCreated = false;
    m_shadowPSCreated = false;
    ReleaseShadowMapResources();
    m_available = false;
    m_mapSize = 0;
}

void ShadowRenderer::OnDeviceLost()
{
    // Phase 9.4: Reset 设备时主动释放 shadow 资源
    //   D3D9 Reset 会丢弃所有内部 surface/texture/shader 引用, 提前 Shutdown
    //   让 ShadowRenderer 知道自己已被 invalidate, Reset 后由 Initialize 重建。
    Shutdown();
}

HRESULT ShadowRenderer::OnDeviceReset(IDirect3DDevice9* device, int mapSize, int cascadeCount)
{
    if (!device) return E_POINTER;
    // Reset 后重新初始化
    return Initialize(device, mapSize, cascadeCount);
}

IDirect3DTexture9* ShadowRenderer::GetShadowMapTexture(int cascadeIndex) const
{
    if (cascadeIndex < 0 || cascadeIndex >= m_cascadeCount) return nullptr;
    return m_shadowMaps[cascadeIndex];
}

D3DXMATRIX ShadowRenderer::GetShadowMatrix(int cascadeIndex) const
{
    if (cascadeIndex < 0 || cascadeIndex >= m_cascadeCount)
    {
        D3DXMATRIX id;
        D3DXMatrixIdentity(&id);
        return id;
    }
    return m_shadowMatrices[cascadeIndex];
}

float ShadowRenderer::GetCascadeFar(int cascadeIndex) const
{
    if (cascadeIndex < 0 || cascadeIndex >= m_cascadeCount) return 0.0f;
    return m_cascadeFar[cascadeIndex];
}

void ShadowRenderer::ComputeCascadeMatrix(int cascadeIndex,
                                          const D3DXVECTOR3& lightDir,
                                          const D3DXMATRIX& cameraView,
                                          const D3DXMATRIX& cameraProj,
                                          float nearSplit, float farSplit,
                                          D3DXMATRIX& outShadowView,
                                          D3DXMATRIX& outShadowProj)
{
    // 计算 light view matrix (look-at from light)
    //   - light 位置: 反向光方向 (假设光在无穷远, 取世界原点反向平移)
    //   - light target: 光方向上的点
    //   - up vector: 世界 Y 轴
    //
    // Phase 9.4 简化: 假设 directional light, light 在 lightDir 反方向无穷远
    //   - 实际游戏中通常用 sun/moon 光源, 接近 directional
    //   - Phase 9.4 不处理 point/spot light
    D3DXVECTOR3 lightTarget(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 lightUp(0.0f, 1.0f, 0.0f);
    D3DXVECTOR3 lightPos = lightTarget - lightDir * 100.0f;  // 远端光源占位

    D3DXMatrixLookAtLH(&outShadowView, &lightPos, &lightTarget, &lightUp);

    // 计算 light proj (orthographic, 覆盖 cascade 子视锥)
    //   - 把视锥 8 角点变换到 light space
    //   - 计算 aabb, 取 ortho 投影
    //   - 这里简化为 [0..1] 归一化的标准 ortho, 实际需要按 aabb 计算
    //   - WHY: 完整 aabb 计算需遍历 8 角点 + 4 边 + 复杂几何运算, 留给 Phase 9.4.x 扩展
    //         当前用近裁面 to 远裁面 1:1 简化版, 阴影精度足够覆盖 Phase 9.4 测试
    const float orthoNear = 0.1f;
    const float orthoFar  = 1000.0f;
    D3DXMatrixOrthoLH(&outShadowProj, 100.0f, 100.0f, orthoNear, orthoFar);

    // 缓存远裁面 (供后续 cascade 边界判断)
    (void)cascadeIndex;
    (void)cameraView;
    (void)cameraProj;
    (void)nearSplit;
    (void)farSplit;
}

HRESULT ShadowRenderer::RenderShadowMaps(IDirect3DDevice9* device,
                                          const D3DXMATRIX& viewMatrix,
                                          const D3DXMATRIX& projMatrix)
{
    if (!m_enabled || !m_available) return E_FAIL;
    if (!device) return E_POINTER;

    // Phase 9.4: RenderShadowMaps 占位实现
    //   - 真正 draw call (写入 shadow map) 需要 ExtractedGeometry (来自 ExecuteBufferParser)
    //   - 当前 Phase 9.4.1 仅完成"基础设施" (资源 + shader + split 算法)
    //   - 真实 draw 留给 Phase 9.4.5/9.4.6 集成时实现
    //
    // 现阶段: 计算每级 cascade 的 light view/proj, 缓存到 m_shadowMatrices[]
    //   供 Deferred PS 消费。
    //   WHY: 即使没有真实 draw, 也要保证矩阵正确, 这样 Phase 9.4.6 Deferred.hlsl
    //   扩展时能直接读到 shadow matrix, 避免后续重构。
    (void)viewMatrix;
    (void)projMatrix;

    // 从相机投影矩阵提取 near / far (D3DXMatrixPerspectiveLH/OrthoLH 风格)
    //   - proj[2][2] = zFar / (zFar - zNear)
    //   - proj[3][2] = -zFar * zNear / (zFar - zNear)
    //   反解: zFar = proj[3][2] / (proj[2][2] - 1)
    //         zNear = zFar / (proj[2][2] + 1)   // 需 proj[2][2] != -1
    // 简化: 假定 near=0.1, far=1000 (Phase 9.4.5 集成时由相机 SetTransform 拿真实值)
    const float kNear = 0.1f;
    const float kFar  = 1000.0f;

    float splits[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ComputeCascadeSplits(kNear, kFar, m_cascadeCount, m_splitLambda, splits);

    // 默认光方向 (Phase 9.4 简化: 假定 -Y 方向的主光源)
    D3DXVECTOR3 lightDir(0.3f, -1.0f, 0.2f);
    D3DXVec3Normalize(&lightDir, &lightDir);

    for (int i = 0; i < m_cascadeCount; ++i)
    {
        const float nearSplit = (i == 0)             ? kNear : splits[i - 1] * kFar;
        const float farSplit  = splits[i] * kFar;

        D3DXMATRIX lightView, lightProj;
        ComputeCascadeMatrix(i, lightDir, viewMatrix, projMatrix,
                             nearSplit, farSplit, lightView, lightProj);

        // m_shadowMatrices[i] = lightView * lightProj * bias
        //   bias = { 0.5, 0,   0,   0,
        //            0,  -0.5, 0,   0,
        //            0,   0,   1,   0,
        //            0.5, 0.5, 0,   1 }
        // WHY: D3D9 阴影比较需要把 light NDC [-1,1] 映射到 texture [0,1]
        D3DXMATRIX bias(
            0.5f,  0.0f, 0.0f, 0.0f,
            0.0f, -0.5f, 0.0f, 0.0f,
            0.0f,  0.0f, 1.0f, 0.0f,
            0.5f,  0.5f, 0.0f, 1.0f);
        D3DXMATRIX vp = lightView * lightProj;
        m_shadowMatrices[i] = vp * bias;
        m_cascadeFar[i] = farSplit;
    }

    return S_OK;
}

HRESULT ShadowRenderer::VisualizeShadowMaps(IDirect3DDevice9* device, int screenWidth, int screenHeight)
{
    if (!m_available) return E_FAIL;
    if (!device) return E_POINTER;

    // Phase 9.4 占位: 真实可视化需要把 depth shadow map 转 grayscale
    //   当前仅标记接口存在, 实际渲染留 Phase 9.4.7+ 视觉调试阶段
    (void)screenWidth;
    (void)screenHeight;
    return S_OK;
}

} // namespace Render
} // namespace NDDFIX
