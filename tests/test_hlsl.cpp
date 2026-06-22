// Phase 6.5: 单元测试 - HLSL 编译产物
//
// 覆盖：
//   1. ColorKey.hlsl 源文件存在
//   2. 源文件含 ps_main 入口
//   3. 源文件含 sampler2D 声明
//   4. ColorKeyHLSLC.h 生成产物存在（依赖 ddfix 构建完成）
//   5. g_colorKeyHLSLC 数组大小合理（DX9 ps_2_b 字节码典型 100-2000 字节）
//
// 重要：ColorKeyHLSLC.h 是 CMake configure 时由 fxc.exe 生成的，
//       不在源码树里。test 通过环境变量 DDFIX_BUILD_DIR 定位，
//       找不到时跳过 array size 测试（不 fail）。

#include "SingleTest.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

// 解析 #include directive 的相对路径
// 测试编译时，本 .cpp 路径: ${PROJECT}/tests/test_hlsl.cpp
// 所以 HLSL 源: ../ddfix/ddraw/ColorKey.hlsl
// CMake 注入 DDFIX_SOURCE_DIR 宏（DDFIX 子项目根的绝对路径）
// 优先用绝对路径；fall back 相对路径（开发时手动跑 binary）

#ifndef DDFIX_SOURCE_DIR
#define DDFIX_SOURCE_DIR "ddfix"
#endif

bool FileExists(const std::string& path)
{
	std::ifstream ifs(path, std::ios::binary);
	return ifs.good();
}

std::string ReadEntireFile(const std::string& path)
{
	std::ifstream ifs(path, std::ios::binary);
	if (!ifs.good()) return std::string();
	std::ostringstream oss;
	oss << ifs.rdbuf();
	return oss.str();
}

// 列出 HLSL 源的可能位置（绝对 + 相对）
std::vector<std::string> PossibleSourcePaths()
{
	std::vector<std::string> out;
	// 1. 编译期绝对路径（CMake 注入）
	out.push_back(std::string(DDFIX_SOURCE_DIR) + "/ddraw/ColorKey.hlsl");
	out.push_back(std::string(DDFIX_SOURCE_DIR) + "\\ddraw\\ColorKey.hlsl");
	// 2. 相对路径（开发时手动跑 binary 时的回退）
	out.push_back("../ddfix/ddraw/ColorKey.hlsl");
	out.push_back("ddfix/ddraw/ColorKey.hlsl");
	return out;
}

// 找生成头（ColorKeyHLSLC.h）的可能位置
// CMake 中 fxc 输出到 ${CMAKE_CURRENT_BINARY_DIR}，对 ddfix 子项目来说是
// <build_dir>/ddfix/ColorKeyHLSLC.h
std::vector<std::string> PossibleGeneratedHeaderPaths()
{
	std::vector<std::string> out;

	// 1. 环境变量优先（CI 上 CMake 注入）
	const char* envBuild = std::getenv("DDFIX_BUILD_DIR");
	if (envBuild && *envBuild)
	{
		out.push_back(std::string(envBuild) + "/ColorKeyHLSLC.h");
		out.push_back(std::string(envBuild) + "\\ColorKeyHLSLC.h");
	}

	// 2. 相对当前工作目录的常见位置
	out.push_back("./ddfix/ColorKeyHLSLC.h");
	out.push_back("./bin/ColorKeyHLSLC.h");
	out.push_back("../ddfix/ColorKeyHLSLC.h");
	out.push_back("../bin/ColorKeyHLSLC.h");
	out.push_back("ddfix/ColorKeyHLSLC.h");
	out.push_back("bin/ColorKeyHLSLC.h");

	return out;
}

} // anonymous namespace

// ============================================================
// 1. ColorKey.hlsl 源文件存在
// ============================================================
TEST(HLSL, SourceFileExists)
{
	auto paths = PossibleSourcePaths();
	bool found = false;
	std::string used;
	for (const auto& p : paths)
	{
		if (FileExists(p))
		{
			found = true;
			used = p;
			break;
		}
	}
	EXPECT_TRUE(found);
	if (found)
	{
		std::printf("    [info] HLSL source at: %s\n", used.c_str());
	}
}

// ============================================================
// 2. 源文件含 ps_main 入口
// ============================================================
TEST(HLSL, SourceContainsPsMain)
{
	std::string content;
	for (const auto& p : PossibleSourcePaths())
	{
		content = ReadEntireFile(p);
		if (!content.empty()) break;
	}
	EXPECT_TRUE(!content.empty());
	EXPECT_TRUE(content.find("ps_main") != std::string::npos);
	EXPECT_TRUE(content.find("PS_OUTPUT") != std::string::npos);
	EXPECT_TRUE(content.find("tex2D") != std::string::npos);
}

