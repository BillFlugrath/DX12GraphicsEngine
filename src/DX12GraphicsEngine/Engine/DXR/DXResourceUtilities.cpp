#include "stdafx.h"


#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif

#ifdef TINYOBJLOADER_IMPLEMENTATION
#undef TINYOBJLOADER_IMPLEMENTATION
#endif


#include "DXResourceUtilities.h"
#include "Utils.h"



DXResourceUtilities::DXResourceUtilities() 
{
}

DXResourceUtilities::~DXResourceUtilities()
{
}

/**
* Create a GPU buffer resource.
*/
void DXResourceUtilities::Create_Buffer(D3D12Global& d3d, D3D12BufferCreateInfo& info, ID3D12Resource** ppResource)
{
	HRESULT hr;

	D3D12_HEAP_PROPERTIES heapDesc = {};
	heapDesc.Type = info.heapType;
	heapDesc.CreationNodeMask = 1;
	heapDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = info.alignment;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Width = info.size;  //number of bytes to allocate for buffer
	resourceDesc.Flags = info.flags;

	// Create the GPU resource
	hr = d3d.device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &resourceDesc, info.state, nullptr, IID_PPV_ARGS(ppResource));
	Utils::Validate(hr, L"Error: failed to create buffer resource!");
}

ID3D12Resource* DXResourceUtilities::Create_Texture(D3D12Global& d3d, D3D12Resources& resources,
	float& textureResolution, string texturePath)
{
	HRESULT hr;
	TextureInfo texture;

	// Load the texture
	if (texturePath == "")
	{
		printf("Texture path string is empty!  Using default texture.\n");
		texturePath = "./Engine/DXR/assets/textures/happy_fish.jpg";
	}
	texture = Utils::LoadTexture(texturePath);
	textureResolution =			static_cast<float>(texture.width);

	// Describe the texture
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.Width = texture.width;
	textureDesc.Height = texture.height;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	// Create the texture resource
	hr = d3d.device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resources.texture));
	Utils::Validate(hr, L"Error: failed to create texture!");

#if _DEBUG
	resources.texture->SetName(L"Texture resource");
#endif

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(resources.texture, 0, 1);

	// Describe the resource
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = uploadBufferSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	// Create the upload heap
	hr = d3d.device->CreateCommittedResource(&UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resources.textureUploadHeap));
	Utils::Validate(hr, L"Error: failed to create texture upload heap!");

	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = texture.pixels.data();
	textureData.RowPitch = texture.width * texture.stride;
	textureData.SlicePitch = textureData.RowPitch * texture.height;

	// Schedule a copy from the upload heap to the Texture2D resource
	UpdateSubresources(d3d.cmdList, resources.texture, resources.textureUploadHeap, 0, 0, 1, &textureData);

	// Transition the texture to a shader resource
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resources.texture;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

	d3d.cmdList->ResourceBarrier(1, &barrier);

	return resources.texture;
}


