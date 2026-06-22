#include <windows.h>
#include <ddraw.h>
#include "dinput/dinput.h"
#undef genericQueryInterface
#include "ddraw/ddraw.h"
#undef genericQueryInterface
#include "dshow/dshow.h"
#undef genericQueryInterface
#include <minhook/include/MinHook.h>

#include <string>
#include <functional>
#include <list>
#include <map>

#include "Config/ConfigManager.h"
#include "Debug/HudRenderer.h"

std::ofstream Log::LOG("ddfix.log");
AddressLookupTable<void> ProxyAddressLookupTable = AddressLookupTable<void>(nullptr);

class MinHookPP
{
	friend class MinHookPPMgr;
public:
	MinHookPP(HMODULE module, const char* procName, void* detoursFunction)
	{
		m_hmodule = module;
		m_address = GetProcAddress(module, procName);
		m_procName = procName;
		m_detoursFunction = detoursFunction;

		m_orignal = nullptr;
		m_created = false;
		m_enabled = false;
	}

	~MinHookPP()
	{
		if (m_created)
		{
			RemoveHook();
		}
	}

	bool CreateHook()
	{
		bool result = false;
		if (!m_created)
		{
			MH_STATUS status = MH_CreateHook(m_address, m_detoursFunction, &m_orignal);
			if (status == MH_STATUS::MH_OK)
			{
				m_created = true;
				m_enabled = false;

				result = true;
			}
		}
		return result;
	}

	bool Enable()
	{
		bool result = false;
		if (m_created && !m_enabled)
		{
			MH_STATUS status = MH_EnableHook(m_address);
			if (status == MH_STATUS::MH_OK)
			{
				m_enabled = true;

				result = true;
			}
		}
		return result;
		
	}

	bool Disable()
	{
		bool result = false;
		if (m_created && m_enabled)
		{
			MH_STATUS status = MH_DisableHook(m_address);
			if (status == MH_STATUS::MH_OK)
			{
				m_enabled = false;

				result = true;
			}
		}
		return result;
	}

	bool RemoveHook()
	{
		bool result = false;
		if (m_created)
		{
			MH_STATUS status = MH_RemoveHook(m_address);
			if (status == MH_STATUS::MH_OK)
			{
				m_created = false;
				m_enabled = false;

				result = true;
			}
		}
		return result;
	}

	template<typename T>
	T* GetOrignalFunctionAddress() const
	{
		if (m_created && m_enabled)
		{
			return static_cast<T*>(m_orignal);
		}
		else
		{
			return nullptr;
		}
	}
private:
	HMODULE m_hmodule;
	void* m_address;
	std::string m_procName;
	void* m_detoursFunction;
	void* m_orignal;

	bool m_created;
	bool m_enabled;
	
};

class MinHookPPMgr final
{
	MinHookPPMgr()
	{
		MH_Initialize();
	}
public:
	static MinHookPPMgr* Instance()
	{
		static MinHookPPMgr mgr;
		return &mgr;
	}

	~MinHookPPMgr()
	{
		m_hookers.clear();
		MH_Uninitialize();
	}

	MinHookPP* CreateHooker(HMODULE module, const char* procName, void* detoursFunction)
	{
		MinHookPP* result = nullptr;
		if (HasHooked(module, procName))
		{
			result = nullptr;
		}
		else
		{
			m_hookers.emplace_back(module, procName, detoursFunction);
			result = &m_hookers.back();
		}
		return result;
	}

	bool HasHooked(HMODULE module, const char* procName) const
	{
		bool result = false;
		for (auto& hooker : m_hookers)
		{
			if (hooker.m_hmodule == module && hooker.m_procName == procName)
			{
				result = true;
				break;
			}
		}

		return result;
	}

	MinHookPP* GetHooker(HMODULE module, const char* procName)
	{
		MinHookPP* result = nullptr;
		for (auto& hooker : m_hookers)
		{
			if ((module ? ( hooker.m_hmodule == module ) : true)  && hooker.m_procName == procName)
			{
				result = &hooker;
				break;
			}
		}

		return result;
	}

private:
	std::list<MinHookPP> m_hookers;
};

