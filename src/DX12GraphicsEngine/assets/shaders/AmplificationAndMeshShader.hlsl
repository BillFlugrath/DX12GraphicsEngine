//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

//signature arguments in C++ before dispatch is called:
/*

        m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress() + sizeof(SceneConstantBuffer) * m_frameIndex); //CBV in b0
        m_commandList->SetGraphicsRoot32BitConstant(1, mesh.IndexSize, 0); //1st 32 bit constant in b1
        m_commandList->SetGraphicsRootShaderResourceView(2, mesh.VertexResources[0]->GetGPUVirtualAddress()); //t0
        m_commandList->SetGraphicsRootShaderResourceView(3, mesh.MeshletResource->GetGPUVirtualAddress()); //t1
        m_commandList->SetGraphicsRootShaderResourceView(4, mesh.UniqueVertexIndexResource->GetGPUVirtualAddress()); //t2
        m_commandList->SetGraphicsRootShaderResourceView(5, mesh.PrimitiveIndexResource->GetGPUVirtualAddress());//t3
        m_commandList->SetGraphicsRoot32BitConstant(1, subset.Offset, 1); //2nd 32 bit constant in b1.  last param=1 is the param offset
*/

//See https://learn.microsoft.com/en-us/windows/win32/direct3d12/specifying-root-signatures-in-hlsl#descriptor-table

#define ROOT_SIG "CBV(b0), \
                  RootConstants(b1, num32bitconstants=2), \
                  SRV(t0), \
                  SRV(t1), \
                  SRV(t2), \
                  SRV(t3), \
                  DescriptorTable(SRV(t4, numDescriptors=1)), \
                  StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR)"

/////////////////////////////////////////////////////////////////////////

//32 is selected to match the number of threads in a GPU wave, aka the GPU’s wave size. 
#define AS_GROUP_SIZE 32

/*
struct CameraProperties 
{
    float4x4 MVP;
};
*/

//ConstantBuffer<CameraProperties> Cam : register(b2);


struct Payload 
{
    uint MeshletIndices[AS_GROUP_SIZE];
};

groupshared Payload sPayload;

// -------------------------------------------------------------------------------------------------
// Amplification Shader
// -------------------------------------------------------------------------------------------------

/*
IMPORTANT: Using UINT INSTEAD OF UINT3 for SV_GroupThreadID and SV_DispatchThreadID !!!
The shader built-in SV_DispatchThreadID is a uint3 type, not uint. It provides the unique 3D index 
for the current thread across the entire dispatch call in a compute shader. 
The reason you may have seen it used as a single uint (e.g., id.x) in code examples is usually for 
simplicity when the shader is operating on a 1D data structure like a flat array or a buffer where 
the y and z dimensions are explicitly set to 1. 
*/

[numthreads(AS_GROUP_SIZE, 1, 1)]
void asmain(
    uint gtid : SV_GroupThreadID, //typically this is an uint3, but ok since group and thread indices for y and z are 1
    uint dtid : SV_DispatchThreadID, //typically this is an uint3, , but ok since group and thread indices for y and z are 1
    uint gid : SV_GroupID
)
{
    sPayload.MeshletIndices[gtid] = dtid;

    // Dispatch all Mesh Shader Threadgroups. If AS_GROUP_SIZE==32, we would be dispatching 32 thread groups where 
    // each  Mesh Shader group contains a number of actual mesh shader threads.  The max number of threads that a group
    // can contain (as specificed by hardware) is known as Warp or Wavefront sizes (NVidia and AMD).
   
    //For all APIs, calling the dispatch mesh shader function will end execution of the entire amplification
    // shader threadgroup’s execution. That is, calling DispatchMesh() will end execution for threadgroup.  Thus,
    // DispatchMesh() will ONLY GET CALLED ONCE PER THREADGROUP!!
    //DispatchMesh() called from the amplification shader, launches the threadgroups for 
    // the mesh shader. This function must be called exactly once per amplification shader
    // !! The DispatchMesh call implies a GroupMemoryBarrierWithGroupSync().  Thus, threads in the thread group will
    //stall at this point until all threads in the group reach the DispatchMesh call.
    
    //Ex, if the c++ is:
    //UINT threadGroupCountX = 2; pCommandList->DispatchMesh(threadGroupCountX, 1, 1); 
    // then 2 AS Workgroups are dispatched.  This will cause DispatchMesh(AS_GROUP_SIZE, 1, 1, sPayload); to get called twice
    // (as shown below). Each DispatchMesh call launches 32 workgroups of mesh shader.
    //Thus, we launched 2*32 = 64 workgroups of mesh shaders where each mesh shader workgroup will have a specific number
    //of threads.
    DispatchMesh(AS_GROUP_SIZE, 1, 1, sPayload); // Tell mesh shader which meshlet to look at
}


