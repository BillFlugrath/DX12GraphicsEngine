
struct Data
{
	float4 v1;
	
};

//StructuredBuffer<Data> gInputA : register(t0);
//StructuredBuffer<Data> gInputB : register(t1);
//RWStructuredBuffer<Data> gOutput : register(u0);
RWTexture2D<float4> gOutput : register(u0);

[numthreads(32, 32, 1)]
void CS(int3 groupThreadID : SV_GroupThreadID,
	int3 dispatchThreadID : SV_DispatchThreadID)
{

	gOutput[dispatchThreadID.xy] = float4(0.0f, 1.0f,1.0f, 0);

}
