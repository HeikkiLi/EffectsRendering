#include "GBuffer.h"

#include <wincodec.h>
#include "ScreenGrab.h"

#pragma pack(push,1)
struct CB_GBUFFER_UNPACK
{
	XMFLOAT4 PerspectiveValues;
	XMMATRIX ViewInv;
};
#pragma pack(pop)

GBuffer::GBuffer() : mpGBufferUnpackCB(NULL), mDepthStencilRT(NULL), mColorSpecIntensityRT(NULL), mNormalRT(NULL), mSpecPowerRT(NULL),
mDepthStencilDSV(NULL), mDepthStencilReadOnlyDSV(NULL), mColorSpecIntensityRTV(NULL), mNormalRTV(NULL), mSpecPowerRTV(NULL),
mDepthStencilSRV(NULL), mColorSpecIntensitySRV(NULL), mNormalSRV(NULL), mSpecPowerSRV(NULL),
mDepthStencilState(NULL)
{

}

GBuffer::~GBuffer()
{

}

bool GBuffer::Init(ID3D11Device* device, UINT width, UINT height)
{
	HRESULT hr;

	// Clear the previous targets
	Release(); 

	// Texture formats
	static const DXGI_FORMAT depthStencilTextureFormat = DXGI_FORMAT_R24G8_TYPELESS;
	static const DXGI_FORMAT basicColorTextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT normalTextureFormat = DXGI_FORMAT_R11G11B10_FLOAT;
	static const DXGI_FORMAT specPowTextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Render view formats
	static const DXGI_FORMAT depthStencilRenderViewFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	static const DXGI_FORMAT basicColorRenderViewFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT normalRenderViewFormat = DXGI_FORMAT_R11G11B10_FLOAT;
	static const DXGI_FORMAT specPowRenderViewFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Resource view formats
	static const DXGI_FORMAT depthStencilResourceViewFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	static const DXGI_FORMAT basicColorResourceViewFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT normalResourceViewFormat = DXGI_FORMAT_R11G11B10_FLOAT;
	static const DXGI_FORMAT specPowResourceViewFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Allocate the depth stencil target
	D3D11_TEXTURE2D_DESC dtd = {
		width, //UINT Width;
		height, //UINT Height;
		1, //UINT MipLevels;
		1, //UINT ArraySize;
		DXGI_FORMAT_UNKNOWN, //DXGI_FORMAT Format;
		1, //DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
		0,//UINT CPUAccessFlags;
		0//UINT MiscFlags;    
	};
	dtd.Format = depthStencilTextureFormat;
	V_RETURN(device->CreateTexture2D(&dtd, NULL, &mDepthStencilRT));

	// Allocate the base color with specular intensity target
	dtd.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	dtd.Format = basicColorTextureFormat;
	V_RETURN(device->CreateTexture2D(&dtd, NULL, &mColorSpecIntensityRT));

	// Allocate the base color with specular intensity target
	dtd.Format = normalTextureFormat;
	V_RETURN(device->CreateTexture2D(&dtd, NULL, &mNormalRT));

	// Allocate the specular power target
	dtd.Format = specPowTextureFormat;
	V_RETURN(device->CreateTexture2D(&dtd, NULL, &mSpecPowerRT));

	// Create the render target views
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvd =
	{
		depthStencilRenderViewFormat,
		D3D11_DSV_DIMENSION_TEXTURE2D,
		0
	};
	V_RETURN(device->CreateDepthStencilView(mDepthStencilRT, &dsvd, &mDepthStencilDSV));

	dsvd.Flags = D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL;
	V_RETURN(device->CreateDepthStencilView(mDepthStencilRT, &dsvd, &mDepthStencilReadOnlyDSV));

	D3D11_RENDER_TARGET_VIEW_DESC rtsvd =
	{
		basicColorRenderViewFormat,
		D3D11_RTV_DIMENSION_TEXTURE2D
	};
	V_RETURN(device->CreateRenderTargetView(mColorSpecIntensityRT, &rtsvd, &mColorSpecIntensityRTV));

	rtsvd.Format = normalRenderViewFormat;
	V_RETURN(device->CreateRenderTargetView(mNormalRT, &rtsvd, &mNormalRTV));

	rtsvd.Format = specPowRenderViewFormat;
	V_RETURN(device->CreateRenderTargetView(mSpecPowerRT, &rtsvd, &mSpecPowerRTV));

	// Create the resource views
	D3D11_SHADER_RESOURCE_VIEW_DESC dsrvd =
	{
		depthStencilResourceViewFormat,
		D3D11_SRV_DIMENSION_TEXTURE2D,
		0,
		0
	};
	dsrvd.Texture2D.MipLevels = 1;
	V_RETURN(device->CreateShaderResourceView(mDepthStencilRT, &dsrvd, &mDepthStencilSRV));

	dsrvd.Format = basicColorResourceViewFormat;
	V_RETURN(device->CreateShaderResourceView(mColorSpecIntensityRT, &dsrvd, &mColorSpecIntensitySRV));

	dsrvd.Format = normalResourceViewFormat;
	V_RETURN(device->CreateShaderResourceView(mNormalRT, &dsrvd, &mNormalSRV));

	dsrvd.Format = specPowResourceViewFormat;
	V_RETURN(device->CreateShaderResourceView(mSpecPowerRT, &dsrvd, &mSpecPowerSRV));

	D3D11_DEPTH_STENCIL_DESC descDepth;
	descDepth.DepthEnable = TRUE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	descDepth.StencilEnable = TRUE;
	descDepth.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	descDepth.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	const D3D11_DEPTH_STENCILOP_DESC stencilMarkOp = { D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_COMPARISON_ALWAYS };
	descDepth.FrontFace = stencilMarkOp;
	descDepth.BackFace = stencilMarkOp;
	V_RETURN(device->CreateDepthStencilState(&descDepth, &mDepthStencilState));

	// Create constant buffers
	D3D11_BUFFER_DESC cbDesc;
	ZeroMemory(&cbDesc, sizeof(cbDesc));
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.ByteWidth = sizeof(CB_GBUFFER_UNPACK);
	V_RETURN(device->CreateBuffer(&cbDesc, NULL, &mpGBufferUnpackCB));

	return S_OK;
}

