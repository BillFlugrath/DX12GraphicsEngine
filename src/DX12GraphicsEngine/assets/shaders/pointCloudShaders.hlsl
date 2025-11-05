//Render Points
// 
//#include "common.hlsl"

// Vertex Shader
struct VS_INPUT
{
	float3 vPosition : POSITION;
	float4 vColor : COLOR0;
	//float2 vUVCoords: TEXCOORD0;
};

struct PS_INPUT
{
	float4 vPosition : SV_POSITION;
	float4 vColor : COLOR0;
	//float2 vUVCoords : TEXCOORD1;
};

cbuffer ObjectConstantBuffer : register(b0)
{
	float4x4 g_MVPMatrix;
};

SamplerState g_SamplerState : register(s0);
Texture2D g_Texture : register(t0);
//ConstantBuffer< ObjectConstantBuffer > g_ConstBuf : register(b0);


float4x4 MatMul(float4x4 ma, float4x4 mb)
{
	float4x4 final = 0.0f;

	uint row = 0;
	uint col = 0;
	uint k = 0;

	for (row = 0; row < 4; ++row)
	{
		for (col = 0; col < 4; ++col)
		{
			for (k = 0; k < 4; ++k)
			{
				final[row][col] += ma[row][k] * mb[k][col];
			}
		}
	}

	return final;
}

float4 GetMatRow(float4x4 mat, uint row)
{
	return mat[row];
}

float4 GetRow2(float4x4 mat, uint row)
{
	float4 vec = { mat._m00, mat._m01, mat._m02, mat._m03 };
	return vec;
}

PS_INPUT VSMain(VS_INPUT i)
{
	PS_INPUT o;
	o.vPosition = mul(float4(i.vPosition, 1.0), g_MVPMatrix);
	o.vColor = i.vColor;

#ifdef VULKAN
	o.vPosition.y = -o.vPosition.y;
#endif
	//o.vUVCoords = i.vUVCoords;
	//o.vUVCoords.y = 1.0 - i.vUVCoords.y; //flip the texture coords on y since they we modeled for gl
	//o.vNormal = i.vNormal;
	return o;
}

float4 PSMain(PS_INPUT i) : SV_TARGET
{
	return i.vColor;

}
