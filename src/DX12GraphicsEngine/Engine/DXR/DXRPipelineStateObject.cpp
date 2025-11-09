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
// (eg. Miss and MissShadow). Note that the hit shaders are now only referred
// to as hit groups, meaning that the underlying intersection, any-hit and
// closest-hit shaders share the same root signature.

void DXRPipelineStateObject::Create_Pipeline_State_Object(D3D12Global& d3d, DXRGlobal& dxr)
{
	// Need 14 subobjects:
	// 1 for RGS program
	// 1 for Miss program
	// 1 for CHS program
	// 1 for Hit Group
	// 2 for RayGen Root Signature (root-signature and association)
	// 2 for Shader Config (config and association)
	// 1 for Global Root Signature 
	// 1 for Pipeline Config	
	// 1 for Miss local signature
	// 1 for Miss shader and its local assocation subobject
	UINT index = 0;
	vector<D3D12_STATE_SUBOBJECT> subobjects;
	subobjects.resize(18);
	
	//We create one DXIL subobject ie D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY for each shader type:
	//ie ray gen, closest hit, and miss.

	// --------------------- Begin Add state subobject for the RayGen shader DXIL------------------------------------
	// 
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

	subobjects[index++] = rgs;

	// --------------------- End Add state subobject for the RayGen shader ------------------------------------

		// -------------------------- Begin Add DXIL state subobject for theMiss shaders exported

	std::vector<std::wstring> missSymbolExport = { L"Miss" };
	std::vector<std::wstring> missSymbolNameExport = { L"Miss_5"};

	size_t missNumExports = missSymbolExport.size();

	std::vector< D3D12_EXPORT_DESC> missExportDesc; //create one D3D12_EXPORT_DESC for each function exported from lib
	missExportDesc.resize(missNumExports);

	for (size_t i = 0; i < missNumExports; ++i)
	{
		missExportDesc[i] = {};
		missExportDesc[i].Name = missSymbolNameExport[i].c_str();
		missExportDesc[i].ExportToRename = missSymbolExport[i].c_str();
		missExportDesc[i].Flags = D3D12_EXPORT_FLAG_NONE;
	}


	D3D12_DXIL_LIBRARY_DESC	missLibDesc = {};
	missLibDesc.DXILLibrary.BytecodeLength = dxr.miss.blob->GetBufferSize();
	missLibDesc.DXILLibrary.pShaderBytecode = dxr.miss.blob->GetBufferPointer();
	missLibDesc.NumExports = (uint32_t)missNumExports;
	missLibDesc.pExports = missExportDesc.data();

	D3D12_STATE_SUBOBJECT miss = {};
	miss.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	miss.pDesc = &missLibDesc;

	subobjects[index++] = miss; //index=2

	// -------------------------- End Add DXIL state subobject for the Miss shaders exported

	// -------------------------- Begin Add DXIL state subobject for the Closest Hit and shadow shaders exported------------

	std::vector<std::wstring> symbolExport = { L"ClosestHit",  L"ClosestHitShadow", L"ClosestHit_2", L"MissShadow", L"ClosestHitReflected"};
	std::vector<std::wstring> symbolNameExport = { L"ClosestHit_76", L"ClosestHitShadow_1",  L"ClosestHit_77", L"MissShadow_5",
												L"ClosestHitReflected_1"};

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
	chsLibDesc.NumExports = (uint32_t)numExports;
	chsLibDesc.pExports = chsExportDesc.data();

	D3D12_STATE_SUBOBJECT chs = {};
	chs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	chs.pDesc = &chsLibDesc;

	subobjects[index++] = chs; //index=2

	// -------------------------- End Add DXIL state subobject for the Closest Hit shaders exported

	// -------------------------- Begin Create state subobject for the Hit Groups exported ------------
	
	//Create a state subobject for each hit group
	std::vector<std::wstring> hitGroupNames = { L"HitGroup", L"HitGroup2", L"HitGroup3", L"HitGroup4" };
	std::vector<std::wstring> symbolNameExport2 = { L"ClosestHit_76", L"ClosestHitShadow_1",  L"ClosestHit_77", L"ClosestHitReflected_1" };
	size_t numHitGroups= hitGroupNames.size();

	std::vector< D3D12_HIT_GROUP_DESC> hitGroupDesc; //create one D3D12_EXPORT_DESC for each fHit Groupb
	hitGroupDesc.resize(numHitGroups);

	std::vector < D3D12_STATE_SUBOBJECT > hitGroup;
	hitGroup.resize(numHitGroups);

	for (size_t i = 0; i < numHitGroups; ++i)
	{
		// Add a state subobject for the hit group
		hitGroupDesc[i] = {};
		hitGroupDesc[i].ClosestHitShaderImport = symbolNameExport2[i].c_str();
		hitGroupDesc[i].HitGroupExport = hitGroupNames[i].c_str();

		hitGroup[i] = {};
		hitGroup[i].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		hitGroup[i].pDesc = &hitGroupDesc[i];

		subobjects[index++] = hitGroup[i];
	}

	// -------------------------- End Create state subobject for the Hit Groups exported ------------


	////////////////////     BEGIN ShaderConfig Payload 1 for rays  /////////////////////////
	
	// Add a state subobject for the shader payload configuration
	D3D12_RAYTRACING_SHADER_CONFIG shaderDesc = {};
	shaderDesc.MaxPayloadSizeInBytes = sizeof(XMFLOAT4);	// only need float4 for color or shadow payload
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
	//that will use the payload defined in shaderConfigObject.
	//Remember that we refer to hit shaders via the HitGroup name as shown below in shaderExports.  The raygen and
	//miss shaders use the actual shader name that was exported.

	const WCHAR* shaderExports[] = { L"RayGen_12", L"Miss_5", L"HitGroup",  L"HitGroup2",  L"MissShadow_5",  L"HitGroup3", L"HitGroup4"};

	// Add a state subobject for the association between shaders and the payload
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
	shaderPayloadAssociation.NumExports = _countof(shaderExports);
	shaderPayloadAssociation.pExports = shaderExports;
	shaderPayloadAssociation.pSubobjectToAssociate = &subobjects[(index - 1)]; //&subobjects[(index - 1)] is shaderconfig subobject

	D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = {};
	shaderPayloadAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	shaderPayloadAssociationObject.pDesc = &shaderPayloadAssociation;

	subobjects[index++] = shaderPayloadAssociationObject; //shader payload association subobject added to pipeline

	////////////////////     END ShaderConfig Payload 1 for rays /////////////////////////


	///////////////////////  BEGIN Create Empty gloal root signature associate it with shadow programs /////////////////////////
	D3D12_STATE_SUBOBJECT globalRootSig;
	globalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	globalRootSig.pDesc = &dxr.empty.pRootSignature;

	subobjects[index++] = globalRootSig;

	// Create a list of the shader export names that use the global signature
	//const WCHAR* globalSigExports[] = { L"MissShadow_5", L"HitGroup3"};
	const WCHAR* globalSigExports[] = { L"MissShadow_5", L"HitGroup2" };

	// Add a state subobject to describe association between the miss shader and its  root signature.
	// First, we create a D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION to describe the association.  Then use this
	// new D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION object to create the final D3D12_STATE_SUBOBJECT.

	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION globalShaderRootSigAssociation = {}; //this object is not stored in array.
	globalShaderRootSigAssociation.NumExports = _countof(globalSigExports);
	globalShaderRootSigAssociation.pExports = globalSigExports;
	globalShaderRootSigAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];//globalRootSig is at &subobjects[(index - 1)]

	D3D12_STATE_SUBOBJECT globalShaderRootSigAssociationObject = {}; //this is the subobject for the assocation
	globalShaderRootSigAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	globalShaderRootSigAssociationObject.pDesc = &globalShaderRootSigAssociation;

	subobjects[index++] = globalShaderRootSigAssociationObject; //store the subobject in the array
	
	///////////////////////  End Create Empty global root signature associate it with shadow programs ////////////////////


	////////////////////     Begin Root Sig for RayGen Shader Subobject  /////////////////////////
	// 
	// Add a state subobject for the shared root signature (ie root signature of the raygen shader).
	D3D12_STATE_SUBOBJECT rayGenRootSigObject = {};
	rayGenRootSigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	rayGenRootSigObject.pDesc = &dxr.rgs.pRootSignature;

	subobjects[index++] = rayGenRootSigObject;

	// Create a list of the shader export names that use the ray gen signature
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

	////////////////////  End Root Sig for RayGen Shader Subobject  /////////////////////////

	/////////////////// Begin Closest Hit Shader and Signature Subobject Creation /////////////////////

	// Add a state subobject for the closest hit ie chs root signature 
	D3D12_STATE_SUBOBJECT chsRootSigObject = {};
	chsRootSigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	chsRootSigObject.pDesc = &dxr.hit.chs.pRootSignature;

	subobjects[index++] = chsRootSigObject;


	// Create a list of the shader export names that use the chs signature
	//const WCHAR* chsSigExports[] = { L"HitGroup",  L"HitGroup2" };
	const WCHAR* chsSigExports[] = { L"HitGroup",  L"HitGroup3", L"HitGroup4" };

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
