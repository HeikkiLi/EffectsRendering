#pragma once

#include "Util.h"
#include "Mesh.h"

// GeometryGenerator
// generates simple mesh objects
class GeometryGenerator
{
public:

	// singleton
	static GeometryGenerator* Instance();

	void CreateBox(float width, float height, float depth, MeshData& meshData);

	void CreateGrid(float width, float depth, UINT m, UINT n, MeshData& meshData);

	void CreateSphere(float radius, UINT sliceCount, UINT stackCount, MeshData& meshData);

private:
	GeometryGenerator();
	~GeometryGenerator();

	static GeometryGenerator* mInstance;

};