#pragma once

#include "Util.h"
#include <map>

// TextureManager
// Loads texture from file using WICTextureLoader and saves
// textures as ID3D11ShaderResourceView to textures std::map object
// so that for each filename there is only one texture and it is not loaded multiple times
// new texture is created only if map does not hold texture with filename if there is value in map
// the createTexture method returns it without loading new one from disk.
class TextureManager
{
public:
	static TextureManager* Instance();

	void Init(ID3D11Device* device);

	ID3D11ShaderResourceView* CreateTexture(std::string filename);

	ID3D11ShaderResourceView* GetTexture(std::string filename);

	void Release();

private:

	static TextureManager* mInstance;

	TextureManager();
	~TextureManager();

	TextureManager(const TextureManager& rhs);

	ID3D11Device* md3dDevice;
	std::map<std::string, ID3D11ShaderResourceView*> mTextureSRVs;
};