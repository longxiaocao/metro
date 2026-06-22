// Phase 4.1: 自包含 IMGUI D3D9 后端（不引入第三方 ImGui 库）
//
// 设计决策：
//   原本计划接入 imgui 1.89+ + imgui_impl_dx9 + imgui_impl_win32。
//   但本项目是离线分发（不连网），且 cmake_minimum_required(3.11) 早于 FetchContent 流行；
//   而且 d3d9 wrapper 本身就要拦截并改写所有 D3D9 调用，再叠加 ImGui 后端容易和游戏自身的
//   render state 互相破坏。
//
//   因此改为：创建本文件作为"假后端"，把 Render() / ToggleVisible() / IsVisible() / Initialize()
//   / Shutdown() 等 ImGui 风格的 API 转发到 HudRenderer（Phase 4.2）。
//   调用方可以 `#include "Debug/ImGuiBackend.h"` 拿到底层 API，未来若要接真 ImGui，
//   只换本文件实现 + 加 .lib 即可，调用方零改动。

#pragma once

#include "HudRenderer.h"

namespace NDDFIX
{
namespace Debug
{

// ImGui 风格后端：内部全部委托给 HudRenderer。
// 保留独立类型是为了在调用方代码里"语义清晰"。
class ImGuiBackend
{
public:
	// 全局单例
	static ImGuiBackend* Instance();

	// 初始化（d3d9 设备就绪后调一次）
	void Initialize() { HudRenderer::Instance()->Initialize(); }

	// 释放（d3d9 设备销毁前调一次）
	void Shutdown() { HudRenderer::Instance()->Shutdown(); }

	// 设备丢失 / 重置
	void OnDeviceLost()  { HudRenderer::Instance()->OnDeviceLost(); }
	void OnDeviceReset() { HudRenderer::Instance()->OnDeviceReset(); }

	// ImGui::NewFrame 风格：当前实现 no-op（HudRenderer 自己 Tick）
	void NewFrame() { /* HudRenderer::Render() 内部 Tick */ }

	// ImGui::Render 风格：把 HUD 画到当前 back buffer
	void Render() { HudRenderer::Instance()->Render(); }

	// 显隐控制
	void SetVisible(bool v)  { HudRenderer::Instance()->SetVisible(v); }
	void ToggleVisible()     { HudRenderer::Instance()->ToggleVisible(); }
	bool IsVisible() const   { return HudRenderer::Instance()->IsVisible(); }
};

} // namespace Debug
} // namespace NDDFIX
