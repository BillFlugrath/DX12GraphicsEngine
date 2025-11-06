#pragma once

#include "Common.h"
#include "Structures.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;


class DXResourceUtilities
{
public:
	DXResourceUtilities();
	~DXResourceUtilities();

	//creates sampler in a sampler descriptor heap.  This descriptor heap is currently NOT used since we create
	//a static sampler when creating shader signatures.
	void Create_Samplers(D3D12Global& d3d, D3D12Resources& resources);

	void Create_BackBuffer_RTV(D3D12Global& d3d, D3D12Resources& resources);
	void Create_View_CB(D3D12Global& d3d, D3D12Resources& resources); //camera data in raygen shader

	//used to hold texture resolution in shader
	void Create_Material_CB(D3D12Global& d3d, D3D12Resources& resources, const Material& material);

	//create a constant buffer for shader data.  calls Create_Buffer with proper params
	static void Create_Constant_Buffer(D3D12Global& d3d, ID3D12Resource** buffer, UINT64 size);

	//general function to create a buffer for shader constants
	static void Create_Buffer(D3D12Global& d3d, D3D12BufferCreateInfo& info, ID3D12Resource** ppResource);

	//Create the RTV and sampler descriptor heaps.
	void Create_RTV_And_Sampler_Descriptor_Heaps(D3D12Global& d3d, D3D12Resources& resources);

	//camera data updated every frame.  Camera data used for ray gen shader.
	void UpdateCameraCB(D3D12Global& d3d, D3D12Resources& resources, XMMATRIX& view, XMFLOAT3& cam_pos, float cam_fov);

	void Destroy(D3D12Resources& resources);

	//  Create_Texture calls into the stbi_image library and is very useful since it supports a 
	// variety of file tpyes including jpg!!

	//textureResolution is an out param. 
	ID3D12Resource* Create_Texture(D3D12Global& d3d, D3D12Resources& resources, float& textureResolution, string texturePath);
	

	/// ----------------------- NOT USED.  KEEP FOR REFERENCE ------------------------------
	void Create_Transform_Buffer(D3D12Global& d3d, D3D12Resources& resources);
	void Create_Texture(D3D12Global& d3d, D3D12Resources& resources, Material& material);
	void Create_Vertex_Buffer(D3D12Global& d3d, D3D12Resources& resources, Model& model);
	void Create_Index_Buffer(D3D12Global& d3d, D3D12Resources& resources, Model& model);

};
