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

static void GetD3DPrimitiveCount(unsigned vertexCount, dx6::D3DPRIMITIVETYPE type, int& primitiveCount)
{
	switch (type)
	{
	case D3DPT_TRIANGLELIST:
		primitiveCount = vertexCount / 3;
		break;

	case D3DPT_LINELIST:
		primitiveCount = vertexCount / 2;
		break;

	case D3DPT_POINTLIST:
		primitiveCount = vertexCount;
		break;

	case D3DPT_TRIANGLESTRIP:
		primitiveCount = vertexCount - 2;
		break;

	case D3DPT_LINESTRIP:
		primitiveCount = vertexCount - 1;
		break;

	case D3DPT_TRIANGLEFAN:
		primitiveCount = vertexCount - 2;
		break;
	}
}

static void GetMagFilterD9(D3DTEXTUREMAGFILTER d6filter, ND3D9::D3DTEXTUREFILTERTYPE& d9filter)
{
	switch (d6filter)
	{
	case D3DTFG_POINT:
		d9filter = ND3D9::D3DTEXF_POINT;
		break;
	case D3DTFG_LINEAR:
		d9filter = ND3D9::D3DTEXF_LINEAR;
		break;
	case D3DTFG_FLATCUBIC:
		d9filter = ND3D9::D3DTEXF_ANISOTROPIC;
		break;
	case D3DTFG_GAUSSIANCUBIC:
		d9filter = ND3D9::D3DTEXF_GAUSSIANQUAD;
		break;
	case D3DTFG_ANISOTROPIC:
		d9filter = ND3D9::D3DTEXF_ANISOTROPIC;
		break;
	default:
		d9filter = ND3D9::D3DTEXF_NONE;
		break;
	}
}

static void GetMagFilterD6(D3DTEXTUREMAGFILTER& d6filter, ND3D9::D3DTEXTUREFILTERTYPE d9filter)
{
	switch (d9filter)
	{
	case ND3D9::D3DTEXF_POINT:
		d6filter = D3DTFG_POINT;
		break;
	case ND3D9::D3DTEXF_LINEAR:
		d6filter = D3DTFG_LINEAR;
		break;
	case ND3D9::D3DTEXF_GAUSSIANQUAD:
		d6filter = D3DTFG_GAUSSIANCUBIC;
		break;
	case ND3D9::D3DTEXF_ANISOTROPIC:
		d6filter = D3DTFG_ANISOTROPIC;
		break;
	default:
		d6filter = D3DTFG_POINT;
		break;
	}
}

static void GetMinFilterD9(D3DTEXTUREMINFILTER d6filter, ND3D9::D3DTEXTUREFILTERTYPE& d9filter)
{
	switch (d6filter)
	{
	case D3DTFN_POINT:
		d9filter = ND3D9::D3DTEXF_POINT;
		break;
	case D3DTFN_LINEAR:
		d9filter = ND3D9::D3DTEXF_LINEAR;
		break;
	case D3DTFN_ANISOTROPIC:
		d9filter = ND3D9::D3DTEXF_ANISOTROPIC;
		break;
	default:
		d9filter = ND3D9::D3DTEXF_POINT;
		break;
	}
}

static void GetMinFilterD6(D3DTEXTUREMINFILTER& d6filter, ND3D9::D3DTEXTUREFILTERTYPE d9filter)
{
	switch (d9filter)
	{
	case ND3D9::D3DTEXF_POINT:
		d6filter = D3DTFN_POINT;
		break;
	case ND3D9::D3DTEXF_LINEAR:
		d6filter = D3DTFN_LINEAR;
		break;
	case ND3D9::D3DTEXF_ANISOTROPIC:
		d6filter = D3DTFN_ANISOTROPIC;
		break;
	default:
		d6filter = D3DTFN_POINT;
		break;
	}
}

static void GetMipFilterD9(D3DTEXTUREMIPFILTER d6filter, ND3D9::D3DTEXTUREFILTERTYPE& d9filter)
{
	switch (d6filter)
	{
	case D3DTFP_NONE:
		d9filter = ND3D9::D3DTEXF_NONE;
		break;
	case D3DTFP_POINT:
		d9filter = ND3D9::D3DTEXF_POINT;
		break;
	case D3DTFP_LINEAR:
		d9filter = ND3D9::D3DTEXF_LINEAR;
		break;
	default:
		d9filter = ND3D9::D3DTEXF_NONE;
		break;
	}
}

static void GetMipFilterD6(D3DTEXTUREMIPFILTER& d6filter, ND3D9::D3DTEXTUREFILTERTYPE d9filter)
{
	switch (d9filter)
	{
	case ND3D9::D3DTEXF_NONE:
		d6filter = D3DTFP_NONE;
		break;
	case ND3D9::D3DTEXF_POINT:
		d6filter = D3DTFP_POINT;
		break;
	case ND3D9::D3DTEXF_LINEAR:
		d6filter = D3DTFP_LINEAR;
		break;
	default:
		d6filter = D3DTFP_POINT;
		break;
	}
}


/*************************************************************************/
// code is taken from project Wine

static UINT Get_TexCoord_Size_From_FVF(DWORD FVF, int tex_num)
{
	return (((((FVF) >> (16 + (2 * (tex_num)))) + 1) & 0x03) + 1);
}

