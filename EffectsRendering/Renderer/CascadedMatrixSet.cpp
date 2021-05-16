#include "Util.h"
#include "Camera.h"
#include "CascadedMatrixSet.h"


CascadedMatrixSet::CascadedMatrixSet() 
{
	mAntiFlickerOn = true;
	mCascadeTotalRange = 80.0;
	mShadowMapSize = 1024;
	mCamera = NULL;
	mShadowBoundRadius = 0;
}

CascadedMatrixSet::~CascadedMatrixSet()
{

}

bool CascadedMatrixSet::Init(int iShadowMapSize, Camera* cam)
{
	if (!cam)
	{
		return false;
	}

	mShadowMapSize = iShadowMapSize;

	mCamera = cam;

	// Set the range values
	mArrCascadeRanges[0] = mCamera->GetNearZ();
	mArrCascadeRanges[1] = 5.0f;
	mArrCascadeRanges[2] = 15.0f;
	mArrCascadeRanges[3] = mCascadeTotalRange;

	for (int i = 0; i < mTotalCascades; i++)
	{
		XMFLOAT3 z = XMFLOAT3(0.0f, 0.0f, 0.0f);
		mArrCascadeBoundCenter[i] = XMLoadFloat3(&z);
		mArrCascadeBoundRadius[i] = 0.0f;
	}

	return true;
}

