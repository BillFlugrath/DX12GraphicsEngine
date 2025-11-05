
cbuffer ObjectConstantBuffer : register(b0)
{
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gView;
    float4x4 gInvView;
    float4 gQuadSize; //only uses first 2 floats
    float4 gCameraProperties;  //{ near, far, rtt width, rtt height}
};

//StructuredBuffer<Data> gInputA : register(t0);
//StructuredBuffer<Data> gInputB : register(t1);
//RWStructuredBuffer<Data> gOutput : register(u0);

Texture2D gInput_1 : register(t0); //scene color texture
Texture2D gInput_2 : register(t1); //depth texture
RWTexture2D<float4> gOutput : register(u0);  //output uav



float NdcDepthToViewDepth(float z_ndc)
{
    //	We	can	invert	the	calculation	from NDC space to view space for the z-coordinate.
     // z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.

    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}

float4 VisualizeDepth(float z_ndc)
{
    float depth = NdcDepthToViewDepth(z_ndc);
    float near_plane = gCameraProperties.x;
    float far_plane = gCameraProperties.y;

    depth = (depth - near_plane) / (far_plane - near_plane);

    return float4(depth, depth, depth, 1.0);
}

float4 PSVizDepthBuffer(int2 xy) 
{
    float z_ndc = gInput_2[xy].r;
    return VisualizeDepth(z_ndc);
}

//Each invocation of the CS shader does a portion of a row of pixels.
//The C++ code to dispatch groups is:
// How many groups do we need to dispatch to cover a row of pixels, where each
// group covers 256 pixels (the 256 is defined in the ComputeShader).
//UINT numGroupsX = (UINT)ceilf(mWidth / 256.0f);  if mWidth=2048, numGroupsX=8
//commandList->Dispatch(numGroupsX, mHeight, 1);  mHeight=2048

#define N 256

[numthreads(N, 1, 1)]
void CS(int3 groupThreadID : SV_GroupThreadID,
	int3 dispatchThreadID : SV_DispatchThreadID)
{
    //Get depth from depth texture
    //int2 xy = int2(dispatchThreadID.x, dispatchThreadID.y);
    //gOutput[dispatchThreadID.xy] = PSVizDepthBuffer(xy);

    //get color of scene
	float4 color = gInput_1[dispatchThreadID.xy];
	gOutput[dispatchThreadID.xy] = color;
}
