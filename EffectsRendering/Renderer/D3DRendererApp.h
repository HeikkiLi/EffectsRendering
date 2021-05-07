#pragma once

#include "Util.h"


struct FrameStats
{
	float fps;
	float mspf; 
};


class D3DRendererApp
{
public:
	D3DRendererApp(HINSTANCE hInstance);
	virtual ~D3DRendererApp();

	int Run();

	virtual bool Init();
	virtual void OnResize();
	virtual void Update(float dt) = 0;
	virtual void Render() = 0;

	virtual void ShutDown();

	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	virtual void OnMouseDown(WPARAM btnState, int x, int y) { }
	virtual void OnMouseUp(WPARAM btnState, int x, int y) { }
	virtual void OnMouseMove(WPARAM btnState, int x, int y) { }

	HINSTANCE AppInst()const;
	HWND      MainWnd()const;
	float     AspectRatio()const;

protected:
	bool InitWindow();
	bool InitD3D();

	void CalcFrameStats();

	HRESULT SnapScreenshot(LPCTSTR szFileName);

protected:

	HINSTANCE mhAppInst;
	HWND      mhMainWnd;
	bool      mAppPaused;
	bool      mMinimized;
	bool      mMaximized;
	bool      mResizing;
	UINT      m4xMsaaQuality;

	DemoTimer mTimer;

	// D3D 
	ID3D11Device*			md3dDevice;
	ID3D11DeviceContext*	md3dImmediateContext;
	IDXGISwapChain*			mSwapChain;
	ID3D11Texture2D*		mDepthStencilBuffer;
	ID3D11RenderTargetView* mRenderTargetView;
	ID3D11DepthStencilView* mDepthStencilView;
	D3D11_VIEWPORT			mScreenViewport;

	std::wstring mMainWndCaption;
	D3D_DRIVER_TYPE md3dDriverType;
	int mClientWidth;
	int mClientHeight;
	bool mEnable4xMsaa;

	bool mShowRenderStats;
	FrameStats mFrameStats;
};
