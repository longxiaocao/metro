// Phase 3.2: ConfigManager 单例
// 设计目标：
//   - 单例；由 dllmain.cpp DllMain 在 hook 装载前调 Load()
//   - 暴露 3 个 struct + 默认值 fallback（与现有硬编码一致）
//   - 加载失败 / 缺失文件 / 缺失 key → 全部静默用默认值
//   - 文件查找顺序：dll 同目录 → game exe 同目录 → 当前工作目录
//   - 支持 [Game.<exe_name>] 覆盖当前 section（详见 Load()）

#pragma once

#include "IniParser.h"

#include <string>

namespace NDDFIX
{
namespace Config
{

struct RenderConfig
{
	bool useSoftwareBlt;          // 替代 g_useSoftwareWrapper9（默认 false）
	bool lockableBackBuffer;      // 替代 D3DPRESENTFLAG_LOCKABLE_BACKBUFFER（默认 true）
	bool vsync;                   // 替代 D3DPRESENT_INTERVAL_IMMEDIATE（默认 false = 立即）
	bool lightingEnabled;         // 替代 D3DRS_LIGHTING（默认 true）
	bool allowBackBufferLock;     // 替代 BackBuffer Lock 行为（默认 false = 强制返错）
	bool allow3DOffScreenLock;    // 替代 3D OffScreen Lock 行为（默认 false = 强制返错）
	bool zBufferAutoRestore;      // 替代 ZBuffer Restore 时开 Z（默认 true）
};

struct LogConfig
{
	std::string level;            // "DEBUG" / "INFO" / "WARN" / "ERROR"（默认 INFO）
	std::string file;             // 日志文件路径（默认 ddfix.log）
	bool toConsole;               // 是否同时输出到 stdout（默认 false）
};

struct DebugConfig
{
	bool hudEnabled;              // 是否启用调试 HUD（默认 false，Phase 4 消费）
	std::string hotkeyToggle;     // HUD 切换快捷键（默认 "F12"）
};

class ConfigManager
{
public:
	static ConfigManager* Instance();

	// 加载配置。会探测 ddfix.ini；找不到不报错，所有结构体取默认。
	// 在 DllMain::DLL_PROCESS_ATTACH 调一次即可。
	bool Load();

	// 热重载：重新扫描文件并刷新结构体。可选，按需调用。
	bool Reload();

	// 当前活跃配置
	const RenderConfig& GetRender() const { return m_render; }
	const LogConfig&    GetLog()    const { return m_log; }
	const DebugConfig&  GetDebug()  const { return m_debug; }

	// 调试/测试用：直接替换某个 section 的值（不写盘）。
	void SetValue(const char* section, const char* key, const std::string& value);

	// 当前加载的 ini 路径（找不到时为空串）
	const std::string& GetLoadedPath() const { return m_loadedPath; }

private:
	ConfigManager();
	~ConfigManager();
	ConfigManager(const ConfigManager&) = delete;
	ConfigManager& operator=(const ConfigManager&) = delete;

	void LoadDefaults();
	void ApplySection(const char* section);
	// 找 ddfix.ini 探测路径：dll 目录、game exe 目录、cwd
	std::string ProbeIniPath() const;

	IniParser m_parser;
	RenderConfig m_render;
	LogConfig    m_log;
	DebugConfig  m_debug;
	std::string  m_loadedPath;
	std::string  m_currentGameProfile; // 例如 "MeteorBlade"
};

} // namespace Config
} // namespace NDDFIX
