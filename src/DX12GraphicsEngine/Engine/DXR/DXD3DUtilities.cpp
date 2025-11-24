#include "stdafx.h"
#include "DXD3DUtilities.h"

#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif

#ifdef TINYOBJLOADER_IMPLEMENTATION
#undef TINYOBJLOADER_IMPLEMENTATION
#endif

#include "Utils.h"


DXD3DUtilities::DXD3DUtilities()
{
}

DXD3DUtilities::~DXD3DUtilities()
{

}

/**
* Create the device.
*/
void DXD3DUtilities::Create_Device(D3D12Global& d3d)
{
#if defined(_DEBUG)
		// Enable the D3D12 debug layer.
		{
			ID3D12Debug* debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
			//	debugController->EnableDebugLayer(); //turned of since it breaks Nsight shader debugging
			}
		}
#endif

	// Create a DXGI Factory
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&d3d.factory));
	Utils::Validate(hr, L"Error: failed to create DXGI factory!");

	// Create the device
	d3d.adapter = nullptr;
	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != d3d.factory->EnumAdapters1(adapterIndex, &d3d.adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 adapterDesc;
		d3d.adapter->GetDesc1(&adapterDesc);

		if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;	// Don't select the Basic Render Driver adapter.
		}

		if (SUCCEEDED(D3D12CreateDevice(d3d.adapter, D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device5), (void**)&d3d.device)))
		{
			// Check if the device supports ray tracing.
			D3D12_FEATURE_DATA_D3D12_OPTIONS5 features = {};
			HRESULT hr = d3d.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features, sizeof(features));
			if (FAILED(hr) || features.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
			{
				SAFE_RELEASE(d3d.device);
				d3d.device = nullptr;
				continue;
			}

			printf("Running on DXGI Adapter %S\n", adapterDesc.Description);
			printf("Raytracing Tier = %d\n", features.RaytracingTier);
			break;
		}

		if (d3d.device == nullptr)
		{
			// Didn't find a device that supports ray tracing.
			Utils::Validate(E_FAIL, L"Error: failed to create ray tracing device!");
		}
	}
}

/**
* Create the command queue(s).
*/
void DXD3DUtilities::Create_Command_Queue(D3D12Global& d3d)
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	HRESULT hr = d3d.device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d.cmdQueue));
	Utils::Validate(hr, L"Error: failed to create command queue!");
}

/**
* Create the command allocator(s) for each frame.
*/
void DXD3DUtilities::Create_Command_Allocator(D3D12Global& d3d)
{
	// Create a command allocator for each frame
	for (UINT n = 0; n < 2; n++)
	{
		HRESULT hr = d3d.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d.cmdAlloc[n]));
		Utils::Validate(hr, L"Error: failed to create the command allocator!");
	}
}

/**
* Create the command list.
*/
void DXD3DUtilities::Create_CommandList(D3D12Global& d3d)
{
	// Create the command list
	HRESULT hr = d3d.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d.cmdAlloc[d3d.frameIndex], nullptr, IID_PPV_ARGS(&d3d.cmdList));
	hr = d3d.cmdList->Close();
	Utils::Validate(hr, L"Error: failed to create the command list!");
}

/**
* Create a fence.
*/
void DXD3DUtilities::Create_Fence(D3D12Global& d3d)
{
	HRESULT hr = d3d.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d.fence));
	Utils::Validate(hr, L"Error: failed to create fence!");
	d3d.fenceValues[d3d.frameIndex]++;

	// Create an event handle to use for frame synchronization
	d3d.fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
	if (d3d.fenceEvent == nullptr)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		Utils::Validate(hr, L"Error: failed to create fence event!");
	}
}

/**
* Create the swap chain.
*/
void DXD3DUtilities::Create_SwapChain(D3D12Global& d3d, HWND window)
{
	// Describe the swap chain
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.BufferCount = 2;
	desc.Width = d3d.width;
	desc.Height = d3d.height;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.SampleDesc.Count = 1;

	IDXGISwapChain1* swapChain;

	// It is recommended to always use the tearing flag when it is available.
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING ;

	// Create the swap chain
	HRESULT hr = d3d.factory->CreateSwapChainForHwnd(d3d.cmdQueue, window, &desc, nullptr, nullptr, &swapChain);
	Utils::Validate(hr, L"Error: failed to create swap chain!");

	// Associate the swap chain with a window
	hr = d3d.factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
	Utils::Validate(hr, L"Error: failed to make window association!");

	// Get the swap chain interface
	hr = swapChain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&d3d.swapChain));
	Utils::Validate(hr, L"Error: failed to cast swap chain!");

	SAFE_RELEASE(swapChain);
	d3d.frameIndex = d3d.swapChain->GetCurrentBackBufferIndex();
}

