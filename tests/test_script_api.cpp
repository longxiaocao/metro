// test_script_api.cpp - Phase 9.7 ScriptApi 单元测试
//
// 覆盖范围（Task 9.7.4）：
//   1-16. 16 个 setter/getter round-trip
//   17.   16 个 C-linkage __stdcall Ddfix_* 函数存在性（链接器 unresolved-external 验证）
//   18.   RegisterAll / UnregisterAll 幂等性
//   19.   ResetForTest 状态归零
//   20.   多线程并发读写（基本 sanity）
//
// 不覆盖：
//   - 真实渲染管线集成（依赖 GBuffer/Deferred 完整管线）
//   - AutoHotkey 端 DllCall 验证（端到端测试范围）
//   - exports.def 符号导出（需要 link ddfix.dll + dumpbin，集成测试）

#include "SingleTest.h"

#include "../ddfix/ddraw/ScriptApi.h"

// 包含 ScriptApi.cpp 内部的 16 个 C-linkage __stdcall 导出声明。
// ScriptApi.cpp 已经是 ddfix-static 的一部分（见 ddfix/CMakeLists.txt），
// 所以这里直接 extern "C" 引用即可让链接器验证函数存在。
extern "C" {
	void __stdcall Ddfix_SetRenderEnabled(int enabled);
	int  __stdcall Ddfix_IsRenderEnabled();
	void __stdcall Ddfix_ChangeRenderConfig(const char* sectionName);
	const char* __stdcall Ddfix_GetCurrentRenderConfig();

	void __stdcall Ddfix_SetAmbientColor(float r, float g, float b);
	void __stdcall Ddfix_GetAmbientColor(float* r, float* g, float* b);

	void __stdcall Ddfix_SetFogDistance(float nearDist, float farDist);
	void __stdcall Ddfix_GetFogDistance(float* nearDist, float* farDist);

	void __stdcall Ddfix_SetFogHeight(float top, float bottom);
	void __stdcall Ddfix_GetFogHeight(float* top, float* bottom);

	void __stdcall Ddfix_SetFogColor(float r, float g, float b);
	void __stdcall Ddfix_GetFogColor(float* r, float* g, float* b);

	void __stdcall Ddfix_SelectLight(const char* lightName);
	const char* __stdcall Ddfix_GetSelectedLight();

	void __stdcall Ddfix_SetLightEnabled(int enabled);
	int  __stdcall Ddfix_IsLightEnabled();

	void __stdcall Ddfix_SetLightPosition(float x, float y, float z);
	void __stdcall Ddfix_GetLightPosition(float* x, float* y, float* z);

	void __stdcall Ddfix_SetLightRotationArc(float x, float y, float z);
	void __stdcall Ddfix_GetLightRotationArc(float* x, float* y, float* z);

	void __stdcall Ddfix_SetLightColor(float r, float g, float b);
	void __stdcall Ddfix_GetLightColor(float* r, float* g, float* b);

	void __stdcall Ddfix_SetLightBrightness(float brightness);
	float __stdcall Ddfix_GetLightBrightness();

	void __stdcall Ddfix_SetLightShadowStrength(float strength);
	float __stdcall Ddfix_GetLightShadowStrength();

	void __stdcall Ddfix_SetArenaUIVisible(int visible);
	int  __stdcall Ddfix_IsArenaUIVisible();

	void __stdcall Ddfix_WorldToScreenPoint(float wx, float wy, float wz,
	                                        float* sx, float* sy);

	void __stdcall Ddfix_SetCharacterHitCallback(void* callback);
	void* __stdcall Ddfix_GetCharacterHitCallback();
}

#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

namespace
{

using NDDFIX::Script::ScriptApi;

// 工具：每个 TEST 之前重置 ScriptApi 状态
struct ResetGuard
{
	ResetGuard() { ScriptApi::Instance()->ResetForTest(); }
};

} // anonymous namespace

