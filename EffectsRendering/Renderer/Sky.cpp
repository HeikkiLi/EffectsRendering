#include "Sky.h"
#include "Mesh.h"
#include "GeometryGenerator.h"
#include "TextureManager.h"
#include "Camera.h"


#pragma pack(push,1)
struct CB_VS_PER_FRAME
{
	XMMATRIX mWorldViewProjection;
};

struct CB_EMISSIVE
{
	XMMATRIX WolrdViewProj;
	XMFLOAT3 Color;
	float pad;
};
#pragma pack(pop)

Sky::Sky()
{
	mCubeMapSRV = NULL;
	mIndexCount = 0;
	mVB = NULL;
	mIB = NULL;
	mSkyPixelShader = NULL;
	mSkyVertexShader = NULL;
	mSkyVSLayout = NULL;
	mSkyVertexShaderCB = NULL;
	mSkyNoDepthStencilMaskState = NULL;
	mCullNone = NULL;
	mSunRadius = 10.0f;

	mEmissiveCB = NULL;
	mEmissiveVertexShader = NULL;
	mEmissiveVSLayout = NULL;
	mEmissivePixelShader = NULL;

	mSunSphere = NULL;
}

bool Sky::Init(ID3D11Device* device, const std::string& cubemapFilename, float skySphereRadius, float sunRadius)
{
	HRESULT hr;

	mSunRadius = sunRadius;

	// load cubemap from file
	mCubeMapSRV = TextureManager::Instance()->CreateTexture(cubemapFilename);

	if (mCubeMapSRV == NULL)
	{
		return false;
	}

	MeshData sphere;
	GeometryGenerator::Instance()->CreateSphere(skySphereRadius, 32, 32, sphere);

	// Sphere Vertex Buffer
	std::vector<XMFLOAT3> vertices(sphere.Vertices.size());
	for (size_t i = 0; i < sphere.Vertices.size(); ++i)
	{
		vertices[i] = sphere.Vertices[i].Position;
	}

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(XMFLOAT3) * vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &vertices[0];

	HR(device->CreateBuffer(&vbd, &vinitData, &mVB));

	// Index Buffer
	mIndexCount = sphere.Indices.size();

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(USHORT) * mIndexCount;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.StructureByteStride = 0;
	ibd.MiscFlags = 0;

	std::vector<USHORT> indices16;
	indices16.assign(sphere.Indices.begin(), sphere.Indices.end());

	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &indices16[0];

	HR(device->CreateBuffer(&ibd, &iinitData, &mIB));


	// Create constant buffers
	D3D11_BUFFER_DESC cbDesc;
	ZeroMemory(&cbDesc, sizeof(cbDesc));
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.ByteWidth = sizeof(CB_VS_PER_FRAME);
	if (FAILED(device->CreateBuffer(&cbDesc, NULL, &mSkyVertexShaderCB)))
		return false;

	cbDesc.ByteWidth = sizeof(CB_EMISSIVE);
	V_RETURN(device->CreateBuffer(&cbDesc, NULL, &mEmissiveCB));
	DX_SetDebugName(mEmissiveCB, "Emissive CB");

	// create Sky shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	WCHAR str[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\Sky.hlsl";

	ID3DBlob* pShaderBlob = NULL;
	if (FAILED(CompileShader(str, NULL, "SkyVS", "vs_5_0", dwShaderFlags, &pShaderBlob)))
		return false;

	if (FAILED(device->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mSkyVertexShader)))
	{
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC layout[1] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	hr = device->CreateInputLayout(layout, 1, pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), &mSkyVSLayout);
	SAFE_RELEASE(pShaderBlob);
	if (FAILED(hr))
		return false;

	if (FAILED(CompileShader(str, NULL, "SkyPS", "ps_5_0", dwShaderFlags, &pShaderBlob)))
		return false;
	hr = device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mSkyPixelShader);
	SAFE_RELEASE(pShaderBlob);
	if (FAILED(hr))
		return false;


	// Emissive shaders
	WCHAR skyShaderSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\Emissive.hlsl";

	V_RETURN(CompileShader(skyShaderSrc, NULL, "RenderEmissiveVS", "vs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mEmissiveVertexShader));
	DX_SetDebugName(mEmissiveVertexShader, "RenderEmissiveVS");

	// Create a layout for the object data
	const D3D11_INPUT_ELEMENT_DESC layoutEmissive[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	V_RETURN(device->CreateInputLayout(layoutEmissive, ARRAYSIZE(layoutEmissive), pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), &mEmissiveVSLayout));
	DX_SetDebugName(mEmissiveVSLayout, "Emissive Layout");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(skyShaderSrc, NULL, "RenderEmissivePS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mEmissivePixelShader));
	DX_SetDebugName(mEmissivePixelShader, "RenderEmissivePS");
	SAFE_RELEASE(pShaderBlob);


	D3D11_DEPTH_STENCIL_DESC descDepth;
	descDepth.DepthEnable = FALSE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	descDepth.StencilEnable = TRUE;
	descDepth.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	descDepth.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	const D3D11_DEPTH_STENCILOP_DESC noSkyStencilOp = { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL };
	descDepth.FrontFace = noSkyStencilOp;
	descDepth.BackFace = noSkyStencilOp;
	V_RETURN(device->CreateDepthStencilState(&descDepth, &mSkyNoDepthStencilMaskState));
	DX_SetDebugName(mSkyNoDepthStencilMaskState, "Sky No Depth Stencil Mask DS");

	D3D11_RASTERIZER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.FillMode = D3D11_FILL_SOLID;
	desc.CullMode = D3D11_CULL_NONE;
	desc.ScissorEnable = true;
	desc.DepthClipEnable = true;
	V_RETURN(device->CreateRasterizerState(&desc, &mCullNone));

	return true;
}

Sky::~Sky()
{
	ReleaseCOM(mVB);
	ReleaseCOM(mIB);
	ReleaseCOM(mCubeMapSRV);

	SAFE_RELEASE(mSkyPixelShader);
	SAFE_RELEASE(mSkyVertexShader);
	SAFE_RELEASE(mSkyVSLayout);
	SAFE_RELEASE(mSkyVertexShaderCB);

	SAFE_RELEASE(mSkyNoDepthStencilMaskState);
	SAFE_RELEASE(mCullNone);

	SAFE_RELEASE(mEmissiveCB);
	SAFE_RELEASE(mEmissiveVertexShader);
	SAFE_RELEASE(mEmissiveVSLayout);
	SAFE_RELEASE(mEmissivePixelShader);
}

ID3D11ShaderResourceView* Sky::CubeMapSRV()
{
	return mCubeMapSRV;
}

void Sky::Render(ID3D11DeviceContext* deviceContext, const Camera* camera, XMFLOAT3 sunDirection, XMFLOAT3 sunColor)
{
	// Store the previous depth state
	ID3D11DepthStencilState* pPrevDepthState;
	UINT nPrevStencil;
	deviceContext->OMGetDepthStencilState(&pPrevDepthState, &nPrevStencil);

	// Set the depth state for the sky rendering
	deviceContext->OMSetDepthStencilState(mSkyNoDepthStencilMaskState, 1);


	XMMATRIX lightWorldScale = XMMatrixScaling(mSunRadius, mSunRadius, mSunRadius);
	
	const XMFLOAT3 eyePos = camera->GetPosition();
	XMMATRIX lightWorldTrans = XMMatrixTranslation(eyePos.x - 200.0f * sunDirection.x, -200.0f * sunDirection.y, eyePos.z - 200.0f * sunDirection.z);
	const XMMATRIX view =  camera->View();
	const XMMATRIX proj = camera->Proj();
	XMMATRIX worldViewProjection = lightWorldScale * lightWorldTrans * view * proj;

	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V(deviceContext->Map(mEmissiveCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	CB_EMISSIVE* pEmissiveCB = (CB_EMISSIVE*)MappedResource.pData;
	pEmissiveCB->WolrdViewProj = XMMatrixTranspose(worldViewProjection);
	pEmissiveCB->Color =sunColor;
	deviceContext->Unmap(mEmissiveCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { mEmissiveCB };
	deviceContext->VSSetConstantBuffers(2, 1, arrConstBuffers);
	deviceContext->PSSetConstantBuffers(2, 1, arrConstBuffers);

	// Set the vertex layout
	deviceContext->IASetInputLayout(mEmissiveVSLayout);

	// Set the shaders
	deviceContext->VSSetShader(mEmissiveVertexShader, NULL, 0);
	deviceContext->PSSetShader(mEmissivePixelShader, NULL, 0);

	// This is an over kill for rendering the sun but it works
	mSunSphere->Render(deviceContext);

	// Cleanup
	deviceContext->VSSetShader(NULL, NULL, 0);
	deviceContext->PSSetShader(NULL, NULL, 0);

	// Restore the states
	deviceContext->OMSetDepthStencilState(pPrevDepthState, nPrevStencil);
	SAFE_RELEASE(pPrevDepthState);
}

void Sky::RenderSkyBox(ID3D11DeviceContext* deviceContext, const Camera* camera, XMFLOAT3 sunDirection, XMFLOAT3 sunColor)
{

	// Store the previous depth state
	ID3D11DepthStencilState* pPrevDepthState;
	UINT nPrevStencil;
	ID3D11RasterizerState* nPrevRState;
	deviceContext->OMGetDepthStencilState(&pPrevDepthState, &nPrevStencil);
	deviceContext->RSGetState(&nPrevRState);

	// Set the depth state for the sky rendering
	deviceContext->OMSetDepthStencilState(mSkyNoDepthStencilMaskState, 1);
	//deviceContext->RSSetState(mCullNone);

	XMFLOAT3 eyePos = camera->GetPosition();
	XMMATRIX T = XMMatrixTranslation(eyePos.x, eyePos.y, eyePos.z);
	XMMATRIX WVP = XMMatrixMultiply(T, camera->ViewProj());


	// Set the constant buffers
	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	HR(deviceContext->Map(mSkyVertexShaderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	CB_VS_PER_FRAME* pVSPerObject = (CB_VS_PER_FRAME*)MappedResource.pData;
	pVSPerObject->mWorldViewProjection = WVP;
	deviceContext->Unmap(mSkyVertexShaderCB, 0);
	deviceContext->VSSetConstantBuffers(0, 1, &mSkyVertexShaderCB);

	// set the cubemap shader resource view
	deviceContext->PSSetShaderResources(4, 1, &mCubeMapSRV);

	
	UINT stride = sizeof(XMFLOAT3);
	UINT offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &mVB, &stride, &offset);
	deviceContext->IASetIndexBuffer(mIB, DXGI_FORMAT_R16_UINT, 0);
	deviceContext->IASetInputLayout(mSkyVSLayout);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set the shaders
	deviceContext->VSSetShader(mSkyVertexShader, NULL, 0);
	deviceContext->PSSetShader(mSkyPixelShader, NULL, 0);

	// render
	deviceContext->DrawIndexed(mIndexCount, 0, 0);

	// Set the shaders
	deviceContext->VSSetShader(NULL, NULL, 0);
	deviceContext->PSSetShader(NULL, NULL, 0);


	// restore the state
	deviceContext->OMSetDepthStencilState(pPrevDepthState, nPrevStencil);
	deviceContext->RSSetState(nPrevRState);
	SAFE_RELEASE(nPrevRState);
	SAFE_RELEASE(pPrevDepthState);

}

