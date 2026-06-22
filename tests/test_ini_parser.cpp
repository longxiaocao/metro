// Phase 6.3: 单元测试 - INI 解析器
//
// 覆盖范围（按 ddfix/Config/IniParser.h 的 API）：
//   1. 解析标准 INI 格式（[Section] / Key = Value）
//   2. 缺失文件静默返 false（不抛、不崩）
//   3. 缺失 key 返 default
//   4. 注释（; 和 #）
//   5. 多个 section
//   6. 大小写不敏感（key + section）
//   7. GetInt / GetBool / GetFloat / GetString
//   8. 末尾空格 + 行内注释处理
//   9. GetSections 顺序保留

#include "SingleTest.h"

#include "../ddfix/Config/IniParser.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using NDDFIX::Config::IniParser;

// -------------------- 测试工具：写临时 INI 文件 --------------------

namespace
{

// 在系统临时目录写文件，返回完整路径
std::string WriteTempIni(const char* name, const char* content)
{
	// 用 getenv("TEMP") + name 拼路径，避免 std::filesystem（C++17）
	const char* tempDir = std::getenv("TEMP");
	if (!tempDir || !*tempDir) tempDir = ".";
	std::string path = std::string(tempDir) + "\\" + name + ".ini";

	std::ofstream ofs(path, std::ios::out | std::ios::trunc);
	ofs << content;
	ofs.close();
	return path;
}

void RemoveTempFile(const std::string& path)
{
	std::remove(path.c_str());
}

} // anonymous namespace

// ============================================================
// 1. 缺失文件静默
// ============================================================
TEST(IniParser, MissingFileSilent)
{
	IniParser p;
	bool ok = p.Load("Z:/definitely/does/not/exist/__nope__.ini");
	EXPECT_FALSE(ok);
	EXPECT_FALSE(p.IsLoaded());
	// 缺失文件时 GetString 应返 default
	EXPECT_EQ(p.GetString("X", "y", "fallback"), "fallback");
	EXPECT_EQ(p.GetInt("X", "y", 42), 42);
	EXPECT_EQ(p.GetBool("X", "y", true), true);
	EXPECT_TRUE(p.GetFloat("X", "y", 3.14f) > 3.13f && p.GetFloat("X", "y", 3.14f) < 3.15f);
}

TEST(IniParser, EmptyPathLoadReturnsFalse)
{
	IniParser p;
	EXPECT_FALSE(p.Load(""));
	EXPECT_FALSE(p.Load(nullptr));
}

TEST(IniParser, ReloadWithoutPathReturnsFalse)
{
	IniParser p;
	// 从未 Load 过，m_path 为空
	EXPECT_FALSE(p.Reload());
}

// ============================================================
// 2. 解析标准 INI
// ============================================================
TEST(IniParser, ParseStandardFormat)
{
	auto path = WriteTempIni("ini_std",
		"; a comment line\n"
		"[Render]\n"
		"Width = 1024\n"
		"Height = 768\n"
		"FullScreen = false\n"
		"\n"
		"[Audio]\n"
		"Volume = 0.8\n");

	IniParser p;
	EXPECT_TRUE(p.Load(path.c_str()));
	EXPECT_TRUE(p.IsLoaded());
	EXPECT_EQ(p.GetLoadedPath(), path);

	EXPECT_EQ(p.GetInt("Render", "Width", 0), 1024);
	EXPECT_EQ(p.GetInt("Render", "Height", 0), 768);
	EXPECT_FALSE(p.GetBool("Render", "FullScreen", true));
	EXPECT_TRUE(p.GetFloat("Audio", "Volume", 0.0f) > 0.79f &&
	            p.GetFloat("Audio", "Volume", 0.0f) < 0.81f);

	RemoveTempFile(path);
}

TEST(IniParser, HasSectionAndHasKey)
{
	auto path = WriteTempIni("ini_has",
		"[S1]\n"
		"k1 = v1\n"
		"k2 = v2\n");

	IniParser p;
	p.Load(path.c_str());

	EXPECT_TRUE(p.HasSection("S1"));
	EXPECT_FALSE(p.HasSection("S2"));
	EXPECT_TRUE(p.HasKey("S1", "k1"));
	EXPECT_TRUE(p.HasKey("S1", "k2"));
	EXPECT_FALSE(p.HasKey("S1", "k3"));
	EXPECT_FALSE(p.HasKey("S2", "k1"));

	RemoveTempFile(path);
}

