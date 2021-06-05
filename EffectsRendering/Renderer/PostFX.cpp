#include "PostFX.h"

#include "Camera.h"
#include "TextureManager.h"

PostFX::PostFX() : mMiddleGrey(0.0025f), mWhite(1.5f), mBloomThreshold(2.0), mBloomScale(0.1f),
	mDownScaleRT(NULL), mDownScaleSRV(NULL), mDownScaleUAV(NULL),
	mBloomRT(NULL), mBloomSRV(NULL), mBloomUAV(NULL),
	mDownScale1DBuffer(NULL), mDownScale1DUAV(NULL), mDownScale1DSRV(NULL),
	mDownScaleCB(NULL), mFinalPassCB(NULL), mBlurCB(NULL),
	mAvgLumBuffer(NULL), mAvgLumUAV(NULL), mAvgLumSRV(NULL),
	mPrevAvgLumBuffer(NULL), mPrevAvgLumUAV(NULL), mPrevAvgLumSRV(NULL),
	mDownScaleFirstPassCS(NULL), mDownScaleSecondPassCS(NULL), mFullScreenQuadVS(NULL),
	mFinalPassPS(NULL), mSampPoint(NULL), mBloomRevealCS(NULL),
	mHorizontalBlurCS(NULL), mVerticalBlurCS(NULL), mSampLinear(NULL),
	mBokehHighlightSearchCS(NULL), mBokehVS(NULL), mBokehGS(NULL), mBokehPS(NULL), mBokehTexView(NULL),
	mAddativeBlendState(NULL), mBokehHightlightScanCB(NULL), mBokehRenderCB(NULL)
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

	// Allocate Bokeh Buffer
	const UINT nMaxBokehInst = 4056;
	bufferDesc.StructureByteStride = 7 * sizeof(float);
	bufferDesc.ByteWidth = nMaxBokehInst * bufferDesc.StructureByteStride;
	//bufferDesc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
	V_RETURN(device->CreateBuffer(&bufferDesc, NULL, &mBokehBuffer));
	DX_SetDebugName(mBokehBuffer, "PostFX - Bokeh Buffer");

	DescUAV.Buffer.NumElements = nMaxBokehInst;
	DescUAV.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
	V_RETURN(device->CreateUnorderedAccessView(mBokehBuffer, &DescUAV, &mBokehUAV));
	DX_SetDebugName(mBokehUAV, "PostFX - Bokeh UAV");

	dsrvd.Buffer.NumElements = nMaxBokehInst;
	V_RETURN(device->CreateShaderResourceView(mBokehBuffer, &dsrvd, &mBokehSRV));
	DX_SetDebugName(mBokehSRV, "PostFX - Bokeh SRV");

	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
	bufferDesc.ByteWidth = 16;

	D3D11_SUBRESOURCE_DATA initData;
	UINT bufferInit[4] = { 0, 1, 0, 0 };
	initData.pSysMem = bufferInit;
	initData.SysMemPitch = 0;
	initData.SysMemSlicePitch = 0;

	V_RETURN(device->CreateBuffer(&bufferDesc, &initData, &mBokehIndirectDrawBuffer));
	DX_SetDebugName(mBokehIndirectDrawBuffer, "PostFX - Bokeh Indirect Draw Buffer");

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

	bufferDesc.ByteWidth = sizeof(TBokehHightlightScanCB);
	V_RETURN(device->CreateBuffer(&bufferDesc, NULL, &mBokehHightlightScanCB));
	DX_SetDebugName(mBokehHightlightScanCB, "PostFX - Bokeh Hightlight Scan CB");

	bufferDesc.ByteWidth = sizeof(TBokehRenderCB);
	V_RETURN(device->CreateBuffer(&bufferDesc, NULL, &mBokehRenderCB));
	DX_SetDebugName(mBokehRenderCB, "PostFX - Bokeh Render CB");

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

	///////////////////
	// Bokeh shaders //
	///////////////////
	WCHAR bokehCSSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\BokehCS.hlsl";

	V_RETURN(CompileShader(bokehCSSrc, NULL, "HighlightScan", "cs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateComputeShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mBokehHighlightSearchCS));
	DX_SetDebugName(mBokehHighlightSearchCS, "Post FX - Bokeh Highlight Scan CS");
	SAFE_RELEASE(pShaderBlob);

	WCHAR bokehSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\Bokeh.hlsl";

	V_RETURN(CompileShader(bokehSrc, NULL, "BokehVS", "vs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mBokehVS));
	DX_SetDebugName(mBokehVS, "Post FX - Bokeh VS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(bokehSrc, NULL, "BokehGS", "gs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateGeometryShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mBokehGS));
	DX_SetDebugName(mBokehGS, "Post FX - Bokeh GS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(bokehSrc, NULL, "BokehPS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mBokehPS));
	DX_SetDebugName(mBokehPS, "Post FX - Bokeh PS");
	SAFE_RELEASE(pShaderBlob);

	// Load the bokeh highlight texture
	mBokehTexView = TextureManager::Instance()->CreateTexture("..\\Assets\\Bokeh.dds");

	// Blend state for the bokeh highlights
	D3D11_BLEND_DESC descBlend;
	descBlend.AlphaToCoverageEnable = FALSE;
	descBlend.IndependentBlendEnable = FALSE;
	const D3D11_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		TRUE,
		D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
		D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
		D3D11_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		descBlend.RenderTarget[i] = defaultRenderTargetBlendDesc;
	V_RETURN(device->CreateBlendState(&descBlend, &mAddativeBlendState));
	DX_SetDebugName(mAddativeBlendState, "Post FX - Addative Blending BS");

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
	SAFE_RELEASE(mBokehHighlightSearchCS);
	SAFE_RELEASE(mBokehVS);
	SAFE_RELEASE(mBokehGS);
	SAFE_RELEASE(mBokehPS);
	SAFE_RELEASE(mAddativeBlendState);
	SAFE_RELEASE(mBokehHightlightScanCB);
	SAFE_RELEASE(mBokehRenderCB);
}

void PostFX::PostProcessing(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11ShaderResourceView* depthSRV, ID3D11RenderTargetView* pLDRRTV, Camera* camera)
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
	float fQ = camera->GetFarZ() / (camera->GetFarZ() - camera->GetNearZ());
	pDownScale->ProjectionValues[0] = -camera->GetNearZ() * fQ;
	pDownScale->ProjectionValues[1] = -fQ;
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
	}

	// Cleanup
	ZeroMemory(&arrConstBuffers, sizeof(arrConstBuffers));
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Scan for the bokeh highlights
	BokehHightlightScan(pd3dImmediateContext, pHDRSRV, depthSRV, camera);

	// Do the final pass
	rt[0] = pLDRRTV;
	pd3dImmediateContext->OMSetRenderTargets(1, rt, NULL);
	FinalPass(pd3dImmediateContext, pHDRSRV, depthSRV, camera);

	// Draw the bokeh highlights on top of the image
	BokehRender(pd3dImmediateContext);

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

