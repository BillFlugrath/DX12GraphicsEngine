#include "stdafx.h"
#include "DXModel.h"
#include "DXPointCloud.h"
#include "DXMesh.h"
#include "DXTexture.h"


#include "./DXR/DXShaderUtilities.h"
#include "./DXR/DXD3DUtilities.h"
#include "./DXR/DXRManager.h"

bool DXModel::msbDebugUseSpritePointCloud = true;
bool DXModel::msbDebugUseDiskSprites = false;
bool DXModel::msbUseInlineRayTracing = false;

ComPtr<ID3D12PipelineState> DXModel::m_pPipelineState = nullptr;
ComPtr<ID3D12PipelineState> DXModel::m_pPointCloudPipelineState = nullptr;
ComPtr<ID3D12PipelineState> DXModel::m_pPointCloudSpritePipelineState = nullptr;

ComPtr<ID3D12RootSignature> DXModel::m_pRootSignature = nullptr;
ComPtr<ID3D12RootSignature> DXModel::m_pPointCloudSpriteRootSignature = nullptr;

DXModel::DXModel() :
	 m_cbvSrvHeap(nullptr)
	, m_cbDescriptorIndex(0)
	, m_Viewport(0.0f, 0.0f, 0.0f, 0.0f)
	, m_ScissorRect(0, 0, 0, 0)
	, m_DXTexture(nullptr)
{
	m_WorldMatrix = XMMatrixIdentity();
}

DXModel::~DXModel()
{
	
}

void  DXModel::Init(ComPtr<ID3D12Device>& pd3dDevice, ComPtr<ID3D12CommandQueue> &commandQueue, 
	ComPtr<ID3D12DescriptorHeap> &pCBVSRVHeap, int cbDescriptorIndex,
	CD3DX12_VIEWPORT Viewport,
	CD3DX12_RECT ScissorRect)
{
	m_pd3dDevice = pd3dDevice;
	
	m_cbvSrvHeap = pCBVSRVHeap; //we are overriding this by creating our own.  ???
	m_cbDescriptorIndex = cbDescriptorIndex;

	m_Viewport = Viewport;
	m_ScissorRect = ScissorRect;

	CreateD3DResources(commandQueue);
}

