#include "Util.h"
#include "GBuffer.h"
#include "Camera.h"
#include "LightManager.h"

const XMFLOAT3 GammaToLinear(const XMFLOAT3& color)
{
	return XMFLOAT3(color.x * color.x, color.y * color.y, color.z * color.z);
}

// Constant buffer for Directional Light
#pragma pack(push,1)
struct CB_DIRECTIONAL
{
	
	XMFLOAT3 vAmbientLower;
	float pad;
	XMFLOAT3 vAmbientRange;
	float pad2;
	XMFLOAT3 vDirToLight;
	float pad3;
	XMFLOAT3 vDirectionalColor;
	float pad4;
	XMMATRIX ToShadowSpace;
	XMFLOAT4 ToCascadeSpace[3];
	int enableFog;
};

struct CB_POINT_LIGHT_DOMAIN
{
	XMMATRIX WorldViewProj;
};

struct CB_POINT_LIGHT_PIXEL
{
	XMFLOAT3 PointLightPos;
	float PointLightRangeRcp;
	XMFLOAT3 PointColor;
	float pad;
	XMFLOAT2 LightPerspectiveValues;
	float pad2[2];
};

struct CB_SPOT_LIGHT_DOMAIN
{
	XMMATRIX WolrdViewProj;
	float fSinAngle;
	float fCosAngle;
	float pad[2];
};

struct CB_SPOT_LIGHT_PIXEL
{
	XMFLOAT3 SpotLightPos;
	float SpotLightRangeRcp;
	XMFLOAT3 vDirToLight;
	float SpotCosOuterCone;
	XMFLOAT3 SpotColor;
	float SpotCosConeAttRange;
	XMMATRIX ToShadowmap;
};
#pragma pack(pop)


const float LightManager::mShadowNear = 1.0f;

LightManager::LightManager() 
{
	mLastShadowLight = -1;
	mNextFreeSpotShadowmap = -1;
	mNextFreePointShadowmap = -1;

	mShowLightVolume = false;
		
	mDirLightVertexShader = NULL; 
	mDirLightPixelShader = NULL;
	mDirLightShadowPixelShader = NULL;
	mDirLightCB = NULL;

	mPointLightVertexShader = NULL;
	mPointLightHullShader = NULL; 
	mPointLightDomainShader = NULL; 
	mPointLightPixelShader = NULL;
	mPointLightDomainCB = NULL; 
	mPointLightPixelCB = NULL;

	mPointLightShadowPixelShader = NULL;
	mPointShadowGenVertexShader = NULL;
	mPointShadowGenGeometryShader = NULL;
	mPointShadowGenGeometryCB = NULL;

	mDebugLightPixelShader = NULL;
	mNoDepthWriteLessStencilMaskState = NULL;
	mNoDepthWriteGreatherStencilMaskState = NULL;
	
	mAdditiveBlendState = NULL;
	mNoDepthClipFrontRS = NULL; 
	mWireframeRS = NULL;
	mShadowGenRS = NULL;

	mSpotLightVertexShader = NULL;
	mSpotLightHullShader = NULL;
	mSpotLightDomainShader = NULL;
	mSpotLightPixelShader = NULL;
	mSpotLightShadowPixelShader = NULL;
	mSpotLightDomainCB = NULL;
	mSpotLightPixelCB = NULL;

	mShadowGenVSLayout = NULL;

	mSpotShadowGenVertexShader = NULL;
	mSpotShadowGenVertexCB = NULL;
	

	mPCFSamplerState = NULL;
	mShadowGenDepthState = NULL;

	for (int i = 0; i < mTotalSpotShadowmaps; i++)
	{
		mSpotDepthStencilRT[i] = NULL;
		mSpotDepthStencilDSV[i] = NULL;
		mSpotDepthStencilSRV[i] = NULL;
	}


	for (int i = 0; i < mTotalPointShadowmaps; i++)
	{
		mPointDepthStencilRT[i] = NULL;
		mPointDepthStencilDSV[i] = NULL;
		mPointDepthStencilSRV[i] = NULL;
	}

	mSampPoint = NULL;
	mShadowMapVisVertexShader = NULL;
	mShadowMapVisPixelShader = NULL;

	XMFLOAT3 origin = XMFLOAT3(0.0, 0.0, 0.0);
	mDirectionalDir = XMLoadFloat3(&origin);
	mDirectionalColor = XMLoadFloat3(&origin);
	mDirCastShadows = false;

	mAmbientLowerColor = XMLoadFloat3(&origin);
	mAmbientUpperColor = XMLoadFloat3(&origin);

	mArrLights.clear();

	// cascaded shadow map
	mCascadedDepthStencilRT = NULL;
	mCascadedDepthStencilDSV = NULL;
	mCascadedDepthStencilSRV = NULL;
	mCascadedShadowGenRS = NULL;

	mCascadedShadowGenVertexShader = NULL;
	mCascadedShadowGenGeometryShader = NULL;
	mCascadedShadowGenGeometryCB = NULL;

	mDebugCascadesPixelShader = NULL;

}


LightManager::~LightManager()
{

}

