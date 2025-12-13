
#include "stdafx.h"
#include <process.h>
#include "DX12MeshShader_1.h"
#include "./Engine/DXTexturedQuad.h"
#include "./Engine/DXTexture.h"
#include "./Engine/DXModel.h"
#include "./Engine/DXGraphicsUtilities.h"
#include "./Engine/DXCamera.h"
#include "./Engine/DXDescriptorHeap.h"
#include  "./Engine/DXComputeShaders/DXPointCloudComputeShader_3.h"
#include "./Engine/DXPointCloud.h"
#include "./Engine/DXSkybox.h"

#include "./Engine/DXR/Common.h"
#include "./Engine/DXR/DXD3DUtilities.h"

#include "./Engine/DXMesh.h"
#include "./Engine/DXR/DXRManager.h"
#include "./Engine/DXMeshShader.h"

DX12MeshShader_1* DX12MeshShader_1::s_app = nullptr;

const float DX12MeshShader_1::LetterboxColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
const float DX12MeshShader_1::ClearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };


DX12MeshShader_1::DX12MeshShader_1(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_frameIndex(0),
	m_fence(nullptr),
    m_sceneViewport(0.0f, 0.0f, 0.0f, 0.0f),
    m_sceneScissorRect(0, 0, 0, 0),
    m_postViewport(0.0f, 0.0f, 0.0f, 0.0f),
    m_postScissorRect(0, 0, 0, 0),
    m_rtvDescriptorSize(0),
    m_cbvSrvDescriptorSize(0),
    m_windowVisible(true),
    m_windowedMode(true),
    m_fenceValues{},
	m_pDXPointCloudModel(nullptr),
	m_DXCamera(nullptr),
	m_pTexturedQuadRTT(nullptr),
	descriptor_heap_srv_(nullptr),
	descriptor_heap_rtv_(nullptr)
{
	s_app = this;

	descriptor_heap_srv_ = std::make_shared<DXDescriptorHeap>();
	descriptor_heap_rtv_ = std::make_shared<DXDescriptorHeap>();

	m_PointCloudComputeShader_3 = std::make_unique<DXPointCloudComputeShader_3>();

	mTexturedQuadRTTWidth = m_width;
	mTexturedQuadRTTHeight = m_height;

	mUavCsTextureWidth = m_width;
	mUavCsTextureHeight = m_height;

	m_pDXRManager = shared_ptr< DXRManager >(new DXRManager);
	m_pDDSCubeMap_0 = shared_ptr<DXTexture>(new DXTexture);
	m_pD3DSceneModels = shared_ptr<D3DSceneModels>(new D3DSceneModels);
	m_pD3DSceneTextures2D = shared_ptr<D3DSceneTextures>(new D3DSceneTextures);

	m_pDXMeshShader = shared_ptr<DXMeshShader>(new DXMeshShader);
}

void DX12MeshShader_1::InitializeComputeShader()
{
	m_PointCloudComputeShader_3->Initialize(m_device, descriptor_heap_srv_, mUavCsTextureWidth, mUavCsTextureHeight,
		L"assets\\Shaders\\computePointCloudShaders_3.hlsl");
}

DX12MeshShader_1::~DX12MeshShader_1()
{
	SAFE_DELETE(m_pTexturedQuadRTT);
	SAFE_DELETE(m_pDXPointCloudModel)
	SAFE_DELETE(m_DXCamera);

	for (DXModel* m : m_DXModelScene)
	{
		SAFE_DELETE(m);
	}
}


void DX12MeshShader_1::OnDestroy()
{
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	WaitForGpu();

	if (!m_tearingSupport)
	{
		// Fullscreen state should always be false before exiting the app.
		ThrowIfFailed(m_swapChain->SetFullscreenState(FALSE, nullptr));
	}

	
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	{
		const UINT64 fence = m_fenceValue;
		const UINT64 lastCompletedFence = m_fence->GetCompletedValue();

		// Signal and increment the fence value.
		ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));
		m_fenceValue++;

		// Wait until the previous frame is finished.
		if (lastCompletedFence < fence)
		{
			ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
			WaitForSingleObject(m_fenceEvent, INFINITE);
		}
		CloseHandle(m_fenceEvent);
	}

	// Close thread events and thread handles.
	for (int i = 0; i < kNumContexts; i++)
	{
		CloseHandle(m_workerBeginRenderFrame[i]);
		CloseHandle(m_workerFinishShadowPass[i]);
		CloseHandle(m_workerFinishedRenderFrame[i]);
		CloseHandle(m_threadHandles[i]);
	}

	m_pDXRManager->Cleanup();
}

void DX12MeshShader_1::CreateDepthTextureSrv()
{
	//get a handle in the srv-cbv-uav descriptor heap
	m_DepthTextureSRVDescriptorIndex = descriptor_heap_srv_->GetNewDescriptorIndex();
	mhDepthMapCpuSrv= descriptor_heap_srv_->GetCD3XD12CPUDescriptorHandle(m_DepthTextureSRVDescriptorIndex);
	mhDepthMapGpuSrv = descriptor_heap_srv_->GetCD3DX12GPUDescriptorHandle(m_DepthTextureSRVDescriptorIndex);

	//Create SRV for Depth map reading in a pixel shader
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	m_device->CreateShaderResourceView(mDepthStencilBuffer.Get(), &srvDesc, mhDepthMapCpuSrv);
}

