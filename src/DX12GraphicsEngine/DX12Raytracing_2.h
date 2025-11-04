
#pragma once

#include "DXSample.h"

class DXCamera;
class DXModel;
class DXMesh;
class DXTexturedQuad;
class DXDescriptorHeap;
class DXTexture;

class DXRUtilities;
class DXRManager;
struct D3DMesh;
struct D3DModel;
struct D3DSceneModels;

class DX12Raytracing_2 : public DXSample
{
public:
    DX12Raytracing_2(UINT width, UINT height, std::wstring name);
	~DX12Raytracing_2();
   

    // Messages
    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();
   
	//utility functions
	// 
	// Create raytracing device and command list from regular D3D graphics versions
	void CreateRaytracingInterfaces(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	//load scene files
	void LoadModelsAndTextures();

	UINT m_Width;
	UINT m_Height;

	std::shared_ptr<DXCamera> m_pDXCamera;

	std::shared_ptr<DXMesh> m_pDXMesh_0;
	std::shared_ptr<DXMesh> m_pDXMesh_1;
	std::shared_ptr<DXMesh> m_pDXMesh_2;
	std::shared_ptr<DXMesh> m_pDXMesh_3;

	std::shared_ptr<D3DModel> m_pD3DModel_0;
	std::shared_ptr<D3DModel> m_pD3DModel_1;

	std::shared_ptr<D3DSceneModels> m_pD3DSceneModels;

	std::shared_ptr<DXTexture> m_pDXTexture_0;
	std::shared_ptr<DXTexture> m_pDXTexture_1;
	std::shared_ptr<DXTexture> m_pDXTexture_2;
	std::shared_ptr<DXTexture> m_pDDSCubeMap_0;

	//DXDescriptorHeap* descriptor_heap_srv_;

	// DirectX Raytracing (DXR) attributes
	ID3D12Device5 *m_dxrDevice;
	ID3D12GraphicsCommandList4 *m_dxrCommandList;
	ID3D12CommandQueue* m_CmdQueue;


	std::shared_ptr< DXRManager > m_pDXRManager;

	// Root assets path.
	std::wstring m_assetsPath;

};
