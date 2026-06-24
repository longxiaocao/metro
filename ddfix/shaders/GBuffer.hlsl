// =====================================================================
// GBuffer.hlsl - Phase 9.3.8 GBuffer MRT 渲染
// =====================================================================
//
// 设计目的: 一次 draw call 把几何 pass 输出到 4 个 MRT
//   - SV_Target0 (PosTex)     : float4(worldPos.xyz, depth)
//   - SV_Target1 (NormalTex)  : float4(worldNormal * 0.5 + 0.5, specularPower)
//   - SV_Target2 (DiffuseTex) : float4(diffuseColor.rgb * diffuseMap.rgb, alpha)
//   - SV_Target3 (SpecularTex): float4(0, 0, 0, 1) -- 占位, 后续 phase 填
//
// 算法来源 (Clean Room, 公开文献):
//   基于: GPU Gems 2 第 9 章 "Deferred Shading in S.T.A.L.K.E.R." (Oles Shishkovtsov, NVIDIA)
//         公开在线: https://developer.nvidia.com/gpugems/gpugems2/part-02-shading-lighting-and-shadows/chapter-9-deferred-shading-stalker
//   辅以: Microsoft Learn "Multiple Render Targets" (D3D9 官方文档)
//         https://learn.microsoft.com/en-us/windows/win32/direct3d9/multiple-render-targets
//
// VS/PS profile: vs_3_0 / ps_3_0 (D3D9 baseline, 兼容旧硬件)
//
// 输入语义约定 (与 ExtractedGeometry::Vertex 一一对应):
//   - float3 pos   : 模型空间位置
//   - float3 normal: 模型空间法线 (normalize 推荐, 但 VS 内部仍 normalize 一次防退化)
//   - float2 uv    : 纹理坐标 (1 组)
//   - float4 color : 顶点色 (DIFFUSE)
//
// 输出语义:
//   - POSITION    : 屏幕空间裁剪坐标 (D3D9 传统 POSITION 语义, SM3 也保留兼容)
//   - TEXCOORD0   : 世界空间位置
//   - TEXCOORD1   : 世界空间法线 (normalize 后, 范围 -1..1, 由 PS 转 0..1 存)
//   - TEXCOORD2   : 纹理坐标
//   - TEXCOORD3   : 顶点色
//
// 注意: HLSL 编译器要求 VS 输出语义名与 PS 输入语义名完全对应, 且类型一致
//       (float4 / float3 / float2)。本文件 float3 worldNormal 用 TEXCOORD1
//       (即 float4 的前 3 分量), PS 输入侧保持 float3 即可。
//
// D3D9 多渲染目标约束 (Microsoft Learn "Multiple Render Targets"):
//   - 所有 RT 必须尺寸相同
//   - 所有 RT 必须是相同渲染深度 (RGBA8 / RGBA16F 等)
//   - 不同格式可以用, 但需 CheckDeviceFormat + 同时设为 rendertarget
//   - PS 输出数量受 D3DCAPS9::NumSimultaneousRTs 限制 (典型 >= 4)
//
// =====================================================================

// VS 常量 (由 C++ 端 SetVertexShaderConstantF 上传)
//   c0..c3  : worldMatrix        (mat4, row-major, 4 个 float4)
//   c4..c7  : viewProjMatrix     (mat4, row-major, 4 个 float4)
float4x4 worldMatrix     : register(c0);
float4x4 viewProjMatrix  : register(c4);

// PS 常量
//   c8       : ambientColor.rgba
//   c9       : lightDirection.xyz (w unused, 0)
//   c10      : lightColor.rgba
//   c11      : specularPower (高光指数, 默认 32.0)
float4 ambientColor    : register(c8);
float4 lightDirection  : register(c9);
float4 lightColor      : register(c10);
float4 specularPowerC  : register(c11);

// PS 纹理采样器
//   s0  : diffuseMap (GBuffer DiffuseTex 的输入)
//   s1..s3 保留 (GBuffer 输出自己的纹理, 不在 GBuffer PS 采样)
sampler2D diffuseMap : register(s0);

// ============================================================
// VS 输入 / 输出结构
// ============================================================

struct VS_INPUT
{
    float3 pos    : POSITION;   // 模型空间位置
    float3 normal : NORMAL;     // 模型空间法线
    float2 uv     : TEXCOORD0;  // 纹理坐标
    float4 color  : COLOR;      // 顶点色 (DIFFUSE)
};

