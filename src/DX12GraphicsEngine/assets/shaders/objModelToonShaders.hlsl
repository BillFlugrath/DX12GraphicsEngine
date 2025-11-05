
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
//ConstantBuffer< ObjectConstantBuffer > g_ConstBuf : register(b0);


PS_INPUT VSMain( VS_INPUT i )
{
	float4x4 Identity =
	{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 }
	};

	PS_INPUT o;
	o.vPosition = mul(float4(i.vPosition, 1.0), g_MVPMatrix);
#ifdef VULKAN
	o.vPosition.y = -o.vPosition.y;
#endif
	o.vUVCoords = i.vUVCoords;
	o.vUVCoords.y = 1.0 - i.vUVCoords.y; //flip the texture coords on y since they we modeled for gl

	//matrix<float,4,4> worldMatrix = Identity;
	o.vNormal = i.vNormal;
	//o.vNormal = mul(float4(i.vNormal,1.0), Identity).xyz;

	return o;
}
float4 _Color;

float4 _AmbientColor;

float4 _SpecularColor;
float _Glossiness;

float4 _RimColor;
float _RimAmount;
float _RimThreshold;

//https://kylehalladay.com/blog/tutorial/2017/02/21/Pencil-Sketch-Effect.html

float3 Hatching(PS_INPUT i)
{
	float2 uvs = i.vUVCoords * 8;
	float4 vColor = g_Texture.Sample(g_SamplerState, i.vUVCoords);
	float3 hatch0 = vColor.rgb;
	//float3 hatch1 = tex2D(_Hatch1, _uv).rgb;
	
	float3 normal = i.vNormal;

	float3 light_dir = (0.0, 0.0, -1.0);
	light_dir = normalize(light_dir);

	float dp = dot(light_dir, normalize(i.vNormal));
	dp = saturate(dp);

	//get brightness
	float intensity = dp;// dot(diffuse, float3(0.2326, 0.7152, 0.0722));
	intensity *= 0.5;

	float3 overbright = max(0, intensity - 1.0);

	float3 weightsA = saturate((intensity * 6.0) + float3(-0, -1, -2));
	//float3 weightsB = saturate((intensity * 6.0) + float3(-3, -4, -5));

	weightsA.xy -= weightsA.yz;
	//weightsA.z -= weightsB.x;
	//weightsB.xy -= weightsB.yz;

	hatch0 = hatch0 * weightsA;
	//hatch1 = hatch1 * weightsB;

	float3 hatching = overbright + hatch0.r + hatch0.g + hatch0.b;
	
	//float3 hatching = overbright + hatch0.r + hatch0.g + hatch0.b + hatch1.r + hatch1.g + hatch1.b;
		
	return hatching;
}

float4 HatchingSimple(PS_INPUT i)
{
	float3 normal = i.vNormal;

	float3 light_dir = (0.0, 0.0, -1.0);
	light_dir = normalize(light_dir);

	float dp = dot(light_dir, normalize(i.vNormal));
	dp = saturate(dp);

	//Red channel is most dense, green is middle dense, and blue is least dense of hand drawn lines.
	float4 vColor = g_Texture.Sample(g_SamplerState, i.vUVCoords);

	float cLit = vColor.b;  //blue is the least dense of handwritten lines
	float cMed = vColor.g;  //green is the medium dense of handwritten lines
	float cHvy = vColor.r;  //red is the most dense of handwritten lines

	float v = dp ;
	
	float c=0;
	if ( v <0.33)
	{
		float t = smoothstep(0, 0.33, v);
		c = lerp(cHvy, cMed, t);
	}
	else if (v < 0.67)
	{
		float t = smoothstep(0.3, 0.67, v);
		c = lerp(cMed, cLit, t);
	}
	else
	{
		c = cLit;
	}

//	c = lerp(cHvy, cMed, v);
//	c = lerp(c, cLit, v);
	
	return float4(c,c,c, 1);

}

