#include "D3D9Context.h"
#include "Common/Logging.h"
#include "Config/ConfigManager.h"
#include "Debug/HudRenderer.h"
// Phase 8.25.15: 改用直接 include ColorKeyHLSLC.h, 不再用 forward declare.
//   根因 (CI #42 LNK2019 unresolved external `g_colorKeyHLSLC`):
//     D3D9Context.cpp line 563 之前用 `extern const DWORD g_colorKeyHLSLC[];` forward declare,
//     期望 IDirectDrawSurface4.cpp 的 #include "ColorKeyHLSLC.h" 提供定义, link 时能找到。
//     但 CI 上 ddfix-static target 的 add_custom_command 触发 fxc 生成 ColorKeyHLSLC.h 时机
//     不确定 (VS multi-config generator + ddfix-static 在 ddfix 之前构建), 导致 ddfix 阶段
//     ColorKeyHLSLC.h 不存在 / 为空, IDirectDrawSurface4.cpp 中 g_colorKeyHLSLC 没有真实定义,
//     D3D9Context.obj 链接时报 LNK2019。
//   修复: 直接在 D3D9Context.cpp #include "ColorKeyHLSLC.h", 让 D3D9Context.obj 自己也持有
//     g_colorKeyHLSLC 数组定义。多个 TU include 同一 .h 在 C++ 中会引起 multiple definition,
//     除非 ColorKeyHLSLC.h 用 `inline const DWORD` 或 `static const DWORD` 限定。fxc 生成的
//     .h 是 `const DWORD g_colorKeyHLSLC[] = {...};` (无 inline/static), 在 C++ 中 const 全局
//     数组默认有 internal linkage (C++ 规则, 与 C 不同), 所以多 TU include 不会冲突。
//     验证方法: g++ / cl 编译 const T x[] 数组, 多个 TU include 不会 LNK4006。
#include "ColorKeyHLSLC.h"
// Phase 9.3.9: GBuffer 资源管理转发到 GBufferRenderer 单例。
//   GBufferRenderer 完整实现 4 RT + depth + 3 shader 管理; D3D9Context 这层
//   只暴露 8 个薄包装 API, 保持 D3D9Context.h API 表面整齐。
#include "ddraw/GBufferRenderer.h"
// Phase 9.4: Shadow Map 资源管理转发到 ShadowRenderer 单例。
//   ShadowRenderer 完整实现 N 张 shadow texture + shadow shader 管理;
//   D3D9Context 这层只暴露 8 个薄包装 API, 保持 D3D9Context.h API 表面整齐。
#define NDDFIX_DEBUG_INCLUDES
#include "ddraw/ShadowRenderer.h"
#undef NDDFIX_DEBUG_INCLUDES

using namespace ND3D9;

class OffscreenSurface9Factory final : public IResource9Factory
{
public:
	OffscreenSurface9Factory(int width, int height, D3DFORMAT format, D3DPOOL pool)
		: m_width(width)
		, m_height(height)
		, m_format(format)
		, m_pool(pool)
	{

	}

	virtual IUnknown* Create(D3D9Context* context) const override
	{
		IDirect3DSurface9* surface9 = nullptr;
		context->GetDevice()->CreateOffscreenPlainSurface(
			m_width,
			m_height,
			m_format,
			m_pool,
			&surface9,
			nullptr);
		return surface9;
	}

	virtual bool IsCreateInVideoMemory() const override
	{
		return m_pool == D3DPOOL_DEFAULT;
	}

	virtual std::string GetType() const override
	{
		return "OffScreen";
	}

private:
	int m_width;
	int m_height;
	D3DFORMAT m_format;
	D3DPOOL m_pool;
};

class ZBufferSurface9Factory final : public IResource9Factory
{
public:
	ZBufferSurface9Factory(int width, int height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL discard)
		: m_width(width)
		, m_height(height)
		, m_format(format)
		, m_multiSample(multiSample)
		, m_multisampleQuality(multisampleQuality)
		, m_discard(discard)
	{

	}

	virtual IUnknown* Create(D3D9Context* context) const override
	{
		IDirect3DSurface9* surface9 = nullptr;
		context->GetDevice()->CreateDepthStencilSurface(m_width, m_height, m_format, m_multiSample, m_multisampleQuality, m_discard, &surface9, nullptr);
		return surface9;
	}

	virtual bool IsCreateInVideoMemory() const override
	{
		return true;
	}

