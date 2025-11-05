#pragma once
#include "DXGraphicsUtilities.h"
using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

#include <vector>


class DXMesh
{
public:
    DXMesh();
    virtual ~DXMesh();

  
	void Render(ComPtr<ID3D12GraphicsCommandList> & pCommandList, const XMMATRIX &matMVP);

	HRESULT LoadModelFromFile(const char *          filename,
		ID3D12Device* pd3dDevice);

	HRESULT LoadModelFromFile(const char* filename,
		ComPtr<ID3D12Device>        pd3dDevice,
		ComPtr<ID3D12DescriptorHeap> pCBVSRVHeap,
		int cbDescriptorIndex,
		float                 scale);


	//get indices
	std::vector< uint16_t >& GetIndices() { return m_Indices; }
	std::vector< uint32_t >& GetIndices32Bit() { return m_Indices32bit; }

	//get vertices
	std::vector< DXGraphicsUtilities::MeshVertexPosNormUV0 > GetVertices() { return m_Vertices; }

	ComPtr< ID3D12Resource > GetVertexBuffer() { return m_pVertexBuffer; }
	ComPtr< ID3D12Resource > GetIndexBuffer() { return m_pIndexBuffer; }

	D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() { return m_vertexBufferView; }
	D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() { return m_indexBufferView; }

protected:
    bool LoadOBJ( const char *                           path,
                  std::vector< DXGraphicsUtilities::vec3 > & out_vertices,
                  std::vector< DXGraphicsUtilities::vec2 > & out_uvs,
                  std::vector< DXGraphicsUtilities::vec3 > & out_normals,
                  bool                                   bFlipWinding );

	//create vertex buffer, index buffer, vertexbuffer  view, index buffer view, constant buffer view
	bool CreateD3DResources(ComPtr<ID3D12Device>        pd3dDevice,
		ComPtr<ID3D12DescriptorHeap> pCBVSRVHeap,
		int cbDescriptorIndex,
		const std::vector< DXGraphicsUtilities::vec3 > &vertices,
		const std::vector< DXGraphicsUtilities::vec2 > & uvs,
		const std::vector< DXGraphicsUtilities::vec3 > & normals,
		const std::vector<WORD> & meshIndices,
		float scale);

	//Create index and vertex buffers.  Note: USES 32 bit UINT INDICES !
	bool CreateVertexAndIndexBuffers(ID3D12Device* pd3dDevice,
		const std::vector< DXGraphicsUtilities::vec3 >& vertices,
		const std::vector< DXGraphicsUtilities::vec2 >& uvs,
		const std::vector< DXGraphicsUtilities::vec3 >& normals,
		const std::vector<UINT>& meshIndices);


    // the device 
	ComPtr<ID3D12Device>       mpd3dDevice;

	ComPtr< ID3D12Resource > m_pVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	ComPtr< ID3D12Resource > m_pIndexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

	ComPtr< ID3D12Resource > m_pConstantBuffer;
	UINT8 *m_pConstantBufferData; 
	size_t m_unVertexCount;
	size_t mNumIndices;
	ComPtr<ID3D12DescriptorHeap> m_pCBVSRVHeap;
	std::string m_sModelName;

	int m_cbDescriptorIndex;

	//store vertices and indices in vectors for easy debugging
	std::vector< uint16_t > m_Indices;
	std::vector< uint32_t > m_Indices32bit;
	std::vector< DXGraphicsUtilities::MeshVertexPosNormUV0 > m_Vertices;
};

