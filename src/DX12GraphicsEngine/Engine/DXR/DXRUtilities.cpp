#include "stdafx.h"


#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif

#ifdef TINYOBJLOADER_IMPLEMENTATION
#undef TINYOBJLOADER_IMPLEMENTATION
#endif


#include "DXRUtilities.h"
#include "Utils.h"

#include "DXShaderUtilities.h"
#include "DXD3DUtilities.h"
#include "DXResourceUtilities.h"


DXRUtilities::DXRUtilities()
{
	m_pDXShaderUtilities = shared_ptr<DXShaderUtilities>(new DXShaderUtilities);
	m_pDXD3DUtilities = shared_ptr< DXD3DUtilities >(new DXD3DUtilities);
	m_pDXResourceUtilities = shared_ptr< DXResourceUtilities>(new DXResourceUtilities);
}

DXRUtilities::~DXRUtilities()
{
}

void DXRUtilities::Init(std::shared_ptr< DXShaderUtilities > pDXShaderUtilities,
	std::shared_ptr< DXD3DUtilities > pDXD3DUtilities,
	std::shared_ptr< DXResourceUtilities> pDXResourceUtilities)
{
	m_pDXShaderUtilities = pDXShaderUtilities;
	m_pDXD3DUtilities = pDXD3DUtilities;
	m_pDXResourceUtilities = pDXResourceUtilities;
}

//Load and create the DXR Ray Generation program and root signature.
void DXRUtilities::Create_RayGen_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler,
										const std::wstring& filename)
{
	// Load and compile the ray generation shader

	dxr.rgs = RtProgram(D3D12ShaderInfo(filename.c_str(), L"", L"lib_6_6"));

	//store compiled shader file in blob. and create final subobject D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY in dxr.rgs
	m_pDXShaderUtilities->Compile_Shader(shaderCompiler, dxr.rgs);

	// Describe the ray generation root signature
	D3D12_DESCRIPTOR_RANGE ranges[3];

	ranges[0].BaseShaderRegister = 0;
	ranges[0].NumDescriptors = 2;
	ranges[0].RegisterSpace = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;

	ranges[1].BaseShaderRegister = 0;
	ranges[1].NumDescriptors = 1;
	ranges[1].RegisterSpace = 0;
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[1].OffsetInDescriptorsFromTableStart = 2;

	ranges[2].BaseShaderRegister = 0;
	ranges[2].NumDescriptors = 8; //BillF
	ranges[2].RegisterSpace = 0;
	ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[2].OffsetInDescriptorsFromTableStart = 3;

	D3D12_ROOT_PARAMETER param0 = {};
	param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	param0.DescriptorTable.NumDescriptorRanges = _countof(ranges);
	param0.DescriptorTable.pDescriptorRanges = ranges;

	D3D12_ROOT_PARAMETER rootParams[1] = { param0 };

	// Create a sampler.
	static D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;


	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = _countof(rootParams);
	rootDesc.pParameters = rootParams;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

	rootDesc.NumStaticSamplers = 1;
	rootDesc.pStaticSamplers = &sampler;

	// Create the root signature
	dxr.rgs.pRootSignature = m_pDXD3DUtilities->Create_Root_Signature(d3d, rootDesc);

	//dxr.rgs.pRootSignature = m_pDXD3DUtilities->Create_Root_Signature_With_Sampler(d3d, rootDesc, 1, rootParams);
}


/**
* Load and create the DXR Miss program and root signature.
*/
void DXRUtilities::Create_Miss_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler,
	const std::wstring& filename)
{
	// Load and compile the miss shader
	dxr.miss = RtProgram(D3D12ShaderInfo(filename.c_str(), L"", L"lib_6_6"));
	m_pDXShaderUtilities->Compile_Shader(shaderCompiler, dxr.miss);


	/////////////////////////////////////////////////////////////////////////////////////////
		// Describe the ray miss root signature
	D3D12_DESCRIPTOR_RANGE ranges[1];

	ranges[0].BaseShaderRegister = 1; //t1 register for cubemap  
	ranges[0].NumDescriptors = 1;
	ranges[0].RegisterSpace = 0; //cube map uses register space "0"
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER param0 = {};
	param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	param0.DescriptorTable.NumDescriptorRanges = _countof(ranges);
	param0.DescriptorTable.pDescriptorRanges = ranges;

	D3D12_ROOT_PARAMETER rootParams[1] = { param0 };

	// Create a sampler.
	static D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;


	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = _countof(rootParams);
	rootDesc.pParameters = rootParams;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

	rootDesc.NumStaticSamplers = 1;
	rootDesc.pStaticSamplers = &sampler;

	// Create the root signature
	dxr.miss.pRootSignature = m_pDXD3DUtilities->Create_Root_Signature(d3d, rootDesc);

	/////////////////////////////////////////////////////////////////////////////////////
	// Create an empty root signature BillF TODO move this elsehere
	D3D12_ROOT_SIGNATURE_DESC rootDesc2 = {};
	rootDesc2.NumParameters = 0;
	rootDesc2.pParameters = 0;
	rootDesc2.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

	//rootDesc2.NumStaticSamplers = 1;
	rootDesc2.pStaticSamplers = &sampler;
	dxr.empty.pRootSignature = m_pDXD3DUtilities->Create_Root_Signature(d3d,{});
}

