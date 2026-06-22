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
#include <assert.h>

HRESULT m_IDirect3D3::QueryInterface(REFIID riid, LPVOID * ppvObj)
{
	if ((riid == dx6::IID_IDirect3D3 || riid == IID_IUnknown) && ppvObj)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	// Phase 2.6: QueryInterface 创建 m_IDirect3DDevice3 改走 WrapperLookupTable 单例。
	// 之前是直接 `new m_IDirect3DDevice3(nullptr, nullptr)`，导致每次 QI 都创一个新 instance，
	// 同一份逻辑设备在游戏侧被当成两个对象，state 不同步（ColorKey 标志 / light ambient / FVF 等）。
	// 现在 FindWrapper 会先在 lookup table 里查 dx6::IID_IDirect3DDevice3，已存在则复用，
	// 不存在则 new 并 SaveWrapper。
	// m_IDirect3DDevice3 构造内已有 `WrapperAddressLookupTable->SaveWrapper(this, dx6::IID_IDirect3DDevice3)`，所以单例有效。
	if (riid == dx6::IID_IDirect3DDevice3)
	{
		*ppvObj = WrapperAddressLookupTable->FindWrapper<m_IDirect3DDevice3>(dx6::IID_IDirect3DDevice3);
	}
	else
	{
		assert(false);
	}

	return S_OK;
}

ULONG m_IDirect3D3::AddRef()
{
	return ++Refs;
	return ProxyInterface->AddRef();
}

ULONG m_IDirect3D3::Release()
{
	//ULONG x = ProxyInterface->Release();
	ULONG x = --Refs;

	if (x == 0)
	{
		delete this;
	}

	return x;
}

HRESULT m_IDirect3D3::EnumDevices(dx6::LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg)
{
	if (!lpEnumDevicesCallback) 
		return DDERR_INVALIDPARAMS;
	m_IDirectDraw* ddraw = WrapperAddressLookupTable->FindWrapper<m_IDirectDraw>(IID_IDirectDraw);
	dx6::D3DDEVICEDESC desc = *ddraw->GetD3DDevice3Desc();
	
	UINT adapterCount = ND3D9::D3D9Context::Instance()->GetD3D9()->GetAdapterCount();
	for (UINT i = 0; i < adapterCount; i++)
	{
		ND3D9::D3DADAPTER_IDENTIFIER9 identifier;
		if (SUCCEEDED(ND3D9::D3D9Context::Instance()->GetD3D9()->GetAdapterIdentifier(i, 0, &identifier)))
		{
			auto guid = identifier.DeviceIdentifier;
			auto description = identifier.Description;
			auto deviceName = identifier.DeviceName;
			if (!SUCCEEDED(lpEnumDevicesCallback(&guid, description, deviceName, &desc, &desc, lpUserArg)))
			{
				return DDERR_GENERIC;
			};
		}
		else
		{
			return DDERR_GENERIC;
		}
	}
	
	return D3D_OK;
}

HRESULT m_IDirect3D3::CreateLight(dx6::LPDIRECT3DLIGHT * a, LPUNKNOWN b)
{
	*a = new m_IDirect3DLight(nullptr, nullptr);
	return DD_OK;
}

HRESULT m_IDirect3D3::CreateMaterial(dx6::LPDIRECT3DMATERIAL3 * a, LPUNKNOWN b)
{
	*a = new m_IDirect3DMaterial3(nullptr, nullptr);
	return DD_OK;
}

HRESULT m_IDirect3D3::CreateViewport(dx6::LPDIRECT3DVIEWPORT3 * a, LPUNKNOWN b)
{
	*a = new m_IDirect3DViewport3(nullptr, WrapperAddressLookupTable);
	return DD_OK;
}

HRESULT m_IDirect3D3::FindDevice(dx6::LPD3DFINDDEVICESEARCH a, dx6::LPD3DFINDDEVICERESULT b)
{
	// P0 修复: 之前直接 ProxyInterface->FindDevice(a, b)，但 ProxyInterface = nullptr → 段错误。
	// 改用 m_IDirectDraw 缓存的 dx6::D3DDEVICEDESC（已在 IDirectDraw 构造时填好）合成 result。
	if (!a || !b)
	{
		return DDERR_INVALIDPARAMS;
	}

	m_IDirectDraw* ddraw = WrapperAddressLookupTable->FindWrapper<m_IDirectDraw>(IID_IDirectDraw);
	dx6::D3DDEVICEDESC desc = *ddraw->GetD3DDevice3Desc();
	b->ddDeviceDesc = desc;
	return D3D_OK;
}

HRESULT m_IDirect3D3::CreateDevice(REFCLSID a, LPDIRECTDRAWSURFACE4 b, dx6::LPDIRECT3DDEVICE3 * c, LPUNKNOWN d)
{
	*c = WrapperAddressLookupTable->FindWrapper<m_IDirect3DDevice3>(dx6::IID_IDirect3DDevice3);
	return D3D_OK;
}

