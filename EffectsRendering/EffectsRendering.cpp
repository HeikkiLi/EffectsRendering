//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EffectsRendering
//
/*
 - HDR
 - Adaptation
 - Bloom
 - DoF
 - Bokeh
 - sunrays
 - volumetric fog?
 - SSAO
 - SSR
 - Lens flare

*/
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
IMPLEMENTED
	- diffuse texture
	- normal maps,  normal texture
	- HDR, tonemapping and adaptation
	- bloom
	- DoF
*/


/// TODO
/*
	- SSAO - Screen Space Ambient Occlusion
	- SSR - Screen Space Reflections
	- fog - Distance Based Fog
	- sunray
	- lens flare
	- Bokeh
	- screenshot name timestamp and save to folder
*/

#include "Renderer/D3DRendererApp.h"
#include "Renderer/Camera.h"
#include "Renderer/GBuffer.h"
#include "Renderer/SceneManager.h"
#include "Renderer/LightManager.h"
#include "Renderer/PostFX.h"
#include "Renderer/Util.h"

enum RENDER_STATE { BACKBUFFERRT, DEPTHRT, COLSPECRT, NORMALRT, SPECPOWRT };

bool g_useNormalMap = true;

class DeferredShaderApp : public D3DRendererApp
{
public:
	DeferredShaderApp(HINSTANCE hInstance);
	~DeferredShaderApp();

	bool Init() override;
	void OnResize() override;
	void Update(float dt)override;
	void Render() override;

	void OnMouseDown(WPARAM btnState, int x, int y) override;
	void OnMouseUp(WPARAM btnState, int x, int y) override;
	void OnMouseMove(WPARAM btnState, int x, int y) override;

private:
	POINT mLastMousePos;

	Camera* mCamera;

	// D3D resources
	ID3D11SamplerState*	mSampPoint = NULL;
	ID3D11SamplerState*	mSampLinear = NULL;
	ID3D11VertexShader*	mGBufferVisVertexShader = NULL;
	ID3D11PixelShader*	mGBufferVisPixelShader = NULL;

	ID3D11VertexShader* mTextureVisVS = NULL;
	ID3D11PixelShader* mTextureVisDepthPS = NULL;
	ID3D11PixelShader* mTextureVisCSpecPS = NULL;
	ID3D11PixelShader* mTextureVisNormalPS = NULL;
	ID3D11PixelShader* mTextureVisSpecPowPS = NULL;

	// for managing the scene
	SceneManager mSceneManager;
	LightManager mLightManager;

	// GBuffer
	GBuffer mGBuffer;
	bool mVisualizeGBuffer;
	void VisualizeGBuffer();
	void VisualizeFullScreenGBufferTexture();

	// Light values
	bool mVisualizeLightVolume;
	XMVECTOR mAmbientLowerColor;
	XMVECTOR mAmbientUpperColor;
	XMVECTOR mDirLightDir;
	XMVECTOR mDirLightColor;

	bool mDirCastShadows;
	bool mAntiFlickerOn;
	bool mVisualizeCascades;

	
	void RenderGUI();
	bool mShowSettings;
	bool mShowShadowMap;

	RENDER_STATE mRenderState;

	// PostFX
	PostFX	mPostFX;
	bool	mEnablePostFX;

	// HDR light accumulation buffer
	ID3D11Texture2D*			mHDRTexture = NULL;
	ID3D11RenderTargetView*		mHDRRTV = NULL;
	ID3D11ShaderResourceView*	mHDRSRV = NULL;

	// PostFX settings
	float	mMiddleGreyMax = 6.0;
	float	mMiddleGrey = 2.863f;
	float	mWhiteMax = 6.0f;
	float	mWhite = 3.53f ;
	float	mAdaptationMax = 10.0f;
	float	mAdaptation = 1.0f;
	
	bool	mEnableBloom = true;
	float	mBloomThresholdMax = 2.5f;
	float	mBloomThreshold = 1.1f;
	float	mBloomScaleMax = 6.0f;
	float	mBloomScale = 0.74f;

	float	mDOFFarStartMax = 400.0f;
	float	mDOFFarStart = 40.0f;
	float	mDOFFarRangeMax = 150.0f;
	float	mDOFFarRange = 60.0f;

