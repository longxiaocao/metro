// Phase 9.7: ScriptApi 实现 + 16 个 C-linkage __stdcall 导出
//
// 设计要点：
//   1. ScriptApi 单例内部状态存取（线程安全，std::mutex 保护）
//   2. 16 个 Ddfix_* 函数全部 extern "C" + __declspec(dllexport) + __stdcall
//      → AutoHotkey v1 的 DllCall 用 stdcall 调用约定，零 marshaling
//   3. 命名规范：Ddfix_<Verb><Noun> 形式，参考 Win32 API（Set/Get/Is 动词 + 名词）
//   4. bool 参数在 __stdcall 边界用 int（4 字节），AHK 0/1 真值
//   5. 字符串返 const char*：内部静态缓冲，调用方必须立即使用
//      （标准 DLL 导出字符串约定，参考 GetProcAddress / GetModuleHandle）
//
// 公开文献：
//   - AutoHotkey v1 DllCall: https://www.autohotkey.com/docs/commands/DllCall.htm
//   - Microsoft x64 Calling Convention: https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention
//   - Win32 API naming: https://learn.microsoft.com/en-us/windows/win32/apiindex/api-index

#include "ScriptApi.h"

#include "../Common/Logging.h"

#include <cstring>   // std::strncpy / std::strlen

namespace NDDFIX
{
namespace Script
{

// ScriptApi 单例内 m_currentConfig / m_selectedLight 是永生 std::string，
// 调 c_str() 返的指针在单例生命周期内稳定。
// AHK 端按 DllCall 文档建议立即 StrVarSetCapacity 复制，详见
// https://www.autohotkey.com/docs/commands/DllCall.htm

// -------------------- 单例 --------------------

ScriptApi* ScriptApi::Instance()
{
	// 函数内 static：C++11 起线程安全初始化
	static ScriptApi inst;
	return &inst;
}

ScriptApi::ScriptApi()
	: m_renderEnabled(true)
	, m_currentConfig("Default")
	, m_ambientColor{0.2f, 0.2f, 0.2f}
	, m_fogNear(100.0f)
	, m_fogFar(1000.0f)
	, m_fogTop(1000.0f)
	, m_fogBottom(0.0f)
	, m_fogColor{0.5f, 0.5f, 0.5f}
	, m_selectedLight("MainLight")
	, m_lightEnabled(true)
	, m_lightPos{0.0f, 50.0f, 0.0f}
	, m_lightRot{0.0f, 0.0f, 0.0f}
	, m_lightColor{1.0f, 1.0f, 1.0f}
	, m_lightBrightness(1.0f)
	, m_lightShadowStrength(0.5f)
	, m_arenaUIVisible(true)
	, m_hitCallback(nullptr)
	, m_registered(false)
{
}

ScriptApi::~ScriptApi()
{
	// 析构时如果还注册着，强行反注册（防止 dllmain 漏调 UnregisterAll）
	if (m_registered)
	{
		UnregisterAll();
	}
}

// -------------------- 生命周期 --------------------

bool ScriptApi::RegisterAll()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_registered)
	{
		// 幂等：重复 Register 不报错，仅记录
		LOG_WARN("ScriptApi::RegisterAll: already registered, ignoring");
		return false;
	}
	m_registered = true;
	LOG_INFO("ScriptApi::RegisterAll: ok (16 Ddfix_* functions exported)");
	return true;
}

void ScriptApi::UnregisterAll()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_registered)
	{
		LOG_WARN("ScriptApi::UnregisterAll: not registered, ignoring");
		return;
	}
	m_registered = false;
	LOG_INFO("ScriptApi::UnregisterAll: ok");
}

// -------------------- 渲染管线 --------------------

void ScriptApi::SetRenderEnabled(bool enabled)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_renderEnabled = enabled;
}

bool ScriptApi::IsRenderEnabled() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_renderEnabled;
}

