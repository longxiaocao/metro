// Phase 4.2: 自绘 HUD 渲染器
// 设计目标：
//   - 完全离线可用：不依赖 ImGui 等第三方头
//   - 用 ID3DXFont 画文本（D3DXCreateFont 来自 ExtraDxSDK）
//   - 用 IDirect3DDevice9::DrawPrimitiveUP 画半透明矩形（D3DFVF_XYZRHW | D3DFVF_DIFFUSE）
//   - 单例，HudRenderer::Instance()
//   - 在 IDirectDrawSurface4::Blt Primary 路径（Present 之后）调 Render()
//   - 设备丢失时 Render() 静默 no-op，等 OnReset 后重建 D3DXFont
//   - 暴露：SetVisible / ToggleVisible / IsVisible / Initialize / Shutdown / Render
//
// 显示内容：
//   - FPS（1秒滑动平均，PerfCounter 提供）
//   - D3D9 设备状态（TestCooperativeLevel 返回值文字化）
//   - 当前主 surface 尺寸（back buffer 宽高）
//   - Blt / BltFast / FillColor 每秒调用次数
//   - 显存使用（IDirect3DDevice9::GetAvailableTextureMem）
//   - PERF_SCOPE hot 列表（PerfCounter 提供）
//   - 进程启动时间（PerfCounter 提供）

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

struct IDirect3DDevice9;
struct ID3DXFont;

namespace NDDFIX
{
namespace Debug
{

class HudRenderer
{
public:
	static HudRenderer* Instance();

	// 初始化 D3DXFont + 默认可见性（受 ConfigManager 控制）。
	// 在 D3D9Context::Initialize 之后调一次。
	void Initialize();

	// 释放 D3DXFont。D3D9Context::Uninitialize 之前调一次。
	void Shutdown();

	// 设备丢失时调：释放依赖设备的资源（D3DXFont）。
	// 内部已经 OnLostDevice 之后，Render() 自动 no-op。
	void OnDeviceLost();

	// 设备重置后调：重建 D3DXFont。
	void OnDeviceReset();

	// HUD 显隐控制
	void SetVisible(bool v)        { m_visible = v; }
	void ToggleVisible()           { m_visible = !m_visible; }
	bool IsVisible() const         { return m_visible; }

	// 每帧渲染（在 Present 之后或 D3D9 EndScene 之前调一次）。
	// 若设备丢失 / D3DXFont 不可用 / 不可见 → no-op。
	void Render();

private:
	HudRenderer();
	~HudRenderer();
	HudRenderer(const HudRenderer&) = delete;
	HudRenderer& operator=(const HudRenderer&) = delete;

	// 内部绘制原语
	void DrawRect(int x, int y, int w, int h, DWORD argb);
	void DrawTextLine(int x, int y, const wchar_t* text, DWORD argb = 0xFFFFFFFFu);
	void DrawTextLine(int x, int y, const char* text, DWORD argb = 0xFFFFFFFFu);

	// 缓存的设备指针（Initialize 时从 D3D9Context::Instance()->GetDevice() 取）
	IDirect3DDevice9* m_device;
	ID3DXFont*        m_font;
	bool              m_visible;
	bool              m_initialized;
};

} // namespace Debug
} // namespace NDDFIX
