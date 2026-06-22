// Phase 6.1: 极简自包含测试框架（SingleTest）
// 设计目标：
//   - 零依赖（仅 STL），不引入 gtest 等第三方库
//   - 项目为离线分发，不联网 FetchContent
//   - 极小：单头、几十行，可读懂
//   - 用法：TEST(group, name) { body; } + RUN_ALL_TESTS()
//
// 设计权衡：
//   - 用静态注册（lazy global vector）保证 test 顺序按 TEST 宏出现顺序
//   - 每个 TEST 宏展开为：函数声明 + 全局注册 + 函数定义
//   - 断言失败不抛异常，而是用 thread_local flag 累积，最后由 RUN_ALL_TESTS 统一报
//   - 不支持 EXPECT_NO_FATAL / death test（不需要）

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include <sstream>

namespace singletest
{

// -------------------- 断言失败状态 --------------------
//
// WHY: 单一测试函数体中可能有多个 EXPECT_* 调用；
//      用 thread_local 累积失败计数，函数返回时汇总到当前 TEST。
struct AssertState
{
	int failCount = 0;       // 当前 TEST 中失败次数
	int totalAssert = 0;     // 当前 TEST 中断言总次数
	const char* currentTest = nullptr;

	void Reset()
	{
		failCount = 0;
		totalAssert = 0;
		currentTest = nullptr;
	}
};

inline AssertState& State()
{
	static thread_local AssertState s;
	return s;
}

// -------------------- 断言宏底层 --------------------
inline void ReportAssertFail(const char* expr, const char* file, int line)
{
	std::fprintf(stderr,
		"  [FAIL] %s:%d: %s\n", file, line, expr);
	++State().failCount;
}

inline void CountAssert()
{
	++State().totalAssert;
}

// -------------------- 公开断言宏 --------------------
#define EXPECT_TRUE(expr)                                                  \
	do {                                                                   \
		singletest::CountAssert();                                         \
		if (!(expr)) {                                                     \
			singletest::ReportAssertFail(#expr, __FILE__, __LINE__);       \
		}                                                                  \
	} while (0)

#define EXPECT_FALSE(expr)                                                 \
	do {                                                                   \
		singletest::CountAssert();                                         \
		if ((expr)) {                                                      \
			singletest::ReportAssertFail("!(" #expr ")", __FILE__, __LINE__); \
		}                                                                  \
	} while (0)

#define EXPECT_EQ(a, b)                                                    \
	do {                                                                   \
		singletest::CountAssert();                                         \
		auto _va = (a);                                                    \
		auto _vb = (b);                                                    \
		if (!(_va == _vb)) {                                               \
			std::ostringstream _oss;                                      \
			_oss << "EXPECT_EQ(" #a ", " #b ") actual=" << _va              \
			     << " expected=" << _vb;                                   \
			singletest::ReportAssertFail(_oss.str().c_str(),               \
				__FILE__, __LINE__);                                       \
		}                                                                  \
	} while (0)

#define EXPECT_NE(a, b)                                                    \
	do {                                                                   \
		singletest::CountAssert();                                         \
		auto _va = (a);                                                    \
		auto _vb = (b);                                                    \
		if (!(_va != _vb)) {                                               \
			std::ostringstream _oss;                                      \
			_oss << "EXPECT_NE(" #a ", " #b ") both=" << _va;              \
			singletest::ReportAssertFail(_oss.str().c_str(),               \
				__FILE__, __LINE__);                                       \
		}                                                                  \
	} while (0)

#define EXPECT_GT(a, b)                                                    \
	do {                                                                   \
		singletest::CountAssert();                                         \
		auto _va = (a);                                                    \
		auto _vb = (b);                                                    \
		if (!(_va > _vb)) {                                                \
			std::ostringstream _oss;                                      \
			_oss << "EXPECT_GT(" #a ", " #b ") actual=" << _va              \
			     << " expected>" << _vb;                                   \
			singletest::ReportAssertFail(_oss.str().c_str(),               \
				__FILE__, __LINE__);                                       \
		}                                                                  \
	} while (0)

#define EXPECT_LT(a, b)                                                    \
	do {                                                                   \
		singletest::CountAssert();                                         \
		auto _va = (a);                                                    \
		auto _vb = (b);                                                    \
		if (!(_va < _vb)) {                                                \
			std::ostringstream _oss;                                      \
			_oss << "EXPECT_LT(" #a ", " #b ") actual=" << _va              \
			     << " expected<" << _vb;                                   \
			singletest::ReportAssertFail(_oss.str().c_str(),               \
				__FILE__, __LINE__);                                       \
		}                                                                  \
	} while (0)

#define EXPECT_GE(a, b)                                                    \
	do {                                                                   \
		singletest::CountAssert();                                         \
		auto _va = (a);                                                    \
		auto _vb = (b);                                                    \
		if (!(_va >= _vb)) {                                               \
			std::ostringstream _oss;                                      \
			_oss << "EXPECT_GE(" #a ", " #b ") actual=" << _va             \
			     << " expected>=" << _vb;                                  \
			singletest::ReportAssertFail(_oss.str().c_str(),               \
				__FILE__, __LINE__);                                       \
		}                                                                  \
	} while (0)

#define EXPECT_LE(a, b)                                                    \
	do {                                                                   \
		singletest::CountAssert();                                         \
		auto _va = (a);                                                    \
		auto _vb = (b);                                                    \
		if (!(_va <= _vb)) {                                               \
			std::ostringstream _oss;                                      \
			_oss << "EXPECT_LE(" #a ", " #b ") actual=" << _va             \
			     << " expected<=" << _vb;                                  \
			singletest::ReportAssertFail(_oss.str().c_str(),               \
				__FILE__, __LINE__);                                       \
		}                                                                  \
	} while (0)

// -------------------- 测试注册表 --------------------
using TestFunc = void (*)();

struct TestEntry
{
	const char* group;
	const char* name;
	TestFunc    func;
};

// 函数内 static：保证线程安全初始化（C++11 起）
inline std::vector<TestEntry>& Registry()
{
	static std::vector<TestEntry> registry;
	return registry;
}

// AutoRegister 仅用于零开销地把 TestEntry 推到 Registry；
// 静态对象在 main 之前构造完成。
struct AutoRegister
{
	explicit AutoRegister(const char* group, const char* name, TestFunc func)
	{
		Registry().push_back({group, name, func});
	}
};

struct TestStats
{
	int total = 0;
	int pass  = 0;
	int fail  = 0;
	int totalAsserts = 0;
};

inline TestStats& GlobalStats()
{
	static TestStats s;
	return s;
}

// -------------------- 主入口 --------------------
//
// 用法：在 main.cpp 里 `singletest::RunAllTests();`
// 返 true 表示全绿，false 表示有失败。
// 内部 std::exit(0/1) 由调用方按需使用。
inline bool RunAllTests()
{
	std::printf("[SingleTest] === Running %zu tests ===\n", Registry().size());

	auto& stats = GlobalStats();
	stats = TestStats{};
	stats.total = static_cast<int>(Registry().size());

	for (const auto& entry : Registry())
	{
		auto& state = State();
		state.Reset();
		state.currentTest = entry.name;

		std::printf("[SingleTest] RUN  %s.%s\n", entry.group, entry.name);
		entry.func();

		stats.totalAsserts += state.totalAssert;
		if (state.failCount == 0)
		{
			std::printf("[SingleTest] PASS %s.%s (%d asserts)\n",
				entry.group, entry.name, state.totalAssert);
			++stats.pass;
		}
		else
		{
			std::printf("[SingleTest] FAIL %s.%s (%d/%d asserts failed)\n",
				entry.group, entry.name, state.failCount, state.totalAssert);
			++stats.fail;
		}
	}

	std::printf("[SingleTest] === %d tests, %d passed, %d failed, %d asserts total ===\n",
		stats.total, stats.pass, stats.fail, stats.totalAsserts);

	return stats.fail == 0;
}

// 简写：跑完测试并按结果决定 exit code
inline int RunAllTestsAndExit()
{
	bool ok = RunAllTests();
	std::exit(ok ? 0 : 1);
}

} // namespace singletest

// -------------------- TEST 宏 --------------------
//
// 用法：TEST(IniParser, MissingKey) { IniParser p; EXPECT_EQ(...); }
//
// 展开为：
//   static void Test_IniParser_MissingKey();
//   namespace { static ::singletest::AutoRegister
//       _reg_IniParser_MissingKey("IniParser", "MissingKey",
//           &Test_IniParser_MissingKey); }
//   static void Test_IniParser_MissingKey()
//
// 注：用匿名 namespace 包裹 AutoRegister 避免符号污染。
//     函数自身仍 static（internal linkage），同名 TEST 在不同 .cpp 也独立。
#define TEST(group, name)                                                   \
	static void Test_##group##_##name();                                    \
	namespace {                                                             \
		static ::singletest::AutoRegister                                   \
			_singletest_reg_##group##_##name(                               \
				#group, #name, &Test_##group##_##name);                     \
	}                                                                       \
	static void Test_##group##_##name()

// 简写：调 RunAllTestsAndExit() 的最简 main
#define RUN_ALL_TESTS() ::singletest::RunAllTests()
