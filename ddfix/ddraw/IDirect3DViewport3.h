#pragma once
#include <array>
namespace ND3D9
{
	typedef struct _D3DVIEWPORT9  D3DVIEWPORT9;
}

class m_IDirect3DViewport3 : public dx6::IDirect3DViewport3, public AddressLookupTableObject
{
private:
	dx6::IDirect3DViewport3 *ProxyInterface;
	ULONG Refs;
	std::shared_ptr<WrapperLookupTable<void>> WrapperAddressLookupTable;
	D3DVIEWPORT9* m_viewport9;
	std::array<dx6::IDirect3DLight*, 8> m_lights;
public:
	m_IDirect3DViewport3(dx6::IDirect3DViewport3 *aOriginal, std::shared_ptr<WrapperLookupTable<void>> wrapperAddressLookupTable)
		: ProxyInterface(aOriginal)
		, Refs(1)
		, WrapperAddressLookupTable(wrapperAddressLookupTable)
		, m_viewport9(nullptr)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
		m_lights.fill(nullptr);
	}
	~m_IDirect3DViewport3();

	dx6::IDirect3DViewport3 *GetProxyInterface() { return ProxyInterface; }
	const D3DVIEWPORT9* GetViewport9() const { return m_viewport9; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3DViewport2 methods ***/
	STDMETHOD(Initialize)(THIS_ dx6::LPDIRECT3D);
	STDMETHOD(GetViewport)(THIS_ dx6::LPD3DVIEWPORT);
	STDMETHOD(SetViewport)(THIS_ dx6::LPD3DVIEWPORT);
	STDMETHOD(TransformVertices)(THIS_ DWORD, dx6::LPD3DTRANSFORMDATA, DWORD, LPDWORD);
	STDMETHOD(LightElements)(THIS_ DWORD, dx6::LPD3DLIGHTDATA);
	STDMETHOD(SetBackground)(THIS_ dx6::D3DMATERIALHANDLE);
	STDMETHOD(GetBackground)(THIS_ dx6::LPD3DMATERIALHANDLE, LPBOOL);
	STDMETHOD(SetBackgroundDepth)(THIS_ LPDIRECTDRAWSURFACE);
	STDMETHOD(GetBackgroundDepth)(THIS_ LPDIRECTDRAWSURFACE*, LPBOOL);
	STDMETHOD(Clear)(THIS_ DWORD, dx6::LPD3DRECT, DWORD);
	STDMETHOD(AddLight)(THIS_ dx6::LPDIRECT3DLIGHT);
	STDMETHOD(DeleteLight)(THIS_ dx6::LPDIRECT3DLIGHT);
	STDMETHOD(NextLight)(THIS_ dx6::LPDIRECT3DLIGHT, dx6::LPDIRECT3DLIGHT*, DWORD);
	STDMETHOD(GetViewport2)(THIS_ dx6::LPD3DVIEWPORT2);
	STDMETHOD(SetViewport2)(THIS_ dx6::LPD3DVIEWPORT2);
	STDMETHOD(SetBackgroundDepth2)(THIS_ LPDIRECTDRAWSURFACE4);
	STDMETHOD(GetBackgroundDepth2)(THIS_ LPDIRECTDRAWSURFACE4*, LPBOOL);
	STDMETHOD(Clear2)(THIS_ DWORD, dx6::LPD3DRECT, DWORD, dx6::D3DCOLOR, dx6::D3DVALUE, DWORD);
};
