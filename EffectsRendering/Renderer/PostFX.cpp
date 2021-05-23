#include "PostFX.h"


PostFX::PostFX() : mMiddleGrey(0.0025f), mWhite(1.5f), mBloomThreshold(2.0), mBloomScale(0.1f),
	mDownScaleRT(NULL), mDownScaleSRV(NULL), mDownScaleUAV(NULL),
	mBloomRT(NULL), mBloomSRV(NULL), mBloomUAV(NULL),
	mDownScale1DBuffer(NULL), mDownScale1DUAV(NULL), mDownScale1DSRV(NULL),
	mDownScaleCB(NULL), mFinalPassCB(NULL), mBlurCB(NULL),
	mAvgLumBuffer(NULL), mAvgLumUAV(NULL), mAvgLumSRV(NULL),
	mPrevAvgLumBuffer(NULL), mPrevAvgLumUAV(NULL), mPrevAvgLumSRV(NULL),
	mDownScaleFirstPassCS(NULL), mDownScaleSecondPassCS(NULL), mFullScreenQuadVS(NULL),
	mFinalPassPS(NULL), mSampPoint(NULL), mBloomRevealCS(NULL),
	mHorizontalBlurCS(NULL), mVerticalBlurCS(NULL), mSampLinear(NULL)
{
	mTempRT[0] = NULL;
	mTempRT[1] = NULL;
	mTempSRV[0] = NULL;
	mTempSRV[1] = NULL;
	mTempUAV[0] = NULL;
	mTempUAV[1] = NULL;

	mEnableBloom = true;
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

	// Allocate the downscaled target
	D3D11_TEXTURE2D_DESC dtd = {
		mWidth / 4, //UINT Width;
		mHeight / 4, //UINT Height;
		1, //UINT MipLevels;
		1, //UINT ArraySize;
		DXGI_FORMAT_R16G16B16A16_TYPELESS, //DXGI_FORMAT Format;
		1, //DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,//UINT BindFlags;
		0,//UINT CPUAccessFlags;
		0//UINT MiscFlags;    
	};
	V_RETURN(device->CreateTexture2D(&dtd, NULL, &mDownScaleRT));
	DX_SetDebugName(mDownScaleRT, "PostFX - Down Scaled RT");

	// Create the resource views
	D3D11_SHADER_RESOURCE_VIEW_DESC dsrvd;
	ZeroMemory(&dsrvd, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	dsrvd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	dsrvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	dsrvd.Texture2D.MipLevels = 1;
	V_RETURN(device->CreateShaderResourceView(mDownScaleRT, &dsrvd, &mDownScaleSRV));
	DX_SetDebugName(mDownScaleSRV, "PostFX - Down Scaled SRV");

	// Create the UAVs
	D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV;
	ZeroMemory(&DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	DescUAV.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	DescUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	DescUAV.Buffer.FirstElement = 0;
	DescUAV.Buffer.NumElements = mWidth * mHeight / 16;
	V_RETURN(device->CreateUnorderedAccessView(mDownScaleRT, &DescUAV, &mDownScaleUAV));
	DX_SetDebugName(mDownScaleUAV, "PostFX - Down Scaled UAV");

	////////////////////////////////
	// Allocate temporary target  //
	////////////////////////////////
	V_RETURN(device->CreateTexture2D(&dtd, NULL, &mTempRT[0]));
	DX_SetDebugName(mTempRT[0], "PostFX - Temp 0 RT");

	V_RETURN(device->CreateShaderResourceView(mTempRT[0], &dsrvd, &mTempSRV[0]));
	DX_SetDebugName(mTempSRV[0], "PostFX - Temp 0 SRV");

	V_RETURN(device->CreateUnorderedAccessView(mTempRT[0], &DescUAV, &mTempUAV[0]));
	DX_SetDebugName(mTempUAV[0], "PostFX - Temp 0 UAV");

	V_RETURN(device->CreateTexture2D(&dtd, NULL, &mTempRT[1]));
	DX_SetDebugName(mTempRT[1], "PostFX - Temp 1 RT");

	V_RETURN(device->CreateShaderResourceView(mTempRT[1], &dsrvd, &mTempSRV[1]));
	DX_SetDebugName(mTempSRV[1], "PostFX - Temp 1 SRV");

	V_RETURN(device->CreateUnorderedAccessView(mTempRT[1], &DescUAV, &mTempUAV[1]));
	DX_SetDebugName(mTempUAV[1], "PostFX - Temp 1 UAV");

	/////////////////////////////
	// Allocate bloom target   //
	/////////////////////////////
	V_RETURN(device->CreateTexture2D(&dtd, NULL, &mBloomRT));
	DX_SetDebugName(mBloomRT, "PostFX - Bloom RT");

	V_RETURN(device->CreateShaderResourceView(mBloomRT, &dsrvd, &mBloomSRV));
	DX_SetDebugName(mBloomSRV, "PostFX - Bloom SRV");

	V_RETURN(device->CreateUnorderedAccessView(mBloomRT, &DescUAV, &mBloomUAV));
	DX_SetDebugName(mBloomUAV, "PostFX - Bloom UAV");


	////////////////////////////////////////////////////////////
	// Down scaled luminance buffer
	// buffer, UAV unordered access view and SRV descriptor
	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.StructureByteStride = sizeof(float);
	bufferDesc.ByteWidth = mDownScaleGroups * bufferDesc.StructureByteStride;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	V_RETURN(device->CreateBuffer(&bufferDesc, NULL, &mDownScale1DBuffer));
	DX_SetDebugName(mDownScale1DBuffer, "PostFX - Luminance Down Scale 1D Buffer");

	ZeroMemory(&DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	DescUAV.Format = DXGI_FORMAT_UNKNOWN;
	DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	DescUAV.Buffer.FirstElement = 0;
	DescUAV.Buffer.NumElements = mDownScaleGroups;
	V_RETURN(device->CreateUnorderedAccessView(mDownScale1DBuffer, &DescUAV, &mDownScale1DUAV));
	DX_SetDebugName(mDownScale1DUAV, "PostFX - Luminance Down Scale 1D UAV");

	dsrvd.Format = DXGI_FORMAT_UNKNOWN;
	dsrvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	dsrvd.Buffer.FirstElement = 0;
	dsrvd.Buffer.NumElements = mDownScaleGroups;
	V_RETURN(device->CreateShaderResourceView(mDownScale1DBuffer, &dsrvd, &mDownScale1DSRV));
	DX_SetDebugName(mDownScale1DSRV, "PostFX - Luminance Down Scale 1D SRV");

	// allocate average luminance buffers
	bufferDesc.ByteWidth = sizeof(float);
	V_RETURN(device->CreateBuffer(&bufferDesc, NULL, &mAvgLumBuffer));
	DX_SetDebugName(mAvgLumBuffer, "PostFX - Average Luminance Buffer");

	DescUAV.Buffer.NumElements = 1;
	V_RETURN(device->CreateUnorderedAccessView(mAvgLumBuffer, &DescUAV, &mAvgLumUAV));
	DX_SetDebugName(mAvgLumUAV, "PostFX - Average Luminance UAV");

	dsrvd.Buffer.NumElements = 1;
	V_RETURN(device->CreateShaderResourceView(mAvgLumBuffer, &dsrvd, &mAvgLumSRV));
	DX_SetDebugName(mAvgLumSRV, "PostFX - Average Luminance SRV");

	// Allocate previous frame average luminance buffer
	V_RETURN(device->CreateBuffer(&bufferDesc, NULL, &mPrevAvgLumBuffer));
	DX_SetDebugName(mPrevAvgLumBuffer, "PostFX - Previous Average Luminance Buffer");
	
	V_RETURN(device->CreateUnorderedAccessView(mPrevAvgLumBuffer, &DescUAV, &mPrevAvgLumUAV));
	DX_SetDebugName(mPrevAvgLumUAV, "PostFX - Previous Average Luminance UAV");

	V_RETURN(device->CreateShaderResourceView(mPrevAvgLumBuffer, &dsrvd, &mPrevAvgLumSRV));
	DX_SetDebugName(mPrevAvgLumSRV, "PostFX - Previous Average Luminance SRV");

	// allocate constant buffer
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

	bufferDesc.ByteWidth = sizeof(TBlurCB);
	V_RETURN(device->CreateBuffer(&bufferDesc, NULL, &mBlurCB));
	DX_SetDebugName(mBlurCB, "PostFX - Blur CB");

	///////////////////////////////////////
	// Compile the shaders               //
	///////////////////////////////////////
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS; // | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pShaderBlob = NULL;

	//////////////////////
	// Downscale shader //
	//////////////////////
	WCHAR postDownScaleShaderSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\PostDownScaleFX.hlsl";

	V_RETURN(CompileShader(postDownScaleShaderSrc, NULL, "DownScaleFirstPass", "cs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateComputeShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mDownScaleFirstPassCS));
	DX_SetDebugName(mDownScaleFirstPassCS, "Post FX - Down Scale First Pass CS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(postDownScaleShaderSrc, NULL, "DownScaleSecondPass", "cs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateComputeShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mDownScaleSecondPassCS));
	DX_SetDebugName(mDownScaleSecondPassCS, "Post FX - Down Scale Second Pass CS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(postDownScaleShaderSrc, NULL, "BloomReveal", "cs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateComputeShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mBloomRevealCS));
	DX_SetDebugName(mBloomRevealCS, "Post FX - Bloom Reveal CS");
	SAFE_RELEASE(pShaderBlob);

	/////////////////////////////
	// Gaussian Blur Shader    //
	/////////////////////////////
	WCHAR blurShaderSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\Blur.hlsl";

	V_RETURN(CompileShader(blurShaderSrc, NULL, "VerticalFilter", "cs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateComputeShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mVerticalBlurCS));
	DX_SetDebugName(mVerticalBlurCS, "Post FX - Vertical Blur CS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(blurShaderSrc, NULL, "HorizFilter", "cs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateComputeShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mHorizontalBlurCS));
	DX_SetDebugName(mHorizontalBlurCS, "Post FX - Horizontal Blur CS");
	SAFE_RELEASE(pShaderBlob);

	/////////////////////
	// PostFX shader   //
	/////////////////////
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

	// linear and point samplers
	D3D11_SAMPLER_DESC samDesc;
	ZeroMemory(&samDesc, sizeof(samDesc));
	samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samDesc.MaxAnisotropy = 1;
	samDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samDesc.MaxLOD = D3D11_FLOAT32_MAX;
	V_RETURN(device->CreateSamplerState(&samDesc, &mSampLinear));

	samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	V_RETURN(device->CreateSamplerState(&samDesc, &mSampPoint));
	return true;

}

void PostFX::Release()
{
	SAFE_RELEASE(mDownScaleRT);
	SAFE_RELEASE(mDownScaleSRV);
	SAFE_RELEASE(mDownScaleUAV);
	SAFE_RELEASE(mTempRT[0]);
	SAFE_RELEASE(mTempSRV[0]);
	SAFE_RELEASE(mTempUAV[0]);
	SAFE_RELEASE(mTempRT[1]);
	SAFE_RELEASE(mTempSRV[1]);
	SAFE_RELEASE(mTempUAV[1]);
	SAFE_RELEASE(mBloomRT);
	SAFE_RELEASE(mBloomSRV);
	SAFE_RELEASE(mBloomUAV);
	SAFE_RELEASE(mDownScale1DBuffer);
	SAFE_RELEASE(mDownScale1DUAV);
	SAFE_RELEASE(mDownScale1DSRV);
	SAFE_RELEASE(mDownScaleCB);
	SAFE_RELEASE(mFinalPassCB);
	SAFE_RELEASE(mBlurCB);
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
	SAFE_RELEASE(mBloomRevealCS);
	SAFE_RELEASE(mHorizontalBlurCS);
	SAFE_RELEASE(mVerticalBlurCS);
	SAFE_RELEASE(mSampPoint);
	SAFE_RELEASE(mSampLinear);
}

void PostFX::PostProcessing(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11RenderTargetView* pLDRRTV)
{
	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(mDownScaleCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TDownScaleCB* pDownScale = (TDownScaleCB*)MappedResource.pData;
	pDownScale->width = mWidth / 4;
	pDownScale->height = mHeight / 4;
	pDownScale->totalPixels = pDownScale->width * pDownScale->height;
	pDownScale->groupSize = mDownScaleGroups;
	pDownScale->adaptation = mAdaptation;
	pDownScale->bloomThreshold = mBloomThreshold;
	pd3dImmediateContext->Unmap(mDownScaleCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { mDownScaleCB };
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Down scale the HDR image
	ID3D11RenderTargetView* rt[1] = { NULL };
	pd3dImmediateContext->OMSetRenderTargets(1, rt, NULL);
	DownScale(pd3dImmediateContext, pHDRSRV);

	if (mEnableBloom)
	{
		// Bloom
		Bloom(pd3dImmediateContext);

		// Blur the bloom values
		Blur(pd3dImmediateContext, mTempSRV[0], mBloomUAV);
	}

	// Cleanup
	ZeroMemory(&arrConstBuffers, sizeof(arrConstBuffers));
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

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

void PostFX::SetParameters(float middleGrey, float white, float adaptation, float bloomThreshold, float bloomScale, bool enableBloom)
{
	mMiddleGrey = middleGrey;
	mWhite = white; 
	mAdaptation = adaptation;
	mBloomThreshold = bloomThreshold;
	mBloomScale = bloomScale;
	mEnableBloom = enableBloom;
}

void PostFX::DownScale(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV)
{
	// Output
	ID3D11UnorderedAccessView* arrUAVs[2] = { mDownScale1DUAV, mDownScaleUAV };
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, arrUAVs, NULL);

	// Input
	ID3D11ShaderResourceView* arrViews[2] = { pHDRSRV, NULL };
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);

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
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, arrUAVs, NULL);

	// Input
	arrViews[0] = mDownScale1DSRV;
	arrViews[1] = mPrevAvgLumSRV;
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader(mDownScaleSecondPassCS, NULL, 0);

	// Excute
	pd3dImmediateContext->Dispatch(1, 1, 1);

	// Cleanup
	pd3dImmediateContext->CSSetShader(NULL, NULL, 0);
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, arrUAVs, (UINT*)(&arrUAVs));
}

void PostFX::Bloom(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Input
	ID3D11ShaderResourceView* arrViews[2] = { mDownScaleSRV, mAvgLumSRV };
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);

	// Output
	ID3D11UnorderedAccessView* arrUAVs[1] = { mTempUAV[0] };
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);

	// Shader
	pd3dImmediateContext->CSSetShader(mBloomRevealCS, NULL, 0);

	// Execute the downscales first pass with enough groups to cover the entire full res HDR buffer
	pd3dImmediateContext->Dispatch(mDownScaleGroups, 1, 1);

	// Cleanup
	pd3dImmediateContext->CSSetShader(NULL, NULL, 0);
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);
}

