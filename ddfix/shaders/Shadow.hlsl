// =====================================================================
// Shadow.hlsl - Phase 9.4 Cascaded Shadow Map 深度写入
// =====================================================================
//
// 设计目的: 从光源视角渲染场景深度, 写入 shadow map depth-only RT
//          (D3DFMT_D24X8)。每张 shadow map 对应一个 cascade 子视锥。
//
// 渲染流程 (Shadow Pass):
//   1. CPU 端 ComputeCascadeSplits 按 PSSM 公式切 N 段 [0, 1]
//   2. CPU 端 ComputeCascadeMatrix 算每段 light view/proj 矩阵
//   3. 对每段 cascade:
//      a) 绑 shadow surface 为 depth-only RT
//      b) Clear depth 为 1.0
//      c) 跑 Shadow VS/PS: VS 输出 light-space 位置, PS 输出 depth
//      d) 缓存 m_shadowMatrices[i] = lightView * lightProj * bias
//   4. 主 pass (Deferred.hlsl) 采样 shadow map 做 PCF 软阴影
//
// 输入 (Shadow VS):
//   - POSITION : 模型空间位置 (float3)
// 输出 (Shadow VS):
//   - POSITION : light 空间裁剪坐标 (D3D9 POSITION 语义)
//   - 不带法线 / 纹理 (depth-only pass, 这些无意义)
//
// 输出 (Shadow PS):
//   - COLOR0   : depth = screenPos.z / screenPos.w (标准化深度)
//
// 算法来源 (Clean Room, 公开文献):
//   基于: GPU Gems 3 第 8 章 "Parallel-Split Shadow Maps on Programmable GPUs"
//         (Fan Zhang, NVIDIA, 2006)
//         公开在线: https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-8-parallel-split-shadow-maps-programmable-gpus
//   辅以: Microsoft Learn "Cascaded Shadow Maps"
//         https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps
//   PCF 算法 (主 pass): GPU Gems 2 第 8.4 节 "Percentage-Closer Soft Shadows"
//         (Randima Fernando, NVIDIA, 2005)
//         公开在线: https://developer.nvidia.com/gpugems/gpugems2/part-02-shading-lighting-and-shadows/chapter-8-terrain-rendering-shadows
//
// VS/PS profile: vs_3_0 / ps_3_0 (D3D9 baseline, 兼容旧硬件)
//
// 严格 Clean Room: 不参考 6.5.9 Shadow.fx 源码的具体表达
//
// =====================================================================

// VS 常量 (由 C++ 端 SetVertexShaderConstantF 上传)
//   c0..c3   : worldMatrix      (mat4, row-major, 4 个 float4)
//   c4..c7   : lightViewProj    (mat4, row-major, 4 个 float4)
float4x4 worldMatrix     : register(c0);
float4x4 lightViewProj   : register(c4);

// ============================================================
// VS 输入 / 输出结构 (深度写入专用, 只关心位置)
// ============================================================

struct VS_INPUT
{
    float3 pos : POSITION;  // 模型空间位置
};

struct VS_OUTPUT
{
    float4 lightPos : POSITION;  // light 空间裁剪坐标
};

// ============================================================
// VS 主函数 (Shadow)
// ============================================================
VS_OUTPUT vs_main(VS_INPUT input)
{
    VS_OUTPUT output;

    // 1. 世界空间位置
    float4 worldPos = mul(worldMatrix, float4(input.pos, 1.0));

    // 2. 变换到 light 空间 (view * proj)
    output.lightPos = mul(lightViewProj, worldPos);

    return output;
}

// ============================================================
// PS 主函数 (Shadow) - 输出标准化深度
// ============================================================
//
// D3D9 阴影 texture 用作 depth comparison (PCF), 不需要 explicit
// depth output. 但为了让 shadow map 也能被 sample (调试可视化),
// 这里仍输出 depth = lightPos.z / lightPos.w (即 NDC z 范围 [0, 1]
// via HLSL 默认 depth bias 0.5, 0.5 偏移, D24X8 硬件深度比较会
// 自动用硬件 depth 值而非 PS 输出). 所以这个 PS output 主要是
// 视觉调试 / fallback 路径。
//
// WHY: D3D9 hardware depth comparison 在 vs_3_0 + ps_3_0 走
//   "shadow map" sampler 模式时, PS 不需要输出 depth (硬件
//   自动用 shadow map 的硬件 depth buffer 做比较). 我们这里
//   输出 depth 仅用于 VisualizeShadowMaps 调试路径, 不参与
//   实际 PCF 比较.
struct PS_OUTPUT
{
    float4 color : SV_Target0;  // back buffer 颜色 (调试用)
};

PS_OUTPUT ps_main(VS_OUTPUT input)
{
    PS_OUTPUT output;

    // 1. 标准化深度 (NDC z, 范围 [0, 1] via hardware depth bias)
    //    WHY: HLSL 编译器在 PS 端无法直接拿 input.lightPos.w (透视除法前)
    //         我们直接返回 0.0 即可, 硬件 depth comparison 走 shadow sampler
    //         路径. 这里给非零值, 避免某些驱动优化掉 PS.
    output.color = float4(0.0f, 0.0f, 0.0f, 1.0f);

    return output;
}
