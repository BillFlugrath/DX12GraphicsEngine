
#pragma once

#include "Common.h"
#include "DXRVertices.h"

static const D3D12_HEAP_PROPERTIES UploadHeapProperties =
{
	D3D12_HEAP_TYPE_UPLOAD,
	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	D3D12_MEMORY_POOL_UNKNOWN,
	0, 0
};

static const D3D12_HEAP_PROPERTIES DefaultHeapProperties =
{
	D3D12_HEAP_TYPE_DEFAULT,
	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	D3D12_MEMORY_POOL_UNKNOWN,
	0, 0
};



//--------------------------------------------------------------------------------------
// Global Structures
//--------------------------------------------------------------------------------------

struct ConfigInfo {
	int			width;
	int			height;
	string		model;
	HINSTANCE	instance;

	ConfigInfo() {
		width = 640;
		height = 360;
		model = "";
		instance = NULL;
	}
};


struct Material {
	string name;
	string texturePath;
	float  textureResolution;

	Material() {
		name = "defaultMaterial";
		texturePath = "";
		textureResolution = 512;
	}
};

struct Model
{
	vector<ModelVertex>									vertices;
	vector<uint32_t>								indices;
}; 

//TODO make this a template that takes T=vertex type
struct D3DMesh 
{
	vector<ModelVertex>	vertices;
	vector<uint32_t>	indices;
	ComPtr< ID3D12Resource > m_pVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	ComPtr< ID3D12Resource > m_pIndexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
	uint32_t m_DiffuseTexIndex = 0;
};

struct D3DModel
{
	D3DModel()
	{
		m_WorldMatrix = XMMatrixIdentity();
	}

	void SetPosition(float x, float y, float z)
	{
		::XMStoreFloat4x4(&m_World4x4, m_WorldMatrix);
		m_World4x4(3, 0) = x; m_World4x4(3, 1) = y; m_World4x4(3, 2) = z;
		m_WorldMatrix = XMLoadFloat4x4(&m_World4x4);
	}

	void AddMesh(D3DMesh& m) { m_vMeshObjects.push_back(m); }
	std::vector<D3DMesh>& GetMeshObjects() { return m_vMeshObjects;}
	void SetInstanceContributionToHitGroupIndex(UINT hg) { m_HitGroupIndex = hg; }
	UINT GetInstanceContributionToHitGroupIndex() { return m_HitGroupIndex; }

	void SetTexture2DIndex(uint32_t i)
	{
		for (auto &mesh : m_vMeshObjects) { mesh.m_DiffuseTexIndex = i;}
	}

	void SetTexture2DIndexForMesh(uint32_t meshIndex, uint32_t tex_index) 
	{
		m_vMeshObjects[meshIndex].m_DiffuseTexIndex = tex_index;
	}
	

	std::vector<D3DMesh> m_vMeshObjects;
	XMMATRIX m_WorldMatrix;
	XMFLOAT4X4 m_World4x4;
	UINT m_HitGroupIndex = 0;
};

struct D3DSceneModels
{
	void AddModel(D3DModel& m) { m_vModelObjects.push_back(m); }
	std::vector<D3DModel>& GetModelObjects() { return m_vModelObjects; }

	std::vector<D3DModel> m_vModelObjects;
};

struct D3DTexture
{
	ComPtr< ID3D12Resource > m_pTextureResource;
};

struct D3DSceneTextures
{
	void AddTexture(D3DTexture& t) { m_vTextureObjects.push_back(t); }
	std::vector<D3DTexture>& GetTextureObjects() { return m_vTextureObjects; }

	std::vector<D3DTexture> m_vTextureObjects;
};

struct TextureInfo
{
	vector<UINT8> pixels;
	int width;
	int height;
	int stride;
};

struct MaterialCB {
	XMFLOAT4 resolution;
};

struct ViewCB
{
	XMMATRIX view;
	XMFLOAT4 viewOriginAndTanHalfFovY;
	XMFLOAT2 resolution;
	XMMATRIX worldToProjection;
	XMMATRIX projectionToWorld;
	XMFLOAT4 eyeToPixelConeSpreadAngle;

	ViewCB() {
		view = XMMatrixIdentity();
		viewOriginAndTanHalfFovY = XMFLOAT4(0, 0.f, 0.f, 0.f);
		resolution = XMFLOAT2(1280, 720);
		worldToProjection = XMMatrixIdentity();
		projectionToWorld = XMMatrixIdentity();
		eyeToPixelConeSpreadAngle = XMFLOAT4(0, 0.f, 0.f, 0.f);
	}
};

