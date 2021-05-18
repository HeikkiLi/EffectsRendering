#include "PostFX.h"


PostFX::PostFX() : mMiddleGrey(0.0025f), mWhite(1.5f),
	mDownScale1DBuffer(NULL), mDownScale1DUAV(NULL), mDownScale1DSRV(NULL),
	mDownScaleCB(NULL), mFinalPassCB(NULL),
	mAvgLumBuffer(NULL), mAvgLumUAV(NULL), mAvgLumSRV(NULL),
	mPrevAvgLumBuffer(NULL), mPrevAvgLumUAV(NULL), mPrevAvgLumSRV(NULL),
	mDownScaleFirstPassCS(NULL), mDownScaleSecondPassCS(NULL), mFullScreenQuadVS(NULL),
	mFinalPassPS(NULL), mSampPoint(NULL)
{

}

PostFX::~PostFX()
{

}

bool PostFX::Init(ID3D11Device* device, UINT width, UINT height)
{
	Release();

	HRESULT hr;

	mWidth = width;
	mHeight = height;
	mDownScaleGroups = (UINT)ceil((float)(mWidth * mHeight / 16) / 1024.0f);

	////////////////////////////////////////////////////////////
	// Down scaled luminance buffer
	// buffer, UAV unordered access view and SRV descriptor
	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.StructureByteStride = sizeof(float);
	bufferDesc.ByteWidth = mDownScaleGroups * bufferDesc.StructureByteStride;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	V_RETURN(device->CreateBuffer(&bufferDesc, NULL, &mDownScale1DBuffer));
	DX_SetDebugName(mDownScale1DBuffer, "PostFX - Down Scale 1D Buffer");

	D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV;
	ZeroMemory(&DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	DescUAV.Format = DXGI_FORMAT_UNKNOWN;
	DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	DescUAV.Buffer.NumElements = mDownScaleGroups;
	V_RETURN(device->CreateUnorderedAccessView(mDownScale1DBuffer, &DescUAV, &mDownScale1DUAV));
	DX_SetDebugName(mDownScale1DSRV, "PostFX - Luminance Down Scale 1D SRV");

	/////////////////////////////////////////////////////
	// Average luminance buffer
	/////////////////////////////////////////////////////
	D3D11_SHADER_RESOURCE_VIEW_DESC dsrvd;
	ZeroMemory(&dsrvd, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	dsrvd.Format = DXGI_FORMAT_UNKNOWN;
	dsrvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	dsrvd.Buffer.NumElements = mDownScaleGroups;
	V_RETURN(device->CreateShaderResourceView(mDownScale1DBuffer, &dsrvd, &mDownScale1DSRV));
	DX_SetDebugName(mDownScale1DSRV, "PostFX - Down Scale 1D SRV");

	bufferDesc.ByteWidth = sizeof(float);
	V_RETURN(device->CreateBuffer(&bufferDesc, NULL, &mAvgLumBuffer));
	DX_SetDebugName(mAvgLumBuffer, "PostFX - Average Luminance Buffer");

	V_RETURN(device->CreateBuffer(&bufferDesc, NULL, &mPrevAvgLumBuffer));
	DX_SetDebugName(mPrevAvgLumBuffer, "PostFX - Previous Average Luminance Buffer");

	DescUAV.Buffer.NumElements = 1;
	V_RETURN(device->CreateUnorderedAccessView(mAvgLumBuffer, &DescUAV, &mAvgLumUAV));
	DX_SetDebugName(mAvgLumUAV, "PostFX - Average Luminance UAV");

	V_RETURN(device->CreateUnorderedAccessView(mPrevAvgLumBuffer, &DescUAV, &mPrevAvgLumUAV));
	DX_SetDebugName(mPrevAvgLumUAV, "PostFX - Previous Average Luminance UAV");

	dsrvd.Buffer.NumElements = 1;
	V_RETURN(device->CreateShaderResourceView(mAvgLumBuffer, &dsrvd, &mAvgLumSRV));
	DX_SetDebugName(mAvgLumSRV, "PostFX - Average Luminance SRV");

	V_RETURN(device->CreateShaderResourceView(mPrevAvgLumBuffer, &dsrvd, &mPrevAvgLumSRV));
	DX_SetDebugName(mPrevAvgLumSRV, "PostFX - Previous Average Luminance SRV");


	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.ByteWidth = sizeof(TDownScaleCB);
	V_RETURN(device->CreateBuffer(&bufferDesc, NULL, &mDownScaleCB));
	DX_SetDebugName(mDownScaleCB, "PostFX - Down Scale CB");

	bufferDesc.ByteWidth = sizeof(TFinalPassCB);
	V_RETURN(device->CreateBuffer(&bufferDesc, NULL, &mFinalPassCB));
	DX_SetDebugName(mFinalPassCB, "PostFX - Final Pass CB");

	// Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pShaderBlob = NULL;

	// Downscale shader
	WCHAR postDownScaleShaderSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\PostDownScaleFX.hlsl";

	V_RETURN(CompileShader(postDownScaleShaderSrc, NULL, "DownScaleFirstPass", "cs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateComputeShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mDownScaleFirstPassCS));
	DX_SetDebugName(mFullScreenQuadVS, "Post FX - Down Scale First Pass CS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(postDownScaleShaderSrc, NULL, "DownScaleSecondPass", "cs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateComputeShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mDownScaleSecondPassCS));
	DX_SetDebugName(m_pDownScaleSecondPassCS, "Post FX - Down Scale Second Pass CS");
	SAFE_RELEASE(pShaderBlob);

	// PostFX shader
	WCHAR postFXShaderSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\PostFX.hlsl";
	V_RETURN(CompileShader(postFXShaderSrc, NULL, "FullScreenQuadVS", "vs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mFullScreenQuadVS));
	DX_SetDebugName(mFullScreenQuadVS, "Post FX - Full Screen Quad VS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(postFXShaderSrc, NULL, "FinalPassPS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mFinalPassPS));
	DX_SetDebugName(mFinalPassPS, "Post FX - Final Pass PS");
	SAFE_RELEASE(pShaderBlob);

	// point sampler
	D3D11_SAMPLER_DESC samDesc;
	ZeroMemory(&samDesc, sizeof(samDesc));
	samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samDesc.MaxAnisotropy = 1;
	samDesc.MaxLOD = D3D11_FLOAT32_MAX;
	samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	V_RETURN(device->CreateSamplerState(&samDesc, &mSampPoint));
	return true;

}

void PostFX::Release()
{
	SAFE_RELEASE(mDownScale1DBuffer);
	SAFE_RELEASE(mDownScale1DUAV);
	SAFE_RELEASE(mDownScale1DSRV);
	SAFE_RELEASE(mDownScaleCB);
	SAFE_RELEASE(mFinalPassCB);
	SAFE_RELEASE(mAvgLumBuffer);
	SAFE_RELEASE(mAvgLumUAV);
	SAFE_RELEASE(mAvgLumSRV);
	SAFE_RELEASE(mPrevAvgLumBuffer);
	SAFE_RELEASE(mPrevAvgLumUAV);
	SAFE_RELEASE(mPrevAvgLumSRV);
	SAFE_RELEASE(mDownScaleFirstPassCS);
	SAFE_RELEASE(mDownScaleSecondPassCS);
	SAFE_RELEASE(mFullScreenQuadVS);
	SAFE_RELEASE(mFinalPassPS);
	SAFE_RELEASE(mSampPoint);
}

void PostFX::PostProcessing(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11RenderTargetView* pLDRRTV)
{
	// Down scale the HDR image
	ID3D11RenderTargetView* rt[1] = { NULL };
	pd3dImmediateContext->OMSetRenderTargets(1, rt, NULL);
	DownScale(pd3dImmediateContext, pHDRSRV);

	// Do the final pass
	rt[0] = pLDRRTV;
	pd3dImmediateContext->OMSetRenderTargets(1, rt, NULL);
	FinalPass(pd3dImmediateContext, pHDRSRV);

	// Swap the previous frame average luminance
	ID3D11Buffer* tempBuffer = mAvgLumBuffer;
	ID3D11UnorderedAccessView* tempUAV = mAvgLumUAV;
	ID3D11ShaderResourceView* tempSRV = mAvgLumSRV;
	mAvgLumBuffer = mPrevAvgLumBuffer;
	mAvgLumUAV = mPrevAvgLumUAV;
	mAvgLumSRV = mPrevAvgLumSRV;
	mPrevAvgLumBuffer = tempBuffer;
	mPrevAvgLumUAV = tempUAV;
	mPrevAvgLumSRV = tempSRV;
}

void PostFX::DownScale(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV)
{
	// Output
	ID3D11UnorderedAccessView* arrUAVs[1] = { mDownScale1DUAV };
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, (UINT*)(&arrUAVs));

	// Input
	ID3D11ShaderResourceView* arrViews[2] = { pHDRSRV, NULL };
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);

	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(mDownScaleCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TDownScaleCB* pDownScale = (TDownScaleCB*)MappedResource.pData;
	pDownScale->width = mWidth / 4;
	pDownScale->height = mHeight / 4;
	pDownScale->totalPixels = pDownScale->width * pDownScale->height;
	pDownScale->groupSize = mDownScaleGroups;
	pDownScale->adaptation = mAdaptation;
	pd3dImmediateContext->Unmap(mDownScaleCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { mDownScaleCB };
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);


	// Shader
	pd3dImmediateContext->CSSetShader(mDownScaleFirstPassCS, NULL, 0);

	// Execute the downscales first pass with enough groups to cover the entire full res HDR buffer
	pd3dImmediateContext->Dispatch(mDownScaleGroups, 1, 1);

	///////////////////////////////////////////////
	// Second pass - reduce to a single pixel
	///////////////////////////////////////////////

	// Output
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	arrUAVs[0] = mAvgLumUAV;
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, (UINT*)(&arrUAVs));

	// Input
	arrViews[0] = mDownScale1DSRV;
	arrViews[1] = mPrevAvgLumSRV;
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);

	// Constants
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Shader
	pd3dImmediateContext->CSSetShader(mDownScaleSecondPassCS, NULL, 0);

	// Excute
	pd3dImmediateContext->Dispatch(1, 1, 1);

	// Cleanup
	pd3dImmediateContext->CSSetShader(NULL, NULL, 0);
	ZeroMemory(arrConstBuffers, sizeof(arrConstBuffers));
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, (UINT*)(&arrUAVs));
}

