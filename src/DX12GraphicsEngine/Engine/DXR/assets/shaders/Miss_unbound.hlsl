
#include "Common_unbound.hlsl"

// ---[ Miss Shader ]---

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
	// Get the normalized world-space direction of the current ray
	float3 worldRayDirection = WorldRayDirection();

	// Sample the cubemap texture using the normalized ray direction
	float4 cubemapColor = cubeMap_0.SampleLevel(g_SamplerState, normalize(worldRayDirection), 0);

	payload.ShadedColorAndHitT = float4(cubemapColor.rgb, -1.f);
    //payload.ShadedColorAndHitT = float4(0.2f, 0.2f, 0.2f, -1.f);
}