#include "stdafx.h"


#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif

#ifdef TINYOBJLOADER_IMPLEMENTATION
#undef TINYOBJLOADER_IMPLEMENTATION
#endif


#include "DXRPipelineStateObject.h"
#include "Utils.h"

#include "DXShaderUtilities.h"
#include "DXD3DUtilities.h"
#include "DXResourceUtilities.h"


DXRPipelineStateObject::DXRPipelineStateObject()
{
	m_pDXShaderUtilities = shared_ptr<DXShaderUtilities>(new DXShaderUtilities);
	m_pDXD3DUtilities = shared_ptr< DXD3DUtilities >(new DXD3DUtilities);
	m_pDXResourceUtilities = shared_ptr< DXResourceUtilities>(new DXResourceUtilities);

	m_RecursionDepth = kMaxRecursionDepth;
}

DXRPipelineStateObject::~DXRPipelineStateObject()
{
}

void DXRPipelineStateObject::Init(std::shared_ptr< DXShaderUtilities > pDXShaderUtilities,
	std::shared_ptr< DXD3DUtilities > pDXD3DUtilities,
	std::shared_ptr< DXResourceUtilities> pDXResourceUtilities)
{
	m_pDXShaderUtilities = pDXShaderUtilities;
	m_pDXD3DUtilities = pDXD3DUtilities;
	m_pDXResourceUtilities = pDXResourceUtilities;
}

/**
* Create the DXR pipeline state object.
*/

//Many of the subobjects associate a shader name with a root signature.  Each root signature defines which
//shader registers it uses such as t0, t1, u0, b1 etc when the signature is created. We associate a shader(by its name)
//with a root signature by creating a subobject of type D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION.
//Thus, when a shader is invoked, it will have access to all of the correct shader registers.

//To be used, each shader needs to be associated to its root signature.A shaders imported from the DXIL
// libraries needs to be associated with exactly one root signature.The shaders comprising the hit groups
// need to share the same root signature, which is associated to the hit group(and not to the shaders themselves).
// Note that a shader does not have to actually access all the resources declared in its root signature, 
// as long as the root signature defines a superset of the resources the shader needs.

// The following section associates a root signature to each shader. Note
// that we can explicitly show that some shaders share the same root signature
// (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
// to as hit groups, meaning that the underlying intersection, any-hit and
// closest-hit shaders share the same root signature.