void DX12MeshShader_1::OnInit()
{
	DXGraphicsUtilities::SetAssetFullPath(m_assetsPath);

	DXModel::SetInlineRayTracing(true);

	//Create device, swap chain, and a RTV descriptor heap named "m_rtvHeap".Below we create a 2nd RTV descriptor heap 
	// named "descriptor_heap_rtv_"  Thus THERE ARE TWO RTV DESCRIPTOR HEAPS!!!
    LoadPipeline();
   
	//descriptor heaps used for rendering 3d models and point clouds. 
	descriptor_heap_srv_->Initialize(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, kMaxNumOfCbSrvDescriptorsInHeap);
	descriptor_heap_rtv_->Initialize(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, kMaxNumOfCbSrvDescriptorsInHeap);

	//Create post process root signature, create post process pipeline state, command lists and 
	// Create a render target view for each texture in the swap chain ie each texture in m_renderTargets at index 0 and 1 in m_rtvHeap
	LoadAssets();

	//Create Depth texture SRV for reading depth texture in pixel shader.  
	CreateDepthTextureSrv();
	
	//Create Constant Buffer and CBV for eading in pixel shader. 
	CreateConstantBuffer();

	CD3DX12_VIEWPORT quad_viewport(0.0f, 0.0f, (float)mTexturedQuadRTTWidth, (float)mTexturedQuadRTTHeight, 0, 1.0);
	CD3DX12_RECT quad_scissor;

	quad_scissor.left = 0;
	quad_scissor.right = mTexturedQuadRTTWidth;
	quad_scissor.top = 0;
	quad_scissor.bottom = mTexturedQuadRTTHeight;
	
	
	//textured quad for rtt.  We use the RTT contained in the DXTexturedQuad as a temp texture that is later used
	//for readback into pixel shader.  We manually set this RTT before drawing by using its cached rtv descriptor,
	// mhTexturedQuadRtv.

	int texture_descriptor_index = descriptor_heap_srv_->GetNewDescriptorIndex(); //index into srv  descriptor heap
	int rtt_texture_descriptor_index = descriptor_heap_rtv_->GetNewDescriptorIndex();; //index into rtv descriptor heap

	m_pTexturedQuadRTT = new DXTexturedQuad();
	m_pTexturedQuadRTT->CreateQuad(m_device, m_commandQueue, descriptor_heap_srv_.get(), quad_viewport, quad_scissor, m_assetsPath);
	
	//CreateRenderTargetTexture creates a new texture as well as srv and rtv for the texture.
	m_pTexturedQuadRTT->CreateRenderTargetTexture(descriptor_heap_srv_->GetDescriptorHeap(), descriptor_heap_rtv_->GetDescriptorHeap(),
		mTexturedQuadRTTWidth, mTexturedQuadRTTHeight, texture_descriptor_index, rtt_texture_descriptor_index);


	//get RTT view for the texture that has a RTT (in our custom class DXTexture)
	mhTexturedQuadRtv = m_pTexturedQuadRTT->GetRtvDescriptorHandle(m_device);

	//load texture for textured quad (not used since we called CreateRenderTargetTexture)
	//m_pTexturedQuadRTT->CreateTextureFromFile(kTestPNGFile, 256, 256, texture_descriptor_index);

	LoadMainSceneModelsAndTextures(quad_viewport, quad_scissor);


	m_DXCamera = new DXCamera();
	m_DXCamera->SetAspectRatio( (float)mTexturedQuadRTTWidth / (float)mTexturedQuadRTTHeight);

	InitThreads();

	//init the compute shader
	InitializeComputeShader();

	//DXR
	InitRayTracing();

	//DXMeshShader init
	std::wstring meshShaderFilename = L"assets/shaders/TriangleMeshShader.hlsl";
	bool bUseEmbeddedRootSig = false;

	static bool bUseMeshlets = true; //toggle meshlets on instead of hardcoded triangle
	if (bUseMeshlets)
	{
		meshShaderFilename = L"assets/shaders/MeshShader.hlsl";
		bUseEmbeddedRootSig = true;
	}
	
	//std::wstring meshletFilename = L"./assets/meshlets/cube3.bin";
	std::wstring meshletFilename = L"./assets/meshlets/sphere1.bin";
	m_pDXMeshShader->Init(m_device, m_commandQueue, descriptor_heap_srv_->GetDescriptorHeap(), quad_viewport, 
		quad_scissor, meshShaderFilename, meshletFilename, bUseEmbeddedRootSig);

	const std::wstring kTestPngFile_1 = L"C:./assets/textures/Countdown_01.png";
	m_pDXMeshShader->AddTexture(kTestPngFile_1, m_device, m_commandQueue, descriptor_heap_srv_);


	ID3D12GraphicsCommandList4* cmdList;
	ID3D12CommandAllocator* cmdAlloc;

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc)));
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, IID_PPV_ARGS(&cmdList)));

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_pDXMeshShader->GetDXTexture()->GetDX12Resource().Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	ThrowIfFailed(cmdList->Close());
	ID3D12CommandList* ppCommandLists[] = { cmdList };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	DXGraphicsUtilities::WaitForGpu(m_device, m_commandQueue);

	cmdList->Release();
	cmdAlloc->Release();
}

void DX12MeshShader_1::LoadMainSceneModelsAndTextures(const CD3DX12_VIEWPORT& quad_viewport, const CD3DX12_RECT& quad_scissor)
{
	//create model 0
	DXModel* pModel0 = new DXModel();
	pModel0->LoadModelAndTexture(kTestObjModelFilename, kTestPNGFile,
		m_device, m_commandQueue, descriptor_heap_srv_, quad_viewport, quad_scissor);
	pModel0->SetModelId(0);
	m_DXModelScene.push_back(pModel0);

	//create model 1
	DXModel* pModel1 = new DXModel();
	pModel1->LoadModelAndTexture("./assets/models/plane300x300_b.obj", L"C:./assets/textures/floorTile.png",
		m_device, m_commandQueue, descriptor_heap_srv_, quad_viewport, quad_scissor);
	pModel1->SetModelId(1);
	pModel1->SetReceiveShadow(true);
	m_DXModelScene.push_back(pModel1);

	DXModel* pModel2 = new DXModel();
	pModel2->LoadModelAndTexture("./assets/models/unitTeapot.obj", kTestPNGFile_2,
		m_device, m_commandQueue, descriptor_heap_srv_, quad_viewport, quad_scissor);
	pModel2->SetModelId(2);
	m_DXModelScene.push_back(pModel2);

	DXModel * pModel3 = new DXModel();
	pModel3->LoadModelAndTexture("./assets/models/unitsphere.obj", L"C:./assets/textures/green.png",
		m_device, m_commandQueue, descriptor_heap_srv_, quad_viewport, quad_scissor);
	pModel3->SetModelId(3);
	m_DXModelScene.push_back(pModel3);

	// skybox
	DXModel* pModel4 = new DXSkyBox();
	const wchar_t* szFileName = L"./assets/textures/CubeMaps/snowcube1024.dds";

	pModel4->LoadModelAndTexture("./assets/models/unitsphere.obj", szFileName,
		m_device, m_commandQueue, descriptor_heap_srv_, quad_viewport, quad_scissor);
	pModel4->SetModelId(4);
	m_DXModelScene.push_back(pModel4);

	//Set Position of scene models
	XMMATRIX world0 = XMMatrixTranslation(2.0f, 0, 2.0f);
	XMMATRIX world1 = XMMatrixTranslation(0.0f, -2.0, 0.0f);
	XMMATRIX world2 = XMMatrixTranslation(-4.0f, 2.0, 0.0f);
	XMMATRIX world3 = XMMatrixTranslation(-3.0f, 2.0, 5.0f);

	pModel0->SetWorldMatrix(world0);
	pModel1->SetWorldMatrix(world1);
	pModel2->SetWorldMatrix(world2);
	pModel3->SetWorldMatrix(world3);

	//Create model (point cloud) and load data from file
	m_pDXPointCloudModel = new DXModel();

	//Set debug flags before creating the point cloud d3d resources
	DXPointCloud::SetDebugVizDepthBuffer(mDebugVizDepthBuffer);

	int cb_descriptor_index_2 = descriptor_heap_srv_->GetNewDescriptorIndex(); //index into descriptor heap cb_descriptor_index for the constant buffer of model

	//set a scissor and viewport that matches the size of the quad RTT we are rendering the model into
	m_pDXPointCloudModel->Init(m_device, m_commandQueue, descriptor_heap_srv_->GetDescriptorHeap(),
		cb_descriptor_index_2, quad_viewport, quad_scissor);

	if (mDebugUseiPhonePointCloud)
	{
		m_pDXPointCloudModel->LoadPointCloud(kiPhonePointCloudFile, true);
	}
	else
	{
		//if here use a procedurally generated point cloud such as a box
		bool bFlipAxes = false;

		if (mDebugUseZPlanePointCloud == false)
			m_pDXPointCloudModel->LoadPointCloud(kBoxPointCloudFile, bFlipAxes);
		else
			m_pDXPointCloudModel->LoadPointCloud(kzPlanePointCloudFile, bFlipAxes);
	}


	//std::shared_ptr<DXPointCloud>& pPointCloudMesh = m_pDXPointCloudModel->GetPointCloudMesh();
	//pPointCloudMesh->SetUseCPUPointSort(true);

	//TODO remove this since point cloud does not require a texture like a model does
	uint32_t texture_descriptor_index = descriptor_heap_srv_->GetNewDescriptorIndex();
	m_pDXPointCloudModel->LoadTexture(kTestPNGFile_2, texture_descriptor_index, m_device, m_commandQueue);
}

