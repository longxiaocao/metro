// SMAA SearchTex (预计算纹理数据) - Phase 9.2
//
// 真实数据来源：https://github.com/iryoku/smaa/tree/master/SMAA/Textures
//   - searchTex: 64x33 R（单通道，1 字节/像素）= 2112 字节。
//
// 本头是占位实现：把纹理留空，PostProcess.cpp 在 Initialize() 时检测
// 数组大小为 0 就跳过 SMAA 资源创建，避免崩 + 退化到 Off 模式。
//
// 部署：把 searchTexBytes / searchTexWidth / searchTexHeight 替换为
// 真实数据（从 iryoku/smaa 的 SMAA_Textures.h 取即可）。

#pragma once

#include <cstdint>

// SearchTex: 真实 64x33 R = 2112 字节。
// 占位：1 字节 = 0。
// WHY: 用 static const 而非 inline const（C++17 内联变量）：项目 CMake
//   未显式设 C++17 标准，inline 变量在 C++14 编译报 C7525。
static const uint8_t searchTexBytes[] = { 0 };
static const int searchTexWidth  = 1;
static const int searchTexHeight = 1;
