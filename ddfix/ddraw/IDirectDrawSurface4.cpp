/**
* Copyright (C) 2017 Elisha Riedlinger
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*/

#include "ddraw.h"
#include "../D3D9Context.h"
#include "../Config/ConfigManager.h"
#include "../Debug/HudRenderer.h"
#include "../Debug/PerfCounter.h"
#include "ColorKeyHLSLC.h"
#include <functional>

// Phase 3.3: g_useSoftwareWrapper9 已迁移到 ConfigManager::GetRender().useSoftwareBlt
// 这里仅保留一行调用包装，宏在多个地方使用，避免逐处改写。
#define USE_SOFTWARE_WRAPPER_9 (NDDFIX::Config::ConfigManager::Instance()->GetRender().useSoftwareBlt)


template <bool HAVESRCCOLORKEY, bool HAVEDESTCOLORKEY, bool CHECKDIRTY = false>
static void LineProcess(D3DCOLOR* srcLine, D3DCOLOR* destLine, int width, DWORD srcColorKey, DWORD destColorKey)
{
	for (int x = 0; x < width; x++)
	{
		auto srcPixel = srcLine[x];
		auto& destPixel = destLine[x];
		if (HAVESRCCOLORKEY)
		{
			if ((srcPixel & 0x00ffffff) == srcColorKey)
			{
				continue;
			}
		}

		if (HAVEDESTCOLORKEY)
		{
			if ((destPixel & 0x00ffffff) != destColorKey)
			{
				continue;
			}
		}

		if (CHECKDIRTY)
		{
			if (true)
			{
				// trick for GetDC
				if (srcPixel != 0)
				{
					destPixel = srcPixel | 0xff000000;
				}
			}
			else
			{
				// 标准写法
				if (srcPixel & 0xff000000)
				{
					destPixel = srcPixel | 0xff000000;
				}
			}
		}
		else
		{
			destPixel = srcPixel | 0xff000000;
		}
	}
};

struct ISurface9Wrapper
{
	virtual ~ISurface9Wrapper() = default;
	virtual SmartPtr<ND3D9::IDirect3DSurface9> GetSurface9() const = 0;
	virtual HRESULT Blt(ISurface9Wrapper* srcSurface9Wrapper, LPRECT lpSrcRect, LPRECT lpDestRect, D3DCOLOR* srcColorKey, D3DCOLOR* destColorKey) = 0;
	virtual HRESULT BltFast(ISurface9Wrapper* srcSurface9Wrapper, LPRECT lpSrcRect, DWORD destX, DWORD destY, D3DCOLOR* srcColorKey, D3DCOLOR* destColorKey) = 0;
	virtual HRESULT FillColor(LPRECT rect, D3DCOLOR color) = 0;
	virtual HRESULT GetDC(HDC FAR * a) = 0;
	virtual HRESULT ReleaseDC(HDC a) = 0;
	virtual std::string GetImplClassName() const = 0;
};

class ZBuffer9Wrapper final : public ISurface9Wrapper
{
public:
	ZBuffer9Wrapper(ND3D9::D3D9Context* d3d9Context, int width, int height, ND3D9::D3DFORMAT format, ESurfaceType surfaceType)
		: m_d3d9Context(d3d9Context)
		, m_surfaceType(surfaceType)
	{
		assert(m_surfaceType == ESurfaceType::ZBuffer);
		m_surface9Handle = m_d3d9Context->CreateZBufferSurface9(width, height, ND3D9::D3DFMT_D16, ND3D9::D3DMULTISAMPLE_NONE, 0, FALSE);
	}

	virtual ~ZBuffer9Wrapper()
	{
		m_d3d9Context->ReleaseResource9(m_surface9Handle);
	}

	virtual std::string GetImplClassName() const override
	{
		return "ZBuffer9Wrapper";
	}

	virtual SmartPtr<ND3D9::IDirect3DSurface9> GetSurface9() const override
	{
		return m_d3d9Context->GetResource9<ND3D9::IDirect3DSurface9>(m_surface9Handle, nullptr);
	}

	virtual HRESULT Blt(ISurface9Wrapper* srcSurface9Wrapper, LPRECT lpSrcRect, LPRECT lpDestRect, D3DCOLOR* srcColorKey, D3DCOLOR* destColorKey) override
	{
		return DDERR_GENERIC;
	}
	virtual HRESULT BltFast(ISurface9Wrapper* srcSurface9Wrapper, LPRECT lpSrcRect, DWORD destX, DWORD destY, D3DCOLOR* srcColorKey, D3DCOLOR* destColorKey) override
	{
		return DDERR_GENERIC;
	}

	virtual HRESULT FillColor(LPRECT rect, D3DCOLOR color) override
	{
		return DDERR_GENERIC;
	}

	virtual HRESULT GetDC(HDC FAR * a)
	{
		return DDERR_GENERIC;
	}
	virtual HRESULT ReleaseDC(HDC a)
	{
		return DDERR_GENERIC;
	}

private:
	ND3D9::D3D9Context* m_d3d9Context;
	ND3D9::Resource9Handle m_surface9Handle;
	ESurfaceType m_surfaceType;
};

// Phase 2.1: Overlay surface 包装。
// 设计：D3D9 没有真正的 hardware overlay 概念，DX6/DX7 的 overlay 主要用于 YUV 视频叠加。
// 流星蝴蝶剑不用 overlay，但某些 DX6 引擎（赛车类、播放器）会先枚举到 overlay 然后放弃。
// 实现：创一张 RENDERTARGET texture 作为后端，Blt/FillColor/GetDC/ReleaseDC 都返 DDERR_GENERIC
// （overlay 不支持 D3D 路径的 Blt，调用方应回退到 OffScreen 路径）。
// 这样不会让 m_surface9Wrapper 为 nullptr 崩，GetSurface9() 也能返一个有效 surface（满足部分查询路径）。
class Overlay9Wrapper final : public ISurface9Wrapper
{
public:
	Overlay9Wrapper(ND3D9::D3D9Context* d3d9Context, int width, int height, ND3D9::D3DFORMAT format, ESurfaceType surfaceType)
		: m_d3d9Context(d3d9Context)
		, m_surfaceType(surfaceType)
	{
		assert(m_surfaceType == ESurfaceType::Overlay);
		// 后端用一张 RENDERTARGET texture 兜底。Overlay 在 D3D9 中没硬件对应，texture 用来满足
		// GetSurface9() 不为 null 的最小契约；UpdateOverlay/UpdateOverlayDisplay/SetOverlayPosition 等
		// 真实 overlay 路径仍走 ProxyInterface 透传。
		m_resource9Handle = m_d3d9Context->CreateTexture9(width, height, 1, ND3D9::D3DUSAGE_RENDERTARGET, format, ND3D9::D3DPOOL_DEFAULT);
	}

	virtual ~Overlay9Wrapper()
	{
		m_d3d9Context->ReleaseResource9(m_resource9Handle);
	}

	virtual std::string GetImplClassName() const override
	{
		return "Overlay9Wrapper";
	}

	virtual SmartPtr<ND3D9::IDirect3DSurface9> GetSurface9() const override
	{
		auto tex9 = m_d3d9Context->GetResource9<ND3D9::IDirect3DTexture9>(m_resource9Handle, nullptr);
		SmartPtr<ND3D9::IDirect3DSurface9> surface9;
		if (tex9)
		{
			tex9->GetSurfaceLevel(0, &surface9);
		}
		return surface9;
	}

	virtual HRESULT Blt(ISurface9Wrapper* srcSurface9Wrapper, LPRECT lpSrcRect, LPRECT lpDestRect, D3DCOLOR* srcColorKey, D3DCOLOR* destColorKey) override
	{
		// Overlay 不在 D3D Blt 路径中实现，避免误用导致图像撕裂。调用方应回退到 OffScreen。
		return DDERR_GENERIC;
	}

	virtual HRESULT BltFast(ISurface9Wrapper* srcSurface9Wrapper, LPRECT lpSrcRect, DWORD destX, DWORD destY, D3DCOLOR* srcColorKey, D3DCOLOR* destColorKey) override
	{
		return DDERR_GENERIC;
	}

	virtual HRESULT FillColor(LPRECT rect, D3DCOLOR color) override
	{
		return DDERR_GENERIC;
	}

	virtual HRESULT GetDC(HDC FAR * a)
	{
		return DDERR_GENERIC;
	}

	virtual HRESULT ReleaseDC(HDC a)
	{
		return DDERR_GENERIC;
	}

private:
	ND3D9::D3D9Context* m_d3d9Context;
	ND3D9::Resource9Handle m_resource9Handle;
	ESurfaceType m_surfaceType;
};


class SoftwareSurface9Wrapper final : public ISurface9Wrapper
{
public:
	SoftwareSurface9Wrapper(ND3D9::D3D9Context* d3d9Context,int width, int height, ND3D9::D3DFORMAT format, ESurfaceType surfaceType)
		: m_d3d9Context(d3d9Context)
		, m_surfaceType(surfaceType)
		, m_srcSampled(nullptr)
		, m_sampleTableHorizon(nullptr)
		, m_sampleTableVertical(nullptr)
	{
		if (surfaceType == ESurfaceType::BackBuffer)
		{
			m_surface9Handle = m_d3d9Context->CreateOffScreenSurface9(width, height, format, ND3D9::D3DPOOL_SYSTEMMEM);
		}
		else if (surfaceType == ESurfaceType::OffScreen)
		{
			m_surface9Handle = m_d3d9Context->CreateOffScreenSurface9(width, height, format, ND3D9::D3DPOOL_SYSTEMMEM);
		}
		else if(surfaceType == ESurfaceType::Primary)
		{
			m_surface9Handle = m_d3d9Context->GetBackBuffer9();
		}

		m_srcSampled = new D3DCOLOR[width * height];
		m_sampleTableHorizon = new int[width];
		m_sampleTableVertical = new int[height];
	}