// Load the rendering pipeline dependencies.
void DX12MeshShader_1::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
			D3D_FEATURE_LEVEL_12_1,
            IID_PPV_ARGS(&m_device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_12_1,
            IID_PPV_ARGS(&m_device)
            ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
    NAME_D3D12_OBJECT(m_commandQueue);

    // Describe and create the swap chain.
    // The resolution of the swap chain buffers will match the resolution of the window, enabling the
    // app to enter iFlip when in fullscreen mode. We will also keep a separate buffer that is not part
    // of the swap chain as an intermediate render target, whose resolution will control the rendering
    // resolution of the scene.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    // It is recommended to always use the tearing flag when it is available.
    swapChainDesc.Flags = m_tearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));

    if (m_tearingSupport)
    {
        // When tearing support is enabled we will handle ALT+Enter key presses in the
        // window message loop rather than let DXGI handle it by calling SetFullscreenState.
        factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER);
    }

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heap m_rtvHeap
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = kMaxNumOfCbSrvDescriptorsInHeap;// FrameCount + 1; // + 1 for the intermediate render target.
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

    // Create command allocators for each frame.
    for (UINT n = 0; n < FrameCount; n++)
    {
		ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_sceneCommandAllocators[n])));
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_postCommandAllocators[n])));
    }
}

void DX12MeshShader_1::CreatePostRootSignature()
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

	// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}


	//The set of descriptor tables being used at a given time, among other things, are defined as part of the root arguments.
	//The layout of the root arguments, the root signature, is an application specified definition of a binding space
	//(with a limited maximum size for efficiency) that identifies how resources in shaders(SRVs, UAVs, CBVs, Samplers) map into 
	//descriptor table locations.The root signature can also hold a small number of descriptors
	//directly(bypassing the need to put them into descriptor heaps / tables).
	//Finally, the root signature can even hold inline 32 - bit values that show up in the shader as a constant buffer.
	// Create a root signature consisting of a descriptor table with a SRV and a sampler.
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		CD3DX12_ROOT_PARAMETER1 rootParameters[2];

		// We don't modify the SRV in the post-processing command list after
		// SetGraphicsRootDescriptorTable is executed on the GPU so we can use the default
		// range behavior: D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

		//CBV with 1 descriptor
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);

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

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_postRootSignature)));
		NAME_D3D12_OBJECT(m_postRootSignature);
	}
}

void DX12MeshShader_1::CreatePostPipelineState()
{
	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> postVertexShader;
		ComPtr<ID3DBlob> postPixelShader;
		ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ThrowIfFailed(D3DCompileFromFile(L"./assets/shaders/postShaders_2.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &postVertexShader, &error));
		ThrowIfFailed(D3DCompileFromFile(L"./assets/shaders/postShaders_2.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &postPixelShader, &error));

		// Define the vertex input layouts.
		D3D12_INPUT_ELEMENT_DESC scaleInputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state objects (PSOs).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		psoDesc.InputLayout = { scaleInputElementDescs, _countof(scaleInputElementDescs) };
		psoDesc.pRootSignature = m_postRootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(postVertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(postPixelShader.Get());

		//psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		//psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_postPipelineState)));
		NAME_D3D12_OBJECT(m_postPipelineState);
	}
}

// Load the sample assets.
void DX12MeshShader_1::LoadAssets()
{
	//Create post process graphics root signature
	CreatePostRootSignature();

	//Create post process pipeline state
	CreatePostPipelineState();

    // Single-use command allocator and command list for creating resources.
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    // Create the command lists.
    {
		ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_sceneCommandAllocators[m_frameIndex].Get(), m_postPipelineState.Get(), IID_PPV_ARGS(&m_sceneCommandList)));
        ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_postCommandAllocators[m_frameIndex].Get(), m_postPipelineState.Get(), IID_PPV_ARGS(&m_postCommandList)));
       
		NAME_D3D12_OBJECT(m_postCommandList);
		NAME_D3D12_OBJECT(m_sceneCommandList);

        // Close the command lists.
      
        ThrowIfFailed(m_postCommandList->Close());
		ThrowIfFailed(m_sceneCommandList->Close());
    }

    CreateRTViewsForSwapChain();

	UpdateSceneViewportAndScissor();

    CreateIntermediateRttResources();

	UpdatePostViewportAndScissor();

	UpdateTitle();

	CreateQuadVertsAndDepthStencil(commandList);
  
}

void DX12MeshShader_1::CreateQuadVertsAndDepthStencil(ComPtr<ID3D12GraphicsCommandList> commandList)
{
	// Create/update the fullscreen quad vertex buffer.
	ComPtr<ID3D12Resource> postVertexBufferUpload;
	{
		// Define the geometry for a fullscreen quad.
		PostVertex quadVertices[] =
		{
			{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },    // Bottom left.
			{ { -1.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },    // Top left.
			{ { 1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },    // Bottom right.
			{ { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } }        // Top right.
		};

		const UINT vertexBufferSize = sizeof(quadVertices);

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_postVertexBuffer)));

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&postVertexBufferUpload)));

		NAME_D3D12_OBJECT(m_postVertexBuffer);

		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the vertex buffer.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(postVertexBufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, quadVertices, sizeof(quadVertices));
		postVertexBufferUpload->Unmap(0, nullptr);

		commandList->CopyBufferRegion(m_postVertexBuffer.Get(), 0, postVertexBufferUpload.Get(), 0, vertexBufferSize);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_postVertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// Initialize the vertex buffer views.
		m_postVertexBufferView.BufferLocation = m_postVertexBuffer->GetGPUVirtualAddress();
		m_postVertexBufferView.StrideInBytes = sizeof(PostVertex);
		m_postVertexBufferView.SizeInBytes = vertexBufferSize;
	}

	//Create D3D Depth-Stencil Resource
	{
		//call DXSample ie base class
		CreateDepthStencilResources(m_device);

		// Transition the resource from its initial state to be used as a depth buffer.
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	}


	// Close the resource creation command list and execute it to begin the vertex buffer copy into
	// the default heap.
	ThrowIfFailed(commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValues[m_frameIndex]++;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// Wait for the command list to execute before continuing.
		WaitForGpu();
	}
}

