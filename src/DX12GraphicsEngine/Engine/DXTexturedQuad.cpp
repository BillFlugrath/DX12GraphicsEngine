#include "stdafx.h"
#include "DXTexturedQuad.h"
#include "DXTexture.h"
#include "DXGraphicsUtilities.h"
#include "DXCamera.h"
#include "DXDescriptorHeap.h"
#include "../DXSampleHelper.h"

const wchar_t* kDefaultVertexShaderFile = L"assets/shaders/quadShaders.hlsl";
const wchar_t* kDefaultPixelShaderFile = L"assets/shaders/quadShaders.hlsl";

const char* kDefaultVertexShaderEntryPoint = "VSMain";
const char* kDefaultPixelShaderEntryPoint = "PSMain";

DXTexturedQuad::DXTexturedQuad() :
	m_QuadViewport(0.0f, 0.0f, 0.0f, 0.0f)
	,m_QuadScissorRect(0, 0, 0, 0)
	, m_DXTexture(nullptr)
	, m_pConstantBufferData(nullptr)
{
	m_DXTexture =  std::make_shared<DXTexture>();

	m_WorldMatrix = XMMatrixIdentity();
}

DXTexturedQuad::~DXTexturedQuad()
{

}

void DXTexturedQuad::SetTexture(std::shared_ptr<DXTexture>& pTexture)
{ 
	m_DXTexture = pTexture; 
}

ComPtr<ID3D12RootSignature> DXTexturedQuad::SetRootSignature(ComPtr<ID3D12RootSignature> quadRootSignature)
{
	ComPtr<ID3D12RootSignature> previous = m_QuadRootSignature;
	m_QuadRootSignature = quadRootSignature;
	return previous;
}

ComPtr<ID3D12PipelineState> DXTexturedQuad::SetPipelineState(ComPtr<ID3D12PipelineState> quadPipelineState)
{
	ComPtr<ID3D12PipelineState> previous = m_QuadPipelineState;
	m_QuadPipelineState = quadPipelineState;
	return previous;
}

void DXTexturedQuad::CreateQuad(ComPtr<ID3D12Device> &device,
	ComPtr<ID3D12CommandQueue> &commandQueue,
	DXDescriptorHeap * descriptor_heap_srv,
	CD3DX12_VIEWPORT QuadViewport,
	CD3DX12_RECT QuadScissorRect, const std::wstring& assetPath)
{
	m_device = device;
	m_commandQueue = commandQueue;
	m_cbvSrvHeap = descriptor_heap_srv->GetDescriptorHeap();
	m_QuadViewport = QuadViewport;
	m_QuadScissorRect = QuadScissorRect;

	m_cbDescriptorIndex = descriptor_heap_srv->GetNewDescriptorIndex();

	m_assetsPath = assetPath;

	
	CreateRootSignature();

	CreatePipelineState(kDefaultVertexShaderFile, kDefaultPixelShaderFile, kDefaultVertexShaderEntryPoint, kDefaultPixelShaderEntryPoint);

	// Define the geometry for a fullscreen quad.
	const std::vector<QuadVertex> quadVertices =
	{
		{ { -1.0f, -1.0f, 0.0f, 1.0f },{ 0.0f, 0.0f } },    // Bottom left.
		{ { -1.0f, 1.0f, 0.0f, 1.0f },{ 0.0f, 1.0f } },    // Top left.
		{ { 1.0f, -1.0f, 0.0f, 1.0f },{ 1.0f, 0.0f } },    // Bottom right.
		{ { 1.0f, 1.0f, 0.0f, 1.0f },{ 1.0f, 1.0f } }        // Top right.
	};

	CreateQuadVertices(quadVertices);

	CreateConstantBuffer();

	m_cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DXTexturedQuad::CreateQuadVertices(const std::vector<QuadVertex> & quadVertices)
{
	// Single-use command allocator and command list for creating resources.
	//These are used temporarily to copy the vertex buffer from a temporary upload heap into a final heap that will hold the buffer.
	//The upload heap allows for CPU writing.  The default heap stores the GPU only accessible heap that holds final vertex buffer.
	//The copy occurs via a command list's CopyBufferRegion function.  Thus, we need a command list "commandList" to do this copy.
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> commandList;

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));


	// Create/update the vertex buffer.
	ComPtr<ID3D12Resource> quadVertexBufferUpload;
	{
		const UINT vertexBufferSize = quadVertices.size() * sizeof(QuadVertex);

		//temp heap to upload the vertex array.  we then copy this data ito final gpu vertuex buffer.
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&quadVertexBufferUpload)));

		//Final GPU heap to store the vertex buffer
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_QuadVertexBuffer)));


		NAME_D3D12_OBJECT(m_QuadVertexBuffer);

		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the vertex buffer.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(quadVertexBufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, quadVertices.data(), vertexBufferSize);
		quadVertexBufferUpload->Unmap(0, nullptr);

		//copy from source buffer (ie upload heap) into final destination buffer (vertex buffer)
		commandList->CopyBufferRegion(m_QuadVertexBuffer.Get(), 0, quadVertexBufferUpload.Get(), 0, vertexBufferSize);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_QuadVertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// Initialize the vertex buffer views.
		m_QuadVertexBufferView.BufferLocation = m_QuadVertexBuffer->GetGPUVirtualAddress();
		m_QuadVertexBufferView.StrideInBytes = sizeof(QuadVertex);
		m_QuadVertexBufferView.SizeInBytes = vertexBufferSize;
	}

	// Close the resource creation command list and execute it to begin the vertex buffer copy into the default heap.
	ThrowIfFailed(commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	//flush GPU to ensure that the vertex buffer has been created
	DXGraphicsUtilities::WaitForGpu(m_device, m_commandQueue);
}

