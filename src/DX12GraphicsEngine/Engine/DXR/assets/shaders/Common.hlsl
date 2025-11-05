
// ---[ Structures ]---

struct HitInfo
{
	float4 ShadedColorAndHitT : SHADED_COLOR_AND_HIT_T;
};

//IntersectionAttributes are the barycentric coords of hit point (SV_IntersectionAttributes).  NOT texture uvs!!
struct IntersectionAttributes 
{
	float2 uv;
};

// ---[ Constant Buffers ]---

cbuffer ViewCB : register(b0)
{
	matrix invView;
	float4 viewOriginAndTanHalfFovY; //camera position in x,y,z.  camera vertical field of view/2 in w.
	float2 outputResolution;  //resolution of the render target UAV 
};

cbuffer MaterialCB : register(b1)
{
	float4 textureResolution;
};

// ---[ Resources ]---

RWTexture2D<float4> RTOutput				: register(u0);
RaytracingAccelerationStructure SceneBVH	: register(t0);

//We created an srv for both the vertex buffer resource and index buffer resource so that they are accessible in shader
//as t1 and t2
ByteAddressBuffer indices					: register(t1);  //index buffer 0
ByteAddressBuffer vertices					: register(t2); //vertex buffer 0
Texture2D<float4> albedo					: register(t3);

ByteAddressBuffer indices_2 : register(t4);  //index buffer 1
ByteAddressBuffer vertices_2 : register(t5); //vertex buffer 1
Texture2D<float4> albedo_2					: register(t6);

TextureCube cubeMap_0 : register(t7);

SamplerState      g_SamplerState : register(s0);

// ---[ Helper Functions ]---

struct VertexAttributes
{
	float3 position;
	float3 normal; //BillF added
	float2 uv;
};

uint3 GetIndices(uint triangleIndex)
{
	uint baseIndex = (triangleIndex * 3);
	int address = (baseIndex * 4); //each index in index buffer is 4 floats ie index stride=4
	return indices.Load3(address); //load the three indices for the three vertices of triangle into an uint3 and return
}

VertexAttributes GetVertexAttributes(uint triangleIndex, float3 barycentrics)
{
	uint3 indices = GetIndices(triangleIndex); //get the indices for vertices for triangle from the index buffer
	VertexAttributes v;
	v.position = float3(0, 0, 0);
	v.normal = float3(0, 0, 0);
	v.uv = float2(0, 0);

	//loop through the vertex data for the triangle's 3 vertices. Calculate a weighted sum of each vertex attribute
	//where the weight for each vertex is its barycentric value.  Each vertex has 1 weight (a weight is a scalar float).
	for (uint i = 0; i < 3; i++)
	{
		//start triangle vertex address.  This is simply an index into the byte array used to store vertices
		//Each vertex is 8 floats and we have 4 bytes per float.
		// Thus, we calculate a byte offset into the vertex buffer.  This is the address of the first vertex.
		int address = (indices[i] * 8) * 4;
		
		//position of vertex is at beginning of address
		v.position += asfloat(vertices.Load3(address)) * barycentrics[i]; //read 3 floats ie x,y,z of position
		
		//normal of vertex is 3 floats offset = 3*4=12 bytes offset from position data
		address += (3 * 4); //3 floats x 4 bytes per float
		v.normal+= asfloat(vertices.Load3(address)) * barycentrics[i]; //read 3 floats ie x,y,z of normal
		
		//uv of vertex is 3 floats offset from normal = 3*4=12 bytes offset from normal (6 floats from start)
		address += (3 * 4);
		v.uv += asfloat(vertices.Load2(address)) * barycentrics[i];//read 2 floats ie x,y of uc tex coord
	}

	return v;
}



uint3 GetIndices_2(uint triangleIndex)
{
	uint baseIndex = (triangleIndex * 3);
	int address = (baseIndex * 4);
	return indices_2.Load3(address);
}

VertexAttributes GetVertexAttributes_2(uint triangleIndex, float3 barycentrics)
{
	uint3 triIndices = GetIndices_2(triangleIndex);
	VertexAttributes v;
	v.position = float3(0, 0, 0);
	v.normal = float3(0, 0, 0);
	v.uv = float2(0, 0);

	for (uint i = 0; i < 3; i++)
	{
		//start triangle vertex address.  This is simply an index into the byte array used to store vertices
		//Each vertex is 8 floats and we have 4 bytes per float
		int address = (triIndices[i] * 8) * 4;

		//position of vertex is at beginning of address
		v.position += asfloat(vertices_2.Load3(address)) * barycentrics[i];

		//normal of vertex is 3 floats offset = 3*4=12 bytes offset from start
		address += (3 * 4); //3 floats x 4 bytes per float
		v.normal += asfloat(vertices_2.Load3(address)) * barycentrics[i];

		//uv of vertex is 3 floats offset from normal = 3*4=12 bytes offset from normal (6 floats from start)
		address += (3 * 4);
		v.uv += asfloat(vertices_2.Load2(address)) * barycentrics[i];
	}

	return v;
}