void DX12MeshShader_1::CreateRTViewsForSwapChain()
{
    // Create a render target view for each texture in the swap chain ie each texture in m_renderTargets.
	//These descriptors (ie render target views) are created in the render target view heap named "m_rtvHeap" and occupy
	//indices 0 and 1 of this heap.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
			//get the next swap chain buffer and store it in m_renderTargets[n]
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));

			//create a render target view so that we can set the final render target for display
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);

			//get the next handle in the render target descriptor heap m"_rtvHeap"
            rtvHandle.Offset(1, m_rtvDescriptorSize);

            NAME_D3D12_OBJECT_INDEXED(m_renderTargets, n);
        }
    }

    // Update resolutions shown in app title.
    //UpdateTitle();
}

void DX12MeshShader_1::UpdateSceneViewportAndScissor()
{
	// Set up the scene viewport and scissor rect to match the current scene rendering resolution.
	m_sceneViewport.Width = static_cast<float>(m_width);
	m_sceneViewport.Height = static_cast<float>(m_height);

	m_sceneScissorRect.right = static_cast<LONG>(m_width);
	m_sceneScissorRect.bottom = static_cast<LONG>(m_height);
}

// Set up appropriate views for the intermediate render target.
void DX12MeshShader_1::CreateIntermediateRttResources()
{
    // Create RTV for the intermediate render target.
    {
        D3D12_RESOURCE_DESC swapChainDesc = m_renderTargets[m_frameIndex]->GetDesc();
        const CD3DX12_CLEAR_VALUE clearValue(swapChainDesc.Format, ClearColor);
        const CD3DX12_RESOURCE_DESC renderTargetDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            swapChainDesc.Format,
            m_width,
            m_height,
            1u, 1u,
            swapChainDesc.SampleDesc.Count,
            swapChainDesc.SampleDesc.Quality,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            D3D12_TEXTURE_LAYOUT_UNKNOWN, 0u);

		//create a handle to a the descriptor in the rtt descriptor heap (ie m_rtvHeap).  At this point, the handle refers to nothing!
		//We need to create the actual texture resource and a view/views to the resource.  To use as a rtt, we create a render target view.
		//To read the texture in a shader, we need to create a shader resource view of the texture.

		//"FrameCount" is simpy a constant, so we create handle at this offset (ie two) in heap.  The first two descriptor handles in the m_rtvHeap
		//descriptor heap are for the two swap chain buffers.  The 3rd handle is for a descriptor for the m_intermediateRenderTarget.  The m_intermediateRenderTarget
		//is used as both a rtt and a shader texture (unlike the two swap chain rtts that can only be written into to).
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), FrameCount, m_rtvDescriptorSize);
        
		mhIntermediateRtv = rtvHandle; //handle used for clear and set rtt

		//create intermediate rtt resource.  we draw scene into this.  when done, we use this texture as a shader resurce
		//as texture a fullscreen quad with it.  Do not confuse the handle with the resource itself.  We do not directly access the texture resource.
		//We access via handles that reside in an array of handles in descriptor heaps.  To set as a rtt, we use a handle stored in the descriptor heap, ie handle rtvHandle.
		ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &renderTargetDesc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            &clearValue,
            IID_PPV_ARGS(&m_intermediateRenderTarget)));

		//create the render target view with the descriptor handle created above.  This is render target view for m_intermediateRenderTarget.
		// Its descriptor is the third handle in the descriptor heap called "m_rtvHeap"
		//ie rtvHandle points to the rtt view.  We don't directly access the render target view!  We access it through the previosuly created 
		//descriptor handle rtvHandle.
        m_device->CreateRenderTargetView(m_intermediateRenderTarget.Get(), nullptr, rtvHandle);

		//name the actual render target texture resource.
        NAME_D3D12_OBJECT(m_intermediateRenderTarget);
    }

    // Create SRV for the intermediate render target.  The actual texture resource
	// "m_intermediateRenderTarget" has two descriptors 1 for the render target
	//descriptor heap and one for the shader resource descriptor heap 


	//get a handle in the srv-cbv-uav descriptor heap
	m_IntermediateRTTDescriptorIndex = descriptor_heap_srv_->GetNewDescriptorIndex();
	mhIntermediateRttCpuSrv = descriptor_heap_srv_->GetCD3XD12CPUDescriptorHandle(m_IntermediateRTTDescriptorIndex);
	mhIntermediateRttGpuSrv = descriptor_heap_srv_->GetCD3DX12GPUDescriptorHandle(m_IntermediateRTTDescriptorIndex);

	m_device->CreateShaderResourceView(m_intermediateRenderTarget.Get(), nullptr, mhIntermediateRttCpuSrv);
}


// Update frame-based values.
void DX12MeshShader_1::OnUpdate()
{
	m_DXCamera->Update();

	//TODO update all  model transform
	//m_pDXModel->Update();
}

// Render the scene.
void DX12MeshShader_1::OnRender()
{
	if (m_windowVisible)
	{
		PIXBeginEvent(m_commandQueue.Get(), 0, L"Render");

		//fork all worker threads
		for (int i = 0; i < kNumContexts; i++)
		{
			SetEvent(m_workerBeginRenderFrame[i]); // Tell each worker to start drawing.
		}

	
		//wait for workers to be done
		WaitForMultipleObjects(kNumContexts, m_workerFinishedRenderFrame, TRUE, INFINITE);

		RenderPostScene();

		// Execute the command lists of post scene
		ID3D12CommandList* ppCommandLists[] = {
			m_sceneCommandList.Get(),
			m_postCommandList.Get() };

		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		PIXEndEvent(m_commandQueue.Get());

		// When using sync interval 0, it is recommended to always pass the tearing
		// flag when it is supported, even when presenting in windowed mode.
		// However, this flag cannot be used if the app is in fullscreen mode as a
		// result of calling SetFullscreenState.
		UINT presentFlags = (m_tearingSupport && m_windowedMode) ? DXGI_PRESENT_ALLOW_TEARING : 0;

		// Present the frame.
		ThrowIfFailed(m_swapChain->Present(0, presentFlags));

	//	WaitForGpu(); //flush for the test DXTexturedQuad BillF

		MoveToNextFrame();
	}
}

void DX12MeshShader_1::OnKeyDown(UINT8 key)
{
	switch (key)
	{
		// Instrument the Space Bar to toggle between fullscreen states.
		// The window message loop callback will receive a WM_SIZE message once the
		// window is in the fullscreen state. At that point, the IDXGISwapChain should
		// be resized to match the new window size.
		//
		// NOTE: ALT+Enter will perform a similar operation; the code below is not
		// required to enable that key combination.
	case VK_ESCAPE:
	{
		PostQuitMessage(0);
	}
	break;

	case VK_SPACE:
	{
		if (m_tearingSupport)
		{
			Win32Application::ToggleFullscreenWindow();
		}
		else
		{
			BOOL fullscreenState;
			ThrowIfFailed(m_swapChain->GetFullscreenState(&fullscreenState, nullptr));
			if (FAILED(m_swapChain->SetFullscreenState(!fullscreenState, nullptr)))
			{
				// Transitions to fullscreen mode can fail when running apps over
				// terminal services or for some other unexpected reason.  Consider
				// notifying the user in some way when this happens.
				OutputDebugString(L"Fullscreen transition failed");
				assert(false);
			}
		}
		break;
	}

	}
}

