#include "stdafx.h"

#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif

#ifdef TINYOBJLOADER_IMPLEMENTATION
#undef TINYOBJLOADER_IMPLEMENTATION
#endif

#include "BLAS_TLAS_Utilities.h"
#include "Utils.h"
#include "Structures.h"

#define d3d_call(a) {HRESULT hr_ = a; if(FAILED(hr_)) { d3dTraceHR( #a, hr_); }}
#define arraysize(a) (sizeof(a)/sizeof(a[0]))
#define align_to(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)

using namespace DXGraphicsUtilities;

static const D3D12_HEAP_PROPERTIES kUploadHeapProps =
{
    D3D12_HEAP_TYPE_UPLOAD,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0,
};

static const D3D12_HEAP_PROPERTIES kDefaultHeapProps =
{
    D3D12_HEAP_TYPE_DEFAULT,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0
};



BLAS_TLAS_Utilities::BLAS_TLAS_Utilities():
    mNumberBLASBuffers(0)
    ,mBottomLevelBuffers(nullptr)
    ,mpBottomLevelAS(nullptr)
    ,mTlasSize(0)//out param gets set as size of TLAS on GPU
    ,mTopLevelBuffers()
{
   
}

BLAS_TLAS_Utilities::~BLAS_TLAS_Utilities()
{
    for (uint32_t i = 0; i < mNumberBLASBuffers; i++)
    {
        SAFE_RELEASE(mBottomLevelBuffers[i].pResult);
        SAFE_RELEASE(mBottomLevelBuffers[i].pScratch);
        SAFE_RELEASE(mBottomLevelBuffers[i].pInstanceDesc);
    }

    SAFE_RELEASE(mTopLevelBuffers.pResult);
    SAFE_RELEASE(mTopLevelBuffers.pScratch);
    SAFE_RELEASE(mTopLevelBuffers.pInstanceDesc);

    SAFE_DELETE_ARRAY(mBottomLevelBuffers);
    SAFE_DELETE_ARRAY(mpBottomLevelAS);
}

ID3D12Resource* BLAS_TLAS_Utilities::createBuffer(ID3D12Device5* pDevice, uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps)
{
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Alignment = 0;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Flags = flags;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.Height = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.SampleDesc.Quality = 0;
    bufDesc.Width = size;

    ID3D12Resource* pBuffer;
    HRESULT hr = pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&pBuffer));

    Utils::Validate(hr, L"Error: failed to create D3D buffer!");
    // d3d_call(pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&pBuffer)));
    return pBuffer;
}

//Create a BLAS resource.  Each BLAS holds vertex buffers for a single model (ie the mesh data for a model is copied
// into a BLAS D3D resource).  The data copied is the actual Vertex Buffer data for each mesh in a model.  If there
//is only 1 mesh in the model, then there is only 1 vertex buffer.  Two mesh objects will have a total of two vertex
//buffers.  For example, if a model has two meshes, geometryCount=2 and we create a D3D12_RAYTRACING_GEOMETRY_DESC
//for each mesh.  
// 
// TODO ADD SUPPORT IN SHADERS FOR MULTPLE MESHES IE READ FROM VERTEX AND INDEX BUFFERS FOR ANY MESH!!
// 
// ONLY 1 VB is supported per model (ie 1mesh) is currently supported!!  The BLAS code will properly create 
//instances of the geometry, but the SHADER code needs to have access to Each VERTEX AND INDEX BUFFER

