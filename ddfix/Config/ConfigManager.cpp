// Phase 3.2 + 3.5: ConfigManager 实现

#include "ConfigManager.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <vector>

namespace NDDFIX
{
namespace Config
{

ConfigManager::ConfigManager()
{
	LoadDefaults();
}

ConfigManager::~ConfigManager() = default;

ConfigManager* ConfigManager::Instance()
{
	static ConfigManager mgr;
	return &mgr;
}

void ConfigManager::LoadDefaults()
{
	// 默认值与 PROJECT_ANALYSIS.md §18.2 列出的"当前硬编码"严格保持一致
	// 这样行为退化：缺 ini 时跟旧版完全一样。
	m_render.useSoftwareBlt       = false; // 旧: g_useSoftwareWrapper9 = false
	m_render.lockableBackBuffer   = true;  // 旧: D3DPRESENTFLAG_LOCKABLE_BACKBUFFER
	m_render.vsync                = false; // 旧: D3DPRESENT_INTERVAL_IMMEDIATE
	m_render.lightingEnabled      = true;  // 旧: 注释里的 D3DRS_LIGHTING = TRUE
	m_render.allowBackBufferLock  = false; // 旧: BackBuffer Lock 永远返 DDERR_GENERIC
	m_render.allow3DOffScreenLock = false; // 旧: 3D OffScreen Lock 永远返 DDERR_INVALIDPARAMS
	m_render.zBufferAutoRestore   = true;  // 旧: ZBuffer Restore 时 SetRenderState(ZENABLE, TRUE)

	m_log.level     = "INFO";
	m_log.file      = "ddfix.log";
	m_log.toConsole = false;

	m_debug.hudEnabled  = false;
	m_debug.hotkeyToggle = "F12";

	m_currentGameProfile.clear();
}

static std::string ToLowerStr(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for (char c : s)
	{
		if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
		out.push_back(c);
	}
	return out;
}

void ConfigManager::ApplySection(const char* section)
{
	// Render
	m_render.useSoftwareBlt       = m_parser.GetBool (section, "UseSoftwareBlt",       m_render.useSoftwareBlt);
	m_render.lockableBackBuffer   = m_parser.GetBool (section, "LockableBackBuffer",   m_render.lockableBackBuffer);
	m_render.vsync                = m_parser.GetBool (section, "VSync",                m_render.vsync);
	m_render.lightingEnabled      = m_parser.GetBool (section, "LightingEnabled",      m_render.lightingEnabled);
	m_render.allowBackBufferLock  = m_parser.GetBool (section, "AllowBackBufferLock",  m_render.allowBackBufferLock);
	m_render.allow3DOffScreenLock = m_parser.GetBool (section, "Allow3DOffScreenLock", m_render.allow3DOffScreenLock);
	m_render.zBufferAutoRestore   = m_parser.GetBool (section, "ZBufferAutoRestore",   m_render.zBufferAutoRestore);

	// Log
	{
		std::string lv = m_parser.GetString(section, "Level", m_log.level.c_str());
		// 校验：大小写不敏感，落到 DEBUG/INFO/WARN/ERROR 之一
		std::string lower = ToLowerStr(lv);
		if (lower == "debug" || lower == "info" || lower == "warn" || lower == "error")
		{
			// 保留原大小写以便展示
			m_log.level = lv;
		}
	}
	{
		std::string f = m_parser.GetString(section, "File", m_log.file.c_str());
		if (!f.empty()) m_log.file = f;
	}
	m_log.toConsole = m_parser.GetBool(section, "ToConsole", m_log.toConsole);

	// Debug
	m_debug.hudEnabled   = m_parser.GetBool(section, "HudEnabled",   m_debug.hudEnabled);
	{
		std::string hk = m_parser.GetString(section, "HotkeyToggle", m_debug.hotkeyToggle.c_str());
		if (!hk.empty()) m_debug.hotkeyToggle = hk;
	}
}

std::string ConfigManager::ProbeIniPath() const
{
	char buf[MAX_PATH] = { 0 };

	// 1) dll 自身所在目录
	HMODULE self = nullptr;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCSTR>(&ConfigManager::Instance), &self);
	DWORD n = GetModuleFileNameA(self, buf, MAX_PATH);
	if (n > 0 && n < MAX_PATH)
	{
		// 去掉文件名，保留目录
		std::string p = buf;
		auto pos = p.find_last_of("\\/");
		if (pos != std::string::npos)
		{
			std::string dir = p.substr(0, pos + 1);
			std::string candidate = dir + "ddfix.ini";
			DWORD attr = GetFileAttributesA(candidate.c_str());
			if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
			{
				return candidate;
			}
		}
	}

