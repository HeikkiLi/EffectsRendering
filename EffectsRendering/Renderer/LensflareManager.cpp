#include "LensflareManager.h"


#include "TextureManager.h"

struct CB_LENSFLARE_VS
{
	XMFLOAT4 Position;
	XMFLOAT4 ScaleRotate;
	XMFLOAT4 Color;
};

LensflareManager::LensflareManager() : mPredicate(NULL), mOcclusionQuery(NULL), mNoDepthState(NULL), mAddativeBlendState(NULL), mNoColorBlendState(NULL),
mLensflareCB(NULL), mLensflareVS(NULL), mLensflarePS(NULL), mCoronaTexView(NULL), mFlareTexView(NULL),
mSunVisibility(0.0f), mQuerySunVisibility(true), mCoronaRotation(0.0f)
{

}

LensflareManager::~LensflareManager()
{

}

HRESULT LensflareManager::Init(ID3D11Device* device)
{
	HRESULT hr;

	D3D11_QUERY_DESC queryDesc;
	queryDesc.Query = D3D11_QUERY_OCCLUSION_PREDICATE;
	queryDesc.MiscFlags = 0;
	V_RETURN(device->CreatePredicate(&queryDesc, &mPredicate));
	DX_SetDebugName(mPredicate, "Lens Flare - Predicate");
	queryDesc.Query = D3D11_QUERY_OCCLUSION;
	V_RETURN(device->CreateQuery(&queryDesc, &mOcclusionQuery));
	DX_SetDebugName(mOcclusionQuery, "Lens Flare - Occlusion Query");

	mSunVisibility = 0.0f;
	mQuerySunVisibility = true;

	// Create constant buffers
	D3D11_BUFFER_DESC cbDesc;
	ZeroMemory(&cbDesc, sizeof(cbDesc));
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.ByteWidth = LensflareManager::mTotalFlares * sizeof(CB_LENSFLARE_VS);
	V_RETURN(device->CreateBuffer(&cbDesc, NULL, &mLensflareCB));
	DX_SetDebugName(mLensflareCB, "Lensflare CB");

	// Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	WCHAR lensflareSrc[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\Lensflare.hlsl";

	ID3DBlob* pShaderBuffer = NULL;
	V_RETURN(CompileShader(lensflareSrc, NULL, "LensflareVS", "vs_5_0", dwShaderFlags, &pShaderBuffer));
	V_RETURN(device->CreateVertexShader(pShaderBuffer->GetBufferPointer(),
		pShaderBuffer->GetBufferSize(), NULL, &mLensflareVS));
	DX_SetDebugName(mLensflareVS, "LensflareVS");
	SAFE_RELEASE(pShaderBuffer);

	V_RETURN(CompileShader(lensflareSrc, NULL, "LensflarePS", "ps_5_0", dwShaderFlags, &pShaderBuffer));
	V_RETURN(device->CreatePixelShader(pShaderBuffer->GetBufferPointer(),
		pShaderBuffer->GetBufferSize(), NULL, &mLensflarePS));
	DX_SetDebugName(mLensflarePS, "LensfalrePS");
	SAFE_RELEASE(pShaderBuffer);

	// Load the corona texture
	mCoronaTexView = TextureManager::Instance()->CreateTexture("..\\Assets\\Corona.jpg");

	// Load the flares texture
	mFlareTexView = TextureManager::Instance()->CreateTexture("..\\Assets\\Flare.jpg");

	D3D11_DEPTH_STENCIL_DESC descDepth;
	descDepth.DepthEnable = FALSE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	descDepth.StencilEnable = FALSE;
	descDepth.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	descDepth.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	const D3D11_DEPTH_STENCILOP_DESC defaultStencilOp = { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS };
	descDepth.FrontFace = defaultStencilOp;
	descDepth.BackFace = defaultStencilOp;
	V_RETURN(device->CreateDepthStencilState(&descDepth, &mNoDepthState));
	DX_SetDebugName(mNoDepthState, "Lens Flare - No Depth DS");

	D3D11_BLEND_DESC descBlend;
	descBlend.AlphaToCoverageEnable = FALSE;
	descBlend.IndependentBlendEnable = FALSE;
	D3D11_RENDER_TARGET_BLEND_DESC TRBlenddDesc =
	{
		TRUE,
		D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD,
		D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD,
		D3D11_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		descBlend.RenderTarget[i] = TRBlenddDesc;
	V_RETURN(device->CreateBlendState(&descBlend, &mAddativeBlendState));
	DX_SetDebugName(mAddativeBlendState, "Lenst Flare - Addative Blending BS");

	TRBlenddDesc.BlendEnable = FALSE;
	TRBlenddDesc.RenderTargetWriteMask = 0;
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		descBlend.RenderTarget[i] = TRBlenddDesc;
	V_RETURN(device->CreateBlendState(&descBlend, &mNoColorBlendState));
	DX_SetDebugName(mNoColorBlendState, "Lens Flare - No Color BS");

	mArrFlares[0].fOffset = 0.0f;
	mArrFlares[0].fScale = 0.028f;
	mArrFlares[0].Color = Vector4(0.2f, 0.18f, 0.15f, 0.25f);
	mArrFlares[1].fOffset = 0.0f;
	mArrFlares[1].fScale = 0.028f;
	mArrFlares[1].Color = Vector4(0.2f, 0.18f, 0.15f, 0.25f);
	mArrFlares[2].fOffset = 0.0f;
	mArrFlares[2].fScale = 0.028f;
	mArrFlares[2].Color = Vector4(0.2f, 0.18f, 0.15f, 0.25f);
	mArrFlares[3].fOffset = 0.0f;
	mArrFlares[3].fScale = 0.028f;
	mArrFlares[3].Color = Vector4(0.2f, 0.18f, 0.15f, 0.25f);
	mArrFlares[4].fOffset = 0.5f;
	mArrFlares[4].fScale = 0.075f;
	mArrFlares[4].Color = Vector4(0.2f, 0.3f, 0.55f, 1.0f);
	mArrFlares[5].fOffset = 1.0f;
	mArrFlares[5].fScale = 0.054f;
	mArrFlares[5].Color = Vector4(0.024f, 0.2f, 0.52f, 1.0f);
	mArrFlares[6].fOffset = 1.35f;
	mArrFlares[6].fScale = 0.095f;
	mArrFlares[6].Color = Vector4(0.032f, 0.1f, 0.5f, 1.0f);
	mArrFlares[7].fOffset = 0.9f;
	mArrFlares[7].fScale = 0.065f;
	mArrFlares[7].Color = Vector4(0.13f, 0.14f, 0.58f, 1.0f);
	mArrFlares[8].fOffset = 1.55f;
	mArrFlares[8].fScale = 0.038f;
	mArrFlares[8].Color = Vector4(0.16f, 0.21, 0.44, 1.0f);
	mArrFlares[9].fOffset = 0.25f;
	mArrFlares[9].fScale = 0.1f;
	mArrFlares[9].Color = Vector4(0.23f, 0.21, 0.44, 0.85f);

	return hr;
}

void LensflareManager::Release()
{
	SAFE_RELEASE(mPredicate);
	SAFE_RELEASE(mOcclusionQuery);
	SAFE_RELEASE(mNoDepthState);
	SAFE_RELEASE(mAddativeBlendState);
	SAFE_RELEASE(mNoColorBlendState);
	SAFE_RELEASE(mLensflareCB);
	SAFE_RELEASE(mLensflareVS);
	SAFE_RELEASE(mLensflarePS);
	SAFE_RELEASE(mCoronaTexView);
	SAFE_RELEASE(mFlareTexView);
}

void LensflareManager::Update(const Vector3& sunWorldPos, float fElapsedTime, Camera* camera)
{
	Matrix mView = camera->View();
	Matrix mProj = camera->Proj();
	Matrix mViewProjection = mView * mProj;
	for (int i = 0; i < mTotalLights; i++)
	{
		mSunWorldPos = sunWorldPos;

		Vector3 ProjPos = XMVector3TransformCoord(mSunWorldPos, mViewProjection);
		mSunPos2D.x = ProjPos.x;
		mSunPos2D.y = ProjPos.y;
	}

	const float fCoronaRotationSpeed = 0.01f * M_PI;
	mCoronaRotation += fElapsedTime * fCoronaRotationSpeed;
}

void LensflareManager::Render(ID3D11DeviceContext* pd3dImmediateContext, Camera* camera)
{
	HRESULT hr;

	pd3dImmediateContext->SetPredication(mPredicate, FALSE);

	mQuerySunVisibility = false;
	UINT64 sunVisibility;
	if (pd3dImmediateContext->GetData(mOcclusionQuery, (void*)&sunVisibility, sizeof(sunVisibility), 0) == S_OK)
	{
		mSunVisibility = (float)sunVisibility / 700.0f;
		mQuerySunVisibility = true;
	}

	ID3D11DepthStencilState* pPrevDepthState;
	pd3dImmediateContext->OMGetDepthStencilState(&pPrevDepthState, NULL);
	pd3dImmediateContext->OMSetDepthStencilState(mNoDepthState, 0);

	ID3D11BlendState* pPrevBlendState;
	FLOAT prevBlendFactor[4];
	UINT prevSampleMask;
	pd3dImmediateContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
	pd3dImmediateContext->OMSetBlendState(mAddativeBlendState, prevBlendFactor, prevSampleMask);

	pd3dImmediateContext->IASetInputLayout(NULL);
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->VSSetShader(mLensflareVS, NULL, 0);
	pd3dImmediateContext->PSSetShader(mLensflarePS, NULL, 0);

	// Fill the corona values
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V(pd3dImmediateContext->Map(mLensflareCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	CB_LENSFLARE_VS* pCB = (CB_LENSFLARE_VS*)MappedResource.pData;
	for (int j = 0; j < mTotalCoronaFlares; j++)
	{
		pCB[j].Position = Vector4(mSunPos2D.x, mSunPos2D.y, 0.0f, 0.0f);

		float fSin = sinf(mCoronaRotation + static_cast<float>(j) * M_PI / mTotalCoronaFlares + M_PI / 5.0f);
		float fCos = cosf(mCoronaRotation + static_cast<float>(j) * M_PI / mTotalCoronaFlares + M_PI / 5.0f);
		const float coronaWidthScale = 5.0f;
		const float coronaHeightScale = 0.25f;
		float fScaleX = mArrFlares[j].fScale * coronaWidthScale;
		float fScaleY = mArrFlares[j].fScale * coronaHeightScale;
		pCB[j].ScaleRotate = Vector4(fScaleX * fCos, fScaleY * -fSin, fScaleX * fSin * camera->GetAspect(), fScaleY * fCos * camera->GetAspect());

		pCB[j].Color = mArrFlares[j].Color * mSunVisibility;
	}
	pd3dImmediateContext->Unmap(mLensflareCB, 0);

	// Render the corona
	pd3dImmediateContext->PSSetShaderResources(0, 1, &mCoronaTexView);
	pd3dImmediateContext->VSSetConstantBuffers(0, 1, &mLensflareCB);
	pd3dImmediateContext->Draw(6 * mTotalCoronaFlares, 0);

	// Fill the flare values
	Vector2 dirFlares = Vector2(0.0f, 0.0f) - mSunPos2D;
	float fLength =  Vector2(XMVector2LengthSq(dirFlares)).x;
	dirFlares = dirFlares * (1.0f / fLength);
	V(pd3dImmediateContext->Map(mLensflareCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	pCB = (CB_LENSFLARE_VS*)MappedResource.pData;
	for (int j = 3; j < mTotalFlares; j++)
	{
		float fOffset = mArrFlares[j].fOffset * fLength;
		Vector2 flarePos2D = fOffset * dirFlares + mSunPos2D;
		pCB[j - 3].Position = Vector4(flarePos2D.x, flarePos2D.y, 0.0f, 0.0f);

		float fScale = mArrFlares[j].fScale;
		pCB[j - 3].ScaleRotate = Vector4(fScale, 0.0f, 0.0f, fScale * camera->GetAspect());

		pCB[j - 3].Color = mArrFlares[j].Color * mSunVisibility;
	}
	pd3dImmediateContext->Unmap(mLensflareCB, 0);

	// Render the flares
	pd3dImmediateContext->PSSetShaderResources(0, 1, &mFlareTexView);
	pd3dImmediateContext->VSSetConstantBuffers(0, 1, &mLensflareCB);
	pd3dImmediateContext->Draw(6 * (mTotalFlares), 0);

	// Restore the blend state
	pd3dImmediateContext->OMSetDepthStencilState(pPrevDepthState, 0);
	SAFE_RELEASE(pPrevDepthState);
	pd3dImmediateContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
	SAFE_RELEASE(pPrevBlendState);

	pd3dImmediateContext->SetPredication(NULL, FALSE);
}

void LensflareManager::BeginSunVisibility(ID3D11DeviceContext* pd3dImmediateContext)
{
	pd3dImmediateContext->OMGetBlendState(&mPrevBlendState, mPrevBlendFactor, &mPrevSampleMask);
	pd3dImmediateContext->OMSetBlendState(mNoColorBlendState, mPrevBlendFactor, mPrevSampleMask);

	pd3dImmediateContext->Begin(mPredicate);
	if (mQuerySunVisibility)
	{
		pd3dImmediateContext->Begin(mOcclusionQuery);
	}

}

void LensflareManager::EndSunVisibility(ID3D11DeviceContext* pd3dImmediateContext)
{
	pd3dImmediateContext->End(mPredicate);
	if (mQuerySunVisibility)
	{
		pd3dImmediateContext->End(mOcclusionQuery);
	}

	// Restore the previous blend state
	pd3dImmediateContext->OMSetBlendState(mPrevBlendState, mPrevBlendFactor, mPrevSampleMask);
	SAFE_RELEASE(mPrevBlendState);
}
