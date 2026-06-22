#pragma once

class m_IDirect3D7 : public dx6::IDirect3D7, public AddressLookupTableObject
{
private:
	dx6::IDirect3D7 *ProxyInterface;

public:
	m_IDirect3D7(dx6::IDirect3D7 *aOriginal, void *temp) : ProxyInterface(aOriginal)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3D7()
	{
		ProxyAddressLookupTable.DeleteAddress(this);
	}

	dx6::IDirect3D7 *GetProxyInterface() { return ProxyInterface; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3D7 methods ***/
	STDMETHOD(EnumDevices)(THIS_ LPD3DENUMDEVICESCALLBACK7, LPVOID);
	STDMETHOD(CreateDevice)(THIS_ REFCLSID, LPDIRECTDRAWSURFACE7, dx6::LPDIRECT3DDEVICE7*);
	STDMETHOD(CreateVertexBuffer)(THIS_ dx6::LPD3DVERTEXBUFFERDESC, dx6::LPDIRECT3DVERTEXBUFFER7*, DWORD);
	STDMETHOD(EnumZBufferFormats)(THIS_ REFCLSID, dx6::LPD3DENUMPIXELFORMATSCALLBACK, LPVOID);
	STDMETHOD(EvictManagedTextures)(THIS);
};
