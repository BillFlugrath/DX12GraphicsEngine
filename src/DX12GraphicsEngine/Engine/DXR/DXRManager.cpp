#include "stdafx.h"
#include "DXRManager.h"
#include "BLAS_TLAS_Utilities.h"

#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif

#ifdef TINYOBJLOADER_IMPLEMENTATION
#undef TINYOBJLOADER_IMPLEMENTATION
#endif

#include "DXRUtilities.h"

#include "DXShaderUtilities.h"
#include "DXD3DUtilities.h"
#include "DXResourceUtilities.h"
#include "DXRPipelineStateObject.h"
#include "DXResourceBindingUtilities.h"

#include "Utils.h"
#include "Window.h"


const wstring kRayGenFilename(L"Engine\\DXR\\assets\\shaders\\RayGen.hlsl");
const wstring kMissFilename(L"Engine\\DXR\\assets\\shaders\\Miss.hlsl");
const wstring kClosestHitFilename(L"Engine\\DXR\\assets\\shaders\\ClosestHit.hlsl");

const wstring kRayGenUnboundFilename(L"Engine\\DXR\\assets\\shaders\\RayGen_unbound.hlsl");
const wstring kMissUnboundFilename(L"Engine\\DXR\\assets\\shaders\\Miss_unbound.hlsl");
const wstring kClosestHitUnboundFilename(L"Engine\\DXR\\assets\\shaders\\ClosestHit_unbound.hlsl");


DXRManager::DXRManager() :
m_Width(0)
,m_Height(0)
, window(0)
{
	m_pDXRUtilities = shared_ptr< DXRUtilities>(new DXRUtilities);

	m_pDXShaderUtilities = shared_ptr< DXShaderUtilities>(new DXShaderUtilities);
	m_pDXD3DUtilities = shared_ptr< DXD3DUtilities>(new DXD3DUtilities);
	m_pDXResourceUtilities = shared_ptr< DXResourceUtilities>(new DXResourceUtilities);
	m_pBLAS_TLAS_Utilities = shared_ptr<BLAS_TLAS_Utilities>(new BLAS_TLAS_Utilities);
	m_pDXRPipelineStateObject = shared_ptr<DXRPipelineStateObject>(new DXRPipelineStateObject);
	m_pDXResourceBindingUtilities = shared_ptr<DXResourceBindingUtilities>(new DXResourceBindingUtilities);

	if (m_bUseDebugConsole)
		CreateDebugConsole();
}

DXRManager::~DXRManager()
{
	// Free the console when done
	if (m_bUseDebugConsole)
		FreeConsole();
}

void DXRManager::CreateDebugConsole()
{
	// Allocate a new console for the current process
	if (AllocConsole()) {
		// Redirect standard output to the new console
		FILE* pConsole;
		freopen_s(&pConsole, "CONOUT$", "w", stdout);

		std::cout << "Debug output in the new console window." << std::endl;
	}
	else {
		// Handle error if console allocation fails
		std::cerr << "Failed to allocate console." << std::endl;
	}
}

HRESULT DXRManager::InitD3D12(int width, int height, HWND h_window, bool bCreateWindow)
{
	HRESULT hr = EXIT_SUCCESS;

	// Get the application configuration
	// BillF original app used command line in VS to set screen width, height, and test model.
	// -width 640 -height 480 -model ".\models\cube.obj"
	// 
	//ConfigInfo config;
	//hr = Utils::ParseCommandLine(lpCmdLine, config);
	//if (hr != EXIT_SUCCESS) return hr;

	m_bCreateWindow = bCreateWindow;

	if (m_bCreateWindow)
	{
		// Create a new window
		HINSTANCE	instance;
		hr = Window::Create(width, height, instance, window, L"Introduction to DXR");
		Utils::Validate(hr, L"Error: failed to create window!");
	}


	m_pDXRUtilities->Init(m_pDXShaderUtilities, m_pDXD3DUtilities, m_pDXResourceUtilities);

	m_Width = width;
	m_Height = height;
	window = h_window;

	d3d.width = m_Width;
	d3d.height = m_Height;

	// Initialize the shader compiler
	m_pDXShaderUtilities->Init_Shader_Compiler(shaderCompiler);

	// Initialize D3D12
	m_pDXD3DUtilities->Create_Device(d3d);
	m_pDXD3DUtilities->Create_Command_Queue(d3d);
	m_pDXD3DUtilities->Create_Command_Allocator(d3d);
	m_pDXD3DUtilities->Create_Fence(d3d);
	m_pDXD3DUtilities->Create_SwapChain(d3d, window);
	m_pDXD3DUtilities->Create_CommandList(d3d);
	m_pDXD3DUtilities->Reset_CommandList(d3d);

	// Create common resources
	m_pDXResourceUtilities->Create_RTV_And_Sampler_Descriptor_Heaps(d3d, resources);
	m_pDXResourceUtilities->Create_BackBuffer_RTV(d3d, resources);
	m_pDXResourceUtilities->Create_Samplers(d3d, resources);
	m_pDXRUtilities->Create_DXR_Output_Buffer(d3d, resources);

	return hr;
}


HRESULT DXRManager::CreateTopAndBottomLevelAS(D3DSceneModels& d3dSceneModels)
{
	HRESULT hr = EXIT_SUCCESS;
	m_pBLAS_TLAS_Utilities->createAccelerationStructures(d3d, d3dSceneModels);

	//TEMP TO TEST using D3DSceneModels and therefore multiple BLAS structs
	{
		//dxr.BLAS = m_pBLAS_TLAS_Utilities->GetBottomLevelASBufferArray()[0];
		dxr.TLAS = m_pBLAS_TLAS_Utilities->GetTopLevelASBuffer();
	}

	return hr;
}

