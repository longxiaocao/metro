#pragma once

class m_IDirect3DMaterial : public dx6::IDirect3DMaterial, public AddressLookupTableObject
{
private:
	dx6::IDirect3DMaterial *ProxyInterface;

public:
	m_IDirect3DMaterial(dx6::IDirect3DMaterial *aOriginal, void *temp) : ProxyInterface(aOriginal)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3DMaterial()
	{
		ProxyAddressLookupTable.DeleteAddress(this);
	}

	dx6::IDirect3DMaterial *GetProxyInterface() { return ProxyInterface; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3DMaterial methods ***/
	STDMETHOD(Initialize)(THIS_ dx6::LPDIRECT3D);
	STDMETHOD(SetMaterial)(THIS_ dx6::LPD3DMATERIAL);
	STDMETHOD(GetMaterial)(THIS_ dx6::LPD3DMATERIAL);
	STDMETHOD(GetHandle)(THIS_ dx6::LPDIRECT3DDEVICE, LPD3DMATERIALHANDLE);
	STDMETHOD(Reserve)(THIS);
	STDMETHOD(Unreserve)(THIS);
};
