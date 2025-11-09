#include "stdafx.h"

#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif

#ifdef TINYOBJLOADER_IMPLEMENTATION
#undef TINYOBJLOADER_IMPLEMENTATION
#endif

#include "DXRShaderBindingTable.h"
#include "Utils.h"
#include "Structures.h"
#include "DXResourceUtilities.h"


DXRShaderBindingTable::DXRShaderBindingTable()
{
   
}

DXRShaderBindingTable::~DXRShaderBindingTable()
{
    
}

/**
* Create the DXR shader table.
*/

// The Shader Binding Table is where all programs and TLAS are bind together to know which program to execute.
// There is one RayGen, at least one Miss, followed by the Hit.There should be n entries for the Hit, 
// up to the maximum index passed to the instance description parameter InstanceContributionToHitGroupIndex.
// for ex, when creating the TLAS, this line gets called twice with:
// instanceDescs[i].InstanceContributionToHitGroupIndex = 0 and
// instanceDescs[i].InstanceContributionToHitGroupIndex = 1 thus, there needs to be 2 entries each specific closest hit 
// shader group in the shader table.

/*
The formula for calculating the address of a hit group record in the SBT is: 
HitGroupRecordAddress = start + stride * (rayContribution + (geometryMultiplier * geometryContribution) + instanceContribution)
start: The starting address of the hit group table in memory.
stride: The size of each hit group record in the table.
rayContribution: A contribution from the ray, defined in the TraceRay call.
geometryMultiplier: A multiplier for the geometry contribution, also defined in the TraceRay call.
geometryContribution: The index of the geometry in the Bottom-Level Acceleration Structure (BLAS).
instanceContribution: The instance contribution from the Top-Level Acceleration Structure (TLAS) instance. 
*/

void DXRShaderBindingTable::Create_Shader_Table(D3D12Global& d3d, DXRGlobal& dxr,
    D3D12Resources& resources)
{

	/*
	All shader records in the Shader Table must have the same size, so shader record size will be based on the
	largest required entry.The ray generation program requires the largest entry:
	sizeof(shader identifier) + 8 bytes for a descriptor table.
	The entry size must be aligned up to D3D12_RAYTRACING_SHADER_BINDING_TABLE_RECORD_BYTE_ALIGNMENT
	*/

	uint32_t numberOfRecords = 9;

	uint32_t shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	uint32_t sbtSize = 0;

	dxr.sbtEntrySize = shaderIdSize;
	dxr.sbtEntrySize += 8;	// CBV/SRV/UAV descriptor table 
	dxr.sbtEntrySize = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, dxr.sbtEntrySize);

	sbtSize = (dxr.sbtEntrySize * numberOfRecords);
	sbtSize = ALIGN(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, sbtSize);

	// Create the shader table buffers
	D3D12BufferCreateInfo bufferInfo(sbtSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	DXResourceUtilities::Create_Buffer(d3d, bufferInfo, &dxr.sbt);

	// Map the buffer
	uint8_t* pData;
	HRESULT hr = dxr.sbt->Map(0, nullptr, (void**)&pData);
	Utils::Validate(hr, L"Error: failed to map shader table!");


	// ---------------------------------Begin Primary Ray Generation Shader  -------------------
	// 
	// Ray Generation program and local root argument data (descriptor table with constant buffer and IB/VB pointers)
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"RayGen_12"), shaderIdSize);

	// Set the root arguments data. Point to start of descriptor heap
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();


	// ---------------------------------Begin Miss Shaders  -------------------
	// 
	// Miss program 0.  This is Miss Program for Primary Ray.
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"Miss_5"), shaderIdSize);

	// Set the local signature's arg data (ie miss shader's arg). The data is the 6th descriptor in descriptor heap. 
	// This is since the cube map texture is the 6th descriptor in the descriptor heap.  The descriptor holds
	// the srv for the cube texture.

	//Miss Program 0 data.  
	D3D12_GPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	handle.ptr += handleIncrement * 6; //get handle to the 6th descriptor ie the cubemap
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = handle; //set handle to the descriptor


	//Miss program 1 ie MissShadow (no data, uses empty signature).  This is Miss Program for Shadow Ray.
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"MissShadow_5"), shaderIdSize);



	// ---- -----------------------------Begin HitGroups -------------------
	// 
	// Each Model Instance has a Chs for ray 0 table entry and a Chs for ray 1 table entry
	// The first is for main color and the second is for the shadow hit ray
	// Currently 4 Unique Hit Groups.  We only use the Closest Hit Shaders, thus there are 4 unique closest
	// hit shaders that can be invoked when a ray is cast into the scene and hits triangle.
	// 
	// "HitGroup" has "ClosestHit" shader used for primary ray.  
	// "HitGroup2"  has "ClosestHitShadow" shader used for shadow ray.  
	// "HitGroup3"  has "ClosestHit_2" shader used for primary ray.  The sphere uses this hit group.
	// "HitGroup4"  has "ClosestHitReflected" shader used for reflected ray. 

	// ---------------------------- Instance 0 ---------------------------------------------
	// 
	// HitGroup 0 Chs and local root argument data (descriptor table).  Used by Primary Ray.
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"HitGroup"), shaderIdSize);

	// Set the root arg data. Point to start of descriptor heap
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();

	// HitGroup2 Chs ie shadow hit.   Used by Shadow Ray.
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"HitGroup2"), shaderIdSize);

	// HitGroup4  Reflect Ray and local root argument data (descriptor table).   Used by Reflected Ray.
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"HitGroup4"), shaderIdSize);

	// Set the root arg data. Point to start of descriptor heap
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();


	// ---------------------------- Instance 1---------------------------------------------

	// HitGroup 3 Chs and local root argument data (descriptor table).  Used by Primary Ray.
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"HitGroup3"), shaderIdSize);

	// Set the root arg data. Point to start of descriptor heap
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();

	// HitGroup2 Chs ie shadow hit.   Used by Shadow Ray.
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"HitGroup2"), shaderIdSize);

	// HitGroup4  Reflect Ray and local root argument data (descriptor table).   Used by Reflected Ray.
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"HitGroup4"), shaderIdSize);

	// Set the root arg data. Point to start of descriptor heap
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();


	// Unmap
	dxr.sbt->Unmap(0, nullptr);
}