HRESULT DXRManager::CreateShadersAndRootSignatures()
{
	//The pipeline contains the DXIL code of all the shaders potentially executed during the raytracing process. This section compiles the HLSL code into a
  // set of DXIL libraries. We chose to separate the code in several libraries by semantic (ray generation, hit, miss) for clarity.  
  // Each library is stored in a subobject of type D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY.  We also create a root signature after the shader file is
	//compiled.  The entire shader file is compiled and therefore can have several shader functions in the DXIL

	//A DXIL library can be seen similarly as a regular DLL, which contains compiled code that can be accessed using a number of exported symbols.
	// In the case of the raytracing pipeline, such symbols correspond to the names of the functions implementing the shader programs. 
	// For each file, we then add the library pointer in the pipeline, along with the name of the function it contains.
	//The calls to create the DXIL libs do not use any specific shader names!!  The entire file is compiled into a blob.

	//To use a function exported from a shader, each DX12 shader needs a root signature defining which
	// parameters and buffers will be accessed.  Thus, the function calls below create a DXIL lib for each shader file
	//along with a root signature to be used with the DXIL.

	HRESULT hr = EXIT_SUCCESS;

	if (m_bUseBoundResources)
	{
		m_pDXRUtilities->Create_RayGen_Program(d3d, dxr, shaderCompiler, kRayGenFilename);
		m_pDXRUtilities->Create_Miss_Program(d3d, dxr, shaderCompiler, kMissFilename);
		m_pDXRUtilities->Create_Closest_Hit_Program(d3d, dxr, shaderCompiler, kClosestHitFilename);
	}
	else
	{
		m_pDXRUtilities->Create_RayGen_Program(d3d, dxr, shaderCompiler, kRayGenUnboundFilename);
		m_pDXRUtilities->Create_Miss_Program(d3d, dxr, shaderCompiler, kMissUnboundFilename);
		m_pDXRUtilities->Create_Closest_Hit_Program_Unbound_Resources(d3d, dxr, shaderCompiler, kClosestHitUnboundFilename);
	}

	return hr;
}

HRESULT DXRManager::CreateCBVSRVUAVDescriptorHeap(D3DSceneModels& d3dSceneModels, std::vector<D3DTexture> textures)
{
	HRESULT hr = EXIT_SUCCESS;
	if (m_bUseBoundResources)
		m_pDXRPipelineStateObject->Create_CBVSRVUAV_Heap(d3d, dxr, resources, d3dSceneModels, textures);
	else
		m_pDXRPipelineStateObject->Create_CBVSRVUAV_Heap_Unbound_Resources(d3d, dxr, resources, d3dSceneModels, textures);
		
	return hr;
}

HRESULT DXRManager::CreateUnboundVertexAndIndexBufferSRVs(D3DSceneModels& d3dSceneModels, std::vector<D3DTexture> textures)
{
	HRESULT hr = EXIT_SUCCESS;
	m_pDXResourceBindingUtilities->CreateVertexAndIndexBufferSRVsUnbound(d3d, dxr, resources, d3dSceneModels, textures);
	return hr;
}

HRESULT DXRManager::Create_PSO_and_ShaderTable()
{
	HRESULT hr = EXIT_SUCCESS;

	// Create DXR specific resources
	m_pDXRPipelineStateObject->Create_Pipeline_State_Object(d3d, dxr);

	if (m_bUseBoundResources)
		m_pDXRPipelineStateObject->Create_Shader_Table(d3d, dxr, resources);
	else
		m_pDXRPipelineStateObject->Create_Shader_Table_Unbound_Resources(d3d, dxr, resources);
		
	return hr;
}

void DXRManager::ExecuteCommandList()
{
	d3d.cmdList->Close();
	ID3D12CommandList* pGraphicsList = { d3d.cmdList };
	d3d.cmdQueue->ExecuteCommandLists(1, &pGraphicsList);

	m_pDXD3DUtilities->WaitForGPU(d3d);
	m_pDXD3DUtilities->Reset_CommandList(d3d);

}

HRESULT DXRManager::CreateConstantBufferResources(float textureResolution)
{
	HRESULT hr = S_OK;

	material.textureResolution = textureResolution;

	m_pDXResourceUtilities->Create_View_CB(d3d, resources);
	m_pDXResourceUtilities->Create_Material_CB(d3d, resources, material);

	return hr;
}

void DXRManager::Update(XMMATRIX& view, XMFLOAT3& cam_pos, float cam_fov)
{
	m_pDXResourceUtilities->UpdateCameraCB(d3d, resources, view, cam_pos, cam_fov);
}

void DXRManager::Render()
{
	m_pDXRUtilities->RenderScene(d3d, dxr, resources);
	m_pDXD3DUtilities->Present(d3d);
	m_pDXD3DUtilities->MoveToNextFrame(d3d);
	m_pDXD3DUtilities->Reset_CommandList(d3d);
}

void DXRManager::ResetCommandList()
{
	m_pDXD3DUtilities->Reset_CommandList(d3d);
}

void DXRManager::Cleanup()
{
	m_pDXD3DUtilities->WaitForGPU(d3d);
	CloseHandle(d3d.fenceEvent);

	m_pDXRUtilities->Destroy(dxr);
	m_pDXResourceUtilities->Destroy(resources);
	m_pDXShaderUtilities->Destroy(shaderCompiler);
	m_pDXD3DUtilities->Destroy(d3d);

	if (m_bCreateWindow)
		DestroyWindow(window);
}

void  DXRManager::InitializeUnboundResources(D3DSceneModels& d3dSceneModels, std::vector<D3DTexture>& textures)
{
	m_pDXResourceBindingUtilities->CreateConstantBufferResources(d3d, d3dSceneModels, textures);
}