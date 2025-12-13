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

struct Constants
{
    float4x4 World;
    float4x4 WorldView;
    float4x4 WorldViewProj;
    uint     DrawMeshlets;
};

struct MeshInfo
{
    uint IndexBytes;
    uint MeshletOffset;
};

struct Vertex
{
    float3 Position;
    float3 Normal;
    float2 uv0;
};

struct VertexOut
{
    float4 PositionHS : SV_Position;
    float3 PositionVS : POSITION0;
    float3 Normal : NORMAL0;
    float2 TexCoord0 : TEXCOORD0;
    uint   MeshletIndex : COLOR0;
};

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

SamplerState g_SamplerState : register(s0);
Texture2D g_Texture : register(t4);


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


[RootSignature(ROOT_SIG)]
[NumThreads(128, 1, 1)]
[OutputTopology("triangle")]
void msmain(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    out indices uint3 tris[126],
    out vertices VertexOut verts[64]
)
{
    Meshlet m = Meshlets[MeshInfo.MeshletOffset + gid];

    SetMeshOutputCounts(m.VertCount, m.PrimCount);

    if (gtid < m.PrimCount)
    {
        tris[gtid] = GetPrimitive(m, gtid);
    }

    if (gtid < m.VertCount)
    {
        uint vertexIndex = GetVertexIndex(m, gtid);
        verts[gtid] = GetVertexAttributes(gid, vertexIndex);
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




/// ----------------------------------- Simple Triangle Mesh Shader and Pixel Shader -------------------

struct MeshOutput {
    float4 Position : SV_POSITION;
    float3 Color : COLOR;
};

[RootSignature(ROOT_SIG)]
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void msmain2(out indices uint3 triangles[1], out vertices MeshOutput vertices[3]) {
    SetMeshOutputCounts(3, 1);
    triangles[0] = uint3(0, 1, 2);

    vertices[0].Position = float4(-0.5, 0.5, 0.0, 1.0);
    vertices[0].Color = float3(1.0, 0.0, 0.0);

    vertices[1].Position = float4(0.5, 0.5, 0.0, 1.0);
    vertices[1].Color = float3(0.0, 1.0, 0.0);

    vertices[2].Position = float4(0.0, -0.5, 0.0, 1.0);
    vertices[2].Color = float3(0.0, 0.0, 1.0);
}

float4 psmain2(MeshOutput input) : SV_TARGET
{
    return float4(1,1,0,1);
}