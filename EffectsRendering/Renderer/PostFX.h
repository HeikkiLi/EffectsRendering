#pragma once

#include "Util.h"

class Camera;

class PostFX
{
public:
	PostFX();
	~PostFX();

	bool Init(ID3D11Device* device, UINT width, UINT height);
	void Release();

	// do post processing
	void PostProcessing(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11ShaderResourceView* depthSRV, ID3D11RenderTargetView* pLDRRTV, Camera* camera);
	
	void SetParameters(float middleGrey, float white, float adaptation, float bloomThreshold, float bloomScale, bool enableBloom, float DOFFarStart, float DOFFarRange,
		float bokehLumThreshold, float bokehBlurThreshold, float bokehRadiusScale, float bokehColorScale);

	// Apply a gaussian blur to the input and store it in the output
	void Blur(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pInput, ID3D11UnorderedAccessView* pOutput);

private:

	// Downscale HDR image
	void DownScale(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV);

	// Extract the bloom values from the downscaled image
	void Bloom(ID3D11DeviceContext* pd3dImmediateContext);

	void BokehHightlightScan(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11ShaderResourceView* pDepthSRV, Camera* camera);

	// Final pass composite all post processing calculations
	void FinalPass(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11ShaderResourceView* depthSRV, Camera* camera);

	void BokehRender(ID3D11DeviceContext* pd3dImmediateContext);

	// Downscaled scene texture
	ID3D11Texture2D* mDownScaleRT;
	ID3D11ShaderResourceView* mDownScaleSRV;
	ID3D11UnorderedAccessView* mDownScaleUAV;

	// Temporary texture
	ID3D11Texture2D* mTempRT[2];
	ID3D11ShaderResourceView* mTempSRV[2];
	ID3D11UnorderedAccessView* mTempUAV[2];

	// Bloom texture
	ID3D11Texture2D* mBloomRT;
	ID3D11ShaderResourceView* mBloomSRV;
	ID3D11UnorderedAccessView* mBloomUAV;

	// 1D intermediate storage for the down scale operation
	ID3D11Buffer* mDownScale1DBuffer;
	ID3D11UnorderedAccessView* mDownScale1DUAV;
	ID3D11ShaderResourceView* mDownScale1DSRV;

	// Average luminance
	ID3D11Buffer* mAvgLumBuffer;
	ID3D11UnorderedAccessView* mAvgLumUAV;
	ID3D11ShaderResourceView* mAvgLumSRV;

	// Previous average luminance used for calculate adaptation
	ID3D11Buffer* mPrevAvgLumBuffer;
	ID3D11UnorderedAccessView* mPrevAvgLumUAV;
	ID3D11ShaderResourceView* mPrevAvgLumSRV;

	// Bokeh buffer
	ID3D11Buffer* mBokehBuffer;
	ID3D11UnorderedAccessView* mBokehUAV;
	ID3D11ShaderResourceView* mBokehSRV;

	// Bokeh indirect draw buffer
	ID3D11Buffer* mBokehIndirectDrawBuffer;

	// Bokeh highlight texture view and blend state
	ID3D11ShaderResourceView* mBokehTexView;
	ID3D11BlendState* mAddativeBlendState;

	UINT	mWidth;
	UINT	mHeight;
	UINT	mDownScaleGroups;
	float	mMiddleGrey;
	float	mWhite;
	float	mAdaptation;
	float	mBloomThreshold;
	float	mBloomScale;
	bool	mEnableBloom;
	float	mDOFFarStart;
	float	mDOFFarRangeRcp;
	float	mBokehLumThreshold;
	float	mBokehBlurThreshold;
	float	mBokehRadiusScale;
	float	mBokehColorScale;

	typedef struct
	{
		UINT width;
		UINT height;
		UINT totalPixels;
		UINT groupSize;
		float adaptation;
		float bloomThreshold;
		float ProjectionValues[2];
	} TDownScaleCB;
	ID3D11Buffer* mDownScaleCB;

	typedef struct
	{
		float MiddleGrey;
		float LumWhiteSqr;
		float BloomScale;
		UINT pad;
		float ProjectionValues[2];
		float DOFFarStart;
		float DOFFarRangeRcp;
	} TFinalPassCB;
	ID3D11Buffer* mFinalPassCB;

	typedef struct
	{
		UINT numApproxPasses;
		float halfBoxFilterWidth;			// w/2
		float fracHalfBoxFilterWidth;		// frac(w/2+0.5)
		float invFracHalfBoxFilterWidth;	// 1-frac(w/2+0.5)
		float rcpBoxFilterWidth;			// 1/w
		UINT pad[3];
	} TBlurCB;
	ID3D11Buffer* mBlurCB;

	typedef struct
	{
		UINT Width;
		UINT Height;
		float ProjectionValues[2];
		float DOFFarStart;
		float DOFFarRangeRcp;
		float MiddleGrey;
		float LumWhiteSqr;
		float BokehBlurThreshold;
		float BokehLumThreshold;
		float RadiusScale;
		float ColorScale;
	} TBokehHightlightScanCB;
	ID3D11Buffer* mBokehHightlightScanCB;

	typedef struct
	{
		float AspectRatio[2];
		UINT pad[2];
	} TBokehRenderCB;
	ID3D11Buffer* mBokehRenderCB;

	ID3D11SamplerState* mSampPoint;
	ID3D11SamplerState* mSampLinear;

	// Shaders
	ID3D11ComputeShader*	mDownScaleFirstPassCS;
	ID3D11ComputeShader*	mDownScaleSecondPassCS;
	ID3D11ComputeShader*	mBloomRevealCS;
	ID3D11ComputeShader*	mHorizontalBlurCS;
	ID3D11ComputeShader*	mVerticalBlurCS;
	ID3D11VertexShader*		mFullScreenQuadVS;
	ID3D11PixelShader*		mFinalPassPS;
	ID3D11ComputeShader*	mBokehHighlightSearchCS;
	ID3D11VertexShader*		mBokehVS;
	ID3D11GeometryShader*	mBokehGS;
	ID3D11PixelShader*		mBokehPS;
};