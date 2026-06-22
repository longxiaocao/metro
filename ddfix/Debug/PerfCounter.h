// Phase 4.3: 性能计数器
// 设计目标：
//   - 全局静态计数：每次 Blt / BltFast / FillColor 进入都 Increment*
//   - 1 秒滑动平均 FPS（基于 PerfCounter::Tick 进入）
//   - PERF_SCOPE(name) 宏：函数入口/出口埋点（基于 RAII），记录调用次数 + 总耗时
//   - 线程安全：所有计数用 std::atomic<>；FPS 滑动窗口用 mutex 保护
//   - HUD 通过 PerfCounter::Instance()->GetSnapshot() 读快照，零拷贝

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <mutex>
#include <chrono>
#include <string>
#include <vector>

namespace NDDFIX
{
namespace Debug
{

struct PerfSnapshot
{
	// 累计计数（自进程启动以来）
	uint64_t bltCount;
	uint64_t bltFastCount;
	uint64_t fillColorCount;
	uint64_t flipCount;
	uint64_t renderCount;

	// 1 秒滑动平均（calls per second）
	double bltPerSec;
	double bltFastPerSec;
	double fillColorPerSec;
	double flipPerSec;
	double renderPerSec;

	// 1 秒滑动平均 FPS（基于 Tick）
	double fps;

	// 自启动以来的总运行时间（秒）
	double uptimeSec;

	// 最后一次 PERF_SCOPE 命中点（环形缓冲前 N 个名字）
	std::vector<std::string> hotScopeNames;
	std::vector<uint64_t>    hotScopeCounts;
};

class PerfCounter
{
public:
	static PerfCounter* Instance();

	// 计数接口：每个调用方在入口处调一次即可（线程安全）
	void IncrementBlt();
	void IncrementBltFast();
	void IncrementFillColor();
	void IncrementFlip();
	void IncrementRender();

	// 每帧 / HUD 刷新时调一次，刷新 1 秒滑动平均 + FPS
	// 返回自上次 Tick 以来的秒数（供 HUD 调试用）
	double Tick();

	// 启动时刻（用于计算 uptime）。构造时已调一次。
	double StartTimeSec() const { return m_startTimeSec; }

	// 读快照（HUD 用）
	PerfSnapshot GetSnapshot();

	// PERF_SCOPE(name) 宏底层：登记一个作用域作用域
	void EnterScope(const char* name);
	void LeaveScope(const char* name);

private:
	PerfCounter();
	~PerfCounter() = default;
	PerfCounter(const PerfCounter&) = delete;
	PerfCounter& operator=(const PerfCounter&) = delete;

	// 1 秒窗口内的累计增量
	struct SlidingWindow
	{
		std::mutex mu;
		double windowStartSec = 0.0;   // 当前窗口起点
		double currentBlt = 0.0;
		double currentBltFast = 0.0;
		double currentFillColor = 0.0;
		double currentFlip = 0.0;
		double currentRender = 0.0;
		double currentFrames = 0.0;    // Tick 次数
		double lastBltPerSec = 0.0;
		double lastBltFastPerSec = 0.0;
		double lastFillColorPerSec = 0.0;
		double lastFlipPerSec = 0.0;
		double lastRenderPerSec = 0.0;
		double lastFps = 0.0;
	};

	SlidingWindow m_window;
	std::atomic<uint64_t> m_bltCount{ 0 };
	std::atomic<uint64_t> m_bltFastCount{ 0 };
	std::atomic<uint64_t> m_fillColorCount{ 0 };
	std::atomic<uint64_t> m_flipCount{ 0 };
	std::atomic<uint64_t> m_renderCount{ 0 };

	double m_startTimeSec;
};

// PERF_SCOPE 宏：函数入口登记，析构时减计数并累加耗时。
// 用法：在函数体第一行 `PERF_SCOPE("Blt");` 即可。
class PerfScopeGuard
{
public:
	explicit PerfScopeGuard(const char* name)
		: m_name(name)
		, m_start(std::chrono::steady_clock::now())
	{
		PerfCounter::Instance()->EnterScope(m_name);
	}
	~PerfScopeGuard()
	{
		PerfCounter::Instance()->LeaveScope(m_name);
		// 耗时可后续扩展：目前只统计次数，Hud 暂时只展示 count
	}
	PerfScopeGuard(const PerfScopeGuard&) = delete;
	PerfScopeGuard& operator=(const PerfScopeGuard&) = delete;
private:
	const char* m_name;
	std::chrono::steady_clock::time_point m_start;
};

#define PERF_SCOPE_CONCAT_INNER(a, b) a##b
#define PERF_SCOPE_CONCAT(a, b) PERF_SCOPE_CONCAT_INNER(a, b)
#define PERF_SCOPE(name) PerfScopeGuard PERF_SCOPE_CONCAT(_perfScope_, __LINE__)(name)

} // namespace Debug
} // namespace NDDFIX