/**
* Load and create the DXR Closest Hit program and root signature.
*/

void DXRUtilities::Create_Closest_Hit_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler,
	const std::wstring& filename, uint32_t numMeshObjects)
{
	// Note: since all of our triangles are opaque, we will ignore the any hit program.

	//Creates a hit group subobject of type D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP.  And named group "Hit".
	// //The HitProgram type has two members of type RtProgram ie 	RtProgram ahs; RtProgram chs;
	//HitProgram constructor does not load any shader.  It does contain two RtProgram objects ahs and chs where each
	//can hold a subobject of type D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY.  In this example, we only load a
	//file for closest hit, chs, and do not load an any hit, ahs file.

	dxr.hit = HitProgram(L"Hit");

	//RtProgram creates subobject of type D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY.  
	// This corresponds to a compiled binary blob of the entire shader file.

	// Load and compile the Closest Hit shader and store the blob in subobject of type 
	// D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY .  We only load the closest hit file.  The any-hit remains unused.

	dxr.hit.chs = RtProgram(D3D12ShaderInfo(filename.c_str(), L"", L"lib_6_6"));
	m_pDXShaderUtilities->Compile_Shader(shaderCompiler, dxr.hit.chs);



	// Describe the root signature
	D3D12_DESCRIPTOR_RANGE ranges[7];

	ranges[0].BaseShaderRegister = 0;
	ranges[0].NumDescriptors = 2;
	ranges[0].RegisterSpace = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;

	ranges[1].BaseShaderRegister = 0;
	ranges[1].NumDescriptors = 1;
	ranges[1].RegisterSpace = 0;
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[1].OffsetInDescriptorsFromTableStart = 2;

	ranges[2].BaseShaderRegister = 0;
	ranges[2].NumDescriptors = 4; //tlas, albedo1,albedo2,cubemap
	ranges[2].RegisterSpace = 0;
	ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[2].OffsetInDescriptorsFromTableStart = 3;

	//1 cbv reg at b2
	ranges[3].BaseShaderRegister = 2;
	ranges[3].NumDescriptors = 1;
	ranges[3].RegisterSpace = 0;
	ranges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	ranges[3].OffsetInDescriptorsFromTableStart = 7;

	//1 cbv reg at b3
	ranges[4].BaseShaderRegister = 3;
	ranges[4].NumDescriptors = 1;
	ranges[4].RegisterSpace = 0;
	ranges[4].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	ranges[4].OffsetInDescriptorsFromTableStart = 8;

	//unbound index and vertex
	ranges[5].BaseShaderRegister = 0;
	ranges[5].NumDescriptors = UINT_MAX;
	ranges[5].RegisterSpace = 1;
	ranges[5].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[5].OffsetInDescriptorsFromTableStart = 9;

	//unbound texture buffers
	ranges[6].BaseShaderRegister = 0;
	ranges[6].NumDescriptors = UINT_MAX;
	ranges[6].RegisterSpace = 2;
	ranges[6].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[6].OffsetInDescriptorsFromTableStart = 9 + 2* numMeshObjects;


	D3D12_ROOT_PARAMETER param0 = {};
	param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	param0.DescriptorTable.NumDescriptorRanges = _countof(ranges);
	param0.DescriptorTable.pDescriptorRanges = ranges;

	D3D12_ROOT_PARAMETER rootParams[1] = { param0 };

	// Create a sampler.
	static D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;


	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = _countof(rootParams);
	rootDesc.pParameters = rootParams;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

	rootDesc.NumStaticSamplers = 1;
	rootDesc.pStaticSamplers = &sampler;

	// Create the root signature.  

	// If the final D3D12_ROOT_SIGNATURE_DESC is identical to the D3D12_ROOT_SIGNATURE_DESC used to create
	//the RayGen root signature, then dxr.hit.chs.pRootSignature = dxr.rgs.pRootSignature ie they use the
	// exact same signature object!
	dxr.hit.chs.pRootSignature = m_pDXD3DUtilities->Create_Root_Signature(d3d, rootDesc);

	//dxr.hit.chs.pRootSignature = m_pDXD3DUtilities->Create_Root_Signature_With_Sampler(d3d,rootDesc, 1, rootParams);
}

