# SMAA 官方仓库

> **引用目的**：为 MeteorBladeEnhancer 抗锯齿提供官方 SMAA 库（MIT/BSD 许可可商用）。
> **官方仓库**：https://github.com/iryoku/smaa
> **官方论文**：https://www.iryoku.com/smaa/
> **许可**：MIT License / 二条款 BSD License（**可商用**）

## 文件清单

| 文件 | 大小 | 用途 |
|---|---|---|
| [SMAA.h](SMAA.h) | 52 KB | SMAA 主头文件（Pixel Shader + 状态设置） |
| [SMAA.fx](SMAA.fx) | 10 KB | SMAA Effect Framework 入口 |
| SearchTex.h | (未单独提供) | 在 SMAA.h 内部引用（如果需要单独 .h 需从官方仓库取） |
| AreaTex.h | (未单独提供) | 在 SMAA.h 内部引用（如果需要单独 .h 需从官方仓库取） |

**注**：6.5.9 闭源补丁包内**未单独提供** SearchTex.h / AreaTex.h；这两个文件被嵌入到 SMAA.h 的二进制数据数组中，或使用了不同的 SMAA 配置。我们的实现将基于 [github.com/iryoku/smaa](https://github.com/iryoku/smaa) 官方仓库的完整文件。

## 许可合规

SMAA 官方仓库使用 **MIT License / 二条款 BSD License** 双重许可：

```
Copyright (c) 2013 Jorge Jimenez (jorge@iryoku.com)
Copyright (c) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)
...

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, ...
```

**结论**：可自由用于商业 / 开源项目，只需保留版权声明。

## 复用来源

6.5.9 闭源补丁包内 `ddfix_assets/shaders/SMAA.h` 和 `SMAA.fx` 是 SMAA 官方**未修改版**（与官方仓库一致）。我们可合法复用这些文件。

## 引用规范

在代码注释中使用：

```hlsl
// SMAA: Enhanced Subpixel Morphological Antialiasing
//   - Author: Jorge Jimenez, Jose I. Echevarria et al.
//   - URL: https://github.com/iryoku/smaa
//   - License: MIT / BSD-2-Clause
//   - Paper: "SMAA: Enhanced Subpixel Morphological Antialiasing" (Eurographics 2012)
//   - Integrated into MeteorBladeEnhancer by [作者], [日期]
```

## 集成方式

```hlsl
#include "SMAA.h"  // 包含主头

// 在 PS 中使用 SMAA：
// 1. SMAAEdgeDetectionPS - 边缘检测
// 2. SMAABlendingWeightCalculationPS - 计算混合权重
// 3. SMAANeighborhoodBlendingPS - 邻域混合
```

## 完整文件获取

如需 SearchTex.h / AreaTex.h 单独文件，从官方仓库克隆：

```bash
git clone https://github.com/iryoku/smaa.git
```

或从 [https://www.iryoku.com/smaa/#downloads](https://www.iryoku.com/smaa/#downloads) 下载预编译 Demo（含完整 .h 文件）。

**最后更新**：2026-06-24
