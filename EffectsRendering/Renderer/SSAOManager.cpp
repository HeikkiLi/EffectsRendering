#include "SSAOManager.h"


SSAOManager::SSAOManager() : mDownscaleCB(NULL),
mSSAO_RT(NULL), mSSAO_SRV(NULL), mSSAO_UAV(NULL),
mMiniDepthBuffer(NULL), mMiniDepthUAV(NULL), mMiniDepthSRV(NULL),
mDepthDownscaleCS(NULL), mComputeCS(NULL)
{

}

SSAOManager::~SSAOManager()
{

}

HRESULT SSAOManager::Init(ID3D11Device* device, UINT width, UINT height)
{
	HRESULT hr;

	mWidth = width / 2;
	mHeight = height / 2;

	// Allocate SSAO
	D3D11_TEXTURE2D_DESC t2dDesc = {
		mWidth, //UINT Width;
		mHeight, //UINT Height;
		1, //UINT MipLevels;
		1, //UINT ArraySize;
		DXGI_FORMAT_R32_TYPELESS,//DXGI_FORMAT_R8_TYPELESS, //DXGI_FORMAT Format;
		1, //DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
		0,//UINT CPUAccessFlags;
		0//UINT MiscFlags;    
	};
	V_RETURN(device->CreateTexture2D(&t2dDesc, NULL, &mSSAO_RT));
	DX_SetDebugName(mSSAO_RT, "SSAO - Final AO values");

	// Create the UAVs
	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
	ZeroMemory(&UAVDesc, sizeof(UAVDesc));
	UAVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	V_RETURN(device->CreateUnorderedAccessView(mSSAO_RT, &UAVDesc, &mSSAO_UAV));
	DX_SetDebugName(mSSAO_UAV, "SSAO - Final AO values UAV");

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory(&SRVDesc, sizeof(SRVDesc));
	SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;
	V_RETURN(device->CreateShaderResourceView(mSSAO_RT, &SRVDesc, &mSSAO_SRV));
	DX_SetDebugName(mMiniDepthSRV, "SSAO - Final AO values SRV");

	// Allocate down scaled depth buffer
	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.StructureByteStride = 4 * sizeof(float);
	bufferDesc.ByteWidth = mWidth * mHeight * bufferDesc.StructureByteStride;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	V_RETURN(device->CreateBuffer(&bufferDesc, NULL, &mMiniDepthBuffer));
	DX_SetDebugName(mMiniDepthBuffer, "SSAO - Downscaled Depth Buffer");

	ZeroMemory(&UAVDesc, sizeof(UAVDesc));
	UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	UAVDesc.Buffer.FirstElement = 0;
	UAVDesc.Buffer.NumElements = mWidth * mHeight;
	V_RETURN(device->CreateUnorderedAccessView(mMiniDepthBuffer, &UAVDesc, &mMiniDepthUAV));
	DX_SetDebugName(mMiniDepthUAV, "SSAO - Downscaled Depth UAV");

	ZeroMemory(&SRVDesc, sizeof(SRVDesc));
	SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SRVDesc.Buffer.FirstElement = 0;
	SRVDesc.Buffer.NumElements = mWidth * mHeight;
	V_RETURN(device->CreateShaderResourceView(mMiniDepthBuffer, &SRVDesc, &mMiniDepthSRV));
	DX_SetDebugName(mMiniDepthSRV, "SSAO - Downscaled Depth SRV");

	// Allocate down scale depth constant buffer
	D3D11_BUFFER_DESC CBDesc;
	ZeroMemory(&CBDesc, sizeof(CBDesc));
	CBDesc.Usage = D3D11_USAGE_DYNAMIC;
	CBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	CBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	CBDesc.ByteWidth = sizeof(TDownscaleCB);
	V_RETURN(device->CreateBuffer(&CBDesc, NULL, &mDownscaleCB));
	DX_SetDebugName(mDownscaleCB, "SSAO - Downscale Depth CB");

	// Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;// | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if defined( DEBUG ) || defined( _DEBUG )
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	WCHAR ssaoSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\SSAO.hlsl";

	ID3DBlob* pShaderBlob = NULL;

	V_RETURN(CompileShader(ssaoSrc, NULL, "DepthDownscale", "cs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateComputeShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mDepthDownscaleCS));
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(ssaoSrc, NULL, "SSAOCompute", "cs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateComputeShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mComputeCS));
	SAFE_RELEASE(pShaderBlob);

	return hr;
}

void SSAOManager::Deinit()
{
	SAFE_RELEASE(mDownscaleCB);
	SAFE_RELEASE(mSSAO_RT);
	SAFE_RELEASE(mSSAO_SRV);
	SAFE_RELEASE(mSSAO_UAV);
	SAFE_RELEASE(mMiniDepthBuffer);
	SAFE_RELEASE(mMiniDepthUAV);
	SAFE_RELEASE(mMiniDepthSRV);
	SAFE_RELEASE(mDepthDownscaleCS);
	SAFE_RELEASE(mComputeCS);
}

void SSAOManager::Compute(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pDepthSRV, ID3D11ShaderResourceView* pNormalsSRV, Camera* camera, PostFX* postFx)
{
	DownscaleDepth(pd3dImmediateContext, pDepthSRV, pNormalsSRV, camera);
	ComputeSSAO(pd3dImmediateContext);
	Blur(pd3dImmediateContext, postFx);
}

void SSAOManager::DownscaleDepth(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pDepthSRV, ID3D11ShaderResourceView* pNormalsSRV, Camera* camera)
{
	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(mDownscaleCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TDownscaleCB* pDownscale = (TDownscaleCB*)MappedResource.pData;
	pDownscale->Width = mWidth;
	pDownscale->Height = mHeight;
	pDownscale->HorResRcp = 1.0f / (float)pDownscale->Width;
	pDownscale->VerResRcp = 1.0f / (float)pDownscale->Height;
	XMFLOAT4X4 pProj;
	XMStoreFloat4x4(&pProj, camera->Proj());
	pDownscale->ProjParams.x = 1.0f / pProj.m[0][0];
	pDownscale->ProjParams.y = 1.0f / pProj.m[1][1];
	float fQ = camera->GetFarZ() / (camera->GetFarZ() - camera->GetNearZ());
	pDownscale->ProjParams.z = -camera->GetNearZ() * fQ;
	pDownscale->ProjParams.w = -fQ;
	pDownscale->ViewMatrix = XMMatrixTranspose(camera->View());
	pDownscale->OffsetRadius = (float)mSSAOSampRadius;
	pDownscale->Radius = mRadius;
	pDownscale->MaxDepth = camera->GetFarZ();
	pd3dImmediateContext->Unmap(mDownscaleCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { mDownscaleCB };
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Output
	ID3D11UnorderedAccessView* arrUAVs[1] = { mMiniDepthUAV };
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);

	// Input
	ID3D11ShaderResourceView* arrViews[2] = { pDepthSRV, pNormalsSRV };
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader(mDepthDownscaleCS, NULL, 0);

	// Execute the downscales first pass with enough groups to cover the entire full res HDR buffer
	pd3dImmediateContext->Dispatch((UINT)ceil((float)(mWidth * mHeight) / 1024.0f), 1, 1);

	// Cleanup
	pd3dImmediateContext->CSSetShader(NULL, NULL, 0);
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);
	ZeroMemory(arrConstBuffers, sizeof(arrConstBuffers));
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);
}

void SSAOManager::ComputeSSAO(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Constants
	ID3D11Buffer* arrConstBuffers[1] = { mDownscaleCB };
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Output
	ID3D11UnorderedAccessView* arrUAVs[1] = { mSSAO_UAV };
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);

	// Input
	ID3D11ShaderResourceView* arrViews[1] = { mMiniDepthSRV };
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader(mComputeCS, NULL, 0);

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

void SSAOManager::Blur(ID3D11DeviceContext* pd3dImmediateContext, PostFX* postFx)
{
	//postFx->Blur(pd3dImmediateContext, mSSAO_SRV, mSSAO_UAV);
}