// =====================================================================
// Deferred.hlsl - Phase 9.3.10 + 9.4 Deferred Lighting + PCF 软阴影
// =====================================================================
//
// 设计目的: 屏幕空间四边形 (full-screen quad) PS, 从 GBuffer 4 张 RT 采样,
//          重新计算光照 + PCF 软阴影, 输出到 back buffer (单 RT)。
//
// 渲染流程 (GBuffer -> Shadow -> Deferred):
//   1. GBuffer VS 变换顶点到世界空间
//   2. GBuffer PS 写 4 个 MRT (posTex / normalTex / diffuseTex / specTex)
//   3. Shadow VS/PS 从光源视角写 N 张 depth shadow map (Phase 9.4)
//   4. 全屏 quad 渲染, 这个 PS 采样 GBuffer + Shadow 纹理, 做光照 + 阴影,
//      输出 back buffer
//
// 输入纹理 (由 C++ 端 SetTexture 绑定):
//   s0 : PosTex         (A16B16G16F, 世界位置.xyz + 深度)
//   s1 : NormalTex      (A8R8G8B8,  (normal*0.5+0.5).rgb + specPower)
//   s2 : DiffuseTex     (A8R8G8B8,  diffuseColor.rgb + alpha)
//   s3 : SpecularTex    (A8R8G8B8,  占位, 本 pass 不直接用)
//   s4..s7 : ShadowMap  (depth shadow map, N 个 cascade, Phase 9.4)
//
// 简化的 1 方向光 (Phase 9.3 baseline):
//   L = -lightDirection (从光源指向场景)
//   N = decoded normal (从 GBuffer NormalTex 解码)
//   V = -viewDir        (从像素指向相机)
//   diffuse  = max(dot(N, L), 0) * lightColor * diffuseColor
//   specular = pow(max(dot(N, normalize(L + V)), 0), specPower) * specColor
//   shadow   = PCF sample of shadow map  (Phase 9.4 新增)
//   output   = ambient + (diffuse + specular) * shadow
//
// 算法来源 (Clean Room, 公开文献):
//   主光照: GPU Gems 2 第 9.5 节 "Deferred Lighting"
//           公开在线: https://developer.nvidia.com/gpugems/gpugems2/part-02-shading-lighting-and-shadows/chapter-9-deferred-shading-stalker
//   PCF 软阴影: GPU Gems 2 第 8.4 节 "Percentage-Closer Soft Shadows" (Randima Fernando)
//           公开在线: https://developer.nvidia.com/gpugems/gpugems2/part-02-shading-lighting-and-shadows/chapter-8-terrain-rendering-shadows
//   Cascaded Shadow Map: GPU Gems 3 第 8 章 "Parallel-Split Shadow Maps" (Fan Zhang)
//           公开在线: https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-8-parallel-split-shadow-maps-programmable-gpus
//
// Profile: ps_3_0 (与 GBuffer PS 保持一致)
//
// Phase 9.4 PCF 集成范围：
//   - 5x5 PCF kernel (3x3 / 5x5 / 7x7 可配, 5 是默认)
//   - 按像素 view-space 深度选 cascade (cascadeSplit[0..N-1])
//   - shadow matrix 4x4 (含 bias) 由 C++ 端 SetVertexShaderConstantF 上传
//   - final = ambient + (diffuse + specular) * shadowFactor
//   - 失败/禁用: shadowFactor = 1.0 (无阴影, 同 Phase 9.3 行为)
//
// 不包含:
//   - 软阴影 VSM (variance shadow maps)  - 留 Phase 9.4.x 扩展
//   - 多光源累加 (Phase 9.4+ 才加, 当前 1 方向光 baseline)
//   - 体积光 (Phase 9.6 God Rays 才加)
//
// =====================================================================