	float mBokehLumThresholdMax = 25.0f;
	float mBokehLumThreshold = 7.65f;
	float mBokehBlurThreshold = 0.43f;
	float mBokehRadiusScaledMax = 0.1;
	float mBokehRadiusScale = 0.05;
	float mBokehColorScaleMax = 0.25f;
	float mBokehColorScale = 0.05f;

};



// The Application entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	DeferredShaderApp shaderApp(hInstance);

	if (!shaderApp.Init())
		return 0;

	return shaderApp.Run();
}

DeferredShaderApp::DeferredShaderApp(HINSTANCE hInstance)
	: D3DRendererApp(hInstance)
{
	
	mMainWndCaption = L"EffectsRendering Demo";
	mCamera = NULL;
	mVisualizeGBuffer = false;
	mShowSettings = true;
	mShowShadowMap = false;
	
	mVisualizeLightVolume = false;

	// init light values
	mAmbientLowerColor = XMVectorSet(0.1f, 0.1f, 0.1f, 1.0f);
	mAmbientUpperColor = XMVectorSet(0.6f, 0.6f, 0.6f, 1.0f);
	mDirLightDir = XMVectorSet(0.101f, -0.183f, -0.978f, 1.0f);
	mDirLightColor = XMVectorSet(0.8f, 0.8f, 0.8f, 1.0f);
	mDirCastShadows = true;
	mAntiFlickerOn = true;
	mVisualizeCascades = false;
	mEnablePostFX = true;

	mRenderState = RENDER_STATE::BACKBUFFERRT;
}

DeferredShaderApp::~DeferredShaderApp()
{
	SAFE_RELEASE(mSampLinear);
	SAFE_RELEASE(mSampPoint);
	SAFE_RELEASE(mGBufferVisVertexShader);
	SAFE_RELEASE(mGBufferVisPixelShader);

	SAFE_RELEASE(mTextureVisVS);

	SAFE_DELETE(mCamera);

	SAFE_RELEASE(mTextureVisDepthPS);
	SAFE_RELEASE(mTextureVisCSpecPS);
	SAFE_RELEASE(mTextureVisNormalPS);
	SAFE_RELEASE(mTextureVisSpecPowPS);

	SAFE_RELEASE(mHDRTexture)
	SAFE_RELEASE(mHDRRTV);
	SAFE_RELEASE(mHDRSRV);

	mSceneManager.Release();
	mLightManager.Release();
	mGBuffer.Release();
	mPostFX.Release();
}