void DXRPipelineStateObject::Create_Pipeline_State_Object(D3D12Global& d3d, DXRGlobal& dxr)
{
	// Need 12 subobjects:
	// 1 for RGS program
	// 1 for Miss program
	// 1 for CHS program
	// 1 for Hit Group
	// 2 for RayGen Root Signature (root-signature and association)
	// 2 for Shader Config (config and association)
	// 1 for Global Root Signature BillF OFF.  replaced by local sig for miss shader and an association subobject
	// 1 for Pipeline Config	
	// 1 for Miss local signature
	// 1 for Miss shader and its local assocation subobject
	UINT index = 0;
	vector<D3D12_STATE_SUBOBJECT> subobjects;
	subobjects.resize(14);
	
	//We create one DXIL subobject ie D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY for each shader type:
	//ie ray gen, closest hit, and miss.

	// Add state subobject for the RGS binary blob.  We specify the specific function to export and what new
	//name if any we want to call it.
	D3D12_EXPORT_DESC rgsExportDesc = {};
	rgsExportDesc.Name = L"RayGen_12";
	rgsExportDesc.ExportToRename = L"RayGen";
	rgsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

	D3D12_DXIL_LIBRARY_DESC	rgsLibDesc = {};
	rgsLibDesc.DXILLibrary.BytecodeLength = dxr.rgs.blob->GetBufferSize();
	rgsLibDesc.DXILLibrary.pShaderBytecode = dxr.rgs.blob->GetBufferPointer();
	rgsLibDesc.NumExports = 1;
	rgsLibDesc.pExports = &rgsExportDesc; //specify what function or functions to export from the lib.

	D3D12_STATE_SUBOBJECT rgs = {};
	rgs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	rgs.pDesc = &rgsLibDesc;

	subobjects[index++] = rgs; //index=0

	// Add state subobject for the Miss shader
	D3D12_EXPORT_DESC msExportDesc = {};
	msExportDesc.Name = L"Miss_5";
	msExportDesc.ExportToRename = L"Miss";
	msExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

	D3D12_DXIL_LIBRARY_DESC	msLibDesc = {};
	msLibDesc.DXILLibrary.BytecodeLength = dxr.miss.blob->GetBufferSize();
	msLibDesc.DXILLibrary.pShaderBytecode = dxr.miss.blob->GetBufferPointer();
	msLibDesc.NumExports = 1;
	msLibDesc.pExports = &msExportDesc;

	D3D12_STATE_SUBOBJECT ms = {};
	ms.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	ms.pDesc = &msLibDesc;

	subobjects[index++] = ms; //index=1

	// Add DXIL state subobject for the Closest Hit shaders exported

	std::vector<std::wstring> symbolExport={ L"ClosestHit", L"ClosestHit_2"};
	std::vector<std::wstring> symbolNameExport = { L"ClosestHit_76", L"ClosestHit_77" };

	size_t numExports = symbolExport.size();

	std::vector< D3D12_EXPORT_DESC> chsExportDesc; //create one D3D12_EXPORT_DESC for each function exported from lib
	chsExportDesc.resize(numExports);

	for (size_t i = 0; i < numExports; ++i)
	{
		chsExportDesc[i] = {};
		chsExportDesc[i].Name = symbolNameExport[i].c_str();
		chsExportDesc[i].ExportToRename = symbolExport[i].c_str();
		chsExportDesc[i].Flags = D3D12_EXPORT_FLAG_NONE;
	}

	
	D3D12_DXIL_LIBRARY_DESC	chsLibDesc = {};
	chsLibDesc.DXILLibrary.BytecodeLength = dxr.hit.chs.blob->GetBufferSize();
	chsLibDesc.DXILLibrary.pShaderBytecode = dxr.hit.chs.blob->GetBufferPointer();
	chsLibDesc.NumExports = numExports;
	chsLibDesc.pExports = chsExportDesc.data();

	D3D12_STATE_SUBOBJECT chs = {};
	chs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	chs.pDesc = &chsLibDesc;

	subobjects[index++] = chs; //index=2

	//Create a state subobject for each hit group
	std::vector<std::wstring> hitGroupNames = { L"HitGroup", L"HitGroup2" };
	size_t numHitGroups= hitGroupNames.size();

	D3D12_HIT_GROUP_DESC hitGroupDesc[2];
	D3D12_STATE_SUBOBJECT hitGroup[2];

	for (size_t i = 0; i < numHitGroups; ++i)
	{
		// Add a state subobject for the hit group
		hitGroupDesc[i] = {};
		hitGroupDesc[i].ClosestHitShaderImport = symbolNameExport[i].c_str();
		hitGroupDesc[i].HitGroupExport = hitGroupNames[i].c_str();

		hitGroup[i] = {};
		hitGroup[i].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		hitGroup[i].pDesc = &hitGroupDesc[i];

		subobjects[index++] = hitGroup[i];
	}

	// Add a state subobject for the shader payload configuration
	D3D12_RAYTRACING_SHADER_CONFIG shaderDesc = {};
	shaderDesc.MaxPayloadSizeInBytes = sizeof(XMFLOAT4);	// only need float4 for color
	shaderDesc.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;

	D3D12_STATE_SUBOBJECT shaderConfigObject = {};
	shaderConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
	shaderConfigObject.pDesc = &shaderDesc;

	subobjects[index++] = shaderConfigObject;

	//payload is called shaderconfig and is a simple struct that passes data between shader. 
	// for ex, float4 ShadedColorAndHitT in the closesthit,raygen, and miss shaders.  It is used as 
	// struct HitInfo
	//{
	//	float4 ShadedColorAndHitT : SHADED_COLOR_AND_HIT_T;
	//};
	// HitInfo payload; payload.ShadedColorAndHitT = float4(1, 0, 0, 0);

	// Create a list of the shader export names that use the payload.  Thus, we create a subobject for the 
	//association between shader names that use a specific payload.  For ex, shaderExports specifies the shaders
	//that ill use the payload defined in shaderConfigObject.
	//Remember that we refer to hit shaders via the HitGroup name as shown below in shaderExports.  The raygen and
	//miss shaders use the actual shader name that was exported.
	const WCHAR* shaderExports[] = { L"RayGen_12", L"Miss_5", L"HitGroup",  L"HitGroup2" };

	// Add a state subobject for the association between shaders and the payload
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
	shaderPayloadAssociation.NumExports = _countof(shaderExports);
	shaderPayloadAssociation.pExports = shaderExports;
	shaderPayloadAssociation.pSubobjectToAssociate = &subobjects[(index - 1)]; //&subobjects[(index - 1)] is shaderconfig subobject

	D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = {};
	shaderPayloadAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	shaderPayloadAssociationObject.pDesc = &shaderPayloadAssociation;

	subobjects[index++] = shaderPayloadAssociationObject; //shader payload association subobject added to pipeline

	// Add a state subobject for the shared root signature (ie root signature of the raygen shader).
	D3D12_STATE_SUBOBJECT rayGenRootSigObject = {};
	rayGenRootSigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	rayGenRootSigObject.pDesc = &dxr.rgs.pRootSignature;

	subobjects[index++] = rayGenRootSigObject;

	// Create a list of the shader export names that use the root signature
	//const WCHAR* rootSigExports[] = { L"RayGen_12", L"HitGroup", L"Miss_5", L"HitGroup2" };
	const WCHAR* rootSigExports[] = { L"RayGen_12" };

	// Add a state subobject for the association between the RayGen shader and the local root signature
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rayGenShaderRootSigAssociation = {};
	rayGenShaderRootSigAssociation.NumExports = _countof(rootSigExports);
	rayGenShaderRootSigAssociation.pExports = rootSigExports;
	rayGenShaderRootSigAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];

	D3D12_STATE_SUBOBJECT rayGenShaderRootSigAssociationObject = {};
	rayGenShaderRootSigAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	rayGenShaderRootSigAssociationObject.pDesc = &rayGenShaderRootSigAssociation;

	subobjects[index++] = rayGenShaderRootSigAssociationObject;

	/////////////////// Begin Closest Hit Shader and Signature Subobject Creation /////////////////////

	// Add a state subobject for the miss root signature 
	D3D12_STATE_SUBOBJECT chsRootSigObject = {};
	chsRootSigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	chsRootSigObject.pDesc = &dxr.hit.chs.pRootSignature;

	subobjects[index++] = chsRootSigObject;


	// Create a list of the shader export names that use the chs signature
	const WCHAR* chsSigExports[] = { L"HitGroup",  L"HitGroup2" };

	// Add a state subobject to describe association between the chs shader and its local root signature.
	// First, we create a D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION to describe the association.  Then use this
	// new D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION object to create the final D3D12_STATE_SUBOBJECT.

	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION chsShaderRootSigAssociation = {}; //this object is not stored in array.
	chsShaderRootSigAssociation.NumExports = _countof(chsSigExports);
	chsShaderRootSigAssociation.pExports = chsSigExports;
	chsShaderRootSigAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];//chsRootSigObject is at &subobjects[(index - 1)]

	D3D12_STATE_SUBOBJECT chsShaderRootSigAssociationObject = {}; //this is the subobject for the assocation
	chsShaderRootSigAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	chsShaderRootSigAssociationObject.pDesc = &chsShaderRootSigAssociation;

	subobjects[index++] = chsShaderRootSigAssociationObject; //store the subobject in the array

	///////////////////////////////  End  Closest Hit Shader and Signature Subobject Creation /////////////////////

	/////////////////// Begin Miss Shader and Signature Subobject Creation /////////////////////
	
	// Add a state subobject for the miss root signature 
	D3D12_STATE_SUBOBJECT missRootSigObject = {};
	missRootSigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	missRootSigObject.pDesc = &dxr.miss.pRootSignature;

	subobjects[index++] = missRootSigObject;


	// Create a list of the shader export names that use the miss signature
	const WCHAR* missSigExports[] = {  L"Miss_5", };

	// Add a state subobject to describe association between the miss shader and its local root signature.
	// First, we create a D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION to describe the association.  Then use this
	// new D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION object to create the final D3D12_STATE_SUBOBJECT.

	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION missShaderRootSigAssociation = {}; //this object is not stored in array.
	missShaderRootSigAssociation.NumExports = _countof(missSigExports);
	missShaderRootSigAssociation.pExports = missSigExports;
	missShaderRootSigAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];//missRootSigObject is at &subobjects[(index - 1)]

	D3D12_STATE_SUBOBJECT missShaderRootSigAssociationObject = {}; //this is the subobject for the assocation
	missShaderRootSigAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	missShaderRootSigAssociationObject.pDesc = &missShaderRootSigAssociation;

	subobjects[index++] = missShaderRootSigAssociationObject; //store the subobject in the array

	///////////////////////////////  End Miss Shader and Signature Subobject Creation /////////////////////
	// 
	// The code below is deprecated.  It was used when the miss shader did not reference any arguments.
	/**
	D3D12_STATE_SUBOBJECT globalRootSig;
	globalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	globalRootSig.pDesc = &dxr.miss.pRootSignature;

	subobjects[index++] = globalRootSig;
	*/

	// Add a state subobject for the ray tracing pipeline config
	D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
	pipelineConfig.MaxTraceRecursionDepth = m_RecursionDepth; //Set Max Recursion Depth.  

	D3D12_STATE_SUBOBJECT pipelineConfigObject = {};
	pipelineConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
	pipelineConfigObject.pDesc = &pipelineConfig;

	subobjects[index++] = pipelineConfigObject;

	// Describe the Ray Tracing Pipeline State Object
	D3D12_STATE_OBJECT_DESC pipelineDesc = {};
	pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	pipelineDesc.NumSubobjects = static_cast<UINT>(subobjects.size());
	pipelineDesc.pSubobjects = subobjects.data();

	// Create the RT Pipeline State Object (RTPSO)
	HRESULT hr = d3d.device->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&dxr.rtpso));
	Utils::Validate(hr, L"Error: failed to create state object!");

	// Get the RTPSO properties
	hr = dxr.rtpso->QueryInterface(IID_PPV_ARGS(&dxr.rtpsoInfo));
	Utils::Validate(hr, L"Error: failed to get RTPSO info object!");
}

