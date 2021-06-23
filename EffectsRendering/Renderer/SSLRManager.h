#pragma once

#include "Util.h"
#include "Camera.h"
#include "DirectXTK/SimpleMath.h"

using namespace DirectX::SimpleMath;

// Screen Space LightRay Manager
class SSLRManager
{
public:

	SSLRManager();
	~SSLRManager();

	HRESULT Init(ID3D11Device* device, UINT width, UINT height);
	void Release();

	// Render the screen space light rays on top of the scene
	void Render(ID3D11DeviceContext* pd3dImmediateContext, ID3D11RenderTargetView* pLightAccumRTV, ID3D11ShaderResourceView* pMiniDepthSRV, const Vector3& vSunDir, const Vector3& vSunColor, Camera* camera);

private:

	// Prepare the occlusion map
	void PrepareOcclusion(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pMiniDepthSRV);

	// Ray trace the occlusion map to generate the rays
	void RayTrace(ID3D11DeviceContext* pd3dImmediateContext, const Vector2& vSunPosSS, const Vector3& vSunColor);

	// Combine the rays with the scene
	void Combine(ID3D11DeviceContext* pd3dImmediateContext, ID3D11RenderTargetView* pLightAccumRTV);

	UINT mWidth;
	UINT mHeight;
	float mInitDecay;
	float mDistDecay;
	float mMaxDeltaLen;

	ID3D11Texture2D* mOcclusionTex;
	ID3D11UnorderedAccessView* mOcclusionUAV;
	ID3D11ShaderResourceView* mOcclusionSRV;

	ID3D11Texture2D* mLightRaysTex;
	ID3D11RenderTargetView* mLightRaysRTV;
	ID3D11ShaderResourceView* mLightRaysSRV;

	// Shaders
	ID3D11Buffer* mOcclusionCB;
	ID3D11ComputeShader* mOcclusionCS;
	ID3D11Buffer* mRayTraceCB;
	ID3D11VertexShader* mFullScreenVS;
	ID3D11PixelShader* mRayTracePS;
	ID3D11PixelShader* mCombinePS;

	// Additive blend state to add the light rays on top of the scene lights
	ID3D11BlendState* mAdditiveBlendState;
};