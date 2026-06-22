#pragma once
#include <vector>
class m_IDirectDrawSurface4;
class m_IDirect3DViewport3;
class m_IDirect3DDevice3 : public dx6::IDirect3DDevice3, public AddressLookupTableObject
{
private:
	dx6::IDirect3DDevice3 *ProxyInterface;
	std::shared_ptr<WrapperLookupTable<void>> WrapperAddressLookupTable;
	m_IDirectDrawSurface4* m_currentRenderTarget;
	std::vector<m_IDirect3DViewport3*> m_viewports;
	bool m_colorKeyEnabled;
	ULONG Refs;
	dx6::D3DCOLORVALUE m_lightAmbient;

	// Phase 2.5: 立即模式 (immediate mode) 状态。
	// Begin() 时分配 m_immediateVB、记录 primitive type / FVF / stride；
	// Vertex() 时按 stride memcpy 追加到 m_immediateVB；
	// End() 时把 m_immediateVB.data() 当 user pointer 传给 device9->DrawPrimitiveUP，然后清空。
	// 这样不需要走 DX6 原始 ExecuteBuffer → D3D9 ExecuteBuffer 模拟，性能与 DrawPrimitive 路径接近。
	std::vector<BYTE> m_immediateVB;
	bool m_immediateModeActive;
	dx6::D3DPRIMITIVETYPE m_immediatePrimType;
	DWORD m_immediateFVF;
	DWORD m_immediateVertexCount;
	DWORD m_immediateStride;
public:
	m_IDirect3DDevice3(dx6::IDirect3DDevice3 *aOriginal, std::shared_ptr<WrapperLookupTable<void>> wrapperAddressLookupTable)
		: ProxyInterface(aOriginal)
		, WrapperAddressLookupTable(wrapperAddressLookupTable)
		, m_currentRenderTarget(nullptr)
		, m_colorKeyEnabled(false)
		, Refs(1)
		, m_lightAmbient({0})
		, m_immediateModeActive(false)
		, m_immediatePrimType(D3DPT_TRIANGLELIST)
		, m_immediateFVF(0)
		, m_immediateVertexCount(0)
		, m_immediateStride(0)
	{
		ProxyAddressLookupTable.SaveAddress(this, ProxyInterface);
		WrapperAddressLookupTable->SaveWrapper(this, dx6::IID_IDirect3DDevice3);
	}
	~m_IDirect3DDevice3();

