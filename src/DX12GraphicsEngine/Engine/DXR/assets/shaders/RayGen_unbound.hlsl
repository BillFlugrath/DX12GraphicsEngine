
#include "Common.hlsl"

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen() 
{
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;
	uint  PixelLinearIndex = LaunchIndex.x + (LaunchIndex.y * LaunchDimensions.x);

	//BillF put the value in the -1 to +1 range
	float2 d = (((LaunchIndex.xy + 0.5f) / outputResolution.xy) * 2.f - 1.f);
	float aspectRatio = (outputResolution.x / outputResolution.y);

	// Setup the ray
	RayDesc ray;
	ray.Origin = viewOriginAndTanHalfFovY.xyz;
	ray.Direction = normalize((d.x * invView[0].xyz * viewOriginAndTanHalfFovY.w * aspectRatio) - (d.y * invView[1].xyz * viewOriginAndTanHalfFovY.w) + invView[2].xyz);
	ray.TMin = 0.1f;
	ray.TMax = 1000.f;	

	// Trace the ray
	HitInfo payload;
	payload.ShadedColorAndHitT = float4(1, 0, 0, 0);

	// RayContributionToHitGroupIndex Offset to add into Addressing calculations within shader tables for hit group indexing.
	//the maximum allowed value is 15.

	//The fourth parameter is the RayContributionToHitGroupIndex.  Also called Ray index. The final hitgroup index to use
	//is calculated using the Instance geometry in each TLAS instance called InstanceContributionToHitGroupIndex.  Thus,
	//both the TraceRay arg and the instance setting is used to calculate hitgroup index in the SBT (shader binding table).
	
	/*TraceRay( scene, RAY_FLAG_NONE, instancesToQuery, // What geometry?
            hitGroup, numHitGroups, missShader,     // Which shaders?
            ray,                                    // What ray to trace?
            payload );                              // What data to use?
	*/

	TraceRay(
		SceneBVH,
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
		0xFF,
		1,   //RayContributionToHitGroupIndex.  Also called Ray index.  
		0,
		0, //index for miss shader
		ray,
		payload);

	RTOutput[LaunchIndex.xy] = float4(payload.ShadedColorAndHitT.rgb, 1.f);
}
