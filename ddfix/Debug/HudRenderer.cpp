// Phase 4.2: HudRenderer 实现
//   - ID3DXFont 画文本（来自 ExtraDxSDK/d3dx9.h）
//   - IDirect3DDevice9::DrawPrimitiveUP 画半透明矩形（D3DFVF_XYZRHW | D3DFVF_DIFFUSE）
//   - 设备丢失 / D3DXFont 失效 → Render() 静默 no-op

#include "HudRenderer.h"
#include "PerfCounter.h"
#include "../D3D9Context.h"  // 引入 ND3D9 命名空间 + d3dx9.h
#include "../Config/ConfigManager.h"
#include "../Common/Logging.h"

#include <cstdio>
#include <cstdint>

namespace NDDFIX
{
namespace Debug
{


HudRenderer* HudRenderer::Instance()
{
	static HudRenderer inst;
	return &inst;
}

HudRenderer::HudRenderer()
	: m_device(nullptr)
	, m_font(nullptr)
	, m_visible(false)
	, m_initialized(false)
{
}

HudRenderer::~HudRenderer()
{
	Shutdown();
}

void HudRenderer::Initialize()
{
	if (m_initialized)
	{
		return;
	}
	m_device = ND3D9::D3D9Context::Instance()->GetDevice();
	if (!m_device)
	{
		logf("HudRenderer::Initialize: D3D9 device is null, deferring");
		return;
	}

	// D3DXCreateFontW：宽 0 = 默认；height=14 像素，中文 Win10 通常够用
	HRESULT hr = D3DXCreateFontW(
		m_device,
		14,                // Height (pixels)
		0,                 // Width (0 = default)
		FW_NORMAL,         // Weight
		1,                 // MipLevels
		FALSE,             // Italic
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE,
		L"Consolas",
		&m_font);
	if (FAILED(hr) || !m_font)
	{
		logf("HudRenderer::Initialize: D3DXCreateFontW failed hr=0x%08x", hr);
		m_font = nullptr;
		return;
	}

	// 初始可见性由 ConfigManager 控制：hudEnabled == true → 默认显示
	const auto& debugCfg = NDDFIX::Config::ConfigManager::Instance()->GetDebug();
	m_visible = debugCfg.hudEnabled;
	m_initialized = true;
	logf("HudRenderer::Initialize: ok, visible=%d", m_visible ? 1 : 0);
}

void HudRenderer::Shutdown()
{
	if (m_font)
	{
		m_font->Release();
		m_font = nullptr;
	}
	m_device = nullptr;
	m_initialized = false;
}

void HudRenderer::OnDeviceLost()
{
	// ID3DXFont 必须 OnLostDevice，否则 Reset 时 D3D9 内部引用悬空
	if (m_font)
	{
		m_font->OnLostDevice();
	}
}

void HudRenderer::OnDeviceReset()
{
	// Reset 成功之后调，OnResetDevice 让 D3DXFont 重新挂回设备
	if (m_font)
	{
		m_font->OnResetDevice();
	}
	else if (m_initialized)
	{
		// 极端情况：font 丢失，尝试重新创建
		Shutdown();
		Initialize();
	}
}

void HudRenderer::DrawRect(int x, int y, int w, int h, DWORD argb)
{
	if (!m_device || w <= 0 || h <= 0)
	{
		return;
	}

	struct HudVertex
	{
		float x, y, z, rhw;
		DWORD color;
	};
	const DWORD FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

	HudVertex verts[4];
	verts[0].x = static_cast<float>(x);     verts[0].y = static_cast<float>(y);     verts[0].z = 0.5f; verts[0].rhw = 1.0f; verts[0].color = argb;
	verts[1].x = static_cast<float>(x + w); verts[1].y = static_cast<float>(y);     verts[1].z = 0.5f; verts[1].rhw = 1.0f; verts[1].color = argb;
	verts[2].x = static_cast<float>(x);     verts[2].y = static_cast<float>(y + h); verts[2].z = 0.5f; verts[2].rhw = 1.0f; verts[2].color = argb;
	verts[3].x = static_cast<float>(x + w); verts[3].y = static_cast<float>(y + h); verts[3].z = 0.5f; verts[3].rhw = 1.0f; verts[3].color = argb;

	// 保存旧 render state，避免破坏游戏自身的 blend / texture 状态
	DWORD oldAlphaBlend = 0;
	DWORD oldSrcBlend = 0;
	DWORD oldDstBlend = 0;
	m_device->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlend);
	m_device->GetRenderState(D3DRS_SRCBLEND, &oldSrcBlend);
	m_device->GetRenderState(D3DRS_DESTBLEND, &oldDstBlend);
	// 设 ALPHABLENDENABLE + SRCALPHA/INVSRCALPHA，stage0 不设（=nullptr）
	m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	m_device->SetTexture(0, nullptr);
	m_device->SetFVF(FVF);

	HRESULT hr = m_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(HudVertex));
	if (FAILED(hr))
	{
		logf("HudRenderer::DrawRect failed hr=0x%08x", hr);
	}

	// 恢复 alpha（texture 状态由调用方自己重设，HUD 下一行 DrawText 会自己处理）
	m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlend);
	m_device->SetRenderState(D3DRS_SRCBLEND, oldSrcBlend);
	m_device->SetRenderState(D3DRS_DESTBLEND, oldDstBlend);
}