	dx6::IDirect3DDevice3 *GetProxyInterface() { return ProxyInterface; }
	bool IsColorKeyEnabled() const { return m_colorKeyEnabled; }
	dx6::D3DCOLORVALUE GetLightAmbient() const { return m_lightAmbient; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	/*** dx6::IDirect3DDevice3 methods ***/
	STDMETHOD(GetCaps)(THIS_ dx6::LPD3DDEVICEDESC, dx6::LPD3DDEVICEDESC);
	STDMETHOD(GetStats)(THIS_ dx6::LPD3DSTATS);
	STDMETHOD(AddViewport)(THIS_ dx6::LPDIRECT3DVIEWPORT3);
	STDMETHOD(DeleteViewport)(THIS_ dx6::LPDIRECT3DVIEWPORT3);
	STDMETHOD(NextViewport)(THIS_ dx6::LPDIRECT3DVIEWPORT3, dx6::LPDIRECT3DVIEWPORT3*, DWORD);
	STDMETHOD(EnumTextureFormats)(THIS_ dx6::LPD3DENUMPIXELFORMATSCALLBACK, LPVOID);
	STDMETHOD(BeginScene)(THIS);
	STDMETHOD(EndScene)(THIS);
	STDMETHOD(GetDirect3D)(THIS_ dx6::LPDIRECT3D3*);
	STDMETHOD(SetCurrentViewport)(THIS_ dx6::LPDIRECT3DVIEWPORT3);
	STDMETHOD(GetCurrentViewport)(THIS_ dx6::LPDIRECT3DVIEWPORT3 *);
	STDMETHOD(SetRenderTarget)(THIS_ LPDIRECTDRAWSURFACE4, DWORD);
	STDMETHOD(GetRenderTarget)(THIS_ LPDIRECTDRAWSURFACE4 *);
	STDMETHOD(Begin)(THIS_ dx6::D3DPRIMITIVETYPE, DWORD, DWORD);
	STDMETHOD(BeginIndexed)(THIS_ dx6::D3DPRIMITIVETYPE, DWORD, LPVOID, DWORD, DWORD);
	STDMETHOD(Vertex)(THIS_ LPVOID);
	STDMETHOD(Index)(THIS_ WORD);
	STDMETHOD(End)(THIS_ DWORD);
	STDMETHOD(GetRenderState)(THIS_ dx6::D3DRENDERSTATETYPE, LPDWORD);
	STDMETHOD(SetRenderState)(THIS_ dx6::D3DRENDERSTATETYPE, DWORD);
	STDMETHOD(GetLightState)(THIS_ dx6::D3DLIGHTSTATETYPE, LPDWORD);
	STDMETHOD(SetLightState)(THIS_ dx6::D3DLIGHTSTATETYPE, DWORD);
	STDMETHOD(SetTransform)(THIS_ dx6::D3DTRANSFORMSTATETYPE, dx6::LPD3DMATRIX);
	STDMETHOD(GetTransform)(THIS_ dx6::D3DTRANSFORMSTATETYPE, dx6::LPD3DMATRIX);
	STDMETHOD(MultiplyTransform)(THIS_ dx6::D3DTRANSFORMSTATETYPE, dx6::LPD3DMATRIX);
	STDMETHOD(DrawPrimitive)(THIS_ dx6::D3DPRIMITIVETYPE, DWORD, LPVOID, DWORD, DWORD);
	STDMETHOD(DrawIndexedPrimitive)(THIS_ dx6::D3DPRIMITIVETYPE, DWORD, LPVOID, DWORD, LPWORD, DWORD, DWORD);
	STDMETHOD(SetClipStatus)(THIS_ dx6::LPD3DCLIPSTATUS);
	STDMETHOD(GetClipStatus)(THIS_ dx6::LPD3DCLIPSTATUS);
	STDMETHOD(DrawPrimitiveStrided)(THIS_ dx6::D3DPRIMITIVETYPE, DWORD, dx6::LPD3DDRAWPRIMITIVESTRIDEDDATA, DWORD, DWORD);
	STDMETHOD(DrawIndexedPrimitiveStrided)(THIS_ dx6::D3DPRIMITIVETYPE, DWORD, dx6::LPD3DDRAWPRIMITIVESTRIDEDDATA, DWORD, LPWORD, DWORD, DWORD);
	STDMETHOD(DrawPrimitiveVB)(THIS_ dx6::D3DPRIMITIVETYPE, dx6::LPDIRECT3DVERTEXBUFFER, DWORD, DWORD, DWORD);
	STDMETHOD(DrawIndexedPrimitiveVB)(THIS_ dx6::D3DPRIMITIVETYPE, dx6::LPDIRECT3DVERTEXBUFFER, LPWORD, DWORD, DWORD);
	STDMETHOD(ComputeSphereVisibility)(THIS_ dx6::LPD3DVECTOR, dx6::LPD3DVALUE, DWORD, DWORD, LPDWORD);
	STDMETHOD(GetTexture)(THIS_ DWORD, dx6::LPDIRECT3DTEXTURE2 *);
	STDMETHOD(SetTexture)(THIS_ DWORD, dx6::LPDIRECT3DTEXTURE2);
	STDMETHOD(GetTextureStageState)(THIS_ DWORD, dx6::D3DTEXTURESTAGESTATETYPE, LPDWORD);
	STDMETHOD(SetTextureStageState)(THIS_ DWORD, dx6::D3DTEXTURESTAGESTATETYPE, DWORD);
	STDMETHOD(ValidateDevice)(THIS_ LPDWORD);
};