	virtual std::string GetType() const override
	{
		return "ZBuffer";
	}

private:
	int m_width;
	int m_height;
	D3DFORMAT m_format;
	D3DMULTISAMPLE_TYPE m_multiSample;
	DWORD m_multisampleQuality;
	BOOL m_discard;
};

class BackBuffer9Factory final : public IResource9Factory
{
public:
	BackBuffer9Factory()
	{

	}

	virtual IUnknown* Create(D3D9Context* context) const override
	{
		IDirect3DSurface9* backbuffer9 = nullptr;
		context->GetDevice()->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer9);
		return backbuffer9;
	}

	virtual bool IsCreateInVideoMemory() const override
	{
		return true;
	}

	virtual std::string GetType() const override
	{
		return "BackBuffer";
	}

};

class Texture9Factory final : public IResource9Factory
{
public:
	Texture9Factory(int width, int height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool)
		: m_width(width)
		, m_height(height)
		, m_levels(levels)
		, m_usage(usage)
		, m_format(format)
		, m_pool(pool)
	{

	}

	virtual IUnknown* Create(D3D9Context* context) const override
	{
		IDirect3DTexture9* tex9 = nullptr;
		context->GetDevice()->CreateTexture(m_width, m_height, m_levels, m_usage, m_format, m_pool, &tex9, nullptr);
		return tex9;
	}

	virtual bool IsCreateInVideoMemory() const override
	{
		return m_pool == D3DPOOL_DEFAULT;
	}

	virtual std::string GetType() const override
	{
		return "Texture";
	}

private:
	int m_width;
	int m_height;
	UINT m_levels;
	DWORD m_usage;
	D3DFORMAT m_format;
	D3DPOOL m_pool;
};

class RenderTarget9Factory final : public IResource9Factory
{
public:
	RenderTarget9Factory(int width, int height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL lockable)
		: m_width(width)
		, m_height(height)
		, m_format(format)
		, m_multiSample(multiSample)
		, m_multisampleQuality(multisampleQuality)
		, m_lockable(lockable)
	{

	}

	virtual IUnknown* Create(D3D9Context* context) const override
	{
		IDirect3DSurface9* surface9 = nullptr;
		context->GetDevice()->CreateRenderTarget(
			m_width,
			m_height,
			m_format,
			m_multiSample,
			m_multisampleQuality,
			m_lockable,
			&surface9,
			nullptr);
		return surface9;
	}

	virtual bool IsCreateInVideoMemory() const override
	{
		return true;
	}

	virtual std::string GetType() const override
	{
		return "RenderTarget";
	}

private:
	int m_width;
	int m_height;
	D3DFORMAT m_format;
	D3DMULTISAMPLE_TYPE m_multiSample;
	DWORD m_multisampleQuality;
	BOOL m_lockable;
};

class Sprite9Factory final : public IResource9Factory
{
public:
	Sprite9Factory()
	{

	}

	virtual IUnknown* Create(D3D9Context* context) const override
	{
		ID3DXSprite* sprite = nullptr;
		D3DXCreateSprite(context->GetDevice(), &sprite);
		return sprite;
	}

	virtual bool IsCreateInVideoMemory() const override
	{
		return true;
	}

	virtual std::string GetType() const override
	{
		return "Sprite";
	}
};

class VertexBuffer9Factory final : public IResource9Factory
{
public:
	VertexBuffer9Factory(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool)
		: m_length(length)
		, m_usage(usage)
		, m_fvf(fvf)
		, m_pool(pool)
	{

	}

	virtual IUnknown* Create(D3D9Context* context) const override
	{
		IDirect3DVertexBuffer9* vb9 = nullptr;
		context->GetDevice()->CreateVertexBuffer(m_length, m_usage, m_fvf, m_pool, &vb9, nullptr);
		return vb9;
	}

	virtual bool IsCreateInVideoMemory() const override
	{
		return m_pool == D3DPOOL_DEFAULT;
	}

	virtual std::string GetType() const override
	{
		return "VertexBuffer";
	}

private:
	UINT m_length;
	DWORD m_usage;
	DWORD m_fvf;
	D3DPOOL m_pool;
};

D3D9Context::~D3D9Context()
{
	logf("%d resource not releases.", m_resAllocated.size());
}

