#pragma once

namespace ND3D9
{
	typedef struct _D3DLIGHT9 D3DLIGHT9;
}

class m_IDirect3DLight : public dx6::IDirect3DLight, public AddressLookupTableObject
{
private:
	dx6::IDirect3DLight *ProxyInterface;
	ULONG Refs;
	D3DLIGHT9* m_light9;
public:
	m_IDirect3DLight(dx6::IDirect3DLight *aOriginal, void *temp) 
		: ProxyInterface(aOriginal)
		, Refs(1)
		, m_light9(nullptr)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3DLight();

	dx6::IDirect3DLight *GetProxyInterface() { return ProxyInterface; }
	D3DLIGHT9* GetLight9() const { return m_light9; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3DLight methods ***/
	STDMETHOD(Initialize)(THIS_ dx6::LPDIRECT3D);
	STDMETHOD(SetLight)(THIS_ dx6::LPD3DLIGHT);
	STDMETHOD(GetLight)(THIS_ dx6::LPD3DLIGHT);
};