// ============================================================
// 1. 渲染管线开关
// ============================================================
TEST(ScriptApi, RenderEnabledRoundTrip)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	EXPECT_TRUE(api->IsRenderEnabled());      // 默认 true
	api->SetRenderEnabled(false);
	EXPECT_FALSE(api->IsRenderEnabled());
	api->SetRenderEnabled(true);
	EXPECT_TRUE(api->IsRenderEnabled());
}

// ============================================================
// 2. Render Config 切换
// ============================================================
TEST(ScriptApi, ChangeRenderConfig)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	EXPECT_EQ(api->GetCurrentRenderConfig(), "Default");
	api->ChangeRenderConfig("Indoor");
	EXPECT_EQ(api->GetCurrentRenderConfig(), "Indoor");
	api->ChangeRenderConfig("");
	// WHY: 空名按设计 = 重置为 Default（与 ConfigManager 行为一致）
	EXPECT_EQ(api->GetCurrentRenderConfig(), "Default");
}

// ============================================================
// 3. Ambient Color
// ============================================================
TEST(ScriptApi, AmbientColorRoundTrip)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	float r = 0, g = 0, b = 0;
	api->GetAmbientColor(&r, &g, &b);
	EXPECT_TRUE(std::fabs(r - 0.2f) < 1e-6f);
	EXPECT_TRUE(std::fabs(g - 0.2f) < 1e-6f);
	EXPECT_TRUE(std::fabs(b - 0.2f) < 1e-6f);

	api->SetAmbientColor(0.5f, 0.6f, 0.7f);
	api->GetAmbientColor(&r, &g, &b);
	EXPECT_TRUE(std::fabs(r - 0.5f) < 1e-6f);
	EXPECT_TRUE(std::fabs(g - 0.6f) < 1e-6f);
	EXPECT_TRUE(std::fabs(b - 0.7f) < 1e-6f);

	// nullptr 安全
	api->GetAmbientColor(nullptr, nullptr, nullptr);  // 不崩
	EXPECT_TRUE(true);
}

// ============================================================
// 4. Fog Distance
// ============================================================
TEST(ScriptApi, FogDistanceRoundTrip)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	float n = 0, f = 0;
	api->GetFogDistance(&n, &f);
	EXPECT_TRUE(std::fabs(n - 100.0f) < 1e-3f);
	EXPECT_TRUE(std::fabs(f - 1000.0f) < 1e-3f);

	api->SetFogDistance(50.0f, 500.0f);
	api->GetFogDistance(&n, &f);
	EXPECT_TRUE(std::fabs(n - 50.0f) < 1e-3f);
	EXPECT_TRUE(std::fabs(f - 500.0f) < 1e-3f);
}

// ============================================================
// 5. Fog Height
// ============================================================
TEST(ScriptApi, FogHeightRoundTrip)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	float t = 0, b = 0;
	api->GetFogHeight(&t, &b);
	EXPECT_TRUE(std::fabs(t - 1000.0f) < 1e-3f);
	EXPECT_TRUE(std::fabs(b - 0.0f)   < 1e-3f);

	api->SetFogHeight(800.0f, -100.0f);
	api->GetFogHeight(&t, &b);
	EXPECT_TRUE(std::fabs(t - 800.0f)  < 1e-3f);
	EXPECT_TRUE(std::fabs(b - -100.0f) < 1e-3f);
}

// ============================================================
// 6. Fog Color
// ============================================================
TEST(ScriptApi, FogColorRoundTrip)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	float r = 0, g = 0, b = 0;
	api->GetFogColor(&r, &g, &b);
	EXPECT_TRUE(std::fabs(r - 0.5f) < 1e-6f);
	EXPECT_TRUE(std::fabs(g - 0.5f) < 1e-6f);
	EXPECT_TRUE(std::fabs(b - 0.5f) < 1e-6f);

	api->SetFogColor(0.1f, 0.2f, 0.3f);
	api->GetFogColor(&r, &g, &b);
	EXPECT_TRUE(std::fabs(r - 0.1f) < 1e-6f);
	EXPECT_TRUE(std::fabs(g - 0.2f) < 1e-6f);
	EXPECT_TRUE(std::fabs(b - 0.3f) < 1e-6f);
}

