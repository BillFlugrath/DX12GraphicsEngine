#pragma once

#include "Structures.h"

#include <stb_image.h>
#include <tiny_obj_loader.h>

//--------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------

namespace Utils
{
	HRESULT ParseCommandLine(LPWSTR lpCmdLine, ConfigInfo &config);

	vector<char> ReadFile(const string &filename);

	void LoadModel(string filepath, Model &model, Material &material, string mtl_basedir);

	void Validate(HRESULT hr, LPWSTR message);

	void FormatTexture(TextureInfo &info, stbi_uc* pixels);
	TextureInfo LoadTexture(string filepath);
}