
#include "Common_unbound.hlsl"

#define SHADOW_RAY_INDEX 2
#define REFLECT_RAY_INDEX 3

struct ShadowPayload
{
	float isVisible;
};

[shader("closesthit")]
void ClosestHitShadow(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.isVisible = 0.0;
}

[shader("miss")]
void MissShadow(inout ShadowPayload payload)
{
	payload.isVisible = 1.0;
}

ShadowPayload CastShadowRay(float3 norm)
{
	float hitT = RayTCurrent();
	float3 rayDirW = WorldRayDirection();
	float3 rayOriginW = WorldRayOrigin();

	// Find the world-space hit position
	float3 posW = rayOriginW + hitT * rayDirW;
	//posW += 0.1 * norm;

	// Fire a shadow ray. The direction is hard-coded here, but can be fetched from a constant-buffer
	RayDesc ray;
	ray.Origin = posW;
	ray.Direction = normalize(float3(0.0, 1.0, 1.0));  //ray direction  toward light source
	ray.TMin = 0.01;
	ray.TMax = 100000;

	//unused flags | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH 
	ShadowPayload shadowPayload;

	//The fourth parameter is the RayContributionToHitGroupIndex.  Also called Ray index. The final hitgroup index to use
	//is calculated using the Instance geometry in each TLAS instance called InstanceContributionToHitGroupIndex.  Thus,
	//both the TraceRay arg and the instance setting is used to calculate hitgroup index in the SBT (shader binding table).
	
	/*TraceRay( scene, RAY_FLAG_NONE, instancesToQuery, // What geometry?
		hitGroup, numHitGroups, missShader,     // Which shaders?
		ray,                                    // What ray to trace?
		payload );                              // What data to use?
	*/

	TraceRay(SceneBVH, 
		 RAY_FLAG_CULL_FRONT_FACING_TRIANGLES  /*rayFlags*/,
		0xFF, 
		SHADOW_RAY_INDEX,	//RayContributionToHitGroupIndex.  Also called Ray index.  
		0, 
		1, //miss shader index
		ray, shadowPayload);

	return shadowPayload;
}

HitInfo CastReflectedRay(float3 norm)
{
	float hitT = RayTCurrent();
	float3 rayDirW = WorldRayDirection();
	float3 rayOriginW = WorldRayOrigin();

	// Find the world-space hit position
	float3 posW = rayOriginW + hitT * rayDirW;

	// Fire a shadow ray. The direction is hard-coded here, but can be fetched from a constant-buffer
	RayDesc ray;
	ray.Origin = posW;
	ray.Direction = reflect(WorldRayDirection(), normalize(norm));
	ray.TMin = 0.01;
	ray.TMax = 100000;

	//unused flags | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH 
	HitInfo payload;

	TraceRay(SceneBVH,
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES  /*rayFlags*/,
		0xFF,
		REFLECT_RAY_INDEX,	//RayContributionToHitGroupIndex.  Also called Ray index.  
		0,
		0, //miss shader index
		ray, payload);

	return payload;
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

	//sample the texture with regular uv coords via a sampler with filter settings
	//The "Texture.Sample" function automatically calculates the appropriate mip level using implicit derivatives (ddx/ddy)
	// of the texture coordinates.  These derivatives are only available in a PS and therefore can't be used
	//in RT, geometry, vertex or compute shaders.  Thus we use SampleLevel and specify the mip level.

	uint meshIndex = GetMeshIndex();
	uint texIndex = diffuseTextureIndexForMesh[meshIndex].x;
	float3 color = diffuse_textures[texIndex].SampleLevel(g_SamplerState, vertex.uv, 0);

	ShadowPayload shadowPayload = CastShadowRay(vertex.normal);

	if (shadowPayload.isVisible == 0.0)
	{
		if (InstanceIndex() == 2) //apply to plane only
			color *= float3(0.4, 0.4, 0.4); // float3(1, 1, 0);
	}

	//Add secondary reflected ray
	//HitInfo payloadReflected = CastReflectedRay(vertex.normal);
	//float3 reflectedColor = payloadReflected.ShadedColorAndHitT.rgb;
	//color += reflectedColor * 0.1;

	//if (InstanceIndex() == 1)
	//{
		
	//}

	payload.ShadedColorAndHitT = float4(color, RayTCurrent());  //use Texture2d sample

}

[shader("closesthit")]
void ClosestHitReflected(inout HitInfo payload : SV_RayPayload,
	IntersectionAttributes attrib : SV_IntersectionAttributes)
{

	uint triangleIndex = PrimitiveIndex(); //get triangle index
	float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
	VertexAttributes vertex = GetVertexAttributes(triangleIndex, barycentrics);

	vertex.uv.y = 1.0f - vertex.uv.y; //flip they coord

	uint meshIndex = GetMeshIndex();
	uint texIndex = diffuseTextureIndexForMesh[meshIndex].x;
	float3 color = diffuse_textures[texIndex].SampleLevel(g_SamplerState, vertex.uv, 0);

	payload.ShadedColorAndHitT = float4(color, RayTCurrent());  //use Texture2d sample

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

	//Add secondary reflected ray
	HitInfo payloadReflected = CastReflectedRay(vertex.normal);

	payload.ShadedColorAndHitT = float4(payloadReflected.ShadedColorAndHitT.rgb, RayTCurrent());

}