void DX12MeshShader_1::UpdateTitle()
{
	// Update resolutions shown in app title.
	wchar_t updatedTitle[256];
	swprintf_s(updatedTitle, L"Screen Resolution( %u x %u ) ", m_width, m_height);
	SetCustomWindowText(updatedTitle);
}


// Set up the screen viewport and scissor rect to match the current window size and scene rendering resolution.
void DX12MeshShader_1::UpdatePostViewportAndScissor()
{
	m_postViewport.TopLeftX = 0 ;
	m_postViewport.TopLeftY = 0;
	m_postViewport.Width = static_cast<float>(m_width);
	m_postViewport.Height = static_cast<float>(m_height);

	m_postScissorRect.left = static_cast<LONG>(m_postViewport.TopLeftX);
	m_postScissorRect.right = static_cast<LONG>(m_postViewport.TopLeftX + m_postViewport.Width);
	m_postScissorRect.top = static_cast<LONG>(m_postViewport.TopLeftY);
	m_postScissorRect.bottom = static_cast<LONG>(m_postViewport.TopLeftY + m_postViewport.Height);
}

void  DX12MeshShader_1::InitThreads()
{
	LoadContexts();
}

// Worker thread body. workerIndex is an integer from 0 to kNumContexts 
// describing the worker's thread index.
void DX12MeshShader_1::WorkerThread(int threadIndex)
{
	assert(threadIndex >= 0);
	assert(threadIndex < kNumContexts);

	while (threadIndex >= 0 && threadIndex < kNumContexts)
	{
		// Wait for main thread to tell us to draw.
		WaitForSingleObject(m_workerBeginRenderFrame[threadIndex], INFINITE);

		//do something...
		
		// Record all the commands we need to render the scene into the command lists
		//for now use only forst worker thread for this
		if (threadIndex == 0)
		{
			/*
			if (mDebugEnableComputeShader)
			{
				m_PointCloudComputeShader_3->DoComputeWork();


				std::shared_ptr<DXTexture>& pDXTexture = m_pDXModel->GetTexture();

				pDXTexture->Initialize(m_PointCloudComputeShader_3->mBuffMap0,
					m_PointCloudComputeShader_3->descriptor_heap_srv_->GetDescriptorHeap(),
					m_PointCloudComputeShader_3->mBuff0CpuSrv,
					m_PointCloudComputeShader_3->mBuff0SrvIndex,
					m_PointCloudComputeShader_3->mWidth,
					m_PointCloudComputeShader_3->mHeight);
					
			}
			*/
			
			m_pDXPointCloudModel->Update(m_DXCamera);

			for (DXModel* pModel : m_DXModelScene)
			{
				pModel->Update(m_DXCamera);
			}

			RenderScene();
		}
			

	//	if (threadIndex == 1)
		//	m_PointCloudComputeShader_3->DoComputeWork();

		// Tell main thread that we are done.
		SetEvent(m_workerFinishedRenderFrame[threadIndex]);

	}

}

// Initialize threads and events.
void DX12MeshShader_1::LoadContexts()
{
	struct threadwrapper
	{
		static unsigned int WINAPI thunk(LPVOID lpParameter)
		{
			ThreadParameter* parameter = reinterpret_cast<ThreadParameter*>(lpParameter);

			//invoke the D3D12Multithreading::WorkerThread member function 
			//This will get invoked for each thread.
			DX12MeshShader_1::Get()->WorkerThread(parameter->threadIndex);
			return 0;
		}
	};

	for (int i = 0; i < kNumContexts; i++)
	{
		m_workerBeginRenderFrame[i] = CreateEvent(
			NULL,
			FALSE,
			FALSE,
			NULL);

		m_workerFinishedRenderFrame[i] = CreateEvent(
			NULL,
			FALSE,
			FALSE,
			NULL);

		m_workerFinishShadowPass[i] = CreateEvent(
			NULL,
			FALSE,
			FALSE,
			NULL);

		m_threadParameters[i].threadIndex = i;

		m_threadHandles[i] = reinterpret_cast<HANDLE>(_beginthreadex(
			nullptr,
			0,
			threadwrapper::thunk,
			reinterpret_cast<LPVOID>(&m_threadParameters[i]),
			0,
			nullptr));

		assert(m_workerBeginRenderFrame[i] != NULL);
		assert(m_workerFinishedRenderFrame[i] != NULL);
		assert(m_threadHandles[i] != NULL);
	}
}

