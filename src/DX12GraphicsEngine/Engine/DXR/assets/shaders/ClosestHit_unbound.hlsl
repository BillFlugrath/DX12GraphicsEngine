
#include "Common_unbound.hlsl"


struct ShadowPayload
{
	bool hit;
};

[shader("closesthit")]
void shadowChs(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.hit = true;
}

[shader("miss")]
void shadowMiss(inout ShadowPayload payload)
{
	payload.hit = false;
}

void CastShadowRay()
{
	float hitT = RayTCurrent();
	float3 rayDirW = WorldRayDirection();
	float3 rayOriginW = WorldRayOrigin();

	// Find the world-space hit position
	float3 posW = rayOriginW + hitT * rayDirW;

	// Fire a shadow ray. The direction is hard-coded here, but can be fetched from a constant-buffer
	RayDesc ray;
	ray.Origin = posW;
	ray.Direction = normalize(float3(0.5, 0.5, -0.5));  //ray direction  toward light source
	ray.TMin = 0.01;
	ray.TMax = 100000;
	ShadowPayload shadowPayload;
	TraceRay(SceneBVH, 0  /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, ray, shadowPayload);
}

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

	vertex.uv.y = 1.0f - vertex.uv.y; //flip they coord

	//calculate texel row,col and load unfiltered texel from texture object's gpu memory
	//int2 coord = floor(vertex.uv * textureResolution.x);
	//float3 color = albedo.Load(int3(coord, 0)).rgb;

	//sample the texture with regular uv coords via a sampler with filter settings
	//The "Texture.Sample" function automatically calculates the appropriate mip level using implicit derivatives (ddx/ddy)
	// of the texture coordinates.  These derivatives are only available in a PS and therefore can't be used
	//in RT, geometry, vertex or compute shaders.  Thus we use SampleLevel and specify the mip level.

	uint meshIndex = GetMeshIndex();
	uint texIndex = diffuseTextureIndexForMesh[meshIndex].x;
	float3 color = diffuse_textures[texIndex].SampleLevel(g_SamplerState, vertex.uv, 0);

	//CastShadowRay();
	//float factor = shadowPayload.hit ? 0.1 : 1.0;

	float factor = 1.0f;
	color = color * factor;

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

	//uint meshIndex = GetMeshIndex();
	//uint texIndex = diffuseTextureIndexForMesh[meshIndex];
	//float3 color = diffuse_textures[texIndex].Load(int3(coord, 0)).rgb;

	// Sample the cubemap texture using the normalized ray direction
	float3 reflct = reflect(WorldRayDirection(), normalize(vertex.normal));
	float4 cubemapColor = cubeMap_0.SampleLevel(g_SamplerState, normalize(reflct),0);

	payload.ShadedColorAndHitT = float4(cubemapColor.rgb, RayTCurrent());

}

