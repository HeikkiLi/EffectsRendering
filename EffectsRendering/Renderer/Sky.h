#pragma once

#include "Util.h"

class Camera;

class Sky
{
public:
	Sky();
	~Sky();

	bool Init(ID3D11Device* device, const std::string& cubemapFilename, float skySphereRadius);

	ID3D11ShaderResourceView* CubeMapSRV();

	void Render(ID3D11DeviceContext* deviceContext, const Camera* camera);

private:
	Sky(const Sky& rhs);
	Sky& operator=(const Sky& rhs);

private:
	// Sky mesh vertex and index buffers
	ID3D11Buffer* mVB;
	ID3D11Buffer* mIB;

	ID3D11ShaderResourceView* mCubeMapSRV;

	UINT mIndexCount;

	ID3D11Buffer* mSkyVertexShaderCB;

	ID3D11VertexShader* mSkyVertexShader;
	ID3D11InputLayout* mSkyVSLayout;
	ID3D11PixelShader* mSkyPixelShader;

	// No depth with sky stencil test
	ID3D11DepthStencilState* mSkyNoDepthStencilMaskState;

	ID3D11RasterizerState* mCullNone;
};