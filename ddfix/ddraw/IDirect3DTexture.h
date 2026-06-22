#pragma once

class m_IDirect3DTexture : public dx6::IDirect3DTexture, public AddressLookupTableObject
{
private:
	dx6::IDirect3DTexture *ProxyInterface;

public:
	m_IDirect3DTexture(dx6::IDirect3DTexture *aOriginal, void *temp) : ProxyInterface(aOriginal)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3DTexture()
	{
		ProxyAddressLookupTable.DeleteAddress(this);
	}

	dx6::IDirect3DTexture *GetProxyInterface() { return ProxyInterface; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3DTexture methods ***/
	STDMETHOD(Initialize)(THIS_ dx6::LPDIRECT3DDEVICE, LPDIRECTDRAWSURFACE);
	STDMETHOD(GetHandle)(THIS_ dx6::LPDIRECT3DDEVICE, LPD3DTEXTUREHANDLE);
	STDMETHOD(PaletteChanged)(THIS_ DWORD, DWORD);
	STDMETHOD(Load)(THIS_ dx6::LPDIRECT3DTEXTURE);
	STDMETHOD(Unload)(THIS);
};