Resource9Handle D3D9Context::LogResource(IResource9Factory* factory, IUnknown* pointer)
{
	m_resCountHistory += 1;
	Resource9Handle handle = m_resCountHistory;
	Resource9Info info = { 0 };
	info.handle = handle;
	info.factory = factory;
	info.pointer = pointer;
	m_resAllocated[handle] = info;
	return handle;
}

IDirect3D9* D3D9Context::GetD3D9() const
{
	return m_d3d9;
}

IDirect3DDevice9* D3D9Context::GetDevice() const
{
	return m_d3dDev9;
}

void D3D9Context::Uninitialize()
{
	if (m_d3d9)
	{
		// Phase 4.2: HUD 依赖设备，先释放。
		NDDFIX::Debug::HudRenderer::Instance()->Shutdown();
		m_d3dDev9->Release();
		m_d3d9->Release();
	}
}

void D3D9Context::Initialize(::HWND hwnd)
{
	assert(!m_d3d9);
	m_hwnd = hwnd;

	m_d3d9 = Direct3DCreate9(DIRECT3D_VERSION);

	CreateDevice();

	// Phase 3.3: LightingEnabled 配置驱动
	if (NDDFIX::Config::ConfigManager::Instance()->GetRender().lightingEnabled)
	{
		m_d3dDev9->SetRenderState(D3DRS_LIGHTING, TRUE);
		m_d3dDev9->SetRenderState(D3DRS_AMBIENT, 0xffffffff);
		m_d3dDev9->SetRenderState(D3DRS_COLORVERTEX, TRUE);
	}

	// Phase 4.2: HUD 初始化（D3DXFont 创建设备相关资源）。
	// 必须在 CreateDevice 之后调；设备丢失/重置由 ResetDevice 内部钩。
	NDDFIX::Debug::HudRenderer::Instance()->Initialize();

	// Phase 9.3.9: GBuffer 初始化（4 RT + depth + 3 shader）。
	//   失败 (caps 不支持 / 创 RT 失败 / 编译 shader 失败) → 内部 unavailable, 后续
	//   RenderFrame 退化为 no-op, 原始 Execute 路径继续可用。
	{
		int bbW = 0, bbH = 0;
		GetBackBufferSize(&bbW, &bbH);
		if (bbW > 0 && bbH > 0)
		{
			HRESULT gbufHr = CreateGBuffer(bbW, bbH);
			if (FAILED(gbufHr))
			{
				logf("D3D9Context::Initialize: GBuffer init failed, hr=0x%08X (will skip GBuffer pass)", gbufHr);
			}
		}
	}

	// Phase 9.4: Shadow Map 初始化（N 张 depth shadow texture + shadow shader）。
	//   失败 (创 texture 失败 / 编译 shader 失败) → 内部 unavailable, 后续
	//   RenderShadowMaps 退化为 no-op, 原始 Execute 路径继续可用。
	//   配置从 ConfigManager.GetShadow() 读 (默认 1024x1024, 3 cascade, 5x5 PCF)。
	{
		const auto& shadowCfg = NDDFIX::Config::ConfigManager::Instance()->GetShadow();
		if (shadowCfg.enableShadow)
		{
			HRESULT shadowHr = CreateShadowMaps(shadowCfg.shadowMapSize, shadowCfg.cascadeCount);
			if (FAILED(shadowHr))
			{
				logf("D3D9Context::Initialize: Shadow init failed, hr=0x%08X (will skip shadow pass)", shadowHr);
			}
		}
	}
}

D3D9Context* D3D9Context::Instance()
{
	static D3D9Context context;
	return &context;
}

D3D9Context::D3D9Context()
	: m_d3d9(nullptr)
	, m_d3dDev9(nullptr)
	, m_backBufferWidth(0)
	, m_backBufferHeight(0)
	// Phase 9.1: 初始化游戏请求模式为"未设置"
	, m_requestedWidth(0)
	, m_requestedHeight(0)
	, m_requestedBpp(0)
	, m_hasRequestedMode(false)
	, m_deviceLost(false)
	, m_hwnd(0)
	, m_resCountHistory(0)
	, m_backBuffer9Handle(0)
	, m_colorKeyShader(nullptr)
	, m_colorKeyConstantTable(nullptr)
	, m_colorKeyShaderInited(false)
{
}

void D3D9Context::GetBackBufferSize(int* width, int* height)
{
	*width = m_backBufferWidth;
	*height = m_backBufferHeight;
}

