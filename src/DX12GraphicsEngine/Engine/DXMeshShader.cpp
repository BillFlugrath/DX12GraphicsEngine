#include "stdafx.h"
#include "DXMeshShader.h"
#include "DXPointCloud.h"
#include "DXMesh.h"
#include "DXTexture.h"
#include "DXDescriptorHeap.h"
#include "DXCamera.h"

#include "./DXR/DXShaderUtilities.h"
#include "./DXR/DXD3DUtilities.h"
#include "./DXR/DXRManager.h"



DXMeshShader::DXMeshShader() :
	m_pMeshShaderPipelineState(nullptr)
{
	m_WorldMatrix = XMMatrixIdentity();
}

DXMeshShader::~DXMeshShader()
{
	
}

void DXMeshShader::Init(ComPtr<ID3D12Device>& pd3dDevice, ComPtr<ID3D12CommandQueue>& commandQueue, ComPtr<ID3D12DescriptorHeap>& pCBVSRVHeap,
	CD3DX12_VIEWPORT Viewport,CD3DX12_RECT ScissorRect, const std::wstring& shaderFileName, 
	const std::wstring& meshletFileName, bool bUseEmbeddedRootSig)
{

	m_pd3dDevice = pd3dDevice;
	m_bUseShaderEmbeddedRootSig = bUseEmbeddedRootSig;

	ThrowIfFailed(m_pd3dDevice->QueryInterface(IID_PPV_ARGS(&m_pd3dDevice2)), L"Couldn't get interface for the device.\n");

	m_cbvSrvHeap = pCBVSRVHeap;

	m_Viewport = Viewport;
	m_ScissorRect = ScissorRect;
	m_ShaderFilename = shaderFileName;
	m_MeshletFilename = meshletFileName;

	CreateD3DResources(commandQueue);
	//CreateGlobalRootSignature();

	CreateMeshlets(commandQueue);

}

void DXMeshShader::ShaderEntryPoint(const std::wstring& meshShaderEntryPoint, const std::wstring& pixelShaderEntryPoint)
{
	m_MeshShaderEntryPoint = meshShaderEntryPoint;
	m_PixelShaderEntryPoint = pixelShaderEntryPoint;
}

void DXMeshShader::CreateGlobalRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};

	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
	ThrowIfFailed(m_pd3dDevice->CreateRootSignature(
		0,                         // nodeMask
		blob->GetBufferPointer(),  // pBloblWithRootSignature
		blob->GetBufferSize(),     // blobLengthInBytes
		IID_PPV_ARGS(&m_TempRootSig))); // riid, ppvRootSignature
}

void DXMeshShader::CreateD3DResources(ComPtr<ID3D12CommandQueue>& commandQueue)
{

	if (!m_bUseShaderEmbeddedRootSig)
		CreateRootSignature();
	
	CreatePipelineState(); 
	
	m_cbvSrvDescriptorSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	CreateConstantBuffer();
}

void DXMeshShader::CreateConstantBuffer()
{
	// Create the constant buffer.
	{
		const UINT64 constantBufferSize = sizeof(SceneConstantBuffer);

		const CD3DX12_HEAP_PROPERTIES constantBufferHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);

		ThrowIfFailed(m_pd3dDevice->CreateCommittedResource(
			&constantBufferHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&constantBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_constantBuffer)));

		// Describe and create a constant buffer view.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = constantBufferSize;

		// Map and initialize the constant buffer. We don't unmap this until the
		// app closes. Keeping things mapped for the lifetime of the resource is okay.
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvDataBegin)));
	}
}

//TODO this loads an obj file, not meshlet.  Either change or remove function.
void DXMeshShader::LoadModelAndTexture(const std::string& modelFileName, const std::wstring& strTextureFullPath,
	ComPtr<ID3D12Device>& pd3dDevice, ComPtr<ID3D12CommandQueue>& pCommandQueue,
	std::shared_ptr<DXDescriptorHeap>& descriptor_heap_srv, const CD3DX12_VIEWPORT& Viewport,
	const CD3DX12_RECT& ScissorRect)
{

	m_pd3dDevice = pd3dDevice;

	m_cbvSrvHeap = descriptor_heap_srv->GetDescriptorHeap();
	m_cbDescriptorIndex = descriptor_heap_srv->GetNewDescriptorIndex();

	m_Viewport = Viewport;
	m_ScissorRect = ScissorRect;

	CreateD3DResources(pCommandQueue);

	LoadModel(modelFileName);

	m_DXTexture = std::make_shared<DXTexture>();

	m_DXTexture->CreateDDSTextureFromFile12(pd3dDevice.Get(), pCommandQueue, strTextureFullPath.c_str());

	CreateSRVs(descriptor_heap_srv);

}

