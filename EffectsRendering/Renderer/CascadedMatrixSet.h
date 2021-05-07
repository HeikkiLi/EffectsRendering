#pragma once

#include "SimpleMath.h"

using namespace DirectX::SimpleMath;

class CascadedMatrixSet
{
public:
	CascadedMatrixSet();
	~CascadedMatrixSet();

	bool Init(int shadowMapSize, Camera* cam);
	void Update(const XMVECTOR& directionalDir);

	void SetAntiFlicker(bool isOn) { mAntiFlickerOn = isOn; }

	XMMATRIX GetWorldToShadowSpace() { return mWorldToShadowSpace; }
	XMMATRIX GetWorldToCascadeProj(int i) { return mArrWorldToCascadeProj[i]; }
	
	
	const XMFLOAT4 GetToCascadeOffsetX() const { return Vector4(mToCascadeOffsetX); }
	const XMFLOAT4 GetToCascadeOffsetY() const { return Vector4(mToCascadeOffsetY); }
	const XMFLOAT4 GetToCascadeScale() const { return Vector4(mToCascadeScale); }

	static const int mTotalCascades = 3;

private:

	// Extract the frustum corners for the given near and far values
	void ExtractFrustumPoints(float fNear, float fFar, XMVECTOR* arrFrustumCorners);

	// Extract the frustum bounding sphere for the given near and far values
	void ExtractFrustumBoundSphere(float fNear, float fFar, XMVECTOR& boundCenter, float& boundRadius);

	// Test if a cascade needs an update
	bool CascadeNeedsUpdate(const XMMATRIX& shadowView, int cascadeIdx, const XMFLOAT3& newCenter, XMFLOAT3& offset);

	bool mAntiFlickerOn;
	int mShadowMapSize;
	float mCascadeTotalRange;
	float mArrCascadeRanges[4];

	XMVECTOR mShadowBoundCenter;
	float mShadowBoundRadius;
	XMVECTOR mArrCascadeBoundCenter[mTotalCascades];
	float mArrCascadeBoundRadius[mTotalCascades];

	XMMATRIX mWorldToShadowSpace;
	XMMATRIX mArrWorldToCascadeProj[mTotalCascades];

	float mToCascadeOffsetX[4];
	float mToCascadeOffsetY[4];
	float mToCascadeScale[4];

	Camera* mCamera;
};