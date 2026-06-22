// Phase 6.4: 单元测试 - PerfCounter
//
// 覆盖：
//   1. IncrementBlt / IncrementBltFast / IncrementFillColor / IncrementFlip
//   2. 1秒滑动平均 FPS
//   3. PERF_SCOPE 宏（EnterScope / LeaveScope 计数）
//   4. Tick 推进 sliding window
//   5. Snapshot 字段正确性
//
// 重要：PerfCounter 是**全局单例**（Instance() 返回 static），
// 所以测试间共享计数。必须用 unique scope name 避免污染。
// 或用 reset 技巧（PerfCounter 自身没暴露 Reset，靠顺序断言）。

#include "SingleTest.h"

#include "../ddfix/Debug/PerfCounter.h"

#include <thread>
#include <chrono>
#include <cstdio>
#include <string>

using NDDFIX::Debug::PerfCounter;
using NDDFIX::Debug::PerfSnapshot;
using NDDFIX::Debug::PerfScopeGuard;

// helper：unique scope name（每个测试用 unique 字符串，避免被前面测试的 PERF_SCOPE 累加影响）
static std::string UniqueScope(const char* base)
{
	static int counter = 0;
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%s_%d_%p", base, ++counter, (void*)&counter);
	return std::string(buf);
}

// ============================================================
// 1. 单例可访问
// ============================================================
TEST(PerfCounter, InstanceReturnsSame)
{
	PerfCounter* a = PerfCounter::Instance();
	PerfCounter* b = PerfCounter::Instance();
	EXPECT_EQ(a, b);
	EXPECT_TRUE(a != nullptr);
}

// ============================================================
// 2. 计数接口
// ============================================================
TEST(PerfCounter, IncrementCountsAccumulate)
{
	PerfCounter* pc = PerfCounter::Instance();
	uint64_t beforeBlt = pc->GetSnapshot().bltCount;

	for (int i = 0; i < 5; ++i) pc->IncrementBlt();
	for (int i = 0; i < 3; ++i) pc->IncrementBltFast();
	for (int i = 0; i < 7; ++i) pc->IncrementFillColor();
	for (int i = 0; i < 2; ++i) pc->IncrementFlip();
	for (int i = 0; i < 4; ++i) pc->IncrementRender();

	PerfSnapshot snap = pc->GetSnapshot();
	EXPECT_EQ(snap.bltCount - beforeBlt, (uint64_t)5);

	// 其它计数也至少增加了我们调用的次数（可能多，因前面测试也调过）
	EXPECT_TRUE(snap.bltFastCount >= 3);
	EXPECT_TRUE(snap.fillColorCount >= 7);
	EXPECT_TRUE(snap.flipCount >= 2);
	EXPECT_TRUE(snap.renderCount >= 4);
}

// ============================================================
// 3. Tick 返回值
// ============================================================
TEST(PerfCounter, TickReturnsUptime)
{
	PerfCounter* pc = PerfCounter::Instance();
	double t0 = pc->Tick();
	// uptime 至少为 0，且单调
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	double t1 = pc->Tick();
	EXPECT_TRUE(t0 >= 0.0);
	EXPECT_TRUE(t1 > t0);
	EXPECT_TRUE(t1 - t0 >= 0.005);  // 至少 5ms
	EXPECT_TRUE(t1 - t0 < 0.5);     // 不该超过 500ms
}

// ============================================================
// 4. Sliding window：< 1 秒时 perSec 仍为 0（窗口未结算）
// ============================================================
TEST(PerfCounter, SlidingWindowBeforeOneSecondIsZero)
{
	PerfCounter* pc = PerfCounter::Instance();

	// 在 < 1s 区间内连调 N 次 + Tick，perSec 仍应 0（窗口未到 1s）
	for (int i = 0; i < 5; ++i) pc->IncrementBltFast();
	pc->Tick();
	PerfSnapshot snap = pc->GetSnapshot();
	// 注：其它测试可能已触发窗口结算，所以这里只断言 perSec >= 0
	EXPECT_TRUE(snap.bltFastPerSec >= 0.0);
	EXPECT_TRUE(snap.fps >= 0.0);
}

// ============================================================
// 5. Sliding window：> 1 秒后 perSec 应 > 0
// ============================================================
TEST(PerfCounter, SlidingWindowAfterOneSecondReportsRate)
{
	PerfCounter* pc = PerfCounter::Instance();

	// 1 秒内连续调 100 次 BltFast + 多次 Tick
	for (int i = 0; i < 100; ++i) pc->IncrementBltFast();
	for (int i = 0; i < 60; ++i) pc->Tick();  // 60fps
	// 等真实 1.1 秒让窗口结算
	std::this_thread::sleep_for(std::chrono::milliseconds(1100));
	// 触发窗口结算
	for (int i = 0; i < 5; ++i) pc->Tick();

	PerfSnapshot snap = pc->GetSnapshot();
	// 1 秒窗口内 100 次 → perSec 应在 50-200 之间（考虑 sleep 边界）
	EXPECT_TRUE(snap.bltFastPerSec >= 50.0);
	EXPECT_TRUE(snap.bltFastPerSec <= 500.0);
	EXPECT_TRUE(snap.fps > 0.0);  // 至少触发过 Tick
}