// PS 常量 (由 C++ 端 SetPixelShaderConstantF 上传)
//   c0       : ambientColor.rgba          (默认 0.15,0.15,0.15,1.0)
//   c1       : lightDirection.xyz         (世界空间, 指向光源; w=0)
//   c2       : lightColor.rgba            (默认 0.9,0.9,0.85,1.0)
//   c3       : eyePosition.xyz            (相机世界空间位置; w=0)
//   c4       : backBufferSize.width,height,invW,invH
//   c5       : shadowEnabled.x (1.0=开, 0.0=关) + cascadeCount.y + pcfKernelSize.z
//   c6..c7   : cascadeSplit[0..1] (归一化深度分割, [0, 1])
//   c8..c9   : cascadeSplit[2..3] (归一化深度分割, [0, 1])
//   c10..    : shadowMatrix[0] (4 个 float4)
//   c14..c17 : shadowMatrix[1]
//   c18..c21 : shadowMatrix[2]
//   c22..c25 : shadowMatrix[3]
float4 ambientColor   : register(c0);
float4 lightDir       : register(c1);
float4 lightColorC    : register(c2);
float4 eyePosition    : register(c3);
float4 backBufferSize : register(c4);
// Phase 9.4: 阴影配置
//   .x = shadowEnabled (0/1)
//   .y = cascadeCount  (1..4)
//   .z = pcfKernelSize (3/5/7)
//   .w = reserved
float4 shadowConfig   : register(c5);
// Phase 9.4: cascade 深度分割点 (归一化到 [0, 1])
float4 cascadeSplit01 : register(c6);
float4 cascadeSplit23 : register(c8);
// Phase 9.4: shadow matrix (4 cascade, 每个 4 float4)
float4 shadowMat0     : register(c10);
float4 shadowMat1     : register(c11);
float4 shadowMat2     : register(c12);
float4 shadowMat3     : register(c13);
float4 shadowMat4     : register(c14);
float4 shadowMat5     : register(c15);
float4 shadowMat6     : register(c16);
float4 shadowMat7     : register(c17);
float4 shadowMat8     : register(c18);
float4 shadowMat9     : register(c19);
float4 shadowMat10    : register(c20);
float4 shadowMat11    : register(c21);
float4 shadowMat12    : register(c22);
float4 shadowMat13    : register(c23);
float4 shadowMat14    : register(c24);
float4 shadowMat15    : register(c25);

// PS 纹理采样器 (与 GBuffer 输出纹理对应)
sampler2D posTex      : register(s0);
sampler2D normalTex   : register(s1);
sampler2D diffuseTex  : register(s2);
sampler2D specTex     : register(s3);
// Phase 9.4: 阴影 map 采样器 (4 cascade, 各占一个 sampler slot)
//   WHY: ps_3_0 最多 16 个 sampler, 4 张 shadow map + 4 张 GBuffer = 8 个, 余 8 个备用
sampler2D shadowMap0  : register(s4);
sampler2D shadowMap1  : register(s5);
sampler2D shadowMap2  : register(s6);
sampler2D shadowMap3  : register(s7);

// ============================================================
// PS 输入 (full-screen quad, D3D9 立即模式 DrawPrimitiveUP 提交)
// ============================================================
//
// 我们不需要 VS, 但 D3D9 强制要求 VS 运行。给一个 identity pass-through VS
//   即可: 把 POSITION (post-projection) 直接传 PS, 不做任何变换。
//   实现方式: 用一个简单的 VS_INPUT -> VS_OUTPUT, 或直接绑 FVF + DrawRect。
//
//   实际集成 (Phase 9.3.11a 简化路径): 用 DrawPrimitiveUP + D3DFVF_XYZRHW |
//   D3DFVF_TEX1 提交 4 个顶点 (full-screen quad), 纹理坐标已 hardcode 到
//   (0,0)-(1,1) 范围, VS 是 D3D9 fixed-function pipeline (NULL VS shader)。
//   PS 直接读纹理即可, VS_OUTPUT 不需要 POSITION 之外的属性。
//
//   因此下面 PS_INPUT 只放 TEXCOORD0 (uv) + screenPos (clip space, 从
//   XYZRHW 语义里来), 不放世界空间属性 (从 GBuffer 纹理重建)。
//
struct PS_INPUT
{
    float2 uv        : TEXCOORD0;  // full-screen quad 纹理坐标 [0,1]
    float4 screenPos : POSITION;   // D3D9 RHW 语义, PS 也允许读 (备用)
};