//--------------------------------------------------------------------------------------
// Standard D3D12
//--------------------------------------------------------------------------------------

struct D3D12BufferCreateInfo
{
	UINT64 size;
	UINT64 alignment;
	D3D12_HEAP_TYPE heapType;
	D3D12_RESOURCE_FLAGS flags;
	D3D12_RESOURCE_STATES state;

	D3D12BufferCreateInfo() :
		size(0), alignment(0),
		heapType(D3D12_HEAP_TYPE_DEFAULT),
		flags(D3D12_RESOURCE_FLAG_NONE),
		state(D3D12_RESOURCE_STATE_COMMON) {}

	D3D12BufferCreateInfo(UINT64 InSize, UINT64 InAlignment, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState) :
		size(InSize), alignment(InAlignment),
		heapType(InHeapType),
		flags(InFlags),
		state(InState) {}

	D3D12BufferCreateInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState) :
		size(InSize), alignment(0),
		heapType(D3D12_HEAP_TYPE_DEFAULT),
		flags(InFlags),
		state(InState) {}

	D3D12BufferCreateInfo(UINT64 InSize, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_STATES InState) :
		size(InSize), alignment(0),
		heapType(InHeapType),
		flags(D3D12_RESOURCE_FLAG_NONE),
		state(InState) {}

	D3D12BufferCreateInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags) :
		size(InSize), alignment(0),
		heapType(D3D12_HEAP_TYPE_DEFAULT),
		flags(InFlags),
		state(D3D12_RESOURCE_STATE_COMMON) {}
};

struct D3D12ShaderCompilerInfo 
{
	dxc::DxcDllSupport		DxcDllHelper;
	IDxcCompiler*			compiler;
	IDxcLibrary*			library;

	D3D12ShaderCompilerInfo() 
	{
		compiler = nullptr;
		library = nullptr;
	}
};

struct D3D12ShaderInfo 
{
	LPCWSTR		filename;
	LPCWSTR		entryPoint;
	LPCWSTR		targetProfile;

	D3D12ShaderInfo(LPCWSTR inFilename, LPCWSTR inEntryPoint, LPCWSTR inProfile) 
	{
		filename = inFilename;
		entryPoint = inEntryPoint;
		targetProfile = inProfile;
	}

	D3D12ShaderInfo() 
	{
		filename = NULL;
		entryPoint = NULL;
		targetProfile = NULL;
	}
};

struct D3D12Resources 
{
	//the resource that holds the image generated by the ray tracer.
	ID3D12Resource*									DXROutput;

	//actual vertices resource.  used when creating the vertex buffer descriptor (ie vertex buffer view). 
	//created via device->CreateShaderResourceView.  stored in t2 register
	ID3D12Resource*									vertexBuffer; 
	D3D12_VERTEX_BUFFER_VIEW						vertexBufferView; //used when creating the bottom level "AS" 

	//actual indices resource.  used when creating descriptor to index buffer (ie index buffer view).  stored in t0.
	ID3D12Resource*									indexBuffer; 
	D3D12_INDEX_BUFFER_VIEW							indexBufferView; //used when creating the bottom level "AS" 

	//view matrix data and gpu resource
	ID3D12Resource*									viewCB; //constant buffer that holds view matrix data
	ViewCB											viewCBData; //view matrix data
	UINT8*											viewCBStart; //final bytes mapped to resource for view matrix

	ID3D12Resource*									materialCB;	
	MaterialCB										materialCBData;	
	UINT8*											materialCBStart;

	//get the start of the rtv descriptor heap and put handle to first back buffer of swap chain there
	//we use two handles since swap chain is two buffers.  create descriptor via CreateRenderTargetView
	//The actual back buffers are created with the call to CreateSwapChainForHwnd.  Descriptors to these frame buffers 
	//(i.e. render target views) are stored in the descriptor heap, "rtvheap".  For this example, we don't create
	//specific render target textures, but use the swap chain buffers instead.
	ID3D12DescriptorHeap*							rtvHeap;

	//descriptor heap for shader registers ie srv, cbv, uav descriptors
	ID3D12DescriptorHeap*							cbvSrvUavHeap; 

	//where is sampler heap used??
	ID3D12DescriptorHeap*							samplerHeap;

