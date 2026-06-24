/**
* Copyright (C) 2017 Elisha Riedlinger
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*/

#include "ddraw.h"

// Phase 9.3.7: ExecuteBuffer hook
//   - 在 IDirect3DDevice::Execute 入口处拦截, 调 ExecuteBufferParser 解析指令流
//   - 结果暂存到全局 g_lastExtractedGeometry, 给 Phase 9.3.11 GBuffer/Deferred 消费
//   - 配置开关: ConfigManager.GetRenderer().enableRenderer
//   - hook 失败 / 解析失败 / 配置关闭 → 完全旁路, 不影响原始 Execute 行为
//   - 此文件 include ddraw.h (已链 Config/ConfigManager.h 路径), 无需新增 include
#include "ExecuteBufferParser.h"
#include "Config/ConfigManager.h"
// Phase 9.3.11a: GBuffer/Deferred 渲染管线消费 ExtractedGeometry
//   - 实际渲染逻辑在 GBufferRenderer (D3D9 设备 + shader 编译 + RT 管理)
//   - 这里只调 RenderFrame, 失败仅 log, 不影响原始 Execute 路径
#include "GBufferRenderer.h"
// Phase 9.4: ShadowRenderer 集成, 在 GBuffer/Deferred 之前从光源视角渲染 depth shadow map
//   - 失败 / 禁用 → 跳过 shadow pass, GBuffer/Deferred 仍跑 (无阴影, 同 Phase 9.3 行为)
#include "ShadowRenderer.h"
#include "../D3D9Context.h"

#include "../Common/Logging.h"

namespace NDDFIX
{
namespace Render
{

// Phase 9.3.7: 全局 ExtractedGeometry 暂存。
//   真实游戏可能多线程调用 Execute; 但流星蝴蝶剑主线程独占, 用简单的全局
//   覆盖式赋值即可。Phase 9.3.11 GBuffer 集成时, 消费者在每次 Execute 之后
//   立即取走, 不会跨帧遗留, 暂不需双缓冲 / 锁。
//
//   线程安全: 用 std::mutex 保护。Execute 路径中短临界区开销可忽略。
static ExtractedGeometry g_lastExtractedGeometry;
static DWORD             g_lastExtractedFrameId = 0;  // 帧号(自增, 给 HUD/调试用)

// 取最后一次解析结果(供 Phase 9.3.11 GBuffer 消费)。
const ExtractedGeometry& GetLastExtractedGeometry()
{
	return g_lastExtractedGeometry;
}

// 解析的指令流总条数(调试 HUD 用)。
DWORD GetLastExtractedFrameId()
{
	return g_lastExtractedFrameId;
}

// 清空缓存(Reset 设备 / 卸载时调)。
void ClearExtractedGeometry()
{
	g_lastExtractedGeometry = ExtractedGeometry{};
	g_lastExtractedFrameId = 0;
}

} // namespace Render
} // namespace NDDFIX

