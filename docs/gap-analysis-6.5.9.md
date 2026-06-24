# 6.5.9 闭源补丁差距分析

> **目的**：对比滴风 6.5.9 闭源补丁的 11 个着色器与公开学术文献，明确每个 .fx 的"实现路径"和"关键卡点"。
> **合规边界**：本分析**不**引用 6.5.9 .fx 源码的具体表达，**不**反汇编 ddfix.dll 提取算法伪代码。所有实现必须基于 [PUBLIC_LITERATURE.md](../../PUBLIC_LITERATURE.md) 中的公开学术文献。

## 6.5.9 11 个着色器 vs 公开文献对照表

| .fx 文件 | 功能描述 | 公开文献章节 | 实现难度 | 关键卡点 | 对应 Phase |
|---|---|---|---|---|---|
| **GBuffer.fx** | 多渲染目标 MRT（PosTex/NormalTex/DiffuseTex/SpecularTex） | GPU Gems 2 第 9 章（STALKER）/ GPU Gems 3 第 19 章（Tabula Rasa） | 🟡 中 | ExecuteBuffer 解析 + MRT 资源管理 | Phase 9.3 |
| **Deferred.fx** | Deferred Lighting 主 pass（含阴影） | GPU Gems 2 第 9.5 节 | 🟡 中 | 多光源累加 + 阴影采样 | Phase 9.3 + 9.4 |
| **MainLighting.fx** | 直接前向渲染（与 GBuffer 并行） | Direct3D 9 FFP + 增强 | 🟢 易 | 兼容固定管线状态 | Phase 9.3 |
| **Shadow.fx** | Cascaded Shadow Map + PCF | GPU Gems 3 第 8 章 / GPU Gems 2 第 8.4 节 | 🟡 中 | 视锥分割 + PCF 软化 | Phase 9.4 |
| **BlendGodRays.fx** | 体积光（GPU Gems 3 第 13 章） | GPU Gems 3 第 13 章 | 🟢 易 | 屏幕空间径向模糊 | Phase 9.6 |
| **SMAA.fx + SMAA.h** | 抗锯齿 | SMAA 官方仓库（MIT/BSD 许可） | 🟢 易 | 三 pass 调度 | Phase 9.2 |
| **Skinning.fx** | 硬件蒙皮 | GPU Gems 第 4.6 节 | 🟡 中 | PVB 格式逆向 + 骨骼索引 | Phase 9.5 |
| **FixedPipelineEx.fx** | 固定管线增强 | Direct3D 9 FFP 文档 | 🟢 易 | 兼容老旧渲染状态 | Phase 9.3 |
| **Debug.fx** | GBuffer/Shadow/Normal 可视化 | 自实现 | 🟢 易 | 多 pass 显示 | Phase 9.3-9.4 |
| **Main.fx** | Effect 框架入口 | D3DX Effect 文档 | 🟢 易 | 框架组合 | 全部 Phase |
| **ColorKey.hlsl** | 颜色键 alpha 测试 | D3D9 AlphaTest 文档 | 🟢 易 | ColorKey HLSL 已有 | Phase 1-8 已完成 |

## 闭源 6.5.9 vs 我们现状对比

| 维度 | 闭源 6.5.9 | 我们现状 | 差距 |
|---|---|---|---|
| **DX6→DX9 翻译层** | ✅ 完整 | ✅ Phase 1-8 完成 | 无差距 |
| **HP 槽/UI 位置** | ✅ 正确 | ❌ CalcBackBufferSize 用显示器分辨率 | Phase 9.1 修复 |
| **ColorKey 处理** | ✅ 1 个 hlsl | ✅ 1 个 hlsl | 无差距 |
| **抗锯齿** | ✅ SMAA 完整 | ❌ 无 | Phase 9.2 集成 |
| **GBuffer 重建** | ✅ 完整 | ❌ 无 | Phase 9.3 核心 |
| **Deferred Lighting** | ✅ 完整（含阴影） | ❌ 无 | Phase 9.3-9.4 |
| **Cascaded Shadow** | ✅ 3-4 级 PCF | ❌ 无 | Phase 9.4 |
| **体积光** | ✅ God Rays | ❌ 无 | Phase 9.6 |
| **硬件蒙皮** | ✅ 6 骨骼/顶点 | ❌ 无 | Phase 9.5 |
| **顶点动画 GPU 化** | ✅ sin/cos 动画查表 | ❌ 无 | Phase 9.5 |
| **AutoHotkey 集成** | ✅ 16 个 API | ❌ 无 | Phase 9.7 |
| **多游戏版本** | ✅ 5 个版本 | ❌ 单一 | Phase 9.10 |
| **配置 GUI** | ✅ d3d8 预览 | ❌ 无 | Phase 9.8（可选） |
| **安装器** | ✅ PE patcher | ❌ 无 | Phase 9.9（可选） |
| **7 年 edge case** | ✅ 沉淀 | ❌ 无 | 无法弥补（持续 7 年迭代） |

