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
	void PostProcessing(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11RenderTargetView* pLDRRTV);
	
	void SetParameters(float middleGrey, float white, float adaptation) { mMiddleGrey = middleGrey; mWhite = white; mAdaptation = adaptation; }

private:

	// Downscale HDR image
	void DownScale(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV);

	// Final pass composite all post processing calculations
	void FinalPass(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV);

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

	typedef struct
	{
		UINT width;
		UINT height;
		UINT totalPixels;
		UINT groupSize;
		float adaptation;
		UINT pad[3];
	} TDownScaleCB;
	ID3D11Buffer* mDownScaleCB;

	typedef struct
	{
		float MiddleGrey;
		float LumWhiteSqr;
		UINT pad[2];
	} TFinalPassCB;
	ID3D11Buffer* mFinalPassCB;

	ID3D11SamplerState* mSampPoint;

	// Shaders
	ID3D11ComputeShader*	mDownScaleFirstPassCS;
	ID3D11ComputeShader*	mDownScaleSecondPassCS;
	ID3D11VertexShader*		mFullScreenQuadVS;
	ID3D11PixelShader*		mFinalPassPS;
};