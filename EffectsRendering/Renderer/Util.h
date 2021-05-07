#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <wchar.h>
#include <winerror.h>
#include <stdarg.h>
#include <cassert>
#include <ctime>
#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>

#include "DemoTimer.h"


// gui includes
#include "imgui.h"
#include "imgui_impl_dx11.h"

using namespace DirectX;

const float M_PI = 3.1415926535f;
const float M_PI2 = 2 * M_PI;

/*
#if defined(DEBUG) | defined(_DEBUG)
	#ifndef HR
	#define HR(x)                                              \
	{                                                          \
		HRESULT hr = (x);                                      \
		if(FAILED(hr))                                         \
		{                                                      \
			DXTrace(__FILE__, (DWORD)__LINE__, hr, L#x, true); \
		}                                                      \
	}
	#endif

#else
*/
	#ifndef HR
	#define HR(x) (x)
	#endif
//#endif

#define ReleaseCOM(x)	{ if(x){ x->Release(); x = 0; } }

#define SAFE_DELETE(x)	{ if(x){ delete x; x = NULL;} }

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }

#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return false; } }
#endif


#ifndef V
#define V(x)           { hr = (x); }
#endif

static bool CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, const char * strEntryPoint, const char * strProfile, DWORD dwShaderFlags, ID3DBlob ** blob)
{
	if (!strPath || !strEntryPoint || !strProfile || !blob)
		return false;

	*blob = nullptr;

	HRESULT hr;
	ID3DInclude* include = D3D_COMPILE_STANDARD_FILE_INCLUDE;
	ID3DBlob* shaderBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	if (FAILED(hr = D3DCompileFromFile(strPath, pMacros, include, strEntryPoint, strProfile, dwShaderFlags, 0,
		&shaderBlob, &errorBlob)))
	{
		if (errorBlob)
		{
			int buffSize = errorBlob->GetBufferSize() + 1;
			LPWSTR gah = new wchar_t[buffSize];
			MultiByteToWideChar(CP_ACP, 0, (char*)errorBlob->GetBufferPointer(), buffSize, gah, buffSize);
			OutputDebugString(gah);
			delete[] gah;
			OutputDebugString(L"\n");
			errorBlob->Release();
		}

		if (shaderBlob)
			shaderBlob->Release();
		
		return false;
	}

	*blob = shaderBlob;

	return true;
}

// Profiling/instrumentation support, modified from DXUT

// Use DX_SetDebugName() to attach names to D3D objects for use by 
// SDKDebugLayer, PIX's object table, etc.
#if defined(PROFILE) || defined(_DEBUG)
inline void DX_SetDebugName(_In_ IDXGIObject* pObj, _In_z_ const CHAR* pstrName)
{
	if (pObj)
		pObj->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(pstrName), pstrName);
}
inline void DX_SetDebugName(_In_ ID3D11Device* pObj, _In_z_ const CHAR* pstrName)
{
	if (pObj)
		pObj->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(pstrName), pstrName);
}
inline void DX_SetDebugName(_In_ ID3D11DeviceChild* pObj, _In_z_ const CHAR* pstrName)
{
	if (pObj)
		pObj->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(pstrName), pstrName);
}
#else
#define DX_SetDebugName( pObj, pstrName )
#endif

static float rad2deg(float rad)
{
	return rad * (180 / M_PI);
}