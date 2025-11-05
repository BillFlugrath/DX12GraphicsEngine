#pragma once
#include "../DXSampleHelper.h"
//#include "DXSample.h"

#include "DDSTextureLoader.h" 

#if !defined(NO_D3D11_DEBUG_NAME) && ( defined(_DEBUG) || defined(PROFILE) )
#pragma comment(lib,"dxguid.lib")
#endif

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;



class DXTexture
{
public:
	DXTexture();
	~DXTexture();

	bool CreateTextureFromFile(ComPtr<ID3D12Device>& device,
		ComPtr<ID3D12CommandQueue> &commandQueue, 
		ComPtr<ID3D12DescriptorHeap> &srvDescriptorHeap,
		const std::wstring& strFullPath, int descriptorIndex);

	bool CreateTextureFromFile(ComPtr<ID3D12Device>& device, ComPtr<ID3D12CommandQueue>& commandQueue, const std::wstring& strFullPath);

	
	void CreateRenderTargetTexture(ComPtr<ID3D12Device> &device,
		ComPtr<ID3D12DescriptorHeap> &srvHeap,
		ComPtr<ID3D12DescriptorHeap> &rtvHeap,
		int nImageWidth, int nImageHeight,
		int srvDescriptorIndex,
		int rttDescriptorIndex);


	//Calls into MS code in DDSTextureLoader.cpp
	HRESULT CreateDDSTextureFromFile12(_In_ ID3D12Device* device, ComPtr<ID3D12CommandQueue> commandQueue, _In_z_ const wchar_t* szFileName);

	int GetSRVDescriptorIndex() const { return m_SRVDescriptorIndex; }
	int GetRTVDescriptorIndex() const { return m_RTVDescriptorIndex; }

	void TransitionToRenderTarget(ComPtr<ID3D12GraphicsCommandList> & commandList);
	void TransitionToShaderResourceView(ComPtr<ID3D12GraphicsCommandList> & commandList);

	D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView() { return m_TextureShaderResourceView; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() { return m_TextureRenderTargetView; }
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetSrvGPUDescriptorHandle(ComPtr<ID3D12Device>& device);

	void Initialize(ComPtr< ID3D12Resource > pTexture, ComPtr<ID3D12DescriptorHeap> m_srvHeap, 
		D3D12_CPU_DESCRIPTOR_HANDLE m_TextureShaderResourceView, int m_SRVDescriptorIndex, int m_Width, int m_Height);

	void GetWidthAndHeight(int& width, int& height);
	void SetWidthAndHeight(int width, int height) { m_Width = width; m_Height = height; }

	void SetCPUDescriptorHandleForSRVHeap(D3D12_CPU_DESCRIPTOR_HANDLE& handle) { m_TextureShaderResourceView = handle; }
	void SetGPUDescriptorHandleForSRVHeap(D3D12_GPU_DESCRIPTOR_HANDLE handle) { m_GPUDescriptorHandle = handle; }

	void SetSRVDescriptorHeap(ComPtr<ID3D12DescriptorHeap> descriptor_heap) { m_srvHeap = descriptor_heap; }

	void SetSRVDescriptorIndex(int index) { m_SRVDescriptorIndex = index; }
	void SetRTVDescriptorIndex(int index) { m_RTVDescriptorIndex = index; }

	ComPtr< ID3D12Resource >& GetDX12Resource() { return m_pTexture; }

	//Get handles for the descriptors of the texture in the srv heap.  One handle for cpu and one handle for gpu
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandleForSRVHeap() { return m_TextureShaderResourceView; } //same as GetShaderResourceView
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandleForSRVHeap() { return m_GPUDescriptorHandle; }

protected:
	
	ComPtr< ID3D12Resource > m_pTexture;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap; //shared
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap; //shared
	D3D12_CPU_DESCRIPTOR_HANDLE m_TextureShaderResourceView;
	D3D12_CPU_DESCRIPTOR_HANDLE m_TextureRenderTargetView;
	
	int m_SRVDescriptorIndex ; //index in the heap that stores SRV, CBV, UAVs
	int m_RTVDescriptorIndex ; //index in the heap that stores rtts
	unsigned int m_Width;
	unsigned int m_Height;

	D3D12_GPU_DESCRIPTOR_HANDLE m_GPUDescriptorHandle;
};