void PostFX::FinalPass(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV)
{
	ID3D11ShaderResourceView* arrViews[2] = { pHDRSRV, mAvgLumSRV };
	pd3dImmediateContext->PSSetShaderResources(0, 2, arrViews);

	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(mFinalPassCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TFinalPassCB* pFinalPass = (TFinalPassCB*)MappedResource.pData;
	pFinalPass->MiddleGrey = mMiddleGrey;
	pFinalPass->LumWhiteSqr = mWhite;
	pFinalPass->LumWhiteSqr *= pFinalPass->MiddleGrey; // Scale by the middle grey value
	pFinalPass->LumWhiteSqr *= pFinalPass->LumWhiteSqr; // Squre
	pd3dImmediateContext->Unmap(mFinalPassCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { mFinalPassCB };
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, arrConstBuffers);

	pd3dImmediateContext->IASetInputLayout(NULL);
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetIndexBuffer(NULL, DXGI_FORMAT_UNKNOWN, 0);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	ID3D11SamplerState* arrSamplers[1] = { mSampPoint };
	pd3dImmediateContext->PSSetSamplers(0, 1, arrSamplers);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(mFullScreenQuadVS, NULL, 0);
	pd3dImmediateContext->PSSetShader(mFinalPassPS, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->PSSetShaderResources(0, 2, arrViews);
	ZeroMemory(arrConstBuffers, sizeof(arrConstBuffers));
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, arrConstBuffers);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
}