void PostFX::SetParameters(float middleGrey, float white, float adaptation, float bloomThreshold, float bloomScale, bool enableBloom, float DOFFarStart, float DOFFarRange,
	float bokehLumThreshold, float bokehBlurThreshold, float bokehRadiusScale, float bokehColorScale)
{
	mMiddleGrey = middleGrey;
	mWhite = white; 
	mAdaptation = adaptation;
	mBloomThreshold = bloomThreshold;
	mBloomScale = bloomScale;
	mEnableBloom = enableBloom;
	
	mDOFFarStart = DOFFarStart;
	mDOFFarRangeRcp = 1.0f / max(DOFFarRange, 0.001f);

	mBokehLumThreshold = bokehLumThreshold;
	mBokehBlurThreshold = bokehBlurThreshold;
	mBokehRadiusScale = bokehRadiusScale;
	mBokehColorScale = bokehColorScale;
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

	// Blur the bloom values
	Blur(pd3dImmediateContext, mTempSRV[0], mBloomUAV);
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

void PostFX::BokehHightlightScan(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11ShaderResourceView* pDepthSRV, Camera* camera)
{
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(mBokehHightlightScanCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TBokehHightlightScanCB* pBokehHightlightScan = (TBokehHightlightScanCB*)MappedResource.pData;
	pBokehHightlightScan->Width = mWidth;
	pBokehHightlightScan->Height = mHeight;
	float fQ = camera->GetFarZ() / (camera->GetFarZ() - camera->GetNearZ());
	pBokehHightlightScan->ProjectionValues[0] = -camera->GetNearZ() * fQ;
	pBokehHightlightScan->ProjectionValues[1] = -fQ;
	pBokehHightlightScan->DOFFarStart = mDOFFarStart;
	pBokehHightlightScan->DOFFarRangeRcp = mDOFFarRangeRcp;
	pBokehHightlightScan->MiddleGrey = mMiddleGrey;
	pBokehHightlightScan->LumWhiteSqr = mWhite;
	pBokehHightlightScan->LumWhiteSqr *= pBokehHightlightScan->MiddleGrey; // Scale by the middle gray value
	pBokehHightlightScan->LumWhiteSqr *= pBokehHightlightScan->LumWhiteSqr; // Square
	pBokehHightlightScan->BokehBlurThreshold = mBokehBlurThreshold;
	pBokehHightlightScan->BokehLumThreshold = mBokehLumThreshold;
	pBokehHightlightScan->RadiusScale = mBokehRadiusScale;
	pBokehHightlightScan->ColorScale = mBokehColorScale;
	pd3dImmediateContext->Unmap(mBokehHightlightScanCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { mBokehHightlightScanCB };
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Output
	ID3D11UnorderedAccessView* arrUAVs[1] = { mBokehUAV };
	UINT nCount = 0; // Indicate we want to flush everything already in the buffer
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, &nCount);

	// Input
	ID3D11ShaderResourceView* arrViews[3] = { pHDRSRV, pDepthSRV, mAvgLumSRV };
	pd3dImmediateContext->CSSetShaderResources(0, 3, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader(mBokehHighlightSearchCS, NULL, 0);

	// Execute the horizontal filter
	pd3dImmediateContext->Dispatch((UINT)ceil((float)(mWidth * mHeight) / 1024.0f), 1, 1);

	// Cleanup
	pd3dImmediateContext->CSSetShader(NULL, NULL, 0);
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->CSSetShaderResources(0, 3, arrViews);
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);
	ZeroMemory(&arrConstBuffers, sizeof(arrConstBuffers));
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);
}

void PostFX::FinalPass(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11ShaderResourceView* depthSRV, Camera* camera)
{
	ID3D11ShaderResourceView* arrViews[6] = { pHDRSRV, mAvgLumSRV, mBloomSRV, mDownScaleSRV, depthSRV };
	pd3dImmediateContext->PSSetShaderResources(0, 6, arrViews);

	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(mFinalPassCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TFinalPassCB* pFinalPass = (TFinalPassCB*)MappedResource.pData;
	pFinalPass->MiddleGrey = mMiddleGrey;
	pFinalPass->LumWhiteSqr = mWhite;
	pFinalPass->LumWhiteSqr *= pFinalPass->MiddleGrey; // Scale by the middle grey value
	pFinalPass->LumWhiteSqr *= pFinalPass->LumWhiteSqr; // Square
	pFinalPass->BloomScale = mEnableBloom? mBloomScale: 0.0f;
	float fQ = camera->GetFarZ() / (camera->GetFarZ() - camera->GetNearZ());
	pFinalPass->ProjectionValues[0] = -camera->GetNearZ() * fQ;
	pFinalPass->ProjectionValues[1] = -fQ;
	pFinalPass->DOFFarStart = mDOFFarStart;
	pFinalPass->DOFFarRangeRcp = mDOFFarRangeRcp;
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
	pd3dImmediateContext->PSSetShaderResources(0, 6, arrViews);
	ZeroMemory(arrConstBuffers, sizeof(arrConstBuffers));
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, arrConstBuffers);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
}

void PostFX::BokehRender(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Copy the amount of appended highlights
	pd3dImmediateContext->CopyStructureCount(mBokehIndirectDrawBuffer, 0, mBokehUAV);

	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(mBokehRenderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TBokehRenderCB* pBokehRender = (TBokehRenderCB*)MappedResource.pData;
	pBokehRender->AspectRatio[0] = 1.0f;
	pBokehRender->AspectRatio[1] = (float)mWidth / (float)mHeight;
	pd3dImmediateContext->Unmap(mBokehRenderCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { mBokehRenderCB };
	pd3dImmediateContext->GSSetConstantBuffers(0, 1, arrConstBuffers);

	ID3D11BlendState* pPrevBlendState;
	FLOAT prevBlendFactor[4];
	UINT prevSampleMask;
	pd3dImmediateContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
	pd3dImmediateContext->OMSetBlendState(mAddativeBlendState, prevBlendFactor, prevSampleMask);

	ID3D11ShaderResourceView* arrViews[1] = { mBokehSRV };
	pd3dImmediateContext->VSSetShaderResources(0, 1, arrViews);

	arrViews[0] = mBokehTexView;
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);

	pd3dImmediateContext->IASetInputLayout(NULL);
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetIndexBuffer(NULL, DXGI_FORMAT_UNKNOWN, 0);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	ID3D11SamplerState* arrSamplers[1] = { mSampLinear };
	pd3dImmediateContext->PSSetSamplers(0, 1, arrSamplers);

	pd3dImmediateContext->VSSetShader(mBokehVS, NULL, 0);
	pd3dImmediateContext->GSSetShader(mBokehGS, NULL, 0);
	pd3dImmediateContext->PSSetShader(mBokehPS, NULL, 0);

	pd3dImmediateContext->DrawInstancedIndirect(mBokehIndirectDrawBuffer, 0);

	// Cleanup
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->VSSetShaderResources(0, 1, arrViews);
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);
	pd3dImmediateContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
	arrConstBuffers[0] = NULL;
	pd3dImmediateContext->GSSetConstantBuffers(0, 1, arrConstBuffers);
}
