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

// void genericDdrawQueryInterface(REFIID CalledID, LPVOID * ppvObj)
// {
// 	REFIID riid = (CalledID == CLSID_DirectDraw) ? IID_IDirectDraw :
// 		(CalledID == CLSID_DirectDraw7) ? IID_IDirectDraw7 :
// 		(CalledID == CLSID_DirectDrawClipper) ? IID_IDirectDrawClipper :
// 		(CalledID == CLSID_DirectDrawFactory) ? IID_IDirectDrawFactory :
// 		CalledID;
// 
// #define QUERYINTERFACE(x) \
// 	if (riid == IID_ ## x) \
// 		{ \
// 			*ppvObj = ProxyAddressLookupTable.FindAddress<m_ ## x>(*ppvObj); \
// 		}
// 
// 	QUERYINTERFACE(dx6::IDirect3D);
// 	QUERYINTERFACE(dx6::IDirect3D2);
// 	QUERYINTERFACE(dx6::IDirect3D3);
// 	QUERYINTERFACE(dx6::IDirect3D7);
// 	QUERYINTERFACE(dx6::IDirect3DDevice);
// 	QUERYINTERFACE(dx6::IDirect3DDevice2);
// 	QUERYINTERFACE(dx6::IDirect3DDevice3);
// 	QUERYINTERFACE(dx6::IDirect3DDevice7);
// 	QUERYINTERFACE(dx6::IDirect3DExecuteBuffer);
// 	QUERYINTERFACE(dx6::IDirect3DLight);
// 	QUERYINTERFACE(dx6::IDirect3DMaterial);
// 	QUERYINTERFACE(dx6::IDirect3DMaterial2);
// 	QUERYINTERFACE(dx6::IDirect3DMaterial3);
// 	QUERYINTERFACE(dx6::IDirect3DTexture);
// 	QUERYINTERFACE(dx6::IDirect3DTexture2);
// 	QUERYINTERFACE(IDirect3DVertexBuffer);
// 	QUERYINTERFACE(dx6::IDirect3DVertexBuffer7);
// 	QUERYINTERFACE(dx6::IDirect3DViewport);
// 	QUERYINTERFACE(dx6::IDirect3DViewport2);
// 	QUERYINTERFACE(dx6::IDirect3DViewport3);
// 	QUERYINTERFACE(IDirectDraw);
// 	QUERYINTERFACE(IDirectDraw2);
// 	QUERYINTERFACE(IDirectDraw3);
// 	QUERYINTERFACE(IDirectDraw4);
// 	QUERYINTERFACE(IDirectDraw7);
// 	QUERYINTERFACE(IDirectDrawClipper);
// 	QUERYINTERFACE(IDirectDrawColorControl);
// 	QUERYINTERFACE(IDirectDrawFactory);
// 	QUERYINTERFACE(IDirectDrawGammaControl);
// 	QUERYINTERFACE(IDirectDrawPalette);
// 	QUERYINTERFACE(IDirectDrawSurface);
// 	QUERYINTERFACE(IDirectDrawSurface2);
// 	QUERYINTERFACE(IDirectDrawSurface3);
// 	QUERYINTERFACE(IDirectDrawSurface4);
// 	QUERYINTERFACE(IDirectDrawSurface7);
// }