void  DXMeshShader::AddTexture(const std::wstring& strTextureFullPath, ComPtr<ID3D12Device>& pd3dDevice, ComPtr<ID3D12CommandQueue>& pCommandQueue, std::shared_ptr<DXDescriptorHeap>& descriptor_heap_srv)
{
	m_DXTexture = std::make_shared<DXTexture>();

	int descriptor_index = descriptor_heap_srv->GetNewDescriptorIndex(); //index into descriptor heap cb_descriptor_index 
	bool bLoaded = m_DXTexture->CreateTextureFromFile(pd3dDevice, pCommandQueue, descriptor_heap_srv->GetDescriptorHeap(), strTextureFullPath, descriptor_index);

	assert(bLoaded);
}

void DXMeshShader::CreatePipelineState()
{
	//if (m_pMeshShaderPipelineState)
		//return;

	// Create the pipeline state, which includes compiling and loading shaders.
	{
#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		IDxcBlob* msBlob = nullptr;
		IDxcBlob* psBlob = nullptr;
		D3D12ShaderCompilerInfo shaderCompiler;
		DXShaderUtilities shaderUtils;

		//load and compile shader files from disk
		
		shaderUtils.Init_Shader_Compiler(shaderCompiler);

		D3D12ShaderInfo infoVS(m_ShaderFilename.c_str(), m_MeshShaderEntryPoint.c_str(), L"ms_6_6");
		shaderUtils.Compile_Shader(shaderCompiler, infoVS, &msBlob);

		D3D12ShaderInfo infoPS(m_ShaderFilename.c_str(), m_PixelShaderEntryPoint.c_str(), L"ps_6_6");
		shaderUtils.Compile_Shader(shaderCompiler, infoPS, &psBlob);

		// Pull root signature from the precompiled mesh shader.
		if (m_bUseShaderEmbeddedRootSig)
		{
			ThrowIfFailed(m_pd3dDevice->CreateRootSignature(0, msBlob->GetBufferPointer(), msBlob->GetBufferSize(),
				IID_PPV_ARGS(&m_pMeshShaderRootSignature)));
		}
		
		//shaderUtils.Destroy(shaderCompiler);
		
		// Describe and create the graphics pipeline state objects (PSOs).
		D3DX12_MESH_SHADER_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_pMeshShaderRootSignature.Get();
		
		psoDesc.MS.BytecodeLength = msBlob->GetBufferSize();
		psoDesc.MS.pShaderBytecode = msBlob->GetBufferPointer();

		psoDesc.PS.BytecodeLength = psBlob->GetBufferSize();
		psoDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
		
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		// The camera is inside the sky sphere, so just turn off culling.
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		//psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		//	psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;  //DXGI_FORMAT_R24G8_TYPELESS
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		CD3DX12_PIPELINE_MESH_STATE_STREAM psoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(psoDesc);

		D3D12_PIPELINE_STATE_STREAM_DESC steamDesc = {};
		steamDesc.SizeInBytes = sizeof(psoStream);
		steamDesc.pPipelineStateSubobjectStream = &psoStream;

		ThrowIfFailed(m_pd3dDevice2->CreatePipelineState(&steamDesc, IID_PPV_ARGS(&m_pMeshShaderPipelineState)));
		NAME_D3D12_OBJECT(m_pMeshShaderPipelineState);

	}
}

