#pragma once

class m_IDirect3DMaterial2 : public dx6::IDirect3DMaterial2, public AddressLookupTableObject
{
private:
	dx6::IDirect3DMaterial2 *ProxyInterface;

public:
	m_IDirect3DMaterial2(dx6::IDirect3DMaterial2 *aOriginal, void *temp) : ProxyInterface(aOriginal)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3DMaterial2()
	{
		ProxyAddressLookupTable.DeleteAddress(this);
	}

	dx6::IDirect3DMaterial2 *GetProxyInterface() { return ProxyInterface; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3DMaterial2 methods ***/
	STDMETHOD(SetMaterial)(THIS_ dx6::LPD3DMATERIAL);
	STDMETHOD(GetMaterial)(THIS_ dx6::LPD3DMATERIAL);
	STDMETHOD(GetHandle)(THIS_ dx6::LPDIRECT3DDEVICE2, dx6::LPD3DMATERIALHANDLE);
};
