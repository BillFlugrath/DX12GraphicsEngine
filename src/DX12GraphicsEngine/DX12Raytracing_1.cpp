
#include "stdafx.h"
#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif

#ifdef TINYOBJLOADER_IMPLEMENTATION
#undef TINYOBJLOADER_IMPLEMENTATION
#endif

#include "DX12Raytracing_1.h"
#include "Win32Application.h"


#include "./Engine/DXCamera.h"
#include "./Engine/DXDescriptorHeap.h"
#include "./Engine/DXGraphicsUtilities.h"
#include "./Engine/DXTexture.h"
#include "./Engine/DXModel.h"
#include "./Engine/DXMesh.h"
#include "./Engine/DXR/DXRManager.h"

using namespace std;


DX12Raytracing_1::DX12Raytracing_1(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name),
	m_Width(width),
	m_Height(height)
{
   
	m_pDXRManager = shared_ptr< DXRManager >(new DXRManager);
	m_pDXCamera = shared_ptr<DXCamera>( new DXCamera );
	m_pDXMesh_0 = shared_ptr<DXMesh>(new DXMesh);
	m_pDXMesh_1 = shared_ptr<DXMesh>(new DXMesh);
	m_pDXMesh_2 = shared_ptr<DXMesh>(new DXMesh);
	m_pDXMesh_3 = shared_ptr<DXMesh>(new DXMesh);
	m_pDXTexture_0 = shared_ptr<DXTexture>(new DXTexture);
	m_pDXTexture_1 = shared_ptr<DXTexture>(new DXTexture);
	m_pDDSCubeMap_0 = shared_ptr<DXTexture>(new DXTexture);
	m_pD3DModel_0 = shared_ptr<D3DModel>(new D3DModel);
	m_pD3DModel_1 = shared_ptr<D3DModel>(new D3DModel);
	m_pD3DSceneModels = shared_ptr<D3DSceneModels>(new D3DSceneModels);
}

DX12Raytracing_1::~DX12Raytracing_1()
{
}

void DX12Raytracing_1::OnDestroy()
{
	//delete descriptor_heap_srv_;
	m_pDXRManager->Cleanup();
}

// Create raytracing device and command list.
void DX12Raytracing_1::CreateRaytracingInterfaces(ID3D12Device* device,
															ID3D12GraphicsCommandList * commandList)
{
	ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&m_dxrDevice)), L"Couldn't get DirectX Raytracing interface for the device.\n");
	ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&m_dxrCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");

}

