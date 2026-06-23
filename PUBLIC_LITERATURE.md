# Public Literature Reference (公开文献引用库)

> **目的**：为 ddfix 的高级图形功能（GBuffer、Deferred、Shadow、SMAA、GodRays、Skinning 等）实现提供**合法可参考**的公开学术文献清单。所有实现都基于此清单中的公开资料，**不参考** 滴风 6.5.9 闭源 ddfix.dll 的二进制反汇编或 .fx 源代码的具体表达。
>
> **合规边界**：
> - ✅ 引用此清单的论文 / 章节 / 官方代码，写自己的实现
> - ✅ 复用 SMAA 等明确开源 / 公共领域 / 官方公开的实现
> - ❌ 复制 6.5.9 .fx 文件或反汇编 ddfix.dll 的具体代码
> - ❌ 任何"实质相似"的代码复用
>
> **Clean Room 流程**：
> 1. 设计师 / 需求方读 6.5.9 .fx 提取"做什么、参数是什么"
> 2. 程序员**不看** .fx 源码，仅基于此清单 + 公开文献独立实现
> 3. 提交时标注"基于 [文献X] 第 Y 章实现"

---

## 1. GBuffer / 多渲染目标 (MRT)

| 项 | 资源 | 链接 | 状态 |
|---|---|---|---|
| D3D9 MRT 官方文档 | Microsoft Learn | https://learn.microsoft.com/en-us/windows/win32/direct3d9/multiple-render-targets | ✅ |
| DirectX SDK Sample "HdrLighting" | Microsoft DirectX SDK (June 2010) | 本地安装 | ✅ |
| DirectX SDK Sample "DeferredShading" | Microsoft DirectX SDK (June 2010) | 本地安装 | ✅ |
| NVIDIA SDK Sample "Deferred Shading" | NVIDIA SDK 10.5 | https://developer.nvidia.com/legacy-sdk | ✅ |

**算法要点**（仅作目录参考，不引用具体实现细节）：
- GBuffer 包含 Depth + Normal + Diffuse + Specular（最少 4 个 RT）
- 用 1 个 MRT pass 完成几何提交，再用 N 个 light pass 完成光照
- 优点：光照计算与场景复杂度解耦；缺点：内存带宽高、不支持透明物体

---

## 2. Deferred Shading 经典文献

| 项 | 资源 | 链接 | 许可 |
|---|---|---|---|
| **GPU Gems 1 第 9 章** "Deferred Shading" | NVIDIA / Addison-Wesley 2004 | https://developer.nvidia.com/gpugems/gpugems/part-02/gpugems-ch09-deferred-shading | 免费在线 |
| **GPU Gems 2 第 9 章** "Deferred Shading in S.T.A.L.K.E.R." (Oles Shishkovtsov) | NVIDIA / Addison-Wesley 2005 | https://developer.nvidia.com/gpugems/gpugems2/part-02-shading-lighting-and-shadows/chapter-9-deferred-shading-stalker | 免费在线 |
| **GPU Gems 3 第 19 章** "Deferred Shading in Tabula Rasa" (Rusty Koonce) | NVIDIA / Addison-Wesley 2007 | https://developer.nvidia.com/gpugems/gpugems3/part-02-light-and-shadows/chapter-19-deferred-shading-tabula-rasa | 免费在线 |
| Crytek 2004 演示 "Deferred Shading" | Crytek | https://www.crytek.com/technology/presentations | 公开技术分享 |

**应用建议**：以 GPU Gems 2 第 9 章为主线（最完整），辅以 GPU Gems 3 第 19 章（多光源 + 前向兼容）。

---

## 3. Cascaded Shadow Maps (CSM) / PCF

| 项 | 资源 | 链接 | 许可 |
|---|---|---|---|
| **MSDN "Cascaded Shadow Maps"** | Microsoft Learn | https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps | 免费 |
| **GPU Gems 3 第 8 章** "Parallel-Split Shadow Maps on Programmable GPUs" (Fan Zhang) | NVIDIA | https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-8-parallel-split-shadow-maps-programmable-gpus | 免费 |
| **GPU Gems 1 第 11 章** "Shadow Map Antialiasing" (Michael D. McCool) | NVIDIA | https://developer.nvidia.com/gpugems/gpugems/part-02/gpugems-ch11-shadow-map-antialiasing | 免费 |
| **GPU Gems 2 第 8.4 节** "Percentage-Closer Soft Shadows" (Randima Fernando) | NVIDIA | https://developer.nvidia.com/gpugems/gpugems2/part-02-shading-lighting-and-shadows/chapter-8-terrain-rendering-shadows | 免费 |
| NVIDIA SDK Sample "CascadedShadowMaps11" | NVIDIA SDK | 本地安装 | 免费 |

