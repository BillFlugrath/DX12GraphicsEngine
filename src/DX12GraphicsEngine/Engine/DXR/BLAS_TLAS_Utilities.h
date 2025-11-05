#pragma once

#include "Common.h"
#include "Structures.h"


using namespace DirectX;
using Microsoft::WRL::ComPtr;

class DXShaderUtilities;
class DXD3DUtilities;
class DXResourceUtilities;
struct D3DModel;


class BLAS_TLAS_Utilities
{
public:
	BLAS_TLAS_Utilities();
	~BLAS_TLAS_Utilities();

	void createAccelerationStructures(D3D12Global d3d, D3DSceneModels& d3dSceneModels);

	ID3D12Resource* GetTLASBuffer() { return mTopLevelBuffers.pResult; }
	ID3D12Resource** GetBLASBufferArray() { return mpBottomLevelAS; }

	AccelerationStructureBuffer* GetBottomLevelASBufferArray() { return mBottomLevelBuffers; }
	AccelerationStructureBuffer GetTopLevelASBuffer() { return mTopLevelBuffers; }

protected:
	ID3D12Resource* createBuffer(ID3D12Device5* pDevice, uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps);

	AccelerationStructureBuffer createBottomLevelAS(ID3D12Device5* pDevice, ID3D12GraphicsCommandList4* pCmdList,
		ID3D12Resource* pVB[],  uint32_t *vertexCount, uint32_t geometryCount);

	AccelerationStructureBuffer createTopLevelAS(ID3D12Device5* pDevice, ID3D12GraphicsCommandList4* pCmdList, 
		ID3D12Resource* pBottomLevelAS[2], uint64_t& tlasSize, D3DModel *d3dModels, uint32_t num_models);

	uint32_t mNumberBLASBuffers;
	AccelerationStructureBuffer *mBottomLevelBuffers; //array of array of ptrs to blas buffers.  results cached in mpBottomLevelAS array
	ID3D12Resource** mpBottomLevelAS; //array of ptrs to blas buffers.  one buffer per model.

	//only supports one top level buffer currently ie only one AccelerationStructureBuffer struct.
	//TODO add support for multiple TLAS structs.  These would then need to be set properly through an srv
	//so that the raygeneration shader could properly dispatch rays into it.
	uint64_t mTlasSize = 0; //out param gets set as size of TLAS on GPU
	AccelerationStructureBuffer mTopLevelBuffers;  
};