bool DeferredShaderApp::Init()
{
	mCamera = new Camera();

	mClientWidth = 1024;
	mClientHeight = 768;

	if (!D3DRendererApp::Init())
		return false;

	// Shader for visualizing GBuffer
	HRESULT hr;
	WCHAR str[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\GBufferVisualize.hlsl";

	// compile shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pShaderBlob = NULL;
	if (!CompileShader(str, NULL, "GBufferVisVS", "vs_5_0", dwShaderFlags, &pShaderBlob))
	{
		MessageBox(0, L"CompileShader Failed.", 0, 0);
		return false;
	}

	hr = md3dDevice->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mGBufferVisVertexShader);
	SAFE_RELEASE(pShaderBlob);
	if (FAILED(hr))
		return false;

	if (!CompileShader(str, NULL, "TextureVisVS", "vs_5_0", dwShaderFlags, &pShaderBlob))
	{
		MessageBox(0, L"CompileShader Failed.", 0, 0);
		return false;
	}

	hr = md3dDevice->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mTextureVisVS);
	SAFE_RELEASE(pShaderBlob);
	if (FAILED(hr))
		return false;

	if (!CompileShader(str, NULL, "GBufferVisPS", "ps_5_0", dwShaderFlags, &pShaderBlob))
	{
		MessageBox(0, L"CompileShader Failed.", 0, 0);
		return false;
	}
	hr = md3dDevice->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mGBufferVisPixelShader);
	SAFE_RELEASE(pShaderBlob);
	if (FAILED(hr))
		return false;

	if (!CompileShader(str, NULL, "TextureVisDepthPS", "ps_5_0", dwShaderFlags, &pShaderBlob))
	{
		MessageBox(0, L"CompileShader Failed.", 0, 0);
		return false;
	}
	hr = md3dDevice->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mTextureVisDepthPS);
	SAFE_RELEASE(pShaderBlob);
	if (FAILED(hr))
		return false;
	

	if (!CompileShader(str, NULL, "TextureVisCSpecPS", "ps_5_0", dwShaderFlags, &pShaderBlob))
	{
		MessageBox(0, L"CompileShader Failed.", 0, 0);
		return false;
	}
	hr = md3dDevice->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mTextureVisCSpecPS);
	SAFE_RELEASE(pShaderBlob);
	if (FAILED(hr))
		return false;

	if (!CompileShader(str, NULL, "TextureVisNormalPS", "ps_5_0", dwShaderFlags, &pShaderBlob))
	{
		MessageBox(0, L"CompileShader Failed.", 0, 0);
		return false;
	}
	hr = md3dDevice->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mTextureVisNormalPS);
	SAFE_RELEASE(pShaderBlob);
	if (FAILED(hr))
		return false;

	if (!CompileShader(str, NULL, "TextureVisSpecPowPS", "ps_5_0", dwShaderFlags, &pShaderBlob))
	{
		MessageBox(0, L"CompileShader Failed.", 0, 0);
		return false;
	}
	hr = md3dDevice->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mTextureVisSpecPowPS);
	SAFE_RELEASE(pShaderBlob);
	if (FAILED(hr))
		return false;


	// create samplers
	D3D11_SAMPLER_DESC samDesc;
	ZeroMemory(&samDesc, sizeof(samDesc));
	samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samDesc.MaxAnisotropy = 1;
	samDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samDesc.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(md3dDevice->CreateSamplerState(&samDesc, &mSampLinear)))
	{
		return false;
	}

	samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	if (FAILED(md3dDevice->CreateSamplerState(&samDesc, &mSampPoint)))
	{
		return false;
	}

	// init camera
	mCamera->LookAt(XMFLOAT3(0.0, 4.0, -5.0), XMFLOAT3(0.0, -0.1, 0.9), XMFLOAT3(0.0, 1.0, 0.0));
	mCamera->SetLens(0.25f*M_PI, AspectRatio(), 0.1f, 500.0f);
	mCamera->UpdateViewMatrix();

	if (!mSceneManager.Init(md3dDevice, mCamera))
		return false;

	V_RETURN(mLightManager.Init(md3dDevice, mCamera));

	return true;
}

void DeferredShaderApp::OnResize()
{
	D3DRendererApp::OnResize();

	// Release the old HDR resources if still around
	SAFE_RELEASE(mHDRTexture);
	SAFE_RELEASE(mHDRRTV);
	SAFE_RELEASE(mHDRSRV);

	// Create the HDR render target
	D3D11_TEXTURE2D_DESC dtd = {
		mClientWidth, //UINT Width;
		mClientHeight, //UINT Height;
		1, //UINT MipLevels;
		1, //UINT ArraySize;
		DXGI_FORMAT_R16G16B16A16_TYPELESS, //DXGI_FORMAT Format;
		1, //DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
		0,//UINT CPUAccessFlags;
		0//UINT MiscFlags;    
	};
	md3dDevice->CreateTexture2D(&dtd, NULL, &mHDRTexture);
	DX_SetDebugName(mHDRTexture, "HDR Light Accumulation Texture");

	D3D11_RENDER_TARGET_VIEW_DESC rtsvd =
	{
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		D3D11_RTV_DIMENSION_TEXTURE2D
	};
	md3dDevice->CreateRenderTargetView(mHDRTexture, &rtsvd, &mHDRRTV);
	DX_SetDebugName(mHDRRTV, "HDR Light Accumulation RTV");

	D3D11_SHADER_RESOURCE_VIEW_DESC dsrvd =
	{
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		D3D11_SRV_DIMENSION_TEXTURE2D,
		0,
		0
	};
	dsrvd.Texture2D.MipLevels = 1;
	md3dDevice->CreateShaderResourceView(mHDRTexture, &dsrvd, &mHDRSRV);
	DX_SetDebugName(mHDRSRV, "HDR Light Accumulation SRV");


	// Recreate the GBuffer with the new size
	mGBuffer.Init(md3dDevice, mClientWidth, mClientHeight);

	// update camera
	mCamera->SetLens(0.25f*M_PI, AspectRatio(), 0.1f, 500.0f);
	mCamera->UpdateViewMatrix();

	// init PostFX with new resized backbuffer info
	mPostFX.Init(md3dDevice, mClientWidth, mClientHeight);
}