	//create a resource to hold matrix transform on GPU and a resource to allow for uploading matrix data from cpu to gpu
	//TODO DOES NOT SEEM TO BE USED.  the function Create_Transform_Buffer() is never called.
	ID3D12Resource*									transform; //gpu memory final matrix data in gpu ram
	ID3D12Resource*									transformUploadHeap;//resource (ie heap) that is mapped for cpu copy.

	ID3D12Resource*									texture; //holds the resource on gpu used while rendedring

	//temporary resource used to load texture from system ram to gpu ram.  the textureUploadHeap gets mapped and a pointer to its start address
	//is obtained.  the raw vertex data is mem copied into it.  Afterward, the data in this resource is copied into "texture"
	ID3D12Resource*									textureUploadHeap; 

	UINT											rtvDescSize; //descriptor size in rtv heap

	float											translationOffset;
	float											rotationOffset;
	XMFLOAT3										eyeAngle;
	XMFLOAT3										eyePosition;
};

struct D3D12Global
{
	IDXGIFactory4*									factory;
	IDXGIAdapter1*									adapter;
	ID3D12Device5*									device;
	ID3D12GraphicsCommandList4*						cmdList;

	ID3D12CommandQueue*								cmdQueue;
	ID3D12CommandAllocator*							cmdAlloc[2];

	IDXGISwapChain3*								swapChain;
	ID3D12Resource*									backBuffer[2];

	ID3D12Fence*									fence;
	UINT64											fenceValues[3];
	HANDLE											fenceEvent;
	UINT											frameIndex;

	int												width;
	int												height;
};

//--------------------------------------------------------------------------------------
//  DXR
//--------------------------------------------------------------------------------------

struct AccelerationStructureBuffer
{
	ID3D12Resource* pScratch;
	ID3D12Resource* pResult;
	ID3D12Resource* pInstanceDesc;			// only used in top-level AS

	AccelerationStructureBuffer()
	{
		pScratch = NULL;
		pResult = NULL;
		pInstanceDesc = NULL;
	}
};

//RtProgram contains the entire compiled shader file in a DXIL lib as well as a root signature.
struct RtProgram
{
	RtProgram(D3D12ShaderInfo shaderInfo)
	{
		info = shaderInfo;
		blob = nullptr;
		//subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		exportName = shaderInfo.entryPoint;
		exportDesc.ExportToRename = nullptr;
		exportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

		pRootSignature = nullptr;
	}

	void SetBytecode()
	{
		//called after shader file is compiled
		exportDesc.Name = exportName.c_str();

		dxilLibDesc.NumExports = 1;
		dxilLibDesc.pExports = &exportDesc;
		dxilLibDesc.DXILLibrary.BytecodeLength = blob->GetBufferSize();
		dxilLibDesc.DXILLibrary.pShaderBytecode = blob->GetBufferPointer();

		//subobject.pDesc = &dxilLibDesc;
	}

	RtProgram() 
	{
		blob = nullptr;
		exportDesc.ExportToRename = nullptr;
		pRootSignature = nullptr;
	}

	D3D12ShaderInfo			info = {};
	IDxcBlob*				blob;

	ID3D12RootSignature*	pRootSignature = nullptr;
	D3D12_DXIL_LIBRARY_DESC	dxilLibDesc;
	D3D12_EXPORT_DESC		exportDesc;
	//D3D12_STATE_SUBOBJECT	subobject;
	std::wstring			exportName;
};

struct HitProgram
{
	HitProgram(LPCWSTR name) : exportName(name)
	{
		desc = {};
		desc.HitGroupExport = exportName.c_str();
		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		subobject.pDesc = &desc;
	}

	void SetExports(bool anyHit)
	{
		desc.HitGroupExport = exportName.c_str();
		if (anyHit) desc.AnyHitShaderImport = ahs.exportDesc.Name;
		desc.ClosestHitShaderImport = chs.exportDesc.Name;
	}

	HitProgram() {}

	RtProgram ahs;
	RtProgram chs;

	std::wstring exportName;
	D3D12_HIT_GROUP_DESC desc = {};
	D3D12_STATE_SUBOBJECT subobject = {};
};

struct DXRGlobal
{
	AccelerationStructureBuffer						TLAS;
	AccelerationStructureBuffer						BLAS;
	uint64_t										tlasSize;

	ID3D12Resource*									sbt;
	uint32_t										sbtEntrySize;

	RtProgram										rgs;
	RtProgram										miss;
	HitProgram										hit;
	RtProgram										empty; //global empty root signature

	ID3D12StateObject*								rtpso;
	ID3D12StateObjectProperties*					rtpsoInfo;
};