	virtual ~SoftwareSurface9Wrapper()
	{
		m_d3d9Context->ReleaseResource9(m_surface9Handle);

		if (m_srcSampled)
		{
			delete m_srcSampled; m_srcSampled = nullptr;
		}
		if (m_sampleTableHorizon)
		{
			delete m_sampleTableHorizon; m_sampleTableHorizon = nullptr;
		}
		if (m_sampleTableVertical)
		{
			delete m_sampleTableVertical; m_sampleTableVertical = nullptr;
		}
	}

	virtual std::string GetImplClassName() const override
	{
		return "SoftwareSurface9Wrapper";
	}

	virtual SmartPtr<ND3D9::IDirect3DSurface9> GetSurface9() const override
	{
		return m_d3d9Context->GetResource9<ND3D9::IDirect3DSurface9>(m_surface9Handle, nullptr);
	}

	virtual HRESULT Blt(ISurface9Wrapper* srcSurface9Wrapper, LPRECT lpSrcRect, LPRECT lpDestRect, D3DCOLOR* srcColorKey, D3DCOLOR* destColorKey) override
	{
		HRESULT hr = DDERR_GENERIC;

		assert(srcSurface9Wrapper->GetImplClassName() == this->GetImplClassName());
		auto srcSurface9 = srcSurface9Wrapper->GetSurface9();

		std::function<void(D3DCOLOR* srcLine, D3DCOLOR* destLine, int x)> lineCallBack;

		using namespace std::placeholders;
		if (m_surfaceType == ESurfaceType::Primary)
		{
			const bool checkDirty = true; // trick for GetDC
			if (srcColorKey && !destColorKey)
			{
				lineCallBack = std::bind(&LineProcess<true, false, checkDirty>, _1, _2, _3, *srcColorKey, 0);
			}
			else if (!srcColorKey && destColorKey)
			{
				lineCallBack = std::bind(&LineProcess<false, true, checkDirty>, _1, _2, _3, 0, *destColorKey);
			}
			else if (srcColorKey && destColorKey)
			{
				lineCallBack = std::bind(&LineProcess<true, true, checkDirty>, _1, _2, _3, *srcColorKey, *destColorKey);
			}
			else
			{
				lineCallBack = std::bind(&LineProcess<false, false, checkDirty>, _1, _2, _3, 0, 0);
			}
		}
		else
		{
			if (srcColorKey && !destColorKey)
			{
				lineCallBack = std::bind(&LineProcess<true, false>, _1, _2, _3, *srcColorKey, 0);
			}
			else if (!srcColorKey && destColorKey)
			{
				lineCallBack = std::bind(&LineProcess<false, true>, _1, _2, _3, 0, *destColorKey);
			}
			else if (srcColorKey && destColorKey)
			{
				lineCallBack = std::bind(&LineProcess<true, true>, _1, _2, _3, *srcColorKey, *destColorKey);
			}
			else
			{
				lineCallBack = std::bind(&LineProcess<false, false>, _1, _2, _3, 0, 0);
			}
		}


		auto destSurface9 = GetSurface9();

		int srcWidth = lpSrcRect->right - lpSrcRect->left;
		int srcHeight = lpSrcRect->bottom - lpSrcRect->top;
		int destWidth = lpDestRect->right - lpDestRect->left;
		int destHeight = lpDestRect->bottom - lpDestRect->top;
		float horizonScale = (float)destWidth / srcWidth;
		float verticalScale = (float)destHeight / srcHeight;

		for (int x = 0; x < destWidth; x++)
		{
			m_sampleTableHorizon[x] = (int)(x / horizonScale);
		}

		for (int y = 0; y < destHeight; y++)
		{
			m_sampleTableVertical[y] = (int)(y / verticalScale);
		}

		{
			ND3D9::D3DLOCKED_RECT srcLockedRect;
			if (SUCCEEDED(hr = srcSurface9->LockRect(&srcLockedRect, lpSrcRect, D3DLOCK_READONLY)))
			{
				int destPitch = destWidth * sizeof(D3DCOLOR);
				char* srcBits = (char*)srcLockedRect.pBits;
				char* destBits = (char*)m_srcSampled;
				for (int y = 0; y < destHeight; y++)
				{
					D3DCOLOR* srcLineStart = (D3DCOLOR*)srcBits;
					D3DCOLOR* destLineStart = (D3DCOLOR*)destBits;
					for (int x = 0; x < destWidth; x++)
					{
						D3DCOLOR srcPixel = srcLineStart[m_sampleTableHorizon[x]];
						D3DCOLOR& destPixel = destLineStart[x];

						destPixel = srcPixel;
					}
					srcBits = (char*)srcLockedRect.pBits + srcLockedRect.Pitch * m_sampleTableVertical[y];
					destBits = (char*)destBits + destPitch;
				}
				srcSurface9->UnlockRect();
			}
		}

		ND3D9::D3DLOCKED_RECT destLockedRect;
		if (SUCCEEDED(hr = destSurface9->LockRect(&destLockedRect, lpDestRect, 0)))
		{
			int srcPitch = destWidth * sizeof(D3DCOLOR);
			char* srcBits = (char*)m_srcSampled;
			char* destBits = (char*)destLockedRect.pBits;
			for (int y = 0; y < destHeight; y++)
			{
				D3DCOLOR* srcLineStart = (D3DCOLOR*)srcBits;
				D3DCOLOR* destLineStart = (D3DCOLOR*)destBits;
				lineCallBack(srcLineStart, destLineStart, destWidth);
				srcBits = (char*)srcBits + srcPitch;
				destBits = (char*)destBits + destLockedRect.Pitch;
			}
			destSurface9->UnlockRect();
		}

		return hr;
	}
	virtual HRESULT BltFast(ISurface9Wrapper* srcSurface9Wrapper, LPRECT lpSrcRect, DWORD destX, DWORD destY, D3DCOLOR* srcColorKey, D3DCOLOR* destColorKey) override
	{
		HRESULT hr = DDERR_GENERIC;

		assert(srcSurface9Wrapper->GetImplClassName() == this->GetImplClassName());
		auto srcSurface9 = srcSurface9Wrapper->GetSurface9();

		std::function<void(D3DCOLOR* srcLine, D3DCOLOR* destLine, int x)> lineCallBack;

		using namespace std::placeholders;

		if (srcColorKey && !destColorKey)
		{
			lineCallBack = std::bind(&LineProcess<true, false>, _1, _2, _3, *srcColorKey, 0);
		}
		else if (!srcColorKey && destColorKey)
		{
			lineCallBack = std::bind(&LineProcess<false, true>, _1, _2, _3, 0, *destColorKey);
		}
		else if (srcColorKey && destColorKey)
		{
			lineCallBack = std::bind(&LineProcess<true, true>, _1, _2, _3, *srcColorKey, *destColorKey);
		}
		else
		{
			lineCallBack = std::bind(&LineProcess<false, false>, _1, _2, _3, 0, 0);
		}

		auto destSurface9 = GetSurface9();

		int width = lpSrcRect->right - lpSrcRect->left;
		int height = lpSrcRect->bottom - lpSrcRect->top;

		RECT destRect = { 0 };
		destRect.left = destX;
		destRect.right = destX + width;
		destRect.top = destY;
		destRect.bottom = destY + height;

		ND3D9::D3DLOCKED_RECT srcLockedRect;
		if (SUCCEEDED(hr = srcSurface9->LockRect(&srcLockedRect, lpSrcRect, D3DLOCK_READONLY)))
		{
			ND3D9::D3DLOCKED_RECT destLockedRect;
			if (SUCCEEDED(hr = destSurface9->LockRect(&destLockedRect, &destRect, 0)))
			{
				char* srcBits = (char*)srcLockedRect.pBits;
				char* destBits = (char*)destLockedRect.pBits;
				for (int y = 0; y < height; y++)
				{
					D3DCOLOR* srcLineStart = (D3DCOLOR*)srcBits;
					D3DCOLOR* destLineStart = (D3DCOLOR*)destBits;
					lineCallBack(srcLineStart, destLineStart, width);
					srcBits += srcLockedRect.Pitch;
					destBits += destLockedRect.Pitch;
				}


				destSurface9->UnlockRect();
			}

			srcSurface9->UnlockRect();
		}

		return hr;
	}

	virtual HRESULT FillColor(LPRECT rect, D3DCOLOR color) override
	{
		HRESULT hr = DDERR_GENERIC;
		auto surface9 = GetSurface9();
		ND3D9::D3DLOCKED_RECT destLockedRect;

		int destWidth = rect->right - rect->left;
		int destHeight = rect->bottom - rect->top;
		if (SUCCEEDED(hr = surface9->LockRect(&destLockedRect, rect, 0)))
		{
			char* destBits = (char*)destLockedRect.pBits;
			for (int y = 0; y < destHeight; y++)
			{
				D3DCOLOR* destLineStart = (D3DCOLOR*)destBits;
				for (int x = 0; x < destWidth; x++)
				{
					D3DCOLOR& destPixel = destLineStart[x];

					destPixel = color;
				}
				destBits = (char*)destBits + destLockedRect.Pitch;
			}

			surface9->UnlockRect();
		}

		return hr;
	}

	virtual HRESULT GetDC(HDC FAR * a)
	{
		return GetSurface9()->GetDC(a);
	}

	virtual HRESULT ReleaseDC(HDC a)
	{
		return GetSurface9()->ReleaseDC(a);;
	}

private:
	ND3D9::D3D9Context* m_d3d9Context;
	ND3D9::Resource9Handle m_surface9Handle;
	ESurfaceType m_surfaceType;