/**
* Create the DXR shader table.
*/

// The Shader Binding Table is where all programs and TLAS are bind together to know which program to execute.
// There is one RayGen, at least one Miss, followed by the Hit.There should be n entries for the Hit, 
// up to the maximum index passed to the instance description parameter InstanceContributionToHitGroupIndex.
// for ex, when creating the TLAS, this line gets called twice with:
// instanceDescs[i].InstanceContributionToHitGroupIndex = 0 and
// instanceDescs[i].InstanceContributionToHitGroupIndex = 1 thus, there needs to be 2 entries for the closest hit 
// shader in the shader table.

void DXRPipelineStateObject::Create_Shader_Table(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources)
{
	/*
	The Shader Table layout is as follows:
	Entry 0 - Ray Generation shader
	Entry 1 - Miss shader
	Entry 2 - Closest Hit shader 0
	Entry 3 - Closest Hit shader 1
	All shader records in the Shader Table must have the same size, so shader record size will be based on the 
	largest required entry.The ray generation program requires the largest entry:
	sizeof(shader identifier) + 8 bytes for a descriptor table.
	The entry size must be aligned up to D3D12_RAYTRACING_SHADER_BINDING_TABLE_RECORD_BYTE_ALIGNMENT
	*/

	uint32_t numberOfRecords = 4;

	uint32_t shaderIdSize = 32;
	uint32_t sbtSize = 0;

	dxr.sbtEntrySize = shaderIdSize;
//	dxr.sbtEntrySize += 8;					// CBV/SRV/UAV descriptor table BillF off since we added many more descriptors
	dxr.sbtEntrySize += 11;					// CBV/SRV/UAV descriptor table  BillF updated the entry size
	dxr.sbtEntrySize = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, dxr.sbtEntrySize);

	sbtSize = (dxr.sbtEntrySize * numberOfRecords);		// 4 shader records in the table
	sbtSize = ALIGN(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, sbtSize);

	// Create the shader table buffers
	D3D12BufferCreateInfo bufferInfo(sbtSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	m_pDXResourceUtilities->Create_Buffer(d3d, bufferInfo, &dxr.sbt);

	// Map the buffer
	uint8_t* pData;
	HRESULT hr = dxr.sbt->Map(0, nullptr, (void**)&pData);
	Utils::Validate(hr, L"Error: failed to map shader table!");

	// Entry 0 - Ray Generation program and local root argument data (descriptor table with constant buffer and IB/VB pointers)
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"RayGen_12"), shaderIdSize);

	// Set the root arguments data. Point to start of descriptor heap
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();

	// Entry 1 - Miss program 
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"Miss_5"), shaderIdSize);

	// Set the local signature's arg data (ie miss shader's arg). The data is the 10th descriptor in descriptor heap. 
	// This is since the cube map texture is the 10th descriptor in the descriptor heap.  The descriptor holds
	// the srv for the cube texture.

	D3D12_GPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	handle.ptr += handleIncrement*10; //get handle to the 10th descriptor

	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = handle; //set handle to the descriptor


	// Entry 2 - Closest Hit program 0 and local root argument data (descriptor table with constant buffer and IB/VB pointers)
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"HitGroup"), shaderIdSize);

	// Set the root arg data. Point to start of descriptor heap
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();

	// Entry 3 - Closest Hit program 1 and local root argument data (descriptor table with constant buffer and IB/VB pointers)
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"HitGroup2"), shaderIdSize);

	// Set the root arg data. Point to start of descriptor heap
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();

	// Unmap
	dxr.sbt->Unmap(0, nullptr);
}

