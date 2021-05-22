#pragma once

#include "Util.h"

class PostFX
{
public:
	PostFX();
	~PostFX();

	bool Init(ID3D11Device* device, UINT width, UINT height);
	void Release();

	// do post processing
	void PostProcessing(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11RenderTargetView* pLDRRTV, bool enableBloom);
	
	void SetParameters(float middleGrey, float white, float adaptation, float bloomThreshold, float bloomScale);

private:

	// Downscale HDR image
	void DownScale(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV);

	// Extract the bloom values from the downscaled image
	void Bloom(ID3D11DeviceContext* pd3dImmediateContext);

	// Apply a gaussian blur to the input and store it in the output
	void Blur(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pInput, ID3D11UnorderedAccessView* pOutput);

	// Final pass composite all post processing calculations
	void FinalPass(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV);

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

	UINT	mWidth;
	UINT	mHeight;
	UINT	mDownScaleGroups;
	float	mMiddleGrey;
	float	mWhite;
	float	mAdaptation;
	float	mBloomThreshold;
	float	mBloomScale;

	typedef struct
	{
		UINT width;
		UINT height;
		UINT totalPixels;
		UINT groupSize;
		float adaptation;
		float bloomThreshold;
		UINT pad[2];
	} TDownScaleCB;
	ID3D11Buffer* mDownScaleCB;

	typedef struct
	{
		float MiddleGrey;
		float LumWhiteSqr;
		float BloomScale;
		UINT pad;
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
};