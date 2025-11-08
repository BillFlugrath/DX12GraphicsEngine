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
	,m_pSceneTextureShaderDataCB(nullptr)
	,m_SceneTextureShaderData() 
	,m_pSceneTextureShaderDataStart(nullptr) //final bytes mapped to resource
{
	
}

DXResourceBindingUtilities::~DXResourceBindingUtilities()
{
	SAFE_RELEASE(m_pSceneShaderDataCB);
	SAFE_RELEASE(m_pSceneTextureShaderDataCB);
}

void DXResourceBindingUtilities::CalculateShaderData(D3DSceneModels& d3dSceneModels, D3DSceneTextures& textures2D)
{
	//model data
	std::vector<D3DModel>& models = d3dSceneModels.GetModelObjects();
	size_t num_mesh_objects_total = 0;
	uint32_t modelIndex = 0;
	uint32_t meshIndex = 0;

	for (auto &model : models)
	{
		uint32_t num_meshes =(uint32_t)model.GetMeshObjects().size();
		num_mesh_objects_total += num_meshes;

		m_SceneShaderData.numberOfMeshes[modelIndex].x = num_meshes;
		modelIndex++;

		std::vector<D3DMesh> meshObjects = model.GetMeshObjects();
		for (auto &mesh : meshObjects)
		{
			uint32_t diffuseIndex = mesh.m_DiffuseTexIndex;

			m_SceneTextureShaderData.diffuseTextureIndexForMesh[meshIndex].x = diffuseIndex; //store albedo texture for this mesh
			meshIndex++;
		}
	}

	m_SceneShaderData.numberOfModelsTotal = (uint32_t)models.size();
	m_SceneShaderData.numberOfMeshObjectsTotal = (uint32_t)num_mesh_objects_total;

	m_SceneShaderData.numberOfDiffuseTexturesTotal = (uint32_t)textures2D.GetTextureObjects().size(); //scalar float array size

	//map start of constant buffer to the pointer m_pSceneShaderDataStart
	HRESULT hr = m_pSceneShaderDataCB->Map(0, nullptr, reinterpret_cast<void**>(&m_pSceneShaderDataStart));
	Utils::Validate(hr, L"Error: failed to map Material constant buffer!");

	//map start of constant buffer to the pointer m_pSceneShaderDataStart
	hr = m_pSceneTextureShaderDataCB->Map(0, nullptr, reinterpret_cast<void**>(&m_pSceneTextureShaderDataStart));
	Utils::Validate(hr, L"Error: failed to map Material constant buffer!");

	//copy data into the constant buffer
	memcpy(m_pSceneShaderDataStart, &m_SceneShaderData, sizeof(m_SceneShaderData));

	m_pSceneShaderDataCB->Unmap(0, nullptr);


	//copy data into the constant buffer
	memcpy(m_pSceneTextureShaderDataStart, &m_SceneTextureShaderData, sizeof(m_SceneTextureShaderData));

	m_pSceneTextureShaderDataCB->Unmap(0, nullptr);

}

HRESULT DXResourceBindingUtilities::CreateConstantBufferResources(D3D12Global& d3d, D3DSceneModels& d3dSceneModels,
									D3DSceneTextures& textures2D)
{
	HRESULT hr = S_OK;

	//create d3d12 resource for the buffer
	DXResourceUtilities::Create_Constant_Buffer(d3d, &m_pSceneShaderDataCB, sizeof(SceneShaderData));
	DXResourceUtilities::Create_Constant_Buffer(d3d, &m_pSceneTextureShaderDataCB, sizeof(SceneTextureShaderData));

	CalculateShaderData(d3dSceneModels, textures2D);

	return hr;
}