void DXRPipelineStateObject::Create_Shader_Table_Unbound_Resources(D3D12Global& d3d, DXRGlobal& dxr,
	D3D12Resources& resources)
{
	/*
	The Shader Table layout is as follows:
	Entry 0 - Ray Generation shader
	Entry 1 - Miss shader
	Entry 2 - Closest Hit shader 0
	Entry 3 - Closest Hit shader 1
	All shader records in the Shader Table must have the same size, so shader record size will be based on the
	largest required entry.The ray generation program requires the largest entry:
	sizeof(shader identifier) + 8 bytes for a descriptor table.
	The entry size must be aligned up to D3D12_RAYTRACING_SHADER_BINDING_TABLE_RECORD_BYTE_ALIGNMENT
	*/

	uint32_t numberOfRecords = 4;

	uint32_t shaderIdSize = 32;
	uint32_t sbtSize = 0;

	dxr.sbtEntrySize = shaderIdSize;
	//	dxr.sbtEntrySize += 8;					// CBV/SRV/UAV descriptor table BillF off since we added many more descriptors
	dxr.sbtEntrySize += 11;					// CBV/SRV/UAV descriptor table  BillF updated the entry size
	dxr.sbtEntrySize = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, dxr.sbtEntrySize);

	sbtSize = (dxr.sbtEntrySize * numberOfRecords);		// 4 shader records in the table
	sbtSize = ALIGN(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, sbtSize);

	// Create the shader table buffers
	D3D12BufferCreateInfo bufferInfo(sbtSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	m_pDXResourceUtilities->Create_Buffer(d3d, bufferInfo, &dxr.sbt);

	// Map the buffer
	uint8_t* pData;
	HRESULT hr = dxr.sbt->Map(0, nullptr, (void**)&pData);
	Utils::Validate(hr, L"Error: failed to map shader table!");

	// Entry 0 - Ray Generation program and local root argument data (descriptor table with constant buffer and IB/VB pointers)
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"RayGen_12"), shaderIdSize);

	// Set the root arguments data. Point to start of descriptor heap
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();

	// Entry 1 - Miss program 
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"Miss_5"), shaderIdSize);

	// Set the local signature's arg data (ie miss shader's arg). The data is the 10th descriptor in descriptor heap. 
	// This is since the cube map texture is the 10th descriptor in the descriptor heap.  The descriptor holds
	// the srv for the cube texture.

	D3D12_GPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	handle.ptr += handleIncrement * 6; //get handle to the 6th descriptor ie the cubemap

	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = handle; //set handle to the descriptor


	// Entry 2 - Closest Hit program 0 and local root argument data (descriptor table with constant buffer and IB/VB pointers)
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"HitGroup"), shaderIdSize);

	// Set the root arg data. Point to start of descriptor heap
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();

	// Entry 3 - Closest Hit program 1 and local root argument data (descriptor table with constant buffer and IB/VB pointers)
	pData += dxr.sbtEntrySize;
	memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"HitGroup2"), shaderIdSize);

	// Set the root arg data. Point to start of descriptor heap
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();

	// Unmap
	dxr.sbt->Unmap(0, nullptr);
}