void ScriptApi::ChangeRenderConfig(const std::string& sectionName)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (sectionName.empty())
	{
		// 空名 = 重置为 Default（与 ConfigManager 行为一致）
		m_currentConfig = "Default";
		return;
	}
	m_currentConfig = sectionName;
}

std::string ScriptApi::GetCurrentRenderConfig() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_currentConfig;
}

// -------------------- 环境/雾 --------------------

void ScriptApi::SetAmbientColor(float r, float g, float b)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_ambientColor.r = r;
	m_ambientColor.g = g;
	m_ambientColor.b = b;
}

void ScriptApi::GetAmbientColor(float* r, float* g, float* b) const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (r) *r = m_ambientColor.r;
	if (g) *g = m_ambientColor.g;
	if (b) *b = m_ambientColor.b;
}

void ScriptApi::SetFogDistance(float nearDist, float farDist)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_fogNear = nearDist;
	m_fogFar = farDist;
}

void ScriptApi::GetFogDistance(float* nearDist, float* farDist) const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (nearDist) *nearDist = m_fogNear;
	if (farDist)  *farDist  = m_fogFar;
}

void ScriptApi::SetFogHeight(float top, float bottom)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_fogTop = top;
	m_fogBottom = bottom;
}

void ScriptApi::GetFogHeight(float* top, float* bottom) const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (top)    *top    = m_fogTop;
	if (bottom) *bottom = m_fogBottom;
}

void ScriptApi::SetFogColor(float r, float g, float b)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_fogColor.r = r;
	m_fogColor.g = g;
	m_fogColor.b = b;
}

void ScriptApi::GetFogColor(float* r, float* g, float* b) const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (r) *r = m_fogColor.r;
	if (g) *g = m_fogColor.g;
	if (b) *b = m_fogColor.b;
}

// -------------------- 光源控制 --------------------

void ScriptApi::SelectLight(const std::string& lightName)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (lightName.empty())
	{
		m_selectedLight = "MainLight";
		return;
	}
	m_selectedLight = lightName;
}

std::string ScriptApi::GetSelectedLight() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_selectedLight;
}

void ScriptApi::SetLightEnabled(bool enabled)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_lightEnabled = enabled;
}

bool ScriptApi::IsLightEnabled() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_lightEnabled;
}

void ScriptApi::SetLightPosition(float x, float y, float z)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_lightPos.x = x;
	m_lightPos.y = y;
	m_lightPos.z = z;
}

void ScriptApi::GetLightPosition(float* x, float* y, float* z) const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (x) *x = m_lightPos.x;
	if (y) *y = m_lightPos.y;
	if (z) *z = m_lightPos.z;
}

void ScriptApi::SetLightRotationArc(float x, float y, float z)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_lightRot.x = x;
	m_lightRot.y = y;
	m_lightRot.z = z;
}

void ScriptApi::GetLightRotationArc(float* x, float* y, float* z) const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (x) *x = m_lightRot.x;
	if (y) *y = m_lightRot.y;
	if (z) *z = m_lightRot.z;
}

void ScriptApi::SetLightColor(float r, float g, float b)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_lightColor.r = r;
	m_lightColor.g = g;
	m_lightColor.b = b;
}

void ScriptApi::GetLightColor(float* r, float* g, float* b) const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (r) *r = m_lightColor.r;
	if (g) *g = m_lightColor.g;
	if (b) *b = m_lightColor.b;
}

void ScriptApi::SetLightBrightness(float brightness)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_lightBrightness = brightness;
}

float ScriptApi::GetLightBrightness() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_lightBrightness;
}

void ScriptApi::SetLightShadowStrength(float strength)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_lightShadowStrength = strength;
}

float ScriptApi::GetLightShadowStrength() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_lightShadowStrength;
}

// -------------------- UI --------------------

void ScriptApi::SetArenaUIVisible(bool visible)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_arenaUIVisible = visible;
}