	D3DCOLOR* m_srcSampled;
	int* m_sampleTableHorizon;
	int* m_sampleTableVertical;

};

class HardwareSurface9Wrapper final : public ISurface9Wrapper
{
public:
	HardwareSurface9Wrapper(ND3D9::D3D9Context* d3d9Context, int width, int height, ND3D9::D3DFORMAT format, ESurfaceType surfaceType, DDSURFACEDESC2* desc)
		: m_d3d9Context(d3d9Context)
		, m_surfaceType(surfaceType)
		, m_desc(*desc)
		, m_resource9Handle(0)
		, m_isRenderTarget(false)
		, m_isTex(false)
		, m_spriteHandle(0)
	{
		if (surfaceType == ESurfaceType::BackBuffer)
		{
			m_resource9Handle = m_d3d9Context->CreateTexture9(width, height, 1, D3DUSAGE_RENDERTARGET, format, ND3D9::D3DPOOL_DEFAULT);
			m_isTex = true;
			m_isRenderTarget = true;
		}
		else if (surfaceType == ESurfaceType::OffScreen)
		{
			if (m_desc.ddsCaps.dwCaps & DDSCAPS_3DDEVICE)
			{
				m_resource9Handle = m_d3d9Context->CreateTexture9(width, height, 1, D3DUSAGE_RENDERTARGET, format, ND3D9::D3DPOOL_DEFAULT);
				m_isTex = true;
				m_isRenderTarget = true;
			}
			else
			{
				m_resource9Handle = m_d3d9Context->CreateTexture9(width, height, 1, 0, format, ND3D9::D3DPOOL_MANAGED);
				m_isTex = true;
			}
		}
		else if (surfaceType == ESurfaceType::Primary)
		{
			m_resource9Handle = m_d3d9Context->GetBackBuffer9();
			m_isRenderTarget = true;
		}

		if (m_isRenderTarget)
		{
			m_spriteHandle = m_d3d9Context->CreateSprite();
			// Phase 2.3: PS / ConstantTable 不再每个 wrapper 各创一份，改用 D3D9Context 共享。
			// 这样所有 RenderTarget HardwareSurface9Wrapper 共用同一份 PS 句柄，避免 CreatePixelShader 重复编译。
			// DrawSprite 中通过 m_d3d9Context->GetSharedColorKeyShader() 实时拿当前有效句柄（Reset 后会自动重建）。
		}
	}

	virtual ~HardwareSurface9Wrapper()
	{
		m_d3d9Context->ReleaseResource9(m_resource9Handle);
		if (m_spriteHandle)
		{
			m_d3d9Context->ReleaseResource9(m_spriteHandle);
		}
	}

	virtual SmartPtr<ND3D9::IDirect3DSurface9> GetSurface9() const override
	{
		if (!m_resource9Handle)
			return nullptr;
		if (!m_isTex)
		{
			SmartPtr<ND3D9::IDirect3DSurface9> surface9 = m_d3d9Context->GetResource9<ND3D9::IDirect3DSurface9>(m_resource9Handle, nullptr);
			return surface9;
		}
		else
		{
			auto tex9 = m_d3d9Context->GetResource9<ND3D9::IDirect3DTexture9>(m_resource9Handle, nullptr);
			SmartPtr<ND3D9::IDirect3DSurface9> surface9;
			tex9->GetSurfaceLevel(0, &surface9);
			return surface9;
		}
	}

	virtual HRESULT Blt(ISurface9Wrapper* srcSurface9Wrapper, LPRECT lpSrcRect, LPRECT lpDestRect, D3DCOLOR* srcColorKey, D3DCOLOR* destColorKey) override
	{
		assert(srcSurface9Wrapper->GetImplClassName() == this->GetImplClassName());

		DrawSprite(srcSurface9Wrapper, lpSrcRect, srcColorKey, destColorKey, lpDestRect);

		return DD_OK;
	}

	void DrawSprite(ISurface9Wrapper* srcSurface9Wrapper, LPRECT lpSrcRect, D3DCOLOR* srcColorKey, D3DCOLOR* destColorKey, LPRECT lpDestRect)
	{
		float widthScale = (float)(lpDestRect->right - lpDestRect->left) / (lpSrcRect->right - lpSrcRect->left);
		float heightScale = (float)(lpDestRect->bottom - lpDestRect->top) / (lpSrcRect->bottom - lpSrcRect->top);

		auto srcTex9 = static_cast<HardwareSurface9Wrapper*>(srcSurface9Wrapper)->GetTexture9();
		auto device9 = m_d3d9Context->GetDevice();

		// Phase 2.3: 从 D3D9Context 单例取共享 PS / ConstantTable。Reset 后会自动重建。
		// 之所以用裸指针（不持 SmartPtr），是因为这些资源由 D3D9Context 单例管理生命周期，
		// Reset 时旧指针置 null 但不 Release（D3D9 内部已清），新指针走 EnsureSharedColorKeyShader 重建。
		// Phase 8.16: 移除 ND3D9:: 限定（类型为全局 typedef，与 D3D9Context.h 保持一致）
		IDirect3DPixelShader9* sharedPS = m_d3d9Context->GetSharedColorKeyShader();
		ID3DXConstantTable* sharedCT = m_d3d9Context->GetSharedColorKeyConstantTable();
		if (!sharedPS || !sharedCT)
		{
			// 着色器初始化失败，跳过 PS 路径并继续（避免崩溃，但渲染可能异常）
			logf("HardwareSurface9Wrapper::DrawSprite: shared ColorKey shader not available, skip PS");
			return;
		}

		SmartPtr<IDirect3DPixelShader9> oldPixelShader;
		device9->GetPixelShader(&oldPixelShader);

		SmartPtr<IDirect3DSurface9> oldRenderTarget;
		device9->GetRenderTarget(0, &oldRenderTarget);
		device9->SetRenderTarget(0, GetSurface9());

		DWORD oldZEnableState;
		device9->GetRenderState(ND3D9::D3DRS_ZENABLE, &oldZEnableState);
		device9->SetRenderState(ND3D9::D3DRS_ZENABLE, FALSE);

		auto sprite = m_d3d9Context->GetResource9<ND3D9::ID3DXSprite>(m_spriteHandle, nullptr);

		device9->BeginScene();
		{
			sprite->Begin(0);
			{
				device9->SetPixelShader(sharedPS);
				ND3D9::D3DXCOLOR srcColorKeyF(srcColorKey ? *srcColorKey : 0);
				ND3D9::D3DXCOLOR destColorKeyF(destColorKey ? *destColorKey : 0);
				BOOL haveColorKey[2] = { (bool)srcColorKey, (bool)destColorKey };
				BOOL checkAlpha = m_surfaceType == ESurfaceType::Primary;

				sharedCT->SetVector((ND3D9::IDirect3DDevice9*)device9, sharedCT->GetConstantByName(NULL, "srcColorKey"), &ND3D9::D3DXVECTOR4(srcColorKeyF));
				// Phase 2.4: 取消注释，让 DDBLT_KEYDEST 真正把 destColorKey 传入 HLSL 常量。
				// ColorKey.hlsl 中 destColorKey 暂未在 ps_main 里直接引用（pre-existing 状态），
				// 但 HLSL 端已声明 float4 destColorKey 常量，保留 SetFloatArray 以备后续 dest-color-key 逻辑扩展。
				sharedCT->SetFloatArray((ND3D9::IDirect3DDevice9*)device9, sharedCT->GetConstantByName(NULL, "destColorKey"), (float*)&destColorKeyF, 1);
				sharedCT->SetBoolArray((ND3D9::IDirect3DDevice9*)device9, sharedCT->GetConstantByName(NULL, "haveColorKey"), haveColorKey, 2);
				sharedCT->SetBool((ND3D9::IDirect3DDevice9*)device9, sharedCT->GetConstantByName(NULL, "checkAlpha"), checkAlpha);

				device9->SetSamplerState(0, ND3D9::D3DSAMP_MAGFILTER, ND3D9::D3DTEXF_POINT);
				device9->SetSamplerState(0, ND3D9::D3DSAMP_MINFILTER, ND3D9::D3DTEXF_POINT);
				device9->SetSamplerState(0, ND3D9::D3DSAMP_MIPFILTER, ND3D9::D3DTEXF_POINT);

				ND3D9::D3DXMATRIX matx;
				ND3D9::D3DXMatrixTransformation2D(&matx, NULL, 0.0f, &ND3D9::D3DXVECTOR2(widthScale, heightScale), NULL, 0.0f, &ND3D9::D3DXVECTOR2(lpDestRect->left, lpDestRect->top));
				sprite->SetTransform(&matx);
				sprite->Draw((ND3D9::IDirect3DTexture9*)*&srcTex9, lpSrcRect, NULL, NULL, 0xffffffff);

				sprite->Flush();
				sprite->End();
			}

			device9->EndScene();
		}

		device9->SetPixelShader(oldPixelShader);

		device9->SetRenderTarget(0, oldRenderTarget);

		device9->SetRenderState(ND3D9::D3DRS_ZENABLE, oldZEnableState);
	}

	virtual HRESULT BltFast(ISurface9Wrapper* srcSurface9Wrapper, LPRECT lpSrcRect, DWORD destX, DWORD destY, D3DCOLOR* srcColorKey, D3DCOLOR* destColorKey) override
	{
		assert(srcSurface9Wrapper->GetImplClassName() == this->GetImplClassName());

		RECT destRect = { 0 };
		destRect.left = destX;
		destRect.top = destY;
		destRect.right = lpSrcRect->right - lpSrcRect->left + destX;
		destRect.bottom = lpSrcRect->bottom - lpSrcRect->top + destY;

		DrawSprite(srcSurface9Wrapper, lpSrcRect, srcColorKey, destColorKey, &destRect);

		return DD_OK;
	}