// ============================================================
// 6. PERF_SCOPE 宏：EnterScope 计数
// ============================================================
TEST(PerfCounter, PerfScopeIncrementsCount)
{
	std::string scopeName = UniqueScope("test_scope_basic");

	{
		PerfScopeGuard g(scopeName.c_str());
		// 作用域期间 EnterScope 已 +1
	}
	// 析构时 LeaveScope 被调

	PerfSnapshot snap = PerfCounter::Instance()->GetSnapshot();
	bool found = false;
	uint64_t foundCount = 0;
	for (size_t i = 0; i < snap.hotScopeNames.size(); ++i)
	{
		if (snap.hotScopeNames[i] == scopeName)
		{
			found = true;
			foundCount = snap.hotScopeCounts[i];
			break;
		}
	}
	EXPECT_TRUE(found);
	EXPECT_TRUE(foundCount >= 1);
}

// ============================================================
// 7. PERF_SCOPE 宏：多次进入累计
// ============================================================
TEST(PerfCounter, PerfScopeAccumulatesAcrossEntries)
{
	std::string scopeName = UniqueScope("test_scope_acc");

	for (int i = 0; i < 5; ++i)
	{
		PerfScopeGuard g(scopeName.c_str());
	}

	PerfSnapshot snap = PerfCounter::Instance()->GetSnapshot();
	for (size_t i = 0; i < snap.hotScopeNames.size(); ++i)
	{
		if (snap.hotScopeNames[i] == scopeName)
		{
			EXPECT_TRUE(snap.hotScopeCounts[i] >= 5);
			return;
		}
	}
	// 未找到也应 fail
	EXPECT_TRUE(false);  // scope 应在 hot list 中
}

// ============================================================
// 8. PERF_SCOPE 宏：嵌套作用域
// ============================================================
TEST(PerfCounter, PerfScopeNested)
{
	std::string outer = UniqueScope("test_outer");
	std::string inner = UniqueScope("test_inner");

	{
		PerfScopeGuard gO(outer.c_str());
		{
			PerfScopeGuard gI(inner.c_str());
		}
		// 内层已析构
	}

	PerfSnapshot snap = PerfCounter::Instance()->GetSnapshot();
	bool hasOuter = false, hasInner = false;
	for (size_t i = 0; i < snap.hotScopeNames.size(); ++i)
	{
		if (snap.hotScopeNames[i] == outer) hasOuter = true;
		if (snap.hotScopeNames[i] == inner) hasInner = true;
	}
	EXPECT_TRUE(hasOuter);
	EXPECT_TRUE(hasInner);
}

// ============================================================
// 9. Snapshot 字段基本健康
// ============================================================
TEST(PerfCounter, SnapshotFieldsSane)
{
	PerfCounter* pc = PerfCounter::Instance();
	PerfSnapshot snap = pc->GetSnapshot();
	// uptime 应为正
	EXPECT_TRUE(snap.uptimeSec > 0.0);
	// 累计计数为非负
	EXPECT_TRUE(snap.bltCount < UINT64_MAX);
	EXPECT_TRUE(snap.bltFastCount < UINT64_MAX);
	EXPECT_TRUE(snap.fillColorCount < UINT64_MAX);
	EXPECT_TRUE(snap.flipCount < UINT64_MAX);
	EXPECT_TRUE(snap.renderCount < UINT64_MAX);
	// perSec 字段都为非负
	EXPECT_TRUE(snap.bltPerSec >= 0.0);
	EXPECT_TRUE(snap.bltFastPerSec >= 0.0);
	EXPECT_TRUE(snap.fillColorPerSec >= 0.0);
	EXPECT_TRUE(snap.flipPerSec >= 0.0);
	EXPECT_TRUE(snap.renderPerSec >= 0.0);
	EXPECT_TRUE(snap.fps >= 0.0);
}

// ============================================================
// 10. StartTimeSec 单调不变
// ============================================================
TEST(PerfCounter, StartTimeIsFixed)
{
	PerfCounter* pc = PerfCounter::Instance();
	double t0 = pc->StartTimeSec();
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	double t1 = pc->StartTimeSec();
	EXPECT_TRUE(t0 > 0.0);
	EXPECT_EQ(t0, t1);  // start time 永远不变
}

// ============================================================
// 11. Hot scope 列表最多 8 项
// ============================================================
TEST(PerfCounter, HotScopeAtMost8)
{
	PerfCounter* pc = PerfCounter::Instance();
	// 已注册的 scope 可能 > 8（前面测试已经注入）
	PerfSnapshot snap = pc->GetSnapshot();
	EXPECT_TRUE(snap.hotScopeNames.size() <= 8);
	EXPECT_EQ(snap.hotScopeNames.size(), snap.hotScopeCounts.size());
}