/**
* Create the DXR descriptor heap for CBVs, SRVs, and the output UAV.
*/
void DXRPipelineStateObject::Create_CBVSRVUAV_Heap(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources,
	D3DSceneModels& d3dSceneModels, std::vector<D3DTexture> textures)
{
	D3DModel model_0 = d3dSceneModels.GetModelObjects()[0];
	D3DMesh mesh_0 = model_0.GetMeshObjects()[0];

	D3DModel model_1 = d3dSceneModels.GetModelObjects()[1];
	D3DMesh mesh_1 = model_1.GetMeshObjects()[0];

	// Describe the CBV/SRV/UAV heap
	// Need 11 entries:
	// 1 CBV for the ViewCB
	// 1 CBV for the MaterialCB
	// 1 UAV for the RT output
	// 1 SRV for the Scene BVH
	// 1 SRV for the index buffer 0
	// 1 SRV for the vertex buffer 0
	// 1 SRV for the texture
	// 1 SRV for the index buffer 1
	// 1 SRV for the vertex buffer 1
	// 1 SRV for the texture 1
	// 1 SRV for the cube texture

	UINT numDescriptors = m_NumDescriptors_CBVSRVUAV_Heap; // 11;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	// Create the descriptor heap
	HRESULT hr = d3d.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&resources.cbvSrvUavHeap));
	Utils::Validate(hr, L"Error: failed to create DXR CBV/SRV/UAV descriptor heap!");

	// Get the descriptor heap handle and increment size
	D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Create the ViewCB CBV (ie create a descriptor that points to data at "BufferLocation"
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(resources.viewCBData));
	cbvDesc.BufferLocation = resources.viewCB->GetGPUVirtualAddress();

	//create the descriptor in the heap location "handle".  dont confuse handle (ie location of descriptor) with actual data of the buffer
	//stored at "cbvDesc.BufferLocation"
	d3d.device->CreateConstantBufferView(&cbvDesc, handle);

	// Create the MaterialCB CBV
	cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(resources.materialCBData));
	cbvDesc.BufferLocation = resources.materialCB->GetGPUVirtualAddress();

	handle.ptr += handleIncrement;
	d3d.device->CreateConstantBufferView(&cbvDesc, handle);

	// Create the DXR output buffer UAV
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	handle.ptr += handleIncrement;
	d3d.device->CreateUnorderedAccessView(resources.DXROutput, nullptr, &uavDesc, handle);

	// Create the DXR Top Level Acceleration Structure SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = dxr.TLAS.pResult->GetGPUVirtualAddress();

	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(nullptr, &srvDesc, handle);

	// Create the index buffer SRV
	// We need an an srv since we access the vertices in the shader ie 
	// ByteAddressBuffer indices					: register(t1);
	D3D12_SHADER_RESOURCE_VIEW_DESC indexSRVDesc;
	indexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	indexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	indexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	indexSRVDesc.Buffer.StructureByteStride = 0;
	indexSRVDesc.Buffer.FirstElement = 0;
	indexSRVDesc.Buffer.NumElements = (static_cast<UINT>(mesh_0.indices.size()) * sizeof(UINT)) / sizeof(float);
	indexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(mesh_0.m_pIndexBuffer.Get(), &indexSRVDesc, handle);


	// Create the vertex buffer SRV
	// We need an an srv since we access the vertices in the shader ie 
	// ByteAddressBuffer vertices					: register(t2);

	D3D12_SHADER_RESOURCE_VIEW_DESC vertexSRVDesc;
	vertexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	vertexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	vertexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	vertexSRVDesc.Buffer.StructureByteStride = 0;
	vertexSRVDesc.Buffer.FirstElement = 0;
	vertexSRVDesc.Buffer.NumElements = (static_cast<UINT>(mesh_0.vertices.size()) * sizeof(ModelVertex)) / sizeof(float);
	vertexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(mesh_0.m_pVertexBuffer.Get(), &vertexSRVDesc, handle);


	// Create the material texture SRV : register(t3);
	D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc = {};
	textureSRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	textureSRVDesc.Texture2D.MipLevels = 1;
	textureSRVDesc.Texture2D.MostDetailedMip = 0;
	textureSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(textures[0].m_pTextureResource.Get(), &textureSRVDesc, handle);

	//Second model indices : register(t4);
	D3D12_SHADER_RESOURCE_VIEW_DESC indexSRVDesc2;
	indexSRVDesc2.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	indexSRVDesc2.Format = DXGI_FORMAT_R32_TYPELESS;
	indexSRVDesc2.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	indexSRVDesc2.Buffer.StructureByteStride = 0;
	indexSRVDesc2.Buffer.FirstElement = 0;
	indexSRVDesc2.Buffer.NumElements = (static_cast<UINT>(mesh_1.indices.size()) * sizeof(UINT)) / sizeof(float);
	indexSRVDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(mesh_1.m_pIndexBuffer.Get(), &indexSRVDesc2, handle);

	//Second model vertices : register(t5);
	D3D12_SHADER_RESOURCE_VIEW_DESC vertexSRVDesc2;
	vertexSRVDesc2.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	vertexSRVDesc2.Format = DXGI_FORMAT_R32_TYPELESS;
	vertexSRVDesc2.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	vertexSRVDesc2.Buffer.StructureByteStride = 0;
	vertexSRVDesc2.Buffer.FirstElement = 0;
	vertexSRVDesc2.Buffer.NumElements = (static_cast<UINT>(mesh_1.vertices.size()) * sizeof(ModelVertex)) / sizeof(float);
	vertexSRVDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(mesh_1.m_pVertexBuffer.Get(), &vertexSRVDesc2, handle);


	// Create the material texture_1 SRV : register(t6);
	D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc2 = {};
	textureSRVDesc2.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureSRVDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	textureSRVDesc2.Texture2D.MipLevels = 1;
	textureSRVDesc2.Texture2D.MostDetailedMip = 0;
	textureSRVDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(textures[1].m_pTextureResource.Get(), &textureSRVDesc2, handle);

	// Create the cube map 0 texture SRV : register(t7);
	D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc3 = {};
	textureSRVDesc3.Format = textures[2].m_pTextureResource.Get()->GetDesc().Format;
	textureSRVDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	textureSRVDesc3.Texture2D.MipLevels = textures[2].m_pTextureResource.Get()->GetDesc().MipLevels;
	textureSRVDesc3.Texture2D.MostDetailedMip = 0;
	textureSRVDesc3.Texture2D.ResourceMinLODClamp = 0.0f;
	textureSRVDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;


	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(textures[2].m_pTextureResource.Get(), &textureSRVDesc3, handle);
}

