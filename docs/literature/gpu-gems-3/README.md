# GPU Gems 3 关键章节

> **引用目的**：为 MeteorBladeEnhancer 高级渲染管线（Cascaded Shadow / God Rays / Tabula Rasa Deferred）提供公开学术文献依据。
> **官方链接**：https://developer.nvidia.com/gpugems/gpugems3
> **许可**：NVIDIA 免费在线

## 引用的章节

| 章节 | 标题 | 用途 | 对应 Phase |
|---|---|---|---|
| 第 8 章 | Parallel-Split Shadow Maps on Programmable GPUs | **Cascaded Shadow Map** | Phase 9.4 |
| 第 13 章 | Volumetric Light Scattering | **God Rays 体积光**（6.5.9 BlendGodRays.fx 注明 "Copy from GPU GEM3 Chapter 13"）| Phase 9.6 |
| 第 19 章 | Deferred Shading in Tabula Rasa | 多光源 + 前向兼容 Deferred | Phase 9.3 |

## 引用规范

在代码注释中使用：

```hlsl
// Implementation based on:
//   - GPU Gems 3, Chapter 13: "Volumetric Light Scattering"
//   - URL: https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-13-volumetric-light-scattering
//   - Author: [NVIDIA GPU Gems 编辑组]
//   - Adapted to MeteorBladeEnhancer by [作者], [日期]
```

## 下载方式

访问 NVIDIA Developer 官网，每章都可单独下载 PDF。

**注**：本目录**不存放**第三方文献 PDF 以避免版权问题。请用户访问上方链接下载。

**最后更新**：2026-06-24
