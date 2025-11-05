#include "stdafx.h"
#include "DXCamera.h"
#include "d3dUtil.h"

DXCamera::DXCamera():
	 mAspectRatio(1.33f)
	,mPitch(0.0f)
	,mYaw(0.0f)
{
	
	mFov = 0.55f*3.14f;
	mNearPlane = 0.01f;
	mFarPlane = 100.0f;

	Update();
}

DXCamera::~DXCamera()
{
}


void DXCamera::SetFOV(float fov_radians)
{
	mFov = fov_radians;
}
void DXCamera::SetNearFarPlanes(float near_plane, float far_plane)
{
	mNearPlane = near_plane;
	mFarPlane = far_plane;
}

void DXCamera::GetNearFarPlanes(float& near_plane, float& far_plane)
{
	near_plane = mNearPlane;
	far_plane = mFarPlane;
}

void DXCamera::ProcessKeyInput()
{
	if (d3dUtil::IsKeyDown(0x52)) //R
	{
		mPosition = XMFLOAT3{ 0.0f, 0.0f, -2.0f };
		mRight = XMFLOAT3{ 1.0f, 0.0f, 0.0f };
	    mUp = XMFLOAT3{ 0.0f, 1.0f, 0.0f };
		mLook = XMFLOAT3{ 0.0f, 0.0f, 1.0f };

		mPosMoveScale = 0.01f;
		mYawAngle = 0.001f;
		mPitchAngle = 0.001f;
	}
	else if (d3dUtil::IsKeyDown(0x44)) //D
	{
		XMVECTOR vecRight = XMLoadFloat3(&mRight);
		XMVECTOR offsetRight = XMVectorScale(vecRight, mPosMoveScale);
		XMVECTOR pos = XMLoadFloat3(&mPosition);
		XMVECTOR finalPos = pos + offsetRight;
		XMStoreFloat3(&mPosition, finalPos);
	}
	else if (d3dUtil::IsKeyDown(0x41)) //A
	{
		XMVECTOR vecRight = XMLoadFloat3(&mRight);
		XMVECTOR offsetRight = XMVectorScale(vecRight, -mPosMoveScale);
		XMVECTOR pos = XMLoadFloat3(&mPosition);
		XMVECTOR finalPos = pos + offsetRight;
		XMStoreFloat3(&mPosition, finalPos);
	}
	else if (d3dUtil::IsKeyDown(0x57)) //W
	{
		XMVECTOR vecLook = XMLoadFloat3(&mLook);
		XMVECTOR offsetLook= XMVectorScale(vecLook, mPosMoveScale);
		XMVECTOR pos = XMLoadFloat3(&mPosition);
		XMVECTOR finalPos = pos + offsetLook;
		XMStoreFloat3(&mPosition, finalPos);
	}
	else if (d3dUtil::IsKeyDown(0x53)) //S
	{
		XMVECTOR vecLook = XMLoadFloat3(&mLook);
		XMVECTOR offsetLook = XMVectorScale(vecLook, -mPosMoveScale);
		XMVECTOR pos = XMLoadFloat3(&mPosition);
		XMVECTOR finalPos = pos + offsetLook;
		XMStoreFloat3(&mPosition, finalPos);
	}
	else if (d3dUtil::IsKeyDown(0x01) || d3dUtil::IsKeyDown(0x02)) //right mouse
	{
		if (d3dUtil::IsKeyDown(0x25)) //left arrow
		{
			Yaw(-mYawAngle);
		}
		else if (d3dUtil::IsKeyDown(0x27)) //right arrow arrow
		{
			Yaw(mYawAngle);
		}
		else if (d3dUtil::IsKeyDown(0x26)) //up arrow
		{
			Pitch(-mPitchAngle);
		}
		else if (d3dUtil::IsKeyDown(0x28)) //down arrow 
		{
			Pitch(mPitchAngle);
		}
	}
	else if (d3dUtil::IsKeyDown(0x26)) //up arrow
	{
		XMVECTOR vecUp = XMLoadFloat3(&mUp);
		XMVECTOR offsetUp = XMVectorScale(vecUp, mPosMoveScale);
		XMVECTOR pos = XMLoadFloat3(&mPosition);
		XMVECTOR finalPos = pos + offsetUp;
		XMStoreFloat3(&mPosition, finalPos);
	}
	else if (d3dUtil::IsKeyDown(0x28)) //down arrow 
	{
		XMVECTOR vecUp = XMLoadFloat3(&mUp);
		XMVECTOR offsetUp = XMVectorScale(vecUp, -mPosMoveScale);
		XMVECTOR pos = XMLoadFloat3(&mPosition);
		XMVECTOR finalPos = pos + offsetUp;
		XMStoreFloat3(&mPosition, finalPos);
	}
}

