#pragma once

#include "Camera.h"
#include "Util.h"

// GBUffer class
//
// GBuffer textures:
// DepthView
// Color specintensivity View
// NormalView
// SpecPowerView
//
// UnPacking, preRender and PostRender
//
class GBuffer
{
public:
	GBuffer();
	~GBuffer();

	bool Init(ID3D11Device* device, UINT width, UINT height);
	void Release();

	void PreRender(ID3D11DeviceContext* pd3dImmediateContext);
	void PostRender(ID3D11DeviceContext* pd3dImmediateContext);
	void PrepareForUnpack(ID3D11DeviceContext* pd3dImmediateContext, Camera* camera);

	ID3D11Texture2D* GetColorTexture() { return mColorSpecIntensityRT; }

	ID3D11DepthStencilView* GetDepthDSV() { return mDepthStencilDSV; }
	ID3D11DepthStencilView* GetDepthReadOnlyDSV() { return mDepthStencilReadOnlyDSV; }

	ID3D11ShaderResourceView* GetDepthView() { return mDepthStencilSRV; }
	ID3D11ShaderResourceView* GetColorView() { return mColorSpecIntensitySRV; }
	ID3D11ShaderResourceView* GetNormalView() { return mNormalSRV; }
	ID3D11ShaderResourceView* GetSpecPowerView() { return mSpecPowerSRV; }


private:

	ID3D11Buffer* mpGBufferUnpackCB;

	// GBuffer textures
	ID3D11Texture2D* mDepthStencilRT;
	ID3D11Texture2D* mColorSpecIntensityRT;
	ID3D11Texture2D* mNormalRT;
	ID3D11Texture2D* mSpecPowerRT;

	// GBuffer render views
	ID3D11DepthStencilView* mDepthStencilDSV;
	ID3D11DepthStencilView* mDepthStencilReadOnlyDSV;
	ID3D11RenderTargetView* mColorSpecIntensityRTV;
	ID3D11RenderTargetView* mNormalRTV;
	ID3D11RenderTargetView* mSpecPowerRTV;

	// GBuffer shader resource views
	ID3D11ShaderResourceView* mDepthStencilSRV;
	ID3D11ShaderResourceView* mColorSpecIntensitySRV;
	ID3D11ShaderResourceView* mNormalSRV;
	ID3D11ShaderResourceView* mSpecPowerSRV;

	ID3D11DepthStencilState *mDepthStencilState;

};