// ============================================================
// 7. Select Light
// ============================================================
TEST(ScriptApi, SelectLight)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	EXPECT_EQ(api->GetSelectedLight(), "MainLight");
	api->SelectLight("FillLight");
	EXPECT_EQ(api->GetSelectedLight(), "FillLight");
	api->SelectLight("");
	EXPECT_EQ(api->GetSelectedLight(), "MainLight");
}

// ============================================================
// 8. Light Enabled
// ============================================================
TEST(ScriptApi, LightEnabledRoundTrip)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	EXPECT_TRUE(api->IsLightEnabled());
	api->SetLightEnabled(false);
	EXPECT_FALSE(api->IsLightEnabled());
	api->SetLightEnabled(true);
	EXPECT_TRUE(api->IsLightEnabled());
}

// ============================================================
// 9. Light Position
// ============================================================
TEST(ScriptApi, LightPositionRoundTrip)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	float x = 0, y = 0, z = 0;
	api->GetLightPosition(&x, &y, &z);
	EXPECT_TRUE(std::fabs(x - 0.0f)  < 1e-3f);
	EXPECT_TRUE(std::fabs(y - 50.0f) < 1e-3f);
	EXPECT_TRUE(std::fabs(z - 0.0f)  < 1e-3f);

	api->SetLightPosition(10.0f, 20.0f, 30.0f);
	api->GetLightPosition(&x, &y, &z);
	EXPECT_TRUE(std::fabs(x - 10.0f) < 1e-3f);
	EXPECT_TRUE(std::fabs(y - 20.0f) < 1e-3f);
	EXPECT_TRUE(std::fabs(z - 30.0f) < 1e-3f);
}

// ============================================================
// 10. Light Rotation Arc
// ============================================================
TEST(ScriptApi, LightRotationArcRoundTrip)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	float x = 1, y = 1, z = 1;
	api->GetLightRotationArc(&x, &y, &z);
	EXPECT_TRUE(std::fabs(x - 0.0f) < 1e-3f);
	EXPECT_TRUE(std::fabs(y - 0.0f) < 1e-3f);
	EXPECT_TRUE(std::fabs(z - 0.0f) < 1e-3f);

	api->SetLightRotationArc(1.5f, 2.5f, 3.5f);
	api->GetLightRotationArc(&x, &y, &z);
	EXPECT_TRUE(std::fabs(x - 1.5f) < 1e-3f);
	EXPECT_TRUE(std::fabs(y - 2.5f) < 1e-3f);
	EXPECT_TRUE(std::fabs(z - 3.5f) < 1e-3f);
}

// ============================================================
// 11. Light Color
// ============================================================
TEST(ScriptApi, LightColorRoundTrip)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	float r = 0, g = 0, b = 0;
	api->GetLightColor(&r, &g, &b);
	EXPECT_TRUE(std::fabs(r - 1.0f) < 1e-6f);
	EXPECT_TRUE(std::fabs(g - 1.0f) < 1e-6f);
	EXPECT_TRUE(std::fabs(b - 1.0f) < 1e-6f);

	api->SetLightColor(0.2f, 0.4f, 0.6f);
	api->GetLightColor(&r, &g, &b);
	EXPECT_TRUE(std::fabs(r - 0.2f) < 1e-6f);
	EXPECT_TRUE(std::fabs(g - 0.4f) < 1e-6f);
	EXPECT_TRUE(std::fabs(b - 0.6f) < 1e-6f);
}

// ============================================================
// 12. Light Brightness
// ============================================================
TEST(ScriptApi, LightBrightnessRoundTrip)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	EXPECT_TRUE(std::fabs(api->GetLightBrightness() - 1.0f) < 1e-6f);
	api->SetLightBrightness(2.5f);
	EXPECT_TRUE(std::fabs(api->GetLightBrightness() - 2.5f) < 1e-6f);
}

