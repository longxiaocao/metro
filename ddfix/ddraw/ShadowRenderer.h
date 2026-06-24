// ShadowRenderer.h - Phase 9.4 Cascaded Shadow Map + PCF 软阴影
//
// 设计目标：
//   1. 单一职责：管理 3-4 级 cascade shadow map + 编译/缓存 shadow shader
//   2. 单例：dll 范围内一份（与 GBufferRenderer/PostProcess 一致）
//   3. 失败容错：任何步骤失败（创 texture/编译 shader）→ 内部 unavailable，
//      RenderShadowMaps 退化为 no-op，绝不阻断 Execute 路径
//   4. 单元测试友好：cascade split 计算为 static / 纯逻辑函数，可独立测
//
// 集成点 (Phase 9.4.5)：
//   - IDirect3DDevice::Execute hook 在 GBuffer/Deferred 之前调
//     ShadowRenderer::Instance()->RenderShadowMaps(...)
//   - 失败/未启用 → 旁路，原始 Execute 走原路径
//   - 缓存的 m_shadowMatrices[] 供 Deferred PS 消费（按像素深度选 cascade）
//
// 基于：GPU Gems 3 第 8 章 "Parallel-Split Shadow Maps on Programmable GPUs" (Fan Zhang)
//       公开在线：https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-8-parallel-split-shadow-maps-programmable-gpus
//       辅以：GPU Gems 2 第 8.4 节 "Percentage-Closer Soft Shadows" (Randima Fernando)
//       公开在线：https://developer.nvidia.com/gpugems/gpugems2/part-02-shading-lighting-and-shadows/chapter-8-terrain-rendering-shadows
//       严格 Clean Room：不参考 6.5.9 .fx 源码
//
// 视锥分割策略 (PSSM - Parallel-Split Shadow Maps, Zhang 2006)：
//   split_i = lambda * (near + i*(far-near)/N) + (1-lambda) * near * (far/near)^(i/N)
//   - lambda = 0 → uniform split（线性）
//   - lambda = 1 → logarithmic split（按远裁面比例分割）
//   - lambda = 0.5 → 经验最优混合
//
// 风险点 (Phase 9.4 关注)：
//   - D3DUSAGE_DEPTHSTENCIL shadow texture 需要 D3D9 device caps
//   - 老 GPU (DX8 级别) 不支持 3 级 cascade，失败时降级为 1 级或 skip
//   - 失败时降级: 退化为 0 张 shadow map (skip shadow pass) → 原始路径

#pragma once

#ifdef NDDFIX_DEBUG_INCLUDES
#pragma message("ShadowRenderer.h: included, namespace NDDFIX::Render = (diagnostic)")
#endif

#include "../D3D9Context.h"

#include <d3d9.h>
#include <d3dx9.h>

namespace NDDFIX
{
namespace Render
{

// Shadow Map 格式配置 (D3D9 baseline)
struct ShadowFormat
{
    int     mapSize;        // 默认 1024
    int     cascadeCount;   // 默认 3 (1..4 有效)
    int     pcfKernelSize;  // 默认 5 (3, 5, 7 有效)
    D3DFORMAT depthFormat;  // 默认 D3DFMT_D24X8 (depth-only, 无 stencil)

    ShadowFormat()
        : mapSize(1024)
        , cascadeCount(3)
        , pcfKernelSize(5)
        // WHY: 阴影只需要 depth, 用 D24X8 节省 1 字节/像素 (D24S8 多了 stencil)
        //   D24X8 = 24-bit depth + 8-bit "don't care" (实际不可用 stencil)
        //   D3D9 CreateDepthStencilSurface 必须与 CreateTexture 共享相同 format
        , depthFormat(D3DFMT_D24X8)
    {
    }
};

// Phase 9.4: Cascaded Shadow Map 渲染器
class ShadowRenderer
{
public:
    static ShadowRenderer* Instance();

    // 初始化：创 N 张 shadow map (depth texture) + 编译 shadow VS/PS。
    //   - 设备就绪后 (D3D9Context::Initialize 之后) 调一次
    //   - 失败 → 内部 unavailable, RenderShadowMaps 退化为 no-op
    //   - 多次调用安全：先 Shutdown 再 Initialize
    HRESULT Initialize(IDirect3DDevice9* device, int mapSize, int cascadeCount);

    // 释放所有资源；Reset 设备 / 卸载 ddfix 时调
    void Shutdown();

    // 设备丢失 (Phase 9.3 ResetDevice hook)
    void OnDeviceLost();
    HRESULT OnDeviceReset(IDirect3DDevice9* device, int mapSize, int cascadeCount);

    // 视锥分割策略 (静态, 单元测试可独立调)
    //   - splits[0..cascadeCount-1] 填充 [0, 1] 范围的归一化深度分割点
    //   - nearDist / farDist 为视锥的 near / far (view space)
    //   - lambda: 0=linear, 1=log, 0.5=混合
    //   - WHY: PSSM 算法 (Zhang 2006) 显式接受 lambda 参数, 这里是公式直接翻译
    static void ComputeCascadeSplits(float nearDist, float farDist, int cascadeCount,
                                     float lambda, float* outSplits);

