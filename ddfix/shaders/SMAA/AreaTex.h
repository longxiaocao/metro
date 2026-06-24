// SMAA AreaTex (预计算纹理数据) - Phase 9.2
//
// 真实数据来源：https://github.com/iryoku/smaa/tree/master/SMAA/Textures
//   - areaTex:  160x560 RG (sRGB off)，存储"线段两端点对 + 偏移"对应的面积信息。
//   - searchTex: 64x33 R (sRGB off)，存储"对每行步数 → 期望线段长"查找表。
//
// 原版 SMAA 仓库的 Tools/SMAA_Textures.h / SMAA_Textures_DX9.h 用 C 数组形式
// 内嵌这些纹理。生成脚本：Tools/SMAA_Textures.py (Python + numpy 读 PNG 输出 .h)。
//
// 本头是占位实现：把纹理留空，PostProcess.cpp 在 Initialize() 时检测
// 数组大小为 0 就跳过 SMAA 资源创建，避免崩 + 退化到 Off 模式。
//
// 如果需要"真正能跑出抗锯齿效果"的部署：
//   1) 从 https://github.com/iryoku/smaa 下载 AreaTex.h + SearchTex.h
//   2) 把同名符号 (areaTexBytes / areaTexWidth / areaTexHeight /
//      searchTexBytes / searchTexWidth / searchTexHeight) 替换成真实数据
//   3) 重新编译 ddfix.dll
//
// 占位实现：1x1 RG texture，2 字节 = (0, 0)。运行时 SMAA 行为不正确，
// 但不影响 build / 单元测试 / Off 模式。

#pragma once

#include <cstdint>

// AreaTex: 真实 160x560 RG（双通道，2 字节/像素）= 179200 字节。
// 占位：2 字节 = 0。
// WHY: 用 static const 而非 inline const（C++17 内联变量）：项目 CMake
//   未显式设 C++17 标准，inline 变量在 C++14 编译报 C7525。static const
//   在每个 TU 独立一份（O(200 字节) 内存可接受），避免 ODR 冲突。
static const uint8_t areaTexBytes[] = { 0, 0 };
static const int areaTexWidth  = 1;
static const int areaTexHeight = 1;