	virtual HRESULT FillColor(LPRECT rect, D3DCOLOR color) override
	{
		if (m_isRenderTarget)
		{
			return FillColorForRenderTarget(rect, color);
		}
		else
		{
			return FillColorLockableSurface(rect, color);
		}
	}

	virtual HRESULT GetDC(HDC FAR * a)
	{
		if (m_surfaceType == ESurfaceType::BackBuffer ||
			(m_surfaceType == ESurfaceType::OffScreen && m_desc.ddsCaps.dwCaps & DDSCAPS_3DDEVICE))
		{
			return DDERR_GENERIC;
			// too slow !!!
			//return  m_d3d9Context->GetResource9<ND3D9::IDirect3DSurface9>(m_d3d9Context->GetBackBuffer9(), nullptr)->GetDC(a);
		}
		else
		{
			return GetSurface9()->GetDC(a);
		}
	}

	virtual HRESULT ReleaseDC(HDC a)
	{
		if (m_surfaceType == ESurfaceType::BackBuffer ||
			(m_surfaceType == ESurfaceType::OffScreen && m_desc.ddsCaps.dwCaps & DDSCAPS_3DDEVICE))
		{
			return DDERR_GENERIC;
			// too slow !!!
			//return  m_d3d9Context->GetResource9<ND3D9::IDirect3DSurface9>(m_d3d9Context->GetBackBuffer9(), nullptr)->ReleaseDC(a);
		}
		else
		{
			return GetSurface9()->ReleaseDC(a);
		}
	}

	virtual std::string GetImplClassName() const override
	{
		return "HardwareSurface9Wrapper";
	}

private:
	SmartPtr<ND3D9::IDirect3DTexture9> GetTexture9() const
	{
		return m_d3d9Context->GetResource9<ND3D9::IDirect3DTexture9>(m_resource9Handle, nullptr);
	}

	HRESULT FillColorLockableSurface(LPRECT rect, D3DCOLOR color)
	{
		HRESULT hr = DDERR_GENERIC;
		auto surface9 = GetSurface9();
		ND3D9::D3DLOCKED_RECT destLockedRect;

		int destWidth = rect->right - rect->left;
		int destHeight = rect->bottom - rect->top;
		if (SUCCEEDED(hr = surface9->LockRect(&destLockedRect, rect, 0)))
		{
			char* destBits = (char*)destLockedRect.pBits;
			for (int y = 0; y < destHeight; y++)
			{
				D3DCOLOR* destLineStart = (D3DCOLOR*)destBits;
				for (int x = 0; x < destWidth; x++)
				{
					D3DCOLOR& destPixel = destLineStart[x];

					destPixel = color;
				}
				destBits = (char*)destBits + destLockedRect.Pitch;
			}

			surface9->UnlockRect();
		}

		return hr;
	}

	HRESULT FillColorForRenderTarget(LPRECT rect, D3DCOLOR color)
	{
		auto surface9 = GetSurface9();
		SmartPtr<ND3D9::IDirect3DSurface9> oldRenderTarget;
		m_d3d9Context->GetDevice()->GetRenderTarget(0, &oldRenderTarget);

		m_d3d9Context->GetDevice()->SetRenderTarget(0, surface9);
		D3DRECT d3dRect = { 0 };
		d3dRect.x1 = rect->left;
		d3dRect.x2 = rect->right;
		d3dRect.y1 = rect->top;
		d3dRect.y2 = rect->bottom;
		m_d3d9Context->GetDevice()->Clear(1, &d3dRect, D3DCLEAR_TARGET, color, 0.0f, 0.0);


		m_d3d9Context->GetDevice()->SetRenderTarget(0, oldRenderTarget);

		return DD_OK;
	}

private:
	ND3D9::D3D9Context* m_d3d9Context;
	ESurfaceType m_surfaceType;
	DDSURFACEDESC2 m_desc;
	ND3D9::Resource9Handle m_resource9Handle;
	bool m_isRenderTarget;
	bool m_isTex;

	ND3D9::Resource9Handle m_spriteHandle;
	// Phase 2.3: m_colorKeyShader / m_constantTable 已迁移到 D3D9Context 单例共享，不再是 wrapper 成员。
};

m_IDirectDrawSurface4::m_IDirectDrawSurface4(IDirectDrawSurface4 *aOriginal, DDSURFACEDESC2 desc, m_IDirectDrawSurface4* linkedPrevSurface, std::shared_ptr<WrapperLookupTable<void>> wrapperAddressLookupTable)
	: ProxyInterface(aOriginal)
	, m_desc(desc)
	, Refs(1)
	, WrapperAddressLookupTable(wrapperAddressLookupTable)
	, m_surfaceType(ESurfaceType::Unknown)
	, m_surface9Wrapper(nullptr)
	, m_clipper(nullptr)
	, m_linkedPrevSurface(linkedPrevSurface)
	, m_linkedNextSurface(nullptr)
	, m_tex2(nullptr)
	, m_locked(false)
{
	ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);

	DWORD dwCaps = m_desc.ddsCaps.dwCaps;
	if (dwCaps & DDSCAPS_OFFSCREENPLAIN)
	{
		m_surfaceType = ESurfaceType::OffScreen;
	}
	else if (dwCaps & DDSCAPS_PRIMARYSURFACE)
	{
		assert(m_surfaceType == ESurfaceType::Unknown);
		m_surfaceType = ESurfaceType::Primary;
	}
	else if (dwCaps & DDSCAPS_TEXTURE)
	{
		assert(m_surfaceType == ESurfaceType::Unknown);
		m_surfaceType = ESurfaceType::Texture;
	}
	else if (dwCaps & DDSCAPS_OVERLAY)
	{
		assert(m_surfaceType == ESurfaceType::Unknown);
		m_surfaceType = ESurfaceType::Overlay;
	}
	else if (dwCaps & DDSCAPS_BACKBUFFER)
	{
		assert(m_surfaceType == ESurfaceType::Unknown);
		m_surfaceType = ESurfaceType::BackBuffer;
	}
	else if (dwCaps & DDSCAPS_ZBUFFER)
	{
		assert(m_surfaceType == ESurfaceType::Unknown);
		m_surfaceType = ESurfaceType::ZBuffer;
	}
	else
	{
		assert(false);
	}


	HRESULT hr = DDERR_GENERIC;
	switch (m_surfaceType)
	{
	case ESurfaceType::OffScreen:
	{
		if (m_desc.ddsCaps.dwCaps & DDSCAPS_3DDEVICE)
		{
			int haha = 0;
			haha = 1;
		}

		bool createInSysMem = m_desc.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY;
		createInSysMem = true;
		if (USE_SOFTWARE_WRAPPER_9)
		{
			m_surface9Wrapper = new SoftwareSurface9Wrapper(ND3D9::D3D9Context::Instance(), m_desc.dwWidth, m_desc.dwHeight, ND3D9::D3DFMT_A8R8G8B8, m_surfaceType);
		}
		else
		{
			m_surface9Wrapper = new HardwareSurface9Wrapper(ND3D9::D3D9Context::Instance(), m_desc.dwWidth, m_desc.dwHeight, ND3D9::D3DFMT_A8R8G8B8, m_surfaceType, &m_desc);
		}
		break;
	}
	case ESurfaceType::Primary:
	{
		int width = 0;
		int height = 0;
		ND3D9::D3D9Context::Instance()->GetBackBufferSize(&width, &height);
		m_desc.dwWidth = width;
		m_desc.dwHeight = height;

		// code is taken from project DXGL
		if (m_desc.ddsCaps.dwCaps & DDSCAPS_COMPLEX)
		{
			if (m_desc.ddsCaps.dwCaps & DDSCAPS_FLIP)
			{
				if ((m_desc.dwFlags & DDSD_BACKBUFFERCOUNT) && (m_desc.dwBackBufferCount > 0))
				{
					if (!(m_desc.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER))
						m_desc.ddsCaps.dwCaps |= DDSCAPS_FRONTBUFFER;
					if (USE_SOFTWARE_WRAPPER_9)
					{
						m_surface9Wrapper = new SoftwareSurface9Wrapper(ND3D9::D3D9Context::Instance(), m_desc.dwWidth, m_desc.dwHeight, ND3D9::D3DFMT_A8R8G8B8, m_surfaceType);
					}
					else
					{
						m_surface9Wrapper = new HardwareSurface9Wrapper(ND3D9::D3D9Context::Instance(), m_desc.dwWidth, m_desc.dwHeight, ND3D9::D3DFMT_A8R8G8B8, m_surfaceType, &m_desc);
					}

					{
						DDSURFACEDESC2 ddsdBack;
						memcpy(&ddsdBack, &m_desc, m_desc.dwSize);
						ddsdBack.dwBackBufferCount--;
						ddsdBack.ddsCaps.dwCaps |= DDSCAPS_BACKBUFFER;
						ddsdBack.ddsCaps.dwCaps &= ~DDSCAPS_FRONTBUFFER;
						auto backbuffer = new m_IDirectDrawSurface4(nullptr, ddsdBack, this, WrapperAddressLookupTable);
						m_linkedNextSurface = backbuffer;
					}
				}
				else if (m_desc.dwFlags & DDSD_BACKBUFFERCOUNT)
				{
					m_surfaceType = ESurfaceType::BackBuffer;
					if (USE_SOFTWARE_WRAPPER_9)
					{
						m_surface9Wrapper = new SoftwareSurface9Wrapper(ND3D9::D3D9Context::Instance(), m_desc.dwWidth, m_desc.dwHeight, ND3D9::D3DFMT_A8R8G8B8, m_surfaceType);
					}
					else
					{
						m_surface9Wrapper = new HardwareSurface9Wrapper(ND3D9::D3D9Context::Instance(), m_desc.dwWidth, m_desc.dwHeight, ND3D9::D3DFMT_A8R8G8B8, m_surfaceType, &m_desc);
					}
				}
				else
				{
					assert(false);
				}
			}
		}
		else
		{
			if (USE_SOFTWARE_WRAPPER_9)
			{
				m_surface9Wrapper = new SoftwareSurface9Wrapper(ND3D9::D3D9Context::Instance(), m_desc.dwWidth, m_desc.dwHeight, ND3D9::D3DFMT_A8R8G8B8, m_surfaceType);
			}
			else
			{
				m_surface9Wrapper = new HardwareSurface9Wrapper(ND3D9::D3D9Context::Instance(), m_desc.dwWidth, m_desc.dwHeight, ND3D9::D3DFMT_A8R8G8B8, m_surfaceType, &m_desc);
			}
		}
		break;
	}
	case ESurfaceType::Texture:
	{
		auto ddraw4 = WrapperAddressLookupTable->FindWrapper<m_IDirectDraw4>(IID_IDirectDraw4);

		if ((!(dwCaps & (DDSCAPS_MIPMAP))) ||
			((dwCaps & (DDSCAPS_MIPMAP)) && !(m_desc.ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL))
			)
		{
			m_tex2 = new m_IDirect3DTexture2(nullptr, this);
			ddraw4->Tex9LookupTable->SaveAddress(this, m_tex2->GetTexture9());
		}

		if (
			((dwCaps & (DDSCAPS_MIPMAP)) && !(m_desc.ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL))
			)
		{
			auto linkedPrevSurface = this;
			for (DWORD mipmapIndex = 1; mipmapIndex < m_desc.dwMipMapCount; mipmapIndex++)
			{
				DDSURFACEDESC2 newDesc = desc;
				newDesc.ddsCaps.dwCaps2 |= DDSCAPS2_MIPMAPSUBLEVEL;

				ND3D9::D3DSURFACE_DESC desc9;
				m_tex2->GetTexture9()->GetLevelDesc(mipmapIndex, &desc9);
				newDesc.dwWidth = desc9.Width;
				newDesc.dwHeight = desc9.Height;

				linkedPrevSurface->m_linkedNextSurface = new m_IDirectDrawSurface4(nullptr, newDesc, linkedPrevSurface, WrapperAddressLookupTable);
				linkedPrevSurface = linkedPrevSurface->m_linkedNextSurface;
			}
		}

		break;
	}
	case ESurfaceType::Overlay:
	{
		// Phase 2.1: Overlay 不再走 nullptr 路径。
		// 用 Overlay9Wrapper 兜底：Blt/FillColor/GetDC/ReleaseDC 返 DDERR_GENERIC，但 GetSurface9() 返有效 surface。
		// 真实 UpdateOverlay/SetOverlayPosition 仍由 m_IDirectDrawSurface4::UpdateOverlay 透传到 ProxyInterface。
		if (m_desc.dwWidth == 0 || m_desc.dwHeight == 0)
		{
			// 防呆：调用方没填宽高时用 BackBuffer 尺寸（与 BackBuffer/ZBuffer 路径一致）
			int width = 0;
			int height = 0;
			ND3D9::D3D9Context::Instance()->GetBackBufferSize(&width, &height);
			m_desc.dwWidth = width;
			m_desc.dwHeight = height;
		}
		m_surface9Wrapper = new Overlay9Wrapper(
			ND3D9::D3D9Context::Instance(),
			m_desc.dwWidth,
			m_desc.dwHeight,
			ND3D9::D3DFMT_A8R8G8B8,
			m_surfaceType);
		break;
	}
	case ESurfaceType::BackBuffer:
	{
		// P0 修复: 原来直接 break，导致 m_surface9Wrapper = nullptr，游戏单独建 BackBuffer 时 GetSurface9()/Blt 全崩。
		// 改为自建 HardwareSurface9Wrapper，与 Primary 链上 BackBuffer 一致。
		int width = 0;
		int height = 0;
		ND3D9::D3D9Context::Instance()->GetBackBufferSize(&width, &height);
		m_desc.dwWidth = width;
		m_desc.dwHeight = height;
		if (USE_SOFTWARE_WRAPPER_9)
		{
			m_surface9Wrapper = new SoftwareSurface9Wrapper(ND3D9::D3D9Context::Instance(), m_desc.dwWidth, m_desc.dwHeight, ND3D9::D3DFMT_A8R8G8B8, m_surfaceType);
		}
		else
		{
			m_surface9Wrapper = new HardwareSurface9Wrapper(ND3D9::D3D9Context::Instance(), m_desc.dwWidth, m_desc.dwHeight, ND3D9::D3DFMT_A8R8G8B8, m_surfaceType, &m_desc);
			// HardwareSurface9Wrapper 构造里已经为 BackBuffer 设置 m_isRenderTarget = true、m_isTex = true。
		}
		break;
	}
	case ESurfaceType::ZBuffer:
	{
		int width = 0;
		int height = 0;
		ND3D9::D3D9Context::Instance()->GetBackBufferSize(&width, &height);
		m_desc.dwWidth = width;
		m_desc.dwHeight = height;
		m_surface9Wrapper = new ZBuffer9Wrapper(ND3D9::D3D9Context::Instance(), width, height, ND3D9::D3DFMT_D16, m_surfaceType);
		break;
	}
	default:
		break;
	}
}

