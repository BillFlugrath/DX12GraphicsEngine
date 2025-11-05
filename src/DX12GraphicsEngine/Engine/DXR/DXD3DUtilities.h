#pragma once

#include "Common.h"
#include "Structures.h"

class DXD3DUtilities
{
public:
	DXD3DUtilities();
	~DXD3DUtilities();

	void Create_Device(D3D12Global& d3d);
	void Create_Command_Queue(D3D12Global& d3d);
	void Create_Command_Allocator(D3D12Global& d3d);
	void Create_CommandList(D3D12Global& d3d);
	void Create_Fence(D3D12Global& d3d);
	void Create_SwapChain(D3D12Global& d3d, HWND window);

	void Reset_CommandList(D3D12Global& d3d);
	void Submit_CmdList(D3D12Global& d3d);
	void Present(D3D12Global& d3d);
	void WaitForGPU(D3D12Global& d3d);
	void MoveToNextFrame(D3D12Global& d3d);

	ID3D12RootSignature* Create_Root_Signature(D3D12Global& d3d, const D3D12_ROOT_SIGNATURE_DESC& desc);

	ID3D12RootSignature* Create_Root_Signature_With_Sampler(D3D12Global& d3d,
		const D3D12_ROOT_SIGNATURE_DESC& desc, UINT numParameters, D3D12_ROOT_PARAMETER* rootParameters);
	void Destroy(D3D12Global& d3d);
};
