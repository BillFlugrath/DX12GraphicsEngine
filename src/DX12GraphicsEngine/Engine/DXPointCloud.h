#pragma once
#include "DXGraphicsUtilities.h"
using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

#include "DXMesh.h"
#include <vector>

class DXCamera;

class DXPointCloud : public DXMesh
{
public:
	DXPointCloud();
	virtual ~DXPointCloud();

	static void SetDebugVizDepthBuffer(bool bVizDepthBuffer) { mDebugVizDepthBuffer = bVizDepthBuffer; }

	HRESULT LoadPointCloudFromFile(const char* filename,
		ComPtr<ID3D12Device>        pd3dDevice,
		ComPtr<ID3D12DescriptorHeap> pCBVSRVHeap,
		int cbDescriptorIndex,
		DXGraphicsUtilities::vec3& scale,
		bool bSwitchYZAxes);

	void Update(DXCamera* pCamera);

	void RenderPointCloud(ComPtr<ID3D12GraphicsCommandList>& pCommandList, const XMMATRIX& matMVP);
	void RenderPointSpriteCloud(ComPtr<ID3D12GraphicsCommandList>& pCommandList, 
		const XMMATRIX& matWVP, const XMMATRIX& matVP, const XMMATRIX& matView);

	DXGraphicsUtilities::BoundingBox& GetBoundingBox() { return mBBox;}
	void CreateBoxPointCloudFile(const char* filename, float box_size);

	void SetUseCPUPointSort(bool bUseCPUSort) { mbUseCPUPointSort = bUseCPUSort; }
	XMFLOAT2& GetQuadSize() { return  mQuadSize; }

	static ComPtr<ID3D12RootSignature>& GetProcessingRootSignature() {
		return m_PointCloudProcessRootSignature;
	}
	static ComPtr<ID3D12PipelineState>& GetProcessingPipelineState() {
		return m_PointCloudProcessPipelineState;
	}
	
	
protected:
	bool DXPointCloud::LoadPLY(const char* path, std::vector< DXGraphicsUtilities::vec3 >& out_vertices, 
								std::vector< DXGraphicsUtilities::vec4 >& out_colors);


	//create vertex buffer, index buffer, vertexbuffer  view, index buffer view, constant buffer view
	bool CreateD3DResources(ComPtr<ID3D12Device>        pDevice,
		ComPtr<ID3D12DescriptorHeap> pCBVSRVHeap,
		int cbDescriptorIndex,
		const std::vector< DXGraphicsUtilities::vec3 >& vertices,
		const std::vector< DXGraphicsUtilities::vec4 >& colors,
		const std::vector<WORD>& meshIndices,
		DXGraphicsUtilities::vec3& scale);

	static void CreateProcessingRootSignature(ComPtr<ID3D12Device> pDevice);
	static void CreateProcessingPipelineState(ComPtr<ID3D12Device> pDevice);

	void UpdateBoundingBox();
	void SortPointCloud(DXCamera* pCamera);

	DXGraphicsUtilities::BoundingBox mBBox;
	std::vector< DXGraphicsUtilities::CloudVertexPosColor> mvCloudVertices;

	void UpdateShaderData(const XMMATRIX& matWVP, const XMMATRIX& matVP, const XMMATRIX& view);

	struct PointSpriteShaderData
	{
		XMFLOAT4X4 g_WVPMatrix;
		XMFLOAT4X4 gViewProj;
		XMFLOAT4X4 gView;
		XMFLOAT4X4 gInvView;
		XMFLOAT4 gEyePosW;
		XMFLOAT4 gQuadSize; //only uses first 2 floats
	};

	PointSpriteShaderData mShaderData;

	static ComPtr<ID3D12RootSignature> m_PointCloudProcessRootSignature;
	static ComPtr<ID3D12PipelineState> m_PointCloudProcessPipelineState;

	DXCamera *m_pDXCamera=nullptr;

	//Debug
	XMFLOAT2 mQuadSize = { 0.00500f, 0.0050f };

	bool mbSwitchYZAxesOnPLYFileLoad = false;  //Scaniverse created .ply files need this set to true
	bool mbUseCPUPointSort = false;  //sort based on point distance to camera
	float mDebugBoxPointCloudResolution = 0.005f;  //spacing between points in box shaped cloud
	float mDebugPointCloudBoxSize = 0.5f;  //dimension of a side of box
	bool mbDebugFrontFaceWriteOnly = false;  //write only z plane of cube to file
	bool mbDebugCreateBoxPointCloud = false;
	static bool mDebugVizDepthBuffer;
};

