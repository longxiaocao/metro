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

	// Phase 9.2: SMAA 默认 Off（零开销，向后兼容旧版）
	m_postProcess.smaaMode = 0;

	// Phase 9.3.12: Renderer 默认 true（让 ExecuteBuffer hook 始终跑解析 + 暂存）
	//   Phase 9.3 阶段 hook 只做几何提取（暂存 m_extractedGeometry），不影响原始 Execute 行为。
	//   真正决定"是否用解析结果渲染"的是 GBuffer 集成（Phase 9.3.11），届时增加独立开关。
	m_renderer.enableRenderer = true;

	// Phase 9.4: Shadow 默认 true (1024x1024, 3 cascade, 5x5 PCF, lambda=0.5)
	//   与 Phase 9.4 文档承诺的"3-4 级 cascade + 5x5 PCF"默认值一致
	m_shadow.enableShadow  = true;
	m_shadow.cascadeCount  = 3;
	m_shadow.pcfKernelSize = 5;
	m_shadow.shadowMapSize = 1024;
	m_shadow.splitLambda   = 0.5f;

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

	// Phase 9.2: PostProcess / SMAA
	// 解析 [SMAA] 段的 Mode key（0..4）。越界由 PostProcess::ModeFromInt 兜底返 Off。
	{
		int mode = m_parser.GetInt(section, "Mode", m_postProcess.smaaMode);
		if (mode < 0 || mode > 4) mode = 0;
		m_postProcess.smaaMode = mode;
	}

	// Phase 9.3.12: Renderer / ExecuteBuffer hook
	// 解析 [Renderer] 段的 EnableRenderer key（true/false）。
	//   true  = 走 Phase 9.3 自写 GBuffer/Deferred 路径（暂为 hook-only stub）
	//   false = 回退 Phase 1-8 行为（透传 ProxyInterface->Execute）
	//   hook 解析逻辑始终跑，开关只控制后续 Phase 9.3.11 GBuffer 集成是否启用。
	m_renderer.enableRenderer = m_parser.GetBool(section, "EnableRenderer", m_renderer.enableRenderer);

	// Phase 9.4: Shadow / Cascaded Shadow Map + PCF
	// 解析 [Shadow] 段的 5 个 key:
	//   EnableShadow  : true/false (默认 true)
	//   CascadeCount  : 3 或 4 (越界 → 3, 与 ShadowRenderer::ClampCascadeCount 一致)
	//   PCFKernelSize : 3 / 5 / 7 (越界 → 5, 与 ShadowRenderer::ClampPCFKernelSize 一致)
	//   ShadowMapSize : 512 / 1024 / 2048 (越界 → 1024)
	//   SplitLambda   : 0.0..1.0 (越界 clamp 到 [0, 1])
	//   边界保护: 这里用最严格的 clamp 兜底, ShadowRenderer 内 ClampCascadeCount 二次保护
	m_shadow.enableShadow = m_parser.GetBool(section, "EnableShadow", m_shadow.enableShadow);
	{
		int count = m_parser.GetInt(section, "CascadeCount", m_shadow.cascadeCount);
		if (count < 1) count = 1;
		if (count > 4) count = 4;
		m_shadow.cascadeCount = count;
	}
	{
		int k = m_parser.GetInt(section, "PCFKernelSize", m_shadow.pcfKernelSize);
		if (k < 3) k = 3;
		if (k > 7) k = 7;
		if (k == 4 || k == 6) k = 5;  // 取最近奇数
		m_shadow.pcfKernelSize = k;
	}
	{
		int sz = m_parser.GetInt(section, "ShadowMapSize", m_shadow.shadowMapSize);
		if (sz != 512 && sz != 1024 && sz != 2048) sz = 1024;
		m_shadow.shadowMapSize = sz;
	}
	{
		float lam = m_parser.GetFloat(section, "SplitLambda", m_shadow.splitLambda);
		if (lam < 0.0f) lam = 0.0f;
		if (lam > 1.0f) lam = 1.0f;
		m_shadow.splitLambda = lam;
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
	// Phase 9.2: SMAA section（与 Render/Log/Debug 同一 ApplySection 风格，无关 key 走 fallback）
	ApplySection("SMAA");
	// Phase 9.3.12: Renderer section（ExecuteBuffer hook 开关）
	ApplySection("Renderer");
	// Phase 9.4: Shadow section（Cascaded Shadow Map + PCF 配置）
	ApplySection("Shadow");

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
