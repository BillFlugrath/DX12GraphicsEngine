#pragma once

#include "stdafx.h"
#include "DXRVertices.h"
#include "Utils.h"

#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif

#ifdef TINYOBJLOADER_IMPLEMENTATION
#undef TINYOBJLOADER_IMPLEMENTATION
#endif

namespace std
{
	void hash_combine(size_t &seed, size_t hash)
	{
		hash += 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= hash;
	}

	template<> struct hash<ModelVertex> {
		size_t operator()(ModelVertex const &vertex) const {
			size_t seed = 0;
			hash<float> hasher;
			hash_combine(seed, hasher(vertex.position.x));
			hash_combine(seed, hasher(vertex.position.y));
			hash_combine(seed, hasher(vertex.position.z));

			hash_combine(seed, hasher(vertex.uv.x));
			hash_combine(seed, hasher(vertex.uv.y));

			return seed;
		}
	};
}

namespace Utils
{

//--------------------------------------------------------------------------------------
// Command Line Parser
//--------------------------------------------------------------------------------------

HRESULT ParseCommandLine(LPWSTR lpCmdLine, ConfigInfo &config)
{
	LPWSTR* argv = NULL;
	int argc = 0;

	argv = CommandLineToArgvW(GetCommandLine(), &argc);
	if (argv == NULL)
	{
		MessageBox(NULL, L"Unable to parse command line!", L"Error", MB_OK);
		return E_FAIL;
	}

	if (argc > 1)
	{
		char str[256];
		int i = 1;
		while (i < argc)
		{
			wcstombs(str, argv[i], 256);

			if (strcmp(str, "-width") == 0)
			{
				i++;
				wcstombs(str, argv[i], 256);
				config.width = atoi(str);
				i++;
				continue;
			}

			if (strcmp(str, "-height") == 0)
			{
				i++;
				wcstombs(str, argv[i], 256);
				config.height = atoi(str);
				i++;
				continue;
			}

			if (strcmp(str, "-model") == 0)
			{
				i++;
				wcstombs(str, argv[i], 256);
				config.model = str;
				i++;
				continue;
			}

			i++;
		}
	}
	else {
		MessageBox(NULL, L"Incorrect command line usage!", L"Error", MB_OK);
		return E_FAIL;
	}

	LocalFree(argv);
	return S_OK;
}

//--------------------------------------------------------------------------------------
// Error Messaging
//--------------------------------------------------------------------------------------

void Validate(HRESULT hr, LPWSTR msg)
{
	if (FAILED(hr))
	{
		MessageBox(NULL, msg, L"Error", MB_OK);
		PostQuitMessage(EXIT_FAILURE);
	}
}

//--------------------------------------------------------------------------------------
// File Reading
//--------------------------------------------------------------------------------------

vector<char> ReadFile(const string &filename) 
{
	ifstream file(filename, ios::ate | ios::binary);

	if (!file.is_open()) 
	{
		throw std::runtime_error("Error: failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

//--------------------------------------------------------------------------------------
// Model Loading
//--------------------------------------------------------------------------------------

void LoadModel(string filepath, Model &model, Material &material, string mtl_basedir)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;

	// Load the OBJ and MTL files
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filepath.c_str(), mtl_basedir.c_str())) 
	{
		throw std::runtime_error(err);
	}

	// Get the first material
	// Only support a single material right now

	//BillF check for no material
	if (materials.empty() == false)
	{
		material.name = materials[0].name;
		material.texturePath = materials[0].diffuse_texname;
	}
	else
	{
		material.name = "";
		material.texturePath = "";
	}


	// Parse the model and store the unique vertices
	unordered_map<ModelVertex, uint32_t> uniqueVertices = {};
	for (const auto &shape : shapes) 
	{
		for (const auto &index : shape.mesh.indices) 
		{
			ModelVertex vertex = {};
			vertex.position = 
			{
				attrib.vertices[3 * index.vertex_index + 2],				
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 0]								
			};

			vertex.uv = 
			{
				1.f - attrib.texcoords[2 * index.texcoord_index + 0],
				attrib.texcoords[2 * index.texcoord_index + 1]
			};

			// Fast find unique vertices using a hash
			if (uniqueVertices.count(vertex) == 0) 
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(model.vertices.size());
				model.vertices.push_back(vertex);
			}

			model.indices.push_back(uniqueVertices[vertex]);
		}
	}
}

//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------

/**
* Convert a three channel RGB texture to four channel RGBA
*/
void FormatTexture(TextureInfo &info, stbi_uc* pixels)
{
	const UINT rowPitch = info.width * 4;
	const UINT cellPitch = rowPitch >> 3;				// The width of a cell in the texture.
	const UINT cellHeight = info.height >> 3;			// The height of a cell in the texture.
	const UINT textureSize = rowPitch * info.height;
	const UINT pixelSize = info.width * 3 * info.height;

	info.pixels.resize(textureSize);
	info.stride = 4;

	UINT c = (pixelSize - 1);
	for (UINT n = 0; n < textureSize; n += 4)
	{
		info.pixels[n] = pixels[c - 2];			// R
		info.pixels[n + 1] = pixels[c - 1];		// G
		info.pixels[n + 2] = pixels[c];			// B
		info.pixels[n + 3] = 0xff;				// A
		c -= 3;
	}
}

/**
* Load an image
*/
TextureInfo LoadTexture(string filepath) 
{
	TextureInfo result;

	// Load image pixels with stb_image
	stbi_uc* pixels = stbi_load(filepath.c_str(), &result.width, &result.height, &result.stride, STBI_rgb);
	if (!pixels) 
	{
		throw runtime_error("Error: failed to load image!");
	}

	FormatTexture(result, pixels);
	stbi_image_free(pixels);
	return result;
}

}