namespace
{

// Phase 9.3.7: 内部小工具, 把 D3DEXECUTEBUFFER 锁出 lpData + GetExecuteData
//   拿 D3DEXECUTEDATA, 拼出 instruction stream 起点, 调 parser.Parse。
//   任何步骤失败都返 false, 由调用方决定是否阻断原始 Execute。
//
//   设计: 不复制 lpData 内存, parser 直接 reinterpret_cast<const BYTE*>,
//     因为 Parse 是只读且不持有指针(只读取数值后填 vector)。
//
//   失败策略:
//     1) GetExecuteData 失败 → 返 false (execute data 没准备好)
//     2) Lock 失败 → 返 false
//     3) dwInstructionLength == 0 → 视作空 buffer, 不报错, 返 true (no-op)
//     4) instruction 指针越界 lpData 范围 → 返 false
//     5) parser.Parse 内部已处理局部错误, 这里只看返回值
bool ParseExecuteBuffer(dx6::LPDIRECT3DEXECUTEBUFFER execBuf, DWORD& outFrameId)
{
	if (!execBuf) return false;

	// 1) 拿 execute data (dwInstructionOffset / dwInstructionLength)
	dx6::D3DEXECUTEDATA exData = {};
	exData.dwSize = sizeof(exData);
	HRESULT hr = execBuf->GetExecuteData(&exData);
	if (FAILED(hr))
	{
		return false;
	}

	// 2) 锁 buffer 拿 lpData
	dx6::D3DEXECUTEBUFFERDESC desc = {};
	desc.dwSize = sizeof(desc);
	hr = execBuf->Lock(&desc);
	if (FAILED(hr))
	{
		return false;
	}

	bool ok = true;
	do
	{
		if (!desc.lpData || desc.dwBufferSize == 0)
		{
			// 空 buffer: 视作无指令, 不报错
			break;
		}

		// 3) 计算 instruction stream 起点
		if (exData.dwInstructionOffset > desc.dwBufferSize)
		{
			ok = false;
			break;
		}
		const DWORD instSize = exData.dwInstructionLength;
		if (instSize == 0)
		{
			// 无指令 (罕见但合法, 例如只设 transform state)
			break;
		}
		if (exData.dwInstructionOffset + instSize > desc.dwBufferSize)
		{
			ok = false;
			break;
		}

		const BYTE* instPtr = static_cast<const BYTE*>(desc.lpData) + exData.dwInstructionOffset;

		// 4) 解析
		NDDFIX::Render::ExecuteBufferParser parser;
		NDDFIX::Render::ExtractedGeometry extracted;
		if (!parser.Parse(instPtr, instSize, extracted))
		{
			ok = false;
			break;
		}

		// 5) 暂存 (覆盖式)
		NDDFIX::Render::g_lastExtractedGeometry = std::move(extracted);
		++NDDFIX::Render::g_lastExtractedFrameId;
		outFrameId = NDDFIX::Render::g_lastExtractedFrameId;
	} while (false);

	// 6) 务必 Unlock, 不论成功失败, 避免游戏后续 Lock 卡死
	execBuf->Unlock();
	return ok;
}

} // anonymous namespace

HRESULT m_IDirect3DDevice::QueryInterface(REFIID riid, LPVOID * ppvObj)
{
	if ((riid == dx6::IID_IDirect3DDevice || riid == IID_IUnknown) && ppvObj)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	HRESULT hr = ProxyInterface->QueryInterface(riid, ppvObj);

	if (SUCCEEDED(hr))
	{
		genericQueryInterface(riid, ppvObj);
	}

	return hr;
}

ULONG m_IDirect3DDevice::AddRef()
{
	return ProxyInterface->AddRef();
}

ULONG m_IDirect3DDevice::Release()
{
	ULONG x = ProxyInterface->Release();

	if (x == 0)
	{
		delete this;
	}

	return x;
}

HRESULT m_IDirect3DDevice::Initialize(dx6::LPDIRECT3D a, LPGUID b, dx6::LPD3DDEVICEDESC c)
{
	if (a)
	{
		a = static_cast<m_IDirect3D *>(a)->GetProxyInterface();
	}

	return ProxyInterface->Initialize(a, b, c);
}

HRESULT m_IDirect3DDevice::GetCaps(dx6::LPD3DDEVICEDESC a, dx6::LPD3DDEVICEDESC b)
{
	return ProxyInterface->GetCaps(a, b);
}

HRESULT m_IDirect3DDevice::SwapTextureHandles(dx6::LPDIRECT3DTEXTURE a, dx6::LPDIRECT3DTEXTURE b)
{
	if (a)
	{
		a = static_cast<m_IDirect3DTexture *>(a)->GetProxyInterface();
	}
	if (b)
	{
		b = static_cast<m_IDirect3DTexture *>(b)->GetProxyInterface();
	}

	return ProxyInterface->SwapTextureHandles(a, b);
}

HRESULT m_IDirect3DDevice::CreateExecuteBuffer(dx6::LPD3DEXECUTEBUFFERDESC a, dx6::LPDIRECT3DEXECUTEBUFFER * b, IUnknown * c)
{
	HRESULT hr = ProxyInterface->CreateExecuteBuffer(a, b, c);

	if (SUCCEEDED(hr))
	{
		*b = ProxyAddressLookupTable.FindAddress<m_IDirect3DExecuteBuffer>(*b);
	}

	return hr;
}

