#pragma once

#include <vector>
#include "CascadedMatrixSet.h"

class GBuffer;
class Camera;
 
// LightManager
//
// Directional, Point and Spot lights
//
//
//
//
//
class LightManager
{
public:

	LightManager();
	~LightManager();

	HRESULT Init(ID3D11Device* device, Camera* camera);
	void Release();

	void Update(float dt);

	// Set the ambient values
	void SetAmbient(const XMVECTOR& ambientLowerColor, const XMVECTOR& ambientUpperColor)
	{
		mAmbientLowerColor = ambientLowerColor;
		mAmbientUpperColor = ambientUpperColor;
	}

	// Set the directional light values
	void SetDirectional(const XMVECTOR& directionalDir, const XMVECTOR& directionalColor, bool castShadow, bool antiFlickerOn)
	{
		mDirectionalDir = XMVector3Normalize(directionalDir);
		mDirectionalColor = directionalColor;
		mDirCastShadows = castShadow;
		mCascadedMatrixSet->SetAntiFlicker(antiFlickerOn);
	}

	// Clear the lights from the previous frame
	void ClearLights() { mArrLights.clear();  mLastShadowLight = -1; mNextFreeSpotShadowmap = -1; mNextFreePointShadowmap = -1; }

	// Add a single point light
	void AddPointLight(const XMFLOAT3& pointPosition, float pointRange, const XMFLOAT3& pointColor, bool bCastShadow)
	{
		LIGHT pointLight;

		pointLight.eLightType = TYPE_POINT;
		pointLight.vPosition = pointPosition;
		pointLight.fRange = pointRange;
		pointLight.vColor = pointColor;

		pointLight.iShadowmapIdx = bCastShadow ? GetNextFreePointShadowmapIdx() : -1;

		mArrLights.push_back(pointLight);
	}

	void AddSpotLight(const XMFLOAT3& spotPosition, const XMFLOAT3& spotDirection, float spotRange,
		float spotOuterAngle, float spotInnerAngle, const XMFLOAT3& spotColor, bool bCastShadow)
	{
		LIGHT spotLight;

		spotLight.eLightType = TYPE_SPOT;
		spotLight.vPosition = spotPosition;
		spotLight.vDirection = spotDirection;
		spotLight.fRange = spotRange;
		spotLight.fOuterAngle = M_PI * spotOuterAngle / 180.0f;
		spotLight.fInnerAngle = M_PI * spotInnerAngle / 180.0f;
		spotLight.vColor = spotColor;
		spotLight.iShadowmapIdx = bCastShadow ? GetNextFreeSpotShadowmapIdx() : -1;

		mArrLights.push_back(spotLight);
	}

	void DoLighting(ID3D11DeviceContext* pd3dImmediateContext, GBuffer* gBuffer, Camera* camera);

	// Render each light colume in wireframe
	void DoDebugLightVolume(ID3D11DeviceContext* pd3dImmediateContext, Camera* camera);

	// Color the pixels affected by each cascade
	void DoDebugCascadedShadows(ID3D11DeviceContext* pd3dImmediateContext, GBuffer* gBuffer);

	// Prepare shadow generation for the next shadow casting light
	bool PrepareNextShadowLight(ID3D11DeviceContext* pd3dImmediateContext);

	// Visualize shadowmap 
	void VisualizeShadowMap(ID3D11DeviceContext* pd3dImmediateContext);

private:

	typedef enum
	{
		TYPE_DIRECTIONAL = 0,
		TYPE_POINT,
		TYPE_SPOT
	} LIGHT_TYPE;

	// Light storage
	typedef struct
	{
		LIGHT_TYPE eLightType;
		XMFLOAT3 vPosition;
		XMFLOAT3 vDirection;
		float fRange;
		float fLength;
		float fOuterAngle;
		float fInnerAngle;
		XMFLOAT3 vColor;
		int iShadowmapIdx;
	} LIGHT;

	// Do the directional light calculation
	void DirectionalLight(ID3D11DeviceContext* pd3dImmediateContext);

	// Based on the value of bWireframe, either do the lighting or render the volume
	void PointLight(ID3D11DeviceContext* pd3dImmediateContext, const XMFLOAT3& vPos, float fRange, const XMFLOAT3& vColor, int iShadowmapIdx, bool bWireframe, Camera* camera);
	
	// Spot light calcultations, 
	// Based on the value of bWireframe, either do the lighting or render the volume
	void SpotLight(ID3D11DeviceContext* pd3dImmediateContext, const XMFLOAT3& vPos, const XMFLOAT3& vDir, float fRange, float fInnerAngle, float fOuterAngle, const XMFLOAT3& vColor, int iShadowmapIdx, bool bWireframe, Camera* camera);

	// Get a shadowmap index - first come first served
	int GetNextFreeSpotShadowmapIdx() { return (mNextFreeSpotShadowmap + 1 < mTotalSpotShadowmaps) ? ++mNextFreeSpotShadowmap : -1; }

	// Get a point shadowmap index - first come first served
	int GetNextFreePointShadowmapIdx() { return (mNextFreePointShadowmap + 1 < mTotalPointShadowmaps) ? ++mNextFreePointShadowmap : -1; }

	// Prepare a spot shadowmap for casters rendering
	void SpotShadowGen(ID3D11DeviceContext* pd3dImmediateContext, const LIGHT& light);

	// Prepare a point shadowmap for casters rendering
	void PointShadowGen(ID3D11DeviceContext* pd3dImmediateContext, const LIGHT& light);

