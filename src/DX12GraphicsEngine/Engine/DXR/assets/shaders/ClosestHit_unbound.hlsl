
#include "Common_unbound.hlsl"




// ---[ Closest Hit Shader ]---

[shader("closesthit")]
void ClosestHit(inout HitInfo payload : SV_RayPayload,
	IntersectionAttributes attrib : SV_IntersectionAttributes)
{
	//payload.ShadedColorAndHitT = float4(float3(1,1,0), RayTCurrent());
	//return;

	uint triangleIndex = PrimitiveIndex(); //get triangle index
	float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
	VertexAttributes vertex = GetVertexAttributes(triangleIndex, barycentrics);

	//vertex.normal *= 0.5;
	//vertex.normal += 0.5;
	//payload.ShadedColorAndHitT = float4(vertex.normal, RayTCurrent());
	
	/*
	uint geoIndex = GeometryIndex();  //GeometryIndex is a BLAS index set automatically.  1 index for each vertex buffer.
	if (geoIndex == 0)
	{
		float3 c = float3(1, 0, 0);
		payload.ShadedColorAndHitT = float4(c, RayTCurrent());
	}
	else if (geoIndex == 1)
	{
		float3 c = float3(0, 1, 0);
		payload.ShadedColorAndHitT = float4(c, RayTCurrent());
	}
	else if (geoIndex == 2)
	{
		float3 c = float3(0, 0, 1);
		payload.ShadedColorAndHitT = float4(c, RayTCurrent());
	}
	*/
	//return;

	vertex.uv.y = 1.0f - vertex.uv.y; //flip they coord

	//calculate texel row,col and load unfiltered texel from texture object's gpu memory
	//int2 coord = floor(vertex.uv * textureResolution.x);
	//float3 color = albedo.Load(int3(coord, 0)).rgb;

	//sample the texture with regular uv coords via a sampler with filter settings
	// float3 color = albedo.Sample(g_SamplerState, vertex.uv); //The "Sample" function does NOT work, use SampleLevel

	uint meshIndex = GetMeshIndex();
	uint texIndex = diffuseTextureIndexForMesh[meshIndex].x;
	float3 color = diffuse_textures[texIndex].SampleLevel(g_SamplerState, vertex.uv, 0);
	
	payload.ShadedColorAndHitT = float4(color, RayTCurrent());

}


float2 sampleCube(
	const float3 v,
	out float faceIndex)
{
	float3 vAbs = abs(v);
	float ma;
	float2 uv;
	if (vAbs.z >= vAbs.x && vAbs.z >= vAbs.y)
	{
		faceIndex = v.z < 0.0 ? 5.0 : 4.0;
		ma = 0.5 / vAbs.z;
		uv = float2(v.z < 0.0 ? -v.x : v.x, -v.y);
	}
	else if (vAbs.y >= vAbs.x)
	{
		faceIndex = v.y < 0.0 ? 3.0 : 2.0;
		ma = 0.5 / vAbs.y;
		uv = float2(v.x, v.y < 0.0 ? -v.z : v.z);
	}
	else
	{
		faceIndex = v.x < 0.0 ? 1.0 : 0.0;
		ma = 0.5 / vAbs.x;
		uv = float2(v.x < 0.0 ? v.z : -v.z, -v.y);
	}
	return uv * ma + 0.5;
}

[shader("closesthit")]
void ClosestHit_2(inout HitInfo payload : SV_RayPayload,
	IntersectionAttributes attrib : SV_IntersectionAttributes)
{
	uint triangleIndex = PrimitiveIndex();  //get triangle index
	float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
	VertexAttributes vertex = GetVertexAttributes(triangleIndex, barycentrics);

	//vertex.normal *= 0.5;
	//vertex.normal += 0.5;
	//payload.ShadedColorAndHitT = float4(vertex.normal, RayTCurrent());
	//return;

	/*
	uint instID = InstanceID(); //InstanceID is a TLAS instance index set in app
	if (instID == 1)
	{
		float3 c = float3(0, 1, 1);
		payload.ShadedColorAndHitT = float4(c, RayTCurrent());
		return;
	}
	*/

	//flip they coord

	int2 coord = floor(vertex.uv * textureResolution.x);

	//uint meshIndex = GetMeshIndex();
	//uint texIndex = diffuseTextureIndexForMesh[meshIndex];
	//float3 color = diffuse_textures[texIndex].Load(int3(coord, 0)).rgb;

	// Get the normalized world-space direction of the current ray
	//float3 worldRayDirection = WorldRayDirection();

	// Sample the cubemap texture using the normalized ray direction
	float3 reflct = reflect(WorldRayDirection(), normalize(vertex.normal));
	float4 cubemapColor = cubeMap_0.SampleLevel(g_SamplerState, normalize(reflct),0);

	
	//color.xy = vertex.uv.xy;
	//color = barycentrics;
	// 
	//payload.ShadedColorAndHitT = float4(color.rgb, RayTCurrent());
	

	payload.ShadedColorAndHitT = float4(cubemapColor.rgb, RayTCurrent());

	//payload.ShadedColorAndHitT = float4(1, 1, 0, 1); //BillF test
}