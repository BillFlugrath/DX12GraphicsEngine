
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

bool IsInCone(float3 P, float3 X, float radius, float height, float3 dir)
{
	float3 XtoP = P - X;
	float d = dot(dir, XtoP);

	if (d<0 || d>height)
		return false;

	float r = (d / height) * radius;

	float3 parallel = d * dir;
	float3 perpendicular = XtoP - parallel;
	float perpen_mag = length(perpendicular);

	if (perpen_mag > r)
		return false;

	return true;
}