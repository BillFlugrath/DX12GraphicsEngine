#pragma once

#include "Common.h"
#include "Structures.h"


using namespace DirectX;
using Microsoft::WRL::ComPtr;

class DXShaderUtilities;
class DXD3DUtilities;
class DXResourceUtilities;

class DXRPipelineStateObject
{
public:
	DXRPipelineStateObject();
	~DXRPipelineStateObject();

	void Init(std::shared_ptr< DXShaderUtilities > pDXShaderUtilities,
			  std::shared_ptr< DXD3DUtilities > pDXD3DUtilities,
			  std::shared_ptr< DXResourceUtilities> pDXResourceUtilities);


	void SetMaximumRecursionDepth(uint32_t maxDepth) { m_RecursionDepth = maxDepth; }

	void Create_Pipeline_State_Object(D3D12Global& d3d, DXRGlobal& dxr);

	void Create_Shader_Table(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources);

	void Create_Shader_Table_Unbound_Resources(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources);

	void Create_CBVSRVUAV_Heap(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources,
		D3DSceneModels& d3dSceneModels, std::vector<D3DTexture> textures);
	
	void Create_CBVSRVUAV_Heap_Unbound_Resources(D3D12Global& d3d, DXRGlobal& dxr,
		D3D12Resources& resources, D3DSceneModels& d3dSceneModels, std::vector<D3DTexture> textures);

	void Destroy(DXRGlobal& dxr);

	std::shared_ptr< DXShaderUtilities > m_pDXShaderUtilities;
	std::shared_ptr< DXD3DUtilities > m_pDXD3DUtilities;
	std::shared_ptr< DXResourceUtilities> m_pDXResourceUtilities;

	const uint32_t kMaxRecursionDepth = 2;
	uint32_t m_RecursionDepth;
	uint32_t m_NumDescriptors_CBVSRVUAV_Heap = 1024;
};
