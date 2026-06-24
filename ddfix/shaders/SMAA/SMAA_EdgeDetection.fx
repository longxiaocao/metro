// SMAA_EdgeDetection.fx - Phase 9.2 SMAA 第一趟：边缘检测（Luma 边缘）
//
// 设计：
//   - 包含 SMAA.h（仅 Luma 边缘检测路径，禁掉 Color/Depth 路径以减少编译量）
//   - 入口：DX9_SMAALumaEdgeDetectionVS / DX9_SMAALumaEdgeDetectionPS
//   - 与原版 SMAA.fx 行为等价（参考 docs/literature/smaa/SMAA.fx LumaEdgeDetection technique）
//   - 单独文件便于 D3DXCompileShaderFromFile("...", "main", "ps_3_0", ...) 调用
//
// SMAA_HLSL_3=1 + SMAA_PREDICATION=0 + SMAA_PREDICATION=0 决定走 Luma 路径
// SMAA_THRESHOLD/MAX_SEARCH_STEPS 等通过 #ifdef SMAA_PRESET_CUSTOM 让运行时选择

// 编译期默认值；运行时由 PostProcess::Run 通过 SetVector 设置 actual。
#ifndef SMAA_PIXEL_SIZE
#define SMAA_PIXEL_SIZE float2(1.0 / 1280.0, 1.0 / 720.0)
#endif

// 强制走 Luma + 简化路径（Medium preset）
#define SMAA_HLSL_3 1
#define SMAA_PRESET_MEDIUM 1
#define SMAA_PREDICATION 0
#define SMAA_REPROJECTION 0
#define SMAA_DIRECTX9_LINEAR_BLEND 0
#define SMAA_ONLY_COMPILE_VS 0
#define SMAA_ONLY_COMPILE_PS 0

#include "SMAA.h"

// DX9 必需的 texture2D + sampler 声明
texture2D colorTex2D;
sampler2D colorTexG
{
    Texture = <colorTex2D>;
    AddressU = Clamp; AddressV = Clamp;
    MipFilter = Linear; MinFilter = Linear; MagFilter = Linear;
    SRGBTexture = false;
};

void DX9_SMAAEdgeDetectionVS(inout float4 position : POSITION,
                             inout float2 texcoord : TEXCOORD0,
                             out float4 offset[3] : TEXCOORD1)
{
    SMAAEdgeDetectionVS(position, position, texcoord, offset);
}

float4 DX9_SMAALumaEdgeDetectionPS(float4 position : SV_POSITION,
                                   float2 texcoord : TEXCOORD0,
                                   float4 offset[3] : TEXCOORD1) : COLOR
{
    return SMAALumaEdgeDetectionPS(texcoord, offset, colorTexG);
}

float4 main_ps(float4 position : SV_POSITION,
               float2 texcoord : TEXCOORD0,
               float4 offset[3] : TEXCOORD1) : COLOR
{
    return SMAALumaEdgeDetectionPS(texcoord, offset, colorTexG);
}
