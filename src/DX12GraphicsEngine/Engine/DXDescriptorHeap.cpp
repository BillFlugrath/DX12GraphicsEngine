#include "stdafx.h"
#include "DXDescriptorHeap.h"
#include "../DXSampleHelper.h"

DXDescriptorHeap::DXDescriptorHeap() :
descriptor_type_(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
{
}

DXDescriptorHeap::~DXDescriptorHeap()
{
}

void DXDescriptorHeap::Initialize (ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE the_type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, int num_descriptors)
{
	device_ = device;
	descriptor_type_ = the_type;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = num_descriptors;
	heapDesc.Type = descriptor_type_;
	heapDesc.Flags = flags;
	ThrowIfFailed(device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptor_heap_)));


}

CD3DX12_CPU_DESCRIPTOR_HANDLE DXDescriptorHeap::GetCD3XD12CPUDescriptorHandle(int descriptor_index)
{
	UINT nCBVSRVDescriptorSize = device_->GetDescriptorHandleIncrementSize(descriptor_type_);

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle(descriptor_heap_->GetCPUDescriptorHandleForHeapStart());
	cpu_handle.Offset(descriptor_index, nCBVSRVDescriptorSize);

	return cpu_handle;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DXDescriptorHeap::GetCD3DX12GPUDescriptorHandle(int descriptor_index)
{
	UINT nCBVSRVDescriptorSize = device_->GetDescriptorHandleIncrementSize(descriptor_type_);
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpu_handle(descriptor_heap_->GetGPUDescriptorHandleForHeapStart(), descriptor_index, nCBVSRVDescriptorSize);
	return gpu_handle;
}

int  DXDescriptorHeap::GetNewDescriptorIndex()
{
	int sz = descriptor_indices_.size();
	int new_index = sz;
	descriptor_indices_.push_back(new_index);

	return new_index;
}
