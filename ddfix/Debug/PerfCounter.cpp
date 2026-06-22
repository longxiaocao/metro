// Phase 4.3: PerfCounter 实现

#include "PerfCounter.h"
#include "../Common/Logging.h"
#include <unordered_map>
#include <algorithm>
#include <cstdint>

namespace NDDFIX
{
namespace Debug
{

// PERF_SCOPE 计数用的内部全局表（独立于 PerfCounter 实例，避免每次 new 重建）。
// 用函数内 static 保证线程安全初始化（C++11 起）。
static std::mutex& GetScopeMutex()
{
	static std::mutex mu;
	return mu;
}

static std::unordered_map<std::string, uint64_t>& GetScopeCountMap()
{
	static std::unordered_map<std::string, uint64_t> m;
	return m;
}

PerfCounter* PerfCounter::Instance()
{
	static PerfCounter inst;
	return &inst;
}

PerfCounter::PerfCounter()
{
	m_startTimeSec = std::chrono::duration<double>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
	// 初始化窗口起点为当前时间
	m_window.windowStartSec = m_startTimeSec;
}

void PerfCounter::IncrementBlt()
{
	++m_bltCount;
	std::lock_guard<std::mutex> lock(m_window.mu);
	++m_window.currentBlt;
}

void PerfCounter::IncrementBltFast()
{
	++m_bltFastCount;
	std::lock_guard<std::mutex> lock(m_window.mu);
	++m_window.currentBltFast;
}

void PerfCounter::IncrementFillColor()
{
	++m_fillColorCount;
	std::lock_guard<std::mutex> lock(m_window.mu);
	++m_window.currentFillColor;
}

void PerfCounter::IncrementFlip()
{
	++m_flipCount;
	std::lock_guard<std::mutex> lock(m_window.mu);
	++m_window.currentFlip;
}

void PerfCounter::IncrementRender()
{
	++m_renderCount;
	std::lock_guard<std::mutex> lock(m_window.mu);
	++m_window.currentRender;
}

double PerfCounter::Tick()
{
	// 用 steady_clock 算 delta（不受系统时间跳变影响）
	auto nowTp = std::chrono::steady_clock::now();
	double nowSec = std::chrono::duration<double>(nowTp.time_since_epoch()).count();

	std::lock_guard<std::mutex> lock(m_window.mu);
	++m_window.currentFrames;

	double elapsed = nowSec - m_window.windowStartSec;
	if (elapsed >= 1.0)
	{
		// 窗口已满 1 秒，结算并滑窗
		m_window.lastBltPerSec       = m_window.currentBlt       / elapsed;
		m_window.lastBltFastPerSec   = m_window.currentBltFast   / elapsed;
		m_window.lastFillColorPerSec = m_window.currentFillColor / elapsed;
		m_window.lastFlipPerSec      = m_window.currentFlip      / elapsed;
		m_window.lastRenderPerSec    = m_window.currentRender    / elapsed;
		m_window.lastFps             = m_window.currentFrames    / elapsed;

		// 滑窗：丢弃旧累计，从当前开始重新累计
		double carry = elapsed - 1.0;
		if (carry < 0.0) carry = 0.0;
		m_window.windowStartSec = nowSec - carry;
		m_window.currentBlt       = 0.0;
		m_window.currentBltFast   = 0.0;
		m_window.currentFillColor = 0.0;
		m_window.currentFlip      = 0.0;
		m_window.currentRender    = 0.0;
		m_window.currentFrames    = 0.0;
	}

	return nowSec - m_startTimeSec;
}

void PerfCounter::EnterScope(const char* name)
{
	std::lock_guard<std::mutex> lock(GetScopeMutex());
	++GetScopeCountMap()[name];
}

void PerfCounter::LeaveScope(const char* /*name*/)
{
	// 当前实现只统计次数，LeaveScope 是 no-op（保持 RAII 配对语义）
}

PerfSnapshot PerfCounter::GetSnapshot()
{
	PerfSnapshot snap;
	snap.bltCount       = m_bltCount.load();
	snap.bltFastCount   = m_bltFastCount.load();
	snap.fillColorCount = m_fillColorCount.load();
	snap.flipCount      = m_flipCount.load();
	snap.renderCount    = m_renderCount.load();

	{
		std::lock_guard<std::mutex> lock(m_window.mu);
		snap.bltPerSec       = m_window.lastBltPerSec;
		snap.bltFastPerSec   = m_window.lastBltFastPerSec;
		snap.fillColorPerSec = m_window.lastFillColorPerSec;
		snap.flipPerSec      = m_window.lastFlipPerSec;
		snap.renderPerSec    = m_window.lastRenderPerSec;
		snap.fps             = m_window.lastFps;
	}

	// uptime
	auto nowTp = std::chrono::steady_clock::now();
	double nowSec = std::chrono::duration<double>(nowTp.time_since_epoch()).count();
	snap.uptimeSec = nowSec - m_startTimeSec;

	// 填充 hotScope 列表：复制 map，排序取前 8
	{
		std::lock_guard<std::mutex> lock(GetScopeMutex());
		auto& scopeMap = GetScopeCountMap();
		std::vector<std::pair<std::string, uint64_t>> vec(scopeMap.begin(), scopeMap.end());
		std::sort(vec.begin(), vec.end(),
			[](const std::pair<std::string, uint64_t>& a,
			   const std::pair<std::string, uint64_t>& b) {
				return a.second > b.second;
			});
		size_t n = (std::min)(static_cast<size_t>(8), vec.size());
		snap.hotScopeNames.reserve(n);
		snap.hotScopeCounts.reserve(n);
		for (size_t i = 0; i < n; ++i)
		{
			snap.hotScopeNames.push_back(vec[i].first);
			snap.hotScopeCounts.push_back(vec[i].second);
		}
	}

	return snap;
}

} // namespace Debug
} // namespace NDDFIX