    // 从光源视角渲染 N 张 depth shadow map。
    //   - viewMatrix / projMatrix: 当前相机视图/投影 (用于视锥裁剪)
    //   - 内部保存旧 RT/depth, 依次绑每张 shadow map 为 RT, 跑 shadow VS/PS, 恢复
    //   - 失败 (RT 失效 / shader 失效) → 返 E_FAIL, 不影响原始 Execute 路径
    //   - 成功后 m_shadowMatrices[i] 包含 viewProjBias 矩阵, 给 Deferred PS 用
    HRESULT RenderShadowMaps(IDirect3DDevice9* device,
                             const D3DXMATRIX& viewMatrix,
                             const D3DXMATRIX& projMatrix);

    // 调试用: 把 N 张 shadow map 拼到屏幕角落 (深度图可视化为灰度)
    //   - 当前 Phase 9.4 占位: 不实际渲染, 仅标记接口存在
    HRESULT VisualizeShadowMaps(IDirect3DDevice9* device, int screenWidth, int screenHeight);

    // 配置读写
    void SetEnabled(bool b) { m_enabled = b; }
    bool IsEnabled() const  { return m_enabled; }

    void SetCascadeCount(int count);
    int  GetCascadeCount() const { return m_cascadeCount; }

    void SetPCFKernelSize(int size);
    int  GetPCFKernelSize() const { return m_pcfKernelSize; }

    void SetSplitLambda(float lambda) { m_splitLambda = lambda; }
    float GetSplitLambda() const { return m_splitLambda; }

    // 单元测试用: 检查 Initialize 是否成功
    bool IsAvailable() const { return m_available; }

    // 当前 shadow map 尺寸 (供测试 / HUD 验证)
    int GetMapSize() const { return m_mapSize; }

    // 拿 shadow map 资源 (供 Deferred PS 消费)
    //   返 nullptr 表示资源未创建 (Initialize 失败或 cascadeIndex 越界)
    IDirect3DTexture9* GetShadowMapTexture(int cascadeIndex) const;
    D3DXMATRIX GetShadowMatrix(int cascadeIndex) const;

    // 拿单级 cascade 的视锥远裁面 (world space), 供调试 / 可视化
    float GetCascadeFar(int cascadeIndex) const;

private:
    ShadowRenderer();
    ~ShadowRenderer();
    ShadowRenderer(const ShadowRenderer&) = delete;
    ShadowRenderer& operator=(const ShadowRenderer&) = delete;

    // 内部: 创建 N 张 shadow map texture + depth surface
    HRESULT CreateShadowMapResources(IDirect3DDevice9* device);
    void ReleaseShadowMapResources();

    // 内部: 编译 shadow VS / PS (懒加载)
    HRESULT EnsureShaders(IDirect3DDevice9* device);

    // 内部: 计算单级 cascade 的光源 ortho projection
    //   - 用相机视锥的 8 个角点反推 light 空间 aabb
    //   - 输出 light view 矩阵 (look-at) + light proj 矩阵 (ortho)
    void ComputeCascadeMatrix(int cascadeIndex,
                              const D3DXVECTOR3& lightDir,
                              const D3DXMATRIX& cameraView,
                              const D3DXMATRIX& cameraProj,
                              float nearSplit, float farSplit,
                              D3DXMATRIX& outShadowView,
                              D3DXMATRIX& outShadowProj);

    // 内部: 限制 cascade count 在 [1, 4] 范围
    static int ClampCascadeCount(int count);
    // 内部: 限制 PCF kernel size 在 {3, 5, 7}
    static int ClampPCFKernelSize(int size);

    // 状态
    bool m_enabled;
    bool m_available;
    int  m_mapSize;
    int  m_cascadeCount;
    int  m_pcfKernelSize;
    float m_splitLambda;

    // Shadow Map 资源 (裸指针, 由 d3d9 device 管理; Reset 时失效)
    IDirect3DTexture9* m_shadowMaps[4];
    IDirect3DSurface9* m_shadowSurfaces[4];
    IDirect3DSurface9* m_shadowDepthSurfaces[4];

    // 保存/恢复 (RenderShadowMaps 内部配对)
    IDirect3DSurface9* m_oldRenderTarget;
    IDirect3DSurface9* m_oldDepthStencil;

    // Shadow view*proj*bias 矩阵 (N 个 cascade, 给 Deferred PS 消费)
    //   bias = 0.5 * (x, -y, z, 1) + 0.5, 把 NDC [-1,1] 映射到 texture [0,1]
    D3DXMATRIX m_shadowMatrices[4];
    // 每个 cascade 的实际远裁面 (view space, 给 PSSM 公式用)
    float      m_cascadeFar[4];

    // Shader (懒加载, Reset 时失效)
    IDirect3DVertexShader9* m_shadowVS;
    IDirect3DPixelShader9*  m_shadowPS;
    ID3DXConstantTable*     m_shadowVSConst;
    ID3DXConstantTable*     m_shadowPSConst;
    bool m_shadowVSCreated;
    bool m_shadowPSCreated;
};

} // namespace Render
} // namespace NDDFIX
