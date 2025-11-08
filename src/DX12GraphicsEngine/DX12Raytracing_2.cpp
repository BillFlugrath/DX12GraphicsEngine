
#include "stdafx.h"
#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif

#ifdef TINYOBJLOADER_IMPLEMENTATION
#undef TINYOBJLOADER_IMPLEMENTATION
#endif

#include "DX12Raytracing_2.h"
#include "Win32Application.h"


#include "./Engine/DXCamera.h"
#include "./Engine/DXDescriptorHeap.h"
#include "./Engine/DXGraphicsUtilities.h"
#include "./Engine/DXTexture.h"
#include "./Engine/DXModel.h"
#include "./Engine/DXMesh.h"
#include "./Engine/DXR/DXRManager.h"

using namespace std;


DX12Raytracing_2::DX12Raytracing_2(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name),
	m_Width(width),
	m_Height(height)
{
   
	m_pDXRManager = shared_ptr< DXRManager >(new DXRManager);
	m_pDXCamera = shared_ptr<DXCamera>( new DXCamera );
	m_pDDSCubeMap_0 = shared_ptr<DXTexture>(new DXTexture);
	m_pD3DSceneModels = shared_ptr<D3DSceneModels>(new D3DSceneModels);
	m_pD3DSceneTextures2D = shared_ptr<D3DSceneTextures>(new D3DSceneTextures);
}

DX12Raytracing_2::~DX12Raytracing_2()
{
}

void DX12Raytracing_2::OnDestroy()
{
	//delete descriptor_heap_srv_;
	m_pDXRManager->Cleanup();
}

// Create raytracing device and command list.
void DX12Raytracing_2::CreateRaytracingInterfaces(ID3D12Device* device,
															ID3D12GraphicsCommandList * commandList)
{
	ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&m_dxrDevice)), L"Couldn't get DirectX Raytracing interface for the device.\n");
	ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&m_dxrCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
}

void DX12Raytracing_2::OnInit()
{
	m_pDXRManager->InitD3D12(m_Width, m_Height, Win32Application::GetHwnd(), false);

	D3D12Global d3d = m_pDXRManager->GetD3DGlobal();
	m_dxrDevice = d3d.device;
	m_dxrCommandList = d3d.cmdList;
	m_CmdQueue = d3d.cmdQueue;

	//descriptor_heap_srv_ = new DXDescriptorHeap();
	//descriptor_heap_srv_->Initialize(static_cast<ComPtr<ID3D12Device>>(m_dxrDevice), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, kMaxNumOfCbSrvDescriptorsInHeap);

	//load assets from disk and create D3D resources
	LoadModelsAndTextures();

	//Create the Scene by adding models to a vector.  Each model will become a TLAS instance
	int numModels = 3;
	for (int i = 0; i < numModels; ++i)
	{
		m_pD3DSceneModels->AddModel( D3DModel() );
	}

	std::vector<D3DModel> &vModelObjects = m_pD3DSceneModels->GetModelObjects();

	//Set world space position of the models
	vModelObjects[0].SetPosition(0, 0, 0);
	vModelObjects[1].SetPosition(1.5, 0, 0);
	vModelObjects[2].SetPosition(-50, -1.0, -50);

	//Set hit group for TLAS objects ie set hit group for each model
	vModelObjects[0].SetHitGroupIndex(0);
	vModelObjects[1].SetHitGroupIndex(0);
	vModelObjects[2].SetHitGroupIndex(0);

	//Create D3DTexture objects
	D3DTexture tex0,tex1,tex2,tex3, tex4;
	tex0.m_pTextureResource = m_vTextureObjects[0]->GetDX12Resource();
	tex1.m_pTextureResource = m_vTextureObjects[1]->GetDX12Resource();
	tex2.m_pTextureResource = m_vTextureObjects[2]->GetDX12Resource();
	tex3.m_pTextureResource = m_vTextureObjects[3]->GetDX12Resource();
	tex4.m_pTextureResource = m_pDDSCubeMap_0->GetDX12Resource(); //cubemap is  m_vTextureObjects[4]

	//this vector is for bound textures (ie not in a texture array).  There are only a few.
	std::vector< D3DTexture > vBoundTextures{ tex0, tex1, tex4 }; //bound textures

	//m_pD3DSceneTextures2D holds unbound textures of type textures2D
	m_pD3DSceneTextures2D->AddTexture(tex0); //index 0
	m_pD3DSceneTextures2D->AddTexture(tex1);  //index 1
	m_pD3DSceneTextures2D->AddTexture(tex2);  //index 2
	m_pD3DSceneTextures2D->AddTexture(tex3);  //index 3


	//get texture width and height.  Currently setting these into HLSL to do texture.Load.
	//TODO just use texture samplers.  Remove the texture loads.
	int width = 0, height = 0, width1 = 0, height1 = 0;
	m_vTextureObjects[0]->GetWidthAndHeight(width, height);
	m_vTextureObjects[1]->GetWidthAndHeight(width1, height1);

	//create D3DMeshes with vb amd ib
	D3DMesh mesh0, mesh1, mesh2, mesh3, mesh4;
	DXGraphicsUtilities::CreateD3DMesh(m_vMeshObjects[0], &mesh0); //cube
	DXGraphicsUtilities::CreateD3DMesh(m_vMeshObjects[1], &mesh1); //teapot
	DXGraphicsUtilities::CreateD3DMesh(m_vMeshObjects[2], &mesh2); //sphere
	DXGraphicsUtilities::CreateD3DMesh(m_vMeshObjects[3], &mesh3); //axes
	DXGraphicsUtilities::CreateD3DMesh(m_vMeshObjects[4], &mesh4); //plane

	
	//Add mesh objects to the D3DModel objects.  The meshes have the actual ib and vb resources.
	vModelObjects[0].AddMesh(mesh0);
	vModelObjects[0].AddMesh(mesh3);

	//m_pD3DModel_1 has 1 mesh
	vModelObjects[1].AddMesh(mesh2);

	vModelObjects[2].AddMesh(mesh4);

	// SetTexture2DIndex sets the same index for all meshes in model.  This sets the diffuse (albedo) texture for the
	//mesh objects by setting a numeric index that is used in an unbound array of texture2D objects.
	vModelObjects[0].SetTexture2DIndex(0);
	vModelObjects[1].SetTexture2DIndex(1);
	vModelObjects[2].SetTexture2DIndex(3);

	// SetTexture2DIndexForMesh allow sfor setting the texture for a specific mesh in the model
	vModelObjects[0].SetTexture2DIndexForMesh(0, 1); //set mesh0 texture1

	//Create BLAS and TLAS
	m_pDXRManager->CreateTopAndBottomLevelAS(*m_pD3DSceneModels); 

	//Create signatures and load shaders
	m_pDXRManager->CreateShadersAndRootSignatures(*m_pD3DSceneModels);
	
	
	//create constant buffer D3D12 resources. The CBV (view descriptors) are made after in CreateCBVSRVUAVDescriptorHeap
	m_pDXRManager->CreateConstantBufferResources((float)width); //Used for texture.LOAD.  TODO just use sampler.
	
	m_pDXRManager->CreateCBVSRVUAVDescriptorHeap(*m_pD3DSceneModels, vBoundTextures);  //Sets BOUND textures and bound models
	
	m_pDXRManager->InitializeUnboundResources(*m_pD3DSceneModels, *m_pD3DSceneTextures2D);
	m_pDXRManager->CreateVertexAndIndexBufferSRVs(*m_pD3DSceneModels, *m_pD3DSceneTextures2D);

	m_pDXRManager->Create_PSO_and_ShaderTable();

	m_pDXRManager->ExecuteCommandList();
}

