// Phase 9.7: 脚本 API 注册器 (ScriptApi)
// 设计目标：
//   1. 暴露 16 个渲染/环境/UI 控制 API 给 .as 脚本（AutoHotkey v1 P/Invoke）
//   2. 全部 API 走 C-linkage __stdcall + 简单类型参数（int/float/const char*）
//      → AutoHotkey v1 的 DllCall 直接消费，无需 marshaling wrapper
//   3. 用 std::function 内部实现 + 单例状态，支持热替换（同一函数名重新 Register）
//   4. 命名规范来源：Windows API（PascalCase 函数名 + 模块前缀 Ddfix_） +
//      AutoHotkey 官方 DllCall 文档（标准调用约定 + 简单类型）
//   5. Clean Room：脚本 API 是独立子系统，不依赖 GBuffer/Deferred 渲染管线
//
// 与 Phase 9.3 GBuffer 的关系：
//   - 本类不直接调用 GBufferRenderer::RenderFrame
//   - 由 Phase 9.3.11b/9.7 后续集成层在每帧 Present 之前拉取本类状态，
//     推送到 GBuffer/Deferred 渲染管线
//   - 本类自身只做"状态存取"职责，不触发渲染
//
// 线程安全：
//   - 内部状态用 std::mutex 保护（脚本可能从 AHK 主线程访问，渲染线程读状态）
//   - RegisterAll / UnregisterAll 假定在 DLL_PROCESS_ATTACH/DETACH 单线程下调用

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <functional>
#include <mutex>
#include <string>

namespace NDDFIX
{
namespace Script
{

// 单例状态容器 + 16 个 setter/getter
//
// WHY: 与 ddfix 项目其他子模块（HudRenderer / ConfigManager / GBufferRenderer）
//   保持一致的"单例 + Instance()"风格，方便调用方零样板集成。
//   复制禁用：避免误用产生多份状态副本。
class ScriptApi
{
public:
	static ScriptApi* Instance();

	// Phase 9.7 build fix: namespace 块内辅助函数 ScriptApi_Get*_Raw
	//   需要访问 private 成员 (m_currentConfig / m_selectedLight / m_mutex).
	//   在类内 friend 声明这些自由函数, 避免把 private 改 public 破坏封装.
	friend const char* ScriptApi_GetCurrentRenderConfig_Raw();
	friend const char* ScriptApi_GetSelectedLight_Raw();

	// -------------------- 生命周期 --------------------
	//
	// RegisterAll / UnregisterAll 是 Phase 9.7 的注册/反注册占位。
	// 当前实现仅做"幂等"标志位 + 日志，不真正调 GetProcAddress。
	// 后续 Phase 9.7+ 可在此处扩展成"向 AHK 引擎注册符号表"（AHK 自身
	// 就靠 DllCall + 函数名解析，所以 C-linkage __stdcall + 函数名导出
	// 已被 GetProcAddress 自然支持）。
	//
	// 返回值：true=状态切换成功，false=幂等冲突（重复 Register 或重复 Unregister）
	bool RegisterAll();
	void UnregisterAll();

	bool IsRegistered() const { return m_registered; }

	// -------------------- 渲染管线控制 --------------------
	void SetRenderEnabled(bool enabled);
	bool IsRenderEnabled() const;

	// 切换活跃 render config section。ConfigManager::Instance() 由调用方（dllmain）
	//   在 RegisterAll 之前已 Load() 过，所以 section 名称一定能命中；不命中时
	//   静默保留原 section（与 ConfigManager::GetRender 行为一致）。
	void ChangeRenderConfig(const std::string& sectionName);
	std::string GetCurrentRenderConfig() const;

	// -------------------- 环境/雾 --------------------
	void SetAmbientColor(float r, float g, float b);
	void GetAmbientColor(float* r, float* g, float* b) const;

	void SetFogDistance(float nearDist, float farDist);
	void GetFogDistance(float* nearDist, float* farDist) const;

	void SetFogHeight(float top, float bottom);
	void GetFogHeight(float* top, float* bottom) const;

	void SetFogColor(float r, float g, float b);
	void GetFogColor(float* r, float* g, float* b) const;

	// -------------------- 光源控制 --------------------
	// SelectLight 切到指定名字的逻辑光源（"MainLight" / "FillLight" / 自定义）。
	//   真实实现由 Phase 9.7+ 与 IDirect3DLight proxy 集成。
	void SelectLight(const std::string& lightName);
	std::string GetSelectedLight() const;

	void SetLightEnabled(bool enabled);
	bool IsLightEnabled() const;

	void SetLightPosition(float x, float y, float z);
	void GetLightPosition(float* x, float* y, float* z) const;

	// 光源旋转（弧度）。命名沿用 D3D 文档 "rotation" 用 arc 表示。
	void SetLightRotationArc(float x, float y, float z);
	void GetLightRotationArc(float* x, float* y, float* z) const;

	void SetLightColor(float r, float g, float b);
	void GetLightColor(float* r, float* g, float* b) const;

	void SetLightBrightness(float brightness);
	float GetLightBrightness() const;

	// 0.0=无阴影, 1.0=全阴影。命名沿用 SMAA / GodRays 的 "Strength" 词。
	void SetLightShadowStrength(float strength);
	float GetLightShadowStrength() const;

	// -------------------- UI --------------------
	void SetArenaUIVisible(bool visible);
	bool IsArenaUIVisible() const;

	// -------------------- 数学 --------------------
	//
	// WorldToScreenPoint: 无视口上下文时返 (0, 0)。
	// 真实世界→屏幕投影留到 Phase 9.7+ 与 D3D9Context 集成后实现。
	void WorldToScreenPoint(float wx, float wy, float wz, float* sx, float* sy) const;

	// -------------------- 回调 --------------------
	//
	// 回调函数签名由调用方（脚本）负责遵守：
	//   void __stdcall OnHit(int attackerId, int victimId, float damage);
	// 这里只存函数指针 + 调用约定标记，不做参数校验（脚本是 trusted caller）。
	void SetCharacterHitCallback(void* callback);
	void* GetCharacterHitCallback() const;

	// -------------------- 内部辅助：测试用重置 --------------------
	//
	// WHY: 单元测试需要在每个 TEST 之间清空状态。
	//   不能删 — 保留为 public 让 ddfixtests 调；产品代码请勿调。
	void ResetForTest();

private:
	ScriptApi();
	~ScriptApi();
	ScriptApi(const ScriptApi&) = delete;
	ScriptApi& operator=(const ScriptApi&) = delete;

	// 内部 POD 类型
	struct Vec3
	{
		float x, y, z;
	};
	struct Color3
	{
		float r, g, b;
	};

	// 状态（全部受 m_mutex 保护）
	bool        m_renderEnabled;
	std::string m_currentConfig;
	Color3      m_ambientColor;
	float       m_fogNear;
	float       m_fogFar;
	float       m_fogTop;
	float       m_fogBottom;
	Color3      m_fogColor;
	std::string m_selectedLight;
	bool        m_lightEnabled;
	Vec3        m_lightPos;
	Vec3        m_lightRot;
	Color3      m_lightColor;
	float       m_lightBrightness;
	float       m_lightShadowStrength;
	bool        m_arenaUIVisible;
	void*       m_hitCallback;

	// 注册状态
	bool m_registered;

	// 互斥锁（mutable：const getter 也能 lock）
	mutable std::mutex m_mutex;
};

} // namespace Script
} // namespace NDDFIX
