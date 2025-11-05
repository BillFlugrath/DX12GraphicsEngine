#pragma once

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <wrl.h>
#include <atlcomcli.h>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxc/dxcapi.h>
#include <dxc/dxcapi.use.h>

using namespace DirectX;

#define ModelVertex   DXGraphicsUtilities::MeshVertexPosNormUV0

//
//--------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------

static bool CompareVector3WithEpsilon(const XMFLOAT3& lhs, const XMFLOAT3& rhs)
{
	const XMFLOAT3 vector3Epsilon = XMFLOAT3(0.00001f, 0.00001f, 0.00001f);
	return XMVector3NearEqual(XMLoadFloat3(&lhs), XMLoadFloat3(&rhs), XMLoadFloat3(&vector3Epsilon)) == TRUE;
}

static bool CompareVector2WithEpsilon(const XMFLOAT2& lhs, const XMFLOAT2& rhs)
{
	const XMFLOAT2 vector2Epsilon = XMFLOAT2(0.00001f, 0.00001f);
	return XMVector3NearEqual(XMLoadFloat2(&lhs), XMLoadFloat2(&rhs), XMLoadFloat2(&vector2Epsilon)) == TRUE;
}

//Deprecated, do not use.
struct ModelVertexExample
{
	XMFLOAT3 position;
	XMFLOAT2 uv;

	bool operator==(const ModelVertexExample& v) const {
		if (CompareVector3WithEpsilon(position, v.position)) {
			if (CompareVector2WithEpsilon(uv, v.uv)) return true;
			return true;
		}
		return false;
	}

	ModelVertexExample& operator=(const ModelVertexExample& v) {
		position = v.position;
		uv = v.uv;
		return *this;
	}
};


namespace DXGraphicsUtilities
{
	// Structures
	struct MeshVertexPosNormUV0
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT2 uv;

		bool operator==(const MeshVertexPosNormUV0& v) const {
			if (CompareVector3WithEpsilon(position, v.position)) {
				if (CompareVector2WithEpsilon(uv, v.uv)) return true;
				return true;
			}
			return false;
		}

		MeshVertexPosNormUV0& operator=(const MeshVertexPosNormUV0& v) {
			position = v.position;
			normal = v.normal;
			uv = v.uv;
			return *this;
		}
	};

	struct MeshVertexPosUV0
	{
		XMFLOAT3 position;
		XMFLOAT2 uv;
		bool operator==(const MeshVertexPosUV0& v) const {
			if (CompareVector3WithEpsilon(position, v.position)) {
				if (CompareVector2WithEpsilon(uv, v.uv)) return true;
				return true;
			}
			return false;
		}

		MeshVertexPosUV0& operator=(const MeshVertexPosUV0& v) {
			position = v.position;
			uv = v.uv;
			return *this;
		}
	};

	struct CloudVertexPosColor
	{
		XMFLOAT3 Pos;
		XMFLOAT4 Color;
	};

	// using our own vec2 and vec3 for model loading
	struct vec2
	{
		float x;
		float y;
	};

	struct vec3
	{
		float x;
		float y;
		float z;
	};

	struct vec4
	{
		float x;
		float y;
		float z;
		float w;
	};

	struct objVertex
	{
		float Position[3];
		float Normal[3];
		float TexCoord[2];
	};

	// Structures
	struct QuadVertex
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT2 Tex;
	};

}