// Fill the command list with all the render commands and dependent state.
void DX12MeshShader_1::RenderScene()
{
	//set near and far planes close together so allow easier depth debugging
	m_DXCamera->SetNearFarPlanes(0.01f, 1000.0f);

	if (mDebugEnableDebugTests)
		DebugTests();

	ThrowIfFailed(m_sceneCommandAllocators[m_frameIndex]->Reset());
	ThrowIfFailed(m_sceneCommandList->Reset(m_sceneCommandAllocators[m_frameIndex].Get(), m_postPipelineState.Get()));
	
	//Draw Model
	CD3DX12_VIEWPORT quad_viewport(0.0f, 0.0f, static_cast<float>(mTexturedQuadRTTWidth), 
									static_cast<float>(mTexturedQuadRTTHeight), 0, 1.0);
	CD3DX12_RECT quad_scissor;

	quad_scissor.left = 0;
	quad_scissor.right = mTexturedQuadRTTWidth;
	quad_scissor.top = 0;
	quad_scissor.bottom = mTexturedQuadRTTHeight;

	m_sceneCommandList->RSSetViewports(1, &quad_viewport);
	m_sceneCommandList->RSSetScissorRects(1, &quad_scissor);

	XMMATRIX view = m_DXCamera->GetViewMatrix();
	XMMATRIX proj = m_DXCamera->GetProjectionMatrix();
	XMMATRIX world0 = XMMatrixTranslation(2.0f, 0, 2.0f);

	float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_sceneCommandList->ClearRenderTargetView(mhTexturedQuadRtv, clear_color, 0, nullptr);
	m_sceneCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	m_sceneCommandList->OMSetRenderTargets(1, &mhTexturedQuadRtv, true, &DepthStencilView());

	//Render 3D models
	if (mDebugRender3dModels)
	{
		DXGraphicsUtilities::SrvParameter param0{ m_BVHGPUHandle,2 };
		DXGraphicsUtilities::SrvParameter param1{ m_CubemapGPUHandle, 3};
		std::vector< DXGraphicsUtilities::SrvParameter> vParams{ param0, param1 };

		for (DXModel* pModel : m_DXModelScene)
		{
			pModel->Render(m_sceneCommandList, view, proj, vParams); //render into RTT contained in quad class
		}

		m_pDXMeshShader->Render(m_sceneCommandList, view, proj, vParams);
	}

	//Render point cloud
	if (mDebugRenderPointCloud)
	{
		m_pDXPointCloudModel->SetWorldMatrix(world0);
		m_pDXPointCloudModel->RenderPointCloud(m_sceneCommandList, view, proj);
	}

	//transition the texture we used as rtt into a shader resource.
	m_pTexturedQuadRTT->GetTexture()->TransitionToShaderResourceView(m_sceneCommandList);

	m_TempRootSignature = DXPointCloud::GetProcessingRootSignature();
	m_TempPipelineState = DXPointCloud::GetProcessingPipelineState();

	// Transition the depth bufferresource from its depth write state to an srv state for shader ead
	m_sceneCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	//set previous rtt as our texture to read in the pixel shader
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvTextureGpu = m_pTexturedQuadRTT->GetSrvGPUDescriptorHandle(m_device);
	
	//update constant buffer data
	UpdateShaderData();

	//Draw a quad using the texture we rendered into above (ie we rendered previously into mhTexturedQuadRtv)
	//(ie the RTT in m_pTexturedQuadRTT).  
	//We set the rtt for the next quad draw as the intermediate texture owned by app ie mhIntermediateRtv  is the render target.  ie we are
	//going to render into m_intermediateRenderTarget.
	m_pTexturedQuadRTT->RenderTexturedQuadIntoRtv(m_sceneCommandList,
		descriptor_heap_srv_->GetDescriptorHeap(),
		srvTextureGpu, //input texture into pixel shader
		mhDepthMapGpuSrv,  //depth texture
		mhIntermediateRtv, //render target we draw into
		mhConstantBufferGpuSrv,
		m_TempRootSignature,
		m_TempPipelineState);
		
	if (mDebugEnableComputeShader)
	{
		ProcessImageWithComputeShader();
		mhTextureInputPostProcessGpuSrv = m_PointCloudComputeShader_3->mBuff0GpuSrv;
	}
	else
	{
		//store texture for final pixel shader
		mhTextureInputPostProcessGpuSrv = mhIntermediateRttGpuSrv;
	}

	//transition the texture we used as shader resource into a rtt for use next frame
	m_pTexturedQuadRTT->GetTexture()->TransitionToRenderTarget(m_sceneCommandList);

	// Transition the depth buffer from shader srv to a writeable depth buffer used for rendering scene
	m_sceneCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	//we are done with the command list for this frame so call close
	ThrowIfFailed(m_sceneCommandList->Close());
}

void DX12MeshShader_1::ProcessImageWithComputeShader()
{
	m_PointCloudComputeShader_3->DoComputeWork(m_sceneCommandList, mhIntermediateRttGpuSrv,
								mhDepthMapGpuSrv, mhConstantBufferGpuSrv);
}

void DX12MeshShader_1::RenderPostScene()
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use fences to determine GPU execution progress.
	ThrowIfFailed(m_postCommandAllocators[m_frameIndex]->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before re-recording.
	ThrowIfFailed(m_postCommandList->Reset(m_postCommandAllocators[m_frameIndex].Get(), m_postPipelineState.Get()));

	// Populate m_postCommandList.  We simply draw a quad with the final scene texture into one of the swap chain buffers.
	//m_renderTargets[m_frameIndex] is the current swap chain buffer (texture resource) that will
	// be displayed on screen. 
	m_postCommandList->SetPipelineState(m_postPipelineState.Get());

	// Set Root Signature for Post Scene Render
	m_postCommandList->SetGraphicsRootSignature(m_postRootSignature.Get());

	//Set the Descriptor Heap of Constant Buffer Views and Shader Resource Views ie CBVs and SRVs
	ID3D12DescriptorHeap* ppHeaps[] = { descriptor_heap_srv_->GetDescriptorHeap().Get() };
	m_postCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// Indicate that the back buffer will be used as a render target and the
	// intermediate render target will be used as a SRV.
	D3D12_RESOURCE_BARRIER barriers[] =
	{
		//m_renderTargets contains the two swap chain buffers (ie backbuffers).  We set m_renderTargets[m_frameIndex] as a render target so we can
		//draw final textured quad into it.  We set m_intermediateRenderTarget as a shader resource since it is the texture we read when rendering the textured
		//quad into m_renderTargets[m_frameIndex].
		CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(m_intermediateRenderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	};

	m_postCommandList->ResourceBarrier(_countof(barriers), barriers);

	m_postCommandList->RSSetViewports(1, &m_postViewport);
	m_postCommandList->RSSetScissorRects(1, &m_postScissorRect);

	//ie we set the final frame buffer to render into.  We previously rendered the entire scene and effects
	//into a texture, so we will now set the swap chain buffer via a render target view
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvSwapChainHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	m_postCommandList->OMSetRenderTargets(1, &rtvSwapChainHandle, FALSE, nullptr);

	//set the "intermediate rtt" texture to read in pixel shader
	int srv_root_parameter = 0;
	m_postCommandList->SetGraphicsRootDescriptorTable(srv_root_parameter, mhTextureInputPostProcessGpuSrv);

	//constant buffer
	int constant_buffer_root_param = 1; //b0 in root param 1
	m_postCommandList->SetGraphicsRootDescriptorTable(constant_buffer_root_param, mhConstantBufferGpuSrv);

	// Clear rtt and set VB
	m_postCommandList->ClearRenderTargetView(rtvSwapChainHandle, LetterboxColor, 0, nullptr);
	m_postCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_postCommandList->IASetVertexBuffers(0, 1, &m_postVertexBufferView);

	PIXBeginEvent(m_postCommandList.Get(), 0, L"Draw texture to screen.");
	m_postCommandList->DrawInstanced(4, 1, 0, 0);
	PIXEndEvent(m_postCommandList.Get());

	//Update swap chain RTT and Intermediate RTT for use on next frame
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; //swap chain rtt
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;  //intermediate rtt
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	m_postCommandList->ResourceBarrier(_countof(barriers), barriers);

	//we are done with the command list for this frame so call close
	ThrowIfFailed(m_postCommandList->Close());
}

void DX12MeshShader_1::UpdateShaderData()
{
	//copy constant buffer data into buffer
	XMMATRIX view = m_DXCamera->GetViewMatrix();
	XMMATRIX proj = m_DXCamera->GetProjectionMatrix();

	//view matrix
	XMStoreFloat4x4(&mShaderData.gView, XMMatrixTranspose(view));

	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMStoreFloat4x4(&mShaderData.gInvView, XMMatrixTranspose(invView));

	//projection matrix
	XMStoreFloat4x4(&mShaderData.gProj, XMMatrixTranspose(proj));

	XMMATRIX invProj= XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMStoreFloat4x4(&mShaderData.gInvProj, XMMatrixTranspose(invProj));

	//quad size
	XMFLOAT2 quad_size = m_pDXPointCloudModel->GetPointCloudMesh()->GetQuadSize();;
	XMFLOAT4 quadSize4 = { quad_size.x, quad_size.y, 0.0f, 0.0f };
	mShaderData.gQuadSize = quadSize4;

	float near_plane = 0, far_plane = 0;
	m_DXCamera->GetNearFarPlanes(near_plane, far_plane);
	XMFLOAT4 cam_props{ near_plane, far_plane, static_cast<float>(m_width), static_cast<float>(m_height)};
	mShaderData.gCameraProperties = cam_props;
	
	memcpy(m_pConstantBufferData, &mShaderData, sizeof(DXGraphicsUtilities::ShaderData));
}

// Wait for pending GPU work to complete.
void DX12MeshShader_1::WaitForGpu()
{
	// Schedule a Signal command in the queue.
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

	// Wait until the fence has been processed.
	ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame.
	m_fenceValues[m_frameIndex]++;
}

// Prepare to render the next frame.
void DX12MeshShader_1::MoveToNextFrame()
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	// Update the frame index (this is simply the frame buffer index)
	// Present has been previously called on swap chain, thus, the back buffer index for next frame has been updated.

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex(); //next frame buffer index 

	//BillF
	//we need to verify that all previous rendering into that new buffer has been completed.  We do this by checking if the
	//current signal on the fence  is greater than or equal to the identifier on the new back buffer.
	//We are preparing to render into buffer with index=m_frameIndex on next frame.  We need to make sure that the
	//next frame buffer has finished with any prior drawing before we proceed to render into it again.
	// If the next frame is not ready to be rendered yet, wait until it is ready.

	// m_fenceValues[m_frameIndex] currently holds an integer fence value used when it was last used as a render target
	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame. 
	// Set the identifier (fence value) to associate with the rendering of data into the frame buffer of index = m_frameIndex
	m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

void DX12MeshShader_1::CreateConstantBuffer()
{
	// Create a constant buffer to hold the global shader data 
	{
		m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64), //min size is PointSpriteShaderData
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_pConstantBuffer));

		// Keep as persistently mapped buffer
		UINT8* pBuffer;
		CD3DX12_RANGE readRange(0, 0);
		m_pConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pBuffer));
		m_pConstantBufferData = pBuffer;

		//get a handle in the srv-cbv-uav descriptor heap
		m_cbDescriptorIndex = descriptor_heap_srv_->GetNewDescriptorIndex();
		mhConstantBufferCpuSrv = descriptor_heap_srv_->GetCD3XD12CPUDescriptorHandle(m_cbDescriptorIndex);
		mhConstantBufferGpuSrv = descriptor_heap_srv_->GetCD3DX12GPUDescriptorHandle(m_cbDescriptorIndex);

		
		//create a buffer view attached to the descriptor
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_pConstantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(DXGraphicsUtilities::ShaderData) + 255) & ~255; // Pad to 256 bytes

		//create constant buffer view for the descriptor
		m_device->CreateConstantBufferView(&cbvDesc, mhConstantBufferCpuSrv);
	}
}

