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

class DXMeshShader : public DXModel
{
public:
	DXMeshShader();
	virtual ~DXMeshShader();

	virtual void LoadModelAndTexture(const std::string& modelFileName, const std::wstring& strTextureFullPath,
		ComPtr<ID3D12Device>& pd3dDevice, ComPtr<ID3D12CommandQueue>& pCommandQueue,
		std::shared_ptr<DXDescriptorHeap>& descriptor_heap_srv, const CD3DX12_VIEWPORT& Viewport,
		const CD3DX12_RECT& ScissorRect);

	virtual void Render(ComPtr<ID3D12GraphicsCommandList>& pCommandList, const DirectX::XMMATRIX& view,
		const DirectX::XMMATRIX& proj, std::vector<DXGraphicsUtilities::SrvParameter>& rootSrvParams);

	virtual void Init(ComPtr<ID3D12Device>& pd3dDevice, ComPtr<ID3D12CommandQueue>& commandQueue, ComPtr<ID3D12DescriptorHeap>& m_pCBVSRVHeap,
		CD3DX12_VIEWPORT Viewport,
		CD3DX12_RECT ScissorRect);

	
protected:
	void CreateD3DResources(ComPtr<ID3D12CommandQueue> & commandQueue);
	void CreatePipelineState();
	void CreateRootSignature();
	void CreateSRVs(std::shared_ptr<DXDescriptorHeap>& descriptor_heap_srv);
	void CreateGlobalRootSignature();

	//uint32_t m_CubeTextureDescriptorIndex;
	//CD3DX12_GPU_DESCRIPTOR_HANDLE m_CubemapGPUHandle;
	ComPtr<ID3D12PipelineState> m_pMeshShaderPipelineState;

	ComPtr<ID3D12RootSignature> rootSig;
};