void  DXResourceBindingUtilities::CreateVertexAndIndexBufferSRVs(D3D12Global& d3d, DXRGlobal& dxr, 
						D3D12Resources& resources, D3DSceneModels& d3dSceneModels, D3DSceneTextures& textures2D)
{
	//start handle 
	D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	uint32_t startDescriptorIndex = 7;
	handle.ptr += startDescriptorIndex * handleIncrement; //start after the Bound Resources' Descriptors in the heap!!

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(SceneShaderData));
	cbvDesc.BufferLocation = m_pSceneShaderDataCB->GetGPUVirtualAddress();


	if (_DEBUG)
		m_pSceneShaderDataCB->SetName(L"m_pSceneShaderDataCB");

	//create the descriptor in the heap location "handle".  dont confuse handle (ie location of descriptor) with actual data of the buffer
	//stored at "cbvDesc.BufferLocation".  The constant buffer view is for the SceneShaderData CB
	d3d.device->CreateConstantBufferView(&cbvDesc, handle);

	//make next CB for TextureShaderData
	cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(SceneTextureShaderData));
	cbvDesc.BufferLocation = m_pSceneTextureShaderDataCB->GetGPUVirtualAddress();
	

	if (_DEBUG)
		m_pSceneTextureShaderDataCB->SetName(L"m_pSceneTextureShaderDataCB");

	handle.ptr += handleIncrement;
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

		m_SceneShaderData.numberOfMeshes[modelIndex].x = GeometryIndex;
		modelIndex++;
	}


	//Store textures2d in array
	std::vector<D3DTexture> textures = textures2D.GetTextureObjects();

	for (auto tex : textures)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc = {};
		textureSRVDesc.Format =  tex.m_pTextureResource.Get()->GetDesc().Format; 
		textureSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		textureSRVDesc.Texture2D.MipLevels = tex.m_pTextureResource.Get()->GetDesc().MipLevels; 
		textureSRVDesc.Texture2D.MostDetailedMip = 0;
		textureSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		handle.ptr += handleIncrement;
		d3d.device->CreateShaderResourceView(tex.m_pTextureResource.Get(), &textureSRVDesc, handle);
	}

	m_SceneShaderData.numberOfMeshObjectsTotal = (uint32_t)num_mesh_objects_total;
	m_SceneShaderData.numberOfModelsTotal = (uint32_t)num_models;
	m_SceneShaderData.numberOfDiffuseTexturesTotal = (uint32_t)textures.size();
}


/**
* Create the DXR descriptor heap for CBVs, SRVs, and the output UAV.
*/

void DXResourceBindingUtilities::Create_CBVSRVUAV_Heap(D3D12Global& d3d, DXRGlobal& dxr,
	D3D12Resources& resources, D3DSceneModels& d3dSceneModels, std::vector<D3DTexture> textures)
{
	// Describe the CBV/SRV/UAV heap
	// Need 11 entries:
	// 1 CBV for the ViewCB
	// 1 CBV for the MaterialCB
	// 1 UAV for the RT output
	// 1 SRV for the Scene BVH
	// 1 SRV for the texture
	// 1 SRV for the texture 1
	// 1 SRV for the cube texture

	UINT numDescriptors = m_NumDescriptors_CBVSRVUAV_Heap;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	// Create the descriptor heap
	HRESULT hr = d3d.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&resources.cbvSrvUavHeap));
	Utils::Validate(hr, L"Error: failed to create DXR CBV/SRV/UAV descriptor heap!");

	// Get the descriptor heap handle and increment size
	D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Create the ViewCB CBV (ie create a descriptor that points to data at "BufferLocation"
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(resources.viewCBData));
	cbvDesc.BufferLocation = resources.viewCB->GetGPUVirtualAddress();

	//create the descriptor in the heap location "handle".  dont confuse handle (ie location of descriptor) with actual data of the buffer
	//stored at "cbvDesc.BufferLocation"
	d3d.device->CreateConstantBufferView(&cbvDesc, handle);

	// Create the MaterialCB CBV
	cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(resources.materialCBData));
	cbvDesc.BufferLocation = resources.materialCB->GetGPUVirtualAddress();

	handle.ptr += handleIncrement;
	d3d.device->CreateConstantBufferView(&cbvDesc, handle);

	// Create the DXR output buffer UAV
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	handle.ptr += handleIncrement;
	d3d.device->CreateUnorderedAccessView(resources.DXROutput, nullptr, &uavDesc, handle);

	// Create the DXR Top Level Acceleration Structure SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = dxr.TLAS.pResult->GetGPUVirtualAddress();

	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(nullptr, &srvDesc, handle);

	// Create the material texture SRV : register(t3);
	D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc = {};
	textureSRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	textureSRVDesc.Texture2D.MipLevels = 1;
	textureSRVDesc.Texture2D.MostDetailedMip = 0;
	textureSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(textures[0].m_pTextureResource.Get(), &textureSRVDesc, handle);


	// Create the material texture_1 SRV : register(t6);
	D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc2 = {};
	textureSRVDesc2.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureSRVDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	textureSRVDesc2.Texture2D.MipLevels = 1;
	textureSRVDesc2.Texture2D.MostDetailedMip = 0;
	textureSRVDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(textures[1].m_pTextureResource.Get(), &textureSRVDesc2, handle);

	// Create the cube map 0 texture SRV : register(t7);
	D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc3 = {};
	textureSRVDesc3.Format = textures[2].m_pTextureResource.Get()->GetDesc().Format;
	textureSRVDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	textureSRVDesc3.Texture2D.MipLevels = textures[2].m_pTextureResource.Get()->GetDesc().MipLevels;
	textureSRVDesc3.Texture2D.MostDetailedMip = 0;
	textureSRVDesc3.Texture2D.ResourceMinLODClamp = 0.0f;
	textureSRVDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;


	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(textures[2].m_pTextureResource.Get(), &textureSRVDesc3, handle);
}