UINT WINAPI D3DXGetFVFVertexSize(DWORD FVF)
{
	DWORD size = 0;
	UINT i;
	UINT numTextures = (FVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;

	if (FVF & D3DFVF_NORMAL) size += sizeof(dx6::D3DVECTOR);
	if (FVF & D3DFVF_DIFFUSE) size += sizeof(DWORD);
	if (FVF & D3DFVF_SPECULAR) size += sizeof(DWORD);
	if (FVF & D3DFVF_PSIZE) size += sizeof(DWORD);

	switch (FVF & D3DFVF_POSITION_MASK)
	{
	case D3DFVF_XYZ:    size += sizeof(dx6::D3DVECTOR); break;
	case D3DFVF_XYZRHW: size += 4 * sizeof(FLOAT); break;
	case D3DFVF_XYZB1:  size += 4 * sizeof(FLOAT); break;
	case D3DFVF_XYZB2:  size += 5 * sizeof(FLOAT); break;
	case D3DFVF_XYZB3:  size += 6 * sizeof(FLOAT); break;
	case D3DFVF_XYZB4:  size += 7 * sizeof(FLOAT); break;
	case D3DFVF_XYZB5:  size += 8 * sizeof(FLOAT); break;
	case D3DFVF_XYZW:   size += 4 * sizeof(FLOAT); break;
	}

	for (i = 0; i < numTextures; i++)
	{
		size += Get_TexCoord_Size_From_FVF(FVF, i) * sizeof(FLOAT);
	}

	return size;
}

/*************************************************************************/

m_IDirect3DDevice3::~m_IDirect3DDevice3()
{
	ProxyAddressLookupTable.DeleteAddress(this);
	WrapperAddressLookupTable->DeleteWrapper(dx6::IID_IDirect3DDevice3);
	for (auto viewport : m_viewports)
	{
		viewport->Release();
	}
	m_viewports.clear();
}

HRESULT m_IDirect3DDevice3::QueryInterface(REFIID riid, LPVOID * ppvObj)
{
	if ((riid == dx6::IID_IDirect3DDevice3 || riid == IID_IUnknown) && ppvObj)
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

ULONG m_IDirect3DDevice3::AddRef()
{
	return ++Refs;
}

ULONG m_IDirect3DDevice3::Release()
{
	ULONG x = --Refs;

	if (x == 0)
	{
		delete this;
	}

	return x;
}

HRESULT m_IDirect3DDevice3::GetCaps(dx6::LPD3DDEVICEDESC lpD3DHWDevDesc, dx6::LPD3DDEVICEDESC lpD3DHELDevDesc)
{
	auto ddraw = WrapperAddressLookupTable->FindWrapper<m_IDirectDraw>(IID_IDirectDraw);
	dx6::D3DDEVICEDESC desc = *ddraw->GetD3DDevice3Desc();
	*lpD3DHWDevDesc = desc;
	*lpD3DHELDevDesc = desc;
	return DD_OK;
}

HRESULT m_IDirect3DDevice3::GetStats(dx6::LPD3DSTATS a)
{
	return ProxyInterface->GetStats(a);
}

HRESULT m_IDirect3DDevice3::AddViewport(dx6::LPDIRECT3DVIEWPORT3 a)
{
	auto itor = std::find(m_viewports.begin(), m_viewports.end(), a);
	if (itor != m_viewports.end())
	{
		return DDERR_INVALIDPARAMS;
	}
	else
	{
		a->AddRef();
		m_viewports.push_back(static_cast<m_IDirect3DViewport3*>(a));
		return DD_OK;
	}
}

HRESULT m_IDirect3DDevice3::DeleteViewport(dx6::LPDIRECT3DVIEWPORT3 a)
{
	auto itor = std::find(m_viewports.begin(), m_viewports.end(), a);
	if (itor != m_viewports.end())
	{
		(*itor)->Release();
		m_viewports.erase(itor);
		return DD_OK;
	}
	else
	{
		return DDERR_INVALIDPARAMS;
	}
}

HRESULT m_IDirect3DDevice3::NextViewport(dx6::LPDIRECT3DVIEWPORT3 a, dx6::LPDIRECT3DVIEWPORT3 * b, DWORD c)
{
	// make compile pass
// 	if (a)
// 	{
// 		a = static_cast<m_IDirect3DViewport3 *>(a)->GetProxyInterface();
// 	}
// 
// 	HRESULT hr = ProxyInterface->NextViewport(a, b, c);
// 
// 	if (SUCCEEDED(hr))
// 	{
// 		*b = ProxyAddressLookupTable.FindAddress<m_IDirect3DViewport3>(*b);
// 	}
// 
// 	return hr;
	return DD_OK;
}

HRESULT m_IDirect3DDevice3::EnumTextureFormats(dx6::LPD3DENUMPIXELFORMATSCALLBACK lpd3dEnumPixelProc, LPVOID lpArg)
{
	extern int g_numtexformats;
	extern const DDPIXELFORMAT* g_texformats;

	HRESULT result;
	DDPIXELFORMAT fmt;
	for (int i = 0; i < g_numtexformats; i++)
	{
		if (i == 7) continue;
		if (g_texformats[i].dwFlags & DDPF_ZBUFFER) continue;
		if (g_texformats[i].dwFlags & DDPF_PALETTEINDEXED8) continue;
		memcpy(&fmt, &g_texformats[i], sizeof(DDPIXELFORMAT));
		result = lpd3dEnumPixelProc(&fmt, lpArg);
		if (result != D3DENUMRET_OK) return DD_OK;
	}
	return DD_OK;
}

HRESULT m_IDirect3DDevice3::BeginScene()
{
	return ND3D9::D3D9Context::Instance()->GetDevice()->BeginScene();
}

HRESULT m_IDirect3DDevice3::EndScene()
{
	//ND3D9::D3D9Context::Instance()->GetDevice()->SetRenderState(ND3D9::D3DRS_FILLMODE, ND3D9::D3DFILL_WIREFRAME);
	return ND3D9::D3D9Context::Instance()->GetDevice()->EndScene();
}

HRESULT m_IDirect3DDevice3::GetDirect3D(dx6::LPDIRECT3D3 * a)
{
	*a = WrapperAddressLookupTable->FindWrapper<m_IDirect3D3>(dx6::IID_IDirect3D3);
	return D3D_OK;
}

HRESULT m_IDirect3DDevice3::SetCurrentViewport(dx6::LPDIRECT3DVIEWPORT3 a)
{
	auto vp = static_cast<m_IDirect3DViewport3*>(a)->GetViewport9();
	return ND3D9::D3D9Context::Instance()->GetDevice()->SetViewport(vp);
	
}

HRESULT m_IDirect3DDevice3::GetCurrentViewport(dx6::LPDIRECT3DVIEWPORT3 * a)
{
	// make compile pass
// 	HRESULT hr = ProxyInterface->GetCurrentViewport(a);
// 
// 	if (SUCCEEDED(hr))
// 	{
// 		*a = ProxyAddressLookupTable.FindAddress<m_IDirect3DViewport3>(*a);
// 	}
// 
// 	return hr;
	return DD_OK;
}

HRESULT m_IDirect3DDevice3::SetRenderTarget(LPDIRECTDRAWSURFACE4 a, DWORD b)
{
	assert(a);
	m_currentRenderTarget = static_cast<m_IDirectDrawSurface4*>(a);
	return ND3D9::D3D9Context::Instance()->GetDevice()->SetRenderTarget(0, m_currentRenderTarget->GetSurface9());
}

HRESULT m_IDirect3DDevice3::GetRenderTarget(LPDIRECTDRAWSURFACE4 * a)
{
	*a = m_currentRenderTarget;
	return D3D_OK;
}

HRESULT m_IDirect3DDevice3::Begin(dx6::D3DPRIMITIVETYPE a, DWORD b, DWORD c)
{
	// Phase 2.5: 立即模式 (Begin) 不再透传到 ProxyInterface（nullptr）。
	// 设计：把 DX6 立即模式转成 D3D9 DrawPrimitiveUP 调用。
	//   b = vertex format descriptor (D3DVERTEXTYPE 枚举)
	//   c = flags (D3DDP_*)：当前 D3D9 路径只识别 D3DDP_DONOTLIGHT，其余忽略。
	// Vertex() 累计顶点数据到 m_immediateVB，End() 一次性 DrawPrimitiveUP 提交。
	// 注意：DX6 Begin 接口的 vertex format 参数语义是 D3DVERTEXTYPE（枚举：TLVertex/LVertex/...)，
	// 但实际项目里 DX6 调用方都把 c 当 D3DFVF（float mask）。这里按 FVF 解析。
	if (m_immediateModeActive)
	{
		// 嵌套 Begin：丢弃当前批次，状态机重置。保守策略，避免脏数据污染下一批。
		m_immediateVB.clear();
	}

	m_immediateModeActive = true;
	m_immediatePrimType = a;
	m_immediateFVF = b;            // 实际是 D3DFVF
	m_immediateVertexCount = 0;
	m_immediateStride = D3DXGetFVFVertexSize(b);
	if (m_immediateStride == 0)
	{
		// fallback：用 dx6::D3DVERTEX (30 byte) 防 0 除 / 越界
		m_immediateStride = sizeof(dx6::D3DVERTEX);
	}
	m_immediateVB.clear();
	m_immediateVB.reserve(64 * m_immediateStride); // 预留 64 顶点容量，减少 realloc

	if (c & D3DDP_DONOTLIGHT)
	{
		auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();
		device9->SetRenderState(ND3D9::D3DRS_LIGHTING, FALSE);
		device9->SetRenderState(ND3D9::D3DRS_AMBIENT, 0x00ffffff);
	}

	return D3D_OK;
}

HRESULT m_IDirect3DDevice3::BeginIndexed(dx6::D3DPRIMITIVETYPE a, DWORD b, LPVOID c, DWORD d, DWORD e)
{
	// Phase 2.5: 索引立即模式暂不实现。
	// dx6::IDirect3DDevice3::BeginIndexed 是带索引的立即模式，需要同时缓存 index buffer。
	// 流星蝴蝶剑不会触发该路径（DX6 游戏中也很少用），保留 nullptr ProxyInterface → 段错误的潜在风险。
	// 临时实现：调用 Begin() 走非索引路径，Index() 暂存，End() 走 DrawIndexedPrimitiveUP。
	// 这里先 return DDERR_GENERIC，让调用方显式知道不支持；后续按需扩展。
	logf("m_IDirect3DDevice3::BeginIndexed not implemented yet, returning DDERR_GENERIC");
	return DDERR_GENERIC;
}

HRESULT m_IDirect3DDevice3::Vertex(LPVOID a)
{
	// Phase 2.5: 立即模式 Vertex，把顶点数据按 stride 追加到 m_immediateVB。
	if (!m_immediateModeActive)
	{
		// 没有 Begin 就调 Vertex：当作隐式 Begin(TRIANGLELIST) 处理（参考 ddraw compat layer 行为）。
		Begin(D3DPT_TRIANGLELIST, D3DFVF_VERTEX, 0);
	}
	if (!a)
	{
		return DDERR_INVALIDPARAMS;
	}

	size_t oldSize = m_immediateVB.size();
	m_immediateVB.resize(oldSize + m_immediateStride);
	memcpy(&m_immediateVB[oldSize], a, m_immediateStride);
	m_immediateVertexCount += 1;
	return D3D_OK;
}

HRESULT m_IDirect3DDevice3::Index(WORD a)
{
	// Phase 2.5: 索引模式当前未启用，调用应视为 no-op（与 BeginIndexed 配套）。
	// 调用方若在 BeginIndexed 之前调 Index，是非法时序，但 dx6::IDirect3DDevice3 文档允许这种"立即索引化"模式。
	// 这里返 DD_OK 不报错（DX6 文档语义是 no-op）。
	return DD_OK;
}

HRESULT m_IDirect3DDevice3::End(DWORD a)
{
	// Phase 2.5: 立即模式 End，把累计的 m_immediateVB 一次性 DrawPrimitiveUP 提交。
	HRESULT hr = DDERR_GENERIC;
	if (!m_immediateModeActive)
	{
		// 非法调用：没 Begin 就 End
		return DDERR_INVALIDPARAMS;
	}
	m_immediateModeActive = false;

	if (m_immediateVertexCount == 0 || m_immediateVB.empty())
	{
		// 0 顶点：直接返 OK（DX6 语义是 no-op）
		return D3D_OK;
	}

	int primitiveCount = 0;
	GetD3DPrimitiveCount(m_immediateVertexCount, m_immediatePrimType, primitiveCount);
	if (primitiveCount <= 0)
	{
		// 顶点数不足以构成本类型（例：仅 1 顶点 + POINTLIST 之外）
		return D3D_OK;
	}

	auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();
	if (a & D3DDP_DONOTLIGHT)
	{
		device9->SetRenderState(ND3D9::D3DRS_LIGHTING, FALSE);
		device9->SetRenderState(ND3D9::D3DRS_AMBIENT, 0x00ffffff);
	}
	device9->LightEnable(0, false);
	hr = device9->SetFVF(m_immediateFVF);
	hr = device9->DrawPrimitiveUP(
		(ND3D9::D3DPRIMITIVETYPE)m_immediatePrimType,
		primitiveCount,
		m_immediateVB.data(),
		m_immediateStride);

	// 释放本批缓冲，让下一批 Begin 干净起步。
	m_immediateVB.clear();
	m_immediateVB.shrink_to_fit();
	m_immediateVertexCount = 0;
	return hr;
}

HRESULT m_IDirect3DDevice3::GetRenderState(dx6::D3DRENDERSTATETYPE a, LPDWORD b)
{
	HRESULT hr = DD_OK;
	*b = 0;
	switch (a)
	{
	case D3DRENDERSTATE_ANTIALIAS:
		break;
	case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
		break;
	case D3DRENDERSTATE_LINEPATTERN:
		break;
	case D3DRENDERSTATE_ZVISIBLE:
		break;
	case D3DRENDERSTATE_STIPPLEDALPHA:
		break;
	case D3DRENDERSTATE_EDGEANTIALIAS:
		break;
	case D3DRENDERSTATE_COLORKEYENABLE:
		*b = m_colorKeyEnabled;
		break;
	case D3DRENDERSTATE_ZBIAS:
		break;
	default:
		hr = ND3D9::D3D9Context::Instance()->GetDevice()->GetRenderState((ND3D9::D3DRENDERSTATETYPE)a, b);
		break;
	}
	return hr;
}

HRESULT m_IDirect3DDevice3::SetRenderState(dx6::D3DRENDERSTATETYPE a, DWORD b)
{
	HRESULT hr = DD_OK;
	switch (a)
	{
	case D3DRENDERSTATE_ANTIALIAS:
		break;
	case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
		break;
	case D3DRENDERSTATE_LINEPATTERN:
		break;
	case D3DRENDERSTATE_ZVISIBLE:
		break;
	case D3DRENDERSTATE_STIPPLEDALPHA:
		break;
	case D3DRENDERSTATE_EDGEANTIALIAS:
		break;
	case D3DRENDERSTATE_COLORKEYENABLE:
		m_colorKeyEnabled = b;
		if (m_colorKeyEnabled)
			ND3D9::D3D9Context::Instance()->GetDevice()->SetRenderState((ND3D9::D3DRS_ALPHATESTENABLE), TRUE);
		break;
	case D3DRENDERSTATE_ZBIAS:
		break;
	case D3DRENDERSTATE_TEXTUREMAPBLEND:
	{
		ND3D9::D3DTEXTUREOP opD9 = ND3D9::D3DTOP_DISABLE;
		switch ((dx6::D3DTEXTUREBLEND)b)
		{
		case D3DTBLEND_DECAL:
			assert(false);
			break;
		case D3DTBLEND_MODULATE:
			opD9 = ND3D9::D3DTOP_MODULATE;
			break;
		case D3DTBLEND_DECALALPHA:
			assert(false);
			break;
		case D3DTBLEND_MODULATEALPHA:
			opD9 = ND3D9::D3DTOP_MODULATEALPHA_ADDCOLOR;
			break;
		case D3DTBLEND_DECALMASK:
			assert(false);
			break;
		case D3DTBLEND_MODULATEMASK:
			assert(false);
			break;
		case D3DTBLEND_COPY:
			assert(false);
			break;
		case D3DTBLEND_ADD:
			opD9 = ND3D9::D3DTOP_ADD;
			assert(false);
			break;
		default:
			assert(false);
			break;
		}
		ND3D9::D3D9Context::Instance()->GetDevice()->SetTextureStageState(0, ND3D9::D3DTSS_COLOROP, opD9);
		ND3D9::D3D9Context::Instance()->GetDevice()->SetTextureStageState(0, ND3D9::D3DTSS_ALPHAOP, opD9);
		break;
	}
// 	case D3DRENDERSTATE_FILLMODE:
// 		b = D3DFILL_WIREFRAME;
// 		hr = ND3D9::D3D9Context::Instance()->GetDevice()->SetRenderState((ND3D9::D3DRENDERSTATETYPE)a, b);
// 		break;
	default:
		hr = ND3D9::D3D9Context::Instance()->GetDevice()->SetRenderState((ND3D9::D3DRENDERSTATETYPE)a, b);
		break;
	}
	return hr;
}

HRESULT m_IDirect3DDevice3::GetLightState(dx6::D3DLIGHTSTATETYPE a, LPDWORD b)
{
	return ProxyInterface->GetLightState(a, b);
}

HRESULT m_IDirect3DDevice3::SetLightState(dx6::D3DLIGHTSTATETYPE a, DWORD b)
{
	HRESULT hr = DDERR_GENERIC;
	auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();
	switch (a)
	{
	case D3DLIGHTSTATE_MATERIAL:
	{
		auto mat3 = reinterpret_cast<m_IDirect3DMaterial3*>(b);
		ND3D9::D3DMATERIAL9 mat9;
		mat3->GetMaterial9(&mat9);
		hr = device9->SetMaterial(&mat9);
		break;
	}
	case D3DLIGHTSTATE_AMBIENT:
		m_lightAmbient.r = RGBA_GETRED(b) / 255.0f;
		m_lightAmbient.g = RGBA_GETGREEN(b) / 255.0f;
		m_lightAmbient.b = RGBA_GETBLUE(b) / 255.0f;
		m_lightAmbient.a = RGBA_GETALPHA(b) / 255.0f;
		break;
	case D3DLIGHTSTATE_COLORMODEL:
		// I don't know how to convert.
		assert(false);
		break;
	case D3DLIGHTSTATE_FOGMODE:
		hr = device9->SetRenderState(ND3D9::D3DRS_FOGTABLEMODE, b);
		hr = device9->SetRenderState(ND3D9::D3DRS_FOGVERTEXMODE, b);
		break;
	case D3DLIGHTSTATE_FOGSTART:
		hr = device9->SetRenderState(ND3D9::D3DRS_FOGSTART, b);
		break;
	case D3DLIGHTSTATE_FOGEND:
		hr = device9->SetRenderState(ND3D9::D3DRS_FOGEND, b);
		break;
	case D3DLIGHTSTATE_FOGDENSITY:
		hr = device9->SetRenderState(ND3D9::D3DRS_FOGDENSITY, b);
		break;
	case D3DLIGHTSTATE_COLORVERTEX:
		hr = device9->SetRenderState(ND3D9::D3DRS_COLORVERTEX, b);
		break;
	default:
		assert(false);
		break;
	}
	return hr;
}

HRESULT m_IDirect3DDevice3::SetTransform(dx6::D3DTRANSFORMSTATETYPE tsType, dx6::LPD3DMATRIX matrix)
{
	HRESULT hr = DDERR_GENERIC;
	auto matrix9 = (ND3D9::D3DXMATRIX*)matrix;
	auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();
	switch (tsType)
	{
	case D3DTRANSFORMSTATE_WORLD:
		hr = device9->SetTransform((ND3D9::D3DTRANSFORMSTATETYPE)D3DTS_WORLD, matrix9);
		break;
	case D3DTRANSFORMSTATE_VIEW:
		hr = device9->SetTransform(ND3D9::D3DTS_VIEW, matrix9);
		break;
	case D3DTRANSFORMSTATE_PROJECTION:
		hr = device9->SetTransform(ND3D9::D3DTS_PROJECTION, matrix9);
		break;
	case D3DTRANSFORMSTATE_WORLD1:
		hr = device9->SetTransform((ND3D9::D3DTRANSFORMSTATETYPE)D3DTS_WORLD1, matrix9);
		break;
	case D3DTRANSFORMSTATE_WORLD2:
		hr = device9->SetTransform((ND3D9::D3DTRANSFORMSTATETYPE)D3DTS_WORLD2, matrix9);
		break;
	case D3DTRANSFORMSTATE_WORLD3:
		hr = device9->SetTransform((ND3D9::D3DTRANSFORMSTATETYPE)D3DTS_WORLD3, matrix9);
		break;
	default:
		hr = device9->SetTransform((ND3D9::D3DTRANSFORMSTATETYPE)tsType, matrix9);
		break;
	}
	return hr;
}

HRESULT m_IDirect3DDevice3::GetTransform(dx6::D3DTRANSFORMSTATETYPE tsType, dx6::LPD3DMATRIX matrix)
{
	HRESULT hr = DDERR_GENERIC;
	auto matrix9 = (ND3D9::D3DXMATRIX*)matrix;
	auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();
	switch (tsType)
	{
	case D3DTRANSFORMSTATE_WORLD:
		hr = device9->GetTransform((ND3D9::D3DTRANSFORMSTATETYPE)D3DTS_WORLD, matrix9);
		break;
	case D3DTRANSFORMSTATE_VIEW:
		hr = device9->GetTransform(ND3D9::D3DTS_VIEW, matrix9);
		break;
	case D3DTRANSFORMSTATE_PROJECTION:
		hr = device9->GetTransform(ND3D9::D3DTS_PROJECTION, matrix9);
		break;
	case D3DTRANSFORMSTATE_WORLD1:
		hr = device9->GetTransform((ND3D9::D3DTRANSFORMSTATETYPE)D3DTS_WORLD1, matrix9);
		break;
	case D3DTRANSFORMSTATE_WORLD2:
		hr = device9->GetTransform((ND3D9::D3DTRANSFORMSTATETYPE)D3DTS_WORLD2, matrix9);
		break;
	case D3DTRANSFORMSTATE_WORLD3:
		hr = device9->GetTransform((ND3D9::D3DTRANSFORMSTATETYPE)D3DTS_WORLD3, matrix9);
		break;
	default:
		hr = device9->GetTransform((ND3D9::D3DTRANSFORMSTATETYPE)tsType, matrix9);
		break;
	}
	return hr;
}

HRESULT m_IDirect3DDevice3::MultiplyTransform(dx6::D3DTRANSFORMSTATETYPE tsType, dx6::LPD3DMATRIX matrix)
{
	HRESULT hr = DDERR_GENERIC;
	auto matrix9 = (ND3D9::D3DXMATRIX*)matrix;
	auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();
	switch (tsType)
	{
	case D3DTRANSFORMSTATE_WORLD:
		hr = device9->MultiplyTransform((ND3D9::D3DTRANSFORMSTATETYPE)D3DTS_WORLD, matrix9);
		break;
	case D3DTRANSFORMSTATE_VIEW:
		hr = device9->MultiplyTransform(ND3D9::D3DTS_VIEW, matrix9);
		break;
	case D3DTRANSFORMSTATE_PROJECTION:
		hr = device9->MultiplyTransform(ND3D9::D3DTS_PROJECTION, matrix9);
		break;
	case D3DTRANSFORMSTATE_WORLD1:
		hr = device9->MultiplyTransform((ND3D9::D3DTRANSFORMSTATETYPE)D3DTS_WORLD1, matrix9);
		break;
	case D3DTRANSFORMSTATE_WORLD2:
		hr = device9->MultiplyTransform((ND3D9::D3DTRANSFORMSTATETYPE)D3DTS_WORLD2, matrix9);
		break;
	case D3DTRANSFORMSTATE_WORLD3:
		hr = device9->MultiplyTransform((ND3D9::D3DTRANSFORMSTATETYPE)D3DTS_WORLD3, matrix9);
		break;
	default:
		hr = device9->MultiplyTransform((ND3D9::D3DTRANSFORMSTATETYPE)tsType, matrix9);
		break;
	}
	return hr;
}

HRESULT m_IDirect3DDevice3::DrawPrimitive(dx6::D3DPRIMITIVETYPE dptPrimitiveType, DWORD dwVertexTypeDesc, LPVOID lpVertices,
	DWORD dwVertexCount, DWORD dwFlags)
{
	HRESULT hr = DDERR_GENERIC;
	auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();
	if (dwFlags & D3DDP_DONOTLIGHT)
	{
		device9->SetRenderState(ND3D9::D3DRS_LIGHTING, FALSE);
		device9->SetRenderState(ND3D9::D3DRS_AMBIENT, 0x00ffffff);
	}
	else
	{
		//device9->SetRenderState(ND3D9::D3DRS_LIGHTING, TRUE);
	}

	int primitiveCount = 0;
	int vertexStreamZeroStride = D3DXGetFVFVertexSize(dwVertexTypeDesc);
	GetD3DPrimitiveCount(dwVertexCount, dptPrimitiveType, primitiveCount);
	device9->LightEnable(0, false);
	hr = device9->SetFVF(dwVertexTypeDesc);
	hr = device9->DrawPrimitiveUP((ND3D9::D3DPRIMITIVETYPE)dptPrimitiveType, primitiveCount, lpVertices, vertexStreamZeroStride);
	return hr;
}

HRESULT m_IDirect3DDevice3::DrawIndexedPrimitive(dx6::D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc,
	LPVOID lpvVertices, DWORD dwVertexCount, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags)
{
	HRESULT hr = DDERR_GENERIC;
	if (dwVertexCount == 0 || dwIndexCount == 0)
	{
		hr = DDERR_INVALIDPARAMS;
		return hr;
	}

	auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();
	if (dwFlags & D3DDP_DONOTLIGHT)
	{
		device9->SetRenderState(ND3D9::D3DRS_LIGHTING, FALSE);
		device9->SetRenderState(ND3D9::D3DRS_AMBIENT, 0x00ffffff);
	}
	else
	{
		//device9->SetRenderState(ND3D9::D3DRS_LIGHTING, TRUE);
	}
	int primitiveCount = 0;
	int vertexStreamZeroStride = D3DXGetFVFVertexSize(dwVertexTypeDesc);
	GetD3DPrimitiveCount(dwIndexCount, d3dptPrimitiveType, primitiveCount);
	hr = device9->SetFVF(dwVertexTypeDesc);
	hr = device9->DrawIndexedPrimitiveUP((ND3D9::D3DPRIMITIVETYPE)d3dptPrimitiveType,0, dwVertexCount,  primitiveCount, lpwIndices, ND3D9::D3DFMT_INDEX16, lpvVertices, vertexStreamZeroStride);
	return hr;
}

HRESULT m_IDirect3DDevice3::SetClipStatus(dx6::LPD3DCLIPSTATUS a)
{
	return ProxyInterface->SetClipStatus(a);
}

HRESULT m_IDirect3DDevice3::GetClipStatus(dx6::LPD3DCLIPSTATUS a)
{
	return ProxyInterface->GetClipStatus(a);
}

HRESULT m_IDirect3DDevice3::DrawPrimitiveStrided(dx6::D3DPRIMITIVETYPE a, DWORD b, dx6::LPD3DDRAWPRIMITIVESTRIDEDDATA c, DWORD d, DWORD e)
{
	return ProxyInterface->DrawPrimitiveStrided(a, b, c, d, e);
}

HRESULT m_IDirect3DDevice3::DrawIndexedPrimitiveStrided(dx6::D3DPRIMITIVETYPE a, DWORD b, dx6::LPD3DDRAWPRIMITIVESTRIDEDDATA c, DWORD d, LPWORD e, DWORD f, DWORD g)
{
	return ProxyInterface->DrawIndexedPrimitiveStrided(a, b, c, d, e, f, g);
}

HRESULT m_IDirect3DDevice3::DrawPrimitiveVB(dx6::D3DPRIMITIVETYPE a, dx6::LPDIRECT3DVERTEXBUFFER b, DWORD c, DWORD d, DWORD e)
{
	if (b)
	{
		b = static_cast<m_IDirect3DVertexBuffer *>(b)->GetProxyInterface();
	}

	return ProxyInterface->DrawPrimitiveVB(a, b, c, d, e);
}

HRESULT m_IDirect3DDevice3::DrawIndexedPrimitiveVB(dx6::D3DPRIMITIVETYPE a, dx6::LPDIRECT3DVERTEXBUFFER b, LPWORD c, DWORD d, DWORD e)
{
	if (b)
	{
		b = static_cast<m_IDirect3DVertexBuffer *>(b)->GetProxyInterface();
	}

	return ProxyInterface->DrawIndexedPrimitiveVB(a, b, c, d, e);
}

HRESULT m_IDirect3DDevice3::ComputeSphereVisibility(dx6::LPD3DVECTOR a, dx6::LPD3DVALUE b, DWORD c, DWORD d, LPDWORD e)
{
	return ProxyInterface->ComputeSphereVisibility(a, b, c, d, e);
}

HRESULT m_IDirect3DDevice3::GetTexture(DWORD a, dx6::LPDIRECT3DTEXTURE2 * b)
{
	auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();
	SmartPtr<ND3D9::IDirect3DBaseTexture9> tex9;
	device9->GetTexture(a, &tex9);
	if (!tex9)
	{
		*b = nullptr;
		return DD_OK;
	}
	else
	{
		auto ddraw4 = WrapperAddressLookupTable->FindWrapper<m_IDirectDraw4>(IID_IDirectDraw4);
		auto surface4 = ddraw4->Tex9LookupTable->FindAddressOnly<m_IDirectDrawSurface4>(tex9);
		auto tex2 = surface4->GetTexture2();
		tex2->AddRef();
		*b = tex2;
		return DD_OK;
	}
}

HRESULT m_IDirect3DDevice3::SetTexture(DWORD a, dx6::LPDIRECT3DTEXTURE2 b)
{
	auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();
	if (b)
	{
		auto tex2 = static_cast<m_IDirect3DTexture2 *>(b);
		auto tex9 = tex2->GetTexture9();
		device9->SetTexture(a, tex9);
		return DD_OK;
	}
	else
	{
		ND3D9::IDirect3DBaseTexture9* tex9 = nullptr;
		device9->GetTexture(a, &tex9);
		if (tex9)
		{
			auto ddraw4 = WrapperAddressLookupTable->FindWrapper<m_IDirectDraw4>(IID_IDirectDraw4);
			auto surface4 = ddraw4->Tex9LookupTable->FindAddressOnly<m_IDirectDrawSurface4>(tex9);
			tex9->Release();
			auto tex2 = surface4->GetTexture2();
		}
		
		device9->SetTexture(a, nullptr);
		return DD_OK;
	}
}

HRESULT m_IDirect3DDevice3::GetTextureStageState(DWORD dwStage, dx6::D3DTEXTURESTAGESTATETYPE dwState, DWORD* pValue)
{
	HRESULT hr = DDERR_GENERIC;
	auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();
	switch (dwState)
	{
	case D3DTSS_ADDRESS:
		hr = device9->GetSamplerState(0, ND3D9::D3DSAMP_ADDRESSU, pValue);
		hr = device9->GetSamplerState(0, ND3D9::D3DSAMP_ADDRESSV, pValue);
		break;
	case D3DTSS_ADDRESSU:
		hr = device9->GetSamplerState(0, ND3D9::D3DSAMP_ADDRESSU, pValue);
		break;
	case D3DTSS_ADDRESSV:
		device9->GetSamplerState(0, ND3D9::D3DSAMP_ADDRESSV, pValue);
		break;
	case D3DTSS_BORDERCOLOR:
		hr = device9->GetSamplerState(0, ND3D9::D3DSAMP_BORDERCOLOR, pValue);
		break;
	case D3DTSS_MAGFILTER:
	{
		ND3D9::D3DTEXTUREFILTERTYPE d9Value;
		hr = device9->GetSamplerState(0, ND3D9::D3DSAMP_MAGFILTER, (DWORD*)&d9Value);
		GetMagFilterD6((D3DTEXTUREMAGFILTER&)*pValue, d9Value);
		break;
	}
	case D3DTSS_MINFILTER:
	{
		ND3D9::D3DTEXTUREFILTERTYPE d9Value;
		hr = device9->GetSamplerState(0, ND3D9::D3DSAMP_MINFILTER, (DWORD*)&d9Value);
		GetMinFilterD6((D3DTEXTUREMINFILTER&)*pValue, d9Value);
		
		break;
	}
	case D3DTSS_MIPFILTER:
	{
		ND3D9::D3DTEXTUREFILTERTYPE d9Value;
		hr = device9->GetSamplerState(0, ND3D9::D3DSAMP_MIPFILTER, (DWORD*)&d9Value);
		GetMipFilterD6((D3DTEXTUREMIPFILTER&)*pValue, d9Value);
		
		break;
	}
	case D3DTSS_MIPMAPLODBIAS:
		hr = device9->GetSamplerState(0, ND3D9::D3DSAMP_MIPMAPLODBIAS, pValue);
		break;
	case D3DTSS_MAXMIPLEVEL:
		hr = device9->GetSamplerState(0, ND3D9::D3DSAMP_MAXMIPLEVEL, pValue);
		break;
	case D3DTSS_MAXANISOTROPY:
		hr = device9->GetSamplerState(0, ND3D9::D3DSAMP_MAXANISOTROPY, pValue);
		break;
	default:
		hr = device9->GetTextureStageState(dwStage, (ND3D9::D3DTEXTURESTAGESTATETYPE)dwState, pValue);
		break;
	}

	return hr;
}

HRESULT m_IDirect3DDevice3::SetTextureStageState(DWORD dwStage, dx6::D3DTEXTURESTAGESTATETYPE dwState, DWORD dwValue)
{
	HRESULT hr = DDERR_GENERIC;
	auto device9 = ND3D9::D3D9Context::Instance()->GetDevice();
	switch (dwState)
	{
	case D3DTSS_ADDRESS:
		hr = device9->SetSamplerState(0, ND3D9::D3DSAMP_ADDRESSU, dwValue);
		hr = device9->SetSamplerState(0, ND3D9::D3DSAMP_ADDRESSV, dwValue);
		break;
	case D3DTSS_ADDRESSU:
		hr = device9->SetSamplerState(0, ND3D9::D3DSAMP_ADDRESSU, dwValue);
		break;
	case D3DTSS_ADDRESSV:
		device9->SetSamplerState(0, ND3D9::D3DSAMP_ADDRESSV, dwValue);
		break;
	case D3DTSS_BORDERCOLOR:
		hr = device9->SetSamplerState(0, ND3D9::D3DSAMP_BORDERCOLOR, dwValue);
		break;
	case D3DTSS_MAGFILTER:
	{
		ND3D9::D3DTEXTUREFILTERTYPE newValue;
		GetMagFilterD9((D3DTEXTUREMAGFILTER)dwValue, newValue);
		hr = device9->SetSamplerState(0, ND3D9::D3DSAMP_MAGFILTER, newValue);
		break;
	}
	case D3DTSS_MINFILTER:
	{
		ND3D9::D3DTEXTUREFILTERTYPE newValue;
		GetMinFilterD9((D3DTEXTUREMINFILTER)dwValue, newValue);
		hr = device9->SetSamplerState(0, ND3D9::D3DSAMP_MINFILTER, newValue);
		break;
	}
	case D3DTSS_MIPFILTER:
	{
		ND3D9::D3DTEXTUREFILTERTYPE newValue;
		GetMipFilterD9((D3DTEXTUREMIPFILTER)dwValue, newValue);
		hr = device9->SetSamplerState(0, ND3D9::D3DSAMP_MIPFILTER, newValue);
		break;
	}
	case D3DTSS_MIPMAPLODBIAS:
		hr = device9->SetSamplerState(0, ND3D9::D3DSAMP_MIPMAPLODBIAS, dwValue);
		break;
	case D3DTSS_MAXMIPLEVEL:
		hr = device9->SetSamplerState(0, ND3D9::D3DSAMP_MAXMIPLEVEL, dwValue);
		break;
	case D3DTSS_MAXANISOTROPY:
		hr = device9->SetSamplerState(0, ND3D9::D3DSAMP_MAXANISOTROPY, dwValue);
		break;
	default:
		hr = device9->SetTextureStageState(dwStage, (ND3D9::D3DTEXTURESTAGESTATETYPE)dwState, dwValue);
		break;
	}

	return hr;
}

HRESULT m_IDirect3DDevice3::ValidateDevice(LPDWORD a)
{
	return ProxyInterface->ValidateDevice(a);
}
