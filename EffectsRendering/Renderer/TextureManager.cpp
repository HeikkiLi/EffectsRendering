#include "TextureManager.h"

#include "../DirectXTex/WICTextureLoader/WICTextureLoader.h"
#include "DirectXTex/DDSTextureLoader/DDSTextureLoader.h"

TextureManager *TextureManager::mInstance = 0;

TextureManager* TextureManager::Instance()
{
	if (mInstance == 0)
	{
		mInstance = new TextureManager();
	}
	return mInstance;
}

TextureManager::TextureManager() : md3dDevice(0)
{
}

TextureManager::~TextureManager()
{
	for (auto it = mTextureSRVs.begin(); it != mTextureSRVs.end(); ++it)
	{
		ReleaseCOM(it->second);
	}

	mTextureSRVs.clear();
}

void TextureManager::Init(ID3D11Device* device)
{
	md3dDevice = device;
}

ID3D11ShaderResourceView* TextureManager::CreateTexture(std::string filename)
{
	if (md3dDevice)
	{
		ID3D11ShaderResourceView* srv = 0;

		if (mTextureSRVs.find(filename) != mTextureSRVs.end())
		{
			srv = mTextureSRVs[filename];
		}
		else
		{
			ID3D11Resource* texture = 0;
			std::wstring wstrFilename = std::wstring(filename.begin(), filename.end());

			size_t pos = 0;
			pos = filename.find_last_of('.');
			std::string strEnding = filename.substr(pos, filename.length());

			if (strEnding == ".dds")
			{
				DirectX::CreateDDSTextureFromFile(md3dDevice, wstrFilename.c_str(), &texture, &srv);
				/*
				size_t maxsize = 0;
				DDS_ALPHA_MODE* alphaMode = nullptr;
				DirectX::CreateDDSTextureFromFileEx(md3dDevice, nullptr,
					wstrFilename.c_str(),
					maxsize,
					D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_TEXTURECUBE,
					false,
					&texture, &srv, alphaMode);
					*/
			}
			else {
				DirectX::CreateWICTextureFromFile(md3dDevice, wstrFilename.c_str(), &texture, &srv);
			}

			mTextureSRVs[filename] = srv;
		}

		return srv;
	}
}

ID3D11ShaderResourceView* TextureManager::GetTexture(std::string filename)
{
	return mTextureSRVs[filename];
}

void TextureManager::Release()
{
	for (auto& kv : mTextureSRVs)
	{
		if (kv.second != NULL)
		{
			ReleaseCOM(kv.second);
			kv.second = NULL;
		}
	}
	
}