void DXTexturedQuad::CreateRootSignature()
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

	// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}


	// Create a root signature consisting of a descriptor table with a SRV and a sampler.
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		CD3DX12_ROOT_PARAMETER1 rootParameters[1];

		// We don't modify the SRV in the post-processing command list after
		// SetGraphicsRootDescriptorTable is executed on the GPU so we can use the default
		// range behavior: D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

		// Allow input layout and pixel shader access and deny uneccessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		// Create a sampler.
		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		//create root signature based on the root parameter and sampler
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_QuadRootSignature)));
		NAME_D3D12_OBJECT(m_QuadRootSignature);
	}
}

void DXTexturedQuad::CreatePipelineState(const std::wstring &vertex_shader_file, const std::wstring &pixel_shader_file, 
	const std::string & vs_entry, const std::string & ps_entry)
{
	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> quadVertexShader;
		ComPtr<ID3DBlob> quadPixelShader;
		ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		//load shader files from disk
		ThrowIfFailed(D3DCompileFromFile(vertex_shader_file.c_str(), nullptr, nullptr, vs_entry.c_str(), "vs_5_0", compileFlags, 0, &quadVertexShader, &error));
		ThrowIfFailed(D3DCompileFromFile(pixel_shader_file.c_str(), nullptr, nullptr, ps_entry.c_str(), "ps_5_0", compileFlags, 0, &quadPixelShader, &error));

		// Define the vertex input layouts.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};


		// Describe and create the graphics pipeline state objects (PSOs).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_QuadRootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(quadVertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(quadPixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		//psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_QuadPipelineState)));
		NAME_D3D12_OBJECT(m_QuadPipelineState);

	}
}

void DXTexturedQuad::CreateConstantBuffer()
{
	// Create a constant buffer to hold the transform 
	m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pConstantBuffer));

	// Keep as persistently mapped buffer, store left eye in first 256 bytes, right eye in second
	UINT8 *pBuffer;
	CD3DX12_RANGE readRange(0, 0);
	m_pConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pBuffer));
	m_pConstantBufferData = pBuffer;



	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart());
	cbvHandle.Offset(m_cbDescriptorIndex, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_pConstantBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (sizeof(XMMATRIX) + 255) & ~255; // Pad to 256 bytes
	m_device->CreateConstantBufferView(&cbvDesc, cbvHandle);

	
}

