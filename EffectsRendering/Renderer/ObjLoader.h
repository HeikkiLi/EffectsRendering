#pragma once

#include "Util.h"
#include "Mesh.h"


// ObjLoader
// singleton class, usage:
// ObjLoader::Instance()->LoadToMesh("..\\Assets\\bunny.obj",  "..\\Assets\\", meshData)
// loads .obj file to MeshData object
class ObjLoader
{
public:
	static ObjLoader* Instance();

	bool LoadToMesh(std::string fileName, std::string mtlBaseDir, MeshData& meshData);

private:
	ObjLoader();
	~ObjLoader();

	static ObjLoader* mInstance;
};