void CascadedMatrixSet::Update(const XMVECTOR& directionalDir)
{
	// Find the view matrix
	XMFLOAT3 camPos = mCamera->GetPosition();
	XMFLOAT3 camLook = mCamera->GetLook();
	XMVECTOR vWorldCenter = XMLoadFloat3(&camPos) + XMLoadFloat3(&camLook) * (mCascadeTotalRange * 0.5f);
	XMVECTOR vPos = vWorldCenter;
	XMVECTOR vLookAt = vWorldCenter + directionalDir * mCamera->GetFarZ();
	
	XMFLOAT3 right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMVECTOR vRight = XMLoadFloat3( &right );
	
	XMVECTOR vUp;
	vUp = XMVector3Cross(directionalDir, vRight);
	vUp = XMVector3Normalize(vUp);

	XMMATRIX mShadowView = XMMatrixLookAtLH(vPos, vLookAt, vUp);

	// Get the bounds for the shadow space
	float fRadius;
	ExtractFrustumBoundSphere(mArrCascadeRanges[0], mArrCascadeRanges[3], mShadowBoundCenter, fRadius);
	mShadowBoundRadius = max(mShadowBoundRadius, fRadius); // Expend the radius to compensate for numerical errors

	// Find the projection matrix
	XMMATRIX mShadowProj = XMMatrixOrthographicLH(mShadowBoundRadius, mShadowBoundRadius, -mShadowBoundRadius, mShadowBoundRadius);
	
	// The combined transformation from world to shadow space
	mWorldToShadowSpace = mShadowView * mShadowProj;

	// For each cascade find the transformation from shadow to cascade space
	XMMATRIX mShadowViewInv = XMMatrixTranspose(mShadowView);
	for (int iCascadeIdx = 0; iCascadeIdx < mTotalCascades; iCascadeIdx++)
	{
		XMMATRIX cascadeTrans;
		XMMATRIX cascadeScale;
		if (mAntiFlickerOn)
		{
			// To avoid anti flickering we need to make the transformation invariant to camera rotation and translation
			// By encapsulating the cascade frustum with a sphere we achive the rotation invariance
			XMVECTOR vNewCenter;
			ExtractFrustumBoundSphere(mArrCascadeRanges[iCascadeIdx], mArrCascadeRanges[iCascadeIdx + 1], vNewCenter, fRadius);
			mArrCascadeBoundRadius[iCascadeIdx] = max(mArrCascadeBoundRadius[iCascadeIdx], fRadius); // Expend the radius to compensate for numerical errors

			// Only update the cascade bounds if it moved at least a full pixel unit
			// This makes the transformation invariant to translation
			Vector3 vOffset;
			if (CascadeNeedsUpdate(mShadowView, iCascadeIdx, Vector3(vNewCenter), vOffset))
			{
				// To avoid flickering we need to move the bound center in full units
				XMVECTOR vOffsetOut = XMVector3TransformNormal(vOffset, mShadowViewInv);
				mArrCascadeBoundCenter[iCascadeIdx] += vOffsetOut;
			}

			// Get the cascade center in shadow space
			Vector3 vCascadeCenterShadowSpace = XMVector3TransformCoord(mArrCascadeBoundCenter[iCascadeIdx], mWorldToShadowSpace);

			// Update the translation from shadow to cascade space
			mToCascadeOffsetX[iCascadeIdx] = -vCascadeCenterShadowSpace.x;
			mToCascadeOffsetY[iCascadeIdx] = -vCascadeCenterShadowSpace.y;

			cascadeTrans = Matrix::CreateTranslation(mToCascadeOffsetX[iCascadeIdx], mToCascadeOffsetY[iCascadeIdx], 0.0f);

			// Update the scale from shadow to cascade space
			mToCascadeScale[iCascadeIdx] = mShadowBoundRadius / mArrCascadeBoundRadius[iCascadeIdx];
			cascadeScale = Matrix::CreateScale(mToCascadeScale[iCascadeIdx], mToCascadeScale[iCascadeIdx], 1.0f);
		}
		else
		{
			// Since we don't care about flickering we can make the cascade fit tightly around the frustum
			// Extract the bounding box
			XMVECTOR arrFrustumPoints[8];
			ExtractFrustumPoints(mArrCascadeRanges[iCascadeIdx], mArrCascadeRanges[iCascadeIdx + 1], arrFrustumPoints);

			// Transform to shadow space and extract the minimum andn maximum
			Vector3 vMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
			Vector3 vMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			for (int i = 0; i < 8; i++)
			{
				Vector3 vPointInShadowSpace = XMVector3TransformCoord(arrFrustumPoints[i], mWorldToShadowSpace);
				
				for (int j = 0; j < 3; j++)
				{
					if ((&vMin.x)[j] > (&vPointInShadowSpace.x)[j])
						(&vMin.x)[j] = (&vPointInShadowSpace.x)[j];
					if ((&vMax.x)[j] < (&vPointInShadowSpace.x)[j])
						(&vMax.x)[j] = (&vPointInShadowSpace.x)[j];
				}
			}

			Vector3 vCascadeCenterShadowSpace = 0.5f * (vMin + vMax);

			// Update the translation from shadow to cascade space
			mToCascadeOffsetX[iCascadeIdx] = -vCascadeCenterShadowSpace.x;
			mToCascadeOffsetY[iCascadeIdx] = -vCascadeCenterShadowSpace.y;
			
			cascadeTrans = Matrix::CreateTranslation(Vector3(mToCascadeOffsetX[iCascadeIdx], mToCascadeOffsetY[iCascadeIdx], 0.0f));
			
			// Update the scale from shadow to cascade space
			mToCascadeScale[iCascadeIdx] = 2.0f / max(vMax.x - vMin.x, vMax.y - vMin.y);
			cascadeScale = Matrix::CreateScale(mToCascadeScale[iCascadeIdx], mToCascadeScale[iCascadeIdx], 1.0f);
		}

		// Combine the matrices to get the transformation from world to cascade space
		mArrWorldToCascadeProj[iCascadeIdx] = mWorldToShadowSpace * cascadeTrans * cascadeScale;
	}

	// Set the values for the unused slots to someplace outside the shadow space
	for (int i = mTotalCascades; i < 4; i++)
	{
		mToCascadeOffsetX[i] = 250.0f;
		mToCascadeOffsetY[i] = 250.0f;
		mToCascadeScale[i] = 0.1f;
	}
}

