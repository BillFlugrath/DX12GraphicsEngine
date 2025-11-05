
// Vertex Shader
struct VS_INPUT
{
	float3 vPosition : POSITION;
	float3 vNormal: NORMAL;
	float2 vUVCoords: TEXCOORD0;
};

struct PS_INPUT
{
	float4 vPosition : SV_POSITION;
	float3 vNormal : TEXCOORD0;
	float2 vUVCoords : TEXCOORD1;
};

cbuffer ObjectConstantBuffer : register(b0)
{
	float4x4 g_MVPMatrix;
};

SamplerState g_SamplerState : register(s0);
Texture2D g_Texture : register(t0);


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
	return o;
}

float4 PSMain( PS_INPUT i ) : SV_TARGET
{
	float3 vNormal = i.vNormal;
	float3 lightDir =float3(-0.5, 2.0f, -1.0);
	lightDir = normalize(lightDir);
	float dp = dot(vNormal, lightDir);
	float4 dp_vec=float4(dp, dp, dp, 1);
	float4 vColor = g_Texture.Sample( g_SamplerState, i.vUVCoords );
	return vColor * dp_vec + float4(0.1, 0.1, 0.1, 1);;
	//return dp_vec;// float4(vNormal, 1);
}
