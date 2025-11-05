//BillF Tests

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

// A pass-through function for the texture coordinate data.
PSInput VSMain(float4 position : POSITION, float2 uv : TEXCOORD)
{
    PSInput result;

    result.position = position;
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	//return float4(0, input.uv.x, 0, 1);
	//return float4(0,1,0,1);
    float4 color= g_texture.Sample(g_sampler, input.uv);
	color.a = 1.0;
	return color;
}
