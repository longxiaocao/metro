# 公开文献引用库目录

> **目的**：为 MeteorBladeEnhancer 高级渲染管线（GBuffer / Deferred / SMAA / Shadow / GodRays / Skinning）的实现提供**合法可参考**的公开学术文献清单。
>
> **合规边界**：所有实现必须基于本目录中的公开文献，**不**参考滴风 6.5.9 闭源 ddfix.dll 的二进制反汇编或 .fx 源代码的具体表达。
>
> **详细引用规范**：[PUBLIC_LITERATURE.md](../../PUBLIC_LITERATURE.md)（根目录）

## 目录结构

```
docs/literature/
├── README.md                       # 本文件（总索引）
├── gap-analysis-6.5.9.md          # 6.5.9 差距分析
│
├── gpu-gems-1/                     # GPU Gems 1 关键章节
│   ├── README.md
│   ├── ch09-deferred-shading.*     # 第 9 章 Deferred Shading
│   └── ch11-shadow-map-antialiasing.*  # 第 11 章
│
├── gpu-gems-2/                     # GPU Gems 2 关键章节
│   ├── README.md
│   ├── ch08-4-pcf.*                # 第 8.4 节 Percentage-Closer Soft Shadows
│   ├── ch09-stalker-deferred.*     # 第 9 章 STALKER Deferred Shading
│   ├── ch09-5-deferred-lighting.*  # 第 9.5 节 Deferred Lighting
│   └── ch12-3-height-fog.*         # 第 12.3 节 Height-Based Fog
│
├── gpu-gems-3/                     # GPU Gems 3 关键章节
│   ├── README.md
│   ├── ch08-parallel-split-shadows.*  # 第 8 章 Parallel-Split Shadow Maps
│   ├── ch13-volumetric-light.*     # 第 13 章 Volumetric Light Scattering
│   └── ch19-tabula-rasa-deferred.*  # 第 19 章 Tabula Rasa Deferred
│
├── smaa/                           # SMAA 官方仓库（MIT/BSD 许可）
│   ├── README.md
│   ├── SMAA.h
│   ├── SMAA.fx
│   ├── SearchTex.h
│   └── AreaTex.h
│
├── cascade-shadow-maps/            # Cascaded Shadow Maps MSDN 文章
│   ├── README.md
│   └── cascaded-shadow-maps.html   # MSDN 离线版
│
└── wine-d3d9/                      # Wine d3d9 公开实现（ExecuteBuffer 参考）
    ├── README.md
    └── d3d9_executebuf.c           # Wine 公开源码
```

## 快速引用

| 实现需求 | 引用文献 | 许可 |
|---|---|---|
| **GBuffer / MRT** | GPU Gems 2 第 9 章 / Direct3D 9 SDK Sample | NVIDIA 免费 / Microsoft 免费 |
| **Deferred Lighting** | GPU Gems 2 第 9.5 节 | NVIDIA 免费 |
| **Cascaded Shadow Maps** | MSDN 官方文章 / GPU Gems 3 第 8 章 | Microsoft/NVIDIA 免费 |
| **PCF 软阴影** | GPU Gems 2 第 8.4 节 | NVIDIA 免费 |
| **SMAA 抗锯齿** | SMAA 官方仓库 | **MIT/BSD（可商用）** |
| **God Rays** | GPU Gems 3 第 13 章 | NVIDIA 免费 |
| **硬件蒙皮** | GPU Gems 第 4.6 节 / DirectX SDK Skinning Sample | NVIDIA / Microsoft 免费 |
| **顶点动画 GPU 化** | GPU Gems 1 第 7.4 节 | NVIDIA 免费 |
| **ExecuteBuffer 解析** | Wine d3d9 项目（公开） | Wine 项目（LGPL） |

## 获取方式

### 1. GPU Gems 系列（免费在线）

访问 NVIDIA 官方开发者网站：
- GPU Gems 1: https://developer.nvidia.com/gpugems/gpugems
- GPU Gems 2: https://developer.nvidia.com/gpugems/gpugems2
- GPU Gems 3: https://developer.nvidia.com/gpugems/gpugems3

每章都可单独下载 PDF。

### 2. SMAA 官方仓库（MIT/BSD 许可）

```bash
git clone https://github.com/iryoku/smaa.git docs/literature/smaa/
```

或从 6.5.9 附带的 `ddfix_assets/shaders/SMAA.h` 等 4 个文件复制（**注意**：6.5.9 附带的就是 SMAA 官方未修改版，可直接复用）。

### 3. Cascaded Shadow Maps（MSDN 官方文章）

- 在线版: https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps
- 离线版: 用浏览"另存为"功能保存为 HTML

### 4. Wine d3d9 公开实现

- 在线版: https://gitlab.winehq.org/wine/wine/-/blob/master/dlls/d3d9/d3d9_executebuf.c
- 本地: 用 git 克隆 Wine 仓库

## 引用规范

每个 .fx/.hlsl 文件头必须包含：

```hlsl
// Implementation based on:
//   - [文献名], [章节号]: "[标题]"
//   - URL: [公开链接]
//   - Author: [原作者]
//   - Adapted to MeteorBladeEnhancer by [作者], [日期]
//
// 与 6.5.9 [6.5.9 文件名] 的功能差异:
//   - [功能差异说明]
```

每个 PR 必须包含：
1. 公开文献引用链接
2. "与 6.5.9 .fx 的功能差异" 段落
3. 单元测试 / 视觉对比截图

## 维护规则

- 任何新增引用必须先 PR 到本目录
- 任何代码提交前必须能在此目录中找到对应文献
- 任何"基于 6.5.9 观察"（比如接口签名）必须有"实际观察" vs "公开文档"的双重证据
- **禁止**复制 6.5.9 .fx 源码（除 SMAA 官方库 MIT 许可部分）
- **禁止**反汇编 ddfix.dll 提取算法伪代码直接使用

## 状态

- [x] GPU Gems 1 关键章节引用（占位 README）
- [x] GPU Gems 2 关键章节引用（占位 README）
- [x] GPU Gems 3 关键章节引用（占位 README）
- [x] SMAA 官方仓库引用（占位 README + 文件复制）
- [x] Cascaded Shadow Maps 引用（占位 README）
- [x] Wine d3d9 公开实现引用（占位 README）
- [ ] 实际 PDF/HTML 下载（用户可手动完成；引用关系已建立）

**注**：本目录的子 README 已建立**引用关系**和**实现路径**。具体文献 PDF/HTML 由用户访问 [PUBLIC_LITERATURE.md](../../PUBLIC_LITERATURE.md) 中的链接手动下载（本仓库不存放第三方文献以避免版权问题）。

**最后更新**：2026-06-24
**维护人**：项目维护组