bool ScriptApi::IsArenaUIVisible() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_arenaUIVisible;
}

// -------------------- 数学 --------------------

void ScriptApi::WorldToScreenPoint(float /*wx*/, float /*wy*/, float /*wz*/,
                                   float* sx, float* sy) const
{
	// 无视口上下文时返 (0, 0)
	// 真实投影留 Phase 9.7+ 集成 D3D9Context 后实现
	if (sx) *sx = 0.0f;
	if (sy) *sy = 0.0f;
}

// -------------------- 回调 --------------------

void ScriptApi::SetCharacterHitCallback(void* callback)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_hitCallback = callback;
}

void* ScriptApi::GetCharacterHitCallback() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_hitCallback;
}

// -------------------- 测试用 --------------------

void ScriptApi::ResetForTest()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_renderEnabled = true;
	m_currentConfig = "Default";
	m_ambientColor = {0.2f, 0.2f, 0.2f};
	m_fogNear = 100.0f;
	m_fogFar = 1000.0f;
	m_fogTop = 1000.0f;
	m_fogBottom = 0.0f;
	m_fogColor = {0.5f, 0.5f, 0.5f};
	m_selectedLight = "MainLight";
	m_lightEnabled = true;
	m_lightPos = {0.0f, 50.0f, 0.0f};
	m_lightRot = {0.0f, 0.0f, 0.0f};
	m_lightColor = {1.0f, 1.0f, 1.0f};
	m_lightBrightness = 1.0f;
	m_lightShadowStrength = 0.5f;
	m_arenaUIVisible = true;
	m_hitCallback = nullptr;
	// 不重置 m_registered — 测试可独立验证 RegisterAll/UnregisterAll 幂等
}

} // namespace Script
} // namespace NDDFIX

// --------------------------------------------------------
// Phase 9.7 build fix: extern "C" 块内嵌套 namespace 块会引起 MSVC
//   解析错 (C2653 "ScriptApi: 不是类或命名空间名称" 等). 把需要在
//   extern "C" 块外访问的辅助函数 (ScriptApi_Get*_Raw) 提到 namespace
//   块内, extern "C" 块内只剩纯转调.
// --------------------------------------------------------
// ScriptApi 单例内 m_currentConfig / m_selectedLight 是永生 std::string，
// 调 c_str() 返的指针在单例生命周期内稳定。
// AHK 端按 DllCall 文档建议立即 StrVarSetCapacity 复制，详见
// https://www.autohotkey.com/docs/commands/DllCall.htm

namespace NDDFIX { namespace Script {

const char* ScriptApi_GetCurrentRenderConfig_Raw()
{
	auto* api = ScriptApi::Instance();
	std::lock_guard<std::mutex> lock(api->m_mutex);
	return api->m_currentConfig.c_str();
}

const char* ScriptApi_GetSelectedLight_Raw()
{
	auto* api = ScriptApi::Instance();
	std::lock_guard<std::mutex> lock(api->m_mutex);
	return api->m_selectedLight.c_str();
}

}} // namespace NDDFIX::Script


// ============================================================
// 16 个 C-linkage __stdcall 导出函数
// ============================================================
//
// 命名规范：Ddfix_<Verb><Noun>
//  - Set / Get / Is 前缀，参考 Win32 API（SetWindowText / GetWindowText / IsWindow）
//  - bool 类型在 stdcall 边界用 int（4 字节），参考 Win32 BOOL = int
//  - 字符串参数用 const char*，内部立刻构造 std::string（生命周期延长）
//
// 调用约定：__stdcall（WINAPI）
//  - AHK v1 DllCall 默认 stdcall，所以脚本端不需要指定 "Cdecl"
//  - x64 下 __stdcall 与 __cdecl 等价（都走 Microsoft x64 calling convention）
//  - 公开文献：https://www.autohotkey.com/docs/commands/DllCall.htm
//
// 链接导出：
//  - MSVC 默认按函数名导出（无 Name Decoration on stdcall x86）
//    但 x86 stdcall 会被 name-decorate 成 _Ddfix_Func@N（N = 参数字节数）
//  - 用 .def 文件（EXPORTS 节）显式声明无装饰名，确保 AHK 端 DllCall 用 "Ddfix_Func"
//  - 当前 exports.def 暂未列出这些符号，AHK 端 DllCall 需要按 name-mangling 调
//    （Phase 9.7+ 收尾时由维护者把 16 个名字加到 exports.def）