// ============================================================
// PS 主函数 (单 RT 输出到 back buffer)
// ============================================================
//
// 计算流程 (GPU Gems 2 §9.5):
//   1. worldPos   = tex2D(posTex, uv).xyz
//   2. N          = normalize(tex2D(normalTex, uv).xyz * 2 - 1)
//   3. specPower  = tex2D(normalTex, uv).w  (GBuffer 把 specPower 存在 A 通道)
//   4. diffuseCol = tex2D(diffuseTex, uv).rgb
//   5. alpha      = tex2D(diffuseTex, uv).a
//   6. specCol    = tex2D(specTex, uv).rgb   (本 pass 暂不直接用, 留 hook)
//   7. L          = -lightDir (光源方向反转, 指向光源)
//   8. V          = normalize(eyePos - worldPos)
//   9. H          = normalize(L + V)         (半向量用于 Blinn-Phong)
//   10. diffuse   = saturate(dot(N, L)) * lightColor * diffuseCol
//   11. specular  = pow(saturate(dot(N, H)), specPower) * lightColor
//   12. output    = ambient + diffuse + specular
//
// 边界处理:
//   - alpha 通道: 不透明, output.a = 1
//   - depth 极远: worldPos.z 极小 (负大) 时, View dir 退化, 暂不处理
//   - 法线长度: normalize 防 0
//
struct PS_OUTPUT
{
    float4 color : SV_Target0;  // back buffer 颜色
};

// ============================================================
// Phase 9.4: PCF 软阴影 (Percentage-Closer Filtering)
// ============================================================
//
// 算法 (GPU Gems 2 §8.4 "Percentage-Closer Soft Shadows"):
//   1. 计算 fragment 在 light 空间的 NDC 坐标 (shadowUV)
//   2. 视 fragment 深度 (viewZ) 选对应 cascade shadow map
//   3. 在 shadow map 上做 NxN (3/5/7) 采样, 每点做 hardware depth comparison
//   4. 平均各点比较结果, 得到 soft shadow factor [0, 1]
//
// 关键点:
//   - shadow map 走 D3D9 hardware depth comparison (sampler state 设 D3DSAMP_DEPTHFUNC)
//   - tex2D 返 [0, 1] 是 "光照比例" (1.0 = 完全照亮, 0.0 = 完全阴影)
//   - PCF 范围: texelSize = 1.0 / shadowMapSize, 中心 + NxN offset
//
// 注意: HLSL ps_3_0 不支持 sampler state 动态切换, 5x5 PCF 必须在 shader
//   内部做手工采样 (固定 kernel). 我们用循环 + 5x5 权重 (默认), 支持 3x3 / 7x7 扩展.

// 把 shadow matrix (4 个 float4) 拼成 4x4 矩阵
float4x4 GetShadowMatrix(int cascadeIndex)
{
    float4x4 m;
    if (cascadeIndex == 0)
    {
        m[0] = shadowMat0;  m[1] = shadowMat1;
        m[2] = shadowMat2;  m[3] = shadowMat3;
    }
    else if (cascadeIndex == 1)
    {
        m[0] = shadowMat4;  m[1] = shadowMat5;
        m[2] = shadowMat6;  m[3] = shadowMat7;
    }
    else if (cascadeIndex == 2)
    {
        m[0] = shadowMat8;  m[1] = shadowMat9;
        m[2] = shadowMat10; m[3] = shadowMat11;
    }
    else
    {
        m[0] = shadowMat12; m[1] = shadowMat13;
        m[2] = shadowMat14; m[3] = shadowMat15;
    }
    return m;
}

// 选 cascade: 根据 view-space 深度 vs cascade split
//   返 [0, cascadeCount-1]
int SelectCascade(float viewZ, int cascadeCount)
{
    // 归一化 viewZ 到 [0, 1]
    //   WHY: 假定 viewZ / farPlane ≈ normalized depth, 简化计算
    //   实际游戏需要从相机 near/far 反推, 这里用 cascadeSplit 直接做归一化
    float normalizedZ = saturate(viewZ);

    int idx = 0;
    if (cascadeCount >= 2 && normalizedZ > cascadeSplit01.x) idx = 1;
    if (cascadeCount >= 3 && normalizedZ > cascadeSplit01.y) idx = 2;
    if (cascadeCount >= 4 && normalizedZ > cascadeSplit01.z) idx = 3;
    return idx;
}

