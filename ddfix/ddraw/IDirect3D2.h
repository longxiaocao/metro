#pragma once

class m_IDirect3D2 : public dx6::IDirect3D2, public AddressLookupTableObject
{
private:
	dx6::IDirect3D2 *ProxyInterface;

public:
	m_IDirect3D2(dx6::IDirect3D2 *aOriginal, void *temp) : ProxyInterface(aOriginal)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3D2()
	{
		ProxyAddressLookupTable.DeleteAddress(this);
	}

	dx6::IDirect3D2 *GetProxyInterface() { return ProxyInterface; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3D2 methods ***/
	STDMETHOD(EnumDevices)(THIS_ dx6::LPD3DENUMDEVICESCALLBACK, LPVOID);
	STDMETHOD(CreateLight)(THIS_ dx6::LPDIRECT3DLIGHT*, IUnknown*);
	STDMETHOD(CreateMaterial)(THIS_ dx6::LPDIRECT3DMATERIAL2*, IUnknown*);
	STDMETHOD(CreateViewport)(THIS_ dx6::LPDIRECT3DVIEWPORT2*, IUnknown*);
	STDMETHOD(FindDevice)(THIS_ dx6::LPD3DFINDDEVICESEARCH, dx6::LPD3DFINDDEVICERESULT);
	STDMETHOD(CreateDevice)(THIS_ REFCLSID, LPDIRECTDRAWSURFACE, dx6::LPDIRECT3DDEVICE2*);
};
