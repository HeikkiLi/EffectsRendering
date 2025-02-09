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

	// Load the scene model
	
	MeshData pillarMData;
	if (!ObjLoader::Instance()->LoadToMesh("..\\Assets\\crytek_sponza\\sponza.obj", "..\\Assets\\crytek_sponza\\", pillarMData))
		return false;
	
	Mesh* sponza = new Mesh();
	sponza->Create(device, pillarMData);
	XMMATRIX matTranslate = XMMatrixTranslation(0.0f, 0.0f, 10.0f);
	XMMATRIX matScale = XMMatrixScaling(0.1f, 0.1f, 0.1f);
	XMMATRIX matRot = XMMatrixIdentity();
	sponza->mWorld = matScale * matRot * matTranslate;
	mMeshes.push_back(sponza);

		
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


// Renders the scene to GBuffer with per-face material handling
void SceneManager::Render(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Get the projection & view matrices from the camera.
	XMMATRIX mView = mCamera->View();
	XMMATRIX mProj = mCamera->Proj();

	// Loop over each mesh in the scene.
	for (int i = 0; i < mMeshes.size(); ++i)
	{
		Mesh* pMesh = mMeshes[i];
		XMMATRIX mWorld = pMesh->mWorld;
		XMMATRIX mWorldViewProjection = mWorld * mView * mProj;

		// Update the Vertex Shader Constant Buffer (per object)
		HRESULT hr;
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		hr = pd3dImmediateContext->Map(mSceneVertexShaderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
		if (SUCCEEDED(hr))
		{
			CB_VS_PER_OBJECT* pVSPerObject = (CB_VS_PER_OBJECT*)MappedResource.pData;
			pVSPerObject->mWorldViewProjection = XMMatrixTranspose(mWorldViewProjection);
			pVSPerObject->mWorld = XMMatrixTranspose(mWorld);
			pd3dImmediateContext->Unmap(mSceneVertexShaderCB, 0);
			pd3dImmediateContext->VSSetConstantBuffers(0, 1, &mSceneVertexShaderCB);
		}

		// Set the shaders and input layout
		pd3dImmediateContext->IASetInputLayout(mSceneVSLayout);
		pd3dImmediateContext->VSSetShader(mSceneVertexShader, nullptr, 0);
		pd3dImmediateContext->PSSetShader(mScenePixelShader, nullptr, 0);

		// Bind the vertex and index buffers
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		pd3dImmediateContext->IASetVertexBuffers(0, 1, &pMesh->mVB, &stride, &offset);
		pd3dImmediateContext->IASetIndexBuffer(pMesh->mIB, DXGI_FORMAT_R32_UINT, 0);
		pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// If no per-face material information is present, render the whole mesh with material 0.
		if (pMesh->mMaterialIndices.empty())
		{
			// Update pixel shader constant buffer with material 0.
			if (pMesh->mMaterials.find(0) != pMesh->mMaterials.end())
			{
				Material& mat = pMesh->mMaterials[0];
				hr = pd3dImmediateContext->Map(mScenePixelShaderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				if (SUCCEEDED(hr))
				{
					CB_PS_PER_OBJECT* pPSPerObject = (CB_PS_PER_OBJECT*)MappedResource.pData;
					pPSPerObject->mSpecExp = mat.specExp;
					pPSPerObject->mSpecIntensity = mat.specIntensivity;
					pPSPerObject->mdiffuseColor = mat.Diffuse;

					// Set diffuse texture if available.
					if (!mat.diffuseTexture.empty() &&
						TextureManager::Instance()->GetTexture(mat.diffuseTexture) != nullptr)
					{
						ID3D11ShaderResourceView* srv = TextureManager::Instance()->GetTexture(mat.diffuseTexture);
						pd3dImmediateContext->PSSetShaderResources(0, 1, &srv);
						pPSPerObject->mUseDiffuseTexture = true;
					}
					else
					{
						pPSPerObject->mUseDiffuseTexture = false;
					}

					// Set normal texture if available.
					if (!mat.normalTexture.empty() &&
						TextureManager::Instance()->GetTexture(mat.normalTexture) != nullptr)
					{
						ID3D11ShaderResourceView* srv = TextureManager::Instance()->GetTexture(mat.normalTexture);
						pd3dImmediateContext->PSSetShaderResources(1, 1, &srv);
						pPSPerObject->mUseNormalMapTexture = g_useNormalMap;
					}
					else
					{
						pPSPerObject->mUseNormalMapTexture = false;
					}

					pd3dImmediateContext->Unmap(mScenePixelShaderCB, 0);
					pd3dImmediateContext->PSSetConstantBuffers(0, 1, &mScenePixelShaderCB);
				}
			}
			// Draw the entire mesh.
			pd3dImmediateContext->DrawIndexed(pMesh->mIndexCount, 0, 0);
		}
		else
		{
			// Render the mesh by grouping faces that share the same material
			// Assume that each face is a triangle (3 indices) and that the number of faces is:
			UINT numFaces = static_cast<UINT>(pMesh->mMaterialIndices.size());
			// Group contiguous faces with the same material.
			UINT groupStartFace = 0;
			UINT currentMaterial = pMesh->mMaterialIndices[0];

			for (UINT face = 1; face <= numFaces; face++)
			{
				bool endOfGroup = (face == numFaces) ||
					(pMesh->mMaterialIndices[face] != currentMaterial);
				if (endOfGroup)
				{
					// Number of faces in this group:
					UINT faceCountInGroup = face - groupStartFace;
					// Calculate starting index in the index buffer.
					UINT indexStart = groupStartFace * 3; // 3 indices per face.
					UINT indexCount = faceCountInGroup * 3;

					// Update Pixel Shader Constant Buffer with current material
					if (pMesh->mMaterials.find(currentMaterial) != pMesh->mMaterials.end())
					{
						Material& mat = pMesh->mMaterials[currentMaterial];
						hr = pd3dImmediateContext->Map(mScenePixelShaderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
						if (SUCCEEDED(hr))
						{
							CB_PS_PER_OBJECT* pPSPerObject = (CB_PS_PER_OBJECT*)MappedResource.pData;
							pPSPerObject->mSpecExp = mat.specExp;
							pPSPerObject->mSpecIntensity = mat.specIntensivity;
							pPSPerObject->mdiffuseColor = mat.Diffuse;

							if (!mat.diffuseTexture.empty() &&
								TextureManager::Instance()->GetTexture(mat.diffuseTexture) != nullptr)
							{
								ID3D11ShaderResourceView* srv = TextureManager::Instance()->GetTexture(mat.diffuseTexture);
								pd3dImmediateContext->PSSetShaderResources(0, 1, &srv);
								pPSPerObject->mUseDiffuseTexture = true;
							}
							else
							{
								pPSPerObject->mUseDiffuseTexture = false;
							}

							if (!mat.normalTexture.empty() &&
								TextureManager::Instance()->GetTexture(mat.normalTexture) != nullptr)
							{
								ID3D11ShaderResourceView* srv = TextureManager::Instance()->GetTexture(mat.normalTexture);
								pd3dImmediateContext->PSSetShaderResources(1, 1, &srv);
								pPSPerObject->mUseNormalMapTexture = g_useNormalMap;
							}
							else
							{
								pPSPerObject->mUseNormalMapTexture = false;
							}

							pd3dImmediateContext->Unmap(mScenePixelShaderCB, 0);
							pd3dImmediateContext->PSSetConstantBuffers(0, 1, &mScenePixelShaderCB);
						}
					}

					// Draw the subset (group) of faces
					pd3dImmediateContext->DrawIndexed(indexCount, indexStart, 0);

					// If not done, start a new group.
					if (face < numFaces)
					{
						currentMaterial = pMesh->mMaterialIndices[face];
						groupStartFace = face;
					}
				}
			}
		} // end if per-face material available
	} // end for each mesh
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