namespace DDRAW_HOOK
{
	static void* m_acquireDDThreadLock;
	static void* m_d3DParseUnknownCommand;
	static void* m_dDInternalLock;
	static void* m_dDInternalUnlock;
	static void* m_directDrawCreate;
	static void* m_directDrawEnumerateA;
	static void* m_releaseDDThreadLock;

	static void CollectOrignalProcAddress()
	{
		char path[MAX_PATH];
		GetSystemDirectoryA(path, MAX_PATH);
		strcat_s(path, "\\ddraw.dll");
		HMODULE ddraw_original = LoadLibraryA(path);
		m_acquireDDThreadLock = GetProcAddress(ddraw_original, "AcquireDDThreadLock");
		m_d3DParseUnknownCommand = GetProcAddress(ddraw_original, "D3DParseUnknownCommand");
		m_dDInternalLock = GetProcAddress(ddraw_original, "DDInternalLock");
		m_dDInternalUnlock = GetProcAddress(ddraw_original, "DDInternalUnlock");
		m_directDrawCreate = GetProcAddress(ddraw_original, "DirectDrawCreate");
		m_directDrawEnumerateA = GetProcAddress(ddraw_original, "DirectDrawEnumerateA");
		m_releaseDDThreadLock = GetProcAddress(ddraw_original, "ReleaseDDThreadLock");
	}

	// Phase 8.14: x64 编译时 MSVC 不支持 __asm / __declspec(naked)。
	//   x86 保持原 naked + __asm jmp 行为（无栈帧 + 直接跳到 hook 目标）
	//   x64 改成普通函数 + reinterpret_cast 函数指针调用（多一层栈帧，但 hook 流程一般可接受）
#if defined(_M_IX86)
	void __declspec(naked) FakeAcquireLock() {
		__asm jmp DDRAW_HOOK::m_acquireDDThreadLock;
	}
	void __declspec(naked) FakeParseUnknown() {
		__asm jmp DDRAW_HOOK::m_d3DParseUnknownCommand;
	}
	void __declspec(naked) FakeInternalLock() {
		__asm jmp DDRAW_HOOK::m_dDInternalLock;
	}
	void __declspec(naked) FakeInternalUnlock() {
		__asm jmp DDRAW_HOOK::m_dDInternalUnlock;
	}
	void __declspec(naked) FakeReleaseLock() {
		__asm jmp DDRAW_HOOK::m_releaseDDThreadLock;
	}
#else
	// x64 fallback
	void FakeAcquireLock() { reinterpret_cast<void(*)()>(DDRAW_HOOK::m_acquireDDThreadLock)(); }
	void FakeParseUnknown() { reinterpret_cast<void(*)()>(DDRAW_HOOK::m_d3DParseUnknownCommand)(); }
	void FakeInternalLock() { reinterpret_cast<void(*)()>(DDRAW_HOOK::m_dDInternalLock)(); }
	void FakeInternalUnlock() { reinterpret_cast<void(*)()>(DDRAW_HOOK::m_dDInternalUnlock)(); }
	void FakeReleaseLock() { reinterpret_cast<void(*)()>(DDRAW_HOOK::m_releaseDDThreadLock)(); }
#endif

	HRESULT WINAPI FakeDirectDrawCreate(GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter) {
		//HRESULT hr = reinterpret_cast<decltype(DirectDrawCreate)*>(DDRAW_HOOK::m_directDrawCreate)(lpGUID, lplpDD, pUnkOuter);
// 		if (SUCCEEDED(hr))
// 		{
// 			*lplpDD = ProxyAddressLookupTable.FindAddress<m_IDirectDraw>(*lplpDD);
// 		}
// 		return hr;

		std::shared_ptr<WrapperLookupTable<void>> WrapperAddressLookupTable;
		WrapperAddressLookupTable = std::make_shared<WrapperLookupTable<void>>(nullptr);
		*lplpDD = WrapperAddressLookupTable->FindWrapper<m_IDirectDraw>(IID_IDirectDraw);
		return S_OK;
	}

