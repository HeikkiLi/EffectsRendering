#include "SceneManager.h"
#include "LightManager.h"
#include "ObjLoader.h"
#include "GeometryGenerator.h"
#include "TextureManager.h"

#include <iostream>

extern bool g_useNormalMap;

#pragma pack(push,1)
struct CB_VS_PER_OBJECT
{
	XMMATRIX mWorldViewProjection;
	XMMATRIX mWorld;
};

struct CB_PS_PER_OBJECT
{
	XMFLOAT4 mdiffuseColor;
	float mSpecExp;
	float mSpecIntensity;
	int mUseDiffuseTexture;
	int mUseNormalMapTexture;
};
#pragma pack(pop)


SceneManager::SceneManager() : mSceneVertexShaderCB(NULL), mScenePixelShaderCB(NULL), mSceneVertexShader(NULL), mSceneVSLayout(NULL), mCamera(NULL),
mScenePixelShader(NULL), mSky(NULL)
{
}

SceneManager::~SceneManager()
{
	Release();
}

bool SceneManager::Init(ID3D11Device* device, Camera* camera)
{
	HRESULT hr;

	mMeshes.clear();

	// Load the models
	
	MeshData pillarMData;
	if (!ObjLoader::Instance()->LoadToMesh("..\\Assets\\pillar.obj", "..\\Assets\\", pillarMData))
		return false;

	Material m_pillar;
	m_pillar.diffuseTexture = "..\\Assets\\factory_brick_1k\\factory_brick_diff_1k.jpg";
	TextureManager::Instance()->CreateTexture(m_pillar.diffuseTexture);

	//m_pillar.normalTexture = "..\\Assets\\factory_brick_1k\\factory_brick_nor_1k.jpg";
	//TextureManager::Instance()->CreateTexture(m_pillar.normalTexture);

	m_pillar.Diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	m_pillar.specExp = 10.0f;
	m_pillar.specIntensivity = 2.0f;
	pillarMData.materials[0] = m_pillar;

	for (int i = 0; i < 5; i++)
	{
		for (int j = 0; j < 6; j++)
		{
			Mesh* pillarMesh = new Mesh();
			pillarMesh->Create(device, pillarMData);
			XMMATRIX matTranslate = XMMatrixTranslation(-10.0f + j*5.0f, 0.0f, 10.0f + i * 5.0f);
			XMMATRIX matScale = XMMatrixScaling(0.1f, 0.1f, 0.1f);
			XMMATRIX matRot = XMMatrixRotationY(M_PI);
			pillarMesh->mWorld = matScale * matRot * matTranslate;
			mMeshes.push_back(pillarMesh);
		}
	}
	

	// plane
	MeshData planeData;
	GeometryGenerator::Instance()->CreateBox(80.0, 0.1, 80.0, planeData);

	Material m_plane;
	
	m_plane.diffuseTexture = "..\\Assets\\floor_tiles_08_1k\\floor_tiles_08_diff_1k.jpg";
	TextureManager::Instance()->CreateTexture(m_plane.diffuseTexture);
	
	m_plane.normalTexture = "..\\Assets\\floor_tiles_08_1k\\floor_tiles_08_nor_1k.jpg";
	TextureManager::Instance()->CreateTexture(m_plane.normalTexture);

	m_plane.Diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	m_plane.specExp = 10.0f;
	m_plane.specIntensivity = 2.0f;
	planeData.materials[0] = m_plane;

	Mesh* plane = new Mesh();
	plane->Create(device, planeData);
	XMMATRIX matTranslate = XMMatrixTranslation(0.0f, 0.0f, 10.0f);
	XMMATRIX matScale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	XMMATRIX matRot = XMMatrixIdentity();
	plane->mWorld = matScale * matRot * matTranslate;
	mMeshes.push_back(plane);

	
		
	// Create constant buffers
	D3D11_BUFFER_DESC cbDesc;
	ZeroMemory(&cbDesc, sizeof(cbDesc));
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.ByteWidth = sizeof(CB_VS_PER_OBJECT);
	if (FAILED(device->CreateBuffer(&cbDesc, NULL, &mSceneVertexShaderCB)))
		return false;

	cbDesc.ByteWidth = sizeof(CB_PS_PER_OBJECT);
	if (FAILED(device->CreateBuffer(&cbDesc, NULL, &mScenePixelShaderCB)))
		return false;

	// Read the HLSL file
	WCHAR str[MAX_PATH] = L"..\\EffectsRendering\\Shaders\\DeferredShading.hlsl";

	// Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	// Load the prepass light shader
	ID3DBlob* pShaderBlob = NULL;
	if (FAILED(CompileShader(str, NULL, "RenderSceneVS", "vs_5_0", dwShaderFlags, &pShaderBlob)))
		return false;

	if (FAILED(device->CreateVertexShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mSceneVertexShader)))
	{
		return false;
	}

	// Create a layout for the object data
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	hr = device->CreateInputLayout(layout, ARRAYSIZE(layout), pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), &mSceneVSLayout);
	SAFE_RELEASE(pShaderBlob);
	if (FAILED(hr))
		return false;

	if (FAILED(CompileShader(str, NULL, "RenderScenePS", "ps_5_0", dwShaderFlags, &pShaderBlob)))
		return false;
	hr = device->CreatePixelShader(pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &mScenePixelShader);
	SAFE_RELEASE(pShaderBlob);
	if (FAILED(hr))
		return false;

	mCamera = camera;

	// Create the Sky object
	mSky = new Sky();
	std::string skyfileName = "..\\Assets\\grasscube1024.dds"; 
	if (!mSky->Init(device, skyfileName, 5000.0f, 2.0f))
	{
		return false;
	}

	return true;
}