void DXMeshShader::CreateRootSignature()
{
	//if (m_pMeshShaderRootSignature)
	//	return;

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

	// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_pd3dDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}


	// Create a root signature consisting of  descriptor tables
	{
		////////
		//CD3DX12_DESCRIPTOR_RANGE specifies the elements of the descriptor table.  for example CD3DX12_DESCRIPTOR_RANGE table[2] would
		//have two elements.  in  example below, there is only 1 element in the table
		CD3DX12_DESCRIPTOR_RANGE texTable0, texTable1, texTable2;
		texTable0.Init(
			D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			1,  // number of texture descriptors
			0); // register t0


		//element for descriptor table
		CD3DX12_DESCRIPTOR_RANGE cbvTable;
		cbvTable.Init(
			D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
			1,  // number of cb descriptors
			0 // register b0
		);

		texTable1.Init(
			D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			1,  // number of texture descriptors
			1); // register t1  RaytracingAccelerationStructure

		texTable2.Init(
			D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			1,  // number of texture descriptors
			2); // t2 cubemap

		//root signature parameters (consists of two tables).  later, before rendering, we need to call
		//pCommandList->SetGraphicsRootDescriptorTable(0, texHandle);
		//pCommandList->SetGraphicsRootDescriptorTable(1, cbvHandle);
		//pCommandList->SetGraphicsRootDescriptorTable(2, scene BVH);
		//pCommandList->SetGraphicsRootDescriptorTable(3, cubeTexHandle);
		//This points the table directly to the heap memory where the descriptor handles reside

		const uint32_t numParameters = 4;
		CD3DX12_ROOT_PARAMETER rootParameters[numParameters];
		rootParameters[0].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[1].InitAsDescriptorTable(1, &cbvTable, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[2].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[3].InitAsDescriptorTable(1, &texTable2, D3D12_SHADER_VISIBILITY_ALL);

		// Allow input layout and pixel shader access and deny uneccessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		// Create a sampler.
		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;


		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(numParameters, rootParameters,
			1, &sampler,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


		// create a root signature with  slot which points to a descriptor range consisting of a table of textures
		// and a slot for a constant buffer 
		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));

		// ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(m_pd3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_pMeshShaderRootSignature)));

	}
}

void DXMeshShader::Render(ComPtr<ID3D12GraphicsCommandList>& pGraphicsCommandList, const DirectX::XMMATRIX& view,
	const DirectX::XMMATRIX& proj, std::vector<DXGraphicsUtilities::SrvParameter>& rootSrvParams)
{
	static bool bUseNewFunction = true;
	if (bUseNewFunction)
	{
		RenderMeshlets(pGraphicsCommandList, view,proj,rootSrvParams);
		return;
	}

	ComPtr<ID3D12GraphicsCommandList6> pCommandList;
	ThrowIfFailed(pGraphicsCommandList->QueryInterface(IID_PPV_ARGS(&pCommandList)), L"Couldn't get interface for the command list.\n");

	//set pipeline and signature
	pCommandList->SetPipelineState(m_pMeshShaderPipelineState.Get());
	pCommandList->SetGraphicsRootSignature(m_pMeshShaderRootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//for (auto param : rootSrvParams)
	//{
		//pCommandList->SetGraphicsRootDescriptorTable(param.mRootParamIndex, param.mHandle);
	//}

	pCommandList->DispatchMesh(1, 1, 1);

//	XMMATRIX wv = XMMatrixMultiply(m_WorldMatrix, view);
	//XMMATRIX wvp = XMMatrixMultiply(wv, proj);
	//XMMATRIX vp = XMMatrixMultiply(view, proj);

	//m_pDXMesh->Render(pCommandList, m_WorldMatrix, wvp);
}

void DXMeshShader::RenderMeshlets(ComPtr<ID3D12GraphicsCommandList>& pGraphicsCommandList, const DirectX::XMMATRIX& view,
	const DirectX::XMMATRIX& proj, std::vector<DXGraphicsUtilities::SrvParameter>& rootSrvParams)
{
	ComPtr<ID3D12GraphicsCommandList6> pCommandList;
	ThrowIfFailed(pGraphicsCommandList->QueryInterface(IID_PPV_ARGS(&pCommandList)), L"Couldn't get interface for the command list.\n");

	//set pipeline and signature
	pCommandList->SetPipelineState(m_pMeshShaderPipelineState.Get());
	pCommandList->SetGraphicsRootSignature(m_pMeshShaderRootSignature.Get());

	//this descriptor heap is not required to draw the original mesh since all of the shader views are set directly
	//with the GPU virtual address since they are buffers not textures.  We do need the descriptor heap for actual texture
	//srvs however.
	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//for (auto param : rootSrvParams)
	//{
		//pCommandList->SetGraphicsRootDescriptorTable(param.mRootParamIndex, param.mHandle);
	//}

	if (m_DXTexture != nullptr)
	{
		CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle = m_DXTexture->GetSrvGPUDescriptorHandle(m_pd3dDevice);
		int root_parameter = 6;
		pCommandList->SetGraphicsRootDescriptorTable(root_parameter, texHandle);
	}
	

	//set the constant buffer address directly instead of using a table (ie instead of a GPU handle).  sets b0
	pCommandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress() );

	XMMATRIX world = XMMATRIX(g_XMIdentityR0, g_XMIdentityR1, g_XMIdentityR2, g_XMIdentityR3);
	
	//copy the data into the constant buffer
	XMStoreFloat4x4(&m_constantBufferData.World, XMMatrixTranspose(world));
	XMStoreFloat4x4(&m_constantBufferData.WorldView, XMMatrixTranspose(world * view));
	XMStoreFloat4x4(&m_constantBufferData.WorldViewProj, XMMatrixTranspose(world * view * proj));
	m_constantBufferData.DrawMeshlets = true;

	memcpy(m_cbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));

	for (auto& mesh : m_Model)
	{
		//Note: register b1 is set with two seperate 32 bit constants via SetGraphicsRoot32BitConstant(0, data, offset)
		pCommandList->SetGraphicsRoot32BitConstant(1, mesh.IndexSize, 0); //b1 at offset 0

		pCommandList->SetGraphicsRootShaderResourceView(2, mesh.VertexResources[0]->GetGPUVirtualAddress()); //t0
		pCommandList->SetGraphicsRootShaderResourceView(3, mesh.MeshletResource->GetGPUVirtualAddress()); //t1
		pCommandList->SetGraphicsRootShaderResourceView(4, mesh.UniqueVertexIndexResource->GetGPUVirtualAddress()); //t2
		pCommandList->SetGraphicsRootShaderResourceView(5, mesh.PrimitiveIndexResource->GetGPUVirtualAddress()); //t3

		for (auto& subset : mesh.MeshletSubsets)
		{
			pCommandList->SetGraphicsRoot32BitConstant(1, subset.Offset, 1);//b1 at offset 1 ie last param=1
			pCommandList->DispatchMesh(subset.Count, 1, 1);
		}
	}

}

