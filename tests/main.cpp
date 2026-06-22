// Phase 6.6: 单元测试主入口
//
// 跑所有 .cpp 中的 TEST(group, name) 宏
// 入口由 SingleTest.h 提供：RUN_ALL_TESTS()
//
// 退出码：
//   0 = 全部通过
//   1 = 有失败
//
// 也支持命令行参数：
//   --list               列出所有测试名（不跑）
//   --filter <substr>    只跑名字含 <substr> 的测试
//   --help               帮助

#include "SingleTest.h"

#include <cstdio>
#include <cstring>
#include <string>

int main(int argc, char** argv)
{
	// 解析命令行
	bool listOnly = false;
	std::string filter;
	for (int i = 1; i < argc; ++i)
	{
		if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
		{
			std::printf("Usage: %s [options]\n"
				"  --list           List all registered tests and exit\n"
				"  --filter <sub>   Run only tests whose group.name contains <sub>\n"
				"  --help, -h       Show this help\n",
				argv[0]);
			return 0;
		}
		else if (std::strcmp(argv[i], "--list") == 0)
		{
			listOnly = true;
		}
		else if (std::strcmp(argv[i], "--filter") == 0 && i + 1 < argc)
		{
			filter = argv[++i];
		}
	}

	// --list
	if (listOnly)
	{
		const auto& reg = singletest::Registry();
		std::printf("Registered %zu tests:\n", reg.size());
		for (const auto& e : reg)
		{
			std::printf("  %s.%s\n", e.group, e.name);
		}
		return 0;
	}

	// --filter：跑过滤后的子集
	if (!filter.empty())
	{
		const auto& reg = singletest::Registry();
		int total = 0, passed = 0, failed = 0, totalAsserts = 0;
		for (const auto& e : reg)
		{
			std::string full = std::string(e.group) + "." + e.name;
			if (full.find(filter) == std::string::npos) continue;

			++total;
			auto& state = singletest::State();
			state.Reset();
			state.currentTest = e.name;
			std::printf("[SingleTest] RUN  %s.%s\n", e.group, e.name);
			e.func();
			totalAsserts += state.totalAssert;
			if (state.failCount == 0)
			{
				std::printf("[SingleTest] PASS %s.%s (%d asserts)\n",
					e.group, e.name, state.totalAssert);
				++passed;
			}
			else
			{
				std::printf("[SingleTest] FAIL %s.%s (%d/%d asserts failed)\n",
					e.group, e.name, state.failCount, state.totalAssert);
				++failed;
			}
		}
		std::printf("[SingleTest] === Filter '%s': %d run, %d passed, %d failed, %d asserts ===\n",
			filter.c_str(), total, passed, failed, totalAsserts);
		return failed == 0 ? 0 : 1;
	}

	// 默认：跑全部
	return singletest::RunAllTestsAndExit();
}
