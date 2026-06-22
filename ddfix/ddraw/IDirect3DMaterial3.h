#pragma once

namespace ND3D9
{
	typedef struct _D3DMATERIAL9 D3DMATERIAL9;
}

class m_IDirect3DMaterial3 : public dx6::IDirect3DMaterial3, public AddressLookupTableObject
{
private:
	dx6::IDirect3DMaterial3 *ProxyInterface;
	ULONG Refs;
	dx6::D3DMATERIAL m_matDef;
public:
	m_IDirect3DMaterial3(dx6::IDirect3DMaterial3 *aOriginal, void *temp) 
		: ProxyInterface(aOriginal)
		, Refs(1)
		, m_matDef({0})
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3DMaterial3()
	{
		ProxyAddressLookupTable.DeleteAddress(this);
	}

	dx6::IDirect3DMaterial3 *GetProxyInterface() { return ProxyInterface; }
	void GetMaterial9(ND3D9::D3DMATERIAL9* mat9) const;

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3DMaterial3 methods ***/
	STDMETHOD(SetMaterial)(THIS_ dx6::LPD3DMATERIAL);
	STDMETHOD(GetMaterial)(THIS_ dx6::LPD3DMATERIAL);
	STDMETHOD(GetHandle)(THIS_ dx6::LPDIRECT3DDEVICE3, dx6::LPD3DMATERIALHANDLE);
};