	void __declspec(naked) FakeDirectDrawEnumerateA() {
		__asm jmp DDRAW_HOOK::m_directDrawEnumerateA;
	}
};

namespace DINPUT_HOOK
{
	static HRESULT WINAPI FakeDirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA *ppDI, LPUNKNOWN punkOuter)
	{
		auto hooker = MinHookPPMgr::Instance()->GetHooker(nullptr, "DirectInputCreateA");
		HRESULT hr = hooker->GetOrignalFunctionAddress<decltype(DirectInputCreateA)>()(hinst, dwVersion, ppDI, punkOuter);

		if (SUCCEEDED(hr))
		{
			*ppDI = ProxyAddressLookupTable.FindAddress<m_IDirectInputA>(*ppDI);
		}

		return hr;
	}

	static HRESULT WINAPI FakeDirectInputCreateEx(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter)
	{
		auto hooker = MinHookPPMgr::Instance()->GetHooker(nullptr, "DirectInputCreateEx");
		HRESULT WINAPI MyDirectInput8Create(HINSTANCE hinst, DWORD dwVersion, LPVOID *ppvOut, LPUNKNOWN punkOuter);

		HRESULT hr = MyDirectInput8Create(hinst, 0x0800, ppvOut, punkOuter);

		if (SUCCEEDED(hr))
		{
			genericDinputQueryInterface(riidltf, ppvOut);
		}

		return hr;
	}

	static void Hook()
	{
		char path[MAX_PATH];
		GetSystemDirectoryA(path, MAX_PATH);
		strcat_s(path, "\\dinput.dll");
		HMODULE dinput = LoadLibraryA(path);
		std::vector<MinHookPP*> hookers;
		hookers.push_back(MinHookPPMgr::Instance()->CreateHooker(dinput, "DirectInputCreateA", &FakeDirectInputCreateA));
		hookers.push_back(MinHookPPMgr::Instance()->CreateHooker(dinput, "DirectInputCreateEx", &FakeDirectInputCreateEx));
		for(auto hooker : hookers)
		{
			hooker->CreateHook();
			hooker->Enable();
		}
	}

};

namespace DSHOW_HOOK
{
	HRESULT WINAPI FakeCoCreateInstance(_In_ REFCLSID rclsid, _In_opt_ LPUNKNOWN pUnkOuter, _In_ DWORD dwClsContext, _In_ REFIID riid, _COM_Outptr_ _At_(*ppv, _Post_readable_size_(_Inexpressible_(varies))) LPVOID FAR* ppv)
	{
		auto hooker = MinHookPPMgr::Instance()->GetHooker(nullptr, "CoCreateInstance");
		HRESULT hr = hooker->GetOrignalFunctionAddress<decltype(CoCreateInstance)>()(rclsid, pUnkOuter, dwClsContext, riid, ppv);

		if (SUCCEEDED(hr))
		{
			if (rclsid == CLSID_FilterGraph)
			{
				if (riid == IID_IFilterGraph)
				{
					*ppv = ProxyAddressLookupTable.FindAddress<m_IFilterGraph>(*ppv);
				}
				else if (riid == IID_IGraphBuilder)
				{
					*ppv = ProxyAddressLookupTable.FindAddress<m_IGraphBuilder>(*ppv);
				}
			}
			if (riid == IID_IBaseFilter)
			{
				int a = 0;
				a = 1;
			}
		}

		return hr;

	}

	static void Hook()
	{
		char path[MAX_PATH];
		GetSystemDirectoryA(path, MAX_PATH);
		strcat_s(path, "\\Ole32.dll");
		HMODULE dinput = LoadLibraryA(path);
		std::vector<MinHookPP*> hookers;
		hookers.push_back(MinHookPPMgr::Instance()->CreateHooker(dinput, "CoCreateInstance", &FakeCoCreateInstance));
		for (auto hooker : hookers)
		{
			hooker->CreateHook();
			hooker->Enable();
		}
	}
}

