#pragma once

using namespace DirectX;

using Microsoft::WRL::ComPtr;

class DXTexture;
class DXMesh;
class DXPointCloud;
class DXCamera;

class DXModel
{
public:
	DXModel();
	~DXModel();

	void Render(ComPtr<ID3D12GraphicsCommandList> & pCommandList, const DirectX::XMMATRIX & view, const DirectX::XMMATRIX & proj) ;
	void RenderPointCloud(ComPtr<ID3D12GraphicsCommandList>& pCommandList, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);

	void SetWorldMatrix(const XMMATRIX &world) {m_WorldMatrix = world;}

	 void Update(DXCamera *pCamera=nullptr);
	 void Init(ComPtr<ID3D12Device>& pd3dDevice, ComPtr<ID3D12CommandQueue> & commandQueue, ComPtr<ID3D12DescriptorHeap>& m_pCBVSRVHeap,
		 int cbDescriptorIndex,
		 CD3DX12_VIEWPORT Viewport,
		 CD3DX12_RECT ScissorRect );

	void LoadModel(const std::string & fileName); // filename is the entire path
	void LoadPointCloud(const std::string& fileName, bool bSwitchYZAxes); // filename is the entire path

	void LoadTexture(const std::wstring& strFullPath,  int descriptorIndex, ComPtr<ID3D12Device>& pd3dDevice, ComPtr<ID3D12CommandQueue> & commandQueue);
	std::shared_ptr<DXTexture>& GetTexture() { return m_DXTexture; }

	std::shared_ptr<DXPointCloud>& GetPointCloudMesh() { return m_pDXPointCloud; }

	void LoadTextureResource(ComPtr<ID3D12Device>& device,
		ComPtr<ID3D12CommandQueue>& commandQueue,
		ComPtr<ID3D12DescriptorHeap>& srvDescriptorHeap,
		const std::wstring& strFullPath,
		int nImageWidth, int nImageHeight, int descriptorIndex);

	//Get diffuse texture
	std::shared_ptr<DXTexture>& GetDXTexture() { return m_DXTexture; }
	//get DXMesh
	std::shared_ptr<DXMesh> GetDXMesh() { return m_pDXMesh; }

	//get pso
	ComPtr<ID3D12PipelineState>& GetPipelineState() { return m_pPipelineState; }

protected:
	void CreateD3DResources(ComPtr<ID3D12CommandQueue> & commandQueue);
	void CreatePipelineState();
	void CreatePointCloudPipelineState();
	void CreatePointCloudSpritePipelineState();
	void CreateRootSignature();
	void CreatePointCloudSpriteRootSignature();

	ComPtr<ID3D12Device>      m_pd3dDevice;//set by another object
	ComPtr<ID3D12DescriptorHeap> m_cbvSrvHeap; //shared

	UINT m_cbvSrvDescriptorSize;
	int m_cbDescriptorIndex;

	static ComPtr<ID3D12PipelineState> m_pPipelineState;
	static ComPtr<ID3D12PipelineState> m_pPointCloudPipelineState;
	static ComPtr<ID3D12PipelineState> m_pPointCloudSpritePipelineState;

	static ComPtr<ID3D12RootSignature> m_pRootSignature;
	static ComPtr<ID3D12RootSignature> m_pPointCloudSpriteRootSignature;


	CD3DX12_VIEWPORT m_Viewport;
	CD3DX12_RECT m_ScissorRect;

	std::shared_ptr<DXMesh> m_pDXMesh;
	std::shared_ptr<DXPointCloud> m_pDXPointCloud;
	XMMATRIX     m_WorldMatrix;

	std::shared_ptr<DXTexture> m_DXTexture;

	
public:
	static bool msbDebugUseSpritePointCloud;
	static bool msbDebugUseDiskSprites;
};