void PostFX::Blur(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pInput, ID3D11UnorderedAccessView* pOutput)
{
	//////////////////////////////////////////
	// horizontal gaussian filter

	// Output
	ID3D11UnorderedAccessView* arrUAVs[1] = { mTempUAV[1] };
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);

	// Input
	ID3D11ShaderResourceView* arrViews[1] = { pInput };
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader(mHorizontalBlurCS, NULL, 0);

	// Execute the horizontal filter
	pd3dImmediateContext->Dispatch((UINT)ceil((mWidth / 4.0f) / (128.0f - 12.0f)), (UINT)ceil(mHeight / 4.0f), 1);

	////////////////////////////////////////////
	// vertical gaussian filter

	// Output
	arrUAVs[0] = pOutput;
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);

	// Input
	arrViews[0] = mTempSRV[1];
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader(mVerticalBlurCS, NULL, 0);

	// Execute the vertical filter
	pd3dImmediateContext->Dispatch((UINT)ceil(mWidth / 4.0f), (UINT)ceil((mHeight / 4.0f) / (128.0f - 12.0f)), 1);

	// Cleanup
	pd3dImmediateContext->CSSetShader(NULL, NULL, 0);
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);
}

void PostFX::FinalPass(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV)
{
	ID3D11ShaderResourceView* arrViews[3] = { pHDRSRV, mAvgLumSRV, mBloomSRV };
	pd3dImmediateContext->PSSetShaderResources(0, 3, arrViews);

	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(mFinalPassCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TFinalPassCB* pFinalPass = (TFinalPassCB*)MappedResource.pData;
	pFinalPass->MiddleGrey = mMiddleGrey;
	pFinalPass->LumWhiteSqr = mWhite;
	pFinalPass->LumWhiteSqr *= pFinalPass->MiddleGrey; // Scale by the middle grey value
	pFinalPass->LumWhiteSqr *= pFinalPass->LumWhiteSqr; // Square
	pFinalPass->BloomScale = mEnableBloom? mBloomScale: 0.0f;
	pd3dImmediateContext->Unmap(mFinalPassCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { mFinalPassCB };
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, arrConstBuffers);

	pd3dImmediateContext->IASetInputLayout(NULL);
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetIndexBuffer(NULL, DXGI_FORMAT_UNKNOWN, 0);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	ID3D11SamplerState* arrSamplers[2] = { mSampPoint, mSampLinear };
	pd3dImmediateContext->PSSetSamplers(0, 2, arrSamplers);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(mFullScreenQuadVS, NULL, 0);
	pd3dImmediateContext->PSSetShader(mFinalPassPS, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->PSSetShaderResources(0, 3, arrViews);
	ZeroMemory(arrConstBuffers, sizeof(arrConstBuffers));
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, arrConstBuffers);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
}