void DXRPipelineStateObject::Create_CBVSRVUAV_Heap_Unbound_Resources(D3D12Global& d3d, DXRGlobal& dxr, 
	D3D12Resources& resources,D3DSceneModels& d3dSceneModels, std::vector<D3DTexture> textures)
{
	// Describe the CBV/SRV/UAV heap
	// Need 11 entries:
	// 1 CBV for the ViewCB
	// 1 CBV for the MaterialCB
	// 1 UAV for the RT output
	// 1 SRV for the Scene BVH
	// 1 SRV for the texture
	// 1 SRV for the texture 1
	// 1 SRV for the cube texture

	UINT numDescriptors = m_NumDescriptors_CBVSRVUAV_Heap; 

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	// Create the descriptor heap
	HRESULT hr = d3d.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&resources.cbvSrvUavHeap));
	Utils::Validate(hr, L"Error: failed to create DXR CBV/SRV/UAV descriptor heap!");

	// Get the descriptor heap handle and increment size
	D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Create the ViewCB CBV (ie create a descriptor that points to data at "BufferLocation"
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(resources.viewCBData));
	cbvDesc.BufferLocation = resources.viewCB->GetGPUVirtualAddress();

	//create the descriptor in the heap location "handle".  dont confuse handle (ie location of descriptor) with actual data of the buffer
	//stored at "cbvDesc.BufferLocation"
	d3d.device->CreateConstantBufferView(&cbvDesc, handle);

	// Create the MaterialCB CBV
	cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(resources.materialCBData));
	cbvDesc.BufferLocation = resources.materialCB->GetGPUVirtualAddress();

	handle.ptr += handleIncrement;
	d3d.device->CreateConstantBufferView(&cbvDesc, handle);

	// Create the DXR output buffer UAV
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	handle.ptr += handleIncrement;
	d3d.device->CreateUnorderedAccessView(resources.DXROutput, nullptr, &uavDesc, handle);

	// Create the DXR Top Level Acceleration Structure SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = dxr.TLAS.pResult->GetGPUVirtualAddress();

	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(nullptr, &srvDesc, handle);

	// Create the material texture SRV : register(t3);
	D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc = {};
	textureSRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	textureSRVDesc.Texture2D.MipLevels = 1;
	textureSRVDesc.Texture2D.MostDetailedMip = 0;
	textureSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(textures[0].m_pTextureResource.Get(), &textureSRVDesc, handle);


	// Create the material texture_1 SRV : register(t6);
	D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc2 = {};
	textureSRVDesc2.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureSRVDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	textureSRVDesc2.Texture2D.MipLevels = 1;
	textureSRVDesc2.Texture2D.MostDetailedMip = 0;
	textureSRVDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(textures[1].m_pTextureResource.Get(), &textureSRVDesc2, handle);

	// Create the cube map 0 texture SRV : register(t7);
	D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc3 = {};
	textureSRVDesc3.Format = textures[2].m_pTextureResource.Get()->GetDesc().Format;
	textureSRVDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	textureSRVDesc3.Texture2D.MipLevels = textures[2].m_pTextureResource.Get()->GetDesc().MipLevels;
	textureSRVDesc3.Texture2D.MostDetailedMip = 0;
	textureSRVDesc3.Texture2D.ResourceMinLODClamp = 0.0f;
	textureSRVDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;


	handle.ptr += handleIncrement;
	d3d.device->CreateShaderResourceView(textures[2].m_pTextureResource.Get(), &textureSRVDesc3, handle);
}
/**
 * Release DXR resources.
 */
void DXRPipelineStateObject::Destroy(DXRGlobal& dxr)
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
	SAFE_RELEASE(dxr.hit.chs.blob);
	SAFE_RELEASE(dxr.hit.chs.pRootSignature);
	SAFE_RELEASE(dxr.rtpso);
	SAFE_RELEASE(dxr.rtpsoInfo);
}