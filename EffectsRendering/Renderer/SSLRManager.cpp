#include "SSLRManager.h"

#pragma pack(push,1)
struct CB_OCCLUSSION
{
	UINT nWidth;
	UINT nHeight;
	UINT pad[2];
};

struct CB_LIGHT_RAYS
{
	Vector2 vSunPos;
	float fInitDecay;
	float fDistDecay;
	Vector3 vRayColor;
	float fMaxDeltaLen;
};
#pragma pack(pop)


bool g_ShowRayTraceRes = false;

SSLRManager::SSLRManager() : mInitDecay(0.2f), mDistDecay(0.8f), mMaxDeltaLen(0.005f),
mOcclusionTex(NULL), mOcclusionUAV(NULL), mOcclusionSRV(NULL),
mLightRaysTex(NULL), mLightRaysRTV(NULL), mLightRaysSRV(NULL),
mOcclusionCB(NULL), mOcclusionCS(NULL), mRayTraceCB(NULL), mFullScreenVS(NULL), mRayTracePS(NULL), mCombinePS(NULL),
mAdditiveBlendState(NULL)
{

}

SSLRManager::~SSLRManager()
{

}

HRESULT SSLRManager::Init(ID3D11Device* device, UINT width, UINT height)
{
	HRESULT hr;

	mWidth = width / 2;
	mHeight = height / 2;

	// Allocate the occlusion resources
	D3D11_TEXTURE2D_DESC t2dDesc = {
		mWidth, //UINT Width;
		mHeight, //UINT Height;
		1, //UINT MipLevels;
		1, //UINT ArraySize;
		DXGI_FORMAT_R8_TYPELESS, //DXGI_FORMAT Format;
		1, //DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
		0,//UINT CPUAccessFlags;
		0//UINT MiscFlags;    
	};
	V_RETURN(device->CreateTexture2D(&t2dDesc, NULL, &mOcclusionTex));
	DX_SetDebugName(mOcclusionTex, "SSLR Occlusion Texture");

	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
	ZeroMemory(&UAVDesc, sizeof(UAVDesc));
	UAVDesc.Format = DXGI_FORMAT_R8_UNORM;
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	V_RETURN(device->CreateUnorderedAccessView(mOcclusionTex, &UAVDesc, &mOcclusionUAV));
	DX_SetDebugName(mOcclusionUAV, "SSLR Occlusion UAV");

	D3D11_SHADER_RESOURCE_VIEW_DESC dsrvd =
	{
		DXGI_FORMAT_R8_UNORM,
		D3D11_SRV_DIMENSION_TEXTURE2D,
		0,
		0
	};
	dsrvd.Texture2D.MipLevels = 1;
	V_RETURN(device->CreateShaderResourceView(mOcclusionTex, &dsrvd, &mOcclusionSRV));
	DX_SetDebugName(mOcclusionSRV, "SSLR Occlusion SRV");

	/////////////////////////////////////////////
	// Allocate the light rays resources
	t2dDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	V_RETURN(device->CreateTexture2D(&t2dDesc, NULL, &mLightRaysTex));
	DX_SetDebugName(mLightRaysTex, "SSLR Light Rays Texture");

	D3D11_RENDER_TARGET_VIEW_DESC rtsvd =
	{
		DXGI_FORMAT_R8_UNORM,
		D3D11_RTV_DIMENSION_TEXTURE2D
	};
	V_RETURN(device->CreateRenderTargetView(mLightRaysTex, &rtsvd, &mLightRaysRTV));
	DX_SetDebugName(mLightRaysRTV, "SSLR Light Rays RTV");

	V_RETURN(device->CreateShaderResourceView(mLightRaysTex, &dsrvd, &mLightRaysSRV));
	DX_SetDebugName(mLightRaysSRV, "SSLR Light Rays SRV");

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate the occlussion constant buffer
	D3D11_BUFFER_DESC CBDesc;
	ZeroMemory(&CBDesc, sizeof(CBDesc));
	CBDesc.Usage = D3D11_USAGE_DYNAMIC;
	CBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	CBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	CBDesc.ByteWidth = sizeof(CB_OCCLUSSION);
	V_RETURN(device->CreateBuffer(&CBDesc, NULL, &mOcclusionCB));
	DX_SetDebugName(mOcclusionCB, "SSLR - Occlussion CB");

	CBDesc.ByteWidth = sizeof(CB_LIGHT_RAYS);
	V_RETURN(device->CreateBuffer(&CBDesc, NULL, &mRayTraceCB));
	DX_SetDebugName(mRayTraceCB, "SSLR - Ray Trace CB");

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;// | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if defined( DEBUG ) || defined( _DEBUG )
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pShaderBlob = NULL;
	WCHAR sslrSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\SSLR.hlsl";

	V_RETURN(CompileShader(sslrSrc, NULL, "Occlussion", "cs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateComputeShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mOcclusionCS));
	DX_SetDebugName(mOcclusionCS, "SSLR - Occlussion CS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(sslrSrc, NULL, "RayTraceVS", "vs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mFullScreenVS));
	DX_SetDebugName(mFullScreenVS, "SSLR - Full Screen VS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(sslrSrc, NULL, "RayTracePS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mRayTracePS));
	DX_SetDebugName(mRayTracePS, "SSLR - Ray Trace PS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(sslrSrc, NULL, "CombinePS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mCombinePS));
	DX_SetDebugName(mCombinePS, "SSLR - Combine PS");
	SAFE_RELEASE(pShaderBlob);

	// Create the additive blend state
	D3D11_BLEND_DESC descBlend;
	descBlend.AlphaToCoverageEnable = FALSE;
	descBlend.IndependentBlendEnable = FALSE;
	const D3D11_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		TRUE,
		D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD,
		D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD,
		D3D11_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		descBlend.RenderTarget[i] = defaultRenderTargetBlendDesc;
	V_RETURN(device->CreateBlendState(&descBlend, &mAdditiveBlendState));
	DX_SetDebugName(mAdditiveBlendState, "SSLR - Additive Alpha BS");

	return hr;
}

