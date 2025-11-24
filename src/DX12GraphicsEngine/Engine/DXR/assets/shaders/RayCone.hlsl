
#include "Common_unbound.hlsl"

/*
GPU Gems 2 Chapter 7 https://github.com/Apress/Ray-Tracing-Gems-II/ for working VS example code.

GPU Gems 1 Chapter 20 for detailed diagrams.  See page 331 for diagram and description of how the cone is
calculated.
*/

void GetVertexAttributes(uint triangleIndex, out VertexAttributes Verts[3])
{
	uint flatIndex = GetIndexBufferArrayIndex();

	uint3 triIndices = GetIndices(triangleIndex, flatIndex); //get the indices for vertices for triangle from the index buffer


	//loop through the vertex data for the triangle's 3 vertices. Calculate a weighted sum of each vertex attribute
	//where the weight for each vertex is its barycentric value.  Each vertex has 1 weight (a weight is a scalar float).
	for (uint i = 0; i < 3; i++)
	{
		//address of vertex i.  This is simply an index into the byte array used to store vertices
		//Each vertex is 8 floats and we have 4 bytes per float.
		// Thus, we calculate a byte offset into the vertex buffer.  This is the address of the vertex.
		int address = (triIndices[i] * 8) * 4;

		//The actual vertex buffer for the triangle is indices_and_verts[flatIndex + 1] ie
		//indices_and_verts[flatIndex + 1] is the Srv for the ByteAddressBuffer corresponding to the vb.

		//position of vertex is at beginning of address
		Verts[i].position = asfloat(indices_and_verts[flatIndex + 1].Load3(address)); //read 3 floats ie x,y,z of position

		//normal of vertex is 3 floats offset = 3*4=12 bytes offset from position data
		address += (3 * 4); //3 floats x 4 bytes per float
		Verts[i].normal = asfloat(indices_and_verts[flatIndex + 1].Load3(address)); //read 3 floats ie x,y,z of normal

		//uv of vertex is 3 floats offset from normal = 3*4=12 bytes offset from normal (6 floats from start)
		address += (3 * 4);
		Verts[i].uv = asfloat(indices_and_verts[flatIndex + 1].Load2(address));//read 2 floats ie x,y of uc tex coord
	}
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float2 HitAttribute(float2 v0, float2 v1, float2 v2, IntersectionAttributes attr)
{
	return v0 + attr.uv.x * (v1 - v0) + attr.uv.y * (v2 - v0);
}

float3 HitAttribute(float3 v0, float3 v1, float3 v2, IntersectionAttributes attr)
{
	return v0 + attr.uv.x * (v1 - v0) + attr.uv.y * (v2 - v0);
}

uint2 TexDims(Texture2D<float4> tex) { uint2 vSize; tex.GetDimensions(vSize.x, vSize.y); return vSize; }
uint2 TexDims(Texture2D<float3> tex) { uint2 vSize; tex.GetDimensions(vSize.x, vSize.y); return vSize; }
uint2 TexDims(Texture2D<float > tex) { uint2 vSize; tex.GetDimensions(vSize.x, vSize.y); return vSize; }

float2 UVAreaFromRayCone(float3 vRayDir, float3 vWorldNormal, float vRayConeWidth, float2 aUV[3], float3 aPos[3], float3x3 matWorld)
{
	float2 vUV10 = aUV[1] - aUV[0];
	float2 vUV20 = aUV[2] - aUV[0];
	float fTriUVArea = abs(vUV10.x * vUV20.y - vUV20.x * vUV10.y);

	// We need the area of the triangle, which is length(triangleNormal) in worldspace, and I
	// could not figure out a way with fewer than two 3x3 mtx multiplies for ray cones.
	float3 vEdge10 = mul(aPos[1] - aPos[0], matWorld);
	float3 vEdge20 = mul(aPos[2] - aPos[0], matWorld);

	float3 vFaceNrm = cross(vEdge10, vEdge20); // in world space, by design
	float fTriLODOffset = 0.5f * log2(fTriUVArea / length(vFaceNrm)); // Triangle-wide LOD offset value
	float fDistTerm = vRayConeWidth * vRayConeWidth;
	float fNormalTerm = dot(vRayDir, vWorldNormal);

	return float2(fTriLODOffset, fDistTerm / (fNormalTerm * fNormalTerm));
}

float UVAreaToTexLOD(uint2 vTexSize, float2 vUVAreaInfo)
{
	return vUVAreaInfo.x + 0.5f * log2(vTexSize.x * vTexSize.y * vUVAreaInfo.y);
}

float4 UVDerivsFromRayCone(float3 vRayDir, float3 vWorldNormal, float vRayConeWidth, float2 aUV[3], float3 aPos[3], float3x3 matWorld)
{
	float2 vUV10 = aUV[1] - aUV[0];
	float2 vUV20 = aUV[2] - aUV[0];
	float fQuadUVArea = abs(vUV10.x * vUV20.y - vUV20.x * vUV10.y);

	// Since the ray cone's width is in world-space, we need to compute the quad
	// area in world-space as well to enable proper ratio calculation
	float3 vEdge10 = mul(aPos[1] - aPos[0], matWorld);
	float3 vEdge20 = mul(aPos[2] - aPos[0], matWorld);
	float3 vFaceNrm = cross(vEdge10, vEdge20);
	float fQuadArea = length(vFaceNrm);

	float fDistTerm = abs(vRayConeWidth);
	float fNormalTerm = abs(dot(vRayDir, vWorldNormal));
	float fProjectedConeWidth = vRayConeWidth / fNormalTerm;
	float fVisibleAreaRatio = (fProjectedConeWidth * fProjectedConeWidth) / fQuadArea;

	float fVisibleUVArea = fQuadUVArea * fVisibleAreaRatio;
	float fULength = sqrt(fVisibleUVArea);
	return float4(fULength, 0, 0, fULength);
}

uint CalculateTexLod(Texture2D<float4> tex, IntersectionAttributes attrib, out float4 uvDerivs)
{
	uint triangleIndex = PrimitiveIndex(); //get triangle index
	uint flatIndex = GetIndexBufferArrayIndex();

	uint3 indices = GetIndices(triangleIndex, flatIndex); //get the indices for vertices for triangle from the index buffer
	
	VertexAttributes Verts[3];
	GetVertexAttributes(triangleIndex, Verts);

    const float3 hitPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
	const float2 uv = HitAttribute(Verts[0].uv, Verts[1].uv, Verts[2].uv, attrib);
	const float3 nrm = HitAttribute(Verts[0].normal, Verts[1].normal, Verts[2].normal, attrib); // in world space because model is in world space
	
    const float3 aPos[3] = { Verts[0].position, Verts[1].position, Verts[2].position };
	const float2 aUVs[3] = { Verts[0].uv, Verts[1].uv, Verts[2].uv };

	//rayCone has 2 members: width and spread angle. thus, rayCone.x=width and rayCone.y=spread angle.  Starting width=0.
	//spread angle equation page 334 spread angle at origin set in C++.  Thus, rayConeAtHit is a new rayCone where
	//the ray intersects the object.

	float rayConeSpreadAngleAtOrigin = eyeToPixelConeSpreadAngle.x;
	float rayConeWidthAtOrigin = 0;

	const float2 rayConeAtOrigin = float2(rayConeWidthAtOrigin, rayConeSpreadAngleAtOrigin);
	const float2 rayConeAtHit = float2(
	// New cone width should increase by 2*RayLength*tan(SpreadAngle/2), but RayLength*SpreadAngle is a close approximation
		rayConeAtOrigin.x+rayConeAtOrigin.y*length(hitPosition-viewOriginAndTanHalfFovY.xyz),
		rayConeAtOrigin.y+eyeToPixelConeSpreadAngle.x);

    const matrix matWorld = matrix(float4(1,0,0,0), float4(0,1,0,0), float4(0,0,1,0), float4(0,0,0,1));

	float rayConeWidthAtHitPoint = rayConeAtHit.x;

	float2 uvAreaFromCone = UVAreaFromRayCone(WorldRayDirection(), nrm, rayConeWidthAtHitPoint, aUVs, aPos, (float3x3)matWorld);
	

	uint2 vTexSize;
	tex.GetDimensions(vTexSize.x, vTexSize.y);
	float texLOD = UVAreaToTexLOD(vTexSize, uvAreaFromCone); //the mip level to use if texture.SampleLevel is called.

	//alternative is to calculate the uv derivatives.  Fill in the out parameter to return uv derivatives.
	uvDerivs = UVDerivsFromRayCone(WorldRayDirection(), nrm, rayConeWidthAtHitPoint, aUVs, aPos, (float3x3)matWorld);
	
	return texLOD;
}