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

	HRESULT CreateConstantBufferResources(D3D12Global& d3d, D3DSceneModels& d3dSceneModels,
		std::vector<D3DTexture>& textures);

	void CalculateShaderData(D3DSceneModels& d3dSceneModels,
						std::vector<D3DTexture>& textures);

	void CreateVertexAndIndexBufferSRVsUnbound(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources,
												D3DSceneModels& d3dSceneModels,std::vector<D3DTexture>& textures);

	static const uint32_t kMaxNumMeshObjects = 512;
	static const uint32_t kMaxNumModelsObjects = 512;

	struct SceneShaderData
	{
		uint32_t numberOfModelsTotal; //total number of models in scene
		uint32_t numberOfMeshObjectsTotal;
		uint32_t numberOfDiffuseTexturesTotal;
		uint32_t pad0;  //MUST PAD!!

		//each index into the array is a model index ie InstanceID. The value at each index is the number of
		//meshes in the model ie the number of vertex buffers in the model.  For ex, if numberOfMeshes[0]=3,
		//then model 0 has 3 unique mesh objects (ie 3 unique vertex buffers).
		uint32_t numberOfMeshes[kMaxNumModelsObjects];

		//get the index into srv descriptor array for diffuse textures for a given mesh
		uint32_t diffuseTextureIndexForMesh[kMaxNumMeshObjects];

	};

protected:

	//view matrix data and gpu resource
	ID3D12Resource* m_pSceneShaderDataCB; //constant buffer that holds SceneShaderData data on gpu
	SceneShaderData	m_SceneShaderData; //cpu struct SceneShaderData
	UINT8* m_pSceneShaderDataStart; //final bytes mapped to resource for SceneShaderData
	

	//--------------------- Debug members --------------------------------------

};