void GBuffer::Release()
{
	SAFE_RELEASE(mpGBufferUnpackCB);

	// Clear all allocated targets
	SAFE_RELEASE(mDepthStencilRT);
	SAFE_RELEASE(mColorSpecIntensityRT);
	SAFE_RELEASE(mNormalRT);
	SAFE_RELEASE(mSpecPowerRT);

	// Clear all views
	SAFE_RELEASE(mDepthStencilDSV);
	SAFE_RELEASE(mDepthStencilReadOnlyDSV);
	SAFE_RELEASE(mColorSpecIntensityRTV);
	SAFE_RELEASE(mNormalRTV);
	SAFE_RELEASE(mSpecPowerRTV);
	SAFE_RELEASE(mDepthStencilSRV);
	SAFE_RELEASE(mColorSpecIntensitySRV);
	SAFE_RELEASE(mNormalSRV);
	SAFE_RELEASE(mSpecPowerSRV);

	// Clear the depth stencil state
	SAFE_RELEASE(mDepthStencilState);
}

void GBuffer::PreRender(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Clear the depth stencil
	pd3dImmediateContext->ClearDepthStencilView(mDepthStencilDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0, 1);

	// only need to do this if scene doesn't cover the whole visible area
	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	pd3dImmediateContext->ClearRenderTargetView(mColorSpecIntensityRTV, ClearColor);
	pd3dImmediateContext->ClearRenderTargetView(mNormalRTV, ClearColor);
	pd3dImmediateContext->ClearRenderTargetView(mSpecPowerRTV, ClearColor);

	// Bind all the render targets together
	ID3D11RenderTargetView* rt[3] = { mColorSpecIntensityRTV, mNormalRTV, mSpecPowerRTV };
	pd3dImmediateContext->OMSetRenderTargets(3, rt, mDepthStencilDSV);

	pd3dImmediateContext->OMSetDepthStencilState(mDepthStencilState, 2);
}

void GBuffer::PostRender(ID3D11DeviceContext* pd3dImmediateContext)
{
	pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Little cleanup
	ID3D11RenderTargetView* rt[3] = { NULL, NULL, NULL };
	pd3dImmediateContext->OMSetRenderTargets(3, rt, mDepthStencilReadOnlyDSV);
}

void GBuffer::PrepareForUnpack(ID3D11DeviceContext* pd3dImmediateContext, Camera* camera)
{
	HRESULT hr;

	// Fill the GBuffer unpack constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	HR(pd3dImmediateContext->Map(mpGBufferUnpackCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	CB_GBUFFER_UNPACK* pGBufferUnpackCB = (CB_GBUFFER_UNPACK*)MappedResource.pData;
	XMFLOAT4X4 proj;
	XMStoreFloat4x4(&proj, camera->Proj());
	pGBufferUnpackCB->PerspectiveValues.x = 1.0f / proj.m[0][0];
	pGBufferUnpackCB->PerspectiveValues.y = 1.0f / proj.m[1][1];
	pGBufferUnpackCB->PerspectiveValues.z = proj.m[3][2];
	pGBufferUnpackCB->PerspectiveValues.w = -proj.m[2][2];
	XMVECTOR pDeterminant;
	XMMATRIX matViewInv = XMMatrixInverse(&pDeterminant, camera->View());
	pGBufferUnpackCB->ViewInv = XMMatrixTranspose(matViewInv);
	pd3dImmediateContext->Unmap(mpGBufferUnpackCB, 0);

	pd3dImmediateContext->PSSetConstantBuffers(0, 1, &mpGBufferUnpackCB);
}
