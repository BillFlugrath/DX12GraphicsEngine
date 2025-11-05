#pragma once


#include <vector>

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class DXDescriptorHeap
{
public:
	DXDescriptorHeap();
	~DXDescriptorHeap();

	void Initialize(ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE the_type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, int num_descriptors);
	int GetNewDescriptorIndex();
	ComPtr<ID3D12DescriptorHeap>& GetDescriptorHeap() { return descriptor_heap_;}

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCD3XD12CPUDescriptorHandle(int descriptor_index);
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetCD3DX12GPUDescriptorHandle(int descriptor_index);

protected:

	ComPtr<ID3D12Device> device_;
	D3D12_DESCRIPTOR_HEAP_TYPE descriptor_type_;
	ComPtr<ID3D12DescriptorHeap> descriptor_heap_;

	std::vector<int> descriptor_indices_;
};

