#pragma once
//#include "DXSampleHelper.h"

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

//See bills_dx_math_vector_notes.txt for explanation of using the DXMath SIMD and non SIMD
//data types.

class DXCamera
{
public:
	DXCamera();
	~DXCamera();

	XMMATRIX& GetViewMatrix()  { return mViewMatrix; }
	XMFLOAT4X4& GetView4x4f()  { return mView;}

	void SetProjectionMatrix(const XMMATRIX& proj);
	XMMATRIX& GetProjectionMatrix() { return mProjectionMatrix; }
	XMFLOAT4X4 GetProj4x4f() { return mProj; }

	void SetFOV(float fov_radians);
	float GetFOV() { return mFov; }
	void SetNearFarPlanes(float near_plane, float far_plane);
	void GetNearFarPlanes(float &near_plane, float &far_plane);
	void SetAspectRatio(float aspect_ratio) { mAspectRatio = aspect_ratio;  }
	void Update(); //update view and proj matrices

	XMFLOAT3 GetPosition() { return mPosition; }
	void SetPosition(float x_pos, float y_pos, float z_pos);

	// Define camera space via LookAt parameters.
	void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
	void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);

	// Strafe/Walk the camera a distance d.
	void Strafe(float d);
	void Walk(float d);

	// Rotate the camera.
	void Pitch(float angle);
	void Yaw(float angle);

protected:
	void ProcessKeyInput();
	void UpdateViewMatrix();

	// think of XMVECTOR as a proxy for a SIMD hardware register, and XMMATRIX as a proxy 
	// for a logical grouping of four SIMD hardware registers.
	XMMATRIX mViewMatrix;
	XMMATRIX mProjectionMatrix;
	float mAspectRatio;
	float mNearPlane;
	float mFarPlane;
	float mFov;

	//XMFLOAT3 is used for storage if vectors.  We convert to XMVECTOR to do calculations via SIMD
	//then, save back into XMFLOAT3 when done.
	XMFLOAT3 mPosition = { 0.0f, 0.0f, -2.0f }; 
	XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
	XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
	XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

	XMFLOAT4X4 mView;
	XMFLOAT4X4 mProj;

	float mPitch;
	float mYaw;
	
	//scale movement of camera
	float mPosMoveScale = 0.01f;
	float mYawAngle = 0.005f;
	float mPitchAngle = 0.005f;
};