void DXMeshShader::CreateSRVs(std::shared_ptr<DXDescriptorHeap>& descriptor_heap_srv)
{
	
}

void  DXMeshShader::CreateMeshlets(ComPtr<ID3D12CommandQueue>& commandQueue)
{
	ID3D12GraphicsCommandList4* cmdList;
	ID3D12CommandAllocator* cmdAlloc;

	ThrowIfFailed(m_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc)));
	ThrowIfFailed(m_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, IID_PPV_ARGS(&cmdList)));


	m_Model.LoadFromFile(m_MeshletFilename.c_str());
	m_Model.UploadGpuResources(m_pd3dDevice.Get(), commandQueue.Get(), cmdAlloc, cmdList);

#ifdef _DEBUG
	// Mesh shader file expects a certain vertex layout; assert our mesh conforms to that layout.
	const D3D12_INPUT_ELEMENT_DESC c_elementDescs[3] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 1 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 1 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 1 },
	};

	for (auto& mesh : m_Model)
	{
		UINT numVertexAttributes = 3; //BillF added "3" instead of "2" to support texcoord after normals
		assert(mesh.LayoutDesc.NumElements == numVertexAttributes);

		for (uint32_t i = 0; i < _countof(c_elementDescs); ++i)
			assert(std::memcmp(&mesh.LayoutElems[i], &c_elementDescs[i], sizeof(D3D12_INPUT_ELEMENT_DESC)) == 0);
	}
#endif

	//The UploadGpuResources closes the command list and submits the command queue to the gpu, thus we don't need to
	//do it again.
	
	/*
	ThrowIfFailed(cmdList->Close());
	ID3D12CommandList* ppCommandLists[] = { cmdList };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		*/


	//ComPtr<ID3D12Device> pDevice(m_pd3dDevice);
	//DXGraphicsUtilities::WaitForGpu(pDevice, commandQueue);


	if (cmdList)
		cmdList->Release();

	if (cmdAlloc)
		cmdAlloc->Release();
}