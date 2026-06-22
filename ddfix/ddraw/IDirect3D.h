#pragma once

class m_IDirect3D : public dx6::IDirect3D, public AddressLookupTableObject
{
private:
	dx6::IDirect3D *ProxyInterface;

public:
	m_IDirect3D(dx6::IDirect3D *aOriginal, void *temp) : ProxyInterface(aOriginal)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3D()
	{
		ProxyAddressLookupTable.DeleteAddress(this);
	}

	dx6::IDirect3D *GetProxyInterface() { return ProxyInterface; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3D methods ***/
	STDMETHOD(Initialize)(THIS_ REFCLSID);
	STDMETHOD(EnumDevices)(THIS_ dx6::LPD3DENUMDEVICESCALLBACK, LPVOID);
	STDMETHOD(CreateLight)(THIS_ dx6::LPDIRECT3DLIGHT*, IUnknown*);
	STDMETHOD(CreateMaterial)(THIS_ dx6::LPDIRECT3DMATERIAL*, IUnknown*);
	STDMETHOD(CreateViewport)(THIS_ dx6::LPDIRECT3DVIEWPORT*, IUnknown*);
	STDMETHOD(FindDevice)(THIS_ dx6::LPD3DFINDDEVICESEARCH, dx6::LPD3DFINDDEVICERESULT);
};