// ============================================================
// 13. Light Shadow Strength
// ============================================================
TEST(ScriptApi, LightShadowStrengthRoundTrip)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	EXPECT_TRUE(std::fabs(api->GetLightShadowStrength() - 0.5f) < 1e-6f);
	api->SetLightShadowStrength(0.85f);
	EXPECT_TRUE(std::fabs(api->GetLightShadowStrength() - 0.85f) < 1e-6f);
}

// ============================================================
// 14. Arena UI Visible
// ============================================================
TEST(ScriptApi, ArenaUIVisibleRoundTrip)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	EXPECT_TRUE(api->IsArenaUIVisible());
	api->SetArenaUIVisible(false);
	EXPECT_FALSE(api->IsArenaUIVisible());
	api->SetArenaUIVisible(true);
	EXPECT_TRUE(api->IsArenaUIVisible());
}

// ============================================================
// 15. WorldToScreenPoint（无视口上下文时返 (0,0)）
// ============================================================
TEST(ScriptApi, WorldToScreenPointNoViewport)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	float sx = -1, sy = -1;
	api->WorldToScreenPoint(100.0f, 200.0f, 300.0f, &sx, &sy);
	EXPECT_TRUE(std::fabs(sx - 0.0f) < 1e-6f);
	EXPECT_TRUE(std::fabs(sy - 0.0f) < 1e-6f);

	// nullptr 安全
	api->WorldToScreenPoint(0, 0, 0, nullptr, nullptr);
	EXPECT_TRUE(true);
}

// ============================================================
// 16. Character Hit Callback
// ============================================================
TEST(ScriptApi, CharacterHitCallbackRoundTrip)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	EXPECT_TRUE(api->GetCharacterHitCallback() == nullptr);

	int dummy = 42;
	api->SetCharacterHitCallback(&dummy);
	EXPECT_TRUE(api->GetCharacterHitCallback() == &dummy);

	api->SetCharacterHitCallback(nullptr);
	EXPECT_TRUE(api->GetCharacterHitCallback() == nullptr);
}