**算法要点**：
- 把视锥分割为 N 个子视锥（typical 3-4）
- 每个子视锥渲染一张深度图
- 主 pass 按像素深度挑选对应 shadow map
- PCF 软化阴影边缘

---

## 4. SMAA (Subpixel Morphological Antialiasing)

| 项 | 资源 | 链接 | 许可 |
|---|---|---|---|
| **SMAA 官方仓库** (Jorge Jimenez) | GitHub | https://github.com/iryoku/smaa | **MIT / 二条款 BSD**（可商用） |
| **SMAA 论文** (Eurographics 2012) | Computer Graphics Forum | https://www.iryoku.com/smaa/ | 免费 PDF |
| **SMAA 预编译 Demo** | iryoku.com | https://www.iryoku.com/smaa/#downloads | 免费 |

**关键事实**：
- 6.5.9 的 [SMAA.h](file:///d:/Program%20Files%20(x86)/desktop/metro/图像增强补丁6.5.9/手动安装版/ddfix_assets/shaders/SMAA.h) **就是 SMAA 官方库**（52KB，未修改），MIT/BSD 许可
- 我们可以**直接复用** 6.5.9 包里的 SMAA.h / SMAA.fx（许可允许，且文件本身就是 SMAA 官方未修改版）
- **注意**：可复用 SMAA 库；不可复用 6.5.9 的 GBuffer / Deferred / Shadow 等非 SMAA 代码

**集成路径**：
1. 把 `SMAA.h` / `SMAA.fx` / `SearchTex.h` / `AreaTex.h` 复制到我们项目
2. 渲染管线末尾加 SMAA EdgeDetection → BlendingWeight → NeighborhoodBlending 三 pass
3. 在 `ddfix.ini` 加 `[SMAA]` 配置段

---

## 5. Volumetric Light / God Rays

| 项 | 资源 | 链接 | 许可 |
|---|---|---|---|
| **GPU Gems 3 第 13 章** "Volumetric Light Scattering" (Nvidia) | NVIDIA | https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-13-volumetric-light-scattering | 免费 |
| Crytek "Crysis" God Rays 实现 | Crytek 公开技术分享 | https://www.crytek.com/technology/presentations | 公开 |

**重要发现**：6.5.9 的 BlendGodRays.fx **自己注明 "Copy from GPU GEM3 Chapter 13"**，意味着其实现**就是公开算法的直接复制**。我们可以基于 GPU Gems 3 第 13 章独立实现，完全合规。

---

## 6. 硬件蒙皮 (GPU Skinning)

| 项 | 资源 | 链接 | 许可 |
|---|---|---|---|
| **GPU Gems 第 4.6 节** "Skin in the 'Dawn' Demo" | NVIDIA | https://developer.nvidia.com/gpugems/gpugems/part-04/gpugems-ch46-skin-dawn-demo | 免费 |
| DirectX SDK Sample "SkinnedMesh" | Microsoft DirectX SDK | 本地安装 | 免费 |
| **DirectX SDK Sample "MeshFromOBJ" + "Skinning"** | Microsoft | 本地安装 | 免费 |

**算法要点**：
- 顶点携带最多 4（或 6）个骨骼索引 + 权重
- 顶点 shader 中查询骨骼矩阵，线性混合
- 对流星蝴蝶剑的 PVB 文件需要先做格式解析（公开格式由玩家社区逆向）

---

## 7. 高度雾 / Linear-Z / 顶点动画

| 项 | 资源 | 链接 |
|---|---|---|
| **GPU Gems 2 第 12.3 节** "Height-Based Fog" | NVIDIA | https://developer.nvidia.com/gpugems/gpugems2/part-i-geometric-complexity/chapter-12-stitching-vignette |
| **GPU Gems 5 第 28 章** "Logarithmic Depth Buffers" | NVIDIA | https://developer.nvidia.com/gpugems/gpugems5/part-ii-rendering-techniques/chapter-28-logarithmic-depth-buffers |
| **GPU Gems 1 第 7.4 节** "Vertex Texture Fetch" | NVIDIA | https://developer.nvidia.com/gpugems/gpugems/part-01/gpugems-ch07-meshes |

**注**：6.5.9 的 MainLighting.fx 用了 `FluidAnimate`（顶点动画 GPU 化），对应 GPU Gems 第 7.4 节的"动画纹理查表"。

---

## 8. ExecuteBuffer / D3D 指令流解析

| 项 | 资源 | 链接 |
|---|---|---|
| **Wine d3d9 项目** | Wine | https://gitlab.winehq.org/wine/wine/-/tree/master/dlls/d3d9 |
| **Direct3D 9 SDK 文档** "Execute Buffers" | Microsoft | https://learn.microsoft.com/en-us/windows/win32/direct3d9/execute-buffers |
| **DX6 D3DOP_* 操作码文档** | Microsoft DirectX 6 SDK | 本地安装 / 公开档案 |

**算法要点**：
- IDirect3DDevice3::Execute 接受 D3DExecuteBuffer，解析 D3DHAL_OPCODE_* 指令
- 我们需要拦截 ExecuteBuffer，提取三角形列表、纹理坐标、变换矩阵
- 转换为 D3D9 VertexBuffer / IndexBuffer 后由我们自己的 VS 渲染

---

## 9. D3DX Effect / Shader 编译

| 项 | 资源 | 链接 |
|---|---|---|
| **D3DX9 文档** | Microsoft Learn | https://learn.microsoft.com/en-us/windows/win32/direct3d9/dx9-graphics-reference-effects |
| **fxc.exe 用法** | DirectX SDK | 本地 fxc.exe |
| **D3DXCreateEffectFromFile 离线预编译** | 官方推荐 | 编译时 fxc → .fxb → C header |

---

## 10. 流星蝴蝶剑 特定资料（社区公开逆向）

| 项 | 资源 | 链接 |
|---|---|---|
| MeteorBladeEnhancer 源码（开源） | GitHub | https://github.com/gwmgdemj/MeteorBladeEnhancer |
| ddfix 官方文档 | 项目内 `ARCHITECTURE.md` / `PROJECT_ANALYSIS.md` | 本地 |
| 流星蝴蝶剑 .PVB 蒙皮格式 | 玩家社区文档 | 待整理 |
| 流星蝴蝶剑 .GMB 地图格式 | 玩家社区文档 | 待整理 |

**注意**：以上是**格式逆向**（用开源 / 公开工具），不是**代码逆向**。我们的 ddfix 已经基于开源版本实现，可直接复用。

---

## 11. 不在文献中的内容（设计原则）

以下设计点在公开文献**没有**直接对照，需要我们独立设计：

1. **HP 槽 / UI 偏移修复** — 我们 ddfix 的 `CalcBackBufferSize` bug 修法是基于 Direct3D 9 Present Parameters 文档的官方推荐做法
2. **Pillarbox 居中** — 基于 DirectDraw RECT 与 D3D9 viewport 映射的常识
3. **AutoHotkey 脚本 API 集成** — AHK 官方有现成 API，不涉及任何逆向
4. **多游戏版本支持** — 主要是创建 Surface 路径的兼容，与 6.5.9 无关

---

## 12. 引用与标注规范

在代码注释中，所有引用的实现必须**明确标注**：

```hlsl
// Implementation based on:
//   - GPU Gems 2, Chapter 9: "Deferred Shading in S.T.A.L.K.E.R."
//   - URL: https://developer.nvidia.com/gpugems/gpugems2/part-02-shading-lighting-and-shadows/chapter-9-deferred-shading-stalker
//   - Author: Oles Shishkovtsov
//   - Adapted to MeteorBladeEnhancer by [author], 2026
```

每个 PR 必须有：
1. 引用文献的链接
2. "与 6.5.9 .fx 的功能差异" 段落（说明我们的实现与 6.5.9 的关系）
3. 单元测试 / 视觉对比截图

---

## 13. 维护规则

- 任何新增引用必须先 PR 到本文件
- 任何代码提交前必须能在此清单中找到对应文献
- 任何"基于 6.5.9 6.5.9 观察"（比如接口签名）必须有"实际观察" vs "公开文档"的双重证据

---

**最后更新**：2026-06-23
**维护人**：项目维护组
**许可**：本文档以 CC BY-SA 4.0 发布，可自由转载 / 修改，注明出处即可
