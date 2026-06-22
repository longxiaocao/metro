#pragma once

class m_IDirect3DVertexBuffer : public dx6::IDirect3DVertexBuffer, public AddressLookupTableObject
{
private:
	dx6::IDirect3DVertexBuffer *ProxyInterface;

public:
	m_IDirect3DVertexBuffer(dx6::IDirect3DVertexBuffer *aOriginal, void *temp) : ProxyInterface(aOriginal)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3DVertexBuffer()
	{
		ProxyAddressLookupTable.DeleteAddress(this);
	}

	dx6::IDirect3DVertexBuffer *GetProxyInterface() { return ProxyInterface; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3DVertexBuffer methods ***/
	STDMETHOD(Lock)(THIS_ DWORD, LPVOID*, LPDWORD);
	STDMETHOD(Unlock)(THIS);
	STDMETHOD(ProcessVertices)(THIS_ DWORD, DWORD, DWORD, dx6::LPDIRECT3DVERTEXBUFFER, DWORD, dx6::LPDIRECT3DDEVICE3, DWORD);
	STDMETHOD(GetVertexBufferDesc)(THIS_ dx6::LPD3DVERTEXBUFFERDESC);
	STDMETHOD(Optimize)(THIS_ dx6::LPDIRECT3DDEVICE3, DWORD);
};