// ============================================================
// 17. 16 个 C-linkage __stdcall 函数存在性
//
// 链接器验证：上面 extern "C" 声明 + ScriptApi.cpp 内的 __declspec(dllexport) 定义
//   → 如果 ScriptApi.cpp 漏掉某个 Ddfix_*, 链接时 LNK2019 unresolved external 失败.
// 运行时验证：每个函数都跑一次 round-trip，确保 __stdcall 调用约定正确（参数传递无错位）。
// ============================================================
TEST(ScriptApi, CLinkageExportsExist)
{
	ResetGuard _;
	// 渲染管线
	Ddfix_SetRenderEnabled(0);
	EXPECT_EQ(Ddfix_IsRenderEnabled(), 0);
	Ddfix_SetRenderEnabled(1);
	EXPECT_EQ(Ddfix_IsRenderEnabled(), 1);

	Ddfix_ChangeRenderConfig("TestSection");
	const char* cfg = Ddfix_GetCurrentRenderConfig();
	EXPECT_TRUE(cfg != nullptr);
	EXPECT_TRUE(std::string(cfg) == "TestSection");

	// 环境/雾
	Ddfix_SetAmbientColor(0.11f, 0.22f, 0.33f);
	float ar = 0, ag = 0, ab = 0;
	Ddfix_GetAmbientColor(&ar, &ag, &ab);
	EXPECT_TRUE(std::fabs(ar - 0.11f) < 1e-6f);
	EXPECT_TRUE(std::fabs(ag - 0.22f) < 1e-6f);
	EXPECT_TRUE(std::fabs(ab - 0.33f) < 1e-6f);

	Ddfix_SetFogDistance(33.0f, 333.0f);
	float fn = 0, ff = 0;
	Ddfix_GetFogDistance(&fn, &ff);
	EXPECT_TRUE(std::fabs(fn - 33.0f)  < 1e-3f);
	EXPECT_TRUE(std::fabs(ff - 333.0f) < 1e-3f);

	Ddfix_SetFogHeight(777.0f, -77.0f);
	float ft = 0, fb = 0;
	Ddfix_GetFogHeight(&ft, &fb);
	EXPECT_TRUE(std::fabs(ft - 777.0f) < 1e-3f);
	EXPECT_TRUE(std::fabs(fb - -77.0f) < 1e-3f);

	Ddfix_SetFogColor(0.1f, 0.2f, 0.3f);
	float fr = 0, fg = 0, fcb = 0;
	Ddfix_GetFogColor(&fr, &fg, &fcb);
	EXPECT_TRUE(std::fabs(fr - 0.1f) < 1e-6f);
	EXPECT_TRUE(std::fabs(fg - 0.2f) < 1e-6f);
	EXPECT_TRUE(std::fabs(fcb - 0.3f) < 1e-6f);

	// 光源控制
	Ddfix_SelectLight("TestLight");
	const char* sl = Ddfix_GetSelectedLight();
	EXPECT_TRUE(sl != nullptr);
	EXPECT_TRUE(std::string(sl) == "TestLight");

	Ddfix_SetLightEnabled(0);
	EXPECT_EQ(Ddfix_IsLightEnabled(), 0);
	Ddfix_SetLightEnabled(1);
	EXPECT_EQ(Ddfix_IsLightEnabled(), 1);

	Ddfix_SetLightPosition(11.0f, 22.0f, 33.0f);
	float lx = 0, ly = 0, lz = 0;
	Ddfix_GetLightPosition(&lx, &ly, &lz);
	EXPECT_TRUE(std::fabs(lx - 11.0f) < 1e-3f);
	EXPECT_TRUE(std::fabs(ly - 22.0f) < 1e-3f);
	EXPECT_TRUE(std::fabs(lz - 33.0f) < 1e-3f);

	Ddfix_SetLightRotationArc(0.1f, 0.2f, 0.3f);
	float rxa = 0, rya = 0, rza = 0;
	Ddfix_GetLightRotationArc(&rxa, &rya, &rza);
	EXPECT_TRUE(std::fabs(rxa - 0.1f) < 1e-3f);
	EXPECT_TRUE(std::fabs(rya - 0.2f) < 1e-3f);
	EXPECT_TRUE(std::fabs(rza - 0.3f) < 1e-3f);

	Ddfix_SetLightColor(0.4f, 0.5f, 0.6f);
	float lcr = 0, lcg = 0, lcb = 0;
	Ddfix_GetLightColor(&lcr, &lcg, &lcb);
	EXPECT_TRUE(std::fabs(lcr - 0.4f) < 1e-6f);
	EXPECT_TRUE(std::fabs(lcg - 0.5f) < 1e-6f);
	EXPECT_TRUE(std::fabs(lcb - 0.6f) < 1e-6f);

	Ddfix_SetLightBrightness(1.75f);
	EXPECT_TRUE(std::fabs(Ddfix_GetLightBrightness() - 1.75f) < 1e-6f);

	Ddfix_SetLightShadowStrength(0.95f);
	EXPECT_TRUE(std::fabs(Ddfix_GetLightShadowStrength() - 0.95f) < 1e-6f);

	// UI
	Ddfix_SetArenaUIVisible(0);
	EXPECT_EQ(Ddfix_IsArenaUIVisible(), 0);
	Ddfix_SetArenaUIVisible(1);
	EXPECT_EQ(Ddfix_IsArenaUIVisible(), 1);

	// 数学
	Ddfix_WorldToScreenPoint(1.0f, 2.0f, 3.0f, &lx, &ly);
	EXPECT_TRUE(std::fabs(lx - 0.0f) < 1e-6f);
	EXPECT_TRUE(std::fabs(ly - 0.0f) < 1e-6f);

	// 回调
	int cb = 99;
	Ddfix_SetCharacterHitCallback(&cb);
	void* got = Ddfix_GetCharacterHitCallback();
	EXPECT_TRUE(got == &cb);
	Ddfix_SetCharacterHitCallback(nullptr);
	EXPECT_TRUE(Ddfix_GetCharacterHitCallback() == nullptr);
}

