#pragma once

class m_IDirect3DDevice : public dx6::IDirect3DDevice, public AddressLookupTableObject
{
private:
	dx6::IDirect3DDevice *ProxyInterface;

public:
	m_IDirect3DDevice(dx6::IDirect3DDevice *aOriginal,void *temp) : ProxyInterface(aOriginal)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3DDevice()
	{
		ProxyAddressLookupTable.DeleteAddress(this);
	}

	dx6::IDirect3DDevice *GetProxyInterface() { return ProxyInterface; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3DDevice methods ***/
	STDMETHOD(Initialize)(THIS_ dx6::LPDIRECT3D, LPGUID, dx6::LPD3DDEVICEDESC);
	STDMETHOD(GetCaps)(THIS_ dx6::LPD3DDEVICEDESC, dx6::LPD3DDEVICEDESC);
	STDMETHOD(SwapTextureHandles)(THIS_ dx6::LPDIRECT3DTEXTURE, dx6::LPDIRECT3DTEXTURE);
	STDMETHOD(CreateExecuteBuffer)(THIS_ LPD3DEXECUTEBUFFERDESC, dx6::LPDIRECT3DEXECUTEBUFFER*, IUnknown*);
	STDMETHOD(GetStats)(THIS_ dx6::LPD3DSTATS);
	STDMETHOD(Execute)(THIS_ dx6::LPDIRECT3DEXECUTEBUFFER, dx6::LPDIRECT3DVIEWPORT, DWORD);
	STDMETHOD(AddViewport)(THIS_ dx6::LPDIRECT3DVIEWPORT);
	STDMETHOD(DeleteViewport)(THIS_ dx6::LPDIRECT3DVIEWPORT);
	STDMETHOD(NextViewport)(THIS_ dx6::LPDIRECT3DVIEWPORT, dx6::LPDIRECT3DVIEWPORT*, DWORD);
	STDMETHOD(Pick)(THIS_ dx6::LPDIRECT3DEXECUTEBUFFER, dx6::LPDIRECT3DVIEWPORT, DWORD, LPD3DRECT);
	STDMETHOD(GetPickRecords)(THIS_ LPDWORD, LPD3DPICKRECORD);
	STDMETHOD(EnumTextureFormats)(THIS_ LPD3DENUMTEXTUREFORMATSCALLBACK, LPVOID);
	STDMETHOD(CreateMatrix)(THIS_ LPD3DMATRIXHANDLE);
	STDMETHOD(SetMatrix)(THIS_ D3DMATRIXHANDLE, const dx6::LPD3DMATRIX);
	STDMETHOD(GetMatrix)(THIS_ D3DMATRIXHANDLE, dx6::LPD3DMATRIX);
	STDMETHOD(DeleteMatrix)(THIS_ D3DMATRIXHANDLE);
	STDMETHOD(BeginScene)(THIS);
	STDMETHOD(EndScene)(THIS);
	STDMETHOD(GetDirect3D)(THIS_ dx6::LPDIRECT3D*);
};