HRESULT LightManager::Init(ID3D11Device* device, Camera* camera)
{
	HRESULT hr;

	ClearLights();

	// Create the constant buffers
	D3D11_BUFFER_DESC cbDesc;
	ZeroMemory(&cbDesc, sizeof(cbDesc));
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.ByteWidth = sizeof(CB_DIRECTIONAL);
	V_RETURN(device->CreateBuffer(&cbDesc, NULL, &mDirLightCB));
	DX_SetDebugName(mDirLightCB, "Directional Light CB");

	cbDesc.ByteWidth = sizeof(CB_POINT_LIGHT_DOMAIN);
	V_RETURN(device->CreateBuffer(&cbDesc, NULL, &mPointLightDomainCB));

	cbDesc.ByteWidth = sizeof(CB_POINT_LIGHT_PIXEL);
	V_RETURN(device->CreateBuffer(&cbDesc, NULL, &mPointLightPixelCB));

	cbDesc.ByteWidth = sizeof(CB_SPOT_LIGHT_DOMAIN);
	V_RETURN(device->CreateBuffer(&cbDesc, NULL, &mSpotLightDomainCB));

	cbDesc.ByteWidth = sizeof(CB_SPOT_LIGHT_PIXEL);
	V_RETURN(device->CreateBuffer(&cbDesc, NULL, &mSpotLightPixelCB));

	cbDesc.ByteWidth = sizeof(XMMATRIX);
	V_RETURN(device->CreateBuffer(&cbDesc, NULL, &mSpotShadowGenVertexCB));
	DX_SetDebugName(mSpotShadowGenVertexCB, "Spot Shadow Gen Vertex CB");

	cbDesc.ByteWidth = 6 * sizeof(XMMATRIX);
	V_RETURN(device->CreateBuffer(&cbDesc, NULL, &mPointShadowGenGeometryCB));
	DX_SetDebugName(mPointShadowGenGeometryCB, "Point Shadow Gen Vertex CB");

	cbDesc.ByteWidth = 3 * sizeof(XMMATRIX);
	V_RETURN(device->CreateBuffer(&cbDesc, NULL, &mCascadedShadowGenGeometryCB));
	DX_SetDebugName(mCascadedShadowGenGeometryCB, "Cascaded Shadow Gen Geometry CB");

	// Read the HLSL file
	WCHAR dirShaderSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\DirectionalLight.hlsl";

	// Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	// Load the directional light shaders
	ID3DBlob* pShaderBlob = NULL;
	V_RETURN(CompileShader(dirShaderSrc, NULL, "DirLightVS", "vs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mDirLightVertexShader));
	DX_SetDebugName(mDirLightVertexShader, "Directional Light VS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(dirShaderSrc, NULL, "DirLightPS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mDirLightPixelShader));
	DX_SetDebugName(mDirLightPixelShader, "Directional Light PS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(dirShaderSrc, NULL, "DirLightShadowPS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mDirLightShadowPixelShader));
	DX_SetDebugName(mDirLightShadowPixelShader, "Directional Light Shadows PS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(dirShaderSrc, NULL, "CascadeShadowDebugPS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mDebugCascadesPixelShader));
	DX_SetDebugName(mDebugCascadesPixelShader, "Debug Cascaded Shadows PS");
	SAFE_RELEASE(pShaderBlob);

	// point light shaders
	WCHAR pointShaderSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\PointLight.hlsl";
	V_RETURN(CompileShader(pointShaderSrc, NULL, "PointLightVS", "vs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mPointLightVertexShader));
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(pointShaderSrc, NULL, "PointLightHS", "hs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateHullShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mPointLightHullShader));
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(pointShaderSrc, NULL, "PointLightDS", "ds_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateDomainShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mPointLightDomainShader));
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(pointShaderSrc, NULL, "PointLightPS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mPointLightPixelShader));
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(pointShaderSrc, NULL, "PointLightShadowPS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mPointLightShadowPixelShader));
	DX_SetDebugName(mPointLightShadowPixelShader, "Point Light Shadow PS");
	SAFE_RELEASE(pShaderBlob);

	// Load the spot light shaders
	WCHAR spotShaderSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\SpotLight.hlsl";
	V_RETURN(CompileShader(spotShaderSrc, NULL, "SpotLightVS", "vs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mSpotLightVertexShader));
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(spotShaderSrc, NULL, "SpotLightHS", "hs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateHullShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mSpotLightHullShader));
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(spotShaderSrc, NULL, "SpotLightDS", "ds_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateDomainShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mSpotLightDomainShader));
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(spotShaderSrc, NULL, "SpotLightPS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mSpotLightPixelShader));
	DX_SetDebugName(mSpotLightPixelShader, "Spot Light PS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(spotShaderSrc, NULL, "SpotLightShadowPS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mSpotLightShadowPixelShader));
	DX_SetDebugName(mSpotLightShadowPixelShader, "Spot Light Shadow PS");
	SAFE_RELEASE(pShaderBlob);

	// Load the shadow generation shaders
	WCHAR shadowgenSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\ShadowGen.hlsl";
	V_RETURN(CompileShader(shadowgenSrc, NULL, "SpotShadowGenVS", "vs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mSpotShadowGenVertexShader));

	// Create a layout for the object data
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	V_RETURN(device->CreateInputLayout(layout, ARRAYSIZE(layout), pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), &mShadowGenVSLayout));
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(shadowgenSrc, NULL, "ShadowMapGenVS", "vs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mPointShadowGenVertexShader));
	DX_SetDebugName(mPointShadowGenVertexShader, "Point Shadow Gen VS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(shadowgenSrc, NULL, "PointShadowGenGS", "gs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateGeometryShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mPointShadowGenGeometryShader));
	DX_SetDebugName(mPointShadowGenGeometryShader, "Point Shadow Gen GS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(shadowgenSrc, NULL, "ShadowMapGenVS", "vs_5_0", dwShaderFlags, &pShaderBlob)); // Both use the same shader
	V_RETURN(device->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mCascadedShadowGenVertexShader));
	DX_SetDebugName(mCascadedShadowGenVertexShader, "Cascaded Shadow Maps Gen VS");
	SAFE_RELEASE(pShaderBlob);

	V_RETURN(CompileShader(shadowgenSrc, NULL, "CascadedShadowMapsGenGS", "gs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreateGeometryShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mCascadedShadowGenGeometryShader));
	DX_SetDebugName(mCascadedShadowGenGeometryShader, "Cascaded Shadow Maps Gen GS");
	SAFE_RELEASE(pShaderBlob);


	// load light volume debugshader
	WCHAR commonSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\Common.hlsl";
	V_RETURN(CompileShader(commonSrc, NULL, "DebugLightPS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN(device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mDebugLightPixelShader));
	SAFE_RELEASE(pShaderBlob);

	D3D11_DEPTH_STENCIL_DESC descDepth;
	descDepth.DepthEnable = TRUE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	descDepth.StencilEnable = TRUE;
	descDepth.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	descDepth.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	const D3D11_DEPTH_STENCILOP_DESC noSkyStencilOp = { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL };
	descDepth.FrontFace = noSkyStencilOp;
	descDepth.BackFace = noSkyStencilOp;
	V_RETURN(device->CreateDepthStencilState(&descDepth, &mNoDepthWriteLessStencilMaskState));

	descDepth.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
	V_RETURN(device->CreateDepthStencilState(&descDepth, &mNoDepthWriteGreatherStencilMaskState));

	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	descDepth.StencilEnable = FALSE;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	D3D11_DEPTH_WRITE_MASK_ALL;
	V_RETURN(device->CreateDepthStencilState(&descDepth, &mShadowGenDepthState));

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

	D3D11_RASTERIZER_DESC descRast = {
		D3D11_FILL_SOLID,
		D3D11_CULL_FRONT,
		FALSE,
		D3D11_DEFAULT_DEPTH_BIAS,
		D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE,
		FALSE,
		FALSE,
		FALSE
	};
	V_RETURN(device->CreateRasterizerState(&descRast, &mNoDepthClipFrontRS));

	//descRast.DepthClipEnable = TRUE;
	descRast.CullMode = D3D11_CULL_BACK;
	descRast.FillMode = D3D11_FILL_WIREFRAME;
	V_RETURN(device->CreateRasterizerState(&descRast, &mWireframeRS));

	// ShadowGen Render State
	descRast.FillMode = D3D11_FILL_SOLID;
	descRast.DepthBias = 1000;
	descRast.DepthBiasClamp = 1.0f;
	descRast.SlopeScaledDepthBias = 1.0f;
	V_RETURN(device->CreateRasterizerState(&descRast, &mShadowGenRS));
	DX_SetDebugName(mCascadedShadowGenRS, "Shadow Gen RS");

	descRast.DepthClipEnable = FALSE;
	V_RETURN(device->CreateRasterizerState(&descRast, &mCascadedShadowGenRS));
	DX_SetDebugName(mCascadedShadowGenRS, "Cascaded Shadow Gen RS");

	// Create the PCF sampler state
	D3D11_SAMPLER_DESC samDesc;
	ZeroMemory(&samDesc, sizeof(samDesc));
	samDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samDesc.MaxAnisotropy = 1;
	samDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	samDesc.MaxLOD = D3D11_FLOAT32_MAX;
	V_RETURN(device->CreateSamplerState(&samDesc, &mPCFSamplerState));

	samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	V_RETURN(device->CreateSamplerState(&samDesc, &mSampPoint));

	// Allocate the depth stencil target
	D3D11_TEXTURE2D_DESC dtd = {
		mShadowMapSize, //UINT Width;
		mShadowMapSize, //UINT Height;
		1, //UINT MipLevels;
		1, //UINT ArraySize;
		//DXGI_FORMAT_R24G8_TYPELESS,
		DXGI_FORMAT_R32_TYPELESS, //DXGI_FORMAT Format;
		1, //DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
		0,//UINT CPUAccessFlags;
		0//UINT MiscFlags;    
	};
	D3D11_DEPTH_STENCIL_VIEW_DESC descDepthView =
	{
		//DXGI_FORMAT_D24_UNORM_S8_UINT,
		DXGI_FORMAT_D32_FLOAT,
		D3D11_DSV_DIMENSION_TEXTURE2D,
		0
	};
	D3D11_SHADER_RESOURCE_VIEW_DESC descShaderView =
	{
		//DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
		DXGI_FORMAT_R32_FLOAT,
		D3D11_SRV_DIMENSION_TEXTURE2D,
		0,
		0
	};

	descShaderView.Texture2D.MipLevels = 1;

	char strResName[32];
	for (int i = 0; i < mTotalSpotShadowmaps; i++)
	{
		sprintf_s(strResName, "Spot Shadowmap Target %d", i);
		V_RETURN(device->CreateTexture2D(&dtd, NULL, &mSpotDepthStencilRT[i]));
		DX_SetDebugName(mSpotDepthStencilRT[i], strResName);

		sprintf_s(strResName, "Spot Shadowmap Depth View %d", i);
		V_RETURN(device->CreateDepthStencilView(mSpotDepthStencilRT[i], &descDepthView, &mSpotDepthStencilDSV[i]));
		DX_SetDebugName(mSpotDepthStencilDSV[i], strResName);
		
		sprintf_s(strResName, "Spot Shadowmap Resource View %d", i);
		V_RETURN(device->CreateShaderResourceView(mSpotDepthStencilRT[i], &descShaderView, &mSpotDepthStencilSRV[i]));
		DX_SetDebugName(mSpotDepthStencilSRV[i], strResName);
	}

	// point shadow targets and views
	dtd.ArraySize = 6;
	dtd.MiscFlags = D3D10_RESOURCE_MISC_TEXTURECUBE;
	descDepthView.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	descDepthView.Texture2DArray.FirstArraySlice = 0;
	descDepthView.Texture2DArray.ArraySize = 6;
	descShaderView.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	descShaderView.TextureCube.MipLevels = 1;
	descShaderView.TextureCube.MostDetailedMip = 0;
	for (int i = 0; i < mTotalPointShadowmaps; i++)
	{
		sprintf_s(strResName, "Point Shadowmap Target %d", i);
		V_RETURN(device->CreateTexture2D(&dtd, NULL, &mPointDepthStencilRT[i]));
		DX_SetDebugName(mPointDepthStencilRT[i], strResName);

		sprintf_s(strResName, "Point Shadowmap Depth View %d", i);
		V_RETURN(device->CreateDepthStencilView(mPointDepthStencilRT[i], &descDepthView, &mPointDepthStencilDSV[i]));
		DX_SetDebugName(mPointDepthStencilDSV[i], strResName);

		sprintf_s(strResName, "Point Shadowmap Resource View %d", i);
		V_RETURN(device->CreateShaderResourceView(mPointDepthStencilRT[i], &descShaderView, &mPointDepthStencilSRV[i]));
		DX_SetDebugName(mPointDepthStencilSRV[i], strResName);
	}

	// Allocate the cascaded shadow maps targets and views
	dtd.ArraySize = CascadedMatrixSet::mTotalCascades;
	dtd.MiscFlags = 0;
	V_RETURN(device->CreateTexture2D(&dtd, NULL, &mCascadedDepthStencilRT));
	DX_SetDebugName(mCascadedDepthStencilRT, "Cascaded Shadow Maps Target");

	descDepthView.Texture2DArray.ArraySize = CascadedMatrixSet::mTotalCascades;
	V_RETURN(device->CreateDepthStencilView(mCascadedDepthStencilRT, &descDepthView, &mCascadedDepthStencilDSV));
	DX_SetDebugName(mCascadedDepthStencilDSV, "Cascaded Shadow Maps DSV");

	descShaderView.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	descShaderView.Texture2DArray.FirstArraySlice = 0;
	descShaderView.Texture2DArray.ArraySize = CascadedMatrixSet::mTotalCascades;
	V_RETURN(device->CreateShaderResourceView(mCascadedDepthStencilRT, &descShaderView, &mCascadedDepthStencilSRV));
	DX_SetDebugName(mCascadedDepthStencilSRV, "Cascaded Shadow Maps SRV");


	mCascadedMatrixSet = new CascadedMatrixSet();

	if (!mCascadedMatrixSet->Init(mShadowMapSize, camera))
	{
		return false;
	}

	// Visualizing the shadow map
	// compile visualize shadow map shaders
	WCHAR shadowVisSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\ShadowMapVisualize.hlsl";

	// compile shaders
	dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	// load shaders to visualize GBuffer
	pShaderBlob = NULL;
	if (!CompileShader(shadowVisSrc, NULL, "ShadowMapVisVS", "vs_5_0", dwShaderFlags, &pShaderBlob))
	{
		MessageBox(0, L"CompileShader Failed.", 0, 0);
		return false;
	}

	hr = device->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mShadowMapVisVertexShader);
	SAFE_RELEASE(pShaderBlob);
	if (FAILED(hr))
		return false;

	if (!CompileShader(shadowVisSrc, NULL, "ShadowMapVisPS", "ps_5_0", dwShaderFlags, &pShaderBlob))
	{
		MessageBox(0, L"CompileShader Failed.", 0, 0);
		return false;
	}
	hr = device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mShadowMapVisPixelShader);
	SAFE_RELEASE(pShaderBlob);
	if (FAILED(hr))
		return false;

	return hr;
}

void LightManager::Release()
{
	SAFE_RELEASE(mDirLightVertexShader);
	SAFE_RELEASE(mDirLightPixelShader);
	SAFE_RELEASE(mDirLightShadowPixelShader);
	SAFE_RELEASE(mDirLightCB);

	SAFE_RELEASE(mPointLightVertexShader);
	SAFE_RELEASE(mPointLightHullShader);
	SAFE_RELEASE(mPointLightDomainShader);
	SAFE_RELEASE(mPointLightPixelShader);
	SAFE_RELEASE(mPointLightShadowPixelShader);
	SAFE_RELEASE(mPointLightDomainCB);
	SAFE_RELEASE(mPointLightPixelCB);

	SAFE_RELEASE(mSpotLightVertexShader);
	SAFE_RELEASE(mSpotLightHullShader);
	SAFE_RELEASE(mSpotLightDomainShader);
	SAFE_RELEASE(mSpotLightPixelShader);
	SAFE_RELEASE(mSpotLightShadowPixelShader);
	SAFE_RELEASE(mSpotLightDomainCB);
	SAFE_RELEASE(mSpotLightPixelCB);

	SAFE_RELEASE(mShadowGenVSLayout);

	SAFE_RELEASE(mSpotShadowGenVertexShader);
	SAFE_RELEASE(mSpotShadowGenVertexCB);

	SAFE_RELEASE(mPointShadowGenVertexShader);
	SAFE_RELEASE(mPointShadowGenGeometryShader);
	SAFE_RELEASE(mPointShadowGenGeometryCB);

	SAFE_RELEASE(mDebugLightPixelShader);

	SAFE_RELEASE(mNoDepthWriteLessStencilMaskState);
	SAFE_RELEASE(mNoDepthWriteGreatherStencilMaskState);

	SAFE_RELEASE(mShadowGenDepthState);

	SAFE_RELEASE(mAdditiveBlendState);

	SAFE_RELEASE(mNoDepthClipFrontRS);

	SAFE_RELEASE(mWireframeRS);

	SAFE_RELEASE(mShadowGenRS);
	SAFE_RELEASE(mCascadedShadowGenRS);

	SAFE_RELEASE(mPCFSamplerState);

	for (int i = 0; i < mTotalSpotShadowmaps; i++)
	{
		SAFE_RELEASE(mSpotDepthStencilRT[i]);
		SAFE_RELEASE(mSpotDepthStencilDSV[i]);
		SAFE_RELEASE(mSpotDepthStencilSRV[i]);
	}

	for (int i = 0; i < mTotalPointShadowmaps; i++)
	{
		SAFE_RELEASE(mPointDepthStencilRT[i]);
		SAFE_RELEASE(mPointDepthStencilDSV[i]);
		SAFE_RELEASE(mPointDepthStencilSRV[i]);
	}

	SAFE_RELEASE(mCascadedShadowGenVertexShader);
	SAFE_RELEASE(mCascadedShadowGenGeometryShader);
	SAFE_RELEASE(mCascadedShadowGenGeometryCB);

	SAFE_RELEASE(mDebugCascadesPixelShader);

	SAFE_DELETE(mCascadedMatrixSet);

	SAFE_RELEASE(mCascadedDepthStencilRT);
	SAFE_RELEASE(mCascadedDepthStencilDSV);
	SAFE_RELEASE(mCascadedDepthStencilSRV);

	SAFE_RELEASE(mSampPoint);
	SAFE_RELEASE(mShadowMapVisPixelShader);
	SAFE_RELEASE(mShadowMapVisVertexShader);

	mArrLights.clear();

}

void LightManager::Update(float dt)
{
}

void LightManager::DoLighting(ID3D11DeviceContext* pd3dImmediateContext, GBuffer* gBuffer, Camera* camera)
{
	// one directional light and array of lights of possible different types

	// Set the shadowmapping PCF sampler
	pd3dImmediateContext->PSSetSamplers(2, 1, &mPCFSamplerState);

	// store previous depth state
	ID3D11DepthStencilState* prevDepthState;
	UINT nPrevStencil;
	pd3dImmediateContext->OMGetDepthStencilState(&prevDepthState, &nPrevStencil);

	// depth state for the directional light
	pd3dImmediateContext->OMSetDepthStencilState(mNoDepthWriteLessStencilMaskState, 2);

	// Set the GBuffer views
	ID3D11ShaderResourceView* arrViews[4] = { gBuffer->GetDepthView(), gBuffer->GetColorView(), gBuffer->GetNormalView(), gBuffer->GetSpecPowerView() };
	pd3dImmediateContext->PSSetShaderResources(0, 4, arrViews);

	// Do the directional light
	DirectionalLight(pd3dImmediateContext);

	// Once done with the directional light, turn on the blending
	ID3D11BlendState* pPrevBlendState;
	FLOAT prevBlendFactor[4];
	UINT prevSampleMask;
	pd3dImmediateContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
	pd3dImmediateContext->OMSetBlendState(mAdditiveBlendState, prevBlendFactor, prevSampleMask);

	// Set the depth state for the rest of the lights
	pd3dImmediateContext->OMSetDepthStencilState(mNoDepthWriteGreatherStencilMaskState, 2);

	ID3D11RasterizerState* pPrevRSState;
	pd3dImmediateContext->RSGetState(&pPrevRSState);
	pd3dImmediateContext->RSSetState(mNoDepthClipFrontRS);

	// Do the rest of the lights
	for (std::vector<LIGHT>::iterator itrCurrentLight = mArrLights.begin(); itrCurrentLight != mArrLights.end(); itrCurrentLight++)
	{
		if ((*itrCurrentLight).eLightType == TYPE_POINT)
		{
			PointLight(pd3dImmediateContext, (*itrCurrentLight).vPosition, (*itrCurrentLight).fRange, (*itrCurrentLight).vColor, (*itrCurrentLight).iShadowmapIdx, false, camera);
		}
		else if ((*itrCurrentLight).eLightType == TYPE_SPOT)
		{
			SpotLight(pd3dImmediateContext, (*itrCurrentLight).vPosition, (*itrCurrentLight).vDirection, (*itrCurrentLight).fRange, (*itrCurrentLight).fInnerAngle,
				(*itrCurrentLight).fOuterAngle, (*itrCurrentLight).vColor, (*itrCurrentLight).iShadowmapIdx, false, camera);
		}
	}

	// Cleanup
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->HSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->DSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);

	// Restore the states
	pd3dImmediateContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
	SAFE_RELEASE(pPrevBlendState);
	pd3dImmediateContext->RSSetState(pPrevRSState);
	SAFE_RELEASE(pPrevRSState);
	pd3dImmediateContext->OMSetDepthStencilState(prevDepthState, nPrevStencil);
	SAFE_RELEASE(prevDepthState);

	// Cleanup
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->PSSetShaderResources(0, 4, arrViews);
}


void LightManager::DoDebugLightVolume(ID3D11DeviceContext* pd3dImmediateContext, Camera* camera)
{
	ID3D11RasterizerState* pPrevRSState;
	pd3dImmediateContext->RSGetState(&pPrevRSState);
	pd3dImmediateContext->RSSetState(mWireframeRS);

	for (std::vector<LIGHT>::iterator itrCurLight = mArrLights.begin(); itrCurLight != mArrLights.end(); itrCurLight++)
	{
		if ((*itrCurLight).eLightType == TYPE_POINT)
		{
			PointLight(pd3dImmediateContext, (*itrCurLight).vPosition, (*itrCurLight).fRange, (*itrCurLight).vColor, (*itrCurLight).iShadowmapIdx, true, camera);
		}
		else if ((*itrCurLight).eLightType == TYPE_SPOT)
		{
			SpotLight(pd3dImmediateContext, (*itrCurLight).vPosition, (*itrCurLight).vDirection, (*itrCurLight).fRange, (*itrCurLight).fInnerAngle,
				(*itrCurLight).fOuterAngle, (*itrCurLight).vColor, (*itrCurLight).iShadowmapIdx, true, camera);
		}
	}

	// Cleanup
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->HSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->DSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);

	// Restore the states
	pd3dImmediateContext->RSSetState(pPrevRSState);
	SAFE_RELEASE(pPrevRSState);
}

void LightManager::DoDebugCascadedShadows(ID3D11DeviceContext* pd3dImmediateContext, GBuffer* gBuffer)
{
	// Set the depth state for the directional light
	pd3dImmediateContext->OMSetDepthStencilState(mNoDepthWriteLessStencilMaskState, 2);

	// Once done with the directional light, turn on the blending
	ID3D11BlendState* pPrevBlendState;
	FLOAT prevBlendFactor[4];
	UINT prevSampleMask;
	pd3dImmediateContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
	pd3dImmediateContext->OMSetBlendState(mAdditiveBlendState, prevBlendFactor, prevSampleMask);

	// Use the same constant buffer values again
	pd3dImmediateContext->PSSetConstantBuffers(1, 1, &mDirLightCB);

	// Set the GBuffer views
	ID3D11ShaderResourceView* arrViews[1] = { gBuffer->GetDepthView() };
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);

	// Primitive settings
	pd3dImmediateContext->IASetInputLayout(NULL);
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(mDirLightVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(mDebugCascadesPixelShader, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	arrViews[0] = NULL;
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pd3dImmediateContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
}

bool LightManager::PrepareNextShadowLight(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Search for the next shadow casting light
	while (++mLastShadowLight < (int)mArrLights.size() && mArrLights[mLastShadowLight].iShadowmapIdx < 0);

	if (mLastShadowLight <= (int)mArrLights.size())
	{
		// Set the shadow depth state
		pd3dImmediateContext->OMSetDepthStencilState(mShadowGenDepthState, 0);

		// If found, prepare for rendering
		if (mLastShadowLight < (int)mArrLights.size())
		{
			const LIGHT& light = mArrLights[mLastShadowLight];
			if (light.eLightType == TYPE_SPOT)
			{
				SpotShadowGen(pd3dImmediateContext, light);
			}
			else if (light.eLightType == TYPE_POINT)
			{
				PointShadowGen(pd3dImmediateContext, light);
			}
			return true;
		}
		else if (mDirCastShadows && mLastShadowLight == (int)mArrLights.size())
		{
			// Set the shadow rasterizer state with the bias
			pd3dImmediateContext->RSSetState(mCascadedShadowGenRS);

			CascadedShadowsGen(pd3dImmediateContext);
			return true;
		}

		return false;
	}

	return false;
}

void LightManager::DirectionalLight(ID3D11DeviceContext* pd3dImmediateContext)
{
	HRESULT hr;

	// Fill the directional and ambient values constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	HR(pd3dImmediateContext->Map(mDirLightCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	CB_DIRECTIONAL* pDirectionalValuesCB = (CB_DIRECTIONAL*)MappedResource.pData;
	// TODO GammaToLinear
	XMStoreFloat3(&pDirectionalValuesCB->vAmbientLower, mAmbientLowerColor);
	XMStoreFloat3(&pDirectionalValuesCB->vAmbientRange, mAmbientUpperColor - mAmbientLowerColor);
	XMStoreFloat3(&pDirectionalValuesCB->vDirToLight, -mDirectionalDir);
	XMStoreFloat3(&pDirectionalValuesCB->vDirectionalColor, mDirectionalColor);

	// Set the shadow matrices if casting shadows
	if (mDirCastShadows)
	{
		pDirectionalValuesCB->ToShadowSpace = XMMatrixTranspose(mCascadedMatrixSet->GetWorldToShadowSpace());

		pDirectionalValuesCB->ToCascadeSpace[0] = mCascadedMatrixSet->GetToCascadeOffsetX();
		pDirectionalValuesCB->ToCascadeSpace[1] = mCascadedMatrixSet->GetToCascadeOffsetY();
		pDirectionalValuesCB->ToCascadeSpace[2] = mCascadedMatrixSet->GetToCascadeScale();
	}

	pDirectionalValuesCB->enableFog = mEnableFog;

	pd3dImmediateContext->Unmap(mDirLightCB, 0);
	pd3dImmediateContext->PSSetConstantBuffers(1, 1, &mDirLightCB);

	// Set the cascaded shadow map if casting shadows
	if (mDirCastShadows)
	{
		pd3dImmediateContext->PSSetShaderResources(5, 1, &mCascadedDepthStencilSRV);
	}

	// Primitive settings
	pd3dImmediateContext->IASetInputLayout(NULL);
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(mDirLightVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(mDirCastShadows ? mDirLightShadowPixelShader : mDirLightPixelShader, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	ID3D11ShaderResourceView *arrRV[1] = { NULL };
	pd3dImmediateContext->PSSetShaderResources(5, 1, arrRV);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}


void LightManager::PointLight(ID3D11DeviceContext* pd3dImmediateContext, const XMFLOAT3& vPos, float fRange, const XMFLOAT3& vColor, int iShadowmapIdx, bool bWireframe, Camera* camera)
{
	HRESULT hr;

	XMMATRIX  mLightWorldScale = XMMatrixScaling(fRange, fRange, fRange);
	XMMATRIX  mLightWorldTrans = XMMatrixTranslation(vPos.x, vPos.y, vPos.z);
	XMMATRIX  mView = camera->View();
	XMMATRIX  mProj = camera->Proj();
	XMMATRIX  mWorldViewProjection = mLightWorldScale * mLightWorldTrans * mView * mProj;

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V(pd3dImmediateContext->Map(mPointLightDomainCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	CB_POINT_LIGHT_DOMAIN* pPointLightDomainCB = (CB_POINT_LIGHT_DOMAIN*)MappedResource.pData;
	pPointLightDomainCB->WorldViewProj = XMMatrixTranspose(mWorldViewProjection);
	pd3dImmediateContext->Unmap(mPointLightDomainCB, 0);
	pd3dImmediateContext->DSSetConstantBuffers(0, 1, &mPointLightDomainCB);

	if (!bWireframe)
	{
		V(pd3dImmediateContext->Map(mPointLightPixelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_POINT_LIGHT_PIXEL* pPointLightPixelCB = (CB_POINT_LIGHT_PIXEL*)MappedResource.pData;
		pPointLightPixelCB->PointLightPos = vPos;
		pPointLightPixelCB->PointLightRangeRcp = 1.0f / fRange;
		pPointLightPixelCB->PointColor = GammaToLinear(vColor);
	
		// Set the shadow map if casting shadows
		if (iShadowmapIdx >= 0)
		{
			// Prepare the projection to shadow space for each cube face
			XMMATRIX matPointProj = XMMatrixPerspectiveFovLH( M_PI * 0.5f, 1.0, mShadowNear, fRange);
			XMFLOAT4X4 tmp;
			XMStoreFloat4x4(&tmp, matPointProj);
			pPointLightPixelCB->LightPerspectiveValues = XMFLOAT2(tmp.m[2][2], tmp.m[3][2]);
		}
	
		pd3dImmediateContext->Unmap(mPointLightPixelCB, 0);
		pd3dImmediateContext->PSSetConstantBuffers(1, 1, &mPointLightPixelCB);

		// Set the shadow map if casting shadows
		if (iShadowmapIdx >= 0)
		{
			pd3dImmediateContext->PSSetShaderResources(4, 1, &mPointDepthStencilSRV[iShadowmapIdx]);
		}
	}


	pd3dImmediateContext->IASetInputLayout(NULL);
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(mPointLightVertexShader, NULL, 0);
	pd3dImmediateContext->HSSetShader(mPointLightHullShader, NULL, 0);
	pd3dImmediateContext->DSSetShader(mPointLightDomainShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(bWireframe ? mDebugLightPixelShader : (iShadowmapIdx >= 0 ? mPointLightShadowPixelShader : mPointLightPixelShader), NULL, 0);

	pd3dImmediateContext->Draw(2, 0);

	// Cleanup
	ID3D11ShaderResourceView* nullSRV = NULL;
	pd3dImmediateContext->PSSetShaderResources(4, 1, &nullSRV);
}

void LightManager::SpotLight(ID3D11DeviceContext* pd3dImmediateContext, const XMFLOAT3& vPos, const XMFLOAT3& vDir, float fRange, float fInnerAngle, float fOuterAngle, const XMFLOAT3& vColor, int iShadowmapIdx, bool bWireframe, Camera* camera)
{
	HRESULT hr;

	// TODO make vector3 class that wraps allthe directx xnamath crazyness!

	// Convert angle in radians to sin/cos values
	float fCosInnerAngle = cosf(fInnerAngle);
	float fSinOuterAngle = sinf(fOuterAngle);
	float fCosOuterAngle = cosf(fOuterAngle);

	// Scale matrix from the cone local space to the world angles and range
	XMMATRIX mLightWorldScale = XMMatrixScaling(fRange, fRange, fRange);

	// Rotate and translate matrix from cone local space to lights world space
	const XMFLOAT3 up = (vDir.y > 0.9 || vDir.y < -0.9) ? XMFLOAT3(0.0f, 0.0f, vDir.y) : XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMVECTOR vUp = XMLoadFloat3(&up);
	XMVECTOR dir = XMLoadFloat3(&vDir);
	
	XMVECTOR vRight = XMVector3Cross(vUp, dir);
	vRight = XMVector3Normalize(vRight);
	vUp = XMVector3Cross(dir, vRight);
	vUp = XMVector3Normalize(vUp);

	XMVECTOR pos = XMLoadFloat3(&vPos);

	XMFLOAT4X4 lightWorldTransRotate;
	XMStoreFloat4x4(&lightWorldTransRotate, XMMatrixIdentity());
	
	XMFLOAT3 r;
	XMStoreFloat3(&r, vRight);
	XMFLOAT3 p;
	XMStoreFloat3(&p, pos);


	for (int i = 0; i < 3; i++)
	{
		lightWorldTransRotate.m[0][i] = (&r.x)[i];
		lightWorldTransRotate.m[1][i] = (&up.x)[i];
		lightWorldTransRotate.m[2][i] = (&vDir.x)[i];
		lightWorldTransRotate.m[3][i] = (&p.x)[i];
	}

	// Prepare the combined local to projected space matrix
	XMMATRIX mView = camera->View();
	XMMATRIX mProj = camera->Proj();
	XMMATRIX m_LightWorldTransRotate = XMLoadFloat4x4(&lightWorldTransRotate);
	XMMATRIX mWorldViewProjection = mLightWorldScale * m_LightWorldTransRotate * mView * mProj;

	// Write the matrix to the domain shader constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V(pd3dImmediateContext->Map(mSpotLightDomainCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	CB_SPOT_LIGHT_DOMAIN* pSpotLightDomainCB = (CB_SPOT_LIGHT_DOMAIN*)MappedResource.pData;
	pSpotLightDomainCB->WolrdViewProj = XMMatrixTranspose(mWorldViewProjection);
	pSpotLightDomainCB->fSinAngle = fSinOuterAngle;
	pSpotLightDomainCB->fCosAngle = fCosOuterAngle;
	pd3dImmediateContext->Unmap(mSpotLightDomainCB, 0);
	pd3dImmediateContext->DSSetConstantBuffers(0, 1, &mSpotLightDomainCB);

	if (!bWireframe)
	{
		V(pd3dImmediateContext->Map(mSpotLightPixelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_SPOT_LIGHT_PIXEL* pSpotLightPixelCB = (CB_SPOT_LIGHT_PIXEL*)MappedResource.pData;
		pSpotLightPixelCB->SpotLightPos = vPos;
		pSpotLightPixelCB->SpotLightRangeRcp = 1.0f / fRange;
		XMStoreFloat3(&pSpotLightPixelCB->vDirToLight, -dir); 
		pSpotLightPixelCB->SpotCosOuterCone = fCosOuterAngle;
		pSpotLightPixelCB->SpotColor = GammaToLinear(vColor);
		pSpotLightPixelCB->SpotCosConeAttRange = fCosInnerAngle - fCosOuterAngle;

		if (iShadowmapIdx >= 0)
		{
			XMMATRIX matSpotView;
			XMVECTOR vLookAt = pos + dir * fRange;
			XMFLOAT3 u = (vDir.y > 0.9 || vDir.y < -0.9) ? XMFLOAT3(0.0f, 0.0f, vDir.y) : XMFLOAT3(0.0f, 1.0f, 0.0f);
			XMVECTOR vUp = XMLoadFloat3(&u);
			XMVECTOR vRight;
			vRight = XMVector3Cross( vUp, dir);
			vRight = XMVector3Normalize(vRight);
			vUp = XMVector3Cross(dir, vRight);
			vUp = XMVector3Normalize(vUp);
			matSpotView = XMMatrixLookAtLH(pos, vLookAt, vUp);
			XMMATRIX matSpotProj = XMMatrixPerspectiveFovLH( 2.0f * fOuterAngle, 1.0, mShadowNear, fRange);
			XMMATRIX ToShadowmap = matSpotView * matSpotProj;
			pSpotLightPixelCB->ToShadowmap = XMMatrixTranspose(ToShadowmap);
		}

		pd3dImmediateContext->Unmap(mSpotLightPixelCB, 0);
		pd3dImmediateContext->PSSetConstantBuffers(1, 1, &mSpotLightPixelCB);

		// Set the shadow map if casting shadows
		if (iShadowmapIdx >= 0)
		{
			pd3dImmediateContext->PSSetShaderResources(4, 1, &mSpotDepthStencilSRV[iShadowmapIdx]);
		}
	}

	pd3dImmediateContext->IASetInputLayout(NULL);
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(mSpotLightVertexShader, NULL, 0);
	pd3dImmediateContext->HSSetShader(mSpotLightHullShader, NULL, 0);
	pd3dImmediateContext->DSSetShader(mSpotLightDomainShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(bWireframe ? mDebugLightPixelShader : (iShadowmapIdx >= 0 ? mSpotLightShadowPixelShader : mSpotLightPixelShader), NULL, 0);

	pd3dImmediateContext->Draw(1, 0);

	// Cleanup
	ID3D11ShaderResourceView* nullSRV = NULL;
	pd3dImmediateContext->PSSetShaderResources(4, 1, &nullSRV);
}

void LightManager::SpotShadowGen(ID3D11DeviceContext* pd3dImmediateContext, const LIGHT& light)
{
	HRESULT hr;

	D3D11_VIEWPORT vp[1] = { { 0, 0, mShadowMapSize, mShadowMapSize, 0.0f, 1.0f } };
	pd3dImmediateContext->RSSetViewports(1, vp);

	// Set the depth target
	ID3D11RenderTargetView* nullRT = NULL;
	ID3D11DepthStencilView* pDSV = mSpotDepthStencilDSV[light.iShadowmapIdx];
	pd3dImmediateContext->OMSetRenderTargets(1, &nullRT, pDSV);

	// Clear the depth stencil
	pd3dImmediateContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH, 1.0, 0);

	// Set the shadow rasterizer state with the bias
	pd3dImmediateContext->RSSetState(mShadowGenRS);

	// Prepare the projection to shadow space
	XMMATRIX matSpotView;
	XMVECTOR vLookAt = XMLoadFloat3(&light.vPosition) + XMLoadFloat3(&light.vDirection) * light.fRange;
	XMFLOAT3 lightY = XMFLOAT3(0.0f, 0.0f, light.vDirection.y);
	XMVECTOR u1 = XMLoadFloat3(&lightY);
	XMFLOAT3 a = XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMVECTOR u2 = XMLoadFloat3(&a);
	XMVECTOR vUp = (light.vDirection.y > 0.9 || light.vDirection.y < -0.9) ? u1 : u2;
	XMVECTOR vRight;
	vRight = XMVector3Cross(vUp, XMLoadFloat3(&light.vDirection));
	vRight = XMVector3Normalize(vRight);
	vUp = XMVector3Cross(XMLoadFloat3(&light.vDirection), vRight);
	vUp = XMVector3Normalize(vUp);
	matSpotView = XMMatrixLookAtLH(XMLoadFloat3(&light.vPosition), vLookAt, vUp);
	XMMATRIX matSpotProj;
	matSpotProj = XMMatrixPerspectiveFovLH(2.0f * light.fOuterAngle, 1.0, mShadowNear, light.fRange);

	// Fill the shadow generation matrix constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V(pd3dImmediateContext->Map(mSpotShadowGenVertexCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	XMMATRIX * shadowGenMat = (XMMATRIX*)MappedResource.pData;
	XMMATRIX toShadow = matSpotView * matSpotProj;
	XMMATRIX toShadowTranspose = XMMatrixTranspose(toShadow);
	shadowGenMat = &toShadowTranspose;
	pd3dImmediateContext->Unmap(mSpotShadowGenVertexCB, 0);
	pd3dImmediateContext->VSSetConstantBuffers(0, 1, &mSpotShadowGenVertexCB);

	// Set the vertex layout
	pd3dImmediateContext->IASetInputLayout(mShadowGenVSLayout);

	// Set the shadow generation shaders
	pd3dImmediateContext->VSSetShader(mSpotShadowGenVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
}

void LightManager::PointShadowGen(ID3D11DeviceContext* pd3dImmediateContext, const LIGHT& light)
{
	HRESULT hr;

	D3D11_VIEWPORT vp[6] = {	{ 0, 0, mShadowMapSize, mShadowMapSize, 0.0f, 1.0f }, 
								{ 0, 0, mShadowMapSize, mShadowMapSize, 0.0f, 1.0f }, 
								{ 0, 0, mShadowMapSize, mShadowMapSize, 0.0f, 1.0f }, 
								{ 0, 0, mShadowMapSize, mShadowMapSize, 0.0f, 1.0f }, 
								{ 0, 0, mShadowMapSize, mShadowMapSize, 0.0f, 1.0f }, 
								{ 0, 0, mShadowMapSize, mShadowMapSize, 0.0f, 1.0f } };

	pd3dImmediateContext->RSSetViewports(6, vp);

	// Set the depth target
	ID3D11RenderTargetView* nullRT = NULL;
	ID3D11DepthStencilView* pDSV = mPointDepthStencilDSV[light.iShadowmapIdx];
	pd3dImmediateContext->OMSetRenderTargets(1, &nullRT, pDSV);

	// Clear the depth stencil
	pd3dImmediateContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH, 1.0, 0);

	// Prepare the projection to shadow space for each cube face
	XMMATRIX matPointProj = XMMatrixPerspectiveFovLH( M_PI * 0.5f, 1.0, mShadowNear, light.fRange);

	// Fill the shadow generation matrices constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V(pd3dImmediateContext->Map(mPointShadowGenGeometryCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	XMMATRIX* pShadowGenMat = (XMMATRIX*)MappedResource.pData;
	XMMATRIX matPointPos = XMMatrixTranslation(-light.vPosition.x, -light.vPosition.y, -light.vPosition.z);

	XMMATRIX matPointView;
	XMMATRIX toShadow;

	// Cube +X 
	matPointView = XMMatrixRotationY(M_PI + M_PI * 0.5f);
	toShadow = matPointPos * matPointView * matPointProj;
	pShadowGenMat[0] = XMMatrixTranspose(toShadow);

	// Cube -X
	matPointView = XMMatrixRotationY(M_PI * 0.5f);
	toShadow = matPointPos * matPointView * matPointProj;
	pShadowGenMat[1] = XMMatrixTranspose(toShadow);

	// Cube +Y
	matPointView = XMMatrixRotationX(M_PI * 0.5f);
	toShadow = matPointPos * matPointView * matPointProj;
	pShadowGenMat[2] = XMMatrixTranspose(toShadow);

	// Cube -Y
	matPointView = XMMatrixRotationX(M_PI + M_PI * 0.5f);
	toShadow = matPointPos * matPointView * matPointProj;
	pShadowGenMat[3] = XMMatrixTranspose(toShadow);

	// Cube +Z
	// Identity view
	toShadow = matPointPos * matPointProj;
	pShadowGenMat[4] = XMMatrixTranspose(toShadow);

	// Cube -Z
	matPointView = XMMatrixRotationY(M_PI);
	toShadow = matPointPos * matPointView * matPointProj;
	pShadowGenMat[5] = XMMatrixTranspose(toShadow);

	pd3dImmediateContext->Unmap(mPointShadowGenGeometryCB, 0);
	pd3dImmediateContext->GSSetConstantBuffers(0, 1, &mPointShadowGenGeometryCB);

	// Set the vertex layout
	pd3dImmediateContext->IASetInputLayout(mShadowGenVSLayout);

	// Set the shadow generation shaders
	pd3dImmediateContext->VSSetShader(mPointShadowGenVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(mPointShadowGenGeometryShader, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
}

void LightManager::CascadedShadowsGen(ID3D11DeviceContext* pd3dImmediateContext)
{
	HRESULT hr;

	D3D11_VIEWPORT vp[3] = {	{ 0, 0, mShadowMapSize, mShadowMapSize, 0.0f, 1.0f },
								{ 0, 0, mShadowMapSize, mShadowMapSize, 0.0f, 1.0f }, 
								{ 0, 0, mShadowMapSize, mShadowMapSize, 0.0f, 1.0f } 
							};
	pd3dImmediateContext->RSSetViewports(3, vp);

	// Set the depth target
	ID3D11RenderTargetView* nullRT = NULL;
	pd3dImmediateContext->OMSetRenderTargets(1, &nullRT, mCascadedDepthStencilDSV);

	// Clear the depth stencil
	pd3dImmediateContext->ClearDepthStencilView(mCascadedDepthStencilDSV, D3D11_CLEAR_DEPTH, 1.0, 0);

	// Get the cascade matrices for the current camera configuration
	mCascadedMatrixSet->Update(mDirectionalDir);

	// Fill the shadow generation matrices constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V(pd3dImmediateContext->Map(mCascadedShadowGenGeometryCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	XMMATRIX* pCascadeShadowGenMat = (XMMATRIX*)MappedResource.pData;

	for (int i = 0; i < CascadedMatrixSet::mTotalCascades; i++)
	{
		pCascadeShadowGenMat[i] = XMMatrixTranspose(mCascadedMatrixSet->GetWorldToCascadeProj(i));
	}

	pd3dImmediateContext->Unmap(mCascadedShadowGenGeometryCB, 0);
	pd3dImmediateContext->GSSetConstantBuffers(0, 1, &mCascadedShadowGenGeometryCB);

	// Set the vertex layout
	pd3dImmediateContext->IASetInputLayout(mShadowGenVSLayout);

	// Set the shadow generation shaders
	pd3dImmediateContext->VSSetShader(mCascadedShadowGenVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(mCascadedShadowGenGeometryShader, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
}

void LightManager::VisualizeShadowMap(ID3D11DeviceContext* pd3dImmediateContext)
{
	ID3D11ShaderResourceView* smv[1] = { mCascadedDepthStencilSRV };
	pd3dImmediateContext->PSSetShaderResources(0, 1, smv);

	pd3dImmediateContext->PSSetSamplers(0, 1, &mSampPoint);

	pd3dImmediateContext->IASetInputLayout(NULL);
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(mShadowMapVisVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(mShadowMapVisPixelShader, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);

	ZeroMemory(smv, sizeof(smv));
	pd3dImmediateContext->PSSetShaderResources(0, 1, smv);
}