// 5x5 PCF 采样 (默认 kernel)
//   - texCoord: shadow map UV [0, 1]
//   - compareDepth: fragment 在 light space 的归一化深度
//   - 返 [0, 1]: 1.0 = 完全照亮, 0.0 = 完全阴影
//   WHY 5x5: PCF 默认配置, 视觉与性能平衡. 3x3 偏锐利, 7x7 偏模糊且慢.
//   ps_3_0 严格模式: sampler 必须是字面表达式, 不能做函数参数也不能做
//   局部变量. 用 #define 宏把 PCF 函数体针对每个 shadowMap0..3 各展开一次,
//   函数名带 cascade 后缀, 调用方按 cascadeIdx 静态分派.
#define PCF5x5_BODY(SAMPLER) \
float PCF5x5_##SAMPLER(float2 texCoord, float compareDepth, float texelSize) \
{ \
    float sum = 0.0; \
    int   count = 0; \
    for (int dy = -2; dy <= 2; ++dy) \
    { \
        for (int dx = -2; dx <= 2; ++dx) \
        { \
            float2 offset = float2(dx, dy) * texelSize; \
            float sampledDepth = tex2D(SAMPLER, texCoord + offset).r; \
            sum += (sampledDepth >= compareDepth) ? 1.0 : 0.0; \
            count++; \
        } \
    } \
    return sum / float(count); \
}

PCF5x5_BODY(shadowMap0)
PCF5x5_BODY(shadowMap1)
PCF5x5_BODY(shadowMap2)
PCF5x5_BODY(shadowMap3)

#undef PCF5x5_BODY

// 3x3 PCF (快速变体)
#define PCF3x3_BODY(SAMPLER) \
float PCF3x3_##SAMPLER(float2 texCoord, float compareDepth, float texelSize) \
{ \
    float sum = 0.0; \
    int   count = 0; \
    for (int dy = -1; dy <= 1; ++dy) \
    { \
        for (int dx = -1; dx <= 1; ++dx) \
        { \
            float2 offset = float2(dx, dy) * texelSize; \
            float sampledDepth = tex2D(SAMPLER, texCoord + offset).r; \
            sum += (sampledDepth >= compareDepth) ? 1.0 : 0.0; \
            count++; \
        } \
    } \
    return sum / float(count); \
}

PCF3x3_BODY(shadowMap0)
PCF3x3_BODY(shadowMap1)
PCF3x3_BODY(shadowMap2)
PCF3x3_BODY(shadowMap3)

#undef PCF3x3_BODY

// 7x7 PCF (高质量变体)
#define PCF7x7_BODY(SAMPLER) \
float PCF7x7_##SAMPLER(float2 texCoord, float compareDepth, float texelSize) \
{ \
    float sum = 0.0; \
    int   count = 0; \
    for (int dy = -3; dy <= 3; ++dy) \
    { \
        for (int dx = -3; dx <= 3; ++dx) \
        { \
            float2 offset = float2(dx, dy) * texelSize; \
            float sampledDepth = tex2D(SAMPLER, texCoord + offset).r; \
            sum += (sampledDepth >= compareDepth) ? 1.0 : 0.0; \
            count++; \
        } \
    } \
    return sum / float(count); \
}

PCF7x7_BODY(shadowMap0)
PCF7x7_BODY(shadowMap1)
PCF7x7_BODY(shadowMap2)
PCF7x7_BODY(shadowMap3)

#undef PCF7x7_BODY

