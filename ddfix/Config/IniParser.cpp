// Phase 3.1: INI 解析器实现
// 说明：使用 C 标准 FILE 读取（不引入 fstream，避免和 Log::LOG 静态初始化顺序冲突）。

#include "IniParser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <cctype>

namespace NDDFIX
{
namespace Config
{

IniParser::IniParser()
	: m_path()
	, m_loaded(false)
{
}

IniParser::~IniParser() = default;

void IniParser::Clear()
{
	m_sections.clear();
	m_sectionOrder.clear();
	m_loaded = false;
	// 故意保留 m_path：调用方经常 Clear 后再 Load 别的文件
}

size_t IniParser::CiHash::operator()(const std::string& s) const noexcept
{
	// 简化的 FNV-1a，对字符做 tolower 后参与运算
	uint32_t h = 2166136261u;
	for (char c : s)
	{
		unsigned char uc = static_cast<unsigned char>(c);
		if (uc >= 'A' && uc <= 'Z')
		{
			uc = static_cast<unsigned char>(uc - 'A' + 'a');
		}
		h ^= uc;
		h *= 16777619u;
	}
	return static_cast<size_t>(h);
}

bool IniParser::CiEq::operator()(const std::string& a, const std::string& b) const noexcept
{
	if (a.size() != b.size())
	{
		return false;
	}
	for (size_t i = 0; i < a.size(); ++i)
	{
		unsigned char ca = static_cast<unsigned char>(a[i]);
		unsigned char cb = static_cast<unsigned char>(b[i]);
		if (ca >= 'A' && ca <= 'Z') ca = static_cast<unsigned char>(ca - 'A' + 'a');
		if (cb >= 'A' && cb <= 'Z') cb = static_cast<unsigned char>(cb - 'A' + 'a');
		if (ca != cb)
		{
			return false;
		}
	}
	return true;
}

std::string IniParser::Trim(const std::string& s)
{
	auto begin = s.begin();
	auto end = s.end();
	while (begin != end)
	{
		unsigned char c = static_cast<unsigned char>(*begin);
		if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
		{
			break;
		}
		++begin;
	}
	while (end != begin)
	{
		unsigned char c = static_cast<unsigned char>(*(end - 1));
		if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
		{
			break;
		}
		--end;
	}
	return std::string(begin, end);
}

void IniParser::ParseLine(const std::string& line, std::string& currentSection)
{
	std::string trimmed = Trim(line);
	if (trimmed.empty())
	{
		return;
	}

	// 注释：';' 开头整行跳过
	if (trimmed[0] == ';' || trimmed[0] == '#')
	{
		return;
	}

	// Section 头
	if (trimmed[0] == '[')
	{
		auto closePos = trimmed.find(']');
		if (closePos == std::string::npos)
		{
			return; // 非法行：忽略
		}
		std::string section = Trim(trimmed.substr(1, closePos - 1));
		if (section.empty())
		{
			currentSection.clear();
			return;
		}
		currentSection = section;
		// 记下首次出现顺序
		if (m_sections.find(section) == m_sections.end())
		{
			m_sectionOrder.push_back(section);
		}
		// 保证 section 一定存在
		if (m_sections.find(section) == m_sections.end())
		{
			m_sections.emplace(section, ValueMap{});
		}
		return;
	}

	// key = value
	auto eqPos = trimmed.find('=');
	if (eqPos == std::string::npos)
	{
		return; // 非法行
	}
	if (currentSection.empty())
	{
		return; // 文件头出现 key，不归属任何 section，忽略
	}
	std::string key = Trim(trimmed.substr(0, eqPos));
	std::string value = Trim(trimmed.substr(eqPos + 1));
	// 去掉行内注释：';' 之后的内容忽略（简单处理，不处理引号）
	auto semiPos = value.find(';');
	if (semiPos != std::string::npos)
	{
		value = Trim(value.substr(0, semiPos));
	}
	if (key.empty())
	{
		return;
	}
	m_sections[currentSection][key] = value;
}

bool IniParser::Load(const char* path)
{
	Clear();
	if (!path || !*path)
	{
		return false;
	}
	m_path = path;

	FILE* fp = nullptr;
	fopen_s(&fp, path, "rb");
	if (!fp)
	{
		// 缺失文件视为正常场景，返 false 但保留 m_path 以便 Reload。
		return false;
	}

	char buf[1024];
	std::string currentSection;
	while (fgets(buf, sizeof(buf), fp))
	{
		ParseLine(buf, currentSection);
	}
	fclose(fp);
	m_loaded = true;
	return true;
}

bool IniParser::Reload()
{
	if (m_path.empty())
	{
		return false;
	}
	return Load(m_path.c_str());
}

void IniParser::GetSections(std::vector<std::string>& outSections) const
{
	outSections.clear();
	outSections.reserve(m_sectionOrder.size());
	for (const auto& s : m_sectionOrder)
	{
		outSections.push_back(s);
	}
}

bool IniParser::HasSection(const char* section) const
{
	if (!section) return false;
	return m_sections.find(section) != m_sections.end();
}

bool IniParser::HasKey(const char* section, const char* key) const
{
	if (!section || !key) return false;
	auto secIt = m_sections.find(section);
	if (secIt == m_sections.end()) return false;
	return secIt->second.find(key) != secIt->second.end();
}

std::string IniParser::GetString(const char* section, const char* key, const char* defaultValue) const
{
	if (!section || !key) return defaultValue ? defaultValue : "";
	auto secIt = m_sections.find(section);
	if (secIt == m_sections.end()) return defaultValue ? defaultValue : "";
	auto kvIt = secIt->second.find(key);
	if (kvIt == secIt->second.end()) return defaultValue ? defaultValue : "";
	return kvIt->second;
}

int IniParser::GetInt(const char* section, const char* key, int defaultValue) const
{
	std::string s = GetString(section, key, "");
	if (s.empty()) return defaultValue;
	char* end = nullptr;
	long v = strtol(s.c_str(), &end, 0);
	if (end == s.c_str()) return defaultValue;
	return static_cast<int>(v);
}

float IniParser::GetFloat(const char* section, const char* key, float defaultValue) const
{
	std::string s = GetString(section, key, "");
	if (s.empty()) return defaultValue;
	char* end = nullptr;
	float v = strtof(s.c_str(), &end);
	if (end == s.c_str()) return defaultValue;
	return v;
}

bool IniParser::GetBool(const char* section, const char* key, bool defaultValue) const
{
	std::string s = GetString(section, key, "");
	if (s.empty()) return defaultValue;
	// 转小写
	std::string lower;
	lower.reserve(s.size());
	for (char c : s)
	{
		lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
	}
	if (lower == "1" || lower == "true" || lower == "yes" || lower == "on" || lower == "y" || lower == "t")
	{
		return true;
	}
	if (lower == "0" || lower == "false" || lower == "no" || lower == "off" || lower == "n" || lower == "f")
	{
		return false;
	}
	// 其它非空字符串：尝试解析数字
	char* end = nullptr;
	long v = strtol(s.c_str(), &end, 0);
	if (end == s.c_str()) return defaultValue;
	return v != 0;
}

} // namespace Config
} // namespace NDDFIX
