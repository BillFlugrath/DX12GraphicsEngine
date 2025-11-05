// Process Image of Main Scene Texture

Texture2D g_texture : register(t0);  //scene color texture
Texture2D g_texture_1 : register(t1); //depth texture

SamplerState g_sampler : register(s0);
SamplerState g_sampler_1 : register(s1); //point sample set

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

//returns value fron near to far plane
float NdcDepthToViewDepth(float z_ndc)
{
    //	We	can	invert	the	calculation	from NDC space to view space for the z-coordinate.
     // z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.

    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}

//returns 0 to 1
float CalculateNormalizedDepth(float z_ndc)
{
    float pz = NdcDepthToViewDepth(z_ndc);

    float near_plane = gCameraProperties.x;
    float far_plane = gCameraProperties.y;

    pz = (pz - near_plane) / (far_plane - near_plane);

    return  pz;
}

float4 VisualizeDepth(float z_ndc)
{
    float depth = CalculateNormalizedDepth(z_ndc);

    return float4(depth, depth, depth, 1.0);
}

float4 ProcessImage(PSInput input, float4 Pos)
{

    bool bFoundHole = false;
    bool bIsFarPlane = false;

    float z_ndc = g_texture_1.Load(int3(Pos.x, 2048-Pos.y, 0));
    float depth = NdcDepthToViewDepth(z_ndc);// CalculateNormalizedDepth(z_ndc);

    if (gCameraProperties.y - depth <= 0.0001)
    {
        //return float4(0, 1, 0, 1);
        bIsFarPlane = true;// return float4(0, 1, 0, 1);
    }
    else
    {
        return g_texture.Sample(g_sampler, input.uv);
    }

   
    int count = 0;
    float4 color = float4(0, 0, 0, 0);


    if (bIsFarPlane)
    {
        for (int i = -10; i <= 10; ++i)
        {
            for (int j = -10; j <= 10; ++j)
            {
                float z_ndc = g_texture_1.Load(int3(Pos.x + i, 2048 - (Pos.y + j), 0)).r;
                float depth = NdcDepthToViewDepth(z_ndc);

                if (gCameraProperties.y - depth > 0.0001)
                {
                    // return float4(0, 0, 1, 0);
                    count++;
                    float4 c = g_texture.Load(int3(Pos.x + i, 2048 - (Pos.y + j), 0));
                    color += c;
                }
            }
        }
    }
    

    if (count >= 260)
    {
        color = color / count;
        return color;
        return float4(1, 0, 0, 1); // color;
    }
    else
    {
        return  g_texture.Load(int3(Pos.x , 2048 - Pos.y, 0));;
       // return g_texture.Sample(g_sampler, input.uv);
    }

    
    return VisualizeDepth(z_ndc);
}

float4 PSVizDepthBuffer(PSInput input, float4 Pos : SV_Position) : SV_TARGET
{
    //float screen_x = Pos.x / gCameraProperties.z;
    //return float4( screen_x,  screen_x,  screen_x, 1);

    float z_ndc = g_texture_1.SampleLevel(g_sampler_1, input.uv, 0.0f).r;
    return VisualizeDepth(z_ndc);
}

float4 PSMain(PSInput input, float4 Pos : SV_Position) : SV_TARGET
{
   //return  ProcessImage(input, Pos);

    //read texture and return color

    //when using texture::Load, the vertical component, y, must be inverted to have parity with texture.Sample calls.
    return  g_texture.Load(int3(Pos.x,  gCameraProperties.w - Pos.y, 0));;

   // return g_texture.Sample(g_sampler, input.uv);
}