void DX12MeshShader_1::DebugTests()
{
	

}

void DX12MeshShader_1::CreateRaytracingInterfaces(ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList)
{
	ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&m_dxrDevice)), L"Couldn't get DirectX Raytracing interface for the device.\n");
	ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&m_dxrCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
}

void DX12MeshShader_1::LoadModelsAndTextures()
{
	for (int i = 0; i < 5; ++i)
		m_vMeshObjects.push_back(shared_ptr<DXMesh>(new DXMesh));


	//load obj models into meshes
	m_vMeshObjects[0]->LoadModelFromFile("./assets/models/cube.obj", static_cast<ID3D12Device*>(m_dxrDevice));
	m_vMeshObjects[1]->LoadModelFromFile("./assets/models/unitTeapot.obj", static_cast<ID3D12Device*>(m_dxrDevice));
	m_vMeshObjects[2]->LoadModelFromFile("./assets/models/unitsphere.obj", static_cast<ID3D12Device*>(m_dxrDevice));
	m_vMeshObjects[3]->LoadModelFromFile("./assets/models/axes.obj", static_cast<ID3D12Device*>(m_dxrDevice));
	m_vMeshObjects[4]->LoadModelFromFile("./assets/models/plane300x300_b.obj", static_cast<ID3D12Device*>(m_dxrDevice));

	//load 2d textures
	for (int i = 0; i < 5; ++i)
		m_vTextureObjects.push_back(shared_ptr<DXTexture>(new DXTexture));

	wstring texture_filepath = kTestPNGFile;
	ComPtr< ID3D12Device> pDevice(m_dxrDevice);
	ComPtr< ID3D12CommandQueue> pCommandQueue(m_CmdQueue);

	bool bLoaded = m_vTextureObjects[0]->CreateTextureFromFile(pDevice, pCommandQueue, texture_filepath);
	assert(bLoaded && "Failed to load texture!");

	const std::wstring kTestPngFile_1 = L"C:./assets/textures/Countdown_01.png";
	bLoaded = m_vTextureObjects[1]->CreateTextureFromFile(pDevice, pCommandQueue, kTestPngFile_1);
	assert(bLoaded && "Failed to load texture!");

	const std::wstring kTestPngFile_2 = L"C:./assets/textures/Countdown_02.png";
	bLoaded = m_vTextureObjects[2]->CreateTextureFromFile(pDevice, pCommandQueue, kTestPngFile_2);
	assert(bLoaded && "Failed to load texture!");

	const std::wstring kTestPngFile_3 = L"C:./assets/textures/floorTile.png";
	bLoaded = m_vTextureObjects[3]->CreateTextureFromFile(pDevice, pCommandQueue, kTestPngFile_3);
	assert(bLoaded && "Failed to load texture!");

	//load cube map
	ID3D12Device* device = static_cast<ID3D12Device*>(m_dxrDevice);
	ID3D12GraphicsCommandList* cmdList = static_cast<ID3D12GraphicsCommandList*>(m_dxrCommandList);
	const wchar_t* szFileName = L"./assets/textures/CubeMaps/snowcube1024.dds";

	m_vTextureObjects[4]->CreateDDSTextureFromFile12(device, pCommandQueue, szFileName);

	m_pDDSCubeMap_0 = m_vTextureObjects[4];
}

void DX12MeshShader_1::InitRayTracing()
{
	// Initialize D3D12
	D3D12Global &d3d = m_pDXRManager->GetD3DGlobal();
	m_pDXRManager->GetDXD3DUtilities()->Create_Device(d3d);
	m_pDXRManager->GetDXD3DUtilities()->Create_Command_Queue(d3d);
	m_pDXRManager->GetDXD3DUtilities()->Create_Command_Allocator(d3d);
	m_pDXRManager->GetDXD3DUtilities()->Create_Fence(d3d);
	//m_pDXRManager->GetDXD3DUtilities()->Create_SwapChain(d3d, window);
	m_pDXRManager->GetDXD3DUtilities()->Create_CommandList(d3d);
	m_pDXRManager->GetDXD3DUtilities()->Reset_CommandList(d3d);

	
	m_dxrDevice = d3d.device;
	m_dxrCommandList = d3d.cmdList;
	m_CmdQueue = d3d.cmdQueue;

	//CreateRaytracingInterfaces(m_device.Get(), m_sceneCommandList.Get());
	LoadModelsAndTextures();

	CreateRayTracingScene();

	CreateSRVsForRayTracing();

	m_pDXRManager->ExecuteCommandList();
}

