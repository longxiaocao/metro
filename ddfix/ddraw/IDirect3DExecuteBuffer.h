#pragma once

class m_IDirect3DExecuteBuffer : public dx6::IDirect3DExecuteBuffer, public AddressLookupTableObject
{
private:
	dx6::IDirect3DExecuteBuffer *ProxyInterface;

public:
	m_IDirect3DExecuteBuffer(dx6::IDirect3DExecuteBuffer *aOriginal, void *temp) : ProxyInterface(aOriginal)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3DExecuteBuffer()
	{
		ProxyAddressLookupTable.DeleteAddress(this);
	}

	dx6::IDirect3DExecuteBuffer *GetProxyInterface() { return ProxyInterface; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3DExecuteBuffer methods ***/
	STDMETHOD(Initialize)(THIS_ dx6::LPDIRECT3DDEVICE, dx6::LPD3DEXECUTEBUFFERDESC);
	STDMETHOD(Lock)(THIS_ dx6::LPD3DEXECUTEBUFFERDESC);
	STDMETHOD(Unlock)(THIS);
	STDMETHOD(SetExecuteData)(THIS_ dx6::LPD3DEXECUTEDATA);
	STDMETHOD(GetExecuteData)(THIS_ dx6::LPD3DEXECUTEDATA);
	STDMETHOD(Validate)(THIS_ LPDWORD, dx6::LPD3DVALIDATECALLBACK, LPVOID, DWORD);
	STDMETHOD(Optimize)(THIS_ DWORD);
};