void SceneManager::Release()
{
	if (mMeshes.size() > 0)
	{
		for (int i = 0; i < mMeshes.size(); ++i)
		{
			if (mMeshes[i] != NULL)
			{
				mMeshes[i]->Destroy();
				mMeshes[i] = NULL;
			}
		}
	}

	SAFE_RELEASE(mSceneVertexShaderCB);
	SAFE_RELEASE(mScenePixelShaderCB);
	SAFE_RELEASE(mSceneVertexShader);
	SAFE_RELEASE(mSceneVSLayout);
	SAFE_RELEASE(mScenePixelShader);

	SAFE_DELETE(mSky);
}


// Renders the scene to GBuffer
void SceneManager::Render(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Get the projection & view matrix from the camera class	
	XMMATRIX mView = mCamera->View();
	XMMATRIX mProj = mCamera->Proj();

	// Render the meshes
	for (int i = 0; i < mMeshes.size(); ++i)
	{
		// mesh world matrix
		XMMATRIX mWorld = mMeshes[i]->mWorld;
		XMMATRIX mWorldViewProjection = mWorld * mView * mProj;

		// Set the constant buffers
		HRESULT hr;
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		HR(pd3dImmediateContext->Map(mSceneVertexShaderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_VS_PER_OBJECT* pVSPerObject = (CB_VS_PER_OBJECT*)MappedResource.pData;
		pVSPerObject->mWorldViewProjection = XMMatrixTranspose(mWorldViewProjection);
		pVSPerObject->mWorld = XMMatrixTranspose(mWorld);
		pd3dImmediateContext->Unmap(mSceneVertexShaderCB, 0);
		pd3dImmediateContext->VSSetConstantBuffers(0, 1, &mSceneVertexShaderCB);

		HR(pd3dImmediateContext->Map(mScenePixelShaderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_PS_PER_OBJECT* pPSPerObject = (CB_PS_PER_OBJECT*)MappedResource.pData;
		//pPSPerObject->mEyePosition = mCamera->GetPosition();
		// set per object properties
		pPSPerObject->mSpecExp = mMeshes[i]->mMaterials[0].specExp;
		pPSPerObject->mSpecIntensity = mMeshes[i]->mMaterials[0].specIntensivity;
		pPSPerObject->mdiffuseColor = mMeshes[i]->mMaterials[0].Diffuse;
		
		if (TextureManager::Instance()->GetTexture(mMeshes[i]->mMaterials[0].diffuseTexture) != NULL)
		{
			ID3D11ShaderResourceView* srv = TextureManager::Instance()->GetTexture(mMeshes[i]->mMaterials[0].diffuseTexture);
			pd3dImmediateContext->PSSetShaderResources(0, 1, &srv);
			pPSPerObject->mUseDiffuseTexture = true;
		}
		else {
			pPSPerObject->mUseDiffuseTexture = false;		
		}

		if (TextureManager::Instance()->GetTexture(mMeshes[i]->mMaterials[0].normalTexture) != NULL)
		{
			ID3D11ShaderResourceView* srv = TextureManager::Instance()->GetTexture(mMeshes[i]->mMaterials[0].normalTexture);
			pd3dImmediateContext->PSSetShaderResources(1, 1, &srv);
			//pPSPerObject->mUseNormalMapTexture = true;
			pPSPerObject->mUseNormalMapTexture = g_useNormalMap;
		}
		else {
			pPSPerObject->mUseNormalMapTexture = false;
		}


		pd3dImmediateContext->Unmap(mScenePixelShaderCB, 0);
		pd3dImmediateContext->PSSetConstantBuffers(0, 1, &mScenePixelShaderCB);

		// Set the vertex layout
		pd3dImmediateContext->IASetInputLayout(mSceneVSLayout);

		// Set the shaders
		pd3dImmediateContext->VSSetShader(mSceneVertexShader, NULL, 0);
		pd3dImmediateContext->PSSetShader(mScenePixelShader, NULL, 0);

		// render
		mMeshes[i]->Render(pd3dImmediateContext);
	}


}

void SceneManager::RenderSceneNoShaders(ID3D11DeviceContext * pd3dImmediateContext)
{

	XMMATRIX mView = mCamera->View();
	XMMATRIX mProj = mCamera->Proj();

	// render meshes
	for (int i = 0; i < mMeshes.size(); ++i)
	{
		// set object world matrix
		XMMATRIX mWorld = mMeshes[i]->mWorld;
		XMMATRIX mWorldViewProjection = mWorld * mView * mProj;

		// Set the constant buffers
		HRESULT hr;
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		HR(pd3dImmediateContext->Map(mSceneVertexShaderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_VS_PER_OBJECT* pVSPerObject = (CB_VS_PER_OBJECT*)MappedResource.pData;
		pVSPerObject->mWorldViewProjection = XMMatrixTranspose(mWorldViewProjection);
		pVSPerObject->mWorld = XMMatrixTranspose(mWorld);
		pd3dImmediateContext->Unmap(mSceneVertexShaderCB, 0);
		pd3dImmediateContext->VSSetConstantBuffers(0, 1, &mSceneVertexShaderCB);

		// render mesh, sets vertex and index buffers
		mMeshes[i]->Render(pd3dImmediateContext);
	}

}

void SceneManager::RenderSky(ID3D11DeviceContext* pd3dImmediateContext, XMFLOAT3 sunDirection, XMFLOAT3 sunColor)
{
	if (mSky)
	{
		// TODO sun direction
		mSky->Render(pd3dImmediateContext, mCamera, sunDirection, sunColor);
	}

}

void SceneManager::RotateObjects(float dx, float dy, float dz)
{
	for (Mesh* mesh : mMeshes)
	{
		XMMATRIX matRot = XMMatrixRotationRollPitchYaw(dx, dy, dz);
		mesh->mWorld *= matRot;
	}
}
