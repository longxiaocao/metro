# Wine D3D9 公开实现

> **引用目的**：为 MeteorBladeEnhancer ExecuteBuffer 解析器提供 Wine 项目公开实现依据。
> **官方仓库**：https://gitlab.winehq.org/wine/wine/-/tree/master/dlls/d3d9
> **许可**：Wine 项目（LGPL-2.1+）

## 关键文件

### d3d9_executebuf.c
- **路径**: `dlls/d3d9/d3d9_executebuf.c`
- **URL**: https://gitlab.winehq.org/wine/wine/-/blob/master/dlls/d3d9/d3d9_executebuf.c
- **内容**:
  - D3D 指令流解析（D3DOP_* 系列）
  - D3DOP_TRIANGLE / D3DOP_MATRIX / D3DOP_TEXTURELOAD 等
  - ExecuteBuffer 结构
- **对应 Phase**: 9.3

## 引用规范

在代码注释中使用：

```cpp
// Implementation based on:
//   - Wine Project: d3d9/d3d9_executebuf.c
//   - URL: https://gitlab.winehq.org/wine/wine/-/blob/master/dlls/d3d9/d3d9_executebuf.c
//   - License: LGPL-2.1+
//   - Adapted to MeteorBladeEnhancer by [作者], [日期]
```

**注**：Wine 是 LGPL-2.1+ 许可，**只读参考**算法思路，**不直接复制代码**到我们的 MIT/BSD 项目（避免许可冲突）。

## 下载方式

```bash
git clone https://gitlab.winehq.org/wine/wine.git
```

或直接访问 GitLab 在线版。

**最后更新**：2026-06-24
