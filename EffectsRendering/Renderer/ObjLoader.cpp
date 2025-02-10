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
    
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, fileName.c_str(), mtlBaseDir.c_str())) 
    {
        std::cerr << "Failed to load OBJ: " << err << std::endl;
        return false;
    }

    std::map<int, int> materialLookup;
    for (size_t i = 0; i < materials.size(); ++i) 
    {
        Material mat;
        tinyobj::material_t& m = materials[i];

        if (!m.diffuse_texname.empty()) {
            mat.diffuseTexture = mtlBaseDir + m.diffuse_texname;
            TextureManager::Instance()->CreateTexture(mat.diffuseTexture);
        }

        if (!m.normal_texname.empty()) {
            mat.normalTexture = mtlBaseDir + m.normal_texname;
            TextureManager::Instance()->CreateTexture(mat.normalTexture);
        }

        if (!m.bump_texname.empty()) {
            mat.bumpMapTexture = mtlBaseDir + m.bump_texname;
            TextureManager::Instance()->CreateTexture(mat.bumpMapTexture);
        }

        mat.Diffuse = XMFLOAT4(m.diffuse[0], m.diffuse[1], m.diffuse[2], 1.0f);
        mat.specExp = m.shininess;
        mat.specIntensivity = 0.25f;

        int meshMaterialID = static_cast<int>(meshData.materials.size());
        meshData.materials[meshMaterialID] = mat;
        materialLookup[i] = meshMaterialID;
    }

    // Temporary storage for tangents
    std::vector<XMFLOAT3> tan1, tan2;

    // Loop through all shapes (meshes in OBJ)
    for (size_t s = 0; s < shapes.size(); s++) 
    {
        size_t index_offset = 0;

        // Loop through faces
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) 
        {
            int fv = shapes[s].mesh.num_face_vertices[f];  // Should be 3 for triangles
            int objMaterialID = (shapes[s].mesh.material_ids.size() > f) ? shapes[s].mesh.material_ids[f] : -1;
            int meshMaterialID = (materialLookup.find(objMaterialID) != materialLookup.end()) ? materialLookup[objMaterialID] : 0;

            // Compute missing face normals
            XMFLOAT3 faceNormal(0, 0, 0);
            if (fv == 3) 
            {  // Only for triangles
                XMFLOAT3 p0, p1, p2;
                p0 = XMFLOAT3(
                    attrib.vertices[3 * shapes[s].mesh.indices[index_offset + 0].vertex_index + 0],
                    attrib.vertices[3 * shapes[s].mesh.indices[index_offset + 0].vertex_index + 1],
                    attrib.vertices[3 * shapes[s].mesh.indices[index_offset + 0].vertex_index + 2]);

                p1 = XMFLOAT3(
                    attrib.vertices[3 * shapes[s].mesh.indices[index_offset + 1].vertex_index + 0],
                    attrib.vertices[3 * shapes[s].mesh.indices[index_offset + 1].vertex_index + 1],
                    attrib.vertices[3 * shapes[s].mesh.indices[index_offset + 1].vertex_index + 2]);

                p2 = XMFLOAT3(
                    attrib.vertices[3 * shapes[s].mesh.indices[index_offset + 2].vertex_index + 0],
                    attrib.vertices[3 * shapes[s].mesh.indices[index_offset + 2].vertex_index + 1],
                    attrib.vertices[3 * shapes[s].mesh.indices[index_offset + 2].vertex_index + 2]);

                // Compute face normal
                XMFLOAT3 edge1 = XMFLOAT3(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
                XMFLOAT3 edge2 = XMFLOAT3(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z);

                faceNormal.x = edge1.y * edge2.z - edge1.z * edge2.y;
                faceNormal.y = edge1.z * edge2.x - edge1.x * edge2.z;
                faceNormal.z = edge1.x * edge2.y - edge1.y * edge2.x;
            }

            // Loop through face vertices
            for (size_t v = 0; v < fv; v++) 
            {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                Vertex vertex;
                vertex.Position.x = attrib.vertices[3 * idx.vertex_index + 0];
                vertex.Position.y = attrib.vertices[3 * idx.vertex_index + 1];
                vertex.Position.z = attrib.vertices[3 * idx.vertex_index + 2];

                // If normal is missing, use computed face normal
                if (idx.normal_index >= 0) 
                {
                    vertex.Normal.x = attrib.normals[3 * idx.normal_index + 0];
                    vertex.Normal.y = attrib.normals[3 * idx.normal_index + 1];
                    vertex.Normal.z = attrib.normals[3 * idx.normal_index + 2];
                }
                else 
                {
                    vertex.Normal = faceNormal;
                }

                // Fix UV flipping issue
                if (idx.texcoord_index >= 0) 
                {
                    vertex.Tex.x = attrib.texcoords[2 * idx.texcoord_index + 0];
                    vertex.Tex.y = 1.0f - attrib.texcoords[2 * idx.texcoord_index + 1];  // Flip V
                }

                meshData.Vertices.push_back(vertex);
                meshData.Indices.push_back(static_cast<UINT>(meshData.Vertices.size()) - 1);
            }

            // Tangent calculation for this triangle
            if (fv == 3)
            {
                Vertex& v0 = meshData.Vertices[meshData.Indices[meshData.Indices.size() - 3]];
                Vertex& v1 = meshData.Vertices[meshData.Indices[meshData.Indices.size() - 2]];
                Vertex& v2 = meshData.Vertices[meshData.Indices[meshData.Indices.size() - 1]];

                XMFLOAT3 edge1 = XMFLOAT3(v1.Position.x - v0.Position.x, v1.Position.y - v0.Position.y, v1.Position.z - v0.Position.z);
                XMFLOAT3 edge2 = XMFLOAT3(v2.Position.x - v0.Position.x, v2.Position.y - v0.Position.y, v2.Position.z - v0.Position.z);

                float du1 = v1.Tex.x - v0.Tex.x;
                float dv1 = v1.Tex.y - v0.Tex.y;
                float du2 = v2.Tex.x - v0.Tex.x;
                float dv2 = v2.Tex.y - v0.Tex.y;

                float f = 1.0f / (du1 * dv2 - du2 * dv1);

                XMFLOAT3 tangent;
                tangent.x = f * (dv2 * edge1.x - dv1 * edge2.x);
                tangent.y = f * (dv2 * edge1.y - dv1 * edge2.y);
                tangent.z = f * (dv2 * edge1.z - dv1 * edge2.z);

                // Normalize the tangent
                float length = sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
                if (length > 0.0001f)  // Avoid division by zero
                {
                    tangent.x /= length;
                    tangent.y /= length;
                    tangent.z /= length;
                }

                // Orthogonalize tangent to normal
                XMFLOAT3 normal = v0.Normal;
                float dot = normal.x * tangent.x + normal.y * tangent.y + normal.z * tangent.z;
                tangent.x -= normal.x * dot;
                tangent.y -= normal.y * dot;
                tangent.z -= normal.z * dot;

                v0.TangentU = tangent;
                v1.TangentU = tangent;
                v2.TangentU = tangent;
            }

            // Store material ID for this face
            meshData.FaceMaterials.push_back(meshMaterialID);
            index_offset += fv;
        }
    }

    meshData.world = XMMatrixIdentity();
    return true;
}