/*
* Create a constant buffer.
*/
void DXResourceUtilities::Create_Constant_Buffer(D3D12Global& d3d, ID3D12Resource** buffer, UINT64 size)
{
	D3D12BufferCreateInfo bufferInfo((size + 255) & ~255, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	Create_Buffer(d3d, bufferInfo, buffer);
}

/**
* Create the back buffer's RTV view.
*/
void DXResourceUtilities::Create_BackBuffer_RTV(D3D12Global& d3d, D3D12Resources& resources)
{
	HRESULT hr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;

	//get the start of the rtv descriptor heap.  we use this address to create the first rtv descriptor.
	rtvHandle = resources.rtvHeap->GetCPUDescriptorHandleForHeapStart();

	// Create a RTV for each back buffer
	for (UINT n = 0; n < 2; n++)
	{
		//get the frame buffer at index n.  this frame buffer is part of the swap chain.
		hr = d3d.swapChain->GetBuffer(n, IID_PPV_ARGS(&d3d.backBuffer[n]));
		if (FAILED(hr)) {
			throw runtime_error("Failed to get swap chain buffer!");
		}

		//Create a descriptor (i.e.  render target view ) to the frame buffer.
		d3d.device->CreateRenderTargetView(d3d.backBuffer[n], nullptr, rtvHandle);
		rtvHandle.ptr += (1 * resources.rtvDescSize);
	}
}

/**
* Create the samplers.
*/
void DXResourceUtilities::Create_Samplers(D3D12Global& d3d, D3D12Resources& resources)
{
	// Get the sampler descriptor heap handle
	D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.samplerHeap->GetCPUDescriptorHandleForHeapStart();

	// Describe the sampler
	D3D12_SAMPLER_DESC desc = {};
	memset(&desc, 0, sizeof(desc));
	desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.MipLODBias = 0.f;
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	desc.MinLOD = 0.f;
	desc.MaxLOD = D3D12_FLOAT32_MAX;
	desc.MaxAnisotropy = 1;

	// Create the sampler
	d3d.device->CreateSampler(&desc, handle);
}

/**
* Create and initialize the view constant buffer.
*/
void DXResourceUtilities::Create_View_CB(D3D12Global& d3d, D3D12Resources& resources)
{
	Create_Constant_Buffer(d3d, &resources.viewCB, sizeof(ViewCB));

	HRESULT hr = resources.viewCB->Map(0, nullptr, reinterpret_cast<void**>(&resources.viewCBStart));
	Utils::Validate(hr, L"Error: failed to map View constant buffer!");

	memcpy(resources.viewCBStart, &resources.viewCBData, sizeof(resources.viewCBData));
}

/**
* Create and initialize the material constant buffer.
*/
void DXResourceUtilities::Create_Material_CB(D3D12Global& d3d, D3D12Resources& resources, const Material& material)
{
	Create_Constant_Buffer(d3d, &resources.materialCB, sizeof(MaterialCB));

	resources.materialCBData.resolution = XMFLOAT4(material.textureResolution, 0.f, 0.f, 0.f);

	//map start of constant buffer to the pointer resources.materialCBStart
	HRESULT hr = resources.materialCB->Map(0, nullptr, reinterpret_cast<void**>(&resources.materialCBStart));
	Utils::Validate(hr, L"Error: failed to map Material constant buffer!");

	//copy data into the constant buffer
	memcpy(resources.materialCBStart, &resources.materialCBData, sizeof(resources.materialCBData));
}

/**
* Create the RTV and sampler descriptor heaps.
*/
void DXResourceUtilities::Create_RTV_And_Sampler_Descriptor_Heaps(D3D12Global& d3d, D3D12Resources& resources)
{
	// Describe the RTV heap
	D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
	rtvDesc.NumDescriptors = 2;
	rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	// Create the RTV heap
	HRESULT hr = d3d.device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&resources.rtvHeap));
	Utils::Validate(hr, L"Error: failed to create RTV descriptor heap!");

	resources.rtvDescSize = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Describe the sampler heap
	D3D12_DESCRIPTOR_HEAP_DESC samplerDesc = {};
	samplerDesc.NumDescriptors = 1;
	samplerDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	samplerDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	// Create the sampler heap
	hr = d3d.device->CreateDescriptorHeap(&samplerDesc, IID_PPV_ARGS(&resources.samplerHeap));
	Utils::Validate(hr, L"Error: failed to create sampler descriptor heap!");
}

/**
* Update the view constant buffer.
*/
void DXResourceUtilities::UpdateCameraCB(D3D12Global& d3d, D3D12Resources& resources, XMMATRIX& view, XMFLOAT3& cam_pos, float cam_fov)
{
	XMMATRIX  invView;
	XMFLOAT3 eye = cam_pos;
	float  fov = cam_fov ;

	invView = XMMatrixInverse(NULL, view);

	resources.viewCBData.view = XMMatrixTranspose(invView);
	resources.viewCBData.viewOriginAndTanHalfFovY = XMFLOAT4(eye.x, eye.y, eye.z, tanf(fov * 0.5f));
	resources.viewCBData.resolution = XMFLOAT2((float)d3d.width, (float)d3d.height);
	memcpy(resources.viewCBStart, &resources.viewCBData, sizeof(resources.viewCBData));
}


/**
 * Release the resources.
 */
void DXResourceUtilities::Destroy(D3D12Resources& resources)
{
	if (resources.viewCB) resources.viewCB->Unmap(0, nullptr);
	if (resources.viewCBStart) resources.viewCBStart = nullptr;
	if (resources.materialCB) resources.materialCB->Unmap(0, nullptr);
	if (resources.materialCBStart) resources.materialCBStart = nullptr;

	SAFE_RELEASE(resources.viewCB);
	SAFE_RELEASE(resources.materialCB);

	SAFE_RELEASE(resources.DXROutput);
	SAFE_RELEASE(resources.vertexBuffer);
	SAFE_RELEASE(resources.indexBuffer);
	SAFE_RELEASE(resources.rtvHeap);
	SAFE_RELEASE(resources.cbvSrvUavHeap);
	SAFE_RELEASE(resources.samplerHeap);
	SAFE_RELEASE(resources.transform);
	SAFE_RELEASE(resources.transformUploadHeap);
	SAFE_RELEASE(resources.texture);
	SAFE_RELEASE(resources.textureUploadHeap);
}