void D3D9Context::TagDeviceLost()
{
	m_deviceLost = true;
	// Phase 4.2: HUD 同步通知，让 ID3DXFont 释放内部 video memory 引用。
	NDDFIX::Debug::HudRenderer::Instance()->OnDeviceLost();
}

bool D3D9Context::IsDeviceLost() const
{
	return m_deviceLost;
}

HRESULT D3D9Context::ResetDevice()
{
	auto testResult = m_d3dDev9->TestCooperativeLevel();
	if (testResult == D3D_OK)
	{
		return D3D_OK;
	}
	else if (testResult != D3DERR_DEVICENOTRESET)
	{
		return D3DERR_DEVICENOTRESET;
	}

	// Phase 9.3.9: Reset 前释放 GBuffer（4 RT + depth + 3 shader）。
	//   D3D9 Reset 会丢弃所有内部 surface/texture/shader 引用, 提前 Shutdown
	//   让 GBufferRenderer 知道自己已被 invalidate, Reset 后由 CreateGBuffer 重建。
	//   WHY 这里 (而不是 Reset 后): Shutdown 必须在 m_d3dDev9->Reset() 前调,
	//   否则 D3D9 内部对象会失效, Release 时崩溃。
	{
		auto* gbuf = NDDFIX::Render::GBufferRenderer::Instance();
		if (gbuf->IsAvailable())
		{
			int prevW = gbuf->GetWidth();
			int prevH = gbuf->GetHeight();
			gbuf->Shutdown();
			logf("D3D9Context::ResetDevice: GBuffer released (was %dx%d), will rebuild", prevW, prevH);
		}
	}

	auto oldResAllocated = m_resAllocated;
	for (auto& kv : oldResAllocated)
	{
		const auto& info = kv.second;
		if (info.factory->IsCreateInVideoMemory())
		{
			auto ptr = GetResource9(info.handle, nullptr); // internal will AddRef
			// P0 修复: 之前 `refs = ptr->Release(); refs = ptr->Release(); assert(refs == 0);`
			// D3D9 内部对象经常有 1 个未释放的内部 ref，二次 Release 不会归零，assert 直接挂。
			// 改为 while 循环 + log warning，保证不崩。
			ULONG refs = ptr->Release();
			while (refs > 0)
			{
				refs = ptr->Release();
			}
			if (refs != 0)
			{
				logf("D3D9Context::ResetDevice warning: resource handle=%d refcount=%u (expected 0)", info.handle, refs);
			}
		}
	}

	// Phase 4.2: 在 Reset 之前通知 HUD 释放 video memory 引用。
	// 必须先于 m_d3dDev9->Reset()，否则 D3D9 内部对象会失效。
	NDDFIX::Debug::HudRenderer::Instance()->OnDeviceLost();

	D3DPRESENT_PARAMETERS d3dpp;
	BuildD3DPresentParameters(d3dpp);
	HRESULT hr = m_d3dDev9->Reset(&d3dpp);
	if (SUCCEEDED(hr))
	{
		m_deviceLost = false;

		for (auto& kv : m_resAllocated)
		{
			const auto& info = kv.second;
			if (info.factory->IsCreateInVideoMemory())
			{
				RebuildResource9(info.handle);
			}
		}

		// Phase 2.3: D3D9 Reset 会丢弃所有内部 PS / ConstantTable 引用，
	// 重置懒加载标志，下次 GetSharedColorKeyShader() 触发重新编译。
	// 旧指针在 D3D9 内部已被清理，外部只持有裸指针，无需 Release。
	m_colorKeyShader = nullptr;
	m_colorKeyConstantTable = nullptr;
	m_colorKeyShaderInited = false;

	// Phase 4.2: HUD 资源重新挂回设备（D3DXFont::OnResetDevice）。
	NDDFIX::Debug::HudRenderer::Instance()->OnDeviceReset();

	// Phase 9.3.9: Reset 成功后, 尝试按 back buffer 尺寸重建 GBuffer。
	//   失败 (caps 不支持 / 创 RT 失败) → 内部 unavailable, 原始路径继续可用。
	{
		int bbW = 0, bbH = 0;
		GetBackBufferSize(&bbW, &bbH);
		if (bbW > 0 && bbH > 0)
		{
			HRESULT gbufHr = CreateGBuffer(bbW, bbH);
			if (FAILED(gbufHr))
			{
				logf("D3D9Context::ResetDevice: GBuffer rebuild failed, hr=0x%08X (will skip GBuffer pass)", gbufHr);
			}
		}
	}

	return D3D_OK;
	}
	else
	{
		return hr;
	}
}