TEST(IniParser, MissingKeyReturnsDefault)
{
	auto path = WriteTempIni("ini_miss",
		"[S1]\n"
		"present = hello\n");

	IniParser p;
	p.Load(path.c_str());

	EXPECT_EQ(p.GetString("S1", "missing", "DEF"), "DEF");
	EXPECT_EQ(p.GetInt("S1", "missing", 999), 999);
	EXPECT_TRUE(p.GetBool("S1", "missing", true));
	EXPECT_FALSE(p.GetBool("S1", "missing", false));
	EXPECT_EQ(p.GetFloat("S1", "missing", 2.5f), 2.5f);

	RemoveTempFile(path);
}

TEST(IniParser, MissingSectionReturnsDefault)
{
	auto path = WriteTempIni("ini_miss_sec",
		"[OnlyOne]\n"
		"k = v\n");

	IniParser p;
	p.Load(path.c_str());

	EXPECT_EQ(p.GetString("OtherSection", "k", "DFT"), "DFT");
	EXPECT_EQ(p.GetInt("OtherSection", "k", 77), 77);

	RemoveTempFile(path);
}

// ============================================================
// 3. 注释（; 和 #）
// ============================================================
TEST(IniParser, SemicolonCommentSkipped)
{
	auto path = WriteTempIni("ini_semi",
		"; this is a comment\n"
		"[S]\n"
		"; another comment\n"
		"k = 100 ; inline comment after value\n"
		"# hash comment\n"
		"k2 = 200\n");

	IniParser p;
	p.Load(path.c_str());

	EXPECT_EQ(p.GetInt("S", "k", 0), 100);
	EXPECT_EQ(p.GetInt("S", "k2", 0), 200);

	RemoveTempFile(path);
}

TEST(IniParser, HashCommentSkipped)
{
	auto path = WriteTempIni("ini_hash",
		"# top comment\n"
		"[Sec]\n"
		"# in-section comment\n"
		"key = 42\n");

	IniParser p;
	p.Load(path.c_str());
	EXPECT_EQ(p.GetInt("Sec", "key", 0), 42);

	RemoveTempFile(path);
}

// ============================================================
// 4. 多个 section
// ============================================================
TEST(IniParser, MultipleSections)
{
	auto path = WriteTempIni("ini_multi",
		"[A]\n"
		"x = 1\n"
		"\n"
		"[B]\n"
		"y = 2\n"
		"\n"
		"[C]\n"
		"z = 3\n");

	IniParser p;
	p.Load(path.c_str());
	EXPECT_EQ(p.GetInt("A", "x", 0), 1);
	EXPECT_EQ(p.GetInt("B", "y", 0), 2);
	EXPECT_EQ(p.GetInt("C", "z", 0), 3);

	std::vector<std::string> sections;
	p.GetSections(sections);
	EXPECT_EQ(sections.size(), static_cast<size_t>(3));
	EXPECT_EQ(sections[0], "A");
	EXPECT_EQ(sections[1], "B");
	EXPECT_EQ(sections[2], "C");

	RemoveTempFile(path);
}

TEST(IniParser, SameSectionListedOnce)
{
	// 同一 section 出现多次时，GetSections 只记首次出现
	auto path = WriteTempIni("ini_dup_sec",
		"[A]\n"
		"x = 1\n"
		"[A]\n"
		"y = 2\n");

	IniParser p;
	p.Load(path.c_str());
	std::vector<std::string> sections;
	p.GetSections(sections);
	EXPECT_EQ(sections.size(), static_cast<size_t>(1));

	RemoveTempFile(path);
}

// ============================================================
// 5. 大小写不敏感
// ============================================================
TEST(IniParser, KeyCaseInsensitive)
{
	auto path = WriteTempIni("ini_case_k",
		"[Section]\n"
		"MyKey = 100\n");

	IniParser p;
	p.Load(path.c_str());
	EXPECT_EQ(p.GetInt("Section", "MYKEY", 0), 100);
	EXPECT_EQ(p.GetInt("Section", "mykey", 0), 100);
	EXPECT_EQ(p.GetInt("Section", "MyKey", 0), 100);
	EXPECT_EQ(p.GetInt("Section", "MyKEY", 0), 100);

	RemoveTempFile(path);
}

