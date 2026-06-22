#pragma once
#include <map>
#include <assert.h>
#include <string>  // Phase 8.11: IResource9Factory::GetType() 返回 std::string

struct HWND__;
typedef struct HWND__ *HWND;

// Phase 8.17: 正确处理 DIRECT3D_VERSION 与 d3d9.h 头保护。
//
// 1) d3d.h (DirectX 6/7) 可能已经被 ddraw.h 间接 include 并把 DIRECT3D_VERSION
//    设为 0x0600。d3d9.h 头部 #if(DIRECT3D_VERSION >= 0x0900) 会让整个头文件
//    内容被跳过，导致 d3dx9math.h 用 LPDIRECT3DCUBETEXTURE9 等类型时报
//    "missing type specifier / syntax error"。
// 2) d3d9.h 自身有 #ifndef _D3D9_H_ 头保护；d3dx9.h 有 #ifndef __D3DX9_H__
//    头保护；d3d9types.h 也自带保护。**绝对不能 #undef 这些保护宏**，
//    否则 d3d9types.h 内的 _D3DMATRIX / _D3DBLEND 等类型在 C++ 模式下
//    报 C2011 'enum/struct type redefinition'（CI #19 失败根因）。
// 3) Phase 8.14 的目标（把 d3dx9.h 移出 ND3D9 namespace，避免 windows.h
//    typedef 被困在 ND3D9 内）仍由 file-scope #include 实现。
//
// 修复策略：
//   * 在 file scope include 之前，用 #if 守护 #define DIRECT3D_VERSION 0x0900
//     强制覆盖 d3d.h 设的旧版本号；
//   * **不 #undef _D3D9_H_ / D3DMATRIX_DEFINED / D3DERR_* 等保护宏**。
// Phase 8.25.6: 用 using 声明把 d3d9 标准类型引入 ND3D9 命名空间。
//
// 原因 (CI #34 新错误):
//   项目代码大量使用 `D3DADAPTER_IDENTIFIER9` / `D3DPOOL` /
//   `D3DTEXTUREFILTERTYPE` / `D3DRENDERSTATETYPE` /
//   `D3DPRIMITIVETYPE` / `D3DTEXTUREOP` /
//   `D3DTRANSFORMSTATETYPE` / `D3DXMATRIX` / `D3DXCOLOR` /
//   `D3DXCreateFontW` / `D3DXGetShaderConstantTable` 等。
//   这些类型/枚举/函数原本假设在 ND3D9 命名空间内（项目演进过程中的历史
//   假设），但 d3d9.h / d3d9types.h / d3dx9.h 当前是 file-scope include，
//   实际定义在全局命名空间。CI #34 编译到这些使用点时报
//   "error C2039: 'X': is not a member of 'ND3D9'"。
//
// 修复策略：
//   1) file scope 仍保留 #include <d3dx9.h>（间接 include d3d9.h / d3d9types.h），
//      所有 d3d9 类型/枚举/函数定义在全局命名空间；
//   2) 在 namespace ND3D9 块内用 `using` 声明逐个把需要的 d3d9 标识符
//      引入到 ND3D9 命名空间（using 声明不重新定义，只是别名）；
//   3) C++11 起的 `using` 声明支持引入 enum 类型本身（不带值），对 enum 值
//      仍需全局访问。**为兼容 DX9 API**，我们这里同时把 d3d9.h 内 IDirect3D9
//      / IDirect3DDevice9 / LPD3DXFONT 等也 using 进来。
//   4) **不在 namespace 内 include 头文件**（#include 不能放 namespace 块内，
//      C++ 标准要求预处理指令在 file scope）。
#if !defined(DIRECT3D_VERSION) || DIRECT3D_VERSION < 0x0900
#undef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0900
#endif
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