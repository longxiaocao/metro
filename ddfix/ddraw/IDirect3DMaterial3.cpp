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


void m_IDirect3DMaterial3::GetMaterial9(D3DMATERIAL9* mat9) const
{
	// Phase 8.25.12: dx6::D3DCOLORVALUE 与全局 D3DCOLORVALUE 内存布局完全相同但
	//   是两个 C++ 类型 (Wine typedef struct _D3DCOLORVALUE vs d3d9types.h typedef
	//   struct _D3DCOLORVALUE), 不能直接赋值 (C2679)。改用逐字段赋值, 安全可读。
	mat9->Diffuse.r = m_matDef.diffuse.r;
	mat9->Diffuse.g = m_matDef.diffuse.g;
	mat9->Diffuse.b = m_matDef.diffuse.b;
	mat9->Diffuse.a = m_matDef.diffuse.a;
	mat9->Ambient.r = m_matDef.ambient.r;
	mat9->Ambient.g = m_matDef.ambient.g;
	mat9->Ambient.b = m_matDef.ambient.b;
	mat9->Ambient.a = m_matDef.ambient.a;
	mat9->Specular.r = m_matDef.specular.r;
	mat9->Specular.g = m_matDef.specular.g;
	mat9->Specular.b = m_matDef.specular.b;
	mat9->Specular.a = m_matDef.specular.a;
	mat9->Emissive.r = m_matDef.emissive.r;
	mat9->Emissive.g = m_matDef.emissive.g;
	mat9->Emissive.b = m_matDef.emissive.b;
	mat9->Emissive.a = m_matDef.emissive.a;
	// Power 是 D3DVALUE (float), 与 dx6::D3DVALUE 同类型 (都来自 wine typedef),
	//   可以直接赋值。
	mat9->Power = m_matDef.power;
}

HRESULT m_IDirect3DMaterial3::QueryInterface(REFIID riid, LPVOID * ppvObj)
{
	if ((riid == dx6::IID_IDirect3DMaterial3 || riid == IID_IUnknown) && ppvObj)
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

ULONG m_IDirect3DMaterial3::AddRef()
{
	return ++Refs;
}

ULONG m_IDirect3DMaterial3::Release()
{
	ULONG x = --Refs;

	if (x == 0)
	{
		delete this;
	}

	return x;
}

HRESULT m_IDirect3DMaterial3::SetMaterial(dx6::LPD3DMATERIAL a)
{
	if (a)
	{
		m_matDef = *a;
		
		//return ND3D9::D3D9Context::Instance()->GetDevice()->SetMaterial(&matDef9);
		return DD_OK;
	}
	else
	{
		return DDERR_INVALIDPARAMS;
	}
}

HRESULT m_IDirect3DMaterial3::GetMaterial(dx6::LPD3DMATERIAL a)
{
	*a = m_matDef;
	return DD_OK;
}

HRESULT m_IDirect3DMaterial3::GetHandle(dx6::LPDIRECT3DDEVICE3 a, dx6::LPD3DMATERIALHANDLE b)
{
	// 存储this指针
	*b = (DWORD)this;
	return DD_OK;
}