AccelerationStructureBuffer BLAS_TLAS_Utilities::createBottomLevelAS(ID3D12Device5* pDevice, ID3D12GraphicsCommandList4* pCmdList,
    ID3D12Resource* pVB[], uint32_t *vertexCount, uint32_t geometryCount)
{
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geomDesc;//store 1 D3D12_RAYTRACING_GEOMETRY_DESC for each VB
    geomDesc.resize(geometryCount);

    //Each geometry is a vertex buffer contained in the model.  For API tier 1_1 there is an HLSL intrinsic called
    // GeometryIndex() that gives index of the geometry (ie index of the VB) that the ray hit.  This is different
    // than the InstanceIndex() that specifices a TLAS object.  The value of GeometryIndex() is an index automatically
    //assigned unlike the InstanceIndex that is assigned via InstanceID.  The GeometryIndex value is set incrementally
    //for each object in geomDesc.  For example, geomDesc[0] corresponds to GeometryIndex==0, 
    //geomDesc[1] corresponds to GeometryIndex==1 etc.  Thus, in a shader we can determine which IB and VB the 
    // triangle that was hit resides in.  We then can use that info to calculate the vertex attributes such as uvs.

    for (uint32_t i = 0; i < geometryCount; i++)
    {
        //Create a GeometryInstance in a BLAS object.  NOT to be confused with an InstanceIndex created in TLAS.
        geomDesc[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geomDesc[i].Triangles.VertexBuffer.StartAddress = pVB[i]->GetGPUVirtualAddress(); //store VB address
        geomDesc[i].Triangles.VertexBuffer.StrideInBytes = sizeof(ModelVertex);
        geomDesc[i].Triangles.VertexCount = vertexCount[i]; //number of vertices in the VB
        geomDesc[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geomDesc[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    }

    // Get the size requirements for the scratch and AS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = geometryCount;
    inputs.pGeometryDescs = geomDesc.data();
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
    pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

    // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
    AccelerationStructureBuffer buffers;
    buffers.pScratch = createBuffer(pDevice, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, kDefaultHeapProps);
    buffers.pResult = createBuffer(pDevice, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);

    // Create the bottom-level AS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
    asDesc.Inputs = inputs;
    asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
    asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

    pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

    // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = buffers.pResult;
    pCmdList->ResourceBarrier(1, &uavBarrier);

    return buffers;
}

//The TLAS specifies what "models" to draw and what the world space transform for each model is.  The TLAS does not
//touch any vertex buffers (ie vbs) directly.  Each "model" has its raw vertices stored in the BLAS while
//the BLAS is constructed.  Thus, adding 1 BLAS to the TLAS is the equivalent of adding a model to a scene.
// In the top - level acceleration structure(TLAS), you instance each BLAS individually.

AccelerationStructureBuffer BLAS_TLAS_Utilities::createTopLevelAS(ID3D12Device5* pDevice, ID3D12GraphicsCommandList4* pCmdList, 
    ID3D12Resource* pBottomLevelAS[2], uint64_t& tlasSize, D3DModel* d3dModels, uint32_t num_models)
{
    uint32_t num_instances = num_models;

    // First, get the size of the TLAS buffers and create them
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = num_instances;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
    pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

    // Create the buffers
    AccelerationStructureBuffer buffers;
    buffers.pScratch = createBuffer(pDevice, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
    buffers.pResult = createBuffer(pDevice, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
    tlasSize = info.ResultDataMaxSizeInBytes;

    // The instance desc should be inside a buffer, create and map the buffer
    
    buffers.pInstanceDesc = createBuffer(pDevice, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * num_instances, 
        D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);

    D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs;
    buffers.pInstanceDesc->Map(0, nullptr, (void**)&instanceDescs);
    ZeroMemory(instanceDescs, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * num_instances);

    //Each "instance" is simply a BLAS created earlier.  Thus, in this example, each instance is pBottomLevelAS[i]
    //where each element in array pBottomLevelAS corresponds to a model (a single model can have multiple vbs), but
    //we assume all vbs in a model use the samie hit group index.
    for (uint32_t i = 0; i < num_instances; i++)
    {
        instanceDescs[i].InstanceID = i; // This value in the shader via InstanceID().This value in shader as SV_InstanceID
        
        // InstanceContributionToHitGroupIndex  is the offset inside the shader-table to get correct hit shader and
        // correct parameters for the shader.
        // Since we have unique constant-buffer for each instance, we need a different offset for each model instance.
        //Thus, this index specifies what shader and registers to use when a hit is detected.  If we wish all TLAS
        //models could use a single shader table record.
        instanceDescs[i].InstanceContributionToHitGroupIndex = d3dModels[i].GetHitGroupIndex();  
        instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

        //BillF added transform
        instanceDescs[i].Transform[0][0] = instanceDescs[i].Transform[1][1] = instanceDescs[i].Transform[2][2] = 1; 
        instanceDescs[i].Transform[0][3] = 3.0f*i;//offset on x axis

       // mat4 m = transpose(transformation[i]); // GLM is column major, the INSTANCE_DESC is row major
      //  memcpy(instanceDescs[i].Transform, &m, sizeof(instanceDescs[i].Transform));

        //Assumes 1 acceleration structure per model.  Each BLAS resource contains all of the VBs for a single model.
        instanceDescs[i].AccelerationStructure = pBottomLevelAS[i]->GetGPUVirtualAddress(); //address of BLAS resource
        instanceDescs[i].InstanceMask = 0xFF;
    }

    // Unmap
    buffers.pInstanceDesc->Unmap(0, nullptr);

    // Create the TLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
    asDesc.Inputs = inputs;
    asDesc.Inputs.InstanceDescs = buffers.pInstanceDesc->GetGPUVirtualAddress();
    asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
    asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

    pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

    // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = buffers.pResult;
    pCmdList->ResourceBarrier(1, &uavBarrier);

    return buffers;
}

void BLAS_TLAS_Utilities::createAccelerationStructures(D3D12Global d3d, D3DSceneModels& d3dSceneModels)
{
    std::vector<D3DModel>& models = d3dSceneModels.GetModelObjects();

    const size_t kMaxNumVertBuffers = 32;
    size_t num_models = models.size();
    size_t num_mesh_objects_total = 0;

    std::vector < std::vector< ID3D12Resource* >  > mSceneVBs;
    std::vector < std::vector< uint32_t >  > mSceneVBsNumVerts;

    for (auto model : models)
    {
        std::vector< ID3D12Resource* > vbs;
        std::vector< uint32_t > numverts;

        std::vector<D3DMesh>& meshes = model.GetMeshObjects();
        for (auto mesh : meshes)
        {
            vbs.push_back(mesh.m_pVertexBuffer.Get());  //store vbs for each mesh
            numverts.push_back(mesh.vertices.size());
            num_mesh_objects_total++;
        }

        mSceneVBs.push_back(vbs);
        mSceneVBsNumVerts.push_back(numverts);
    }

    mBottomLevelBuffers = new AccelerationStructureBuffer[num_models];
    mpBottomLevelAS = new ID3D12Resource* [num_models];

    int modelindex = 0;

    for (auto model : models)
    {
        mBottomLevelBuffers[modelindex] = createBottomLevelAS(d3d.device, d3d.cmdList,
            mSceneVBs[modelindex].data(), mSceneVBsNumVerts[modelindex].data(), mSceneVBs[modelindex].size());

        mpBottomLevelAS[modelindex] = mBottomLevelBuffers[modelindex].pResult;

        modelindex++;
        mNumberBLASBuffers++;
    }


    mTopLevelBuffers = createTopLevelAS(d3d.device, d3d.cmdList,
        mpBottomLevelAS, mTlasSize, models.data(), models.size());

}
