// SMAA_BlendingWeight.fx - Phase 9.2 SMAA 第二趟：混合权重计算
//
// 设计：
//   - 入口：DX9_SMAABlendingWeightCalculationVS / DX9_SMAABlendingWeightCalculationPS
//   - 输入：edgesTex（第一趟输出）+ areaTex + searchTex（预计算）
//   - 输出：blendTex（存储每个像素的 4 个混合权重）
//
// 依赖 areaTex/searchTex 的 C header 静态数据，搜索/区域采样由硬件 sampler 完成。
// 见 ddfix/shaders/SMAA/AreaTex.h 与 SearchTex.h 注释。

#ifndef SMAA_PIXEL_SIZE
#define SMAA_PIXEL_SIZE float2(1.0 / 1280.0, 1.0 / 720.0)
#endif

#define SMAA_HLSL_3 1
#define SMAA_PRESET_MEDIUM 1
#define SMAA_PREDICATION 0
#define SMAA_REPROJECTION 0
#define SMAA_DIRECTX9_LINEAR_BLEND 0
#define SMAA_ONLY_COMPILE_VS 0
#define SMAA_ONLY_COMPILE_PS 0

#include "SMAA.h"

texture2D edgesTex2D;
texture2D areaTex2D;
texture2D searchTex2D;

sampler2D edgesTex
{
    Texture = <edgesTex2D>;
    AddressU = Clamp; AddressV = Clamp;
    MipFilter = Linear; MinFilter = Linear; MagFilter = Linear;
    SRGBTexture = false;
};

sampler2D areaTex
{
    Texture = <areaTex2D>;
    AddressU = Clamp; AddressV = Clamp; AddressW = Clamp;
    MipFilter = Linear; MinFilter = Linear; MagFilter = Linear;
    SRGBTexture = false;
};

sampler2D searchTex
{
    Texture = <searchTex2D>;
    AddressU = Clamp; AddressV = Clamp; AddressW = Clamp;
    MipFilter = Point; MinFilter = Point; MagFilter = Point;
    SRGBTexture = false;
};

void DX9_SMAABlendingWeightCalculationVS(inout float4 position : POSITION,
                                         inout float2 texcoord : TEXCOORD0,
                                         out float2 pixcoord : TEXCOORD1,
                                         out float4 offset[3] : TEXCOORD2)
{
    SMAABlendingWeightCalculationVS(position, position, texcoord, pixcoord, offset);
}

float4 DX9_SMAABlendingWeightCalculationPS(float4 position : SV_POSITION,
                                           float2 texcoord : TEXCOORD0,
                                           float2 pixcoord : TEXCOORD1,
                                           float4 offset[3] : TEXCOORD2) : COLOR
{
    // subsampleIndices = int4(0, 0, 0, 0) for SMAA 1x
    return SMAABlendingWeightCalculationPS(texcoord, pixcoord, offset, edgesTex, areaTex, searchTex, 0);
}

float4 main_ps(float4 position : SV_POSITION,
               float2 texcoord : TEXCOORD0,
               float2 pixcoord : TEXCOORD1,
               float4 offset[3] : TEXCOORD2) : COLOR
{
    return SMAABlendingWeightCalculationPS(texcoord, pixcoord, offset, edgesTex, areaTex, searchTex, 0);
}