void DXTexturedQuad::RenderVerticesOnly(ComPtr<ID3D12GraphicsCommandList>& pCommandList)
{
	pCommandList->RSSetViewports(1, &m_QuadViewport);
	pCommandList->RSSetScissorRects(1, &m_QuadScissorRect);
	
	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pCommandList->IASetVertexBuffers(0, 1, &m_QuadVertexBufferView);

	pCommandList->DrawInstanced(4, 1, 0, 0);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DXTexturedQuad::GetSrvGPUDescriptorHandle(ComPtr<ID3D12Device>& device)
{
	return m_DXTexture->GetSrvGPUDescriptorHandle(m_device);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DXTexturedQuad::GetRtvDescriptorHandle(ComPtr<ID3D12Device>& device)
{
	//get RTT view for the textured quad that has a RTT (in our custom class DXTexture)
	D3D12_CPU_DESCRIPTOR_HANDLE hTexturedQuadRttInRttHeap = m_DXTexture->GetRenderTargetView();

	//convert to CD3DX12_CPU_DESCRIPTOR_HANDLE 
	CD3DX12_CPU_DESCRIPTOR_HANDLE hTexturedQuadRtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(hTexturedQuadRttInRttHeap);

	return hTexturedQuadRtv;
}

//Render into rtv passed as param
void DXTexturedQuad::RenderTexturedQuadIntoRtv(ComPtr<ID3D12GraphicsCommandList>& pCommandList,
	ComPtr<ID3D12DescriptorHeap> cbvSrvHeap,
	CD3DX12_CPU_DESCRIPTOR_HANDLE& inputTextureRtv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE& inputConstantBufferSrv,
	ComPtr<ID3D12RootSignature>& quadRootSignature,
	ComPtr<ID3D12PipelineState>& quadPipelineState)
{
	//set pipeline state object
	pCommandList->SetPipelineState(quadPipelineState.Get());

	pCommandList->OMSetRenderTargets(1, &inputTextureRtv, FALSE, nullptr);//set RTT

	// Set necessary state.
	pCommandList->SetGraphicsRootSignature(quadRootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { cbvSrvHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//bind the texture view to the root parameter
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle = m_DXTexture->GetSrvGPUDescriptorHandle(m_device);
	int texture_root_param = 0;
	pCommandList->SetGraphicsRootDescriptorTable(texture_root_param, srvHandle);

	int constant_buffer_root_param = 1;
	pCommandList->SetGraphicsRootDescriptorTable(constant_buffer_root_param, inputConstantBufferSrv);

	int width = 0, height = 0;
	m_DXTexture->GetWidthAndHeight(width, height);

	CD3DX12_VIEWPORT quadViewport = m_QuadViewport;
	CD3DX12_RECT quadcissorRect = m_QuadScissorRect;

	m_QuadViewport.Width = static_cast<float>(width);
	m_QuadViewport.Height = static_cast<float>(height);

	m_QuadScissorRect.right = static_cast<LONG>(width);
	m_QuadScissorRect.bottom = static_cast<LONG>(height);

	RenderVerticesOnly(pCommandList);

	m_QuadViewport = quadViewport;
	m_QuadScissorRect = quadcissorRect;
}

void DXTexturedQuad::RenderTexturedQuadIntoRtv(ComPtr<ID3D12GraphicsCommandList>& pCommandList,
	ComPtr<ID3D12DescriptorHeap> cbvSrvHeap,
	CD3DX12_GPU_DESCRIPTOR_HANDLE& inputTextureSrv_1,
	CD3DX12_GPU_DESCRIPTOR_HANDLE& inputTextureSrv_2,
	CD3DX12_CPU_DESCRIPTOR_HANDLE& inputTextureRtv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE& inputConstantBufferSrv,
	ComPtr<ID3D12RootSignature>& quadRootSignature,
	ComPtr<ID3D12PipelineState>& quadPipelineState)
{
	pCommandList->SetPipelineState(quadPipelineState.Get());	//set pipeline state object

	pCommandList->OMSetRenderTargets(1, &inputTextureRtv, FALSE, nullptr);//set RTT

	pCommandList->SetGraphicsRootSignature(quadRootSignature.Get());	// Set necessary state.

	ID3D12DescriptorHeap* ppHeaps[] = { cbvSrvHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	int texture_root_param = 0; //t0 in root param 0
	pCommandList->SetGraphicsRootDescriptorTable(texture_root_param, inputTextureSrv_1);

	int constant_buffer_root_param = 1; //b0 in root param 1
	pCommandList->SetGraphicsRootDescriptorTable(constant_buffer_root_param, inputConstantBufferSrv);

	texture_root_param = 2; //t1 in root param 2
	pCommandList->SetGraphicsRootDescriptorTable(texture_root_param, inputTextureSrv_2);

	int width = 0, height = 0;
	m_DXTexture->GetWidthAndHeight(width, height);

	CD3DX12_VIEWPORT quadViewport = m_QuadViewport;
	CD3DX12_RECT quadcissorRect = m_QuadScissorRect;

	m_QuadViewport.Width = static_cast<float>(width);
	m_QuadViewport.Height = static_cast<float>(height);

	m_QuadScissorRect.right = static_cast<LONG>(width);
	m_QuadScissorRect.bottom = static_cast<LONG>(height);

	RenderVerticesOnly(pCommandList);

	m_QuadViewport = quadViewport;
	m_QuadScissorRect = quadcissorRect;
}

//Both pixel render target texture and input texture specified in parameters
void DXTexturedQuad::RenderTexturedQuadIntoRtv(ComPtr<ID3D12GraphicsCommandList>& pCommandList,
	ComPtr<ID3D12DescriptorHeap> cbvSrvHeap,
	CD3DX12_GPU_DESCRIPTOR_HANDLE& inputTextureSrv,
	CD3DX12_CPU_DESCRIPTOR_HANDLE& inputTextureRtv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE& inputConstantBufferSrv,
	ComPtr<ID3D12RootSignature>& quadRootSignature,
	ComPtr<ID3D12PipelineState>& quadPipelineState)
{
	//set pipeline state object
	pCommandList->SetPipelineState(quadPipelineState.Get());

	pCommandList->OMSetRenderTargets(1, &inputTextureRtv, FALSE, nullptr);//set RTT

	// Set necessary state.
	pCommandList->SetGraphicsRootSignature(quadRootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { cbvSrvHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//bind the texture view to the root parameter
	int texture_root_param = 0;
	pCommandList->SetGraphicsRootDescriptorTable(texture_root_param, inputTextureSrv);

	int constant_buffer_root_param = 1;
	pCommandList->SetGraphicsRootDescriptorTable(constant_buffer_root_param, inputConstantBufferSrv);

	int width = 0, height = 0;
	m_DXTexture->GetWidthAndHeight(width, height);

	CD3DX12_VIEWPORT quadViewport = m_QuadViewport;
	CD3DX12_RECT quadcissorRect = m_QuadScissorRect;

	m_QuadViewport.Width = static_cast<float>(width);
	m_QuadViewport.Height = static_cast<float>(height);

	m_QuadScissorRect.right = static_cast<LONG>(width);
	m_QuadScissorRect.bottom = static_cast<LONG>(height);

	RenderVerticesOnly(pCommandList);

	m_QuadViewport = quadViewport;
	m_QuadScissorRect = quadcissorRect;
}


void DXTexturedQuad::RenderTexturedQuad(ComPtr<ID3D12GraphicsCommandList>& pCommandList,
	ComPtr<ID3D12DescriptorHeap> cbvSrvHeap,
	CD3DX12_GPU_DESCRIPTOR_HANDLE& inputTextureSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE& inputConstantBufferSrv,
	ComPtr<ID3D12RootSignature>& quadRootSignature,
	ComPtr<ID3D12PipelineState>& quadPipelineState)
{

	//set pipeline state object
	pCommandList->SetPipelineState(quadPipelineState.Get());

	//get RTT view for the textured quad that has a RTT (in our custom class DXTexture)
	D3D12_CPU_DESCRIPTOR_HANDLE hTexturedQuadRttInRttHeap = m_DXTexture->GetRenderTargetView();

	//get RTT view for the textured  that has a RTT (in our custom class DXTexture)
	CD3DX12_CPU_DESCRIPTOR_HANDLE hTexturedQuadRtv = GetRtvDescriptorHandle(m_device);
	
	pCommandList->OMSetRenderTargets(1, &hTexturedQuadRtv, FALSE, nullptr);//set RTT

	// Set necessary state.
	pCommandList->SetGraphicsRootSignature(quadRootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { cbvSrvHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//bind the texture view to the root parameter
	int texture_root_param = 0;
	pCommandList->SetGraphicsRootDescriptorTable(texture_root_param, inputTextureSrv);

	int constant_buffer_root_param = 1;
	pCommandList->SetGraphicsRootDescriptorTable(constant_buffer_root_param, inputConstantBufferSrv);

	RenderVerticesOnly(pCommandList);
}

void DXTexturedQuad::RenderQuad(ComPtr<ID3D12GraphicsCommandList> & pCommandList, CD3DX12_CPU_DESCRIPTOR_HANDLE &rtvHandle)
{
	//set pipeline state object
	pCommandList->SetPipelineState(m_QuadPipelineState.Get());

	//set RTT
	pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
	
	// Set necessary state.
	pCommandList->SetGraphicsRootSignature(m_QuadRootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//bind the texture view to the root parameter
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle=m_DXTexture->GetSrvGPUDescriptorHandle(m_device);
	pCommandList->SetGraphicsRootDescriptorTable(0, srvHandle);
	
	pCommandList->RSSetViewports(1, &m_QuadViewport);
	pCommandList->RSSetScissorRects(1, &m_QuadScissorRect);
	
	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pCommandList->IASetVertexBuffers(0, 1, &m_QuadVertexBufferView);

	pCommandList->DrawInstanced(4, 1, 0, 0);


}

void DXTexturedQuad::RenderQuad(ComPtr<ID3D12GraphicsCommandList> & pCommandList, CD3DX12_CPU_DESCRIPTOR_HANDLE &rtvHandle,
	const CD3DX12_VIEWPORT & viewport, const CD3DX12_RECT & scissor_rect)
{
	pCommandList->SetPipelineState(m_QuadPipelineState.Get());

	//float clear_color[4] = { 1.0f, 0.2f, 0.4f, 1.0f };
	//pCommandList->ClearRenderTargetView(rtvHandle, clear_color, 0, nullptr);

	pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Set necessary state.
	pCommandList->SetGraphicsRootSignature(m_QuadRootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//bind the texture view to the root parameter
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle = m_DXTexture->GetSrvGPUDescriptorHandle(m_device);
	pCommandList->SetGraphicsRootDescriptorTable(0, srvHandle);

	pCommandList->RSSetViewports(1, &viewport);
	pCommandList->RSSetScissorRects(1, &scissor_rect);

	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pCommandList->IASetVertexBuffers(0, 1, &m_QuadVertexBufferView);

	pCommandList->DrawInstanced(4, 1, 0, 0);

}

void DXTexturedQuad::RenderWorldQuad(ComPtr<ID3D12GraphicsCommandList> & pCommandList, CD3DX12_CPU_DESCRIPTOR_HANDLE &rtvHandle,
	const CD3DX12_VIEWPORT & viewport, const CD3DX12_RECT & scissor_rect, DXCamera *dx_camera)
{
	pCommandList->SetPipelineState(m_QuadPipelineState.Get());

	//float clear_color[4] = { 1.0f, 0.2f, 0.4f, 1.0f };
	//pCommandList->ClearRenderTargetView(rtvHandle, clear_color, 0, nullptr);

	pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Set necessary state.
	pCommandList->SetGraphicsRootSignature(m_QuadRootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//bind the texture view to the root parameter
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle = m_DXTexture->GetSrvGPUDescriptorHandle(m_device);
	pCommandList->SetGraphicsRootDescriptorTable(0, srvHandle);

	pCommandList->RSSetViewports(1, &viewport);
	pCommandList->RSSetScissorRects(1, &scissor_rect);

	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pCommandList->IASetVertexBuffers(0, 1, &m_QuadVertexBufferView);

	XMMATRIX view = dx_camera->GetViewMatrix();
	XMMATRIX proj = dx_camera->GetViewMatrix();

	XMMATRIX wv = XMMatrixMultiply(m_WorldMatrix, view);
	XMMATRIX mvp = XMMatrixMultiply(wv, proj);

	//copy mvp matrix data into the constant buffer
	DirectX::XMFLOAT4X4 mvp4x4;
	XMStoreFloat4x4(&mvp4x4, XMMatrixTranspose(mvp));
	memcpy(m_pConstantBufferData, &mvp4x4, sizeof(mvp4x4));

	pCommandList->DrawInstanced(4, 1, 0, 0);


}

void DXTexturedQuad::CreateTextureFromFile(const std::wstring& strFullPath,  int descriptorIndex)
{
	bool bLoaded=m_DXTexture->CreateTextureFromFile(m_device, m_commandQueue, m_cbvSrvHeap, strFullPath, descriptorIndex);
	assert(bLoaded);
}

void DXTexturedQuad::CreateRenderTargetTexture(ComPtr<ID3D12DescriptorHeap> &srvDescriptorHeap, ComPtr<ID3D12DescriptorHeap> &rttDescriptorHeap, int nImageWidth, int nImageHeight, int srvDescriptorIndex, int rttDescriptorIndex)
{
	m_DXTexture->CreateRenderTargetTexture(m_device, srvDescriptorHeap, rttDescriptorHeap, nImageWidth, nImageHeight, srvDescriptorIndex,  rttDescriptorIndex);

}
