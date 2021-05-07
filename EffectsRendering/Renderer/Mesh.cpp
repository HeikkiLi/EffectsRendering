#include "Mesh.h"


Mesh::Mesh() : mVB(NULL), mIB(NULL), mIndexCount(0), mVertexCount(0)
{
}

Mesh::~Mesh()
{
	Destroy();
}

void Mesh::Create(ID3D11Device* device, MeshData meshData)
{
	mMaterials = meshData.materials;
	mWorld = meshData.world;
	mIndexCount = meshData.Indices.size();
	mVertexCount = meshData.Vertices.size();

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(Vertex)*meshData.Vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &meshData.Vertices[0]; 
	HR(device->CreateBuffer(&vbd, &vinitData, &mVB));

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(UINT) * meshData.Indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &meshData.Indices[0];
	HR(device->CreateBuffer(&ibd, &iinitData, &mIB));
}

void Mesh::Render(ID3D11DeviceContext* pd3dDeviceContext)
{
	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	pd3dDeviceContext->IASetVertexBuffers(0, 1, &mVB, &stride, &offset);
	pd3dDeviceContext->IASetIndexBuffer(mIB, DXGI_FORMAT_R32_UINT, 0);

	pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pd3dDeviceContext->DrawIndexed(mIndexCount, 0, 0);
}

void Mesh::Destroy()
{
	ReleaseCOM(mVB);
	ReleaseCOM(mIB);
	mIndexCount = 0;
	mVertexCount = 0;
	mMaterials.clear();
}
