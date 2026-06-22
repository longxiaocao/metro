#pragma once

class m_IDirect3DViewport : public dx6::IDirect3DViewport, public AddressLookupTableObject
{
private:
	dx6::IDirect3DViewport *ProxyInterface;

public:
	m_IDirect3DViewport(dx6::IDirect3DViewport *aOriginal, void *temp) : ProxyInterface(aOriginal)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3DViewport()
	{
		ProxyAddressLookupTable.DeleteAddress(this);
	}

	dx6::IDirect3DViewport *GetProxyInterface() { return ProxyInterface; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3DViewport methods ***/
	STDMETHOD(Initialize)(THIS_ dx6::LPDIRECT3D);
	STDMETHOD(GetViewport)(THIS_ dx6::LPD3DVIEWPORT);
	STDMETHOD(SetViewport)(THIS_ dx6::LPD3DVIEWPORT);
	STDMETHOD(TransformVertices)(THIS_ DWORD, dx6::LPD3DTRANSFORMDATA, DWORD, LPDWORD);
	STDMETHOD(LightElements)(THIS_ DWORD, dx6::LPD3DLIGHTDATA);
	STDMETHOD(SetBackground)(THIS_ D3DMATERIALHANDLE);
	STDMETHOD(GetBackground)(THIS_ LPD3DMATERIALHANDLE, LPBOOL);
	STDMETHOD(SetBackgroundDepth)(THIS_ LPDIRECTDRAWSURFACE);
	STDMETHOD(GetBackgroundDepth)(THIS_ LPDIRECTDRAWSURFACE*, LPBOOL);
	STDMETHOD(Clear)(THIS_ DWORD, LPD3DRECT, DWORD);
	STDMETHOD(AddLight)(THIS_ dx6::LPDIRECT3DLIGHT);
	STDMETHOD(DeleteLight)(THIS_ dx6::LPDIRECT3DLIGHT);
	STDMETHOD(NextLight)(THIS_ dx6::LPDIRECT3DLIGHT, dx6::LPDIRECT3DLIGHT*, DWORD);
};