// ============================================================
// 18. RegisterAll / UnregisterAll 幂等性
// ============================================================
TEST(ScriptApi, RegisterUnregisterIdempotent)
{
	auto* api = ScriptApi::Instance();

	// 不管之前状态如何, 先反注册
	api->UnregisterAll();
	EXPECT_FALSE(api->IsRegistered());

	// 第一次 Register: 成功
	EXPECT_TRUE(api->RegisterAll());
	EXPECT_TRUE(api->IsRegistered());

	// 重复 Register: 幂等返 false, 状态不变
	EXPECT_FALSE(api->RegisterAll());
	EXPECT_TRUE(api->IsRegistered());

	// Unregister: 成功
	api->UnregisterAll();
	EXPECT_FALSE(api->IsRegistered());

	// 重复 Unregister: 幂等无操作
	api->UnregisterAll();
	EXPECT_FALSE(api->IsRegistered());

	// 恢复: 测试结束后 ScriptApi 保持 registered = false (与初始一致)
}

// ============================================================
// 19. ResetForTest 状态归零
// ============================================================
TEST(ScriptApi, ResetForTestRestoresDefaults)
{
	auto* api = ScriptApi::Instance();
	// 改一波
	api->SetRenderEnabled(false);
	api->SetLightBrightness(99.0f);
	api->SelectLight("ZZZ");
	api->SetCharacterHitCallback(reinterpret_cast<void*>(0xDEADBEEF));

	// 重置
	api->ResetForTest();

	// 校验
	EXPECT_TRUE(api->IsRenderEnabled());
	EXPECT_TRUE(std::fabs(api->GetLightBrightness() - 1.0f) < 1e-6f);
	EXPECT_EQ(api->GetSelectedLight(), "MainLight");
	EXPECT_TRUE(api->GetCharacterHitCallback() == nullptr);
	EXPECT_EQ(api->GetCurrentRenderConfig(), "Default");
	EXPECT_TRUE(api->IsArenaUIVisible());
	EXPECT_TRUE(api->IsLightEnabled());
}

// ============================================================
// 20. 多线程并发读写（基本 sanity）
// ============================================================
TEST(ScriptApi, MultiThreadConcurrentAccess)
{
	ResetGuard _;
	auto* api = ScriptApi::Instance();
	const int kIterations = 1000;
	std::atomic<int> errors{0};

	// 写线程 1
	std::thread t1([&]() {
		for (int i = 0; i < kIterations; ++i) {
			api->SetLightBrightness(static_cast<float>(i));
		}
	});

	// 写线程 2
	std::thread t2([&]() {
		for (int i = 0; i < kIterations; ++i) {
			api->SetLightColor(
				static_cast<float>(i) / 100.0f,
				static_cast<float>(i) / 200.0f,
				static_cast<float>(i) / 300.0f);
		}
	});

	// 读线程
	std::thread t3([&]() {
		for (int i = 0; i < kIterations; ++i) {
			float b = api->GetLightBrightness();
			float r = 0, g = 0, cb = 0;
			api->GetLightColor(&r, &g, &cb);
			// 简单 sanity: 数值必须是有限 float（不是 NaN / Inf）
			if (!(b == b) || !(r == r) || !(g == g) || !(cb == cb)) {
				++errors;
			}
			if (b < 0.0f || b > 10000.0f) ++errors;  // 越界
		}
	});

	t1.join();
	t2.join();
	t3.join();

	EXPECT_EQ(errors.load(), 0);
	EXPECT_TRUE(true);  // 跑到这里没崩 = 互斥锁工作
}