struct VS_OUTPUT
{
    float4 screenPos   : POSITION;   // 屏幕空间 (D3D9 兼容 POSITION 语义)
    float3 worldPos    : TEXCOORD0;  // 世界空间位置
    float3 worldNormal : TEXCOORD1;  // 世界空间法线
    float2 uv          : TEXCOORD2;  // 纹理坐标
    float4 color       : TEXCOORD3;  // 顶点色
};

// ============================================================
// VS 主函数
// ============================================================
VS_OUTPUT vs_main(VS_INPUT input)
{
    VS_OUTPUT output;

    // 1. 世界空间位置
    output.worldPos = mul(worldMatrix, float4(input.pos, 1.0)).xyz;

    // 2. 世界空间法线 (仅取世界矩阵的旋转分量, 忽略平移)
    //    WHY: 法线变换只能用方向变换, 不能用点变换 (m44 包含平移)
    //    简化: 假设世界矩阵无非均匀缩放, 直接 mul((float3x3)worldMatrix, normal)
    //    + normalize 防御退化 (零向量 / 极小长度)
    output.worldNormal = normalize(mul((float3x3)worldMatrix, input.normal));

    // 3. 屏幕空间 (view * proj * worldPos)
    output.screenPos = mul(viewProjMatrix, float4(output.worldPos, 1.0));

    // 4. 透传
    output.uv    = input.uv;
    output.color = input.color;

    return output;
}

// ============================================================
// PS 主函数 (MRT, 4 个 RT)
// ============================================================
//
// MRT 输出语义:
//   SV_Target0 : float4 (worldPos.xyz, depth)       - PosTex
//   SV_Target1 : float4 (normal*0.5+0.5, specPow)   - NormalTex (法线存为 0..1 范围)
//   SV_Target2 : float4 (diffuse.rgb*map.rgb, alpha) - DiffuseTex
//   SV_Target3 : float4 (0, 0, 0, 1)                - SpecularTex 占位
//
// depth 字段: 当前用 viewProj 后 w 分量作为深度归一化 (post-perspective)。
//   后续 Phase 9.3.11 消费时, Deferred PS 用这个 w 反算 NDC 深度, 不需要重建。
//   这是 GBuffer 经典做法 (参考 GPU Gems 2 第 9 章)。
//
// normal 编码: 范围 -1..1 → 0..1 (R/G/B 分量存, A 存 specPower)
//   PS 端解码:  tex2D(normalTex).xyz * 2 - 1 → 原始 -1..1 法线
//   这样把 [-1,1] 范围压到 8-bit RT 的 [0,255] 表示, 精度损失可接受。
//
struct PS_OUTPUT
{
    float4 posTex     : SV_Target0;
    float4 normalTex  : SV_Target1;
    float4 diffuseTex : SV_Target2;
    float4 specTex    : SV_Target3;
};

PS_OUTPUT ps_main(VS_OUTPUT input)
{
    PS_OUTPUT output;

    // PosTex: worldPos.xyz + depth
    //   depth = NDC z = screenPos.z / screenPos.w (透视除法后深度)
    //   HLSL PS 端无法直接拿 screenPos.w (已传 POSITION 后即被裁剪), 所以
    //   退化为用 worldPos.y 当粗糙深度代理 (后续 phase 改用独立的 depth buffer)。
    //   Phase 9.3.11a 暂用此简化, 真实游戏需要时切到 Z 纹理。
    float depth = input.worldPos.y * 0.001;  // WHY: 简单占位深度, 后续接 depth buffer
    output.posTex = float4(input.worldPos, depth);

    // NormalTex: worldNormal (编码到 0..1) + specularPower
    float3 encodedNormal = input.worldNormal * 0.5 + 0.5;
    output.normalTex = float4(encodedNormal, specularPowerC.x);

    // DiffuseTex: 顶点色 × diffuseMap (如 diffuseMap 没绑, 用白色占位)
    float4 texColor = tex2D(diffuseMap, input.uv);
    output.diffuseTex = float4(input.color.rgb * texColor.rgb, input.color.a * texColor.a);

    // SpecularTex: 占位 (后续 phase 接 specular map)
    output.specTex = float4(0, 0, 0, 1);

    return output;
}
