#pragma once

#include "DXGraphicsUtilities.h"
#include "DXModel.h"

#include <string>
#include "MeshShaderModel.h"

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

	_declspec(align(256u)) struct SceneConstantBuffer
	{
		XMFLOAT4X4 World;
		XMFLOAT4X4 WorldView;
		XMFLOAT4X4 WorldViewProj;
		uint32_t   DrawMeshlets;
	};

	virtual void Render(ComPtr<ID3D12GraphicsCommandList>& pCommandList, const DirectX::XMMATRIX& view,
		const DirectX::XMMATRIX& proj, std::vector<DXGraphicsUtilities::SrvParameter>& rootSrvParams);

	void Init(ComPtr<ID3D12Device>& pd3dDevice, ComPtr<ID3D12CommandQueue>& commandQueue,
		ComPtr<ID3D12DescriptorHeap>& m_pCBVSRVHeap,CD3DX12_VIEWPORT Viewport, CD3DX12_RECT ScissorRect,
		const std::wstring& shaderFileName, const std::wstring& meshletFileName, bool bUseEmbeddedRootSig,
		bool bUseAmplificationShader);

	void AddTexture(const std::wstring& strTextureFullPath, ComPtr<ID3D12Device>& pd3dDevice, 
		ComPtr<ID3D12CommandQueue>& pCommandQueue, std::shared_ptr<DXDescriptorHeap>& descriptor_heap_srv);

	void ShaderEntryPoint(const std::wstring& meshShaderEntryPoint, const std::wstring& pixelShaderEntryPoint,
		const std::wstring& amplificationShaderEntryPoint);
	
protected:
	void CreateD3DResources(ComPtr<ID3D12CommandQueue> & commandQueue);
	void CreatePipelineState();
	void CreateRootSignature();
	void CreateSRVs(std::shared_ptr<DXDescriptorHeap>& descriptor_heap_srv);
	void CreateGlobalRootSignature();
	void CreateMeshlets(ComPtr<ID3D12CommandQueue>& commandQueue);

	void RenderMeshlets(ComPtr<ID3D12GraphicsCommandList>& pGraphicsCommandList, const DirectX::XMMATRIX& view,
		const DirectX::XMMATRIX& proj, std::vector<DXGraphicsUtilities::SrvParameter>& rootSrvParams);

	void RenderMeshletsWithAS(ComPtr<ID3D12GraphicsCommandList>& pGraphicsCommandList, const DirectX::XMMATRIX& view,
		const DirectX::XMMATRIX& proj, std::vector<DXGraphicsUtilities::SrvParameter>& rootSrvParams);

	void CreateConstantBuffer();

	ComPtr<ID3D12PipelineState> m_pMeshShaderPipelineState;

	ComPtr<ID3D12RootSignature> m_TempRootSig;
	ComPtr<ID3D12Device2> m_pd3dDevice2;
	std::wstring m_ShaderFilename;
	std::wstring m_MeshletFilename;

	std::wstring m_MeshShaderEntryPoint = L"msmain";
	std::wstring m_PixelShaderEntryPoint = L"psmain";
	std::wstring m_AmplificationShaderEntryPoint = L"asmain";

	MeshShaderModel m_Model;
	bool m_bUseShaderEmbeddedRootSig = false; //if true, the root sig is read from compiled shader
	bool m_bUseAmplificationShader = false;

	ComPtr<ID3D12RootSignature> m_pMeshShaderRootSignature;

	//Constant buffer for meshlet rendering to hold matrices of scene
	ComPtr<ID3D12Resource> m_constantBuffer;
	SceneConstantBuffer m_constantBufferData;
	UINT8* m_cbvDataBegin;
};