void HudRenderer::DrawTextLine(int x, int y, const wchar_t* text, DWORD argb)
{
	if (!m_font || !text) return;
	RECT rc;
	rc.left = x;
	rc.top = y;
	rc.right = x + 600;
	rc.bottom = y + 24;
	// DT_NOCLIP 防止超出范围不画（DEBUG 模式够用）；DT_SINGLELINE 强制单行
	m_font->DrawTextW(nullptr, text, -1, &rc, DT_NOCLIP | DT_SINGLELINE | DT_LEFT, argb);
}

void HudRenderer::DrawTextLine(int x, int y, const char* text, DWORD argb)
{
	if (!text) return;
	wchar_t buf[512];
	MultiByteToWideChar(CP_ACP, 0, text, -1, buf, 512);
	buf[511] = 0;
	DrawTextLine(x, y, buf, argb);
}

void HudRenderer::Render()
{
	if (!m_initialized || !m_visible || !m_device || !m_font)
	{
		return;
	}

	// 设备丢失检测：若 TestCooperativeLevel 非 OK，跳过本次绘制。
	HRESULT coop = m_device->TestCooperativeLevel();
	if (coop != D3D_OK)
	{
		// 设备丢失/尚未重置完成，不画 HUD
		return;
	}

	// 滑动窗口结算（HUD 1Hz 刷新）
	PerfCounter::Instance()->Tick();

	// 抓快照
	PerfSnapshot snap = PerfCounter::Instance()->GetSnapshot();

	// HUD 布局
	const int PAD = 8;
	const int LINE_H = 16;
	const int WIDTH = 360;
	const int LINES = 9; // 标题 + 7 数据行 + 1 hot 标题
	int totalH = (LINES + 1 + 8) * LINE_H; // 留 8 行给 hot scope

	// 半透明黑底
	DrawRect(PAD, PAD, WIDTH, totalH, 0xC0000000u);

	int x = PAD + 6;
	int y = PAD + 4;

	char buf[256];

	// 1) 标题
	DrawTextLine(x, y, "MeteorBladeEnhancer HUD (F12 toggle)", 0xFF00FF00u);
	y += LINE_H;

	// 2) FPS
	std::snprintf(buf, sizeof(buf), "FPS:        %.1f", snap.fps);
	DrawTextLine(x, y, buf);
	y += LINE_H;

	// 3) 设备状态
	const char* deviceState = "OK";
	if (coop == D3DERR_DEVICENOTRESET) deviceState = "NOTRESET";
	else if (coop == D3DERR_DEVICELOST) deviceState = "LOST";
	else if (coop == D3DERR_DRIVERINTERNALERROR) deviceState = "DRIVER_ERR";
	std::snprintf(buf, sizeof(buf), "D3D9:       %s (hr=0x%08x)", deviceState, static_cast<unsigned>(coop));
	DrawTextLine(x, y, buf);
	y += LINE_H;

	// 4) 主 surface 尺寸
	int bbW = 0, bbH = 0;
	ND3D9::D3D9Context::Instance()->GetBackBufferSize(&bbW, &bbH);
	std::snprintf(buf, sizeof(buf), "BackBuffer: %d x %d", bbW, bbH);
	DrawTextLine(x, y, buf);
	y += LINE_H;

	// 5) Blt / BltFast / FillColor / Flip 每秒
	std::snprintf(buf, sizeof(buf), "Blt/s:      %.1f", snap.bltPerSec);
	DrawTextLine(x, y, buf);
	y += LINE_H;
	std::snprintf(buf, sizeof(buf), "BltFast/s:  %.1f", snap.bltFastPerSec);
	DrawTextLine(x, y, buf);
	y += LINE_H;
	std::snprintf(buf, sizeof(buf), "FillColor/s:%.1f", snap.fillColorPerSec);
	DrawTextLine(x, y, buf);
	y += LINE_H;
	std::snprintf(buf, sizeof(buf), "Flip/s:     %.1f", snap.flipPerSec);
	DrawTextLine(x, y, buf);
	y += LINE_H;

	// 6) 显存使用
	UINT availBytes = 0;
	if (m_device)
	{
		availBytes = m_device->GetAvailableTextureMem();
	}
	double availMB = static_cast<double>(availBytes) / (1024.0 * 1024.0);
	std::snprintf(buf, sizeof(buf), "VRAM avail: %.1f MB", availMB);
	DrawTextLine(x, y, buf);
	y += LINE_H;

	// 7) Uptime
	std::snprintf(buf, sizeof(buf), "Uptime:     %.1f s", snap.uptimeSec);
	DrawTextLine(x, y, buf);
	y += LINE_H;

	// 8) PERF_SCOPE hot 列表
	y += 4;
	DrawTextLine(x, y, "PERF_SCOPE hot:", 0xFFFFFF00u);
	y += LINE_H;
	size_t n = snap.hotScopeNames.size();
	for (size_t i = 0; i < n; ++i)
	{
		std::snprintf(buf, sizeof(buf), "  %-20s %llu", snap.hotScopeNames[i].c_str(),
			static_cast<unsigned long long>(snap.hotScopeCounts[i]));
		DrawTextLine(x, y, buf);
		y += LINE_H;
	}
}

} // namespace Debug
} // namespace NDDFIX