	// Prepare cascaded shadow maps for casters rendering
	void CascadedShadowsGen(ID3D11DeviceContext* pd3dImmediateContext);


	// Directional light shaders
	ID3D11VertexShader* mDirLightVertexShader;
	ID3D11PixelShader* mDirLightPixelShader;
	ID3D11PixelShader* mDirLightShadowPixelShader;
	ID3D11Buffer* mDirLightCB;

	// Point light shaders
	ID3D11VertexShader* mPointLightVertexShader;
	ID3D11HullShader*	mPointLightHullShader;
	ID3D11DomainShader* mPointLightDomainShader;
	ID3D11PixelShader*	mPointLightPixelShader;
	ID3D11PixelShader*	mPointLightShadowPixelShader;
	ID3D11Buffer*		mPointLightDomainCB;
	ID3D11Buffer*		mPointLightPixelCB;

	// Spot light shaders
	ID3D11VertexShader* mSpotLightVertexShader;
	ID3D11HullShader*	mSpotLightHullShader;
	ID3D11DomainShader* mSpotLightDomainShader;
	ID3D11PixelShader*	mSpotLightPixelShader;
	ID3D11PixelShader*	mSpotLightShadowPixelShader;
	ID3D11Buffer*		mSpotLightDomainCB;
	ID3D11Buffer*		mSpotLightPixelCB;


	// Shadowmap generation layout
	ID3D11InputLayout* mShadowGenVSLayout;

	// Spot Shadowmap assets
	ID3D11VertexShader* mSpotShadowGenVertexShader;
	ID3D11Buffer*		mSpotShadowGenVertexCB;

	// Point shadow generation assets
	ID3D11VertexShader*		mPointShadowGenVertexShader;
	ID3D11GeometryShader*	mPointShadowGenGeometryShader;
	ID3D11Buffer*			mPointShadowGenGeometryCB;

	// Light volume debug shader
	ID3D11PixelShader* mDebugLightPixelShader;

	// Depth state with no writes and stencil test on
	ID3D11DepthStencilState* mNoDepthWriteLessStencilMaskState;
	ID3D11DepthStencilState* mNoDepthWriteGreatherStencilMaskState;

	// Depth state for shadow generation
	ID3D11DepthStencilState* mShadowGenDepthState;

	// Additive blend state to accumulate light influence
	ID3D11BlendState* mAdditiveBlendState;

	// Front face culling for lights volume
	ID3D11RasterizerState* mNoDepthClipFrontRS;

	// Wireframe render state for light volume debugging
	ID3D11RasterizerState* mWireframeRS;

	// Depth bias for shadow generation
	ID3D11RasterizerState* mShadowGenRS;
	ID3D11RasterizerState* mCascadedShadowGenRS;

	// PCF sampler state for shadow mapping
	ID3D11SamplerState* mPCFSamplerState;

	// Visualize the lights volume
	bool mShowLightVolume;

	// Near plane distance for shadow map generation
	static const float mShadowNear;

	// Index to the last shadow casting light a map was generated for
	int mLastShadowLight;

	// Index to the next avaliable spot shadowmap
	int mNextFreeSpotShadowmap;

	// Size in pixels of the shadow map
	static const int mShadowMapSize = 2048;

	// Maximum supported spot shadowmaps
	static const int mTotalSpotShadowmaps = 3;

	// Spot light shadowmap resources
	ID3D11Texture2D*			mSpotDepthStencilRT[mTotalSpotShadowmaps];
	ID3D11DepthStencilView*		mSpotDepthStencilDSV[mTotalSpotShadowmaps];
	ID3D11ShaderResourceView*	mSpotDepthStencilSRV[mTotalSpotShadowmaps];

	// Index to the next available point shadowmap
	int mNextFreePointShadowmap;

	// Maximum supported point shadowmaps
	static const int mTotalPointShadowmaps = 3;

	// Point light shadowmap resources
	ID3D11Texture2D* mPointDepthStencilRT[mTotalPointShadowmaps];
	ID3D11DepthStencilView* mPointDepthStencilDSV[mTotalSpotShadowmaps];
	ID3D11ShaderResourceView* mPointDepthStencilSRV[mTotalSpotShadowmaps];

	// Cascaded shadow maps generation
	ID3D11VertexShader* mCascadedShadowGenVertexShader;
	ID3D11GeometryShader* mCascadedShadowGenGeometryShader;
	ID3D11Buffer* mCascadedShadowGenGeometryCB;

	// Cascaded shadows debug shader
	ID3D11PixelShader* mDebugCascadesPixelShader;

	// Cascaded Shadow Maps
	CascadedMatrixSet* mCascadedMatrixSet;
	ID3D11Texture2D* mCascadedDepthStencilRT;
	ID3D11DepthStencilView* mCascadedDepthStencilDSV;
	ID3D11ShaderResourceView* mCascadedDepthStencilSRV;


	// for shadowmap visualisation
	ID3D11SamplerState*	mSampPoint;
	ID3D11VertexShader*	mShadowMapVisVertexShader;
	ID3D11PixelShader*	mShadowMapVisPixelShader;

	// Ambient light information
	XMVECTOR mAmbientLowerColor;
	XMVECTOR mAmbientUpperColor;

	// Directional light information
	XMVECTOR mDirectionalDir;
	XMVECTOR mDirectionalColor;
	bool mDirCastShadows;

	// Linked list with the active lights
	std::vector<LIGHT> mArrLights;
};