void DXModel::CreatePipelineState()
{
	if (m_pPipelineState)
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
		IDxcBlob* vsBlob=nullptr;
		IDxcBlob* psBlob = nullptr;
		D3D12ShaderCompilerInfo shaderCompiler;
		DXShaderUtilities shaderUtils;

		//load shader files from disk
		if (msbUseInlineRayTracing)
		{
			shaderUtils.Init_Shader_Compiler(shaderCompiler);

			D3D12ShaderInfo infoVS(L"assets/shaders/objModelShadersRayTrace.hlsl", L"VSMain", L"vs_6_6");
			shaderUtils.Compile_Shader(shaderCompiler, infoVS, &vsBlob);

			D3D12ShaderInfo infoPS(L"assets/shaders/objModelShadersRayTrace.hlsl", L"PSMain", L"ps_6_6");
			shaderUtils.Compile_Shader(shaderCompiler, infoPS, &psBlob);

			//shaderUtils.Destroy(shaderCompiler);
		}
		else
		{
			ThrowIfFailed(D3DCompileFromFile(L"assets/shaders/objModelShaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &objModelVertexShader, &error));
			ThrowIfFailed(D3DCompileFromFile(L"assets/shaders/objModelShaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &objModelPixelShader, &error));
		}
		
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
		//psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		//	psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;  //DXGI_FORMAT_R24G8_TYPELESS
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		ThrowIfFailed(m_pd3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState)));
		NAME_D3D12_OBJECT(m_pPipelineState);

	}

}

void DXModel::CreatePointCloudPipelineState()
{
	if (m_pPointCloudPipelineState)
		return;

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> pointCloudVertexShader;
		ComPtr<ID3DBlob> pointCloudPixelShader;
		ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		//load shader files from disk
		ThrowIfFailed(D3DCompileFromFile(L"assets/shaders/pointCloudShaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &pointCloudVertexShader, &error));
		ThrowIfFailed(D3DCompileFromFile(L"assets/shaders/pointCloudShaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pointCloudPixelShader, &error));


		// Define the vertex input layouts.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			//{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};


		// Describe and create the graphics pipeline state objects (PSOs).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_pRootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(pointCloudVertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pointCloudPixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		//psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		//	psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;  //DXGI_FORMAT_R24G8_TYPELESS
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		ThrowIfFailed(m_pd3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPointCloudPipelineState)));
		NAME_D3D12_OBJECT(m_pPointCloudPipelineState);

	}

}

void DXModel::CreateD3DResources(ComPtr<ID3D12CommandQueue> & commandQueue)
{
	CreateRootSignature();
	CreatePointCloudSpriteRootSignature();

	CreatePipelineState(); //obj models
	CreatePointCloudPipelineState(); //points
	CreatePointCloudSpritePipelineState(); //points with geometry shader

	
	m_cbvSrvDescriptorSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

}

void DXModel::CreatePointCloudSpriteRootSignature()
{
	if (m_pPointCloudSpriteRootSignature)
		return;

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

	// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_pd3dDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}


	// Create a root signature consisting of two descriptor tables
	{
		////////
		//CD3DX12_DESCRIPTOR_RANGE specifies the elements of the descriptor table.  for example CD3DX12_DESCRIPTOR_RANGE table[2] would
		//have two elements.  in  example below, there is only 1 element in the table
		CD3DX12_DESCRIPTOR_RANGE texTable;
		texTable.Init(
			D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			1,  // number of texture descriptors
			0); // register t0


		//element for descriptor table
		CD3DX12_DESCRIPTOR_RANGE cbvTable;
		cbvTable.Init(
			D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
			4,  // number of cb descriptors for view and proj matrices, camera pos, quad size
			0 // register b0
		);


		//root signature parameters (consists of two tables).  later, before rendering, we need to call
		//pCommandList->SetGraphicsRootDescriptorTable(0, texHandle);
		//pCommandList->SetGraphicsRootDescriptorTable(1, cbvHandle);
		//This points the table directly to the heap memory where the descriptor handles reside
		CD3DX12_ROOT_PARAMETER rootParameters[2];
		rootParameters[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsDescriptorTable(1, &cbvTable, D3D12_SHADER_VISIBILITY_ALL);


		// Allow input layout and pixel shader access and deny uneccessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

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
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(2, rootParameters,
			1, &sampler,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


		// create a root signature with  slot which points to a descriptor range consisting of a table of textures
		// and a slot for a constant buffer 
		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));

		ThrowIfFailed(m_pd3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_pPointCloudSpriteRootSignature)));
	}

}

