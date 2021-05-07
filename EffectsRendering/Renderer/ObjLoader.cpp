#include "ObjLoader.h"

#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
#include "tiny_obj_loader.h"

#include <iostream>

#include "TextureManager.h"

ObjLoader* ObjLoader::mInstance = 0;

ObjLoader* ObjLoader::Instance()
{
	if (mInstance == 0)
	{
		mInstance = new ObjLoader();
	}
	return mInstance;
}

ObjLoader::ObjLoader()
{
}

ObjLoader::~ObjLoader()
{
	if (mInstance != NULL)
	{
		delete mInstance;
		mInstance = NULL;
	}
}

bool ObjLoader::LoadToMesh(std::string fileName, std::string mtlBaseDir, MeshData& meshData)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string err;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, fileName.c_str(), mtlBaseDir.c_str());

	if (!err.empty()) { // `err` may contain warning message.
		std::cerr << err << std::endl;
	}

	if (!ret) {
		return false;
	}

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			int fv = shapes[s].mesh.num_face_vertices[f];

			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
				
				tinyobj::real_t tx = 0;
				tinyobj::real_t ty = 0;
				if (idx.texcoord_index >= 0)
				{
					tx = attrib.texcoords[2 * idx.texcoord_index + 0];
					ty = attrib.texcoords[2 * idx.texcoord_index + 1];
				}
				
				// Optional: vertex colors
				// tinyobj::real_t red = attrib.colors[3*idx.vertex_index+0];
				// tinyobj::real_t green = attrib.colors[3*idx.vertex_index+1];
				// tinyobj::real_t blue = attrib.colors[3*idx.vertex_index+2];

				Vertex vertex;
				vertex.Position.x = vx;
				vertex.Position.y = vy;
				vertex.Position.z = vz;
				vertex.Normal.x = nx;
				vertex.Normal.y = ny;
				vertex.Normal.z = nz;
				vertex.Tex.x = tx;
				vertex.Tex.y = ty;
				meshData.Vertices.push_back(vertex);
				meshData.Indices.push_back(index_offset + v);
				

			}
			index_offset += fv;

			// per-face material
			if (!materials.empty())
			{
				// TODO support for per-face materials
				// shapes[s].mesh.material_ids[f];
			}
		}
	}

	if (!materials.empty())
	{
		Material mat;
		tinyobj::material_t m = materials[0];
		mat.diffuseTexture = mtlBaseDir + m.diffuse_texname;
		TextureManager::Instance()->CreateTexture(mat.diffuseTexture);
		mat.Diffuse = XMFLOAT4(m.diffuse[0], m.diffuse[1], m.diffuse[2], 1.0f);
		mat.specExp = m.shininess;
		mat.specIntensivity = 0.25f;
		meshData.materials[0] = mat;
	}
	else {
		Material mat;
		mat.diffuseTexture = "";
		mat.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mat.specExp = 250.0f;
		mat.specIntensivity = 0.25f;
		meshData.materials[0] = mat;
	}

	meshData.world = XMMatrixIdentity();

	return true;
}
