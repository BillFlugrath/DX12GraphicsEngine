#pragma once

#include "Common.h"
#include "Structures.h"
#include <string>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class DXShaderUtilities;
class DXD3DUtilities;
class DXResourceUtilities;
class DXRUtilities;
class BLAS_TLAS_Utilities;
class DXRPipelineStateObject;

class DXResourceBindingUtilities
{
public:
	DXResourceBindingUtilities();
	~DXResourceBindingUtilities();

	HRESULT CreateConstantBufferResources(D3D12Global& d3d, D3DSceneModels& d3dSceneModels, D3DSceneTextures& textures2D);

	void CalculateShaderData(D3DSceneModels& d3dSceneModels, D3DSceneTextures& textures2D);

	void CreateVertexAndIndexBufferSRVs(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources,
												D3DSceneModels& d3dSceneModels, D3DSceneTextures& textures2D);

	void Create_CBVSRVUAV_Heap(D3D12Global& d3d, DXRGlobal& dxr,
		D3D12Resources& resources, D3DSceneModels& d3dSceneModels, std::vector<D3DTexture> textures);


	static const uint32_t kShaderDataArraySize = 128;
	 
	struct SceneShaderData
	{
		uint32_t numberOfModelsTotal; //total number of models in scene
		uint32_t numberOfMeshObjectsTotal;
		uint32_t numberOfDiffuseTexturesTotal;
		uint32_t pad0;  //MUST PAD!!

		//each index into the array is a model index ie InstanceID. The value at each index is the number of
		//meshes in the model ie the number of vertex buffers in the model.  For ex, if numberOfMeshes[0]=3,
		//then model 0 has 3 unique mesh objects (ie 3 unique vertex buffers).

		//CB arrays must have EACH element on 16 byte boundary, thus we are using XMUINT4
		XMUINT4 numberOfMeshes[kShaderDataArraySize];  //only use x component
	};


	// struct to hold an array of uint32_t indices.  Attempts to use diffuseTextureIndexForMesh in SceneShaderData
	// resulted in the data not appearing in the shader.  Thus, a new constant buffer was used.
	struct SceneTextureShaderData
	{
		//store the index into srv descriptor array for diffuse textures for all mesh objs.  For example, for mesh 5,
		//uint texIndex=diffuseTextureIndexForMesh[5].x.  Thus, we lookup the index of the albedo texture used by mesh 5.
		//We then use that index into the actual texture array "diffuse_textures"  For ex, if texIndex=1, then the
		//texture2D resource is accessed via diffuse_textures[1] ie diffuse_textures[texIndex].

		//CB arrays must have EACH element on 16 byte boundary, thus we are using XMUINT4
		XMUINT4 diffuseTextureIndexForMesh[kShaderDataArraySize]; //only use x component
	};

protected:

	ID3D12Resource* m_pSceneShaderDataCB; //constant buffer that holds SceneShaderData data on gpu
	SceneShaderData	m_SceneShaderData; //cpu struct SceneShaderData
	UINT8* m_pSceneShaderDataStart; //final bytes mapped to resource for SceneShaderData

	ID3D12Resource* m_pSceneTextureShaderDataCB; //constant buffer that holds SceneTextureShaderData data on gpu
	SceneTextureShaderData	m_SceneTextureShaderData; //cpu struct SceneTextureShaderData
	UINT8* m_pSceneTextureShaderDataStart; //final bytes mapped to resource for SceneTextureShaderData
	

	uint32_t m_NumDescriptors_CBVSRVUAV_Heap = 1024;
	//--------------------- Debug members --------------------------------------

};