void D3D9Context::CalcBackBufferSize()
{
	// Phase 9.1: 优先使用游戏请求的显示模式（来自 IDirectDraw::SetDisplayMode 拦截）
	// 解决 HP 槽 / UI 偏移问题：之前用显示器物理分辨率（如 1920x1080）作为 back buffer 尺寸，
	// 导致游戏在 800x600 内部坐标系画的 HP 槽位置（350, 560）出现在 back buffer 的左上角
	// 而不是游戏期望的"屏幕底部居中"。
	//
	// 修法：拦截 SetDisplayMode 后，存储游戏请求的 width/height；
	//      CalcBackBufferSize 优先用请求尺寸；fallback 到显示器分辨率（保持原行为）。
	//
	// 引用：docs/literature/PUBLIC_LITERATURE.md
	//       Direct3D 9 官方文档 D3DPRESENT_PARAMETERS
	if (m_hasRequestedMode && m_requestedWidth > 0 && m_requestedHeight > 0)
	{
		m_backBufferWidth = m_requestedWidth;
		m_backBufferHeight = m_requestedHeight;
		logf("D3D9Context::CalcBackBufferSize: using requested mode %dx%d (game)",
			m_backBufferWidth, m_backBufferHeight);
		return;
	}

	// Fallback: 显示器物理分辨率（原行为）
	DEVMODE currmode;
	ZeroMemory(&currmode, sizeof(DEVMODE));
	currmode.dmSize = sizeof(DEVMODE);
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &currmode);
	m_backBufferWidth = currmode.dmPelsWidth;
	m_backBufferHeight = currmode.dmPelsHeight;
	logf("D3D9Context::CalcBackBufferSize: using display mode %dx%d (no game request)",
		m_backBufferWidth, m_backBufferHeight);
}

// Phase 9.1: 拦截 IDirectDraw::SetDisplayMode 时调此方法存储请求模式
void D3D9Context::SetRequestedMode(DWORD width, DWORD height, DWORD bpp)
{
	m_requestedWidth = (int)width;
	m_requestedHeight = (int)height;
	m_requestedBpp = (int)bpp;
	m_hasRequestedMode = true;
	logf("D3D9Context::SetRequestedMode: %dx%d %dbpp (will use as BackBuffer size)",
		m_requestedWidth, m_requestedHeight, m_requestedBpp);
}

void D3D9Context::GetRequestedMode(int* width, int* height, int* bpp) const
{
	if (width) *width = m_requestedWidth;
	if (height) *height = m_requestedHeight;
	if (bpp) *bpp = m_requestedBpp;
}

HRESULT D3D9Context::CreateDevice()
{
	CalcBackBufferSize();
	D3DPRESENT_PARAMETERS d3dpp;
	BuildD3DPresentParameters(d3dpp);
	HRESULT hr = m_d3d9->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &m_d3dDev9);

	auto backBufferFactory = new BackBuffer9Factory();
	auto backBuffer9 = backBufferFactory->Create(this);
	m_backBuffer9Handle = LogResource(backBufferFactory, backBuffer9);

	return hr;
}

void D3D9Context::BuildD3DPresentParameters(D3DPRESENT_PARAMETERS &d3dpp)
{
	// Phase 3.3: Render 段配置驱动，缺省值与原硬编码一致
	const auto& render = NDDFIX::Config::ConfigManager::Instance()->GetRender();
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.Windowed = TRUE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
	d3dpp.hDeviceWindow = (HWND)m_hwnd;
	d3dpp.BackBufferWidth = m_backBufferWidth;
	d3dpp.BackBufferHeight = m_backBufferHeight;
	d3dpp.Flags = render.lockableBackBuffer ? D3DPRESENTFLAG_LOCKABLE_BACKBUFFER : 0;
	d3dpp.EnableAutoDepthStencil = FALSE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	d3dpp.PresentationInterval = render.vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
}

Resource9Handle D3D9Context::CreateOffScreenSurface9(int width, int height, D3DFORMAT format, D3DPOOL pool)
{
	auto factory = new OffscreenSurface9Factory(width, height, format, pool);
	auto ptr = factory->Create(this);
	return LogResource(factory, ptr);
}