// Phase 4.4: F12 切换 HUD 可见性。
// 设计要点：
//   - 用 SetWindowsHookEx(WH_KEYBOARD_LL) 监听全局键盘事件。
//   - 钩子回调里检查 ConfigManager::Instance()->GetDebug().hudEnabled，
//     关闭时直接 return 0（放行键事件到目标应用）。
//   - 仅当 hudEnabled==true 时安装钩子；缺省 false → 零开销。
//   - 注意：DLL_PROCESS_DETACH 必须 UnhookWindowsHookEx，否则 host 进程会闪退。
//   - 注意：F12 按下后 CallNextHookEx 仍调，让游戏也收到该事件（避免影响游戏内 F12 默认行为）。
namespace DEBUG_HOOK
{
	static HHOOK   g_hHook       = nullptr;
	static HMODULE g_hModule     = nullptr;
	static bool    g_hudEnabled  = false;

	static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
	{
		if (nCode == HC_ACTION && g_hudEnabled)
		{
			const auto* p = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
			// 仅处理 key down（防止长按 F12 反复 toggle）
			if (wParam == WM_KEYDOWN && p->vkCode == VK_F12)
			{
				NDDFIX::Debug::HudRenderer::Instance()->ToggleVisible();
				Log() << "F12 pressed, HUD visible=" << (NDDFIX::Debug::HudRenderer::Instance()->IsVisible() ? 1 : 0);
			}
		}
		// 必须调 CallNextHookEx，否则游戏/系统会卡键
		return CallNextHookEx(g_hHook, nCode, wParam, lParam);
	}

	static void Install()
	{
		if (g_hHook)
		{
			return;
		}
		// 仅当 ConfigManager 启用 HUD 时才安装（缺省 false → 不挂任何钩）
		g_hudEnabled = NDDFIX::Config::ConfigManager::Instance()->GetDebug().hudEnabled;
		if (!g_hudEnabled)
		{
			return;
		}
		g_hModule = GetModuleHandleA(nullptr); // 当前 DLL 句柄
		g_hHook = SetWindowsHookExA(WH_KEYBOARD_LL, &LowLevelKeyboardProc, g_hModule, 0);
		if (g_hHook)
		{
			Log() << "DEBUG_HOOK: F12 hook installed (HudEnabled=true)";
		}
		else
		{
			Log() << "DEBUG_HOOK: SetWindowsHookExA failed, last error=" << GetLastError();
		}
	}

	static void Uninstall()
	{
		if (g_hHook)
		{
			UnhookWindowsHookEx(g_hHook);
			g_hHook = nullptr;
			Log() << "DEBUG_HOOK: F12 hook uninstalled";
		}
		g_hudEnabled = false;
	}
}


BOOL WINAPI DllMain(
	_In_ HINSTANCE hinstDLL,
	_In_ DWORD     fdwReason,
	_In_ LPVOID    lpvReserved
)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		// Phase 3.2: 尽早加载配置，使后续 hook / wrapper 都能读到开关。
		// 缺 ddfix.ini 不报错，所有结构体取默认值（与旧硬编码一致）。
		NDDFIX::Config::ConfigManager::Instance()->Load();
		DDRAW_HOOK::CollectOrignalProcAddress();
		DINPUT_HOOK::Hook(); // 如果不Hook，会导致调试困难
		DSHOW_HOOK::Hook(); // 避免DShow里使用DX6的接口
		// Phase 4.4: 安装 F12 切换钩子。内部按 ConfigManager.HudEnabled 判定是否真挂。
		DEBUG_HOOK::Install();
		break;
	case DLL_PROCESS_DETACH:
		// Phase 4.4: 卸载键盘钩，避免 host 进程闪退。
		DEBUG_HOOK::Uninstall();
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	default:
		break;
	}
	return TRUE;
}