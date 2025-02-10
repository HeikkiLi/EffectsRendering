#pragma once

#include "Util.h"


struct Vertex
{
	Vertex() : Position(0.0f, 0.0f, 0.0f), Normal(0.0f, 0.0f, 0.0f), 
				Tex(0.0f, 0.0f), TangentU(0.0f, 0.0f, 0.0f) {}
	Vertex(const XMFLOAT3& p, const XMFLOAT3& n, const XMFLOAT3& tu, const XMFLOAT2& uv)
		: Position(p), Normal(n), Tex(uv), TangentU(tu) {}
	Vertex(
		float px, float py, float pz,
		float nx, float ny, float nz,
		float tux, float tuy, float tuz,
		float u, float v)
		: Position(px, py, pz), Normal(nx, ny, nz), Tex(u, v), TangentU(tux, tuy, tuz) {}
	Vertex(
		float px, float py, float pz)
		: Position(px, py, pz), Normal(0.0f, 0.0f, 0.0f), Tex(0.0f, 0.0f), TangentU(0.0f, 0.0f, 0.0f) {}

	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT2 Tex;
	XMFLOAT3 TangentU;
};


struct Material
{
	Material() {
		ZeroMemory(this, sizeof(this));
	};

	XMFLOAT4 Diffuse;
	std::string diffuseTexture;
	std::string normalTexture;
	std::string bumpMapTexture;
	float specExp;
	float specIntensivity;

};

struct MeshData
{
	std::vector<Vertex> Vertices;
	std::vector<UINT> Indices;
	std::map<UINT, Material> materials;
	std::vector<UINT> FaceMaterials;  // Stores material indices per face
	XMMATRIX world;
};

class Mesh
{
public:
	Mesh();
	~Mesh();

	// Reads data from Param meshData and creates vertex,Index buffers, Material info.
	void Create(ID3D11Device* device, MeshData meshData);

	void Render(ID3D11DeviceContext* pd3dDeviceContext);
	
	// sets vertex and index buffers and calls draw
	void Destroy();

	ID3D11Buffer* mVB;	// Vertex buffer
	ID3D11Buffer* mIB;	// Index buffer
	UINT mVertexCount;
	UINT mIndexCount;

	// material indices
	std::vector<UINT> mMaterialIndices;

	// material list.
	std::map<UINT, Material> mMaterials;

	// world matrix
	XMMATRIX mWorld;

};
