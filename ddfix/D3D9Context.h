#pragma once
#include <map>
#include <assert.h>
#include <string>  // Phase 8.11: IResource9Factory::GetType() 返回 std::string

struct HWND__;
typedef struct HWND__ *HWND;

// Phase 8.17: d3d9.h / d3dx9.h 自身已有标准头保护：
//   d3d9.h  : #ifndef _D3D9_H_  /  #define _D3D9_H_
//   d3dx9.h : #ifndef __D3DX9_H__  /  #define __D3DX9_H__
//   d3d9types.h : #ifndef _d3d9types_h_  /  #define _d3d9types_h_
// 而且本头用 #pragma once。**不需要也不应该** #undef 这些保护宏：
//   1) 删 #undef _D3D9_H_ —— 之前 undef 后 d3d9.h 被二次 include，
//      d3d9types.h 内的 _D3DMATRIX/_D3DBLEND 等 9 个类型在 C++ 模式下
//      报 C2011 'enum/struct type redefinition'（CI #19 失败根因）。
//   2) 删 #undef DIRECT3D_VERSION —— d3d9.h 内部已经 #ifndef 包了，
//      重复 #define 也不会出错。
//   3) 删 #undef D3DMATRIX_DEFINED —— d3dx9.h 自带 #ifndef D3DMATRIX_DEFINED 保护。
// Phase 8.14 的目标（把 d3dx9.h 移出 ND3D9 namespace，避免 windows.h typedef 被困）
// 仍然通过 file-scope #include 实现，与这些 #undef 无关。
#include <d3dx9.h>
#include "Common/SmartPointer.h"

namespace ND3DX9
{

}

namespace ND3D9
{
	using Resource9Handle = int;

	class D3D9Context;

	struct IResource9Factory
	{
		virtual ~IResource9Factory() = default;

		virtual IUnknown* Create(D3D9Context* context) const = 0;
		virtual bool IsCreateInVideoMemory() const = 0;
		virtual std::string GetType() const = 0;
	};

	class D3D9Context final
	{
		struct Resource9Info
		{
			Resource9Handle handle;
			IResource9Factory* factory;
			IUnknown* pointer;
		};

	private:
		D3D9Context();
	public:
		~D3D9Context();
		static D3D9Context* Instance();

		void Initialize(::HWND hwnd);
		void Uninitialize();
		IDirect3DDevice9* GetDevice() const;
		IDirect3D9* GetD3D9() const;
		void GetBackBufferSize(int* width, int* height);
		void TagDeviceLost();
		bool IsDeviceLost() const;

		Resource9Handle CreateOffScreenSurface9(int width, int height, D3DFORMAT format, D3DPOOL pool);
		Resource9Handle CreateZBufferSurface9(int width, int height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL discard);
		Resource9Handle GetBackBuffer9();
		Resource9Handle CreateTexture9(int width, int height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool);
		Resource9Handle CreateRenderTarget(int width, int height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL lockable);
		Resource9Handle CreateSprite();
		Resource9Handle CreateVertexBuffer9(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool);

		template<class T>
		SmartPtr<T> GetResource9(Resource9Handle handle, std::string* pType)
		{
			SmartPtr<T> result;
			*(void**)&result = GetResource9(handle, pType);
			return result;
		}

		
		HRESULT ResetDevice();
		ULONG ReleaseResource9(Resource9Handle handle);

		// Phase 2.3: 共享 ColorKey 像素着色器（懒加载，单例）
		// 所有 RenderTarget HardwareSurface9Wrapper 共享同一 PS/ConstantTable，避免每次构造时重复 CreatePixelShader。
		// Phase 8.16: 移除 ND3D9:: 限定。d3dx9.h 已在 file scope include，类型为全局 typedef，
		// 之前的限定是冗余但无害；清理后避免 C2872 ambiguous（ND3D9 vs ::）风险。
		IDirect3DPixelShader9* GetSharedColorKeyShader();
		ID3DXConstantTable* GetSharedColorKeyConstantTable();

	private:
		// 内部：懒加载创一次 ColorKey 着色器配套资源。
		void EnsureSharedColorKeyShader();

	private:
		IUnknown * GetResource9(Resource9Handle handle, std::string* pType)
		{
			// 不知道为什么定义如果写在cpp文件里，会导致编译失败
			IUnknown* result = nullptr;
			auto itor = m_resAllocated.find(handle);
			if (itor != m_resAllocated.end())
			{
				auto ptr = itor->second.pointer;
				ptr->AddRef();
				if (pType)
				{
					*pType = itor->second.factory->GetType();
				}
				result = ptr;
			}
			return result;
		}
		Resource9Handle LogResource(IResource9Factory* factory, IUnknown* pointer);
		void CalcBackBufferSize();
		HRESULT CreateDevice();
		void BuildD3DPresentParameters(D3DPRESENT_PARAMETERS &d3dpp);
		void RebuildResource9(Resource9Handle handle);
	private:

		IDirect3D9 * m_d3d9;
		IDirect3DDevice9* m_d3dDev9;

		std::map<Resource9Handle, Resource9Info> m_resAllocated;
		int m_resCountHistory;

		int m_backBufferWidth;
		int m_backBufferHeight;
		Resource9Handle m_backBuffer9Handle;

		bool m_deviceLost;

		::HWND m_hwnd;

		// Phase 2.3: 共享 PS / ConstantTable。裸指针由 D3D9 设备管理，Reset 时随设备一起失效。
		// Phase 8.16: 移除 ND3D9:: 限定，保持与 D3D9Context.cpp 实现一致。
		IDirect3DPixelShader9* m_colorKeyShader;
		ID3DXConstantTable* m_colorKeyConstantTable;
		bool m_colorKeyShaderInited;
	};
}