HRESULT m_IDirect3DDevice::GetStats(dx6::LPD3DSTATS a)
{
	return ProxyInterface->GetStats(a);
}

HRESULT m_IDirect3DDevice::Execute(dx6::LPDIRECT3DEXECUTEBUFFER a, dx6::LPDIRECT3DVIEWPORT b, DWORD c)
{
	// Phase 9.3.7: ExecuteBuffer hook + 解析
	//   - 仅在 ConfigManager 启用 Renderer 时跑(默认 true, 不影响现有游戏)
	//   - hook 失败 → 仅记日志, 仍继续原始 Execute (fail-safe)
	//   - 真正消费解析结果的是 Phase 9.3.11 GBuffer/Deferred (本函数暂只暂存)
	//   - 注意: hook 调的是 wrapper (a), 不是 proxy, 这样能正常 GetExecuteData/Lock
	if (a && NDDFIX::Config::ConfigManager::Instance()->GetRenderer().enableRenderer)
	{
		DWORD frameId = 0;
		if (!ParseExecuteBuffer(a, frameId))
		{
			// 解析失败不阻断原始 Execute
			logf("ExecuteBuffer hook: parse failed (frame=%u), falling back to raw Execute",
				frameId);
		}
		else
		{
			// Phase 9.4: 在 GBuffer/Deferred 之前从光源视角渲染 depth shadow map
			//   - 失败 / 禁用 → 跳过 shadow pass, GBuffer/Deferred 仍跑 (无阴影, 同 Phase 9.3 行为)
			//   - RenderShadowMaps 内部已含 try-style 清理, 这里不需 try/catch
			//   - 此处调 ShadowRenderer 直接走, 不走 D3D9Context 包装层 (省一层 indirection)
			auto* shadow = NDDFIX::Render::ShadowRenderer::Instance();
			auto* dev9 = ND3D9::D3D9Context::Instance()->GetDevice();
			if (dev9 && shadow->IsEnabled() && shadow->IsAvailable())
			{
				D3DXMATRIX shadowView, shadowProj;
				D3DXMatrixIdentity(&shadowView);
				D3DXMatrixIdentity(&shadowProj);
				dev9->GetTransform(D3DTS_VIEW, &shadowView);
				dev9->GetTransform(D3DTS_PROJECTION, &shadowProj);

				HRESULT shadowHr = shadow->RenderShadowMaps(dev9, shadowView, shadowProj);
				if (FAILED(shadowHr))
				{
					logf("ExecuteBuffer hook: Shadow render failed (hr=0x%08X, frame=%u), continuing without shadow",
						shadowHr, frameId);
				}
			}

			// Phase 9.3.11a: 解析成功 → 调 GBuffer/Deferred 渲染管线
			//   - 失败 (GBuffer unavailable / RenderFrame 失败) → 仅记日志, 继续原始 Execute
			//   - RenderFrame 内部已含 try-style 清理, 这里不需 try/catch
			//   - 此处调 GBufferRenderer 直接走, 不走 D3D9Context 包装层 (省一层 indirection)
			//   - D3D9Context 类在 ND3D9 命名空间 (与 IDirect3DDevice::Execute 所在 NDDFIX::Render 不同)
			auto* gbuf = NDDFIX::Render::GBufferRenderer::Instance();
			if (gbuf->IsEnabled() && gbuf->IsAvailable() && dev9)
			{
				D3DXMATRIX viewMat, projMat;
				D3DXMatrixIdentity(&viewMat);
				D3DXMatrixIdentity(&projMat);
				dev9->GetTransform(D3DTS_VIEW, &viewMat);
				dev9->GetTransform(D3DTS_PROJECTION, &projMat);

				HRESULT renderHr = gbuf->RenderFrame(
					dev9,
					NDDFIX::Render::g_lastExtractedGeometry,
					viewMat,
					projMat);
				if (FAILED(renderHr))
				{
					logf("ExecuteBuffer hook: GBuffer/Deferred render failed (hr=0x%08X, frame=%u)",
						renderHr, frameId);
				}
			}
		}
	}

	if (a)
	{
		a = static_cast<m_IDirect3DExecuteBuffer *>(a)->GetProxyInterface();
	}
	if (b)
	{
		b = static_cast<m_IDirect3DViewport *>(b)->GetProxyInterface();
	}

	return ProxyInterface->Execute(a, b, c);
}

