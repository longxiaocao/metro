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

// code is taken from project DXGL
// Use EXACTLY one line per entry.  Don't change layout of the list.
// Phase 5.2: 数组大小改用 std::size 推导，移除 __LINE__ 依赖。
//   旧实现用 __LINE__ 差值计数，对"必须在文件末尾留 4 行空行"极脆，
//   任何插入/删除都会让 g_numtexformats 错位。
//   现改为 static constexpr 派生，编译器自行算长度，零运行时开销。
#include <iterator>  // std::size
const DDPIXELFORMAT texformats[] =
{ // Size					Flags							FOURCC	bits	R/Ymask		G/U/Zmask	B/V/STmask	A/Zmask
{ sizeof(DDPIXELFORMAT),	DDPF_PALETTEINDEXED8,			0,		8,		0,			0,			0,			0 },
{ sizeof(DDPIXELFORMAT),	DDPF_RGB,						0,		8,		0xE0,		0x1C,		0x3,		0 },
{ sizeof(DDPIXELFORMAT),	DDPF_RGB,						0,		16,		0x7C00,		0x3E0,		0x1F,		0 },
{ sizeof(DDPIXELFORMAT),	DDPF_RGB,						0,		16,		0xF800,		0x7E0,		0x1F,		0 },
{ sizeof(DDPIXELFORMAT),	DDPF_RGB,						0,		24,		0xFF0000,	0xFF00,		0xFF,		0 },
{ sizeof(DDPIXELFORMAT),	DDPF_RGB,						0,		32,		0xFF0000,	0xFF00,		0xFF,		0 },
{ sizeof(DDPIXELFORMAT),	DDPF_RGB,						0,		32,		0xFF,		0xFF00,		0xFF0000,	0 },
{ sizeof(DDPIXELFORMAT),	DDPF_RGB | DDPF_ALPHAPIXELS,		0,		16,		0xE0,		0x1C,		0x3,		0xFF00 },
{ sizeof(DDPIXELFORMAT), DDPF_RGB | DDPF_ALPHAPIXELS,		0,		16,		0xF00,		0xF0,		0xF,		0xF000 },
{ sizeof(DDPIXELFORMAT), DDPF_RGB | DDPF_ALPHAPIXELS,		0,		16,		0x7c00,		0x3E0,		0x1F,		0x8000 },
{ sizeof(DDPIXELFORMAT), DDPF_RGB | DDPF_ALPHAPIXELS,		0,		32,		0xFF0000,	0xFF00,		0xFF,		0xFF000000 },
{ sizeof(DDPIXELFORMAT),	DDPF_LUMINANCE,					0,		8,		0xFF,		0,			0,			0 },
{ sizeof(DDPIXELFORMAT),	DDPF_ALPHA,						0,		8,		0,			0,			0,			0 },
{ sizeof(DDPIXELFORMAT),	DDPF_LUMINANCE | DDPF_ALPHAPIXELS,0,		16,		0xFF,		0,			0,			0xFF00 },
{ sizeof(DDPIXELFORMAT), DDPF_ZBUFFER,					0,		16,		0,			0xFFFF,		0,			0 },
{ sizeof(DDPIXELFORMAT),	DDPF_ZBUFFER,					0,		24,		0,			0xFFFFFF00,	0,			0 },
{ sizeof(DDPIXELFORMAT),	DDPF_ZBUFFER,					0,		32,		0,			0xFFFFFF00,	0,			0 },
{ sizeof(DDPIXELFORMAT),	DDPF_ZBUFFER,					0,		32,		0,			0xFFFFFFFF,	0,			0 },
{ sizeof(DDPIXELFORMAT),	DDPF_ZBUFFER,					0,		32,		8,			0xFFFFFF00,	0xFF,		0 },
{ sizeof(DDPIXELFORMAT),	DDPF_ZBUFFER,					0,		32,		8,			0xFF,		0xFFFFFF00,	0 }
};
static constexpr int g_numtexformats = static_cast<int>(std::size(texformats));
const DDPIXELFORMAT* g_texformats = texformats;

HRESULT m_IDirect3DTexture::QueryInterface(REFIID riid, LPVOID * ppvObj)
{
	if ((riid == IID_IDirect3DTexture || riid == IID_IUnknown) && ppvObj)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	HRESULT hr = ProxyInterface->QueryInterface(riid, ppvObj);

	if (SUCCEEDED(hr))
	{
		genericQueryInterface(riid, ppvObj);
	}

	return hr;
}

ULONG m_IDirect3DTexture::AddRef()
{
	return ProxyInterface->AddRef();
}

ULONG m_IDirect3DTexture::Release()
{
	ULONG x = ProxyInterface->Release();

	if (x == 0)
	{
		delete this;
	}

	return x;
}

HRESULT m_IDirect3DTexture::Initialize(LPDIRECT3DDEVICE a, LPDIRECTDRAWSURFACE b)
{
	if (a)
	{
		a = static_cast<m_IDirect3DDevice *>(a)->GetProxyInterface();
	}
	if (b)
	{
		b = static_cast<m_IDirectDrawSurface *>(b)->GetProxyInterface();
	}

	return ProxyInterface->Initialize(a, b);
}

HRESULT m_IDirect3DTexture::GetHandle(LPDIRECT3DDEVICE a, LPD3DTEXTUREHANDLE b)
{
	if (a)
	{
		a = static_cast<m_IDirect3DDevice *>(a)->GetProxyInterface();
	}

	return ProxyInterface->GetHandle(a, b);
}

HRESULT m_IDirect3DTexture::PaletteChanged(DWORD a, DWORD b)
{
	return ProxyInterface->PaletteChanged(a, b);
}

HRESULT m_IDirect3DTexture::Load(LPDIRECT3DTEXTURE a)
{
	if (a)
	{
		a = static_cast<m_IDirect3DTexture *>(a)->GetProxyInterface();
	}

	return ProxyInterface->Load(a);
}

HRESULT m_IDirect3DTexture::Unload()
{
	return ProxyInterface->Unload();
}