void DXModel::CreateRootSignature()
{
	if (m_pRootSignature)
		return;

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
		rootParameters[0].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsDescriptorTable(1, &cbvTable, D3D12_SHADER_VISIBILITY_VERTEX);
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

void DXModel::Render(ComPtr<ID3D12GraphicsCommandList> & pCommandList, const DirectX::XMMATRIX & view, const DirectX::XMMATRIX & proj)
{
	//XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	//XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	//XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	//XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	//DirectX::XMFLOAT4X4 view4x4, proj4x4, viewProj4x4;
	//XMStoreFloat4x4(&view4x4, XMMatrixTranspose(view));
	//XMStoreFloat4x4(&proj4x4, XMMatrixTranspose(proj));
	//XMStoreFloat4x4(&viewProj4x4, XMMatrixTranspose(viewProj));

	if (m_pDXPointCloud)
	{
		if (msbDebugUseSpritePointCloud)
		{
			pCommandList->SetPipelineState(m_pPointCloudSpritePipelineState.Get());
		}
		else
		{
			pCommandList->SetPipelineState(m_pPointCloudPipelineState.Get());
		}
	}
	else
	{
		pCommandList->SetPipelineState(m_pPipelineState.Get());
	}
	
	// Set pipeline state.
	if (m_pDXPointCloud && msbDebugUseSpritePointCloud)
	{
		pCommandList->SetGraphicsRootSignature(m_pPointCloudSpriteRootSignature.Get());
	}
	else
	{
		pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
	}
	
	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//bind the texture view to the root parameter.  The root parameter has nothing to do with the shader register.
	//For example, the shader register for the texture is t0, but the root parameter has nothing to do with that.
	//the  root parameters are set when the signature is created and an array of all the  root parameters is used.
	CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle = m_DXTexture->GetSrvGPUDescriptorHandle(m_pd3dDevice);
	int root_parameter = 0;
	pCommandList->SetGraphicsRootDescriptorTable(root_parameter, texHandle);


	//get mvp matrix and render the mesh
	//static float z_pos = 5.0f;

	//m_WorldMatrix = XMMatrixTranslation(0, 0, z_pos);
	//XMMATRIX mvp = m_WorldMatrix * view * proj;
	//XMMATRIX mvp = proj * view * m_WorldMatrix;

	XMMATRIX wv= XMMatrixMultiply(m_WorldMatrix, view);
	XMMATRIX wvp = XMMatrixMultiply(wv, proj);
	XMMATRIX vp = XMMatrixMultiply(view, proj);


	if (m_pDXPointCloud)
	{
		if (msbDebugUseSpritePointCloud)
		{
			m_pDXPointCloud->RenderPointSpriteCloud(pCommandList, wvp, vp, view);
			
		}
		else
		{
			m_pDXPointCloud->RenderPointCloud(pCommandList, wvp);
		}		
	}
	else
	{
		m_pDXMesh->Render(pCommandList, wvp);
	}
}

void DXModel::Render(ComPtr<ID3D12GraphicsCommandList>& pCommandList, const DirectX::XMMATRIX& view, 
	const DirectX::XMMATRIX& proj, std::vector<DXGraphicsUtilities::SrvParameter>& rootSrvParams)
{
	pCommandList->SetPipelineState(m_pPipelineState.Get());
	pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
	
	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//bind the texture view to the root parameter.  The root parameter has nothing to do with the shader register.
	//For example, the shader register for the texture is t0, but the root parameter has nothing to do with that.
	//the  root parameters are set when the signature is created and an array of all the  root parameters is used.
	CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle = m_DXTexture->GetSrvGPUDescriptorHandle(m_pd3dDevice);
	int root_parameter = 0;
	pCommandList->SetGraphicsRootDescriptorTable(root_parameter, texHandle);

	for (auto param : rootSrvParams)
	{
		pCommandList->SetGraphicsRootDescriptorTable(param.mRootParamIndex, param.mHandle);
	}


	XMMATRIX wv = XMMatrixMultiply(m_WorldMatrix, view);
	XMMATRIX wvp = XMMatrixMultiply(wv, proj);
	XMMATRIX vp = XMMatrixMultiply(view, proj);

	m_pDXMesh->Render(pCommandList, m_WorldMatrix, wvp);
}

void DXModel::RenderPointCloud(ComPtr<ID3D12GraphicsCommandList>& pCommandList,  const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj)
{
	if (msbDebugUseSpritePointCloud)
	{
		pCommandList->SetPipelineState(m_pPointCloudSpritePipelineState.Get());
	}
	else
	{
		pCommandList->SetPipelineState(m_pPointCloudPipelineState.Get());
	}

	// Set pipeline state.
	if (msbDebugUseSpritePointCloud)
	{
		pCommandList->SetGraphicsRootSignature(m_pPointCloudSpriteRootSignature.Get());
	}
	else
	{
		pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
	}


	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//bind the texture view to the root parameter.  The root parameter has nothing to do with the shader register.
	//For example, the shader register for the texture is t0, but the root parameter has nothing to do with that.
	//the  root parameters are set when the signature is created and an array of all the  root parameters is used.
	CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle = m_DXTexture->GetSrvGPUDescriptorHandle(m_pd3dDevice);
	int root_parameter = 0;
	pCommandList->SetGraphicsRootDescriptorTable(root_parameter, texHandle);


	//get mvp matrix and render the mesh
	//static float z_pos = 5.0f;

	//m_WorldMatrix = XMMatrixTranslation(0, 0, z_pos);
	//XMMATRIX mvp = m_WorldMatrix * view * proj;
	//XMMATRIX mvp = proj * view * m_WorldMatrix;

	XMMATRIX wv = XMMatrixMultiply(m_WorldMatrix, view);
	XMMATRIX wvp = XMMatrixMultiply(wv, proj);
	XMMATRIX vp = XMMatrixMultiply(view, proj);

	if (msbDebugUseSpritePointCloud)
	{
		m_pDXPointCloud->RenderPointSpriteCloud(pCommandList, wvp, vp, view);

	}
	else
	{
		m_pDXPointCloud->RenderPointCloud(pCommandList, wvp);
	}
}

void  DXModel::Update(DXCamera* pCamera)
{
	if (m_pDXPointCloud)
	{
		m_pDXPointCloud->Update(pCamera);
	}
}

void  DXModel::LoadModel(const std::string & fileName)
{
	//create new mesh
	m_pDXMesh = std::make_shared<DXMesh>();

	m_pDXMesh->LoadModelFromFile(fileName.c_str(), m_pd3dDevice, m_cbvSrvHeap, m_cbDescriptorIndex, 1.0);
}

void DXModel::LoadPointCloud(const std::string& fileName, bool bSwitchYZAxes)
{
	//create new mesh
	m_pDXPointCloud = std::make_shared<DXPointCloud>();

	DXGraphicsUtilities::vec3 scale{ 1.0f,1.0f, 1.0f };
	m_pDXPointCloud->LoadPointCloudFromFile(fileName.c_str(), m_pd3dDevice, m_cbvSrvHeap, m_cbDescriptorIndex, scale, bSwitchYZAxes);
}

void DXModel::LoadTexture(const std::wstring& strFullPath,  int descriptorIndex, ComPtr<ID3D12Device>& pd3dDevice, ComPtr<ID3D12CommandQueue> & commandQueue)
{
	m_DXTexture = std::make_shared<DXTexture>();


	bool bLoaded = m_DXTexture->CreateTextureFromFile(pd3dDevice, commandQueue, m_cbvSrvHeap, strFullPath, descriptorIndex);
	assert(bLoaded);
}

void DXModel::CreatePointCloudSpritePipelineState()
{

	if ( m_pPointCloudSpritePipelineState )
		return;

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> pointCloudVertexShader;
		ComPtr<ID3DBlob> pointCloudPixelShader;
		ComPtr<ID3DBlob> pointCloudGeometryShader;
		ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		//load shader files from disk
		ThrowIfFailed(D3DCompileFromFile(L"assets/shaders/PointSpriteShaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &pointCloudVertexShader, &error));
		ThrowIfFailed(D3DCompileFromFile(L"assets/shaders/PointSpriteShaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pointCloudPixelShader, &error));
		
		if (!msbDebugUseDiskSprites)
			ThrowIfFailed(D3DCompileFromFile(L"assets/shaders/PointSpriteShaders.hlsl", nullptr, nullptr, "GSMain", "gs_5_0", compileFlags, 0, &pointCloudGeometryShader, &error));
		else
			ThrowIfFailed(D3DCompileFromFile(L"assets/shaders/PointSpriteShaders.hlsl", nullptr, nullptr, "GSMainDisk", "gs_5_0", compileFlags, 0, &pointCloudGeometryShader, &error));


		// Define the vertex input layouts.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			//{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};


		// Describe and create the graphics pipeline state objects (PSOs).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_pPointCloudSpriteRootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(pointCloudVertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pointCloudPixelShader.Get());
		psoDesc.GS = CD3DX12_SHADER_BYTECODE(pointCloudGeometryShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;// D3D12_CULL_MODE_NONE;
		//psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//	psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;  //DXGI_FORMAT_R24G8_TYPELESS
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		ThrowIfFailed(m_pd3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPointCloudSpritePipelineState)));
		NAME_D3D12_OBJECT(m_pPointCloudSpritePipelineState);

	}

}

void  DXModel::LoadTextureResource(ComPtr<ID3D12Device>& device,
	ComPtr<ID3D12CommandQueue>& commandQueue,
	ComPtr<ID3D12DescriptorHeap>& srvDescriptorHeap,
	const std::wstring& strFullPath,
	int nImageWidth, int nImageHeight, int descriptorIndex)
{

	m_DXTexture = std::make_shared<DXTexture>();

	bool bLoaded = m_DXTexture->CreateTextureFromFile(device, commandQueue, srvDescriptorHeap, strFullPath, descriptorIndex);
	
	assert(bLoaded);
}