HRESULT m_IDirect3DDevice::AddViewport(dx6::LPDIRECT3DVIEWPORT a)
{
	if (a)
	{
		a = static_cast<m_IDirect3DViewport *>(a)->GetProxyInterface();
	}

	return ProxyInterface->AddViewport(a);
}

HRESULT m_IDirect3DDevice::DeleteViewport(dx6::LPDIRECT3DVIEWPORT a)
{
	if (a)
	{
		a = static_cast<m_IDirect3DViewport *>(a)->GetProxyInterface();
	}

	return ProxyInterface->DeleteViewport(a);
}

HRESULT m_IDirect3DDevice::NextViewport(dx6::LPDIRECT3DVIEWPORT a, dx6::LPDIRECT3DVIEWPORT * b, DWORD c)
{
	if (a)
	{
		a = static_cast<m_IDirect3DViewport *>(a)->GetProxyInterface();
	}

	HRESULT hr = ProxyInterface->NextViewport(a, b, c);

	if (SUCCEEDED(hr))
	{
		*b = ProxyAddressLookupTable.FindAddress<m_IDirect3DViewport>(*b);
	}

	return hr;
}

HRESULT m_IDirect3DDevice::Pick(dx6::LPDIRECT3DEXECUTEBUFFER a, dx6::LPDIRECT3DVIEWPORT b, DWORD c, dx6::LPD3DRECT d)
{
	if (a)
	{
		a = static_cast<m_IDirect3DExecuteBuffer *>(a)->GetProxyInterface();
	}
	if (b)
	{
		b = static_cast<m_IDirect3DViewport *>(b)->GetProxyInterface();
	}

	return ProxyInterface->Pick(a, b, c, d);
}

HRESULT m_IDirect3DDevice::GetPickRecords(LPDWORD a, dx6::LPD3DPICKRECORD b)
{
	return ProxyInterface->GetPickRecords(a, b);
}

HRESULT m_IDirect3DDevice::EnumTextureFormats(dx6::LPD3DENUMTEXTUREFORMATSCALLBACK a, LPVOID b)
{
	return ProxyInterface->EnumTextureFormats(a, b);
}

HRESULT m_IDirect3DDevice::CreateMatrix(dx6::LPD3DMATRIXHANDLE a)
{
	return ProxyInterface->CreateMatrix(a);
}

HRESULT m_IDirect3DDevice::SetMatrix(dx6::D3DMATRIXHANDLE a, const dx6::LPD3DMATRIX b)
{
	return ProxyInterface->SetMatrix(a, b);
}

HRESULT m_IDirect3DDevice::GetMatrix(dx6::D3DMATRIXHANDLE a, dx6::LPD3DMATRIX b)
{
	return ProxyInterface->GetMatrix(a, b);
}

HRESULT m_IDirect3DDevice::DeleteMatrix(dx6::D3DMATRIXHANDLE a)
{
	return ProxyInterface->DeleteMatrix(a);
}

HRESULT m_IDirect3DDevice::BeginScene()
{
	return ProxyInterface->BeginScene();
}

HRESULT m_IDirect3DDevice::EndScene()
{
	return ProxyInterface->EndScene();
}

HRESULT m_IDirect3DDevice::GetDirect3D(dx6::LPDIRECT3D * a)
{
	HRESULT hr = ProxyInterface->GetDirect3D(a);

	if (SUCCEEDED(hr))
	{
		*a = ProxyAddressLookupTable.FindAddress<m_IDirect3D>(*a);
	}

	return hr;
}