void DeferredShaderApp::Update(float dt)
{
	// set ambient colors
	mLightManager.SetAmbient(mAmbientLowerColor, mAmbientUpperColor);

	///// sun / directional light
	mLightManager.SetDirectional(mDirLightDir, mDirLightColor, mDirCastShadows, mAntiFlickerOn);

	mCamera->UpdateViewMatrix();

	if (GetAsyncKeyState(VK_F2) & 0x01)
		mVisualizeGBuffer = !mVisualizeGBuffer;

	if (GetAsyncKeyState(VK_F3) & 0x01)
		mShowShadowMap = !mShowShadowMap;

	if (GetAsyncKeyState(VK_F4) & 0x01)
	{
		// Save backbuffer
		LPCTSTR screenshotFileName = L"screenshot.jpg";
		SnapScreenshot(screenshotFileName);
	}

	if (GetAsyncKeyState(VK_F11) & 0x01)
		mShowSettings = !mShowSettings;

	if (GetAsyncKeyState(VK_DOWN) & 0x01)
		mCamera->Walk(-dt*50.0f);

	if (GetAsyncKeyState(VK_UP) & 0x01)
		mCamera->Walk(dt * 50.0f);

	if (GetAsyncKeyState(0x31) & 0x01)
		mRenderState = RENDER_STATE::BACKBUFFERRT;
	if (GetAsyncKeyState(0x32) & 0x01)
		mRenderState = RENDER_STATE::DEPTHRT;
	if (GetAsyncKeyState(0x33) & 0x01)
		mRenderState = RENDER_STATE::COLSPECRT;
	if (GetAsyncKeyState(0x34) & 0x01)
		mRenderState = RENDER_STATE::NORMALRT;
	if (GetAsyncKeyState(0x35) & 0x01)
		mRenderState = RENDER_STATE::SPECPOWRT;

	mLightManager.ClearLights();

	float adaptationNorm = 0.0f;
	static bool s_bFirstTime = true;
	if (s_bFirstTime)
	{
		adaptationNorm = 0.0f;
		s_bFirstTime = false;
	}
	else
	{
		// Normalize the adaptation time with the frame time (all in seconds)
		// Never use a value higher or equal to 1 since that means no adaptation at all (keeps the old value)
		adaptationNorm = min(mAdaptation < 0.0001f ? 1.0f : dt / mAdaptation, 0.9999f);
	}
	mPostFX.SetParameters(mMiddleGrey, mWhite, adaptationNorm,
							mBloomThreshold, mBloomScale, mEnableBloom, 
							mDOFFarStart, mDOFFarRange,
							mBokehLumThreshold, mBokehBlurThreshold, mBokehRadiusScale,
							mBokehColorScale);

}