float4 GetToonColor_2(PS_INPUT i)
{
	float3 normal = i.vNormal;
	//float3 viewDir = normalize(i.viewDir);

	float3 light_dir = (0.0, 0.0, -1.0);
	light_dir = normalize(light_dir);


	float dp = dot(light_dir, normalize(i.vNormal));
	dp = saturate(dp);

	//dp = dp * 0.5 + 0.5; //wrap lighting

	float3 highlight_color = float3(1.0, 0, 0.0);
	float3 shadow_color = float3(0.2, 0.0, 0.0);

	// Partition the intensity into light and dark, smoothly interpolated
	// between the two to avoid a jagged break.
	float lightIntensity = dp; // smoothstep(0, 0.01, NdotL* shadow);

	float4 quantized_color = float4(1, 1, 1, 1);
	
		// Discretize the intensity, based on a few cutoff points
	if (lightIntensity > 0.95)
		quantized_color = float4(1.0, 1, 1, 1.0) * lightIntensity;
	else if (lightIntensity > 0.5)
		quantized_color = float4(0.7, 0.7, 0.7, 1.0) * lightIntensity;
	else if (lightIntensity > 0.05)
		quantized_color = float4(0.35, 0.35, 0.35, 1.0) * lightIntensity;
	else
		quantized_color = float4(0.1, 0.1, 0.1, 1.0) * lightIntensity;

	return quantized_color;

	/*
	// Multiply by the main directional light's intensity and color.
	float4 light = lightIntensity * _LightColor0;



	// Calculate specular reflection.
	float3 floatVector = normalize(_WorldSpaceLightPos0 + viewDir);
	float NdotH = dot(normal, halfVector);
	// Multiply _Glossiness by itself to allow artist to use smaller
	// glossiness values in the inspector.
	float specularIntensity = pow(NdotH * lightIntensity, _Glossiness * _Glossiness);
	float specularIntensitySmooth = smoothstep(0.005, 0.01, specularIntensity);
	float4 specular = specularIntensitySmooth * _SpecularColor;

	// Calculate rim lighting.
	float rimDot = 1 - dot(viewDir, normal);
	// We only want rim to appear on the lit side of the surface,
	// so multiply it by NdotL, raised to a power to smoothly blend it.
	float rimIntensity = rimDot * pow(NdotL, _RimThreshold);
	rimIntensity = smoothstep(_RimAmount - 0.01, _RimAmount + 0.01, rimIntensity);
	float4 rim = rimIntensity * _RimColor;

//	float4 sample = tex2D(_MainTex, i.uv);

//	return (light + _AmbientColor + specular + rim) * _Color * sample;
	return (light + _AmbientColor + specular + rim) * _Color ;
	*/
}

//stylized diffuse illumination
float3 GetToonColor(PS_INPUT i)
{
	float3 light_dir = (0.0, 0.0, -1.0);
	light_dir = normalize(light_dir);
	
	//return light_dir;
	//return i.vNormal;
	
	float dp = dot(light_dir, normalize(i.vNormal));
	dp = saturate(dp);

	float3 highlight_color = float3(1.0, 0, 0.0);
	float3 shadow_color = float3(0.2, 0.0, 0.0);

	float threshold = 0.5;

	float3 final_color=float3( 0,1.0,0 );

	//instead of hard edge we can use smooth step
	float shadow_threshold = 0.2;
	float highlight_threshold = 0.4;

	float final_light_intensity = smoothstep(shadow_threshold, highlight_threshold, dp);
	final_color = lerp(shadow_color, highlight_color, final_light_intensity);

	if (dp >= threshold)
	{
		final_color = highlight_color;
	}
	else
	{
		final_color = shadow_color;
	}

	return  final_color;
}

float4 GetWrapLighting(PS_INPUT i)
{
	float3 normal = i.vNormal;

	float3 light_dir = (0.0, 1.0, -1.0);
	light_dir = normalize(light_dir);

	float modulate = 1.0f;
	float dp = dot(light_dir, normalize(i.vNormal));
	dp *= modulate;

	dp = dp * 0.5f + 0.5f; //wrap lighting

	//dp = saturate(dp);

	float4 final_lighting = float4(dp, dp, dp, 1);

	return final_lighting;
}

float4 PSMain( PS_INPUT i ) : SV_TARGET
{
	SamplerState MeshTextureSampler
	{
		Filter = MIN_MAG_MIP_LINEAR;
		AddressU = Wrap;
		AddressV = Wrap;
	};

	float3 toon_color = GetToonColor(i);
	float4 toon_color_2 = GetToonColor_2(i);

	float4 hatching_color_simple = HatchingSimple(i);
	float3 hatching_color = Hatching(i);

	float4 wrap_lighting = GetWrapLighting(i);

	return toon_color_2;
	//return float4(toon_color, 1.0);
	//return float4(hatching_color, 1.0);
	return hatching_color_simple;
	return wrap_lighting;

	float3 vNormal = i.vNormal;

	float3 lightDir =float3(0, -1.0f, 0);

	lightDir = normalize(lightDir);
	float dp = dot(vNormal, lightDir);
	dp = max(0,dp);

	float4 dp_vec=float4(dp, dp, dp, 1);

	float4 vColor = g_Texture.Sample(g_SamplerState, i.vUVCoords ) ;

	float light_brightness = 4.0f;
	return vColor * dp_vec + float4(0.1, 0.1, 0.1, 1) * light_brightness;

}