void SSLRManager::Release()
{
	SAFE_RELEASE(mOcclusionTex);
	SAFE_RELEASE(mOcclusionUAV);
	SAFE_RELEASE(mOcclusionSRV);
	SAFE_RELEASE(mLightRaysTex);
	SAFE_RELEASE(mLightRaysRTV);
	SAFE_RELEASE(mLightRaysSRV);
	SAFE_RELEASE(mOcclusionCB);
	SAFE_RELEASE(mOcclusionCS);
	SAFE_RELEASE(mRayTraceCB);
	SAFE_RELEASE(mFullScreenVS);
	SAFE_RELEASE(mRayTracePS);
	SAFE_RELEASE(mCombinePS);
	SAFE_RELEASE(mAdditiveBlendState);
}

void SSLRManager::Render(ID3D11DeviceContext* pd3dImmediateContext, ID3D11RenderTargetView* pLightAccumRTV, ID3D11ShaderResourceView* pMiniDepthSRV, const Vector3& vSunDir, const Vector3& vSunColor, Camera* camera)
{
	// No need to do anything if the camera is facing away from the sun
	// This will not work if the FOV is close to 180 or higher
	const float dotCamSun = -Vector3(camera->GetLook()).Dot(vSunDir);
	if (dotCamSun <= 0.0f)
	{
		return;
	}

	Vector3 vSunPos = -200.0f * vSunDir;
	Vector3 vEyePos = camera->GetPosition();
	vSunPos.x += vEyePos.x;
	vSunPos.z += vEyePos.z;
	Matrix mView = camera->View();
	Matrix mProj = camera->Proj();
	Matrix mViewProjection = mView * mProj;
	Vector3 vSunPosSS =	XMVector3TransformCoord(vSunPos, mViewProjection);

	// If the sun is too far out of view we just want to turn off the effect
	static const float fMaxSunDist = 1.3f;
	if (abs(vSunPosSS.x) >= fMaxSunDist || abs(vSunPosSS.y) >= fMaxSunDist)
	{
		return;
	}

	// Attenuate the sun color based on how far the sun is from the view
	Vector3 vSunColorAtt = vSunColor;
	float fMaxDist = max(abs(vSunPosSS.x), abs(vSunPosSS.y));
	if (fMaxDist >= 1.0f)
	{
		vSunColorAtt *= (fMaxSunDist - fMaxDist);
	}

	PrepareOcclusion(pd3dImmediateContext, pMiniDepthSRV);
	RayTrace(pd3dImmediateContext, Vector2(vSunPosSS.x, vSunPosSS.y), vSunColorAtt);
	if (!g_ShowRayTraceRes)
		Combine(pd3dImmediateContext, pLightAccumRTV);
}