m_IDirectDrawSurface4::~m_IDirectDrawSurface4()
{
	ProxyAddressLookupTable.DeleteAddress(this);
	if (m_clipper)
	{
		m_clipper->Release();
		m_clipper = nullptr;
	}
	if (m_linkedNextSurface)
	{
		m_linkedNextSurface->Release();
		m_linkedNextSurface = nullptr;
	}
	if (m_tex2)
	{
		if (m_tex2->GetTexture9())
		{
			auto ddraw4 = WrapperAddressLookupTable->FindWrapperOnly<m_IDirectDraw4>(IID_IDirectDraw4);
			if (ddraw4)
			{
				ddraw4->Tex9LookupTable->DeleteAddress(this);
			}
		}
		else
		{
			assert(false);
		}
		m_tex2->Release();
		m_tex2 = nullptr;
	}
	if (m_surface9Wrapper)
	{
		delete m_surface9Wrapper;
		m_surface9Wrapper = 0;
	}
}

SmartPtr<ND3D9::IDirect3DSurface9> m_IDirectDrawSurface4::GetSurface9() const
{
	return m_surface9Wrapper->GetSurface9();
}

HRESULT m_IDirectDrawSurface4::QueryInterface(REFIID riid, LPVOID FAR * ppvObj)
{
	if ((riid == IID_IDirectDrawSurface4 || riid == IID_IUnknown) && ppvObj)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	if (riid == dx6::IID_IDirect3DTexture2)
	{
		assert(m_surfaceType == ESurfaceType::Texture);
		m_tex2->AddRef();
		*ppvObj = m_tex2;
		return DD_OK;
	}
	else
	{
		HRESULT hr = ProxyInterface->QueryInterface(riid, ppvObj);

		if (SUCCEEDED(hr))
		{
			genericQueryInterface(riid, ppvObj);
		}

		return hr;
	}
}

ULONG m_IDirectDrawSurface4::AddRef()
{
	return ++Refs;
}

ULONG m_IDirectDrawSurface4::Release()
{
	ULONG x = --Refs;

	if (x == 0)
	{
		delete this;
	}

	return x;
}

HRESULT m_IDirectDrawSurface4::AddAttachedSurface(LPDIRECTDRAWSURFACE4 a)
{
	assert(!m_linkedNextSurface);
	auto suffaceZBuffer = static_cast<m_IDirectDrawSurface4*>(a);
	assert(suffaceZBuffer->m_surfaceType == ESurfaceType::ZBuffer);
	suffaceZBuffer->AddRef();
	m_linkedNextSurface = suffaceZBuffer;
	auto zbuffer9 = suffaceZBuffer->GetSurface9();
	ND3D9::D3D9Context::Instance()->GetDevice()->SetDepthStencilSurface(zbuffer9);
	return DD_OK;
}

HRESULT m_IDirectDrawSurface4::AddOverlayDirtyRect(LPRECT a)
{
	return ProxyInterface->AddOverlayDirtyRect(a);
}