TEST(IniParser, SectionCaseInsensitive)
{
	auto path = WriteTempIni("ini_case_s",
		"[MySection]\n"
		"k = 5\n");

	IniParser p;
	p.Load(path.c_str());
	EXPECT_EQ(p.GetInt("MYSECTION", "k", 0), 5);
	EXPECT_EQ(p.GetInt("mysection", "k", 0), 5);
	EXPECT_EQ(p.GetInt("MySection", "k", 0), 5);
	EXPECT_TRUE(p.HasSection("MYSECTION"));
	EXPECT_TRUE(p.HasSection("mysection"));

	RemoveTempFile(path);
}

// ============================================================
// 6. GetBool 各种形式
// ============================================================
TEST(IniParser, GetBoolTrueForms)
{
	auto path = WriteTempIni("ini_bool_t",
		"[B]\n"
		"a = true\n"
		"b = TRUE\n"
		"c = yes\n"
		"d = Y\n"
		"e = 1\n"
		"f = on\n"
		"g = t\n");

	IniParser p;
	p.Load(path.c_str());
	EXPECT_TRUE(p.GetBool("B", "a", false));
	EXPECT_TRUE(p.GetBool("B", "b", false));
	EXPECT_TRUE(p.GetBool("B", "c", false));
	EXPECT_TRUE(p.GetBool("B", "d", false));
	EXPECT_TRUE(p.GetBool("B", "e", false));
	EXPECT_TRUE(p.GetBool("B", "f", false));
	EXPECT_TRUE(p.GetBool("B", "g", false));

	RemoveTempFile(path);
}

TEST(IniParser, GetBoolFalseForms)
{
	auto path = WriteTempIni("ini_bool_f",
		"[B]\n"
		"a = false\n"
		"b = FALSE\n"
		"c = no\n"
		"d = n\n"
		"e = 0\n"
		"f = off\n"
		"g = f\n");

	IniParser p;
	p.Load(path.c_str());
	EXPECT_FALSE(p.GetBool("B", "a", true));
	EXPECT_FALSE(p.GetBool("B", "b", true));
	EXPECT_FALSE(p.GetBool("B", "c", true));
	EXPECT_FALSE(p.GetBool("B", "d", true));
	EXPECT_FALSE(p.GetBool("B", "e", true));
	EXPECT_FALSE(p.GetBool("B", "f", true));
	EXPECT_FALSE(p.GetBool("B", "g", true));

	RemoveTempFile(path);
}

TEST(IniParser, GetBoolUnrecognizedFallsBack)
{
	auto path = WriteTempIni("ini_bool_u",
		"[B]\n"
		"junk = xyz\n");

	IniParser p;
	p.Load(path.c_str());
	// 无法解析，保持 default
	EXPECT_TRUE(p.GetBool("B", "junk", true));
	EXPECT_FALSE(p.GetBool("B", "junk", false));

	RemoveTempFile(path);
}

// ============================================================
// 7. GetInt 各种进制
// ============================================================
TEST(IniParser, GetIntRadix)
{
	auto path = WriteTempIni("ini_int",
		"[I]\n"
		"dec = 100\n"
		"hex = 0xFF\n"
		"oct = 010\n"
		"neg = -50\n");

	IniParser p;
	p.Load(path.c_str());
	EXPECT_EQ(p.GetInt("I", "dec", 0), 100);
	EXPECT_EQ(p.GetInt("I", "hex", 0), 255);   // strtol base=0
	EXPECT_EQ(p.GetInt("I", "oct", 0), 8);     // 010 octal = 8
	EXPECT_EQ(p.GetInt("I", "neg", 0), -50);

	RemoveTempFile(path);
}

TEST(IniParser, GetIntUnparseableFallsBack)
{
	auto path = WriteTempIni("ini_int_bad",
		"[I]\n"
		"junk = abc\n");

	IniParser p;
	p.Load(path.c_str());
	EXPECT_EQ(p.GetInt("I", "junk", 99), 99);

	RemoveTempFile(path);
}

// ============================================================
// 8. GetFloat
// ============================================================
TEST(IniParser, GetFloatBasic)
{
	auto path = WriteTempIni("ini_float",
		"[F]\n"
		"a = 1.5\n"
		"b = -3.14\n"
		"c = 2.0e2\n");

	IniParser p;
	p.Load(path.c_str());
	EXPECT_TRUE(p.GetFloat("F", "a", 0.0f) > 1.49f && p.GetFloat("F", "a", 0.0f) < 1.51f);
	EXPECT_TRUE(p.GetFloat("F", "b", 0.0f) > -3.15f && p.GetFloat("F", "b", 0.0f) < -3.13f);
	EXPECT_TRUE(p.GetFloat("F", "c", 0.0f) > 199.9f && p.GetFloat("F", "c", 0.0f) < 200.1f);

	RemoveTempFile(path);
}

