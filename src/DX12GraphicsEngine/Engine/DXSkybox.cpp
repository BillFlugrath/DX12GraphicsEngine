#include "stdafx.h"
#include "DXSkybox.h"
#include "DXPointCloud.h"
#include "DXMesh.h"
#include "DXTexture.h"
#include "DXDescriptorHeap.h"

#include "./DXR/DXShaderUtilities.h"
#include "./DXR/DXD3DUtilities.h"
#include "./DXR/DXRManager.h"


DXSkyBox::DXSkyBox() :
	m_CubeTextureDescriptorIndex(0)
	, m_pSkyboxPipelineState(nullptr)
	, m_SkyboxSizeScale(1000.0f)
{
	m_WorldMatrix = XMMatrixIdentity();
}

DXSkyBox::~DXSkyBox()
{
	
}

void DXSkyBox::CreateD3DResources(ComPtr<ID3D12CommandQueue>& commandQueue)
{
	CreateRootSignature();
	
	CreatePipelineState(); //sky box shader
	
	m_cbvSrvDescriptorSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

}

void DXSkyBox::LoadModelAndTexture(const std::string& modelFileName, const std::wstring& strTextureFullPath,
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

	CreateSkyboxSRV(descriptor_heap_srv);

	m_WorldMatrix = XMMatrixScaling(m_SkyboxSizeScale, m_SkyboxSizeScale, m_SkyboxSizeScale);
}

void DXSkyBox::CreatePipelineState()
{
	if (m_pSkyboxPipelineState)
		return;

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> objModelVertexShader;
		ComPtr<ID3DBlob> objModelPixelShader;
		ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		IDxcBlob* vsBlob = nullptr;
		IDxcBlob* psBlob = nullptr;
		D3D12ShaderCompilerInfo shaderCompiler;
		DXShaderUtilities shaderUtils;

		//load shader files from disk
		
		shaderUtils.Init_Shader_Compiler(shaderCompiler);

		D3D12ShaderInfo infoVS(L"assets/shaders/objModelSkyboxShaders.hlsl", L"VSMain", L"vs_6_6");
		shaderUtils.Compile_Shader(shaderCompiler, infoVS, &vsBlob);

		D3D12ShaderInfo infoPS(L"assets/shaders/objModelSkyboxShaders.hlsl", L"PSMain", L"ps_6_6");
		shaderUtils.Compile_Shader(shaderCompiler, infoPS, &psBlob);

		//shaderUtils.Destroy(shaderCompiler);
		
		
		// Define the vertex input layouts.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};


		// Describe and create the graphics pipeline state objects (PSOs).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_pRootSignature.Get();

		if (msbUseInlineRayTracing)
		{
			psoDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
			psoDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();

			psoDesc.PS.BytecodeLength = psBlob->GetBufferSize();
			psoDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
		}
		else
		{
			psoDesc.VS = CD3DX12_SHADER_BYTECODE(objModelVertexShader.Get());
			psoDesc.PS = CD3DX12_SHADER_BYTECODE(objModelPixelShader.Get());
		}


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

		ThrowIfFailed(m_pd3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pSkyboxPipelineState)));
		NAME_D3D12_OBJECT(m_pSkyboxPipelineState);

	}
}

void DXSkyBox::CreateRootSignature()
{
	//if (m_pRootSignature)
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
		ThrowIfFailed(m_pd3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature)));

	}
}

void DXSkyBox::Render(ComPtr<ID3D12GraphicsCommandList>& pCommandList, const DirectX::XMMATRIX& view,
	const DirectX::XMMATRIX& proj, std::vector<DXGraphicsUtilities::SrvParameter>& rootSrvParams)
{
	pCommandList->SetPipelineState(m_pSkyboxPipelineState.Get());
	pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	int cubemap_root_parameter = 3;
	pCommandList->SetGraphicsRootDescriptorTable(cubemap_root_parameter, m_CubemapGPUHandle);

	for (auto param : rootSrvParams)
	{
		pCommandList->SetGraphicsRootDescriptorTable(param.mRootParamIndex, param.mHandle);
	}


	XMMATRIX wv = XMMatrixMultiply(m_WorldMatrix, view);
	XMMATRIX wvp = XMMatrixMultiply(wv, proj);
	XMMATRIX vp = XMMatrixMultiply(view, proj);

	m_pDXMesh->Render(pCommandList, m_WorldMatrix, wvp);
}

void DXSkyBox::CreateSkyboxSRV(std::shared_ptr<DXDescriptorHeap>& descriptor_heap_srv)
{
	// Create the cube map 0 texture SRV : register(t0);
	D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc3 = {};
	textureSRVDesc3.Format = m_DXTexture->GetDX12Resource()->GetDesc().Format;
	textureSRVDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	textureSRVDesc3.Texture2D.MipLevels = m_DXTexture->GetDX12Resource()->GetDesc().MipLevels;
	textureSRVDesc3.Texture2D.MostDetailedMip = 0;
	textureSRVDesc3.Texture2D.ResourceMinLODClamp = 0.0f;
	textureSRVDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	m_CubeTextureDescriptorIndex = descriptor_heap_srv->GetNewDescriptorIndex();
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle = descriptor_heap_srv->GetCD3XD12CPUDescriptorHandle(m_CubeTextureDescriptorIndex);
	m_CubemapGPUHandle = descriptor_heap_srv->GetCD3DX12GPUDescriptorHandle(m_CubeTextureDescriptorIndex);

	m_pd3dDevice->CreateShaderResourceView(m_DXTexture->GetDX12Resource().Get(), &textureSRVDesc3, handle);

	/*m_CubeTextureDescriptorIndex = descriptor_heap_srv->GetNewDescriptorIndex();

	m_DXTexture = std::make_shared<DXTexture>();

	m_DXTexture->CreateDDSTextureFromFile12(pd3dDevice.Get(), pCommandQueue, strTextureFullPath.c_str());

	m_DXTexture->SetSRVDescriptorIndex(m_CubeTextureDescriptorIndex);
	m_DXTexture->SetSRVDescriptorHeap(descriptor_heap_srv->GetDescriptorHeap());*/
}