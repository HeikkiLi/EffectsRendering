#pragma once

#include "Util.h"

#include "Camera.h"

#include "DirectXTK/SimpleMath.h"

using namespace DirectX::SimpleMath;

class LensflareManager
{
public:

	LensflareManager();
	~LensflareManager();

	HRESULT Init(ID3D11Device* device);
	void Release();
	void Update(const Vector3& sunWorldPos, float fElapsedTime, Camera* camera);
	void Render(ID3D11DeviceContext* pd3dImmediateContext, Camera* camera);
	void BeginSunVisibility(ID3D11DeviceContext* pd3dImmediateContext);
	void EndSunVisibility(ID3D11DeviceContext* pd3dImmediateContext);

	static const int mTotalLights = 1;
	static const int mTotalCoronaFlares = 9;
	static const int mTotalFlares = 10;

private:

	ID3D11Predicate* mPredicate;
	ID3D11Query* mOcclusionQuery;
	ID3D11DepthStencilState* mNoDepthState;
	ID3D11BlendState* mAddativeBlendState;
	ID3D11BlendState* mNoColorBlendState;
	ID3D11Buffer* mLensflareCB;
	ID3D11VertexShader* mLensflareVS;
	ID3D11PixelShader* mLensflarePS;
	ID3D11ShaderResourceView* mCoronaTexView;
	ID3D11ShaderResourceView* mFlareTexView;

	// Sun position in world space and in 2D space
	Vector3 mSunWorldPos;
	Vector2 mSunPos2D;
	float mSunVisibility;
	bool mQuerySunVisibility;

	// Array with flares information
	typedef struct
	{
		float fOffset;
		float fScale;
		Vector4 Color;
	} FLARE;
	FLARE mArrFlares[mTotalFlares];

	typedef struct
	{
		float fScale;
		Vector4 Color;
	} CORONA;
	CORONA mArrCorona[3];

	// Rotate the corona over time
	float mCoronaRotation;

	// Store pre-visibility blend values
	ID3D11BlendState* mPrevBlendState;
	FLOAT mPrevBlendFactor[4];
	UINT mPrevSampleMask;
};