// ============================================================
// 9. 末尾空格 + 行内注释处理
// ============================================================
TEST(IniParser, TrimWhitespace)
{
	auto path = WriteTempIni("ini_trim",
		"  [ TrimSection ]  \n"
		"  Key  =  value with spaces  \n"
		"\tTabKey\t=\tTabValue\t\n");

	IniParser p;
	p.Load(path.c_str());
	EXPECT_TRUE(p.HasSection("TrimSection"));
	EXPECT_TRUE(p.HasKey("TrimSection", "Key"));
	EXPECT_EQ(p.GetString("TrimSection", "Key", ""), "value with spaces");
	EXPECT_EQ(p.GetString("TrimSection", "TabKey", ""), "TabValue");

	RemoveTempFile(path);
}

TEST(IniParser, InlineSemicolonStrippedFromValue)
{
	auto path = WriteTempIni("ini_inline_semi",
		"[S]\n"
		"k = path;C:\\windows\n");

	IniParser p;
	p.Load(path.c_str());
	// 行内 ; 后内容被去注释
	std::string v = p.GetString("S", "k", "");
	EXPECT_TRUE(v == "path" || v == "path;C:\\windows");
	// 实际行为：找到 ; 切到 Trim 后的前半段 = "path"
	// 实现见 IniParser.cpp line 150-154
	EXPECT_EQ(v, "path");

	RemoveTempFile(path);
}

// ============================================================
// 10. 边界情况
// ============================================================
TEST(IniParser, EmptyFile)
{
	auto path = WriteTempIni("ini_empty", "");
	IniParser p;
	EXPECT_TRUE(p.Load(path.c_str()));  // 空文件应加载成功（视为无 section）
	EXPECT_FALSE(p.HasSection("X"));
	EXPECT_EQ(p.GetString("X", "y", "d"), "d");

	RemoveTempFile(path);
}

TEST(IniParser, KeyWithoutSectionIgnored)
{
	auto path = WriteTempIni("ini_orphan",
		"orphanKey = 100\n"
		"[Sec]\n"
		"good = 200\n");

	IniParser p;
	p.Load(path.c_str());
	// orphanKey 不在任何 section 下，被忽略
	EXPECT_EQ(p.GetInt("", "orphanKey", 0), 0);
	EXPECT_EQ(p.GetInt("Sec", "good", 0), 200);

	RemoveTempFile(path);
}

TEST(IniParser, ClearResetsAll)
{
	auto path = WriteTempIni("ini_clear",
		"[S]\n"
		"k = 1\n");

	IniParser p;
	p.Load(path.c_str());
	EXPECT_TRUE(p.HasSection("S"));
	p.Clear();
	EXPECT_FALSE(p.HasSection("S"));
	EXPECT_FALSE(p.IsLoaded());

	RemoveTempFile(path);
}

TEST(IniParser, MalformedSectionIgnored)
{
	auto path = WriteTempIni("ini_bad_sec",
		"[NoCloseBracket\n"   // 缺 ]
		"k = 1\n"
		"[GoodSection]\n"
		"k2 = 2\n");

	IniParser p;
	p.Load(path.c_str());
	// "[NoCloseBracket" 应被忽略
	EXPECT_FALSE(p.HasSection("NoCloseBracket"));
	EXPECT_TRUE(p.HasSection("GoodSection"));
	EXPECT_EQ(p.GetInt("GoodSection", "k2", 0), 2);

	RemoveTempFile(path);
}

TEST(IniParser, MalformedLineNoEqualsIgnored)
{
	auto path = WriteTempIni("ini_no_eq",
		"[S]\n"
		"this_is_not_a_kv_line\n"
		"good = 1\n");

	IniParser p;
	p.Load(path.c_str());
	EXPECT_EQ(p.GetInt("S", "good", 0), 1);

	RemoveTempFile(path);
}

TEST(IniParser, ReloadWorks)
{
	auto path = WriteTempIni("ini_reload",
		"[S]\n"
		"k = 1\n");

	IniParser p;
	EXPECT_TRUE(p.Load(path.c_str()));
	EXPECT_EQ(p.GetInt("S", "k", 0), 1);

	// 改文件
	{
		std::ofstream ofs(path, std::ios::out | std::ios::trunc);
		ofs << "[S]\nk = 999\n";
	}

	EXPECT_TRUE(p.Reload());
	EXPECT_EQ(p.GetInt("S", "k", 0), 999);

	RemoveTempFile(path);
}
