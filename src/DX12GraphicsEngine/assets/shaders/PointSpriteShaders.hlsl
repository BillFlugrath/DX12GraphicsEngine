//Render Points As Sprites


// Vertex Shader
struct VS_INPUT
{
	float3 CenterW : POSITION;
	float4 vColor : COLOR0;
};

struct VertexOut
{
	float3 CenterW : POSITION;
	float4 vColor : COLOR0;
	float2 SizeW : SIZE;
};

//Vertex data type output from Geometry shader
struct GeoOut
{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC : TEXCOORD;
	uint   PrimID : SV_PrimitiveID;
	float4 vColor : COLOR0;
};


cbuffer ObjectConstantBuffer : register(b0)
{
	float4x4 g_WVPMatrix;
	float4x4 gViewProj;
	float4x4 gView;
	float4x4 gInvView;
	float4 gEyePosW;
	float4 gQuadSize; //only uses first 2 floats
};

SamplerState g_SamplerState : register(s0);
Texture2D g_Texture : register(t0);
//ConstantBuffer< ObjectConstantBuffer > g_ConstBuf : register(b0);


VertexOut VSMain(VS_INPUT i)
{
	//Vertex shader just passes through vertex data.  The projection is done in the geo shader
	VertexOut o;
	o.CenterW = i.CenterW;// mul(float4(i.CenterW, 1.0), g_WVPMatrix);
	o.vColor = i.vColor;
	o.SizeW = float2(gQuadSize.x, gQuadSize.y);

	return o;
}


[maxvertexcount(4)]
void GSMain(point VertexOut gin[1],
	uint primID : SV_PrimitiveID,
	inout TriangleStream<GeoOut> triStream)
{
	// Compute the local coordinate system of the sprite relative to the world
	
	/*
	float3 up = float3(0.0f, 1.0f, 0.0f); 
	float3 look = gEyePosW - gin[0].CenterW;
	look = normalize(look);
	float3 right = cross(up, look);
	*/

	float3 up = { gView._m01, gView._m11, gView._m21 }; 
	float3 look = { -gView._m02, -gView._m12, -gView._m22 }; //gEyePosW - gin[0].CenterW;
	//look = normalize(look);
	float3 right = { -gView._m00, -gView._m10, -gView._m20 };

	//
	// Compute triangle strip vertices (quad) in world space.
	//
	float halfWidth = 0.5f * gin[0].SizeW.x;
	float halfHeight = 0.5f * gin[0].SizeW.y;

	float4 v[4];
	v[0] = float4(gin[0].CenterW + halfWidth * right - halfHeight * up, 1.0f);
	v[1] = float4(gin[0].CenterW + halfWidth * right + halfHeight * up, 1.0f);
	v[2] = float4(gin[0].CenterW - halfWidth * right - halfHeight * up, 1.0f);
	v[3] = float4(gin[0].CenterW - halfWidth * right + halfHeight * up, 1.0f);

	//
	// Transform quad vertices to world space and output 
	// them as a triangle strip.
	//

	float2 texC[4] =
	{
		float2(0.0f, 1.0f),
		float2(0.0f, 0.0f),
		float2(1.0f, 1.0f),
		float2(1.0f, 0.0f)
	};

	GeoOut gout;
	[unroll]
		for (int i = 0; i < 4; ++i)
		{
			gout.PosH = mul(v[i], gViewProj);
			gout.PosW = v[i].xyz;
			gout.NormalW = look;
			gout.TexC =  texC[i];
			gout.PrimID = primID;
			gout.vColor = gin[0].vColor;
			triStream.Append(gout);
		}
}


//Output a disk for each point.  The output of the GS is a tristrip.  To output a triangle list, we
// call triStream.RestartStrip() after every three vertices have been emitted.  This outputs a list.
[maxvertexcount(48)]
void GSMainDisk(point VertexOut gin[1],
	uint primID : SV_PrimitiveID,
	inout TriangleStream<GeoOut> triStream)
{
	// Compute the local coordinate system of the sprite relative to the world by
	//Calculating a matrix where triangle turns toward camera

	float3 up = float3(0.0f, 1.0f, 0.0f);
	float3 look = gEyePosW - gin[0].CenterW;
	look = -normalize(look);
	float3 right = cross(up, look);
	right = normalize(right);
	up = cross(look, right);
	up = normalize(up);

	float4x4 camera_;
	camera_[0] = float4(right.x, right.y, right.z, 0 );
	camera_[1] = float4(up.x, up.y, up.z, 0 );
	camera_[2] = float4( look.x, look.y, look.z, 0 );
	camera_[3] = float4( 0,0,0,0 );
	
	float4x4 camera_world = camera_;
	//
	// Compute triangle strip vertices in world space.
	// We use triStream.RestartStrip(); to write out a triangle list instead of a strip.
	// Thus, every 3 vertices will be a triangle.

	float3 center = gin[0].CenterW; // Input point is the center
	float radius = gin[0].SizeW.x;// 1.0;
	int segments = 16;

	[unroll]
		for (int i = 0; i < segments; ++i)
		{
			GeoOut gout;

			float4 pos1 = float4(center.xyz,1.0f);

			float angle = 2.0 * 3.14159 * float(i + 1) / float(segments);
			float3 circumference_point = center.xyz + float3(radius * cos(angle), radius * sin(angle), 0.0);
			float4 pos2 = float4(circumference_point, 1.0);

			angle = 2.0 * 3.14159 * float(i) / float(segments);
			circumference_point = center.xyz + float3(radius * cos(angle), radius * sin(angle), 0.0);
			float4 pos3 = float4(circumference_point, 1.0);

			//rotate the points on circumference to align with the camera
			pos2 -= pos1;
			pos3 -= pos1;

			float4x4 camera_rot = gInvView;  //world space camera matrix

			//set world space position of camera to origin.  Thus, the matrix is simply the world
			//rotation of camera
			camera_rot._m03 = 0.0f; camera_rot._m13 = 0.0f; camera_rot._m23 = 0.0f;

			//pos2 = mul(pos2, camera_rot);
			//pos3 = mul(pos3, camera_rot);

			pos2= mul(pos2, camera_world);
			pos3 = mul(pos3, camera_world);
			
			//after apply rotation, translate back to positions relative to center
			pos2 += pos1;
			pos3 += pos1;


			gout.PosH = mul(pos1, gViewProj);
			gout.PosW = pos1.xyz;
			gout.NormalW = look;
			gout.TexC = float2(0, 0);
			gout.PrimID = primID;
			gout.vColor = gin[0].vColor;
			triStream.Append(gout);

			gout.PosH = mul(pos2, gViewProj);
			gout.PosW = pos2.xyz;
			gout.NormalW = look;
			gout.TexC = float2(0, 0);
			gout.PrimID = primID;
			gout.vColor = gin[0].vColor;
			triStream.Append(gout);

			gout.PosH = mul(pos3, gViewProj);
			gout.PosW = pos3.xyz;
			gout.NormalW = look;
			gout.TexC = float2(0, 0);
			gout.PrimID = primID;
			gout.vColor = gin[0].vColor;
			triStream.Append(gout);

			triStream.RestartStrip();  //after 3 vertices are emitted, call RestartStrip.  This creates a list of tris.	
		}
}


float4 PSMain(GeoOut i, float4 Pos : SV_Position) : SV_TARGET
{
	return i.vColor;
}


