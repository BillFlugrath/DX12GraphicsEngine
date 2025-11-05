#include "stdafx.h"
#include "DXResourceBindingUtilities.h"


#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif

#ifdef TINYOBJLOADER_IMPLEMENTATION
#undef TINYOBJLOADER_IMPLEMENTATION
#endif

#include "DXRUtilities.h"
#include "BLAS_TLAS_Utilities.h"
#include "DXShaderUtilities.h"
#include "DXD3DUtilities.h"
#include "DXResourceUtilities.h"
#include "DXRPipelineStateObject.h"
#include "Utils.h"
#include "Window.h"

DXResourceBindingUtilities::DXResourceBindingUtilities() :
	m_pSceneShaderDataCB(nullptr)	
	,m_SceneShaderData() //cpu struct SceneShaderData
	,m_pSceneShaderDataStart(nullptr) //final bytes mapped to resource for SceneShaderData
{
	
}

DXResourceBindingUtilities::~DXResourceBindingUtilities()
{
	SAFE_RELEASE(m_pSceneShaderDataCB);
}

void DXResourceBindingUtilities::CalculateShaderData(D3DSceneModels& d3dSceneModels,
	std::vector<D3DTexture>& textures)
{
	//model data
	std::vector<D3DModel>& models = d3dSceneModels.GetModelObjects();
	size_t num_mesh_objects_total = 0;
	uint32_t modelIndex = 0;

	for (auto model : models)
	{
		uint32_t num_meshes = model.GetMeshObjects().size();
		num_mesh_objects_total += num_meshes;

		m_SceneShaderData.numberOfMeshes[modelIndex] = num_meshes;
		modelIndex++;
	}

	m_SceneShaderData.numberOfModelsTotal = models.size();
	m_SceneShaderData.numberOfMeshObjectsTotal = num_mesh_objects_total;

	//texture data
	m_SceneShaderData.numberOfDiffuseTexturesTotal = textures.size();

	//map start of constant buffer to the pointer m_pSceneShaderDataStart
	HRESULT hr = m_pSceneShaderDataCB->Map(0, nullptr, reinterpret_cast<void**>(&m_pSceneShaderDataStart));
	Utils::Validate(hr, L"Error: failed to map Material constant buffer!");

	//copy data into the constant buffer
	memcpy(m_pSceneShaderDataStart, &m_SceneShaderData, sizeof(m_SceneShaderData));

	m_pSceneShaderDataCB->Unmap(0, nullptr);
}

HRESULT DXResourceBindingUtilities::CreateConstantBufferResources(D3D12Global& d3d, D3DSceneModels& d3dSceneModels,
									std::vector<D3DTexture>& textures)
{
	HRESULT hr = S_OK;

	//create d3d12 resource for the buffer
	DXResourceUtilities::Create_Constant_Buffer(d3d, &m_pSceneShaderDataCB, sizeof(SceneShaderData));

	CalculateShaderData(d3dSceneModels, textures);

	return hr;
}

void  DXResourceBindingUtilities::CreateVertexAndIndexBufferSRVsUnbound(D3D12Global& d3d, DXRGlobal& dxr, 
						D3D12Resources& resources, D3DSceneModels& d3dSceneModels,std::vector<D3DTexture>& textures)
{
	//start handle 
	D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	handle.ptr += 7 * handleIncrement;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(SceneShaderData));
	cbvDesc.BufferLocation = m_pSceneShaderDataCB->GetGPUVirtualAddress();

	//create the descriptor in the heap location "handle".  dont confuse handle (ie location of descriptor) with actual data of the buffer
	//stored at "cbvDesc.BufferLocation"
	d3d.device->CreateConstantBufferView(&cbvDesc, handle);
	
	std::vector<D3DModel>& models = d3dSceneModels.GetModelObjects();

	size_t num_models = models.size();
	size_t num_mesh_objects_total = 0;

	uint32_t modelIndex = 0;

	for (auto model : models)
	{
		std::vector<D3DMesh>& meshes = model.GetMeshObjects();
		uint32_t GeometryIndex = 0;
		
		for (auto mesh : meshes)
		{
			// Create the index buffer SRV We need an an srv since we access the vertices in shader ie ByteAddressBuffer s
			D3D12_SHADER_RESOURCE_VIEW_DESC indexSRVDesc;
			indexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			indexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			indexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			indexSRVDesc.Buffer.StructureByteStride = 0;
			indexSRVDesc.Buffer.FirstElement = 0;
			indexSRVDesc.Buffer.NumElements = (static_cast<UINT>(mesh.indices.size()) * sizeof(UINT)) / sizeof(float);
			indexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			handle.ptr += handleIncrement;
			d3d.device->CreateShaderResourceView(mesh.m_pIndexBuffer.Get(), &indexSRVDesc, handle);

			// Create the vertex buffer SRV We need an an srv since we access the verts in ByteAddressBuffer
			D3D12_SHADER_RESOURCE_VIEW_DESC vertexSRVDesc;
			vertexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			vertexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			vertexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			vertexSRVDesc.Buffer.StructureByteStride = 0;
			vertexSRVDesc.Buffer.FirstElement = 0;
			vertexSRVDesc.Buffer.NumElements = (static_cast<UINT>(mesh.vertices.size()) * sizeof(ModelVertex)) / sizeof(float);
			vertexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			handle.ptr += handleIncrement;
			d3d.device->CreateShaderResourceView(mesh.m_pVertexBuffer.Get(), &vertexSRVDesc, handle);
			

			num_mesh_objects_total++;
			GeometryIndex++;
		}

		m_SceneShaderData.numberOfMeshes[modelIndex] = GeometryIndex;
		modelIndex++;
	}

	m_SceneShaderData.numberOfMeshObjectsTotal = num_mesh_objects_total;
	m_SceneShaderData.numberOfModelsTotal = num_models;
	m_SceneShaderData.numberOfDiffuseTexturesTotal = textures.size();
	//m_SceneShaderData.diffuseTextureIndexForMesh TODO
}
