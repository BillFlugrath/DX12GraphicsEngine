#pragma once

#include "Common.h"
#include "Structures.h"
#include <string>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class DXShaderUtilities;
class DXD3DUtilities;
class DXResourceUtilities;
class DXRUtilities;
class BLAS_TLAS_Utilities;
class DXRPipelineStateObject;
class DXResourceBindingUtilities;
class DXRShaderBindingTable;

class DXRManager
{
public:
	DXRManager();
	~DXRManager();

	HRESULT InitD3D12(int width, int height, HWND h_window, bool bCreateWindow);

	HRESULT CreateConstantBufferResources(float textureResolution);

	HRESULT CreateVertexAndIndexBufferSRVs(D3DSceneModels& d3dSceneModels, D3DSceneTextures& textures2D);
	
	HRESULT CreateCBVSRVUAVDescriptorHeap(D3DSceneModels& d3dSceneModels, std::vector<D3DTexture> textures);

	HRESULT Create_PSO_and_ShaderTable();

	HRESULT CreateTopAndBottomLevelAS(D3DSceneModels& d3dSceneModels);

	HRESULT CreateShadersAndRootSignatures(D3DSceneModels& d3dSceneModels);

	void ExecuteCommandList();
	void ResetCommandList();

	void Update(XMMATRIX& view, XMFLOAT3& cam_pos, float cam_fov);
	void UpdateUnboundCB(D3DSceneModels& d3dSceneModels, D3DSceneTextures& textures2D);
	void Render();

	void Cleanup();

	D3D12Global& GetD3DGlobal() { return d3d; }

	void InitializeUnboundResources(D3DSceneModels& d3dSceneModels, D3DSceneTextures& textures2D);

protected:
	
	//Dimesnions of rt output UAV
	int m_Width;
	int m_Height;

	std::shared_ptr<DXRUtilities> m_pDXRUtilities;

	std::shared_ptr< DXShaderUtilities > m_pDXShaderUtilities;
	std::shared_ptr< DXD3DUtilities > m_pDXD3DUtilities;
	std::shared_ptr< DXResourceUtilities> m_pDXResourceUtilities;

	std::shared_ptr< BLAS_TLAS_Utilities> m_pBLAS_TLAS_Utilities;
	std::shared_ptr< DXRPipelineStateObject> m_pDXRPipelineStateObject;
	std::shared_ptr< DXResourceBindingUtilities> m_pDXResourceBindingUtilities;
	std::shared_ptr< DXRShaderBindingTable > m_pDXRShaderBindingTable;

	//Example members
	HWND window;
	Material material;  //deprecated used to load a simple obj via tinyobj

	//Globals
	DXRGlobal dxr = {};
	D3D12Global d3d = {};
	D3D12Resources resources = {};
	D3D12ShaderCompilerInfo shaderCompiler;

	bool m_bCreateWindow = false;

	//--------------------- Debug members --------------------------------------
	void CreateDebugConsole();
	bool m_bUseDebugConsole = true;
};