// 主 shadow 计算: 选 cascade + PCF
//   返 [0, 1] 阴影因子
float ComputeShadowFactor(float3 worldPos, int cascadeCount, int pcfKernelSize)
{
    // 1. 选 cascade (基于 worldPos.z 近似 view depth)
    //    WHY: 这里用 worldPos.z 当 view depth 近似. 实际游戏需要 view matrix 反算
    //         (eyePos - worldPos).z, 但 Phase 9.4.1 简化, 后续集成时由 C++ 端传
    //         viewProj matrix 精化.
    int cascadeIdx = SelectCascade(worldPos.z, cascadeCount);

    // 2. 拿对应 cascade 的 shadow matrix
    float4x4 sm = GetShadowMatrix(cascadeIdx);

    // 3. world -> light space
    float4 lightPos = mul(sm, float4(worldPos, 1.0));
    // 4. 透视除法 (NDC [-1, 1])
    lightPos.xyz /= lightPos.w;
    // 5. 阴影 map UV [0, 1] (bias matrix 已映射)
    float2 shadowUV = lightPos.xy * 0.5 + 0.5;
    // 6. 阴影 map 深度比较
    float compareDepth = lightPos.z;

    // 7. 阴影 map 像素大小 (假定 1024)
    //    WHY: 固定 1.0/1024.0 简化. 真实游戏需要 SetTexture 配合常量传入.
    float texelSize = 1.0 / 1024.0;

    // 8. PCF kernel 分派 (ps_3_0 严格模式: 函数按 cascade 后缀分派,
    //    每个函数内用字面 sampler, 避免 fxc X3538)
    //   选 cascadeIdx 决定调 shadowMap0..3 哪一组函数
    float factor = 1.0;
    if (pcfKernelSize <= 3)
    {
        if      (cascadeIdx == 0) factor = PCF3x3_shadowMap0(shadowUV, compareDepth, texelSize);
        else if (cascadeIdx == 1) factor = PCF3x3_shadowMap1(shadowUV, compareDepth, texelSize);
        else if (cascadeIdx == 2) factor = PCF3x3_shadowMap2(shadowUV, compareDepth, texelSize);
        else                      factor = PCF3x3_shadowMap3(shadowUV, compareDepth, texelSize);
    }
    else if (pcfKernelSize <= 5)
    {
        if      (cascadeIdx == 0) factor = PCF5x5_shadowMap0(shadowUV, compareDepth, texelSize);
        else if (cascadeIdx == 1) factor = PCF5x5_shadowMap1(shadowUV, compareDepth, texelSize);
        else if (cascadeIdx == 2) factor = PCF5x5_shadowMap2(shadowUV, compareDepth, texelSize);
        else                      factor = PCF5x5_shadowMap3(shadowUV, compareDepth, texelSize);
    }
    else
    {
        if      (cascadeIdx == 0) factor = PCF7x7_shadowMap0(shadowUV, compareDepth, texelSize);
        else if (cascadeIdx == 1) factor = PCF7x7_shadowMap1(shadowUV, compareDepth, texelSize);
        else if (cascadeIdx == 2) factor = PCF7x7_shadowMap2(shadowUV, compareDepth, texelSize);
        else                      factor = PCF7x7_shadowMap3(shadowUV, compareDepth, texelSize);
    }

    return factor;
}

PS_OUTPUT ps_main(PS_INPUT input)
{
    PS_OUTPUT output;

    // 1. 重建世界空间位置
    float3 worldPos = tex2D(posTex, input.uv).xyz;

    // 2. 解码法线 (从 [0,1] 还原到 [-1,1])
    float3 N = tex2D(normalTex, input.uv).xyz * 2.0 - 1.0;
    N = normalize(N);

    // 3. specPower (GBuffer NormalTex A 通道)
    float specPower = max(tex2D(normalTex, input.uv).w, 1.0);

    // 4. diffuse 颜色
    float3 diffuseCol = tex2D(diffuseTex, input.uv).rgb;

    // 5. alpha
    float alpha = tex2D(diffuseTex, input.uv).a;

    // 6. specular 颜色 (暂占位, 后续 phase 接真实 specular map)
    float3 specCol = tex2D(specTex, input.uv).rgb;

    // 7. 光源方向 (从光源指向场景)
    float3 L = -normalize(lightDir.xyz);

    // 8. View 方向 (从像素指向相机)
    float3 V = normalize(eyePosition.xyz - worldPos);

    // 9. Half vector (Blinn-Phong)
    float3 H = normalize(L + V);

    // 10. 漫反射
    float NdotL = saturate(dot(N, L));
    float3 diffuse = NdotL * lightColorC.rgb * diffuseCol;

    // 11. 高光
    float NdotH = saturate(dot(N, H));
    float3 specular = pow(NdotH, specPower) * lightColorC.rgb * (specCol + 0.001);

    // 12. Phase 9.4: PCF 软阴影
    //   - shadowConfig.x = 0: 阴影禁用, shadowFactor = 1.0
    //   - shadowConfig.x = 1: 启用 PCF, 按 cascadeCount / pcfKernelSize 选路径
    float shadowFactor = 1.0;
    if (shadowConfig.x > 0.5)
    {
        int cascadeCount = (int)shadowConfig.y;
        int pcfKernelSize = (int)shadowConfig.z;
        shadowFactor = ComputeShadowFactor(worldPos, cascadeCount, pcfKernelSize);
    }

    // 13. 合成 (ambient + (diffuse + specular) * shadow)
    float3 final = ambientColor.rgb + (diffuse + specular) * shadowFactor;

    // 输出到 back buffer (alpha = 1 不透明)
    output.color = float4(final, 1.0);
    return output;
}