void DX12MeshShader_1::CreateRayTracingScene()
{
	//Create the Scene by adding models to a vector.  Each model will become a TLAS instance
	int numModels = 4;
	for (int i = 0; i < numModels; ++i)
	{
		m_pD3DSceneModels->AddModel(D3DModel());
	}

	std::vector<D3DModel>& vModelObjects = m_pD3DSceneModels->GetModelObjects();

	//Set world space position of the models
	vModelObjects[0].SetPosition(2.0, 0, 2.0); //cube and axes
	vModelObjects[1].SetPosition(-3.0, 2.0f, 5.0f); //sphere
	vModelObjects[2].SetPosition(0, -2.0, 0); //plane
	vModelObjects[3].SetPosition(-3, 2.0, 0); //teapot

	//Set hit group for TLAS objects ie set hit group for each model
	//index 1 is used for shadow hit
	vModelObjects[0].SetInstanceContributionToHitGroupIndex(0); //cube and axes
	vModelObjects[1].SetInstanceContributionToHitGroupIndex(3);  //sphere
	vModelObjects[2].SetInstanceContributionToHitGroupIndex(0);  //plane
	vModelObjects[3].SetInstanceContributionToHitGroupIndex(0);  //teapot

	//Create D3DTexture objects
	/*
	D3DTexture tex0, tex1, tex2, tex3, tex4;
	tex0.m_pTextureResource = m_vTextureObjects[0]->GetDX12Resource();
	tex1.m_pTextureResource = m_vTextureObjects[1]->GetDX12Resource();
	tex2.m_pTextureResource = m_vTextureObjects[2]->GetDX12Resource();
	tex3.m_pTextureResource = m_vTextureObjects[3]->GetDX12Resource();
	tex4.m_pTextureResource = m_pDDSCubeMap_0->GetDX12Resource(); //cubemap is  m_vTextureObjects[4]

	//this vector is for bound textures (ie not in a texture array).  There are only a few.
	std::vector< D3DTexture > vBoundTextures{ tex0, tex1, tex4 }; //bound textures

	//m_pD3DSceneTextures2D holds unbound textures of type textures2D
	m_pD3DSceneTextures2D->AddTexture(tex0); //index 0  fish
	m_pD3DSceneTextures2D->AddTexture(tex1);  //index 1 countdown1
	m_pD3DSceneTextures2D->AddTexture(tex2);  //index 2 countdown2
	m_pD3DSceneTextures2D->AddTexture(tex3);  //index 3 countdown3


	//get texture width and height.  Currently setting these into HLSL to do texture.Load.
	int width = 0, height = 0, width1 = 0, height1 = 0;
	m_vTextureObjects[0]->GetWidthAndHeight(width, height);
	m_vTextureObjects[1]->GetWidthAndHeight(width1, height1);
	*/

	//create D3DMeshes with vb amd ib
	D3DMesh mesh0, mesh1, mesh2, mesh3, mesh4;
	DXGraphicsUtilities::CreateD3DMesh(m_vMeshObjects[0], &mesh0); //cube
	DXGraphicsUtilities::CreateD3DMesh(m_vMeshObjects[1], &mesh1); //teapot
	DXGraphicsUtilities::CreateD3DMesh(m_vMeshObjects[2], &mesh2); //sphere
	DXGraphicsUtilities::CreateD3DMesh(m_vMeshObjects[3], &mesh3); //axes
	DXGraphicsUtilities::CreateD3DMesh(m_vMeshObjects[4], &mesh4); //plane


	//Add mesh objects to the D3DModel objects.  The meshes have the actual ib and vb resources.
	vModelObjects[0].AddMesh(mesh0); //cube
	//vModelObjects[0].AddMesh(mesh3); //axes

	//m_pD3DModel_1 has 1 mesh
	vModelObjects[1].AddMesh(mesh2); //sphere

	vModelObjects[2].AddMesh(mesh4); //plane

	vModelObjects[3].AddMesh(mesh1); //teapot

	// SetTexture2DIndex sets the same index for all meshes in model.  This sets the diffuse (albedo) texture for the
	//mesh objects by setting a numeric index that is used in an unbound array of texture2D objects.
	/*
	vModelObjects[0].SetTexture2DIndex(0);
	vModelObjects[1].SetTexture2DIndex(1);
	vModelObjects[2].SetTexture2DIndex(3);
	vModelObjects[3].SetTexture2DIndex(2);

	// SetTexture2DIndexForMesh allows for setting the texture for a specific mesh in the model
	vModelObjects[0].SetTexture2DIndexForMesh(0, 1); //set mesh0 texture1
	*/

	//Create BLAS and TLAS
	m_pDXRManager->CreateTopAndBottomLevelAS(*m_pD3DSceneModels);
}

void  DX12MeshShader_1::CreateSRVsForRayTracing()
{
	// Create the cube map 0 texture SRV : register(t0);
	D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc3 = {};
	textureSRVDesc3.Format = m_pDDSCubeMap_0->GetDX12Resource()->GetDesc().Format;
	textureSRVDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	textureSRVDesc3.Texture2D.MipLevels = m_pDDSCubeMap_0->GetDX12Resource()->GetDesc().MipLevels;
	textureSRVDesc3.Texture2D.MostDetailedMip = 0;
	textureSRVDesc3.Texture2D.ResourceMinLODClamp = 0.0f;
	textureSRVDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	uint32_t texture_descriptor_index = descriptor_heap_srv_->GetNewDescriptorIndex();

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle = descriptor_heap_srv_->GetCD3XD12CPUDescriptorHandle(texture_descriptor_index);
	m_CubemapGPUHandle = descriptor_heap_srv_->GetCD3DX12GPUDescriptorHandle(texture_descriptor_index);
											   
	m_device->CreateShaderResourceView(m_pDDSCubeMap_0->GetDX12Resource().Get(), &textureSRVDesc3, handle);


	// Create the DXR Top Level Acceleration Structure SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = m_pDXRManager->GetDXRGlobal().TLAS.pResult->GetGPUVirtualAddress();


	uint32_t tlas_descriptor_index = descriptor_heap_srv_->GetNewDescriptorIndex();

	CD3DX12_CPU_DESCRIPTOR_HANDLE tlas_cpu_handle = descriptor_heap_srv_->GetCD3XD12CPUDescriptorHandle(tlas_descriptor_index);
	m_device->CreateShaderResourceView(nullptr, &srvDesc, tlas_cpu_handle);

	m_BVHGPUHandle = descriptor_heap_srv_->GetCD3DX12GPUDescriptorHandle(tlas_descriptor_index);
}