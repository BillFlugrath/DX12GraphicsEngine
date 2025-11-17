
// Vertex Shader input
struct VS_INPUT
{
	float3 vPosition : POSITION;
	float3 vNormal: NORMAL;
	float2 vUVCoords: TEXCOORD0;
};

// Pixel Shader input
struct PS_INPUT
{
	float4 vPosition : SV_POSITION;
	float4 vWorldPosition : TEXCOORD2;
	float3 vNormal : TEXCOORD0;
	float2 vUVCoords : TEXCOORD1;
};

cbuffer ObjectConstantBuffer : register(b0)
{
	float4x4 g_MVPMatrix;
	float4x4 g_WorldMatrix;
};

SamplerState g_SamplerState : register(s0);
Texture2D g_Texture : register(t0);

RaytracingAccelerationStructure SceneBVH : register(t1);
TextureCube cubeMap_0 : register(t2);

PS_INPUT VSMain( VS_INPUT i )
{
	PS_INPUT o;
	o.vPosition = mul(float4(i.vPosition, 1.0), g_MVPMatrix );

#ifdef VULKAN
	o.vPosition.y = -o.vPosition.y;
#endif

	o.vUVCoords = i.vUVCoords;
	o.vUVCoords.y = 1.0 - i.vUVCoords.y; //flip the texture coords on y since they we modeled for gl
	o.vNormal = i.vNormal;
	o.vWorldPosition = mul(float4(i.vPosition, 1.0), g_WorldMatrix);

	return o;
}

float4 CastRay(float3 worldPos)
{
	float4 final = float4(0, 1, 0, 1);

	// Instantiate ray query object. Template parameter allows driver to generate a specialized implementation.
	RayQuery<RAY_FLAG_CULL_NON_OPAQUE 
		| RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES 
		| RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;

	//RayQuery < RAY_FLAG_FORCE_OPAQUE > q2;

	// Create a shadow ray. The direction is hard-coded here, but can be fetched from a constant-buffer

	float3 rayDirW = normalize(float3(0.0, 1.0, -0.2));
	//float3 rayOriginW = float3(0, 0, 0);
	float3 rayOriginW =  worldPos;

	RayDesc ray;
	ray.Origin = rayOriginW;
	ray.Direction = rayDirW;
	ray.TMin = 0.01;
	ray.TMax = 10000;

	uint myInstanceMask = 0xFF;

	q.TraceRayInline(
		SceneBVH,
		RAY_FLAG_NONE, // OR'd with flags above
		myInstanceMask,
		ray);

	// Proceed() below is where behind-the-scenes traversal happens,including the heaviest of any driver inlined code.
	// In this simplest of scenarios, Proceed() only needs to be called once rather than a loop:
	// Based on the template specialization above, traversal completion is guaranteed.
	// Proceed()==TRUE means there is a candidate for a hit that requires shader consideration.
	while (q.Proceed()) 
	{
		return float4(0, 1, 0, 1);
	}

	//q.Proceed();

	// Examine and act on the result of the traversal. Was a hit committed?
	if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
	{
		final = float4(0.2, 0.2, 0.2, 1);
		/*
		ShadeMyTriangleHit(
			q.CommittedInstanceIndex(),
			q.CommittedPrimitiveIndex(),
			q.CommittedGeometryIndex(),
			q.CommittedRayT(),
			q.CommittedTriangleBarycentrics(),
			q.CommittedTriangleFrontFace());
			*/
	}
	else if (q.CommittedStatus() == COMMITTED_NOTHING) // COMMITTED_NOTHING
		 // From template specialization,
		 // COMMITTED_PROCEDURAL_PRIMITIVE can't happen.
	{
		final = float4(1,1,1, 1);
		// Do miss shading
		/*
		MyMissColorCalculation(
			q.WorldRayOrigin(),
			q.WorldRayDirection());
			*/
	}
	else
	{
		final = float4(0, 0, 1, 1);
	}

	return final;
}

float4 PSMain( PS_INPUT i ) : SV_TARGET
{
	//return float4(1, 1, 0, 1);

	float3 vNormal = i.vNormal;
	float3 lightDir =float3(-0.5, 2.0f, -1.0);
	lightDir = normalize(lightDir);
	float dp = dot(vNormal, lightDir);
	float4 dp_vec=float4(dp, dp, dp, 1);
	float4 vColor = g_Texture.Sample( g_SamplerState, i.vUVCoords );
	
	float3 worldPos =  i.vWorldPosition.xyz;
	float4 rayColor = CastRay(worldPos);

	//return rayColor;

	return float4(vColor.rgb,1.0) * rayColor; //modulate by inline ray color

	return vColor * dp_vec + float4(0.1, 0.1, 0.1, 1);;

	//return dp_vec;// float4(vNormal, 1);
}