struct Constants
{
    float4x4 World;
    float4x4 WorldView;
    float4x4 WorldViewProj;
    uint     DrawMeshlets; //draw debug viz of meshlets if true
};

//The MeshInfo struct is stored in register b1.  It is stored via TWO C++ calls each setting a 32-bit const:
//pCommandList->SetGraphicsRoot32BitConstant(1, mesh.IndexSize, 0); //b1 at offset 0 ie last param=0
//pCommandList->SetGraphicsRoot32BitConstant(1, subset.Offset, 1);//b1 at offset 1 ie last param=1

// !!THUS THERE IS NO "MeshInfo" struct on the C++ side!!  The C++ manually sets  register b1 with the two calls above
struct MeshInfo
{
    uint IndexBytes;// mesh.IndexSize is number of bytes to store vertex index ie 4
    uint MeshletOffset;
};

//struct defines the actual vertices in the VB for the original mesh.  This matches the vertex struct used in the C++ when writing the vertices
//to the vertex buffer
struct Vertex
{
    float3 Position;
    float3 Normal;
    float2 uv0;
};

//struct to define the output vertex type emitted from mesh shader
struct VertexOut
{
    float4 PositionHS : SV_Position;
    float3 PositionVS : POSITION0;
    float3 Normal : NORMAL0;
    float2 TexCoord0 : TEXCOORD0;
    uint   MeshletIndex : COLOR0;
};

//The struct Meshlet defines the element type in StructuredBuffer array.  This struct matches the original meshlet data in the imported mesh data.  For ex,
//the sphere1.bin model has 33 meshlets each with the data shown below.  These 33 meshlets are stored in StructuredBuffer<Meshlet> Meshlets below. 
struct Meshlet
{
    uint VertCount;
    uint VertOffset;
    uint PrimCount;
    uint PrimOffset;
};

ConstantBuffer<Constants> Globals             : register(b0);
ConstantBuffer<MeshInfo>  MeshInfo            : register(b1);

StructuredBuffer<Vertex>  Vertices            : register(t0);
StructuredBuffer<Meshlet> Meshlets            : register(t1);
ByteAddressBuffer         UniqueVertexIndices : register(t2);
StructuredBuffer<uint>    PrimitiveIndices    : register(t3);

SamplerState g_SamplerState : register(s0); //texture sampler for diffuse texture in PS
Texture2D g_Texture : register(t4); //diffuse texture in PS


/////
// Data Loaders

uint3 UnpackPrimitive(uint primitive)
{
    // Unpacks a 10 bits per index triangle from a 32-bit uint.
    return uint3(primitive & 0x3FF, (primitive >> 10) & 0x3FF, (primitive >> 20) & 0x3FF);
}

uint3 GetPrimitive(Meshlet m, uint index)
{
    return UnpackPrimitive(PrimitiveIndices[m.PrimOffset + index]);
}

uint GetVertexIndex(Meshlet m, uint localIndex)
{
    localIndex = m.VertOffset + localIndex;

    if (MeshInfo.IndexBytes == 4) // 32-bit Vertex Indices
    {
        return UniqueVertexIndices.Load(localIndex * 4);
    }
    else // 16-bit Vertex Indices
    {
        // Byte address must be 4-byte aligned.
        uint wordOffset = (localIndex & 0x1);
        uint byteOffset = (localIndex / 2) * 4;

        // Grab the pair of 16-bit indices, shift & mask off proper 16-bits.
        uint indexPair = UniqueVertexIndices.Load(byteOffset);
        uint index = (indexPair >> (wordOffset * 16)) & 0xffff;

        return index;
    }
}

