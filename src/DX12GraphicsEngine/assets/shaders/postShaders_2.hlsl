
Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

cbuffer ObjectConstantBuffer : register(b0)
{
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gView;
    float4x4 gInvView;
    float4 gQuadSize; //only uses first 2 floats
    float4 gCameraProperties;  //{ near, far, rtt width, rtt height}
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 PosV : POSITION;
    float2 uv : TEXCOORD;
};


// A pass-through function for the texture coordinate data.
PSInput VSMain(float4 position : POSITION, float2 uv : TEXCOORD)
{
    PSInput result;

    result.position = position;
    result.uv = uv;
   

    // Transform quad corners to	view space near	plane.
    // These to-near-plane vectors are interpolated across the quad	and	give us	a vector v
    // from the eye (ie camera pos) to the near plane for each pixel.
    float4	ph = mul(result.position, gInvProj);
    result.PosV = ph.xyz / ph.w;

    return result;
}

float NdcDepthToViewDepth(float z_ndc)
{
    //	We	can	invert	the	calculation	from NDC space tocviewcspacecfor the z-coordinate.
     // z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.

    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}

float4 VisualizeDepth(float pz)
{
    pz = NdcDepthToViewDepth(pz);
    float near_plane = gCameraProperties.x;
    float far_plane = gCameraProperties.y;

    pz = (pz - near_plane)/(far_plane - near_plane);

    return float4(pz, pz, pz, 1.0);
}

float4 PSVizDepthBuffer(PSInput input) : SV_TARGET
{
    float pz = g_texture.SampleLevel(g_sampler, input.uv, 0.0f).r;
    return VisualizeDepth(pz);
}

float4 PSMain(PSInput input, float4 Pos : SV_Position) : SV_TARGET
{
    //read texture and return color
    int mip = 0;
    int y = gCameraProperties.w -1 - Pos.y;
    return  g_texture.Load(int3(Pos.x,y, mip));

    //read texture and return color
    //return g_texture.Sample(g_sampler, input.uv);
}