HRESULT m_IDirectDrawSurface4::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx)
{
	// Phase 4.3: 性能埋点（函数入口 RAII 计数 + 累计耗时）
	PERF_SCOPE("Blt");
	// 区分 Blt / BltFast / FillColor / Flip 四类计数（每秒滑动平均由 HUD 展示）
	if (dwFlags & DDBLT_COLORFILL)
	{
		NDDFIX::Debug::PerfCounter::Instance()->IncrementFillColor();
	}
	else
	{
		NDDFIX::Debug::PerfCounter::Instance()->IncrementBlt();
	}

	auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();

	if (!(dwFlags & DDBLT_COLORFILL) && !lpDDSrcSurface)
	{
		return DDERR_INVALIDPARAMS;
	}

	m_IDirectDrawSurface4* srcSurface = static_cast<m_IDirectDrawSurface4*>(lpDDSrcSurface);
	m_IDirectDrawSurface4* destSurface = this;
	SmartPtr<ND3D9::IDirect3DSurface9> srcSurface9;
	SmartPtr<ND3D9::IDirect3DSurface9> destSurface9;

	int destWidth = lpDestRect ? (lpDestRect->right - lpDestRect->left) : destSurface->m_desc.dwWidth;
	int destHeight = lpDestRect ? (lpDestRect->bottom - lpDestRect->top) : destSurface->m_desc.dwHeight;
	
	RECT destRectOverride = { 0 };

	if (lpDestRect)
	{
		destRectOverride = *lpDestRect;
	}
	else
	{
		destRectOverride.left = 0;
		destRectOverride.top = 0;
		destRectOverride.right = destWidth;
		destRectOverride.bottom = destHeight;
	}

	if (dwFlags & DDBLT_COLORFILL)
	{
		D3DCOLOR color = lpDDBltFx->dwFillColor;
		if (m_surfaceType == ESurfaceType::Primary)
		{
			if (m_desc.ddsCaps.dwCaps & DDSCAPS_COMPLEX)
			{
				auto backBuffer9 = m_linkedNextSurface->GetSurface9();
				destSurface9 = backBuffer9;
			}
			else
			{
				destSurface9 = GetSurface9();
			}
		}
		else if (m_surfaceType == ESurfaceType::OffScreen)
		{
			destSurface9 = GetSurface9();
		}
		else if (m_surfaceType == ESurfaceType::BackBuffer)
		{
			destSurface9 = GetSurface9();
		}
		else
		{
			assert(false);
		}
		
		m_surface9Wrapper->FillColor(&destRectOverride, color);

		return DD_OK;
	}

	int srcWidth = lpSrcRect ? (lpSrcRect->right - lpSrcRect->left) : srcSurface->m_desc.dwWidth;
	int srcHeight = lpSrcRect ? (lpSrcRect->bottom - lpSrcRect->top) : srcSurface->m_desc.dwHeight;

	RECT srcRectOverride = { 0 };
	if (lpSrcRect)
	{
		srcRectOverride = *lpSrcRect;
	}
	else
	{
		srcRectOverride.left = 0;
		srcRectOverride.top = 0;
		srcRectOverride.right = srcWidth;
		srcRectOverride.bottom = srcHeight;
	}

	if (TRUE)
	{
		// correct dest rect offset;
		if (m_surfaceType == ESurfaceType::Primary)
		{
			int offsetX = destRectOverride.left;
			int offsetY = destRectOverride.top;
			destRectOverride.left = 0;
			destRectOverride.top = 0;
			destRectOverride.right = destRectOverride.right - offsetX;
			destRectOverride.bottom = destRectOverride.bottom - offsetY;
		}
	}

	srcSurface9 = srcSurface->GetSurface9();

	destSurface = this;
	destSurface9 = destSurface->GetSurface9();

	D3DCOLOR* srcColorKey = nullptr;
	D3DCOLOR* destColorKey = nullptr;

	if (dwFlags & DDBLT_KEYSRC)
	{
		srcColorKey = &srcSurface->m_desc.ddckCKSrcBlt.dwColorSpaceLowValue;
	}
	if (dwFlags & DDBLT_KEYDEST)
	{
		destColorKey = &destSurface->m_desc.ddckCKDestBlt.dwColorSpaceLowValue;
	}

	if (dwFlags & DDBLT_KEYSRCOVERRIDE)
	{
		srcColorKey = &lpDDBltFx->ddckSrcColorkey.dwColorSpaceLowValue;
	}
	if (dwFlags & DDBLT_KEYDESTOVERRIDE)
	{
		destColorKey = &lpDDBltFx->ddckDestColorkey.dwColorSpaceLowValue;
	}

	// Phase 2.2: DDBLT_ALPHASRC / DDBLT_ALPHADEST 路径不再 assert(false)。
	// D3D9 路径：用 SetRenderState(ALPHABLENDENABLE, TRUE) + SRCBLEND/DESTBLEND 模拟 alpha 混合。
	// 语义映射：
	//   DDBLT_ALPHASRC  → SRCBLEND = SRCALPHA, DESTBLEND = INVSRCALPHA（按源 alpha 混合）
	//   DDBLT_ALPHADEST → DESTBLEND = DESTALPHA, SRCBLEND = INVDESTALPHA（按目标 alpha 混合）
	//   两者都开时按 SRCALPHA 处理（DDBLT_ALPHASRC 优先，因 IDirectDrawSurface::GetCaps 文档约定）。
	// 备注：这里只设 render state；具体混合效果由后续 PS 决定。当前 D3D9 PS 是 ColorKey 路径，
	// alpha 状态会在 ColorKey 之外再叠加一层 blend。fallback 走默认 One/One 也比直接 assert 崩好。
	if ((dwFlags & DDBLT_ALPHASRC) && (dwFlags & DDBLT_ALPHADEST))
	{
		device9->SetRenderState(ND3D9::D3DRS_ALPHABLENDENABLE, TRUE);
		device9->SetRenderState(ND3D9::D3DRS_SRCBLEND, ND3D9::D3DBLEND_SRCALPHA);
		device9->SetRenderState(ND3D9::D3DRS_DESTBLEND, ND3D9::D3DBLEND_INVSRCALPHA);
	}
	else if (dwFlags & DDBLT_ALPHASRC)
	{
		device9->SetRenderState(ND3D9::D3DRS_ALPHABLENDENABLE, TRUE);
		device9->SetRenderState(ND3D9::D3DRS_SRCBLEND, ND3D9::D3DBLEND_SRCALPHA);
		device9->SetRenderState(ND3D9::D3DRS_DESTBLEND, ND3D9::D3DBLEND_INVSRCALPHA);
	}
	else if (dwFlags & DDBLT_ALPHADEST)
	{
		device9->SetRenderState(ND3D9::D3DRS_ALPHABLENDENABLE, TRUE);
		device9->SetRenderState(ND3D9::D3DRS_SRCBLEND, ND3D9::D3DBLEND_DESTALPHA);
		device9->SetRenderState(ND3D9::D3DRS_DESTBLEND, ND3D9::D3DBLEND_INVDESTALPHA);
	}

	m_surface9Wrapper->Blt(srcSurface->m_surface9Wrapper, &srcRectOverride, &destRectOverride, srcColorKey, destColorKey);

	if (m_surfaceType == ESurfaceType::Primary)
	{
		if (D3DERR_DEVICELOST == device9->Present(&destRectOverride, &destRectOverride, 0, nullptr))
		{
			ND3D9::D3D9Context::Instance()->TagDeviceLost();
			return DDERR_SURFACELOST;
		}
		else
		{
			srcSurface->m_surface9Wrapper->FillColor(&srcRectOverride, 0);
		}

		// Phase 4.2: HUD 渲染（在 Present 之后叠加显示）。设备丢失/未启用 → 内部 no-op。
		NDDFIX::Debug::PerfCounter::Instance()->IncrementRender();
		NDDFIX::Debug::HudRenderer::Instance()->Render();
	}
	return DD_OK;
}

HRESULT m_IDirectDrawSurface4::BltBatch(LPDDBLTBATCH a, DWORD b, DWORD c)
{
	if (a && a->lpDDSSrc)
	{
		a->lpDDSSrc = static_cast<m_IDirectDrawSurface *>(a->lpDDSSrc)->GetProxyInterface();
	}

	return ProxyInterface->BltBatch(a, b, c);
}

HRESULT m_IDirectDrawSurface4::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags)
{
	// Phase 4.3: 性能埋点 + BltFast 计数
	PERF_SCOPE("BltFast");
	NDDFIX::Debug::PerfCounter::Instance()->IncrementBltFast();

	if (!lpDDSrcSurface || !lpSrcRect)
	{
		return DDERR_INVALIDPARAMS;
	}

	m_IDirectDrawSurface4* srcSurface = static_cast<m_IDirectDrawSurface4*>(lpDDSrcSurface);
	if (m_surfaceType == ESurfaceType::OffScreen || m_surfaceType == ESurfaceType::BackBuffer)
	{
		m_IDirectDrawSurface4* destSurface = this;

		D3DCOLOR* srcColorKey = nullptr;
		D3DCOLOR* destColorKey = nullptr;

		if (dwFlags & DDBLTFAST_SRCCOLORKEY)
		{
			srcColorKey = &srcSurface->m_desc.ddckCKSrcBlt.dwColorSpaceLowValue;
		}
		if (dwFlags & DDBLTFAST_DESTCOLORKEY)
		{
			destColorKey = &destSurface->m_desc.ddckCKDestBlt.dwColorSpaceLowValue;
		}

		HRESULT hr = m_surface9Wrapper->BltFast(srcSurface->m_surface9Wrapper, lpSrcRect, dwX, dwY, srcColorKey, destColorKey);

		return hr;
	}
	else if (m_surfaceType == ESurfaceType::Primary)
	{
		// P0 修复: 原来 Primary 路径直接 assert(false)。
		// 改为内部 redirect 到 Blt：构造 destRect = {dwX, dwY, dwX+w, dwY+h} 后调 Blt，保留 ColorKey / 视口等原有路径。
		RECT destRect = { 0 };
		destRect.left = (LONG)dwX;
		destRect.top = (LONG)dwY;
		destRect.right = destRect.left + (lpSrcRect->right - lpSrcRect->left);
		destRect.bottom = destRect.top + (lpSrcRect->bottom - lpSrcRect->top);
		return Blt(&destRect, lpDDSrcSurface, lpSrcRect, dwFlags, nullptr);
	}
	return DDERR_GENERIC;
}

HRESULT m_IDirectDrawSurface4::DeleteAttachedSurface(DWORD a, LPDIRECTDRAWSURFACE4 b)
{
	if (b)
	{
		b = static_cast<m_IDirectDrawSurface4 *>(b)->GetProxyInterface();
	}

	return ProxyInterface->DeleteAttachedSurface(a, b);
}

HRESULT m_IDirectDrawSurface4::EnumAttachedSurfaces(LPVOID a, LPDDENUMSURFACESCALLBACK2 b)
{
	ENUMSURFACE2 CallbackContext;
	CallbackContext.lpContext = a;
	CallbackContext.lpCallback = b;

	return ProxyInterface->EnumAttachedSurfaces(&CallbackContext, m_IDirectDrawEnumSurface2::EnumSurface2Callback);
}