/**
* Create a root signature.
*/
ID3D12RootSignature* DXD3DUtilities::Create_Root_Signature(D3D12Global& d3d, const D3D12_ROOT_SIGNATURE_DESC& desc)
{
	HRESULT hr;
	ID3DBlob* sig;
	ID3DBlob* error;

	hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &error);
	Utils::Validate(hr, L"Error: failed to serialize root signature!");

	ID3D12RootSignature* pRootSig;
	hr = d3d.device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&pRootSig));
	Utils::Validate(hr, L"Error: failed to create root signature!");

	SAFE_RELEASE(sig);
	SAFE_RELEASE(error);
	return pRootSig;
}


/**
* Create a root signature.
*/
ID3D12RootSignature* DXD3DUtilities::Create_Root_Signature_With_Sampler(D3D12Global& d3d, 
	const D3D12_ROOT_SIGNATURE_DESC& desc, UINT numParameters, D3D12_ROOT_PARAMETER* rootParameters)
{
	// Create a sampler.
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;


	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(numParameters, rootParameters,
		1, &sampler,
		desc.Flags);


	// create a root signature with  slot which points to a descriptor range consisting of a table of textures
	// and a slot for a constant buffer 
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));


	ID3D12RootSignature* pRootSig;
	HRESULT hr = d3d.device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&pRootSig));
	Utils::Validate(hr, L"Error: failed to create root signature!");

	return pRootSig;
}

/**
* Reset the command list(s).
*/
void DXD3DUtilities::Reset_CommandList(D3D12Global& d3d)
{
	// Reset the command allocator for the current frame
	HRESULT hr = d3d.cmdAlloc[d3d.frameIndex]->Reset();
	Utils::Validate(hr, L"Error: failed to reset command allocator!");

	// Reset the command list for the current frame
	hr = d3d.cmdList->Reset(d3d.cmdAlloc[d3d.frameIndex], nullptr);
	Utils::Validate(hr, L"Error: failed to reset command list!");
}

/*
* Submit the command list.
*/
void DXD3DUtilities::Submit_CmdList(D3D12Global& d3d)
{
	d3d.cmdList->Close();

	ID3D12CommandList* pGraphicsList = { d3d.cmdList };
	d3d.cmdQueue->ExecuteCommandLists(1, &pGraphicsList);
	d3d.fenceValues[d3d.frameIndex]++;
	d3d.cmdQueue->Signal(d3d.fence, d3d.fenceValues[d3d.frameIndex]);
}

/**
 * Swap the buffers.
 */
void DXD3DUtilities::Present(D3D12Global& d3d)
{
	HRESULT hr = d3d.swapChain->Present(1, 0);
	if (FAILED(hr))
	{
		hr = d3d.device->GetDeviceRemovedReason();
		Utils::Validate(hr, L"Error: failed to present!");
	}
}

/*
* Wait for pending GPU work to complete.
*/
void DXD3DUtilities::WaitForGPU(D3D12Global& d3d)
{
	if (d3d.device == nullptr)
		return;

	// Schedule a signal command in the queue
	HRESULT hr = d3d.cmdQueue->Signal(d3d.fence, d3d.fenceValues[d3d.frameIndex]);
	Utils::Validate(hr, L"Error: failed to signal fence!");

	// Wait until the fence has been processed
	hr = d3d.fence->SetEventOnCompletion(d3d.fenceValues[d3d.frameIndex], d3d.fenceEvent);
	Utils::Validate(hr, L"Error: failed to set fence event!");

	WaitForSingleObjectEx(d3d.fenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame
	d3d.fenceValues[d3d.frameIndex]++;
}

/**
* Prepare to render the next frame.
*/
void DXD3DUtilities::MoveToNextFrame(D3D12Global& d3d)
{
	// Schedule a Signal command in the queue
	const UINT64 currentFenceValue = d3d.fenceValues[d3d.frameIndex];
	HRESULT hr = d3d.cmdQueue->Signal(d3d.fence, currentFenceValue);
	Utils::Validate(hr, L"Error: failed to signal command queue!");

	// Update the frame index
	d3d.frameIndex = d3d.swapChain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is
	if (d3d.fence->GetCompletedValue() < d3d.fenceValues[d3d.frameIndex])
	{
		hr = d3d.fence->SetEventOnCompletion(d3d.fenceValues[d3d.frameIndex], d3d.fenceEvent);
		Utils::Validate(hr, L"Error: failed to set fence value!");

		WaitForSingleObjectEx(d3d.fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame
	d3d.fenceValues[d3d.frameIndex] = currentFenceValue + 1;
}

/**
 * Release D3D12 resources.
 */
void DXD3DUtilities::Destroy(D3D12Global& d3d)
{
	SAFE_RELEASE(d3d.fence);
	SAFE_RELEASE(d3d.backBuffer[1]);
	SAFE_RELEASE(d3d.backBuffer[0]);
	SAFE_RELEASE(d3d.swapChain);
	SAFE_RELEASE(d3d.cmdAlloc[0]);
	SAFE_RELEASE(d3d.cmdAlloc[1]);
	SAFE_RELEASE(d3d.cmdQueue);
	SAFE_RELEASE(d3d.cmdList);
	SAFE_RELEASE(d3d.device);
	SAFE_RELEASE(d3d.adapter);
	SAFE_RELEASE(d3d.factory);
}