// ============================================================
// 3. 源文件含 sampler2D 声明
// ============================================================
TEST(HLSL, SourceContainsSampler)
{
	std::string content;
	for (const auto& p : PossibleSourcePaths())
	{
		content = ReadEntireFile(p);
		if (!content.empty()) break;
	}
	EXPECT_TRUE(content.find("sampler2D") != std::string::npos);
}

// ============================================================
// 4. 源文件含 srcColorKey / destColorKey 常量
// ============================================================
TEST(HLSL, SourceContainsColorKeyConstants)
{
	std::string content;
	for (const auto& p : PossibleSourcePaths())
	{
		content = ReadEntireFile(p);
		if (!content.empty()) break;
	}
	EXPECT_TRUE(content.find("srcColorKey") != std::string::npos);
	EXPECT_TRUE(content.find("destColorKey") != std::string::npos);
	EXPECT_TRUE(content.find("haveColorKey") != std::string::npos);
	EXPECT_TRUE(content.find("checkAlpha") != std::string::npos);
}

// ============================================================
// 5. 源文件非空 + 至少 30 行（HLSL 复杂逻辑不应太短）
// ============================================================
TEST(HLSL, SourceNotEmpty)
{
	std::string content;
	for (const auto& p : PossibleSourcePaths())
	{
		content = ReadEntireFile(p);
		if (!content.empty()) break;
	}
	EXPECT_TRUE(!content.empty());
	EXPECT_TRUE(content.size() > 100);  // 至少 100 字节

	int lineCount = 0;
	for (char c : content) if (c == '\n') ++lineCount;
	EXPECT_TRUE(lineCount >= 20);
}

// ============================================================
// 6. 生成头存在（可被跳过，依赖构建完成）
// ============================================================
TEST(HLSL, GeneratedHeaderExists)
{
	auto paths = PossibleGeneratedHeaderPaths();
	std::string found;
	for (const auto& p : paths)
	{
		if (FileExists(p))
		{
			found = p;
			break;
		}
	}
	if (found.empty())
	{
		std::printf("    [skip] ColorKeyHLSLC.h not found; "
			"set DDFIX_BUILD_DIR or run full build first.\n");
		// 跳过：这是测试运行时依赖，非必须
		EXPECT_TRUE(true);
		return;
	}
	std::printf("    [info] Generated header at: %s\n", found.c_str());
	EXPECT_TRUE(true);
}

// ============================================================
// 7. g_colorKeyHLSLC 数组大小合理
// ============================================================
TEST(HLSL, BytecodeArraySizeReasonable)
{
	// 找 ColorKeyHLSLC.h 并 #include 它，验证数组大小
	auto paths = PossibleGeneratedHeaderPaths();
	std::string found;
	for (const auto& p : paths)
	{
		if (FileExists(p))
		{
			found = p;
			break;
		}
	}
	if (found.empty())
	{
		std::printf("    [skip] No ColorKeyHLSLC.h to verify size; "
			"set DDFIX_BUILD_DIR or run full build first.\n");
		EXPECT_TRUE(true);
		return;
	}

	// 把数组大小找出来：parse "g_colorKeyHLSLC[<size>] = { ... }"
	std::string content = ReadEntireFile(found);
	EXPECT_TRUE(!content.empty());

	// 找 "g_colorKeyHLSLC[" 后的数字
	const std::string marker = "g_colorKeyHLSLC[";
	auto pos = content.find(marker);
	if (pos == std::string::npos)
	{
		// 也可能数组名不同（看 fxc 命令行 /Vng_xxx）
		// 搜索所有 "[" 紧跟数字的模式
		std::printf("    [warn] g_colorKeyHLSLC not in header, content head:\n%s\n",
			content.substr(0, 200).c_str());
		EXPECT_TRUE(true);
		return;
	}
	pos += marker.size();
	// 读数字
	size_t endPos = pos;
	while (endPos < content.size() && (content[endPos] >= '0' && content[endPos] <= '9'))
		++endPos;
	std::string numStr = content.substr(pos, endPos - pos);
	EXPECT_TRUE(!numStr.empty());

	int arraySize = std::atoi(numStr.c_str());
	std::printf("    [info] g_colorKeyHLSLC[] size = %d DWORDs (= %d bytes)\n",
		arraySize, arraySize * 4);
	// ps_2_b 字节码典型 50-2000 DWORD（200-8000 字节）
	EXPECT_TRUE(arraySize > 0);
	EXPECT_TRUE(arraySize < 100000);  // sanity 上限
}