HRESULT m_IDirect3D3::CreateVertexBuffer(dx6::LPD3DVERTEXBUFFERDESC a, dx6::LPDIRECT3DVERTEXBUFFER * b, DWORD c, LPUNKNOWN d)
{
	// P0 修复 (Task 1.2): 不再调 ProxyInterface (nullptr)。
	// 改用 D3D9 device->CreateVertexBuffer，包装成 m_IDirect3DVertexBuffer7。
	// 注意：函数签名要求返回 dx6::LPDIRECT3DVERTEXBUFFER (v1)，m_IDirect3DVertexBuffer7 的 vtable 与 v1 前 8 槽
	// 兼容（DX6/7 vertex buffer 的 Lock/Unlock/ProcessVertices/GetVertexBufferDesc/Optimize 槽位一致），
	// 这里用 static_cast 投射。后续 Lock/Unlock 走包装内 D3D9 后端，不依赖 ProxyInterface。
	if (!a || !b)
	{
		return DDERR_INVALIDPARAMS;
	}

	// 把 DX6 dx6::D3DVERTEXBUFFERDESC 翻译到 D3D9 CreateVertexBuffer。
	// dwCaps 决定 pool，dwFVF 决定 fvf，dwSize 决定 length。
	ND3D9::D3DPOOL pool = ND3D9::D3DPOOL_MANAGED;
	if (a->dwCaps & D3DVBCAPS_SYSTEMMEMORY)
	{
		pool = ND3D9::D3DPOOL_SYSTEMMEM;
	}
	else if (a->dwCaps & D3DVBCAPS_WRITEONLY)
	{
		// WRITEONLY：放 MANAGED 池即可，D3D9 优化时不必回读
		pool = ND3D9::D3DPOOL_MANAGED;
	}
	// dx6 没显式标志时默认 MANAGED（与原项目 texture 选 MANAGED 保持一致）

	DWORD usage = 0;
	if (a->dwCaps & D3DVBCAPS_DONOTCLIP)
	{
		// D3D9 无对应，忽略
	}

	ND3D9::Resource9Handle vb9Handle = ND3D9::D3D9Context::Instance()->CreateVertexBuffer9(
		a->dwSize, usage, a->dwFVF, pool);

	if (vb9Handle == 0)
	{
		return DDERR_GENERIC;
	}

	// 包装成 m_IDirect3DVertexBuffer7。第二个参数 (void*) 复用为 Resource9Handle 槽位（见 dx6::IDirect3DVertexBuffer7.h）。
	// ProxyInterface 传 nullptr：游戏后续不会通过 ProxyInterface 调到原始 ddraw。
	auto wrapper = new m_IDirect3DVertexBuffer7(nullptr, (void*)(INT_PTR)vb9Handle);
	*b = (dx6::LPDIRECT3DVERTEXBUFFER)(dx6::IDirect3DVertexBuffer7*)wrapper;
	return D3D_OK;
}

HRESULT m_IDirect3D3::EnumZBufferFormats(REFCLSID riidDevice, dx6::LPD3DENUMPIXELFORMATSCALLBACK lpEnumCallback, LPVOID lpContext)
{
	// code is taken from project DXGL
	DDPIXELFORMAT ddpf;
	ZeroMemory(&ddpf, sizeof(DDPIXELFORMAT));
	ddpf.dwSize = sizeof(DDPIXELFORMAT);
	ddpf.dwFlags = DDPF_ZBUFFER;
	ddpf.dwZBufferBitDepth = 16;
	ddpf.dwZBitMask = 0xffff;
	if (lpEnumCallback(&ddpf, lpContext) == D3DENUMRET_CANCEL) return D3D_OK;
	ddpf.dwZBufferBitDepth = 24;
	ddpf.dwZBitMask = 0xffffff00;
	if (lpEnumCallback(&ddpf, lpContext) == D3DENUMRET_CANCEL) return D3D_OK;
	ddpf.dwZBufferBitDepth = 32;
	if (lpEnumCallback(&ddpf, lpContext) == D3DENUMRET_CANCEL) return D3D_OK;
	ddpf.dwZBitMask = 0xffffffff;
	if (lpEnumCallback(&ddpf, lpContext) == D3DENUMRET_CANCEL) return D3D_OK;
// 	if (glDD7->renderer)
// 	{
// 		if (glDD7->renderer->ext->GLEXT_EXT_packed_depth_stencil || glDD7->renderer->ext->GLEXT_NV_packed_depth_stencil)
// 		{
// 			ddpf.dwZBufferBitDepth = 32;
// 			ddpf.dwStencilBitDepth = 8;
// 			ddpf.dwZBitMask = 0xffffff00;
// 			ddpf.dwStencilBitMask = 0xff;
// 			if (lpEnumCallback(&ddpf, lpContext) == D3DENUMRET_CANCEL) TRACE_RET(HRESULT, 23, D3D_OK);
// 			ddpf.dwZBitMask = 0x00ffffff;
// 			ddpf.dwStencilBitMask = 0xff000000;
// 			if (lpEnumCallback(&ddpf, lpContext) == D3DENUMRET_CANCEL) TRACE_RET(HRESULT, 23, D3D_OK);
// 		}
// 	}
// 	TRACE_EXIT(23, D3D_OK);
	return D3D_OK;
}

HRESULT m_IDirect3D3::EvictManagedTextures()
{
	return ND3D9::D3D9Context::Instance()->GetDevice()->EvictManagedResources();
}
