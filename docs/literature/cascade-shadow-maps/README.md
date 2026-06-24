# Cascaded Shadow Maps 公开文献

> **引用目的**：为 MeteorBladeEnhancer 级联阴影提供 Microsoft 官方技术文章依据。
> **官方链接**：https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps
> **许可**：Microsoft 免费公开

## 关键文章

### 1. Microsoft Learn: Cascaded Shadow Maps
- **URL**: https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps
- **作者**: Microsoft Corporation
- **内容**:
  - 视锥分割为 N 个子视锥
  - 每个子视锥渲染一张深度图
  - 主 pass 按像素深度挑选对应 shadow map
  - PCF 软化阴影边缘
- **对应 Phase**: 9.4

## 引用规范

在代码注释中使用：

```hlsl
// Implementation based on:
//   - Microsoft Learn: "Cascaded Shadow Maps"
//   - URL: https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps
//   - Author: Microsoft Corporation
//   - Adapted to MeteorBladeEnhancer by [作者], [日期]
```

## 下载方式

访问 MSDN 官方文章页（链接见上），用浏览器"另存为"功能保存为 HTML 即可。

**注**：本目录**不存放**第三方文献 HTML 以避免版权问题。请用户访问上方链接下载。

**最后更新**：2026-06-24