HRESULT m_IDirectDrawSurface4::EnumOverlayZOrders(DWORD a, LPVOID b, LPDDENUMSURFACESCALLBACK2 c)
{
	ENUMSURFACE2 CallbackContext;
	CallbackContext.lpContext = b;
	CallbackContext.lpCallback = c;

	return ProxyInterface->EnumOverlayZOrders(a, &CallbackContext, m_IDirectDrawEnumSurface2::EnumSurface2Callback);
}

HRESULT m_IDirectDrawSurface4::Flip(LPDIRECTDRAWSURFACE4 a, DWORD b)
{
	// P0 修复: 不再访问 ProxyInterface（nullptr），改用 D3D9 Present。
	// 仿照 m_IDirectDrawSurface4::Blt (Primary 路径) 与 HardwareSurface9Wrapper::DrawSprite。
	// Phase 4.3: 性能埋点 + Flip 计数
	PERF_SCOPE("Flip");
	NDDFIX::Debug::PerfCounter::Instance()->IncrementFlip();

	auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();

	// 翻页链中 backbuffer 是 m_linkedNextSurface，把它设为当前 RenderTarget。
	if (m_linkedNextSurface && m_linkedNextSurface->m_surface9Wrapper)
	{
		device9->SetRenderTarget(0, m_linkedNextSurface->GetSurface9());
	}

	// D9 Present: 把后台缓冲提交到屏幕。
	if (D3DERR_DEVICELOST == device9->Present(nullptr, nullptr, 0, nullptr))
	{
		ND3D9::D3D9Context::Instance()->TagDeviceLost();
		return DDERR_SURFACELOST;
	}

	// Phase 4.2: HUD 渲染（在 Present 之后叠加显示）。设备丢失/未启用 → 内部 no-op。
	NDDFIX::Debug::PerfCounter::Instance()->IncrementRender();
	NDDFIX::Debug::HudRenderer::Instance()->Render();

	return DD_OK;
}

HRESULT m_IDirectDrawSurface4::GetAttachedSurface(LPDDSCAPS2 a, LPDIRECTDRAWSURFACE4 FAR * b)
{
	if (m_linkedNextSurface)
	{
		LPDDSCAPS2 nextSurfaceCaps = &m_linkedNextSurface->m_desc.ddsCaps;
		if ((nextSurfaceCaps->dwCaps & a->dwCaps) == a->dwCaps)
		{
			if (nextSurfaceCaps->dwCaps & DDSCAPS_MIPMAP)
			{
				if (nextSurfaceCaps->dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL)
				{
					*b = m_linkedNextSurface;
					m_linkedNextSurface->AddRef();
					return DD_OK;
				}
				else
				{
					// impossible run to here?
					assert(false);
					return DDERR_NOTFOUND;
				}
			}
			else if (nextSurfaceCaps->dwCaps & DDSCAPS_BACKBUFFER)
			{
				*b = m_linkedNextSurface;
				m_linkedNextSurface->AddRef();
				return DD_OK;
			}
			else
			{
				assert(false);
				return DDERR_NOTFOUND;
			}
		}
		else
		{
			*b = nullptr;
			return DDERR_NOTFOUND;
		}
	}
	else
	{
		*b = nullptr;
		return DDERR_NOTFOUND;
	}

}

HRESULT m_IDirectDrawSurface4::GetBltStatus(DWORD a)
{
	return ProxyInterface->GetBltStatus(a);
}

HRESULT m_IDirectDrawSurface4::GetCaps(LPDDSCAPS2 a)
{
	return ProxyInterface->GetCaps(a);
}

HRESULT m_IDirectDrawSurface4::GetClipper(LPDIRECTDRAWCLIPPER FAR * a)
{
	if (m_clipper)
	{
		m_clipper->AddRef();
		*a = m_clipper;
		return DD_OK;
	}
	else
	{
		*a = nullptr;
		return DDERR_NOCLIPPERATTACHED;
	}
}

HRESULT m_IDirectDrawSurface4::GetColorKey(DWORD a, LPDDCOLORKEY b)
{
	HRESULT hr = DDERR_NOCOLORKEY;
	if (b)
	{
		switch (a)
		{
		case DDCKEY_DESTBLT:
			if (m_desc.dwFlags & DDSD_CKDESTBLT)
			{
				*b = m_desc.ddckCKDestBlt;
				hr = DD_OK;
			}
			break;
		case DDCKEY_DESTOVERLAY:
			if (m_desc.dwFlags & DDSD_CKDESTOVERLAY)
			{
				*b = m_desc.ddckCKDestOverlay;
				hr = DD_OK;
			}
			break;
		case DDCKEY_SRCBLT:
			if (m_desc.dwFlags & DDSD_CKSRCBLT)
			{
				*b = m_desc.ddckCKSrcBlt;
				hr = DD_OK;
			}
			break;
		case DDCKEY_SRCOVERLAY:
			if (m_desc.dwFlags & DDSD_CKSRCOVERLAY)
			{
				*b = m_desc.ddckCKSrcOverlay;
				hr = DD_OK;
			}
			break;
		default:
			assert(false);
			break;
		}

	}
	else
	{
		hr = DDERR_INVALIDPARAMS;
	}
	return hr;
}

HRESULT m_IDirectDrawSurface4::GetDC(HDC FAR * a)
{
	// TODO: IDirect3DSurface9::GetDC() 在有ALPHA通道时，行为不确定
	return m_surface9Wrapper->GetDC(a);
}

HRESULT m_IDirectDrawSurface4::GetFlipStatus(DWORD a)
{
	return ProxyInterface->GetFlipStatus(a);
}

HRESULT m_IDirectDrawSurface4::GetOverlayPosition(LPLONG a, LPLONG b)
{
	return ProxyInterface->GetOverlayPosition(a, b);
}

HRESULT m_IDirectDrawSurface4::GetPalette(LPDIRECTDRAWPALETTE FAR * a)
{
	HRESULT hr = ProxyInterface->GetPalette(a);

	if (SUCCEEDED(hr))
	{
		*a = ProxyAddressLookupTable.FindAddress<m_IDirectDrawPalette>(*a);
	}

	return hr;
}

HRESULT m_IDirectDrawSurface4::GetPixelFormat(LPDDPIXELFORMAT a)
{
	return ProxyInterface->GetPixelFormat(a);
}

HRESULT m_IDirectDrawSurface4::GetSurfaceDesc(LPDDSURFACEDESC2 a)
{
	*a = m_desc;
	return DD_OK;
}

HRESULT m_IDirectDrawSurface4::Initialize(LPDIRECTDRAW a, LPDDSURFACEDESC2 b)
{
	if (a)
	{
		a = static_cast<m_IDirectDraw *>(a)->GetProxyInterface();
	}

	return ProxyInterface->Initialize(a, b);
}

HRESULT m_IDirectDrawSurface4::IsLost()
{
	if (m_surfaceType == ESurfaceType::Primary &&
		ND3D9::D3D9Context::Instance()->IsDeviceLost()
		)
	{
		return DDERR_SURFACELOST;
	}
	else
	{
		return DD_OK;
	}
}

HRESULT m_IDirectDrawSurface4::Lock(LPRECT lpDestRect, LPDDSURFACEDESC2 lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent)
{
	// TODO: https://msdn.microsoft.com/en-us/library/windows/desktop/gg426221(v=vs.85).aspx
	// Because you can call IDirectDrawSurface7::Lock multiple times for the same surface with different destination rectangles, the pointer in lpRect links the calls to the IDirectDrawSurface7::Lock and IDirectDrawSurface7::Unlock methods.

	if (m_locked)
	{
		return DDERR_GENERIC;
	}

	HRESULT hr = DDERR_GENERIC;

	bool readonly = dwFlags & DDLOCK_READONLY;
	bool nosyslock = dwFlags & DDLOCK_NOSYSLOCK;
	bool discard = dwFlags & DDLOCK_DISCARDCONTENTS;
	DWORD lockFlags =
		(readonly ? D3DLOCK_READONLY : 0) |
		(nosyslock ? D3DLOCK_NOSYSLOCK : 0) |
		(discard ? D3DLOCK_DISCARD : 0);

	if (m_surfaceType == ESurfaceType::OffScreen)
	{
		// Phase 3.3: Allow3DOffScreenLock 决定 3D OffScreen Lock 行为
		// 默认 false = 保持原"trick for meteor blade"行为（返 DDERR_INVALIDPARAMS）
		if (m_desc.ddsCaps.dwCaps == (DDSCAPS_3DDEVICE | DDSCAPS_OFFSCREENPLAIN)
			&& !NDDFIX::Config::ConfigManager::Instance()->GetRender().allow3DOffScreenLock)
		{
			hr = DDERR_INVALIDPARAMS;
		}
		else
		{
			ND3D9::D3DLOCKED_RECT lockedRect = { 0 };
			if (SUCCEEDED(GetSurface9()->LockRect(&lockedRect, lpDestRect, lockFlags)))
			{
				*lpDDSurfaceDesc = m_desc;
				lpDDSurfaceDesc->lpSurface = lockedRect.pBits;
				lpDDSurfaceDesc->lPitch = lockedRect.Pitch;
				hr = DD_OK;
			}
			else
			{
				hr = DDERR_INVALIDPARAMS;
			}
		}
	}
	else if (m_surfaceType == ESurfaceType::Texture)
	{
		ND3D9::D3DLOCKED_RECT lockedRect = { 0 };
		int level = 0;
		SmartPtr<ND3D9::IDirect3DTexture9> tex9;
		if (m_desc.ddsCaps.dwCaps & DDSCAPS_MIPMAP)
		{
			if (m_desc.ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL)
			{
				auto curSurface = this;
				while (curSurface && curSurface->m_linkedPrevSurface)
				{
					level += 1;
					curSurface = curSurface->m_linkedPrevSurface;
				}
				tex9 = curSurface->m_tex2->GetTexture9();
			}
			else
			{
				tex9 = m_tex2->GetTexture9();
			}
		}
		else
		{
			tex9 = m_tex2->GetTexture9();
		}

		if (SUCCEEDED(tex9->LockRect(level, &lockedRect, lpDestRect, lockFlags)))
		{
			*lpDDSurfaceDesc = m_desc;
			lpDDSurfaceDesc->lpSurface = lockedRect.pBits;
			lpDDSurfaceDesc->lPitch = lockedRect.Pitch;
			hr = DD_OK;
		}
		else
		{
			hr = DDERR_INVALIDPARAMS;
		}
	}
	else if (m_surfaceType == ESurfaceType::BackBuffer)
	{
		// Phase 3.3: AllowBackBufferLock 决定 BackBuffer Lock 行为
		// 默认 false = 保持原"trick for meteor blade"行为（永远返 DDERR_GENERIC）
		if (!NDDFIX::Config::ConfigManager::Instance()->GetRender().allowBackBufferLock)
		{
			hr = DDERR_GENERIC;
		}
		else
		{

			ND3D9::D3DLOCKED_RECT lockedRect = { 0 };
			if (SUCCEEDED(GetSurface9()->LockRect(&lockedRect, lpDestRect, lockFlags)))
			{
				*lpDDSurfaceDesc = m_desc;
				lpDDSurfaceDesc->lpSurface = lockedRect.pBits;
				lpDDSurfaceDesc->lPitch = lockedRect.Pitch;
				hr = DD_OK;
			}
			else
			{
				hr = DDERR_INVALIDPARAMS;
			}
		}
	}
	else
	{
		assert(false);
		hr = DDERR_GENERIC;
	}
	
	if (SUCCEEDED(hr))
	{
		m_locked = true;
	}
	return hr;
}