void DX12Raytracing_2::LoadModelsAndTextures()
{
	for (int i=0; i<5; ++i)
		m_vMeshObjects.push_back(shared_ptr<DXMesh>(new DXMesh));


	//load obj models into meshes
	m_vMeshObjects[0]->LoadModelFromFile("./assets/models/cube.obj", static_cast<ID3D12Device*>(m_dxrDevice));
	m_vMeshObjects[1]->LoadModelFromFile("./assets/models/unitTeapot.obj", static_cast<ID3D12Device*>(m_dxrDevice));
	m_vMeshObjects[2]->LoadModelFromFile("./assets/models/unitsphere.obj", static_cast<ID3D12Device*>(m_dxrDevice));
	m_vMeshObjects[3]->LoadModelFromFile("./assets/models/axes.obj", static_cast<ID3D12Device*>(m_dxrDevice));
	m_vMeshObjects[4]->LoadModelFromFile("./assets/models/plane300x300.obj", static_cast<ID3D12Device*>(m_dxrDevice));

	//load 2d textures
	for (int i = 0; i < 5; ++i)
		m_vTextureObjects.push_back(shared_ptr<DXTexture>(new DXTexture));
	
	wstring texture_filepath = kTestPNGFile;
	ComPtr< ID3D12Device> pDevice(m_dxrDevice);
	ComPtr< ID3D12CommandQueue> pCommandQueue(m_CmdQueue);

	bool bLoaded= m_vTextureObjects[0]->CreateTextureFromFile(pDevice, pCommandQueue, texture_filepath);
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
	ID3D12GraphicsCommandList* cmdList= static_cast<ID3D12GraphicsCommandList*>(m_dxrCommandList);
	const wchar_t* szFileName = L"./assets/textures/CubeMaps/snowcube1024.dds";

	m_vTextureObjects[4]->CreateDDSTextureFromFile12(device, pCommandQueue, szFileName);

	m_pDDSCubeMap_0 = m_vTextureObjects[4];
}

// Update frame-based values and update CB data for use by shaders.
void DX12Raytracing_2::OnUpdate()
{
	m_pDXCamera->Update();
	m_pDXRManager->Update(m_pDXCamera->GetViewMatrix(), m_pDXCamera->GetPosition(), m_pDXCamera->GetFOV());
	m_pDXRManager->UpdateUnboundCB(*m_pD3DSceneModels, *m_pD3DSceneTextures2D);
}

void DX12Raytracing_2::OnRender()
{
	m_pDXRManager->Render();
}

