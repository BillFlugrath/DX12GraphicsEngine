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

// The Shader Binding Table is where all programs and TLAS are bound together to know which program to execute.
// There is one RayGen, at least one Miss, followed by the Hit.There should be n entries for the Hit, 
// up to the maximum index passed to the instance description parameter InstanceContributionToHitGroupIndex.
// for ex, when creating the TLAS, consider two instances in the TLAS that have been assigned different values for
// InstanceContributionToHitGroupIndex:
// instanceDescs[0].InstanceContributionToHitGroupIndex = 0 and
// instanceDescs[1].InstanceContributionToHitGroupIndex = 1 thus, there needs to be 2 entries in the shader table 
// for the Higroup records.  One record for each of the 2 instances. 
// If the instances use the same Hitgroup ie "0":
// instanceDescs[0].InstanceContributionToHitGroupIndex = 0 and
// instanceDescs[1].InstanceContributionToHitGroupIndex = 0
// Then only a single record is used in the SBT.

/*
* See page 197 GPU Raytracing Gems 2 for the equation, parameters, and very good explaination.

The formula for calculating the address of a hit group record in the SBT is: 
HitGroupRecordAddress = Start + Stride * (InstanceContribution + RayContribution + (GeometryMultiplier * GeometryContribution)  )

start: The starting address of the hit group table in memory (not a parameter)
stride: The size of each hit group record in the table. (not a parameter)

InstanceContribution: The instance contribution from the TLAS instance ie InstanceContributionToHitGroupIndex
RayContribution: A contribution from the ray, defined in the TraceRay call.  The Ray Index.
GeometryMultiplier: A multiplier for the geometry contribution, also defined in the TraceRay call.  
GeometryContribution: The index of the geometry in the Bottom-Level Acceleration Structure (BLAS) ie Geometry Index (G-Id)


(note: The "Start" and "Stride" refer to SBT first Hitgroup address and SBT Record Stride in bytes. 
These are set when creating the SBT and are not parameters).

Notes: 
InstanceContributionToHitGroupIndex is also called the starting offset or Instance Offset.  Manually set on the TLAS instance.

RayContribution called "R-offset", typically referred to as the ray type.

The "GeometryMultiplier" typically referred to  an SBT stride to apply between each geometry’s hit group records
also called  "R-stride". The value is typically referred to as the number of ray types.

The GeometryContribution is the GeometryIndex() value, G-Id.  It is the geometry index inside a specific BLAS.  For ex, if a Model
has 2 vertex buffers, this means the BLAS corresponding to the model has two Geometries with index 0 and index 1.  If the model
only has 1 vb, then the only geometry index is index=0.  These are autoassigned as the BLAS is created.  Geometry ID.

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
	// "HitGroup"	has "ClosestHit" shader used for primary ray.  
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