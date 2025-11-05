//Encapsulate compute shader


#pragma once
#include "../DXComputeShader.h"
#include <unordered_map>

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class DXDescriptorHeap;

class DXPointCloudComputeShader_1: public DXComputeShader
{
public:
	DXPointCloudComputeShader_1();
	virtual ~DXPointCloudComputeShader_1();

	bool Initialize(ComPtr<ID3D12Device>& device, std::shared_ptr<DXDescriptorHeap> descriptor_heap_srv, const std::wstring& filename);
	void DoComputeWork();

private:

	//virtual void BuildBuffers();
	virtual void BuildRootSignature();
	//virtual void BuildShadersAndInputLayout(const std::wstring& filename);
	virtual void BuildPSOs();
	
	//------------New
	void BuildResources(); //build buffers we can write into as UAVs and read from as textures
	void BuildDescriptors(); //create srvs and uavs to read and write to the buffers


public:
	UINT mWidth = 32;
	UINT mHeight = 32;
	DXGI_FORMAT mFormat =DXGI_FORMAT_R8G8B8A8_UNORM;

	//indices to retreive GPU and CPU handles from the descriptor heaps
	int mBuff0SrvIndex;
	int mBuff1SrvIndex;
	int mBuff0UavIndex;
	int mBuff1UavIndex;

	//CPU used when creating views/descriptors
	CD3DX12_CPU_DESCRIPTOR_HANDLE mBuff0CpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mBuff0CpuUav;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mBuff1CpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mBuff1CpuUav;

	//GPU used for setting device arguments before draw or dispatch calls
	CD3DX12_GPU_DESCRIPTOR_HANDLE mBuff0GpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mBuff0GpuUav;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mBuff1GpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mBuff1GpuUav;



	// Two for ping-ponging the textures.
	Microsoft::WRL::ComPtr<ID3D12Resource> mBuffMap0 = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBuffMap1 = nullptr; //NOT USED

	std::shared_ptr<DXDescriptorHeap> descriptor_heap_srv_;;


};
