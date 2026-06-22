// Phase 3.1: 单头 INI 解析器
// 设计目标：
//   - 零依赖（仅 STL），不依赖 Windows API，便于跨编译
//   - 缺失文件 / 缺失 key 均返默认值，不抛异常，不写日志
//   - 仅做只读解析；不做热重载
//   - 支持 [Section] / Key = Value / ; 注释 / 前后空白

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace NDDFIX
{
namespace Config
{

class IniParser
{
public:
	IniParser();
	~IniParser();

	// 加载并解析文件。文件不存在或读取失败时返 false；调用方可继续用 GetXxx 拿默认值。
	bool Load(const char* path);

	// 重新加载最近一次 Load 时的路径（用于热重载；缺省路径时返 false）。
	bool Reload();

	// 全部清空。
	void Clear();

	// 取出已加载的 section 列表（顺序：扫描时第一次出现的顺序）。
	// 用于 ConfigManager 扫 [Game.*] section。
	void GetSections(std::vector<std::string>& outSections) const;

	bool HasSection(const char* section) const;
	bool HasKey(const char* section, const char* key) const;

	std::string GetString(const char* section, const char* key, const char* defaultValue) const;
	int GetInt(const char* section, const char* key, int defaultValue) const;
	bool GetBool(const char* section, const char* key, bool defaultValue) const;
	float GetFloat(const char* section, const char* key, float defaultValue) const;

	const char* GetLoadedPath() const { return m_path.c_str(); }
	bool IsLoaded() const { return m_loaded; }

private:
	// key 大小写不敏感比较：Windows INI 传统约定
	struct CiHash
	{
		size_t operator()(const std::string& s) const noexcept;
	};
	struct CiEq
	{
		bool operator()(const std::string& a, const std::string& b) const noexcept;
	};
	using ValueMap = std::unordered_map<std::string, std::string, CiHash, CiEq>;

	// 反向索引：section -> [key -> value]
	std::unordered_map<std::string, ValueMap, CiHash, CiEq> m_sections;
	// 记录 section 首次出现顺序，供 GetSections 用
	std::vector<std::string> m_sectionOrder;

	std::string m_path;
	bool m_loaded;

	// 解析工具
	void ParseLine(const std::string& line, std::string& currentSection);
	static std::string Trim(const std::string& s);
};

} // namespace Config
} // namespace NDDFIX