void DX12Raytracing_1::OnInit()
{
	m_pDXRManager->InitD3D12(m_Width, m_Height, Win32Application::GetHwnd(), false);

	D3D12Global d3d = m_pDXRManager->GetD3DGlobal();
	m_dxrDevice = d3d.device;
	m_dxrCommandList = d3d.cmdList;
	m_CmdQueue = d3d.cmdQueue;

	//descriptor_heap_srv_ = new DXDescriptorHeap();
	//descriptor_heap_srv_->Initialize(static_cast<ComPtr<ID3D12Device>>(m_dxrDevice), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, kMaxNumOfCbSrvDescriptorsInHeap);

	LoadModelsAndTextures();

	m_pD3DModel_0->SetHitGroupIndex(0);
	m_pD3DModel_1->SetHitGroupIndex(1);

	D3DTexture tex0,tex1,tex2;
	tex0.m_pTextureResource = m_pDXTexture_0->GetDX12Resource();
	tex1.m_pTextureResource = m_pDXTexture_1->GetDX12Resource();
	tex2.m_pTextureResource = m_pDDSCubeMap_0->GetDX12Resource();

	std::vector< D3DTexture > vTextures{ tex0, tex1, tex2 };

	//create constant buffers
	int width = 0, height = 0, width1 = 0, height1 = 0;
	m_pDXTexture_0->GetWidthAndHeight(width, height);
	m_pDXTexture_1->GetWidthAndHeight(width1, height1);

	//create models with vb amd ib
	D3DMesh mesh0, mesh1, mesh2, mesh3;
	DXGraphicsUtilities::CreateD3DMesh(m_pDXMesh_0, &mesh0); //cube
	DXGraphicsUtilities::CreateD3DMesh(m_pDXMesh_1, &mesh1); //teapot
	DXGraphicsUtilities::CreateD3DMesh(m_pDXMesh_2, &mesh2); //sphere
	DXGraphicsUtilities::CreateD3DMesh(m_pDXMesh_3, &mesh3); //sphere

	static bool bUseOneMesh = false;

	if (bUseOneMesh)
	{
		m_pD3DModel_0->AddMesh(mesh0);
		m_pD3DSceneModels->AddModel(*m_pD3DModel_0);
	}
	else
	{
		//Create D3DModel objects that contain mesh objects
		m_pD3DModel_0->AddMesh(mesh0);
		//m_pD3DModel_0->AddMesh(mesh1);
		//m_pD3DModel_0->AddMesh(mesh3);

		//m_pD3DModel_1 has 1 mesh
		m_pD3DModel_1->AddMesh(mesh2);

		//Create the Final Scene
		m_pD3DSceneModels->AddModel(*m_pD3DModel_0);
		m_pD3DSceneModels->AddModel(*m_pD3DModel_1);
	}
	

	m_pDXRManager->CreateTopAndBottomLevelAS(*m_pD3DSceneModels); 
	m_pDXRManager->CreateShadersAndRootSignatures();

	//create constant buffer D3D12 resources. The CBV (view descriptors) are made after in CreateCBVSRVUAVDescriptorHeap
	m_pDXRManager->CreateConstantBufferResources((float)width);
	m_pDXRManager->CreateCBVSRVUAVDescriptorHeap(*m_pD3DSceneModels, vTextures);

	m_pDXRManager->Create_PSO_and_ShaderTable();
	m_pDXRManager->ExecuteCommandList();
}

void DX12Raytracing_1::LoadModelsAndTextures()
{
	m_pDXMesh_0->LoadModelFromFile("./assets/models/cube.obj", static_cast<ID3D12Device*>(m_dxrDevice));
	m_pDXMesh_1->LoadModelFromFile("./assets/models/unitTeapot.obj", static_cast<ID3D12Device*>(m_dxrDevice));
	m_pDXMesh_2->LoadModelFromFile("./assets/models/unitsphere.obj", static_cast<ID3D12Device*>(m_dxrDevice));
	m_pDXMesh_3->LoadModelFromFile("./assets/models/axes.obj", static_cast<ID3D12Device*>(m_dxrDevice));

	wstring texture_filepath = kTestPNGFile;
	ComPtr< ID3D12Device> pDevice(m_dxrDevice);
	ComPtr< ID3D12CommandQueue> pCommandQueue(m_CmdQueue);

	bool bLoaded=m_pDXTexture_0->CreateTextureFromFile(pDevice, pCommandQueue, texture_filepath);
	assert(bLoaded && "Failed to load texture!");

	const std::wstring kTestPngFile_2 = L"C:./assets/textures/Countdown_02.png";
	bLoaded = m_pDXTexture_1->CreateTextureFromFile(pDevice, pCommandQueue, kTestPngFile_2);
	assert(bLoaded && "Failed to load texture!");

	//load cube map
	ID3D12Device* device = static_cast<ID3D12Device*>(m_dxrDevice);
	ID3D12GraphicsCommandList* cmdList= static_cast<ID3D12GraphicsCommandList*>(m_dxrCommandList);
	const wchar_t* szFileName = L"./assets/textures/CubeMaps/sunsetcube1024.dds";

	m_pDDSCubeMap_0->CreateDDSTextureFromFile12(device, pCommandQueue, szFileName);
}

// Update frame-based values.
void DX12Raytracing_1::OnUpdate()
{
	m_pDXCamera->Update();
	m_pDXRManager->Update(m_pDXCamera->GetViewMatrix(), m_pDXCamera->GetPosition(), m_pDXCamera->GetFOV());
}

void DX12Raytracing_1::OnRender()
{
	m_pDXRManager->Render();
}