VertexOut GetVertexAttributes(uint meshletIndex, uint vertexIndex)
{
    Vertex v = Vertices[vertexIndex];

    VertexOut vout;
    vout.PositionVS = mul(float4(v.Position, 1), Globals.WorldView).xyz;
    vout.PositionHS = mul(float4(v.Position, 1), Globals.WorldViewProj);
    vout.Normal = mul(float4(v.Normal, 0), Globals.World).xyz;
    vout.TexCoord0 = v.uv0;
    vout.MeshletIndex = meshletIndex;

    return vout;
}

//mesh shader
[RootSignature(ROOT_SIG)]
[NumThreads(128, 1, 1)] // 128 equals the max number of primitives (ie triangles) output.  Thus, 1 triangle/thread output.
[OutputTopology("triangle")]
void msmain(
    uint gtid : SV_GroupThreadID,//typically this is an uint3, but ok since group and thread indices for y and z are 1
    uint gid : SV_GroupID,//typically this is an uint3, but ok since group and thread indices for y and z are 1
    in payload  Payload    payload,
    out indices uint3 tris[126],
    out vertices VertexOut verts[64]
)
{
    //one meshlet/thread group.  Thus, we use the group id, "gid" as an index into the array of meshlets.
    Meshlet m = Meshlets[MeshInfo.MeshletOffset + gid];   // Look up meshlet

    SetMeshOutputCounts(m.VertCount, m.PrimCount); //MUST be called for all mesh shaders

    //Use the group thread id, "gtid" as an index into various arrays for a specific meshlet.  We test the gtid
    // using  if (gtid < m.PrimCount) and  if (gtid < m.VertCount) to make sure a thread with an index to high
    //simply does nothing!!
    if (gtid < m.PrimCount)
    {
        tris[gtid] = GetPrimitive(m, gtid); //ouput 1 triangle per thread ie  out indices uint3 tris[126]
    }

    if (gtid < m.VertCount)
    {
        uint vertexIndex = GetVertexIndex(m, gtid);
        verts[gtid] = GetVertexAttributes(gid, vertexIndex); //ouput 1 vertex ie  out vertices VertexOut verts[64]
    }
}


////////////////////////////////////////////////////////////////////////////////

float4 psmain(VertexOut input) : SV_TARGET
{
    // return float4(input.TexCoord0.x,input.TexCoord0.y,0,1);
    float2 uv = input.TexCoord0;
    uv.y = 1.0f - uv.y;
    float4 texColor = g_Texture.Sample(g_SamplerState, uv);
//    return texColor;

    float ambientIntensity = 0.1;
    float3 lightColor = float3(1, 1, 1);
    float3 lightDir = -normalize(float3(1, -1, 1));

    float3 diffuseColor;
    float shininess;

    //draw debug viz of meshlets if true
    if (Globals.DrawMeshlets)
    {
        uint meshletIndex = input.MeshletIndex;
        diffuseColor = float3(
            float(meshletIndex & 1),
            float(meshletIndex & 3) / 4,
            float(meshletIndex & 7) / 8);
        shininess = 16.0;
        return float4(diffuseColor, 1);
    }
    else
    {
        diffuseColor = 0.8;
        shininess = 64.0;
    }

    float3 normal = normalize(input.Normal);

    // Do some fancy Blinn-Phong shading!
    float cosAngle = saturate(dot(normal, lightDir));
    float3 viewDir = -normalize(input.PositionVS);
    float3 halfAngle = normalize(lightDir + viewDir);

    float blinnTerm = saturate(dot(normal, halfAngle));
    blinnTerm = cosAngle != 0.0 ? blinnTerm : 0.0;
    blinnTerm = pow(blinnTerm, shininess);

    float3 finalColor = (cosAngle + blinnTerm + ambientIntensity) * diffuseColor;

    return float4(finalColor, 1);
}