HRESULT m_IDirectDrawSurface4::ReleaseDC(HDC a)
{
	return m_surface9Wrapper->ReleaseDC(a);
}

HRESULT m_IDirectDrawSurface4::Restore()
{
	if (m_surfaceType == ESurfaceType::Primary)
	{
		assert(ND3D9::D3D9Context::Instance()->IsDeviceLost());
		if (!SUCCEEDED(ND3D9::D3D9Context::Instance()->ResetDevice()))
		{
			return DDERR_GENERIC;
		}
		else
		{
			return DD_OK;
		}
	}
	else if (m_surfaceType == ESurfaceType::ZBuffer)
	{
		auto zbuffer9 = GetSurface9();
		ND3D9::D3D9Context::Instance()->GetDevice()->SetDepthStencilSurface(zbuffer9);
		// Phase 3.3: ZBufferAutoRestore 决定是否在 Restore 时开 Z
		// 默认 true = 保持原"trick for game meteor blade"行为
		if (NDDFIX::Config::ConfigManager::Instance()->GetRender().zBufferAutoRestore)
		{
			ND3D9::D3D9Context::Instance()->GetDevice()->SetRenderState(ND3D9::D3DRS_ZENABLE, TRUE);
		}
		return DD_OK;
	}
	else
	{
		return DD_OK;
	}
}

HRESULT m_IDirectDrawSurface4::SetClipper(LPDIRECTDRAWCLIPPER a)
{
	if (a)
	{
		if (m_clipper)
		{
			assert(m_clipper == a);
		}
		a->AddRef();
		m_clipper = static_cast<m_IDirectDrawClipper*>(a);
		return DD_OK;
	}
	else
	{
		if (m_clipper)
		{
			m_clipper->Release();
			m_clipper = nullptr;
		}
		return DD_OK;
	}

}

HRESULT m_IDirectDrawSurface4::SetColorKey(DWORD a, LPDDCOLORKEY b)
{
	if (b)
	{
		switch (a)
		{
		case DDCKEY_COLORSPACE:
			break;
		case DDCKEY_DESTBLT:
			m_desc.ddckCKDestBlt = *b;
			m_desc.dwFlags |= DDSD_CKDESTBLT;
			break;
		case DDCKEY_DESTOVERLAY:
			m_desc.ddckCKDestOverlay = *b;
			m_desc.dwFlags |= DDSD_CKDESTOVERLAY;
			break;
		case DDCKEY_SRCBLT:
			m_desc.ddckCKSrcBlt = *b;
			m_desc.dwFlags |= DDSD_CKSRCBLT;
			break;
		case DDCKEY_SRCOVERLAY:
			m_desc.ddckCKSrcOverlay = *b;
			m_desc.dwFlags |= DDSD_CKSRCOVERLAY;
			break;
		default:
			assert(false);
			break;
		}
	}
	else
	{
		return DDERR_INVALIDPARAMS;
	}

	return DD_OK;
}

HRESULT m_IDirectDrawSurface4::SetOverlayPosition(LONG a, LONG b)
{
	return ProxyInterface->SetOverlayPosition(a, b);
}

HRESULT m_IDirectDrawSurface4::SetPalette(LPDIRECTDRAWPALETTE a)
{
	if (a)
	{
		a = static_cast<m_IDirectDrawPalette *>(a)->GetProxyInterface();
	}

	return ProxyInterface->SetPalette(a);
}

HRESULT m_IDirectDrawSurface4::Unlock(LPRECT a)
{
	if (!m_locked)
	{
		return DDERR_GENERIC;
	}

	HRESULT hr = DDERR_GENERIC;

	if (m_surfaceType == ESurfaceType::OffScreen)
	{
		if (SUCCEEDED(GetSurface9()->UnlockRect()))
		{
			hr = DD_OK;
		}
		else
		{
			hr = DDERR_INVALIDPARAMS;
		}
	}
	else if (m_surfaceType == ESurfaceType::Texture)
	{
		int level = 0;
		SmartPtr<ND3D9::IDirect3DTexture9> tex9 = nullptr;
		if (m_desc.ddsCaps.dwCaps & DDSCAPS_MIPMAP)
		{
			if (m_desc.ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL)
			{
				auto curSurface = this;
				while (curSurface && curSurface->m_linkedPrevSurface)
				{
					level += 1;
					curSurface = curSurface->m_linkedPrevSurface;
				}
				tex9 = curSurface->m_tex2->GetTexture9();
			}
			else
			{
				tex9 = m_tex2->GetTexture9();
			}
		}
		else
		{
			tex9 = m_tex2->GetTexture9();
		}


		if (SUCCEEDED(tex9->UnlockRect(level)))
		{
			hr = DD_OK;
		}
		else
		{
			hr = DDERR_INVALIDPARAMS;
		}
	}
	else if (m_surfaceType == ESurfaceType::BackBuffer)
	{
		if (SUCCEEDED(GetSurface9()->UnlockRect()))
		{
			hr = DD_OK;
		}
		else
		{
			hr = DDERR_INVALIDPARAMS;
		}
	}
	else
	{
		hr = DDERR_GENERIC;
	}

	if (SUCCEEDED(hr))
	{
		m_locked = false;
	}
	return hr;
}

HRESULT m_IDirectDrawSurface4::UpdateOverlay(LPRECT a, LPDIRECTDRAWSURFACE4 b, LPRECT c, DWORD d, LPDDOVERLAYFX e)
{
	if (b)
	{
		b = static_cast<m_IDirectDrawSurface4 *>(b)->GetProxyInterface();
	}

	return ProxyInterface->UpdateOverlay(a, b, c, d, e);
}

HRESULT m_IDirectDrawSurface4::UpdateOverlayDisplay(DWORD a)
{
	return ProxyInterface->UpdateOverlayDisplay(a);
}

HRESULT m_IDirectDrawSurface4::UpdateOverlayZOrder(DWORD a, LPDIRECTDRAWSURFACE4 b)
{
	if (b)
	{
		b = static_cast<m_IDirectDrawSurface4 *>(b)->GetProxyInterface();
	}

	return ProxyInterface->UpdateOverlayZOrder(a, b);
}

HRESULT m_IDirectDrawSurface4::GetDDInterface(LPVOID FAR * a)
{
	*a = WrapperAddressLookupTable->FindWrapper<m_IDirectDraw4>(IID_IDirectDraw4);
	return DD_OK;
}

HRESULT m_IDirectDrawSurface4::PageLock(DWORD a)
{
	return ProxyInterface->PageLock(a);
}

HRESULT m_IDirectDrawSurface4::PageUnlock(DWORD a)
{
	return ProxyInterface->PageUnlock(a);
}

HRESULT m_IDirectDrawSurface4::SetSurfaceDesc(LPDDSURFACEDESC2 a, DWORD b)
{
	return ProxyInterface->SetSurfaceDesc(a, b);
}

HRESULT m_IDirectDrawSurface4::SetPrivateData(REFGUID a, LPVOID b, DWORD c, DWORD d)
{
	return ProxyInterface->SetPrivateData(a, b, c, d);
}

HRESULT m_IDirectDrawSurface4::GetPrivateData(REFGUID a, LPVOID b, LPDWORD c)
{
	return ProxyInterface->GetPrivateData(a, b, c);
}

HRESULT m_IDirectDrawSurface4::FreePrivateData(REFGUID a)
{
	return ProxyInterface->FreePrivateData(a);
}

HRESULT m_IDirectDrawSurface4::GetUniquenessValue(LPDWORD a)
{
	return ProxyInterface->GetUniquenessValue(a);
}

HRESULT m_IDirectDrawSurface4::ChangeUniquenessValue()
{
	return ProxyInterface->ChangeUniquenessValue();
}