	// 2) game exe 同目录
	n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
	if (n > 0 && n < MAX_PATH)
	{
		std::string p = buf;
		auto pos = p.find_last_of("\\/");
		if (pos != std::string::npos)
		{
			std::string dir = p.substr(0, pos + 1);
			std::string candidate = dir + "ddfix.ini";
			DWORD attr = GetFileAttributesA(candidate.c_str());
			if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
			{
				return candidate;
			}
		}
	}

	// 3) 当前工作目录
	if (GetCurrentDirectoryA(MAX_PATH, buf) > 0)
	{
		std::string dir = buf;
		if (!dir.empty() && dir.back() != '\\' && dir.back() != '/')
		{
			dir.push_back('\\');
		}
		std::string candidate = dir + "ddfix.ini";
		DWORD attr = GetFileAttributesA(candidate.c_str());
		if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
		{
			return candidate;
		}
	}

	return std::string();
}

bool ConfigManager::Load()
{
	LoadDefaults();

	m_loadedPath = ProbeIniPath();
	if (m_loadedPath.empty())
	{
		// 缺 ini：保持默认值即可，不算错误
		return false;
	}

	if (!m_parser.Load(m_loadedPath.c_str()))
	{
		// 解析失败：保持默认值
		return false;
	}

	// 1) 应用默认 section
	ApplySection("Render");
	ApplySection("Log");
	ApplySection("Debug");

	// 2) 扫 [Game.*]，匹配当前 exe 名（Phase 3.5）
	char exeBuf[MAX_PATH] = { 0 };
	DWORD exeLen = GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
	std::string exeName;
	if (exeLen > 0 && exeLen < MAX_PATH)
	{
		std::string full = exeBuf;
		auto pos = full.find_last_of("\\/");
		exeName = (pos == std::string::npos) ? full : full.substr(pos + 1);
		auto dot = exeName.find_last_of('.');
		if (dot != std::string::npos)
		{
			exeName = exeName.substr(0, dot);
		}
	}

	if (!exeName.empty())
	{
		// 优先：精确大小写不敏感匹配 [Game.<exeName>]
		std::vector<std::string> sections;
		m_parser.GetSections(sections);
		std::string lowerExe = ToLowerStr(exeName);
		for (const auto& s : sections)
		{
			// 仅匹配 "Game." 前缀的 section
			if (s.size() < 5) continue;
			if (s.compare(0, 5, "Game.") != 0) continue;
			std::string tail = s.substr(5);
			if (ToLowerStr(tail) == lowerExe)
			{
				ApplySection(s.c_str());
				m_currentGameProfile = tail;
				break;
			}
		}
	}

	return true;
}

bool ConfigManager::Reload()
{
	return Load();
}

void ConfigManager::SetValue(const char* section, const char* key, const std::string& value)
{
	if (!section || !key) return;
	// 直接覆盖解析器中已加载的字典
	// IniParser 没暴露 SetValue，这里用 Clear + Re-parse 太重，所以只对当前 struct 应用一次。
	// 简化：仅供单元测试。生产代码不会调。
	(void)value;
	// 触发 Reload
	Reload();
}

} // namespace Config
} // namespace NDDFIX