void CascadedMatrixSet::ExtractFrustumPoints(float fNear, float fFar, XMVECTOR* arrFrustumCorners)
{
	// Get the camera bases
	const Vector3& camPos = mCamera->GetPosition();
	const Vector3& camRight = mCamera->GetRight();
	const Vector3& camUp = mCamera->GetUp();
	const Vector3& camForward = mCamera->GetLook();

	// Calculate the tangent values (this can be cached
	const float fTanFOVX = tanf(mCamera->GetAspect() * mCamera->GetFovX());
	const float fTanFOVY = tanf(mCamera->GetAspect());

	// Calculate the points on the near plane
	arrFrustumCorners[0] = camPos + (-camRight * fTanFOVX + camUp * fTanFOVY + camForward) * fNear;
	arrFrustumCorners[1] = camPos + (camRight * fTanFOVX + camUp * fTanFOVY + camForward) * fNear;
	arrFrustumCorners[2] = camPos + (camRight * fTanFOVX - camUp * fTanFOVY + camForward) * fNear;
	arrFrustumCorners[3] = camPos + (-camRight * fTanFOVX - camUp * fTanFOVY + camForward) * fNear;

	// Calculate the points on the far plane
	arrFrustumCorners[4] = camPos + (-camRight * fTanFOVX + camUp * fTanFOVY + camForward) * fFar;
	arrFrustumCorners[5] = camPos + (camRight * fTanFOVX + camUp * fTanFOVY + camForward) * fFar;
	arrFrustumCorners[6] = camPos + (camRight * fTanFOVX - camUp * fTanFOVY + camForward) * fFar;
	arrFrustumCorners[7] = camPos + (-camRight * fTanFOVX - camUp * fTanFOVY + camForward) * fFar;
}

void CascadedMatrixSet::ExtractFrustumBoundSphere(float fNear, float fFar, XMVECTOR& boundCenter, float& boundRadius)
{
	// Get the camera bases
	const Vector3& camPos = mCamera->GetPosition();
	const Vector3& camRight = mCamera->GetRight();
	const Vector3& camUp = mCamera->GetUp();
	const Vector3& camForward = mCamera->GetLook();

	// Calculate the tangent values 
	const float fTanFOVX = tanf(mCamera->GetAspect() * mCamera->GetFovX());
	const float fTanFOVY = tanf(mCamera->GetAspect());

	// The center of the sphere is in the center of the frustum
	boundCenter = camPos + camForward * (fNear + 0.5f * (fNear + fFar));

	// Radius is the distance to one of the frustum far corners
	const Vector3 vBoundSpan = camPos + (-camRight * fTanFOVX + camUp * fTanFOVY + camForward) * fFar - Vector3(boundCenter);
	boundRadius = vBoundSpan.Length();
}

bool CascadedMatrixSet::CascadeNeedsUpdate(const XMMATRIX & mShadowView, int iCascadeIdx, const XMFLOAT3& newCenter, XMFLOAT3& vOffset)
{
	// Find the offset between the new and old bound ceter
	Vector3 vOldCenterInCascade = XMVector3TransformCoord(mArrCascadeBoundCenter[iCascadeIdx], mShadowView);
	Vector3 vNewCenterInCascade = XMVector3TransformCoord(Vector3(newCenter), mShadowView);
	Vector3 vCenterDiff = vNewCenterInCascade - vOldCenterInCascade;

	// Find the pixel size based on the diameters and map pixel size
	float fPixelSize = (float)mShadowMapSize / (2.0f * mArrCascadeBoundRadius[iCascadeIdx]);

	float fPixelOffX = vCenterDiff.x * fPixelSize;
	float fPixelOffY = vCenterDiff.y * fPixelSize;

	// Check if the center moved at least half a pixel unit
	bool bNeedUpdate = abs(fPixelOffX) > 0.5f || abs(fPixelOffY) > 0.5f;
	if (bNeedUpdate)
	{
		// Round to the 
		vOffset.x = floorf(0.5f + fPixelOffX) / fPixelSize;
		vOffset.y = floorf(0.5f + fPixelOffY) / fPixelSize;
		vOffset.z = vCenterDiff.z;
	}

	return bNeedUpdate;
}