extern "C" {

// -------- 渲染管线 --------
__declspec(dllexport) void __stdcall Ddfix_SetRenderEnabled(int enabled)
{
	NDDFIX::Script::ScriptApi::Instance()->SetRenderEnabled(enabled != 0);
}

__declspec(dllexport) int __stdcall Ddfix_IsRenderEnabled()
{
	return NDDFIX::Script::ScriptApi::Instance()->IsRenderEnabled() ? 1 : 0;
}

__declspec(dllexport) void __stdcall Ddfix_ChangeRenderConfig(const char* sectionName)
{
	// 立即构造 std::string，sectionName 生命周期由 std::string 接管
	std::string s = sectionName ? std::string(sectionName) : std::string();
	NDDFIX::Script::ScriptApi::Instance()->ChangeRenderConfig(s);
}

__declspec(dllexport) const char* __stdcall Ddfix_GetCurrentRenderConfig()
{
	// WHY: 临时 std::string 析构后 c_str() 失效, 必须直接访问单例内 c_str().
	//   实际取数在 namespace Script::ScriptApi_GetCurrentRenderConfig_Raw() 内做 (可访问 private).
	return NDDFIX::Script::ScriptApi_GetCurrentRenderConfig_Raw();
}

// -------- 环境/雾 --------
__declspec(dllexport) void __stdcall Ddfix_SetAmbientColor(float r, float g, float b)
{
	NDDFIX::Script::ScriptApi::Instance()->SetAmbientColor(r, g, b);
}

__declspec(dllexport) void __stdcall Ddfix_GetAmbientColor(float* r, float* g, float* b)
{
	NDDFIX::Script::ScriptApi::Instance()->GetAmbientColor(r, g, b);
}

__declspec(dllexport) void __stdcall Ddfix_SetFogDistance(float nearDist, float farDist)
{
	NDDFIX::Script::ScriptApi::Instance()->SetFogDistance(nearDist, farDist);
}

__declspec(dllexport) void __stdcall Ddfix_GetFogDistance(float* nearDist, float* farDist)
{
	NDDFIX::Script::ScriptApi::Instance()->GetFogDistance(nearDist, farDist);
}

__declspec(dllexport) void __stdcall Ddfix_SetFogHeight(float top, float bottom)
{
	NDDFIX::Script::ScriptApi::Instance()->SetFogHeight(top, bottom);
}

__declspec(dllexport) void __stdcall Ddfix_GetFogHeight(float* top, float* bottom)
{
	NDDFIX::Script::ScriptApi::Instance()->GetFogHeight(top, bottom);
}

__declspec(dllexport) void __stdcall Ddfix_SetFogColor(float r, float g, float b)
{
	NDDFIX::Script::ScriptApi::Instance()->SetFogColor(r, g, b);
}

__declspec(dllexport) void __stdcall Ddfix_GetFogColor(float* r, float* g, float* b)
{
	NDDFIX::Script::ScriptApi::Instance()->GetFogColor(r, g, b);
}

// -------- 光源控制 --------
__declspec(dllexport) void __stdcall Ddfix_SelectLight(const char* lightName)
{
	std::string s = lightName ? std::string(lightName) : std::string();
	NDDFIX::Script::ScriptApi::Instance()->SelectLight(s);
}

__declspec(dllexport) const char* __stdcall Ddfix_GetSelectedLight()
{
	// 与 Ddfix_GetCurrentRenderConfig 同因: 实际取数在 namespace ScriptApi 内.
	return NDDFIX::Script::ScriptApi_GetSelectedLight_Raw();
}

__declspec(dllexport) void __stdcall Ddfix_SetLightEnabled(int enabled)
{
	NDDFIX::Script::ScriptApi::Instance()->SetLightEnabled(enabled != 0);
}

__declspec(dllexport) int __stdcall Ddfix_IsLightEnabled()
{
	return NDDFIX::Script::ScriptApi::Instance()->IsLightEnabled() ? 1 : 0;
}

__declspec(dllexport) void __stdcall Ddfix_SetLightPosition(float x, float y, float z)
{
	NDDFIX::Script::ScriptApi::Instance()->SetLightPosition(x, y, z);
}

__declspec(dllexport) void __stdcall Ddfix_GetLightPosition(float* x, float* y, float* z)
{
	NDDFIX::Script::ScriptApi::Instance()->GetLightPosition(x, y, z);
}

__declspec(dllexport) void __stdcall Ddfix_SetLightRotationArc(float x, float y, float z)
{
	NDDFIX::Script::ScriptApi::Instance()->SetLightRotationArc(x, y, z);
}

__declspec(dllexport) void __stdcall Ddfix_GetLightRotationArc(float* x, float* y, float* z)
{
	NDDFIX::Script::ScriptApi::Instance()->GetLightRotationArc(x, y, z);
}

__declspec(dllexport) void __stdcall Ddfix_SetLightColor(float r, float g, float b)
{
	NDDFIX::Script::ScriptApi::Instance()->SetLightColor(r, g, b);
}

__declspec(dllexport) void __stdcall Ddfix_GetLightColor(float* r, float* g, float* b)
{
	NDDFIX::Script::ScriptApi::Instance()->GetLightColor(r, g, b);
}

__declspec(dllexport) void __stdcall Ddfix_SetLightBrightness(float brightness)
{
	NDDFIX::Script::ScriptApi::Instance()->SetLightBrightness(brightness);
}

__declspec(dllexport) float __stdcall Ddfix_GetLightBrightness()
{
	return NDDFIX::Script::ScriptApi::Instance()->GetLightBrightness();
}

__declspec(dllexport) void __stdcall Ddfix_SetLightShadowStrength(float strength)
{
	NDDFIX::Script::ScriptApi::Instance()->SetLightShadowStrength(strength);
}

__declspec(dllexport) float __stdcall Ddfix_GetLightShadowStrength()
{
	return NDDFIX::Script::ScriptApi::Instance()->GetLightShadowStrength();
}

// -------- UI --------
__declspec(dllexport) void __stdcall Ddfix_SetArenaUIVisible(int visible)
{
	NDDFIX::Script::ScriptApi::Instance()->SetArenaUIVisible(visible != 0);
}

__declspec(dllexport) int __stdcall Ddfix_IsArenaUIVisible()
{
	return NDDFIX::Script::ScriptApi::Instance()->IsArenaUIVisible() ? 1 : 0;
}

// -------- 数学 --------
__declspec(dllexport) void __stdcall Ddfix_WorldToScreenPoint(float wx, float wy, float wz,
                                                              float* sx, float* sy)
{
	NDDFIX::Script::ScriptApi::Instance()->WorldToScreenPoint(wx, wy, wz, sx, sy);
}

// -------- 回调 --------
__declspec(dllexport) void __stdcall Ddfix_SetCharacterHitCallback(void* callback)
{
	NDDFIX::Script::ScriptApi::Instance()->SetCharacterHitCallback(callback);
}

__declspec(dllexport) void* __stdcall Ddfix_GetCharacterHitCallback()
{
	return NDDFIX::Script::ScriptApi::Instance()->GetCharacterHitCallback();
}

} // extern "C"