Resource9Handle D3D9Context::CreateZBufferSurface9(int width, int height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL discard)
{
	auto factory = new ZBufferSurface9Factory(width, height, format, multiSample, multisampleQuality, discard);
	auto ptr = factory->Create(this);
	return LogResource(factory, ptr);
}

Resource9Handle D3D9Context::GetBackBuffer9()
{
	return m_backBuffer9Handle;
}

Resource9Handle D3D9Context::CreateTexture9(int width, int height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool)
{
	auto factory = new Texture9Factory(width, height, levels, usage, format, pool);
	auto ptr = factory->Create(this);
	return LogResource(factory, ptr);
}

ULONG D3D9Context::ReleaseResource9(Resource9Handle handle)
{
	assert(handle > 0);
	auto itor = m_resAllocated.find(handle);
	if (itor != m_resAllocated.end())
	{
		auto info = m_resAllocated.at(handle);
		auto refs = info.pointer->Release();
		if (refs == 0)
		{
			delete info.factory;
			m_resAllocated.erase(itor);
		}
		return refs;
	}
	else
	{
		assert(false);
		return 0;
	}
}

void D3D9Context::RebuildResource9(Resource9Handle handle)
{
	auto& info = m_resAllocated.at(handle);
	auto newPtr = info.factory->Create(this);
	info.pointer = newPtr;
}

Resource9Handle D3D9Context::CreateRenderTarget(int width, int height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL lockable)
{
	auto factory = new RenderTarget9Factory(width, height, format, multiSample, multisampleQuality, lockable);
	auto ptr = factory->Create(this);
	return LogResource(factory, ptr);
}

Resource9Handle D3D9Context::CreateSprite()
{
	auto factory = new Sprite9Factory();
	auto ptr = factory->Create(this);
	return LogResource(factory, ptr);
}

Resource9Handle D3D9Context::CreateVertexBuffer9(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool)
{
	auto factory = new VertexBuffer9Factory(length, usage, fvf, pool);
	auto ptr = factory->Create(this);
	return LogResource(factory, ptr);
}

// Phase 2.3: 共享 ColorKey PS / ConstantTable 懒加载。
// 之所以放在 D3D9Context 单例而不是 HardwareSurface9Wrapper 内部，是因为：
// 1) D3D9 每帧 SetPixelShader + Set*Constant 开销可控，但 CreatePixelShader 是一次性编译，应只做一次。
// 2) D3D9 Reset 会清空所有内部对象，所以 ResetDevice 之后要把 m_colorKeyShaderInited 重置，
//    下次 GetSharedColorKeyShader() 触发 EnsureSharedColorKeyShader() 重新编译。
// 注意：编译产物 g_colorKeyHLSLC 来自 ddraw/ColorKey.hlsl，由 CMakeLists.txt 用 fxc.exe 预编译生成 ColorKeyHLSLC.h。
// Phase 8.25.15: 已在文件顶部 (line 19) 直接 #include "ColorKeyHLSLC.h", 这里不再 forward declare,
//   避免 LNK2019 (g_colorKeyHLSLC 在多个 TU 中真实定义, 避免 forward declare 但定义缺失的隐性 bug)。

void D3D9Context::EnsureSharedColorKeyShader()
{
	if (m_colorKeyShaderInited)
	{
		return;
	}
	if (!m_d3dDev9)
	{
		return;
	}
	HRESULT hr = m_d3dDev9->CreatePixelShader((DWORD*)g_colorKeyHLSLC, &m_colorKeyShader);
	if (SUCCEEDED(hr))
	{
		hr = D3DXGetShaderConstantTable((DWORD*)g_colorKeyHLSLC, &m_colorKeyConstantTable);
	}
	if (FAILED(hr))
	{
		logf("D3D9Context::EnsureSharedColorKeyShader failed: hr=0x%08x", hr);
		m_colorKeyShader = nullptr;
		m_colorKeyConstantTable = nullptr;
	}
	m_colorKeyShaderInited = true;
}

IDirect3DPixelShader9* D3D9Context::GetSharedColorKeyShader()
{
	EnsureSharedColorKeyShader();
	return m_colorKeyShader;
}

ID3DXConstantTable* D3D9Context::GetSharedColorKeyConstantTable()
{
	EnsureSharedColorKeyShader();
	return m_colorKeyConstantTable;
}