/// ----------------------- NOT USED.  KEEP FOR REFERENCE ------------------------------


/**
* Create a matrix transform buffer and copy a transform to the upload heap.
*/
void DXResourceUtilities::Create_Transform_Buffer(D3D12Global& d3d, D3D12Resources& resources)
{
	HRESULT hr;

	D3D12_RESOURCE_DESC matrixDesc = {};
	matrixDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	matrixDesc.Alignment = 0;
	matrixDesc.Width = sizeof(XMMATRIX);
	matrixDesc.Height = 1;
	matrixDesc.DepthOrArraySize = 1;
	matrixDesc.MipLevels = 1;
	matrixDesc.Format = DXGI_FORMAT_UNKNOWN;
	matrixDesc.SampleDesc.Count = 1;
	matrixDesc.SampleDesc.Quality = 0;
	matrixDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	matrixDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	// Create the matrix transform buffer
	hr = d3d.device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &matrixDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resources.transform));
	Utils::Validate(hr, L"Error: failed to create matrix transform resource!");

	// Create the matrix transform upload heap
	hr = d3d.device->CreateCommittedResource(&UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &matrixDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resources.transformUploadHeap));
	Utils::Validate(hr, L"Failed to create matrix transform upload heap!");

	// Copy the transform matrix to the upload heap
	XMFLOAT4X4* pMatrix(nullptr);
	resources.transformUploadHeap->Map(0, nullptr, (void**)&pMatrix);
	::XMStoreFloat4x4(pMatrix, XMMatrixIdentity());
	resources.transformUploadHeap->Unmap(0, nullptr);

	D3D12_RESOURCE_BARRIER barrierDesc = {};
	barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Transition.pResource = resources.transform;
	barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

	d3d.cmdList->CopyBufferRegion(resources.transform, 0, resources.transformUploadHeap, 0, sizeof(XMMATRIX));
	d3d.cmdList->ResourceBarrier(1, &barrierDesc);
}


/*
* Create the vertex buffer.
*/
void DXResourceUtilities::Create_Vertex_Buffer(D3D12Global& d3d, D3D12Resources& resources, Model& model)
{
	// Create the buffer resource from the model's vertices
	D3D12BufferCreateInfo info(((UINT)model.vertices.size() * sizeof(ModelVertex)), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	Create_Buffer(d3d, info, &resources.vertexBuffer);

#if defined(_DEBUG)
	resources.vertexBuffer->SetName(L"VertexBuffer");
#endif

	// Copy the vertex data to the vertex buffer
	UINT8* pVertexDataBegin;
	D3D12_RANGE readRange = {};
	HRESULT hr = resources.vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
	Utils::Validate(hr, L"Error: failed to map vertex buffer!");

	memcpy(pVertexDataBegin, model.vertices.data(), info.size);
	resources.vertexBuffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view
	resources.vertexBufferView.BufferLocation = resources.vertexBuffer->GetGPUVirtualAddress();
	resources.vertexBufferView.StrideInBytes = sizeof(ModelVertex);
	resources.vertexBufferView.SizeInBytes = static_cast<UINT>(info.size);
}

/**
* Create the index buffer.
*/
void DXResourceUtilities::Create_Index_Buffer(D3D12Global& d3d, D3D12Resources& resources, Model& model)
{
	// Create the index buffer resource
	D3D12BufferCreateInfo info((UINT)model.indices.size() * sizeof(UINT), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	Create_Buffer(d3d, info, &resources.indexBuffer);

#if defined(_DEBUG)
	resources.indexBuffer->SetName(L"IndexBuffer");
#endif

	// Copy the index data to the index buffer
	UINT8* pIndexDataBegin;
	D3D12_RANGE readRange = {};
	HRESULT hr = resources.indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
	Utils::Validate(hr, L"Error: failed to map index buffer!");

	memcpy(pIndexDataBegin, model.indices.data(), info.size);
	resources.indexBuffer->Unmap(0, nullptr);

	// Initialize the index buffer view
	resources.indexBufferView.BufferLocation = resources.indexBuffer->GetGPUVirtualAddress();
	resources.indexBufferView.SizeInBytes = static_cast<UINT>(info.size);
	resources.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
}

/**
* Create a texture.
*/
void DXResourceUtilities::Create_Texture(D3D12Global& d3d, D3D12Resources& resources, Material& material)
{
	//material.textureResolution is an out parameter
	Create_Texture(d3d, resources, material.textureResolution, material.texturePath);
}