void DeferredShaderApp::Render()
{
	
	// Store the current states
	D3D11_VIEWPORT oldvp;
	UINT num = 1;
	md3dImmediateContext->RSGetViewports(&num, &oldvp);
	ID3D11RasterizerState* pPrevRSState;
	md3dImmediateContext->RSGetState(&pPrevRSState);

	mCamera->UpdateViewMatrix();
		
	// Generate the shadow maps
	while (mLightManager.PrepareNextShadowLight(md3dImmediateContext))
	{
		mSceneManager.RenderSceneNoShaders(md3dImmediateContext);
	}

	// Restore the states
	md3dImmediateContext->RSSetViewports(num, &oldvp);
	md3dImmediateContext->RSSetState(pPrevRSState);
	SAFE_RELEASE(pPrevRSState);
	// Cleanup
	md3dImmediateContext->VSSetShader(NULL, NULL, 0);
	md3dImmediateContext->GSSetShader(NULL, NULL, 0);
	

	// clear render target view
	float clearColor[4] = { 0.35, 0.35, 0.72, 0.0 };
	md3dImmediateContext->ClearRenderTargetView(mEnablePostFX ? mHDRRTV : mRenderTargetView, clearColor);

	// clear depth stencil view
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH, 1.0, 0);

	// Store depth state
	ID3D11DepthStencilState* pPrevDepthState;
	UINT nPrevStencil;
	md3dImmediateContext->OMGetDepthStencilState(&pPrevDepthState, &nPrevStencil);

	// set samplers
	ID3D11SamplerState* samplers[2] = { mSampLinear, mSampPoint };
	md3dImmediateContext->PSSetSamplers(0, 2, samplers);

	// Render to GBuffer
	mGBuffer.PreRender(md3dImmediateContext);
	mSceneManager.Render(md3dImmediateContext);
	mGBuffer.PostRender(md3dImmediateContext);

	// set render target
	md3dImmediateContext->OMSetRenderTargets(1, mEnablePostFX ? &mHDRRTV : &mRenderTargetView, mGBuffer.GetDepthReadOnlyDSV());
	mGBuffer.PrepareForUnpack(md3dImmediateContext, mCamera);
	
	// do lighting
	mLightManager.DoLighting(md3dImmediateContext, &mGBuffer, mCamera);

	// Render the sky
	XMFLOAT3 sunDir;
	XMStoreFloat3(&sunDir, mDirLightDir);
	XMFLOAT3 sunColor;
	XMStoreFloat3(&sunColor, (2.0f * mDirLightColor));
	mSceneManager.RenderSky(md3dImmediateContext, sunDir, sunColor);

	if (mEnablePostFX)
	{
		// Do post processing into the LDR render target
		mPostFX.PostProcessing(md3dImmediateContext, mHDRSRV, mGBuffer.GetDepthView(), mRenderTargetView, mCamera);
		md3dImmediateContext->OMSetRenderTargets(1, &mRenderTargetView, mGBuffer.GetDepthDSV());
	}

	// Add the light sources wireframe on top of the LDR target
	if (mVisualizeLightVolume)
	{
		mLightManager.DoDebugLightVolume(md3dImmediateContext, mCamera);
	}

	// show debug buffers
	if (mVisualizeGBuffer)
	{
		md3dImmediateContext->OMSetRenderTargets(1, &mRenderTargetView, NULL);

		VisualizeGBuffer();

		md3dImmediateContext->OMSetRenderTargets(1, &mRenderTargetView, mGBuffer.GetDepthDSV());

	}


	if (mRenderState != RENDER_STATE::BACKBUFFERRT)
	{
		md3dImmediateContext->OMSetRenderTargets(1, &mRenderTargetView, NULL);

		VisualizeFullScreenGBufferTexture();

		md3dImmediateContext->OMSetRenderTargets(1, &mRenderTargetView, mGBuffer.GetDepthDSV());
	}

	// Show the shadow cascades
	if (mVisualizeCascades && mDirCastShadows)
	{
		mLightManager.DoDebugCascadedShadows(md3dImmediateContext, &mGBuffer);
	}

	// Show shadowmap
	if (mShowShadowMap)
	{
		mLightManager.VisualizeShadowMap(md3dImmediateContext);
	}

	// retore prev depth state
	md3dImmediateContext->OMSetDepthStencilState(pPrevDepthState, nPrevStencil);
	SAFE_RELEASE(pPrevDepthState);

	// Render gui
	RenderGUI();
	

	md3dImmediateContext->RSSetViewports(1, &mScreenViewport);
	HR(mSwapChain->Present(0, 0));
}

void DeferredShaderApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void DeferredShaderApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void DeferredShaderApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_RBUTTON) != 0)
	{
		// Each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// move Camera
		mCamera->Pitch(dy);
		mCamera->RotateY(dx);
	}
	else if ((btnState & MK_MBUTTON) != 0 && ( x != mLastMousePos.x && y != mLastMousePos.y) )
	{
		// if middle mouse button then rotate directional light.
		float fDX = (float)(x - mLastMousePos.x) * 0.02f;
		float fDY = (float)(y - mLastMousePos.y) * 0.02f;
		
		XMVECTOR pDeterminant;
		XMFLOAT4X4 matViewInv;
		XMStoreFloat4x4(&matViewInv, XMMatrixInverse(&pDeterminant, mCamera->View()));
		XMFLOAT3 mwiRight = XMFLOAT3(matViewInv._11, matViewInv._12, matViewInv._13);
		XMVECTOR right = XMLoadFloat3( &mwiRight);
		XMFLOAT3 mwiUp = XMFLOAT3(matViewInv._21, matViewInv._22, matViewInv._23);
		XMVECTOR up = XMLoadFloat3( &mwiUp);
		XMFLOAT3 mwiFo = XMFLOAT3(matViewInv._31, matViewInv._32, matViewInv._33);
		XMVECTOR forward = XMLoadFloat3( &mwiFo);

		mDirLightDir -= right * fDX;
		mDirLightDir -= up * fDY;
		mDirLightDir += forward * fDY;

		mDirLightDir = XMVector3Normalize(mDirLightDir);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void DeferredShaderApp::VisualizeGBuffer()
{
	ID3D11ShaderResourceView* arrViews[4] = { mGBuffer.GetDepthView(), mGBuffer.GetColorView(), mGBuffer.GetNormalView() , mGBuffer.GetSpecPowerView() };
	md3dImmediateContext->PSSetShaderResources(0, 4, arrViews);

	md3dImmediateContext->PSSetSamplers(1, 1, &mSampPoint);

	md3dImmediateContext->IASetInputLayout(NULL);
	md3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	md3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Set the shaders
	md3dImmediateContext->VSSetShader(mGBufferVisVertexShader, NULL, 0);
	md3dImmediateContext->GSSetShader(NULL, NULL, 0);
	md3dImmediateContext->PSSetShader(mGBufferVisPixelShader, NULL, 0);

	md3dImmediateContext->Draw(16, 0);

	// Cleanup
	md3dImmediateContext->VSSetShader(NULL, NULL, 0);
	md3dImmediateContext->PSSetShader(NULL, NULL, 0);

	ZeroMemory(arrViews, sizeof(arrViews));
	md3dImmediateContext->PSSetShaderResources(0, 4, arrViews);
}

void DeferredShaderApp::VisualizeFullScreenGBufferTexture()
{
	ID3D11ShaderResourceView* arrViews[4] = { mGBuffer.GetDepthView(), mGBuffer.GetColorView(), mGBuffer.GetNormalView() , mGBuffer.GetSpecPowerView() };
	md3dImmediateContext->PSSetShaderResources(0, 4, arrViews);

	md3dImmediateContext->PSSetSamplers(1, 1, &mSampPoint);

	md3dImmediateContext->IASetInputLayout(NULL);
	md3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	md3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Set the shaders
	md3dImmediateContext->VSSetShader(mTextureVisVS, NULL, 0);
	md3dImmediateContext->GSSetShader(NULL, NULL, 0);
	switch (mRenderState)
	{
	case DEPTHRT:
		md3dImmediateContext->PSSetShader(mTextureVisDepthPS, NULL, 0);
		break;
	case COLSPECRT:
		md3dImmediateContext->PSSetShader(mTextureVisCSpecPS, NULL, 0);
		break;
	case NORMALRT:
		md3dImmediateContext->PSSetShader(mTextureVisNormalPS, NULL, 0);
		break;
	case SPECPOWRT:
		md3dImmediateContext->PSSetShader(mTextureVisSpecPowPS, NULL, 0);
		break;
	default:
		break;
	}
	

	md3dImmediateContext->Draw(4, 0);

	// Cleanup
	md3dImmediateContext->VSSetShader(NULL, NULL, 0);
	md3dImmediateContext->PSSetShader(NULL, NULL, 0);
	md3dImmediateContext->CSSetShader(NULL, NULL, 0);

	ZeroMemory(arrViews, sizeof(arrViews));
	md3dImmediateContext->PSSetShaderResources(0, 4, arrViews);
}

void DeferredShaderApp::RenderGUI()
{
	ImGui_ImplDX11_NewFrame();
	{ //using brackets to control scope makes formatting and checking where the ImGui::Render(); is easier.

		if (mShowRenderStats)
		{
			ImGui::Begin("Framerate", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
			ImGui::SetWindowSize(ImVec2(200, 30), ImGuiSetCond_FirstUseEver);
			ImGui::SetWindowPos(ImVec2(2, 2), ImGuiSetCond_FirstUseEver);
			ImGui::Text("%.3f ms/frame (%.1f FPS)", mFrameStats.mspf, mFrameStats.fps);
			ImGui::End();
		}

		if (mShowSettings)
		{
			ImGui::Begin("Settings", 0, 0);
			ImGui::SetWindowSize(ImVec2(200, 600), ImGuiSetCond_FirstUseEver);
			ImGui::SetWindowPos(ImVec2(10, 60), ImGuiSetCond_FirstUseEver);

			ImGui::Spacing();

			ImGui::Text("Directional Light");
			
			ImGui::Text("Color:");
			static ImVec4 color = ImColor(mDirLightColor.m128_f32[0], mDirLightColor.m128_f32[1], mDirLightColor.m128_f32[2]);
			ImGui::ColorEdit3("DirLightColor##dcol1", (float*)&color, ImGuiColorEditFlags_NoLabel);
			XMFLOAT3 color3 = XMFLOAT3((float*)&color);
			mDirLightColor = XMLoadFloat3(&color3);
			ImGui::Checkbox("Shadows##dirshadow", &mDirCastShadows); 
			ImGui::Checkbox("Visualize Cascades##visCascades", &mVisualizeCascades);
			
			if (ImGui::CollapsingHeader("Camera"))
			{
				float campos[3] = { mCamera->GetPosition().x, mCamera->GetPosition().y, mCamera->GetPosition().z };
				float camlook[3] = { mCamera->GetLook().x, mCamera->GetLook().y, mCamera->GetLook().z };
				ImGui::SliderFloat3("position##campos", campos, -50.0f, 50.0f);
				mCamera->SetPosition(XMFLOAT3((float*)&campos));

				ImGui::SliderFloat3("look##campos", camlook, -50.0f, 50.0f);
				mCamera->SetLook(XMFLOAT3((float*)&camlook));
				mCamera->UpdateViewMatrix();

			}

			ImGui::Checkbox("FrameStats (F1)", &mShowRenderStats);
			ImGui::Checkbox("Visualize Buffers (F2)", &mVisualizeGBuffer);
			ImGui::Checkbox("Visualize ShadowMap (F3)", &mShowShadowMap);
			ImGui::TextWrapped("\nToggle settings window (F11)");
			ImGui::TextWrapped("\nSave screenshot (F4).\n\n");

			ImGui::TextWrapped("MMB rotate sun direction");

			ImGui::Checkbox("Use NormalMap", &g_useNormalMap);

			if (ImGui::CollapsingHeader("Post Effects", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Enable Post Effects", &mEnablePostFX);

				int iwhite = (int)((mWhite/mWhiteMax) * 255.0f);
				ImGui::SliderInt("White", &iwhite, 0, 255, "%1f");
				mWhite = (iwhite / 255.0f) * mWhiteMax + 0.00001f;

				int imiddleGrey = (int)((mMiddleGrey / mMiddleGreyMax) * 255.0f);
				ImGui::SliderInt("MiddleGrey", &imiddleGrey, 0, 255, "%1f");
				mMiddleGrey = (imiddleGrey / 255.0f)  * mMiddleGreyMax + 0.000001f;

				ImGui::SliderFloat("Adaptation factor", &mAdaptation, 1.0f, mAdaptationMax, "%.1f");
				
				ImGui::Checkbox("Enable Bloom", &mEnableBloom);
				ImGui::TextWrapped("Bloom");
				ImGui::SliderFloat("Threshold", &mBloomThreshold, .1f, mBloomThresholdMax, "%.01f");
				ImGui::SliderFloat("Scale", &mBloomScale, .1f, mBloomScaleMax, "%.01f");

				ImGui::TextWrapped("Depth of Field");
				ImGui::SliderFloat("Start", &mDOFFarStart, 0.0f, mDOFFarStartMax, "%.1f");
				ImGui::SliderFloat("Range", &mDOFFarRange, .1f, mDOFFarRangeMax, "%.1f");
			}

			ImGui::End();
		}

	}
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


}