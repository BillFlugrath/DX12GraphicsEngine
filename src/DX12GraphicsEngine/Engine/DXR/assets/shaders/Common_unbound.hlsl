
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


cbuffer SceneShaderData : register(b2)
{
	uint numberOfModelsTotal; //total number of models in scene
	uint numberOfMeshObjectsTotal;
	uint numberOfDiffuseTexturesTotal;
	uint32_t pad0; //MUST PAD!!

	//each index into the array is a model index ie InstanceID. The value at each index is the number of
	//meshes in the model ie the number of vertex buffers in the model.  For ex, if numberOfMeshes[0]=3,
	//then model 0 has 3 unique mesh objects (ie 3 unique vertex buffers).
	uint numberOfMeshes[512];

};


cbuffer SceneTextureShaderData : register(b3)
{
	//get the index into srv descriptor array for diffuse textures for a given mesh
	uint diffuseTextureIndexForMesh[512];
}

// ---[ Resources ]---

RWTexture2D<float4> RTOutput				: register(u0);
RaytracingAccelerationStructure SceneBVH	: register(t0);


//Texture2D<float4> albedo					: register(t1);
//Texture2D<float4> albedo_2					: register(t2);

TextureCube cubeMap_0						: register(t3);

SamplerState      g_SamplerState : register(s0);

//Bindless Buffers for all index and vertex buffers in scene
ByteAddressBuffer indices_and_verts[] : register(t0, space1);  //index and vertex buffers
Texture2D<float4> diffuse_textures[] : register(t0, space2); //diffuse textures

// ---[ Helper Functions ]---

struct VertexAttributes
{
	float3 position;
	float3 normal; 
	float2 uv;
};

uint3 GetIndices(uint triangleIndex, uint flatIndex)
{
	uint baseIndex = (triangleIndex * 3);
	int address = (baseIndex * 4); //each index in index buffer is 4 floats ie index stride=4
	//return indices.Load3(address); //load the three indices for the three vertices of triangle into an uint3 and return
	return indices_and_verts[flatIndex].Load3(address); //load the three indices for the three vertices of triangle into an uint3 and return
}

//Calculate the index into an array of srvs.  The srvs are interleaved index-vertex buffer pairs. The returned index
//is the array index into indices_and_verts[].  The value indices_and_verts[flatIndex] is the srv of index buffer for
//a mesh.  indices_and_verts[flatIndex+1] is the corresponding vertex buffer.
uint GetIndexBufferArrayIndex()
{
	uint instID = InstanceIndex();
	uint i = 0;
	uint flatIndex = 0;

	for (i = 0; i < instID; ++i)
	{
		flatIndex += 2 * numberOfMeshes[i];  //each mesh has 2 buffers (ie vb and ib).  Thus, move 2 indices for each mesh
	}

	uint geoIndex = GeometryIndex();

	flatIndex += 2 * geoIndex;

	return flatIndex;
}

uint GetMeshIndex()
{
	uint instID = InstanceIndex();
	uint i = 0;
	uint flatIndex = 0;

	for (i = 0; i < instID; ++i)
	{
		flatIndex +=  numberOfMeshes[i];  
	}

	uint geoIndex = GeometryIndex();

	flatIndex += geoIndex;
	return flatIndex;
}

VertexAttributes GetVertexAttributes(uint triangleIndex, float3 barycentrics)
{
	uint flatIndex = GetIndexBufferArrayIndex();

	uint3 triIndices = GetIndices(triangleIndex, flatIndex); //get the indices for vertices for triangle from the index buffer
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
		int address = (triIndices[i] * 8) * 4;
		
		//position of vertex is at beginning of address
		v.position += asfloat(indices_and_verts[flatIndex +1].Load3(address)) * barycentrics[i]; //read 3 floats ie x,y,z of position
		
		//normal of vertex is 3 floats offset = 3*4=12 bytes offset from position data
		address += (3 * 4); //3 floats x 4 bytes per float
		v.normal+= asfloat(indices_and_verts[flatIndex +1].Load3(address)) * barycentrics[i]; //read 3 floats ie x,y,z of normal
		
		//uv of vertex is 3 floats offset from normal = 3*4=12 bytes offset from normal (6 floats from start)
		address += (3 * 4);
		v.uv += asfloat(indices_and_verts[flatIndex +1].Load2(address)) * barycentrics[i];//read 2 floats ie x,y of uc tex coord
	}

	return v;
}



