#pragma once

#include "Util.h"
#include "Camera.h"

#include "PostFX.h"

class SSAOManager
{
public:

	SSAOManager();
	~SSAOManager();

	HRESULT Init(ID3D11Device* device, UINT width, UINT height);
	void Deinit();

	void Compute(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pDepthSRV, ID3D11ShaderResourceView* pNormalsSRV, Camera* camera, PostFX* postFx);

	void SetParameters(int SSAOSampRadius, float radius) { mSSAOSampRadius = SSAOSampRadius; mRadius = radius; }

	ID3D11ShaderResourceView* GetSSAOSRV() { return mSSAO_SRV; }

private:

	void DownscaleDepth(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pDepthSRV, ID3D11ShaderResourceView* pNormalsSRV, Camera* camera);

	void ComputeSSAO(ID3D11DeviceContext* pd3dImmediateContext);

	void Blur(ID3D11DeviceContext* pd3dImmediateContext, PostFX* postFx);

	UINT mWidth;
	UINT mHeight;
	int mSSAOSampRadius;
	float mRadius;

	typedef struct
	{
		UINT Width;
		UINT Height;
		float HorResRcp;
		float VerResRcp;
		XMFLOAT4 ProjParams;
		XMMATRIX ViewMatrix;
		float OffsetRadius;
		float Radius;
		float MaxDepth;
		UINT pad;
	} TDownscaleCB;
	ID3D11Buffer* mDownscaleCB;

	// SSAO values for usage with the directional light
	ID3D11Texture2D* mSSAO_RT;
	ID3D11UnorderedAccessView* mSSAO_UAV;
	ID3D11ShaderResourceView* mSSAO_SRV;

	// Downscaled depth buffer (1/4 size)
	ID3D11Buffer* mMiniDepthBuffer;
	ID3D11UnorderedAccessView* mMiniDepthUAV;
	ID3D11ShaderResourceView* mMiniDepthSRV;

	// Shaders
	ID3D11ComputeShader* mDepthDownscaleCS;
	ID3D11ComputeShader* mComputeCS;
};