//Encapsulate compute shader
//based on C:\dx12tests\d3d12book-master\Chapter 13 The Compute Shader\VecAdd

#pragma once
//#include "DXSample.h"
#include <unordered_map>

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class DXDescriptorHeap;

class DXComputeShader
{
public:
	DXComputeShader();
	virtual ~DXComputeShader();

	bool Initialize(ComPtr<ID3D12Device>& device, std::shared_ptr<DXDescriptorHeap> descriptor_heap_srv,  const std::wstring& filename);
	void DoComputeWork();

protected:
	
	//virtual void Update(const GameTimer& gt)override;
//	virtual void Draw(const GameTimer& gt)override;

	virtual void BuildBuffers();
	virtual void BuildRootSignature();
	virtual void BuildShadersAndInputLayout(const std::wstring& filename);
	virtual void BuildPSOs();
//	void BuildFrameResources();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	const int NumDataElements = 32;

	ComPtr<ID3D12Resource> mInputBufferA = nullptr;
	ComPtr<ID3D12Resource> mInputUploadBufferA = nullptr;
	ComPtr<ID3D12Resource> mInputBufferB = nullptr;
	ComPtr<ID3D12Resource> mInputUploadBufferB = nullptr;
	ComPtr<ID3D12Resource> mOutputBuffer = nullptr;
	ComPtr<ID3D12Resource> mReadBackBuffer = nullptr;


	void CreateCommandObjects();
	void FlushCommandQueue();

	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	std::unordered_map<std::wstring, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::wstring, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	

	std::wstring mShaderFilename = L"";
};
