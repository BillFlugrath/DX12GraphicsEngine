#include "stdafx.h"
#include "DXTexture.h"
#include "DXGraphicsUtilities.h"

DXTexture::DXTexture() :
m_Width(0)
,m_Height(0)
,m_SRVDescriptorIndex (kInvalidDescriptorHandle)
, m_RTVDescriptorIndex(kInvalidDescriptorHandle)
{
}

DXTexture::~DXTexture()
{
	m_pTexture = nullptr;
	m_srvHeap = nullptr; 
	m_rtvHeap = nullptr; 
}

void DXTexture::Initialize(ComPtr< ID3D12Resource > pTexture, ComPtr<ID3D12DescriptorHeap> srvHeap,
	D3D12_CPU_DESCRIPTOR_HANDLE TextureShaderResourceView, int SRVDescriptorIndex, int Width, int Height)
{
	m_pTexture = pTexture;
	m_srvHeap = srvHeap;
	
	m_TextureShaderResourceView = TextureShaderResourceView;
	
	m_SRVDescriptorIndex = SRVDescriptorIndex; //index in the heap that stores SRV, CBV, UAVs

	m_Width = Width;
	m_Height = Height;
}

bool DXTexture::CreateTextureFromFile(ComPtr<ID3D12Device>& device, ComPtr<ID3D12CommandQueue>& commandQueue, const std::wstring& strFullPath)
{
	//load png  m_Width  and m_Height get filled in
	bool bLoaded = DXGraphicsUtilities::LoadPNGTextureMap(strFullPath, device, commandQueue, m_pTexture, m_Width, m_Height);

	return bLoaded;
}

bool DXTexture::CreateTextureFromFile(ComPtr<ID3D12Device> &device,
	ComPtr<ID3D12CommandQueue> &commandQueue,
	ComPtr<ID3D12DescriptorHeap> &srvDescriptorHeap,
	const std::wstring& strFullPath,
	int descriptorIndex
	)
{
	m_SRVDescriptorIndex  = descriptorIndex;
	m_srvHeap = srvDescriptorHeap;
	
	//load png
	bool bLoaded=DXGraphicsUtilities::LoadPNGTextureMap(strFullPath, device, commandQueue, m_pTexture, m_Width, m_Height);

	
	// Create shader resource view descriptor in the heap
	UINT nCBVSRVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	int texture_index = m_SRVDescriptorIndex ;
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	srvHandle.Offset(texture_index, nCBVSRVDescriptorSize);

	//the call to create CreateShaderResourceView takes a CPU handle.  After this call is complete, we can set the
	//GPU despcriptor on the graphics root since a valid GPU descriptor will automatically exist.
	device->CreateShaderResourceView(m_pTexture.Get(), nullptr, srvHandle);
	m_TextureShaderResourceView = srvHandle;


	return bLoaded;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DXTexture::GetSrvGPUDescriptorHandle(ComPtr<ID3D12Device>& device)
{
	UINT nCBVSRVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvGPUHandle(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), m_SRVDescriptorIndex, nCBVSRVDescriptorSize);
	return srvGPUHandle;
}

void  DXTexture::GetWidthAndHeight(int& width, int& height)
{
	width = m_Width;
	height = m_Height;
}

void DXTexture::CreateRenderTargetTexture(ComPtr<ID3D12Device> &device,
	ComPtr<ID3D12DescriptorHeap> &srvDescriptorHeap,
	ComPtr<ID3D12DescriptorHeap> &rtvDescriptorHeap,
	int nImageWidth, int nImageHeight,
	int srvDescriptorIndex,
	int rttDescriptorIndex)
{

	m_srvHeap = srvDescriptorHeap; 
	m_rtvHeap = rtvDescriptorHeap;

	m_Width =  static_cast<int>(nImageWidth);
	m_Height =  static_cast<int>(nImageHeight);
	m_SRVDescriptorIndex  = srvDescriptorIndex;
	m_RTVDescriptorIndex =  rttDescriptorIndex;

	const float clearCol[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	const CD3DX12_CLEAR_VALUE clearValue(DXGI_FORMAT_R8G8B8A8_UNORM, clearCol);

	//create the rtt descriptor of the resource
	const CD3DX12_RESOURCE_DESC renderTargetDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		nImageWidth,
		nImageHeight,
		1u, 1u,
		1, //count
		0, //quality
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		D3D12_TEXTURE_LAYOUT_UNKNOWN, 0u);

	//create the resource
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&renderTargetDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&m_pTexture)));

	//create rtt view
	UINT  rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_RTVDescriptorIndex, rtvDescriptorSize);
	device->CreateRenderTargetView(m_pTexture.Get(), nullptr, rtvHandle);

	//store the handle
	m_TextureRenderTargetView = rtvHandle;

	NAME_D3D12_OBJECT(m_pTexture);

	// Create shader resource view descriptor in the heap so we can read texture in shader
	UINT nCBVSRVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	int texture_index = m_SRVDescriptorIndex;
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	srvHandle.Offset(texture_index, nCBVSRVDescriptorSize);
	device->CreateShaderResourceView(m_pTexture.Get(), nullptr, srvHandle);

	//store the handle
	m_TextureShaderResourceView = srvHandle;
}

void DXTexture::TransitionToRenderTarget(ComPtr<ID3D12GraphicsCommandList> & commandList)
{
	D3D12_RESOURCE_BARRIER barriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(m_pTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET )
	};

	commandList->ResourceBarrier(_countof(barriers), barriers);
}

void DXTexture::TransitionToShaderResourceView(ComPtr<ID3D12GraphicsCommandList> & commandList)
{
	D3D12_RESOURCE_BARRIER barriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(m_pTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	};

	commandList->ResourceBarrier(_countof(barriers), barriers);
}

HRESULT DXTexture::CreateDDSTextureFromFile12(_In_ ID3D12Device* device, ComPtr<ID3D12CommandQueue> commandQueue, _In_z_ const wchar_t* szFileName
)
{
	HRESULT hr = S_OK;

	ID3D12GraphicsCommandList4* cmdList;
	ID3D12CommandAllocator* cmdAlloc;

	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc)) );
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, IID_PPV_ARGS(&cmdList)) );
	
	Microsoft::WRL::ComPtr<ID3D12Resource> textureUploadHeap;
	size_t maxsize = 0;
	DDS_ALPHA_MODE* alphaMode = nullptr;

	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(device,cmdList, szFileName, m_pTexture, textureUploadHeap));

	m_Width = m_pTexture->GetDesc().Width;
	m_Height = m_pTexture->GetDesc().Height;

/*  //We can create a unique queue as well used for creating texture resource as shown below.  For now, we use main queue passed in from app
	ComPtr<ID3D12CommandQueue> commandQueue;
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));
	*/

	ThrowIfFailed(cmdList->Close());
	ID3D12CommandList* ppCommandLists[] = { cmdList };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	ComPtr<ID3D12Device> pDevice(device);
	DXGraphicsUtilities::WaitForGpu(pDevice, commandQueue);

	if (cmdList)
		cmdList->Release();

	if (cmdAlloc)
		cmdAlloc->Release();
	
	return hr;
}