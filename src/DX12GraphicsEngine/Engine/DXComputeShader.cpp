
#include "stdafx.h"
#include "DXComputeShader.h"
#include <DirectXPackedVector.h>
#include "d3dUtil.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

struct Data
{
	XMFLOAT3 v1;
	XMFLOAT2 v2;
};

DXComputeShader::DXComputeShader()
{

}

DXComputeShader::~DXComputeShader()
{

}

bool DXComputeShader::Initialize(ComPtr<ID3D12Device>& device, std::shared_ptr<DXDescriptorHeap> descriptor_heap_srv, const std::wstring& filename)
{
	m_device = device;
	mShaderFilename = filename;

	CreateCommandObjects();


	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&mFence)));

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	BuildBuffers();
	BuildRootSignature();
//	BuildDescriptorHeaps();
	BuildShadersAndInputLayout(filename);
//	BuildFrameResources();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void DXComputeShader::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

	ThrowIfFailed(m_device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));

	ThrowIfFailed(m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mDirectCmdListAlloc.Get(), // Associated command allocator
		nullptr,                   // Initial PipelineStateObject
		IID_PPV_ARGS(mCommandList.GetAddressOf())));

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	mCommandList->Close();
}

void  DXComputeShader::BuildBuffers()
{
	// Generate some data.
	std::vector<Data> dataA(NumDataElements);
	std::vector<Data> dataB(NumDataElements);
	for (int j = 0; j < NumDataElements; ++j)
	{
		float i = static_cast<float>(j);

		dataA[i].v1 = XMFLOAT3(i, i, i);
		dataA[i].v2 = XMFLOAT2(i, 0.0f);

		dataB[i].v1 = XMFLOAT3(-i, i, 0.0f);
		dataB[i].v2 = XMFLOAT2(0.0f, -i);
	}

	UINT64 byteSize = dataA.size() * sizeof(Data);

	// Create some buffers to be used as SRVs.  Specifically we create buffers to be read in t0 and t1 of the compute shader.
	//CreateDefaultBuffer creates two buffers.  one is temp for uploading andis accessible to CPU. The other is in default GPU heap and 
	//can't be accessed directly.  It requires a call to UpdateSubresources.
	mInputBufferA = d3dUtil::CreateDefaultBuffer(
		m_device.Get(),
		mCommandList.Get(),
		dataA.data(),//data will be copied into D3D12_HEAP_TYPE_DEFAULT by creating a temp cpu accessible D3D12_HEAP_TYPE_UPLOAD and copying it into default
		byteSize,
		mInputUploadBufferA); //ComPtr<ID3D12Resource> mInputUploadBufferA this is a buffer we will create 

	mInputBufferB = d3dUtil::CreateDefaultBuffer(
		m_device.Get(),
		mCommandList.Get(),
		dataB.data(),//data will be copied into D3D12_HEAP_TYPE_DEFAULT by creating a temp cpu accessible D3D12_HEAP_TYPE_UPLOAD and copying it into default
		byteSize,
		mInputUploadBufferB); //mInputUploadBufferB this is a buffer we will create 

	// Create the buffer that will be a UAV.
	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mOutputBuffer))); //mOutputBuffer a buffer we will create.  compute shader writes into this via UAV

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mReadBackBuffer))); //mReadBackBuffer a buffer we will create.

	//thus we wrote data into the mOutputBuffer uav in the compute shader.  To read it back on CPU, we need to copy the buffer into the mReadBackBuffer buffer.
	//mCommandList->CopyResource(mReadBackBuffer.Get(), mOutputBuffer.Get()); // called in DXComputeShader::DoComputeWork()
}

void  DXComputeShader::BuildRootSignature()
{
	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];

	// Perfomance TIP: Order from most frequent to least frequent.
	//Compute shader parameters that define the compute shader signature
	slotRootParameter[0].InitAsShaderResourceView(0); //we load a buffer directly into t0 via SetComputeRootShaderResourceView right before we dispatch
	slotRootParameter[1].InitAsShaderResourceView(1); //we load a buffer directly into t1 via SetComputeRootShaderResourceView right before we dispatch
	slotRootParameter[2].InitAsUnorderedAccessView(0); //we load a buffer directly into uo via  SetComputeRootUnorderedAccessView right before we dispatch

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
		0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_NONE);

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

void  DXComputeShader::BuildShadersAndInputLayout(const std::wstring& filename)
{
	mShaders[filename] = d3dUtil::CompileShader(filename, nullptr, "CS", "cs_5_0");
}

void  DXComputeShader::BuildPSOs()
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	computePsoDesc.pRootSignature = mRootSignature.Get();
	computePsoDesc.CS =
	{
		reinterpret_cast<BYTE*>(mShaders[mShaderFilename]->GetBufferPointer()),
		mShaders[mShaderFilename]->GetBufferSize()
	};
	computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	assert(mShaderFilename.empty() == false && "Must have a valid name for PSO hash map\n");

	ThrowIfFailed(m_device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs[mShaderFilename])));
}


void DXComputeShader::FlushCommandQueue()
{
	// Advance the fence value to mark commands up to this fence point.
	mCurrentFence++;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	// Wait until the GPU has completed commands up to this fence point.
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}


void DXComputeShader::DoComputeWork()
{
	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSOs[mShaderFilename].Get()));

	mCommandList->SetComputeRootSignature(mRootSignature.Get());

	//Setup the buffers used by the Compute shader.
	/*
		struct Data
		{
			float3 v1;
			float2 v2;
		};

		StructuredBuffer<Data> gInputA : register(t0);
		StructuredBuffer<Data> gInputB : register(t1);
		RWStructuredBuffer<Data> gOutput : register(u0);
	*/
	//note: we can not call SetComputeRootShaderResourceView on textures!  This only works for buffer rersources ie GPU heap resources.
	mCommandList->SetComputeRootShaderResourceView(0, mInputBufferA->GetGPUVirtualAddress()); //set buffer for t0
	mCommandList->SetComputeRootShaderResourceView(1, mInputBufferB->GetGPUVirtualAddress()); //set buffer for t1
	mCommandList->SetComputeRootUnorderedAccessView(2, mOutputBuffer->GetGPUVirtualAddress()); //set ouput buffer for u0

	mCommandList->Dispatch(1, 1, 1);

	// Schedule to copy the data to the default buffer to the readback buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));

	//We wrote data into the mOutputBuffer uav in the compute shader.  To read it back on CPU, we need to copy the buffer into the mReadBackBuffer buffer.
	mCommandList->CopyResource(mReadBackBuffer.Get(), mOutputBuffer.Get());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait for the work to finish.
	FlushCommandQueue();

	//debug write the contents of the buffer to file
	static bool bDebugWriteDataToDisk = true;

	if (bDebugWriteDataToDisk)
	{
		// Map the data so we can read it on CPU.  Mapping the buffer gives us pointer to the data.
		Data* mappedData = nullptr;
		D3D12_RANGE readbackBufferRange{ 0, sizeof(Data) };
		ThrowIfFailed(mReadBackBuffer->Map(0, &readbackBufferRange, reinterpret_cast<void**>(&mappedData)));

		std::ofstream fout("results.txt");

		for (int i = 0; i < NumDataElements; ++i)
		{

			fout << "(" << mappedData[i].v1.x << ", " << mappedData[i].v1.y << ", " << mappedData[i].v1.z <<
				", " << mappedData[i].v2.x << ", " << mappedData[i].v2.y << ")" << std::endl;
		}

		mReadBackBuffer->Unmap(0, nullptr);
	}
}


std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> DXComputeShader::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