/**
* Create the DXR output buffer.
*/
void DXRUtilities::Create_DXR_Output_Buffer(D3D12Global& d3d, D3D12Resources& resources)
{
	// Describe the DXR output resource (texture)
	// Dimensions and format should match the swapchain
	// Initialize as a copy source, since we will copy this buffer's contents to the swapchain
	D3D12_RESOURCE_DESC desc = {};
	desc.DepthOrArraySize = 1;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	desc.Width = d3d.width;
	desc.Height = d3d.height;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	// Create the buffer resource (this is the output buffer that the ray tracer writes to)
	HRESULT hr = d3d.device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&resources.DXROutput));
	Utils::Validate(hr, L"Error: failed to create DXR output buffer!");
}


/**
* Builds the frame's DXR command list
*/
void DXRUtilities::RenderScene(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources)
{
	D3D12_RESOURCE_BARRIER OutputBarriers[2] = {};
	//D3D12_RESOURCE_BARRIER CounterBarriers[2] = {}; //BillF off since it is not used
	//D3D12_RESOURCE_BARRIER UAVBarriers[3] = {}; //BillF off since it is not used

	// Transition the back buffer to a copy destination
	//We will copy the DXR output buffer to the back buffer after all the rays have dispatched, thus we set it to copy_dest
	OutputBarriers[0].Transition.pResource = d3d.backBuffer[d3d.frameIndex];
	OutputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	OutputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	OutputBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;


	// We are transitioning the DXR ouput buffer to uav so we can write color data to it during the ray dispatch
	OutputBarriers[1].Transition.pResource = resources.DXROutput;
	OutputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	OutputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	OutputBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	// Wait for the transitions to complete
	d3d.cmdList->ResourceBarrier(2, OutputBarriers);

	// Set the UAV/SRV/CBV and sampler heaps
	ID3D12DescriptorHeap* ppHeaps[] = { resources.cbvSrvUavHeap, resources.samplerHeap };
	d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// Dispatch rays
	D3D12_DISPATCH_RAYS_DESC desc = {};
	desc.RayGenerationShaderRecord.StartAddress = dxr.sbt->GetGPUVirtualAddress();
	desc.RayGenerationShaderRecord.SizeInBytes = dxr.sbtEntrySize;

	desc.MissShaderTable.StartAddress = dxr.sbt->GetGPUVirtualAddress() + dxr.sbtEntrySize;
	desc.MissShaderTable.SizeInBytes = dxr.sbtEntrySize;		// Only a single Miss program entry
	desc.MissShaderTable.StrideInBytes = dxr.sbtEntrySize;

	desc.HitGroupTable.StartAddress = dxr.sbt->GetGPUVirtualAddress() + (dxr.sbtEntrySize * 2);
	desc.HitGroupTable.SizeInBytes = dxr.sbtEntrySize;			// Only a single Hit program entry
	desc.HitGroupTable.StrideInBytes = dxr.sbtEntrySize;

	desc.Width = d3d.width;
	desc.Height = d3d.height;
	desc.Depth = 1;

	d3d.cmdList->SetPipelineState1(dxr.rtpso);
	d3d.cmdList->DispatchRays(&desc);

	// Transition DXR output to a copy source
	//Set to copy source so that we can read data from DXR ouput buffer and copy the data into the back buffer
	OutputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	OutputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

	// Wait for the transitions to complete
	d3d.cmdList->ResourceBarrier(1, &OutputBarriers[1]);

	// Copy the DXR output to the back buffer
	d3d.cmdList->CopyResource(d3d.backBuffer[d3d.frameIndex], resources.DXROutput);

	// Transition back buffer to present 
	// back buffer set to "present" so it can be seen on screen
	OutputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	OutputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	// Wait for the transitions to complete
	d3d.cmdList->ResourceBarrier(1, &OutputBarriers[0]);

	// Submit the command list and wait for the GPU to idle
	m_pDXD3DUtilities->Submit_CmdList(d3d);
	m_pDXD3DUtilities->WaitForGPU(d3d);
}

/**
 * Release DXR resources.
 */
void DXRUtilities::Destroy(DXRGlobal& dxr)
{
	SAFE_RELEASE(dxr.TLAS.pScratch);
	SAFE_RELEASE(dxr.TLAS.pResult);
	SAFE_RELEASE(dxr.TLAS.pInstanceDesc);
	SAFE_RELEASE(dxr.BLAS.pScratch);
	SAFE_RELEASE(dxr.BLAS.pResult);
	SAFE_RELEASE(dxr.BLAS.pInstanceDesc);
	SAFE_RELEASE(dxr.sbt);
	SAFE_RELEASE(dxr.rgs.blob);
	SAFE_RELEASE(dxr.rgs.pRootSignature);
	SAFE_RELEASE(dxr.miss.blob);
	SAFE_RELEASE(dxr.miss.pRootSignature);
	SAFE_RELEASE(dxr.empty.pRootSignature);
	SAFE_RELEASE(dxr.hit.chs.blob);
	SAFE_RELEASE(dxr.hit.chs.pRootSignature);
	SAFE_RELEASE(dxr.rtpso);
	SAFE_RELEASE(dxr.rtpsoInfo);
}