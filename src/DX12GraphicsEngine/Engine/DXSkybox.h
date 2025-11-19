#pragma once

#include "DXGraphicsUtilities.h"
#include "DXModel.h"

#include <string>
using namespace DirectX;

using Microsoft::WRL::ComPtr;

class DXTexture;
class DXMesh;
class DXPointCloud;
class DXCamera;
class DXDescriptorHeap;

class DXSkyBox : public DXModel
{
public:
	DXSkyBox();
	virtual ~DXSkyBox();

	virtual void LoadModelAndTexture(const std::string& modelFileName, const std::wstring& strTextureFullPath,
		ComPtr<ID3D12Device>& pd3dDevice, ComPtr<ID3D12CommandQueue>& pCommandQueue,
		std::shared_ptr<DXDescriptorHeap>& descriptor_heap_srv, const CD3DX12_VIEWPORT& Viewport,
		const CD3DX12_RECT& ScissorRect);

	virtual void Render(ComPtr<ID3D12GraphicsCommandList>& pCommandList, const DirectX::XMMATRIX& view,
		const DirectX::XMMATRIX& proj, std::vector<DXGraphicsUtilities::SrvParameter>& rootSrvParams);
	
protected:
	void CreateD3DResources(ComPtr<ID3D12CommandQueue> & commandQueue);
	void CreatePipelineState();
	void CreateRootSignature();
	void CreateSkyboxSRV(std::shared_ptr<DXDescriptorHeap>& descriptor_heap_srv);

	uint32_t m_CubeTextureDescriptorIndex;
	CD3DX12_GPU_DESCRIPTOR_HANDLE m_CubemapGPUHandle;
	ComPtr<ID3D12PipelineState> m_pSkyboxPipelineState;
	float m_SkyboxSizeScale;

};