## 关键卡点详细分析

### 1. ExecuteBuffer 解析（最大难点）
- **问题**：6.5.9 拦截 `IDirect3DDevice3::Execute` 逐条解析 D3D 指令流
- **D3D 指令数**：约 30 个 OP 码（D3DOP_TRIANGLE / D3DOP_MATRIX / D3DOP_TEXTURELOAD / D3DOP_STATERENDER / D3DOP_PROCESSVERTICES 等）
- **公开参考**：
  - [Wine d3d9 项目](https://gitlab.winehq.org/wine/wine/-/tree/master/dlls/d3d9)（`d3d9_executebuf.c`）
  - Microsoft DirectX 6 SDK 文档（公开档案）
- **工作估算**：4 周（6 个核心 OP 码 + 边界处理）
- **风险**：高（需要真实游戏实例反复调试）

### 2. PVB 蒙皮格式逆向
- **问题**：6.5.9 自己逆向了 .PVB 蒙皮格式（顶点 + 骨骼索引 + 权重）
- **公开参考**：玩家社区公开逆向笔记（非 6.5.9 提供）
- **工作估算**：2 周
- **风险**：中（玩家社区资源不完整）

### 3. 多游戏版本兼容
- **问题**：5 个版本（1.07.16 / 9.07.16 / 1.08.1 / 9.08.1 / 1.08.3）ExecuteBuffer 字节流可能不同
- **公开参考**：无（需要 5 个版本的真实实例）
- **工作估算**：3 周
- **风险**：高（需要 5 个版本的游戏实例）

### 4. 性能调优
- **问题**：60 FPS 稳定 + GPU 占用 < 80%
- **公开参考**：RenderDoc 官方文档、AMD GPU PerfStudio、NVIDIA NSight
- **工作估算**：3 周
- **风险**：中（取决于硬件性能）

## 90% 可达性分析

### 能做到的 90%
- 抗锯齿（SMAA 100% 还原）
- 光照方向感（GBuffer + Deferred 90%）
- 阴影（Cascaded Shadow + PCF 90%）
- 体积光（God Rays 95%）
- 抗锯齿边缘平滑（SMAA 100%）

### 部分做到的（70-85%）
- 硬件蒙皮（PVB 解析可能不完整）
- 顶点动画（需要真实 .ANM 文件）

### 难做到的（< 60%）
- 5 个游戏版本的所有兼容性细节
- 6.5.9 的 7 年 edge case 沉淀
- 6.5.9 与特定游戏机制（招式系统、战斗数值）的耦合

### 做不到的（0%）
- 滴风的 7 年特定知识
- 6.5.9 的注册表/驱动层修改
- 6.5.9 的所有隐藏配置项

## 优先级建议

| 优先级 | Phase | 理由 |
|---|---|---|
| 🔴 P0 | 9.0 文献本地化 | 后续所有阶段的基础 |
| 🔴 P0 | 9.1 HP 槽修复 | 用户朋友已确认是真问题，立竿见影 |
| 🟡 P1 | 9.2 SMAA 集成 | 1 周内可见效果，官方库可直接复用 |
| 🟡 P1 | 9.3 GBuffer + Deferred | 核心渲染管线，最大工作量 |
| 🟡 P1 | 9.4 Cascaded Shadow | GBuffer 完成后的自然延伸 |
| 🟢 P2 | 9.5 硬件蒙皮 | 依赖 PVB 解析，需时较长 |
| 🟢 P2 | 9.6 God Rays | 视觉效果好，1-2 周可完成 |
| 🟢 P2 | 9.7 AutoHotkey API | 用户可配置化 |
| ⚪ P3 | 9.8 配置 GUI | 可选，性价比低 |
| ⚪ P3 | 9.9 PE Patcher | 可选，6.5.9 用此方法 |
| 🟡 P1 | 9.10 多版本兼容 | 决定 5 个版本能跑 |

## 实现路径示例

### 示例：实现 GBuffer（基于 GPU Gems 2 第 9 章）

```hlsl
// Implementation based on:
//   - GPU Gems 2, Chapter 9: "Deferred Shading in S.T.A.L.K.E.R."
//   - URL: https://developer.nvidia.com/gpugems/gpugems2/part-02-shading-lighting-and-shadows/chapter-9-deferred-shading-stalker
//   - Author: Oles Shishkovtsov
//   - Adapted to MeteorBladeEnhancer by longxiaocao, 2026
//
// 与 6.5.9 GBuffer.fx 的功能差异：
//   - 6.5.9 用了 4 个 MRT（PosTex/NormalTex/DiffuseTex/SpecularTex）
//   - 我们的实现也用 4 个 MRT，命名一致
//   - 但 PS 中具体实现细节（采样纹理、计算公式）完全独立
//   - 我们不参考 6.5.9 .fx 源码的具体表达

struct PS_OUTPUT
{
    float4 PosTex : COLOR0;       // RGB = world position, A = depth
    float4 NormalTex : COLOR1;    // RGB = world normal, A = unused
    float4 DiffuseTex : COLOR2;   // RGBA = diffuse color (vertex + texture)
    float4 SpecularTex : COLOR3;  // RGB = specular, A = specular power
};

PS_OUTPUT PS_GBuffer(VS_OUTPUT input)
{
    PS_OUTPUT output;
    
    // World position (来自 VS)
    output.PosTex = float4(input.WorldPos, input.Depth);
    
    // World normal (来自 VS, 变换到世界空间)
    output.NormalTex = float4(normalize(input.WorldNormal), 0.0f);
    
    // Diffuse = 顶点色 × 纹理采样
    float4 texColor = tex2D(DiffuseSampler, input.UV);
    output.DiffuseTex = input.VertexColor * texColor;
    
    // Specular (默认 0.5 specular, 32 power)
    output.SpecularTex = float4(0.5f, 0.5f, 0.5f, 32.0f);
    
    return output;
}
```

## 风险与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| 没有游戏实机测试 | 部分功能可能到 90% 后难提升 | 早期 + 频繁找朋友实机测试 |
| ExecuteBuffer 解析不完备 | GBuffer/Deferred 效果打折扣 | 从基础 OP 码开始，逐步覆盖 |
| PVB 蒙皮格式内部细节 | 蒙皮效果到 70% 难提升 | 公开逆向 + 玩家社区资源 |
| 6.5.9 闭源无法对照 | 可能重蹈覆辙或落后 | 严格 Clean Room + 公开文献 |
| 性能调优耗时 | 可能拖 1-2 个月 | 早 profiling + 早 RenderDoc |

## 维护规则

1. **每个 .fx/.hlsl 文件头必须包含**：
   ```hlsl
   // Implementation based on:
   //   - [公开文献名], [章节号]: "[标题]"
   //   - URL: [公开链接]
   //   - Author: [原作者]
   //   - Adapted to MeteorBladeEnhancer by [作者], [日期]
   //
   // 与 6.5.9 [6.5.9 文件名] 的功能差异:
   //   - [功能差异说明]
   ```

2. **每个 PR 必须包含**：
   - 公开文献引用链接
   - "与 6.5.9 .fx 的功能差异" 段落
   - 单元测试 / 视觉对比截图

3. **禁止**：
   - 复制 6.5.9 .fx 源码到仓库（除 SMAA 官方库 MIT 许可部分）
   - 反汇编 ddfix.dll 提取算法伪代码直接使用

## 进度跟踪

- [ ] Phase 9.0 完成（差距分析 + 文献本地化）
- [ ] Phase 9.1 完成（HP 槽/UI 修复）
- [ ] Phase 9.2 完成（SMAA 集成）
- [ ] Phase 9.3 完成（ExecuteBuffer + GBuffer + Deferred）
- [ ] Phase 9.4 完成（Cascaded Shadow + PCF）
- [ ] Phase 9.5 完成（硬件蒙皮 + 顶点动画）
- [ ] Phase 9.6 完成（God Rays）
- [ ] Phase 9.7 完成（AutoHotkey API）
- [ ] Phase 9.8 完成（配置 GUI，可选）
- [ ] Phase 9.9 完成（PE Patcher，可选）
- [ ] Phase 9.10 完成（多版本兼容 + 性能调优）

**最后更新**：2026-06-24
**维护人**：项目维护组
