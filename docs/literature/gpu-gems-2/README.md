# GPU Gems 2 关键章节

> **引用目的**：为 MeteorBladeEnhancer 高级渲染管线（特别是 STALKER 风格 Deferred Shading）提供公开学术文献依据。
> **官方链接**：https://developer.nvidia.com/gpugems/gpugems2
> **许可**：NVIDIA 免费在线

## 引用的章节

| 章节 | 标题 | 用途 | 对应 Phase |
|---|---|---|---|
| 第 8.4 节 | Percentage-Closer Soft Shadows (PCF) | 软阴影算法 | Phase 9.4 |
| 第 9 章 | Deferred Shading in S.T.A.L.K.E.R. | **核心 GBuffer 实现** | Phase 9.3 |
| 第 9.5 节 | Deferred Lighting | **核心 Deferred Lighting** | Phase 9.3 |
| 第 12.3 节 | Height-Based Fog | 高度雾 | Phase 9.3+ |

## 引用规范

在代码注释中使用：

```hlsl
// Implementation based on:
//   - GPU Gems 2, Chapter 9: "Deferred Shading in S.T.A.L.K.E.R."
//   - URL: https://developer.nvidia.com/gpugems/gpugems2/part-02-shading-lighting-and-shadows/chapter-9-deferred-shading-stalker
//   - Author: Oles Shishkovtsov
//   - Adapted to MeteorBladeEnhancer by [作者], [日期]
```

## 下载方式

访问 NVIDIA Developer 官网，每章都可单独下载 PDF。

**注**：本目录**不存放**第三方文献 PDF 以避免版权问题。请用户访问上方链接下载。

**最后更新**：2026-06-24
