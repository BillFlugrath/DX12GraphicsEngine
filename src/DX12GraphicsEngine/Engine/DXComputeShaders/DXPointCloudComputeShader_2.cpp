
#include "stdafx.h"
#include "DXPointCloudComputeShader_2.h"
#include <DirectXPackedVector.h>
#include "../d3dUtil.h"
//#include "../DXGraphicsUtilities.h"
#include"../DXDescriptorHeap.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")


struct TestData
{
	XMFLOAT4 v1;
};

DXPointCloudComputeShader_2::DXPointCloudComputeShader_2()
{
}

DXPointCloudComputeShader_2::~DXPointCloudComputeShader_2()
{

}

bool DXPointCloudComputeShader_2::Initialize(ComPtr<ID3D12Device>& device, std::shared_ptr<DXDescriptorHeap> descriptor_heap_srv, 
							UINT width, UINT height, const std::wstring& filename)
{
	DXComputeShader::Initialize(device, descriptor_heap_srv, filename);

	mWidth = width;
	mHeight = height;
	
	descriptor_heap_srv_ = descriptor_heap_srv;
	
	BuildResources(); //create buffers
	BuildDescriptors(); //srv and uavs for buffers

	return true;
}

void  DXPointCloudComputeShader_2::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	//CD3DX12_ROOT_PARAMETER slotRootParameter[3];
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	// Perfomance TIP: Order from most frequent to least frequent.
	//slotRootParameter[0].InitAsConstants(12, 0);
	slotRootParameter[0].InitAsDescriptorTable(1, &srvTable);
	slotRootParameter[1].InitAsDescriptorTable(1, &uavTable);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter,
		0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(m_device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}


void  DXPointCloudComputeShader_2::BuildPSOs()
{
	DXComputeShader::BuildPSOs();
}


void DXPointCloudComputeShader_2::DoComputeWork()
{
	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSOs[mShaderFilename].Get()));


	ID3D12DescriptorHeap* ppHeaps[] = { descriptor_heap_srv_->GetDescriptorHeap().Get() };
	mCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	mCommandList->SetComputeRootSignature(mRootSignature.Get());


	//cmdList->SetComputeRootDescriptorTable(1, mBlur0GpuSrv);
	//cmdList->SetComputeRootDescriptorTable(2, mBlur1GpuUav);

	mCommandList->SetComputeRootDescriptorTable(1, mBuff0GpuUav);

	//switch the texture to uav write
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBuffMap0.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));


	mCommandList->Dispatch(1, 1, 1);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBuffMap0.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait for the work to finish.
	FlushCommandQueue();	
}

void DXPointCloudComputeShader_2::BuildResources()
{
	// Note, compressed formats cannot be used for UAV.  We get error like:
	// ERROR: ID3D11Device::CreateTexture2D: The format (0x4d, BC3_UNORM) 
	// cannot be bound as an UnorderedAccessView, or cast to a format that
	// could be bound as an UnorderedAccessView.  Therefore this format 
	// does not support D3D11_BIND_UNORDERED_ACCESS.

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// Note: the call CD3DX12_RESOURCE_DESC::Buffer(byteSize) is sometimes used as the third parameter
	// to the CreateCommittedResource call since  D3DX12_RESOURCE_DESC::Buffer(byteSize) returns 
	// a CD3DX12_RESOURCE_DESC.  It creates a 1-D buffer and therefore is NOT compatable with 2D
	// buffers such as the one created below using texDesc since texDesc is 2D !!
	
	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc, //const D3D12_RESOURCE_DESC *pDesc
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mBuffMap0)));
		

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mBuffMap1)));
}

void DXPointCloudComputeShader_2::BuildDescriptors()
{
	mBuff0SrvIndex = descriptor_heap_srv_->GetNewDescriptorIndex();
	mBuff1SrvIndex = descriptor_heap_srv_->GetNewDescriptorIndex();
	mBuff0UavIndex = descriptor_heap_srv_->GetNewDescriptorIndex();
	mBuff1UavIndex = descriptor_heap_srv_->GetNewDescriptorIndex();

	//CPU
	mBuff0CpuSrv = descriptor_heap_srv_->GetCD3XD12CPUDescriptorHandle(mBuff0SrvIndex);
	mBuff1CpuSrv = descriptor_heap_srv_->GetCD3XD12CPUDescriptorHandle(mBuff1SrvIndex);

	mBuff0CpuUav = descriptor_heap_srv_->GetCD3XD12CPUDescriptorHandle(mBuff0UavIndex);
	mBuff1CpuUav = descriptor_heap_srv_->GetCD3XD12CPUDescriptorHandle(mBuff1UavIndex);

	//GPU
	mBuff0GpuSrv = descriptor_heap_srv_->GetCD3DX12GPUDescriptorHandle(mBuff0SrvIndex);
	mBuff0GpuUav = descriptor_heap_srv_->GetCD3DX12GPUDescriptorHandle(mBuff0UavIndex);

	mBuff1GpuSrv = descriptor_heap_srv_->GetCD3DX12GPUDescriptorHandle(mBuff1SrvIndex);
	mBuff1GpuUav = descriptor_heap_srv_->GetCD3DX12GPUDescriptorHandle(mBuff1UavIndex);


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	uavDesc.Format = mFormat;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	//Create descriptors  uav and srv.  We use the CPU handles for these function calls, but the GPU handles
	//will become valid after these calls.  Thus, we can use mBuff0GpuSrv as a root paramter to read from  texture 0
	//and mBuff0GpuUav as parameter to write to texture 0 in a compute shader.
	m_device->CreateShaderResourceView(mBuffMap0.Get(), &srvDesc, mBuff0CpuSrv);
	m_device->CreateUnorderedAccessView(mBuffMap0.Get(), nullptr, &uavDesc, mBuff0CpuUav);

	m_device->CreateShaderResourceView(mBuffMap1.Get(), &srvDesc, mBuff1CpuSrv);
	m_device->CreateUnorderedAccessView(mBuffMap1.Get(), nullptr, &uavDesc, mBuff1CpuUav);

}
