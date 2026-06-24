// SMAA_NeighborhoodBlending.fx - Phase 9.2 SMAA 第三趟：邻域混合
//
// 设计：
//   - 入口：DX9_SMAANeighborhoodBlendingVS / DX9_SMAANeighborhoodBlendingPS
//   - 输入：colorTex（原始颜色）+ blendTex（第二趟输出）
//   - 输出：最终抗锯齿后的颜色（写到 outputRT）
//
// 关键：D3D9 文档要求 NeighborhoodBlending 的 colorTex 必须支持 sRGB read
// 才能得到正确的 gamma 空间抗锯齿结果。我们在 PostProcess::Run 期间
// 把 colorTex 的 SamplerState 设成 SRGBTexture = true 的等价物（D3DSAMP_SRGBTEXTURE = TRUE）。

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

texture2D colorTex2D;
texture2D blendTex2D;

sampler2D colorTex
{
    Texture = <colorTex2D>;
    AddressU = Clamp; AddressV = Clamp;
    MipFilter = Point; MinFilter = Linear; MagFilter = Linear;
    SRGBTexture = true;
};

sampler2D blendTex
{
    Texture = <blendTex2D>;
    AddressU = Clamp; AddressV = Clamp;
    MipFilter = Linear; MinFilter = Linear; MagFilter = Linear;
    SRGBTexture = false;
};

void DX9_SMAANeighborhoodBlendingVS(inout float4 position : POSITION,
                                    inout float2 texcoord : TEXCOORD0,
                                    out float4 offset[2] : TEXCOORD1)
{
    SMAANeighborhoodBlendingVS(position, position, texcoord, offset);
}

float4 DX9_SMAANeighborhoodBlendingPS(float4 position : SV_POSITION,
                                      float2 texcoord : TEXCOORD0,
                                      float4 offset[2] : TEXCOORD1) : COLOR
{
    return SMAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);
}

float4 main_ps(float4 position : SV_POSITION,
               float2 texcoord : TEXCOORD0,
               float4 offset[2] : TEXCOORD1) : COLOR
{
    return SMAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);
}