// Phase 9.3.9: GBuffer API 薄包装（实际实现在 GBufferRenderer 单例）
//   设计动机：D3D9Context.h 暴露 8 个 GBuffer 公开方法, 但实际逻辑
//     (RT 创建/shader 编译/caps check) 在 GBufferRenderer. 这里只 forward,
//     让 ddfix IDirect3DDevice::Execute 调 D3D9Context::* 而不是直连 renderer.
//   单元测试通过 D3D9Context 验证 GBuffer 资源管理 (IsGBufferAvailable 等)。
HRESULT D3D9Context::CreateGBuffer(int width, int height)
{
	if (!m_d3dDev9) return E_POINTER;
	return NDDFIX::Render::GBufferRenderer::Instance()->Initialize(m_d3dDev9, width, height);
}

HRESULT D3D9Context::BindGBufferAsRenderTarget()
{
	if (!m_d3dDev9) return E_POINTER;
	return NDDFIX::Render::GBufferRenderer::Instance()->BindGBufferAsRenderTarget(m_d3dDev9);
}

HRESULT D3D9Context::UnbindGBuffer()
{
	if (!m_d3dDev9) return E_POINTER;
	return NDDFIX::Render::GBufferRenderer::Instance()->UnbindGBuffer(m_d3dDev9);
}

IDirect3DTexture9* D3D9Context::GetGBufferPosTex() const
{
	return NDDFIX::Render::GBufferRenderer::Instance()->GetPosTex();
}

IDirect3DTexture9* D3D9Context::GetGBufferNormalTex() const
{
	return NDDFIX::Render::GBufferRenderer::Instance()->GetNormalTex();
}

IDirect3DTexture9* D3D9Context::GetGBufferDiffuseTex() const
{
	return NDDFIX::Render::GBufferRenderer::Instance()->GetDiffuseTex();
}

IDirect3DTexture9* D3D9Context::GetGBufferSpecularTex() const
{
	return NDDFIX::Render::GBufferRenderer::Instance()->GetSpecTex();
}

void D3D9Context::ReleaseGBuffer()
{
	NDDFIX::Render::GBufferRenderer::Instance()->Shutdown();
}

int D3D9Context::GetGBufferWidth() const
{
	return NDDFIX::Render::GBufferRenderer::Instance()->GetWidth();
}

int D3D9Context::GetGBufferHeight() const
{
	return NDDFIX::Render::GBufferRenderer::Instance()->GetHeight();
}

bool D3D9Context::IsGBufferAvailable() const
{
	return NDDFIX::Render::GBufferRenderer::Instance()->IsAvailable();
}

// Phase 9.4: Shadow API 薄包装 (实际实现在 ShadowRenderer 单例)
//   设计动机: D3D9Context.h 暴露 8 个 Shadow 公开方法, 但实际逻辑
//     (texture 创建 / shader 编译 / cascade 计算) 在 ShadowRenderer. 这里只 forward,
//     让 ddfix IDirect3DDevice::Execute 调 D3D9Context::* 而不是直连 renderer.
//   单元测试通过 D3D9Context 验证 Shadow 资源管理 (IsShadowAvailable 等)。
HRESULT D3D9Context::CreateShadowMaps(int mapSize, int cascadeCount)
{
	if (!m_d3dDev9) return E_POINTER;
	return NDDFIX::Render::ShadowRenderer::Instance()->Initialize(m_d3dDev9, mapSize, cascadeCount);
}

void D3D9Context::ReleaseShadowMaps()
{
	NDDFIX::Render::ShadowRenderer::Instance()->Shutdown();
}

IDirect3DTexture9* D3D9Context::GetShadowMapTexture(int cascadeIndex) const
{
	return NDDFIX::Render::ShadowRenderer::Instance()->GetShadowMapTexture(cascadeIndex);
}

D3DXMATRIX D3D9Context::GetShadowMatrix(int cascadeIndex) const
{
	return NDDFIX::Render::ShadowRenderer::Instance()->GetShadowMatrix(cascadeIndex);
}

int D3D9Context::GetCascadeCount() const
{
	return NDDFIX::Render::ShadowRenderer::Instance()->GetCascadeCount();
}

int D3D9Context::GetShadowMapSize() const
{
	return NDDFIX::Render::ShadowRenderer::Instance()->GetMapSize();
}

int D3D9Context::GetPCFKernelSize() const
{
	return NDDFIX::Render::ShadowRenderer::Instance()->GetPCFKernelSize();
}

bool D3D9Context::IsShadowAvailable() const
{
	return NDDFIX::Render::ShadowRenderer::Instance()->IsAvailable();
}

