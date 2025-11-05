#pragma once

#include <string>

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class DXTexture;
class DXCamera;
class DXDescriptorHeap;

class DXTexturedQuad
{
public:
	DXTexturedQuad();
	~DXTexturedQuad();

	void CreateQuad(ComPtr<ID3D12Device> &device,
		ComPtr<ID3D12CommandQueue> &commandQueue, 
		DXDescriptorHeap * descriptor_heap_srv,
		CD3DX12_VIEWPORT QuadViewport,
		CD3DX12_RECT QuadScissorRect, const std::wstring& assetPath);

	void CreateTextureFromFile(const std::wstring& strFullPath,  int descriptorIndex );
	void CreateRenderTargetTexture(ComPtr<ID3D12DescriptorHeap> &srvDescriptorHeap, 
								   ComPtr<ID3D12DescriptorHeap> &rttDescriptorHeap, 
								   int nImageWidth, int nImageHeight,  int srvDescriptorIndex, int rttDescriptorIndex);

	void RenderQuad(ComPtr<ID3D12GraphicsCommandList> & pCommandList, CD3DX12_CPU_DESCRIPTOR_HANDLE &rtvHandle);
	void RenderVerticesOnly(ComPtr<ID3D12GraphicsCommandList>& pCommandList);

	//assumes signature uses root param 0 = input texture and root param 1 = constant buffer
	//Render target is member m_DXTexture
	void RenderTexturedQuad(ComPtr<ID3D12GraphicsCommandList>& pCommandList,
		ComPtr<ID3D12DescriptorHeap> cbvSrvHeap,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& inputTextureSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& inputConstantBufferSrv,
		ComPtr<ID3D12RootSignature>& quadRootSignature,
		ComPtr<ID3D12PipelineState>& quadPipelineState);

	//Render into rtv passed as param. root param 0 = input texture.  root param 1= constant buffer
	//Srv ie texture we read in pixel shader is m_DXTexture
	void RenderTexturedQuadIntoRtv(ComPtr<ID3D12GraphicsCommandList>& pCommandList,
		ComPtr<ID3D12DescriptorHeap> cbvSrvHeap,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& inputTextureRtv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& inputConstantBufferSrv,
		ComPtr<ID3D12RootSignature>& quadRootSignature,
		ComPtr<ID3D12PipelineState>& quadPipelineState);

	//Both render target texture and input texture specified in parameters
	// root param 0 = input texture.  root param 1= constant buffer
	void RenderTexturedQuadIntoRtv(ComPtr<ID3D12GraphicsCommandList>& pCommandList,
		ComPtr<ID3D12DescriptorHeap> cbvSrvHeap,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& inputTextureSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& inputTextureRtv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& inputConstantBufferSrv,
		ComPtr<ID3D12RootSignature>& quadRootSignature,
		ComPtr<ID3D12PipelineState>& quadPipelineState);

	//Both render target texture and TWO input textures specified in parameters
	// root param 0 = input texture.  root param 1= constant buffer  root param 2 = input texture 2
	void RenderTexturedQuadIntoRtv(ComPtr<ID3D12GraphicsCommandList>& pCommandList,
		ComPtr<ID3D12DescriptorHeap> cbvSrvHeap, //descriptor heap for texture srvs and cbvs
		CD3DX12_GPU_DESCRIPTOR_HANDLE& inputTextureSrv_1, //input tex for root param 0
		CD3DX12_GPU_DESCRIPTOR_HANDLE& inputTextureSrv_2, //input tex for root param 2
		CD3DX12_CPU_DESCRIPTOR_HANDLE& inputTextureRtv, //render target to draw into
		CD3DX12_GPU_DESCRIPTOR_HANDLE& inputConstantBufferSrv,  //input cbv for root param 1
		ComPtr<ID3D12RootSignature>& quadRootSignature,
		ComPtr<ID3D12PipelineState>& quadPipelineState);
	
	void RenderQuad(ComPtr<ID3D12GraphicsCommandList> & pCommandList, CD3DX12_CPU_DESCRIPTOR_HANDLE &rtvHandle,
		const CD3DX12_VIEWPORT & viewport, const CD3DX12_RECT & scissor_rect);

	void RenderWorldQuad(ComPtr<ID3D12GraphicsCommandList> & pCommandList, CD3DX12_CPU_DESCRIPTOR_HANDLE &rtvHandle,
		const CD3DX12_VIEWPORT & viewport, const CD3DX12_RECT & scissor_rect, DXCamera *dx_camera);

	void SetTexture(std::shared_ptr<DXTexture>& pTexture);
	std::shared_ptr<DXTexture>& GetTexture() {return m_DXTexture;}

	void SetViewport(const CD3DX12_VIEWPORT & viewport) { m_QuadViewport = viewport; }
	void SetScissorRect(const CD3DX12_RECT scissor_rect) { m_QuadScissorRect = scissor_rect; }
	
	//returns current signature and pipeline state before setting new one
	ComPtr<ID3D12RootSignature> SetRootSignature(ComPtr<ID3D12RootSignature> quadRootSignature);
	ComPtr<ID3D12PipelineState> SetPipelineState(ComPtr<ID3D12PipelineState> quadPipelineState);

	ComPtr<ID3D12RootSignature> GetRootSignature() { return m_QuadRootSignature;}
	ComPtr<ID3D12PipelineState> GetPipelineState() { return m_QuadPipelineState; }

	CD3DX12_GPU_DESCRIPTOR_HANDLE GetSrvGPUDescriptorHandle(ComPtr<ID3D12Device>& device);
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtvDescriptorHandle(ComPtr<ID3D12Device>& device);

protected:
	struct QuadVertex
	{
		XMFLOAT4 position;
		XMFLOAT2 uv;
	};

	void CreatePipelineState(const std::wstring& vertex_shader_file, const std::wstring& pixel_shader_file,
		const std::string& vs_entry, const std::string& ps_entry);

	void CreateRootSignature();
	void CreateConstantBuffer();
	
	// Helper function for resolving the full path of assets.
	std::wstring GetAssetFullPath(LPCWSTR assetName) { return m_assetsPath + assetName; }

	void CreateQuadVertices(const std::vector<QuadVertex> & quadVertices);


	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_commandQueue; //TODO remove.  dont use list/queue.   use a shared queue
	ComPtr<ID3D12RootSignature> m_QuadRootSignature;
	ComPtr<ID3D12DescriptorHeap> m_cbvSrvHeap; //shared.  used to store descriptor for constant buffer view
	ComPtr<ID3D12PipelineState> m_QuadPipelineState;
	UINT m_cbvSrvDescriptorSize;

	ComPtr<ID3D12Resource> m_QuadVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_QuadVertexBufferView;

	CD3DX12_VIEWPORT m_QuadViewport;
	CD3DX12_RECT m_QuadScissorRect;

	// Root assets path.
	std::wstring m_assetsPath;

	std::shared_ptr<DXTexture> m_DXTexture;  //either created from a file loaded or created as a new rtt.

	//constant buffer data for a world space quad (ie instea of a screen space quad)
	XMMATRIX     m_WorldMatrix;
	int m_cbDescriptorIndex;
	ComPtr< ID3D12Resource > m_pConstantBuffer;
	UINT8 *m_pConstantBufferData;

};