void SSLRManager::PrepareOcclusion(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pMiniDepthSRV)
{
	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(mOcclusionCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	CB_OCCLUSSION* pOcclussion = (CB_OCCLUSSION*)MappedResource.pData;
	pOcclussion->nWidth = mWidth;
	pOcclussion->nHeight = mHeight;
	pd3dImmediateContext->Unmap(mOcclusionCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { mOcclusionCB };
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Output
	ID3D11UnorderedAccessView* arrUAVs[1] = { mOcclusionUAV };
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);

	// Input
	ID3D11ShaderResourceView* arrViews[1] = { pMiniDepthSRV };
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader(mOcclusionCS, NULL, 0);

	// Execute the downscales first pass with enough groups to cover the entire full res HDR buffer
	pd3dImmediateContext->Dispatch((UINT)ceil((float)(mWidth * mHeight) / 1024.0f), 1, 1);

	// Cleanup
	pd3dImmediateContext->CSSetShader(NULL, NULL, 0);
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);
	ZeroMemory(arrConstBuffers, sizeof(arrConstBuffers));
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);
}

void SSLRManager::RayTrace(ID3D11DeviceContext* pd3dImmediateContext, const Vector2& vSunPosSS, const  Vector3& vSunColor)
{
	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	pd3dImmediateContext->ClearRenderTargetView(mLightRaysRTV, ClearColor);

	D3D11_VIEWPORT oldvp;
	UINT num = 1;
	pd3dImmediateContext->RSGetViewports(&num, &oldvp);
	if (!g_ShowRayTraceRes)
	{
		D3D11_VIEWPORT vp[1] = { { 0, 0, (float)mWidth, (float)mHeight, 0.0f, 1.0f } };
		pd3dImmediateContext->RSSetViewports(1, vp);

		pd3dImmediateContext->OMSetRenderTargets(1, &mLightRaysRTV, NULL);
	}

	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(mRayTraceCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	CB_LIGHT_RAYS* pRayTrace = (CB_LIGHT_RAYS*)MappedResource.pData;
	pRayTrace->vSunPos = Vector2(0.5f * vSunPosSS.x + 0.5f, -0.5f * vSunPosSS.y + 0.5f);
	pRayTrace->fInitDecay = mInitDecay;
	pRayTrace->fDistDecay = mDistDecay;
	pRayTrace->vRayColor = vSunColor;
	pRayTrace->fMaxDeltaLen = mMaxDeltaLen;
	pd3dImmediateContext->Unmap(mRayTraceCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { mRayTraceCB };
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, arrConstBuffers);

	// Input
	ID3D11ShaderResourceView* arrViews[1] = { mOcclusionSRV };
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);

	// Primitive settings
	pd3dImmediateContext->IASetInputLayout(NULL);
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(mFullScreenVS, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(mRayTracePS, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	arrViews[0] = NULL;
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pd3dImmediateContext->RSSetViewports(1, &oldvp);
}

void SSLRManager::Combine(ID3D11DeviceContext* pd3dImmediateContext, ID3D11RenderTargetView* pLightAccumRTV)
{
	ID3D11BlendState* pPrevBlendState;
	FLOAT prevBlendFactor[4];
	UINT prevSampleMask;
	pd3dImmediateContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
	pd3dImmediateContext->OMSetBlendState(mAdditiveBlendState, prevBlendFactor, prevSampleMask);

	// Restore the light accumulation view
	pd3dImmediateContext->OMSetRenderTargets(1, &pLightAccumRTV, NULL);

	// Constants
	ID3D11Buffer* arrConstBuffers[1] = { mRayTraceCB };
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, arrConstBuffers);

	// Input
	ID3D11ShaderResourceView* arrViews[1] = { mLightRaysSRV };
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);

	// Primitive settings
	pd3dImmediateContext->IASetInputLayout(NULL);
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(mFullScreenVS, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(mCombinePS, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	arrViews[0] = NULL;
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pd3dImmediateContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
}
