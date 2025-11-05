#include "HatchingFunctions.hlsl"

struct appdata
{
	float4 vertex : POSITION;
	float2 uv : TEXCOORD0;
	float3 norm : NORMAL;
};

struct v2f
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 nrm : TEXCOORD1;
	float3 wPos : TEXCOORD2;
};

v2f vert(appdata v)
{
	v2f o;
	o.pos = UnityObjectToClipPos(v.vertex);
	o.uv = v.uv * _MainTex_ST.xy + _MainTex_ST.zw;
	o.nrm = mul(float4(v.norm, 0.0), unity_WorldToObject).xyz;
	o.wPos = mul(unity_ObjectToWorld, v.vertex).xyz;
	return o;
}


float4 frag(v2f i) : SV_Target
{
	//return float4(1, 1, 1, 1);
	float4 colorTex = tex2D(_MainTex, i.uv);

	//for directional light _WorldSpaceLightPos0 is direction, all others it's position.
	float3 diffuse = colorTex.rgb * _LightColor0.rgb * dot(_WorldSpaceLightPos0, normalize(i.nrm));

	//get brightness
	float intensity = dot(diffuse, float3(0.2326, 0.7152, 0.0722));

	float4 color = { 0,0,0,1 };
//	color.rgb = HatchingConstantScale(i.uv * 3, intensity, distance(_WorldSpaceCameraPos.xyz, i.wPos) * unity_CameraInvProjection[0][0]);
	
	color.rgb= Hatching(i.uv * 8, intensity);

	color.rgb *= colorTex.rgb;

	return color;
}