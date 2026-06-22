#pragma once

// Phase 8.19: d3d.h 内的 IDirect3D* / LPDIRECT3D* / IID_IDirect3D* 全部进入 dx6:: 命名空间
class m_IDirect3D3 : public dx6::IDirect3D3, public AddressLookupTableObject
{
private:
	dx6::IDirect3D3 *ProxyInterface;
	ULONG Refs;
	std::shared_ptr<WrapperLookupTable<void>> WrapperAddressLookupTable;
public:
	m_IDirect3D3(dx6::IDirect3D3 *aOriginal, std::shared_ptr<WrapperLookupTable<void>> wrapperAddressLookupTable)
		: ProxyInterface(aOriginal)
		, Refs(1)
		, WrapperAddressLookupTable(wrapperAddressLookupTable)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
		WrapperAddressLookupTable->SaveWrapper(this, dx6::IID_IDirect3D3);
	}
	~m_IDirect3D3()
	{
		ProxyAddressLookupTable.DeleteAddress(this);
		WrapperAddressLookupTable->DeleteWrapper(dx6::IID_IDirect3D3);
	}

	dx6::IDirect3D3 *GetProxyInterface() { return ProxyInterface; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** IDirect3D3 methods ***/
	STDMETHOD(EnumDevices)(THIS_ dx6::LPD3DENUMDEVICESCALLBACK, LPVOID);
	STDMETHOD(CreateLight)(THIS_ dx6::LPDIRECT3DLIGHT*, LPUNKNOWN);
	STDMETHOD(CreateMaterial)(THIS_ dx6::LPDIRECT3DMATERIAL3*, LPUNKNOWN);
	STDMETHOD(CreateViewport)(THIS_ dx6::LPDIRECT3DVIEWPORT3*, LPUNKNOWN);
	STDMETHOD(FindDevice)(THIS_ dx6::LPD3DFINDDEVICESEARCH, dx6::LPD3DFINDDEVICERESULT);
	STDMETHOD(CreateDevice)(THIS_ REFCLSID, LPDIRECTDRAWSURFACE4, dx6::LPDIRECT3DDEVICE3*, LPUNKNOWN);
	STDMETHOD(CreateVertexBuffer)(THIS_ dx6::LPD3DVERTEXBUFFERDESC, dx6::LPDIRECT3DVERTEXBUFFER*, DWORD, LPUNKNOWN);
	STDMETHOD(EnumZBufferFormats)(THIS_ REFCLSID, dx6::LPD3DENUMPIXELFORMATSCALLBACK, LPVOID);
	STDMETHOD(EvictManagedTextures)(THIS);
};