void DXCamera::Update()
{
	ProcessKeyInput();

	// Build the view matrix.
	UpdateViewMatrix();

	mProjectionMatrix = XMMatrixPerspectiveFovLH(mFov, mAspectRatio, mNearPlane, mFarPlane);
	XMStoreFloat4x4(&mProj, mProjectionMatrix);

	//XMVECTOR pos = XMVectorSet(mPosition.x, mPosition.y, mPosition.z, 1.0f);
	//XMVECTOR target = XMVectorZero();
	//XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	//mViewMatrix = XMMatrixLookAtLH(pos, target, up);
}

void DXCamera::SetPosition(float x_pos, float y_pos, float z_pos)
{
	mPosition.x = x_pos;
	mPosition.y = y_pos;
	mPosition.z = z_pos;
}

void DXCamera::SetProjectionMatrix(const XMMATRIX & proj)
{
	mProjectionMatrix = proj;
	XMStoreFloat4x4(&mProj, proj);
}


void DXCamera::UpdateViewMatrix()
{
	XMVECTOR R = XMLoadFloat3(&mRight);
	XMVECTOR U = XMLoadFloat3(&mUp);
	XMVECTOR L = XMLoadFloat3(&mLook);
	XMVECTOR P = XMLoadFloat3(&mPosition);

	// Keep camera's axes orthogonal to each other and of unit length.
	L = XMVector3Normalize(L);
	U = XMVector3Normalize(XMVector3Cross(L, R));

	// U, L already ortho-normal, so no need to normalize cross product.
	R = XMVector3Cross(U, L);

	// Fill in the view matrix entries.
	float x = -XMVectorGetX(XMVector3Dot(P, R));
	float y = -XMVectorGetX(XMVector3Dot(P, U));
	float z = -XMVectorGetX(XMVector3Dot(P, L));

	XMStoreFloat3(&mRight, R);
	XMStoreFloat3(&mUp, U);
	XMStoreFloat3(&mLook, L);


	mView(0, 0) = mRight.x;
	mView(1, 0) = mRight.y;
	mView(2, 0) = mRight.z;
	mView(3, 0) = x;

	mView(0, 1) = mUp.x;
	mView(1, 1) = mUp.y;
	mView(2, 1) = mUp.z;
	mView(3, 1) = y;

	mView(0, 2) = mLook.x;
	mView(1, 2) = mLook.y;
	mView(2, 2) = mLook.z;
	mView(3, 2) = z;

	mView(0, 3) = 0.0f;
	mView(1, 3) = 0.0f;
	mView(2, 3) = 0.0f;
	mView(3, 3) = 1.0f;

	mViewMatrix = XMLoadFloat4x4(&mView);
}

void DXCamera::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
{
	XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
	XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
	XMVECTOR U = XMVector3Cross(L, R);

	XMStoreFloat3(&mPosition, pos);
	XMStoreFloat3(&mLook, L);
	XMStoreFloat3(&mRight, R);
	XMStoreFloat3(&mUp, U);

}

void DXCamera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up)
{
	XMVECTOR P = XMLoadFloat3(&pos);
	XMVECTOR T = XMLoadFloat3(&target);
	XMVECTOR U = XMLoadFloat3(&up);

	LookAt(P, T, U);
}


void DXCamera::Strafe(float d)
{
	// mPosition += d*mRight
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR r = XMLoadFloat3(&mRight);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));
}

void DXCamera::Walk(float d)
{
	// mPosition += d*mLook
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR l = XMLoadFloat3(&mLook);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));
}

void DXCamera::Pitch(float angle)
{
	// Define the axis (e.g., a vector pointing up)
	XMVECTOR rotationAxis = XMLoadFloat3(&mRight);

	float rotationAngle = angle;

	// Create the rotation matrix for an arbitrary axis
	XMMATRIX rotationMatrix = XMMatrixRotationAxis(rotationAxis, rotationAngle);

	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), rotationMatrix));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), rotationMatrix));

	/*
	// Rotate up and look vector about the right vector.

	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);

	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
	*/

}

void DXCamera::Yaw(float angle)
{
	// Define the axis (e.g., a vector pointing up)
	XMVECTOR rotationAxis = XMLoadFloat3(&mUp);

	float rotationAngle = angle;

	// Create the rotation matrix for an arbitrary axis
	XMMATRIX rotationMatrix = XMMatrixRotationAxis(rotationAxis, rotationAngle);

	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mRight), rotationMatrix));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), rotationMatrix));

	/*
	// Rotate the basis vectors about the world y-axis.
	XMMATRIX R = XMMatrixRotationY(angle);

	XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
	*/
}