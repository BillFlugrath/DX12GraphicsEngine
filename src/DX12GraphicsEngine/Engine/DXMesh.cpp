#include "stdafx.h"
#include "DXMesh.h"
#include "DXCamera.h"

#include <stdio.h>
#include <string>
#include <cstring>
#include <iostream>
#include <fstream>

#include <assert.h>
#include <sstream>


using namespace std;
using namespace DirectX;




// constructor
DXMesh::DXMesh() :
	m_cbDescriptorIndex(0)
	, m_pConstantBufferData(nullptr)
	, m_ModelID(0)
	, m_bReceiveShadow(false)
	, m_pDXCamera(nullptr)
{
	
}



// destructor
DXMesh::~DXMesh()
{
   
}


bool DXMesh::LoadOBJ(
    const char *                           path,
    std::vector< DXGraphicsUtilities::vec3 > & out_vertices,
    std::vector< DXGraphicsUtilities::vec2 > & out_uvs,
    std::vector< DXGraphicsUtilities::vec3 > & out_normals,
    bool                                   bFlipWinding
    )
{
    printf( "Loading OBJ file %s...\n", path );

    std::vector< unsigned int > vertexIndices, uvIndices, normalIndices;
    std::vector< DXGraphicsUtilities::vec3 > temp_vertices;
    std::vector< DXGraphicsUtilities::vec2 > temp_uvs;
    std::vector< DXGraphicsUtilities::vec3 > temp_normals;

    // open the file and read it into char buffer
    std::ifstream infile;
    infile.open( path, std::ios::binary );
    infile.seekg( 0, std::ios::end );
    size_t fileSize = infile.tellg();
    std::vector< char > buffer; // used to store text data
    buffer.resize( fileSize );
    infile.seekg( 0, std::ios::beg );
    infile.read( &buffer[0], fileSize );

    std::string strBuffer( buffer.data(), fileSize );
    std::stringstream ss( strBuffer );

	const int scanf_buffer_sz = 512; //hack since we switched from sscanf to sscanf_s
    std::string strLine;
    while ( std::getline( ss, strLine, '\n' ) )
    {
        const char * lineHeader = strLine.c_str();
        char firstWord[scanf_buffer_sz];
        sscanf_s( lineHeader, "%s", firstWord, scanf_buffer_sz);

        // get second word in string.  this is start of actual data
        int    firstWordLength = static_cast< int >( strlen( firstWord ) );
        char * it              = const_cast< char * >( lineHeader ) + firstWordLength;
        while ( *it == ' ' || *it == '\t' || *it == '\0' )
        {
            it++;
        }

        char * secondWord = it;

        if ( strcmp( firstWord, "v" ) == 0 )
        {
            DXGraphicsUtilities::vec3 vertex;
            sscanf_s( secondWord, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
            temp_vertices.push_back( vertex );
        }
        else if ( strcmp( firstWord, "vt" ) == 0 )
        {
            DXGraphicsUtilities::vec2 uv;
            sscanf_s( secondWord, "%f %f\n", &uv.x, &uv.y);
            // uv.y = -uv.y; // Invert V coordinate since we will only use DDS texture, which are inverted. Remove if
            // you want to use TGA or BMP loaders.
            temp_uvs.push_back( uv );
        }
        else if ( strcmp( firstWord, "vn" ) == 0 )
        {
            DXGraphicsUtilities::vec3 normal;
            sscanf_s( secondWord, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
            temp_normals.push_back( normal );
        }
        else if ( strcmp( firstWord, "f" ) == 0 )
        {
            std::string  vertex1, vertex2, vertex3;
            unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
			int matches = sscanf_s(secondWord,
				"%d/%d/%d %d/%d/%d %d/%d/%d\n",
				&vertexIndex[0],
				&uvIndex[0],
				&normalIndex[0],
				&vertexIndex[1],
				&uvIndex[1],
				&normalIndex[1],
				&vertexIndex[2],
				&uvIndex[2],
				&normalIndex[2]);
            if ( matches != 9 )
            {
                printf( "File can't be read by our simple parser :-( Try exporting with other options\n" );
                return false;
            }
            if ( bFlipWinding )
            {
                vertexIndices.push_back( vertexIndex[0] );
                vertexIndices.push_back( vertexIndex[2] );
                vertexIndices.push_back( vertexIndex[1] );
            }
            else
            {
                vertexIndices.push_back( vertexIndex[0] );
                vertexIndices.push_back( vertexIndex[1] );
                vertexIndices.push_back( vertexIndex[2] );
            }

            uvIndices.push_back( uvIndex[0] );
            uvIndices.push_back( uvIndex[1] );
            uvIndices.push_back( uvIndex[2] );
            normalIndices.push_back( normalIndex[0] );
            normalIndices.push_back( normalIndex[1] );
            normalIndices.push_back( normalIndex[2] );
        }
        else
        {
            // Probably a comment, eat up the rest of the line
            // char stupidBuffer[1000];
            // fgets(stupidBuffer, 1000, file);
        }
    }


    // For each vertex of each triangle
    int numIndices = static_cast< int >( vertexIndices.size() );

    for ( int i = 0; i < numIndices; i++ )
    {
        // Get the indices of its attributes
        unsigned int vertexIndex = vertexIndices[i];
        unsigned int uvIndex     = uvIndices[i];
        unsigned int normalIndex = normalIndices[i];

        // Get the attributes thanks to the index
        DXGraphicsUtilities::vec3 vertex = temp_vertices[vertexIndex - 1];
        DXGraphicsUtilities::vec2 uv     = temp_uvs[uvIndex - 1];
        DXGraphicsUtilities::vec3 normal = temp_normals[normalIndex - 1];

        // Put the attributes in buffers
        out_vertices.push_back( vertex );
        out_uvs.push_back( uv );
        out_normals.push_back( normal );
    }

    return true;
}

HRESULT DXMesh::LoadModelFromFile(const char* filename, ID3D12Device* pd3dDevice)
{
    mpd3dDevice = pd3dDevice;

    std::vector< DXGraphicsUtilities::vec3 >* out_vertices = new std::vector< DXGraphicsUtilities::vec3 >();
    std::vector< DXGraphicsUtilities::vec2 >* out_uvs = new std::vector< DXGraphicsUtilities::vec2 >();
    std::vector< DXGraphicsUtilities::vec3 >* out_normals = new std::vector< DXGraphicsUtilities::vec3 >();

    bool bFlipWinding = false;

    // load data for the vertex position, normals, and uvs into speparate vectors
    bool bLoaded = LoadOBJ(
        filename,
        *out_vertices,
        *out_uvs,
        *out_normals,
        bFlipWinding);

    assert(bLoaded && "Failed to load obj file\n");
    assert(out_vertices->size() == out_uvs->size());

    int numberOfVertices = static_cast<int>(out_vertices->size());
    int numberOfIndices = numberOfVertices;


    int triangleCount = numberOfIndices / 3;

    // create an index array.  vertices are in the vector in proper order already

    std::vector<UINT> meshIndicesVector;

    int i = 0;
    for (i = 0; i < numberOfIndices; ++i)
    {
        meshIndicesVector.push_back(i);
        m_Indices.push_back(i);
		m_Indices32bit.push_back(i);
    }

    mNumIndices = numberOfIndices;

    //create vertex buffer, index buffer, vertexbuffer 
    CreateVertexAndIndexBuffers(pd3dDevice, *out_vertices, *out_uvs, *out_normals, meshIndicesVector);

    // delete the vectors
    delete  out_vertices;
    delete  out_uvs;
    delete  out_normals;

    return S_OK;
}

bool DXMesh::CreateVertexAndIndexBuffers(ID3D12Device  *pd3dDevice,
    const std::vector< DXGraphicsUtilities::vec3 >& vertices,
    const std::vector< DXGraphicsUtilities::vec2 >& uvs,
    const std::vector< DXGraphicsUtilities::vec3 >& normals,
    const std::vector<UINT>& meshIndices)
{
	int numVerts = (int)vertices.size();
	int sizeOfVert = sizeof(DXGraphicsUtilities::MeshVertexPosNormUV0);
	void* vertexData = (void*)vertices.data();
	int numTris = numVerts / 3;
	void* indexData = (void*)meshIndices.data(); //data is WORD ie 16 bit int

	// create a vertex array
	DXGraphicsUtilities::MeshVertexPosNormUV0* verts = new DXGraphicsUtilities::MeshVertexPosNormUV0[numVerts];

	// create an array of vertices.  we will submit this to D3D to create a D3D vertex buffer resource
	for (int i = 0; i < numVerts; ++i)
	{
		DXGraphicsUtilities::MeshVertexPosNormUV0 v =
		{ XMFLOAT3(vertices[i].x ,   vertices[i].y , vertices[i].z ),
			XMFLOAT3(normals[i].x, normals[i].y, normals[i].z),
			XMFLOAT2(uvs[i].x, uvs[i].y) };

		verts[i] = v;
		m_Vertices.push_back(v);
	}

	// Create and populate the vertex buffer
	{
		pd3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(numVerts * sizeOfVert),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_pVertexBuffer));

		UINT8* pMappedBuffer;
		CD3DX12_RANGE readRange(0, 0);
		m_pVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pMappedBuffer));
		memcpy(pMappedBuffer, verts, numVerts * sizeOfVert);
		m_pVertexBuffer->Unmap(0, nullptr);

		m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeOfVert;
		m_vertexBufferView.SizeInBytes = numVerts * sizeOfVert;
	}

	// Create and populate the index buffer
	{
		pd3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * numTris * 3),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_pIndexBuffer));

		UINT8* pMappedBuffer;
		CD3DX12_RANGE readRange(0, 0);
		m_pIndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pMappedBuffer));
		memcpy(pMappedBuffer, indexData, sizeof(uint32_t) * numTris * 3);
		m_pIndexBuffer->Unmap(0, nullptr);

		m_indexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		m_indexBufferView.SizeInBytes = sizeof(uint32_t) * numTris * 3;
	}

    return true;

}

HRESULT DXMesh::LoadModelFromFile( const char *          filename,
								   ComPtr<ID3D12Device>        pd3dDevice,
								   ComPtr<ID3D12DescriptorHeap> pCBVSRVHeap,
								   int cbDescriptorIndex,
                                   float                 scale )
{
    mpd3dDevice        = pd3dDevice;
	m_pCBVSRVHeap = pCBVSRVHeap;
	m_cbDescriptorIndex = cbDescriptorIndex;

    std::vector< DXGraphicsUtilities::vec3 > * out_vertices = new std::vector< DXGraphicsUtilities::vec3 >();
    std::vector< DXGraphicsUtilities::vec2 > * out_uvs      = new std::vector< DXGraphicsUtilities::vec2 >();
    std::vector< DXGraphicsUtilities::vec3 > * out_normals  = new std::vector< DXGraphicsUtilities::vec3 >();


    bool bFlipWinding = false;

    // load data for the vertex position, normals, and uvs into speparate vectors
    bool bLoaded = LoadOBJ(
        filename,
        *out_vertices,
        *out_uvs,
        *out_normals,
        bFlipWinding );

    assert( bLoaded && "Failed to load obj file\n" );
    assert( out_vertices->size() == out_uvs->size() );

    int numberOfVertices = static_cast< int >( out_vertices->size() );
    int numberOfIndices  = numberOfVertices;


    int triangleCount = numberOfIndices / 3;

    // create an index array.  vertices are in the vector in proper order already
   
	std::vector<WORD> meshIndicesVector;

    int i = 0;
    for ( i = 0; i < numberOfIndices; ++i )
    {
		meshIndicesVector.push_back(i);
        m_Indices.push_back(i);
		m_Indices32bit.push_back(i);
    }

    mNumIndices = numberOfIndices;

   
	//create vertex buffer, index buffer, vertexbuffer  view, index buffer view, constant buffer view
	CreateD3DResources(pd3dDevice, pCBVSRVHeap, m_cbDescriptorIndex, *out_vertices, *out_uvs, *out_normals, meshIndicesVector, scale);
	

    // delete the vectors
    delete  out_vertices;
    delete  out_uvs;
    delete  out_normals;

 

	return S_OK;
}

bool DXMesh::CreateD3DResources(ComPtr<ID3D12Device>        pDevice,
	ComPtr<ID3D12DescriptorHeap> pCBVSRVHeap,
	int cbDescriptorIndex,
	const std::vector< DXGraphicsUtilities::vec3 > &vertices,
	const std::vector< DXGraphicsUtilities::vec2 > & uvs,
	const std::vector< DXGraphicsUtilities::vec3 > & normals,
	const std::vector<WORD> & meshIndices,
	float scale)
{
	m_cbDescriptorIndex = cbDescriptorIndex;
	m_pCBVSRVHeap = pCBVSRVHeap;

	int numVerts = (int)vertices.size();
	int sizeOfVert = sizeof(DXGraphicsUtilities::MeshVertexPosNormUV0);
	void *vertexData = (void*)vertices.data();
	int numTris = numVerts/3;
	void *indexData = (void*)meshIndices.data(); //data is WORD ie 16 bit int

												 // create a vertex array
	DXGraphicsUtilities::MeshVertexPosNormUV0 * verts = new DXGraphicsUtilities::MeshVertexPosNormUV0[numVerts];

	
	// create an array of vertices.  we will submit this to D3D to create a D3D vertex buffer resource
	for (int i = 0; i < numVerts; ++i)
	{
		DXGraphicsUtilities::MeshVertexPosNormUV0 v =
		{ XMFLOAT3(vertices[i].x * scale,   vertices[i].y * scale, vertices[i].z * scale),
			XMFLOAT3(normals[i].x, normals[i].y, normals[i].z),
			XMFLOAT2(uvs[i].x, uvs[i].y) };

		verts[i] = v;
        m_Vertices.push_back(v);
	}

	// Create and populate the vertex buffer
	{
		pDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(numVerts *sizeOfVert),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_pVertexBuffer));

		UINT8 *pMappedBuffer;
		CD3DX12_RANGE readRange(0, 0);
		m_pVertexBuffer->Map(0, &readRange, reinterpret_cast< void** >(&pMappedBuffer));
		memcpy(pMappedBuffer, verts, numVerts *sizeOfVert);
		m_pVertexBuffer->Unmap(0, nullptr);

		m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeOfVert;
		m_vertexBufferView.SizeInBytes = numVerts *sizeOfVert;
	}

	// Create and populate the index buffer
	{
		pDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint16_t) * numTris * 3),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_pIndexBuffer));

		UINT8 *pMappedBuffer;
		CD3DX12_RANGE readRange(0, 0);
		m_pIndexBuffer->Map(0, &readRange, reinterpret_cast< void** >(&pMappedBuffer));
		memcpy(pMappedBuffer, indexData, sizeof(uint16_t) * numTris * 3);
		m_pIndexBuffer->Unmap(0, nullptr);

		m_indexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
		m_indexBufferView.SizeInBytes = sizeof(uint16_t) * numTris * 3;
	}



	// Create a constant buffer to hold the transform 
	{
		pDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_pConstantBuffer));

		// Keep as persistently mapped buffer, store left eye in first 256 bytes, right eye in second
		UINT8 *pBuffer;
		CD3DX12_RANGE readRange(0, 0);
		m_pConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pBuffer));
		m_pConstantBufferData = pBuffer;
		

		
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(m_pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbDescriptorIndex, pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_pConstantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(XMMATRIX) + 255) & ~255; // Pad to 256 bytes
		pDevice->CreateConstantBufferView(&cbvDesc, cbvHandle);

	}

	m_unVertexCount = numTris * 3;

	
	return true;
}

void DXMesh::Render(ComPtr<ID3D12GraphicsCommandList> & pCommandList, const XMMATRIX &matMVP)
{
	UINT nCBVSRVDescriptorSize = mpd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);;
	
	//copy mvp matrix data into the constant buffer
	DirectX::XMFLOAT4X4 mvp4x4;
	XMStoreFloat4x4(&mvp4x4, XMMatrixTranspose(matMVP));
	memcpy(m_pConstantBufferData, &mvp4x4, sizeof(mvp4x4));


	// Bind the CB
	int nStartOffset = 0;
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(m_pCBVSRVHeap->GetGPUDescriptorHandleForHeapStart());
	cbvHandle.Offset(nStartOffset + m_cbDescriptorIndex, nCBVSRVDescriptorSize);

	//cb descriptor is second parameter of the root
	int cb_root_parameter = 1;
	pCommandList->SetGraphicsRootDescriptorTable(cb_root_parameter, cbvHandle);

	// Bind the texture
	//CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(m_pCBVSRVHeap->GetGPUDescriptorHandleForHeapStart());
//	srvHandle.Offset(m_unTextureIndex, nCBVSRVDescriptorSize);
//	pCommandList->SetGraphicsRootDescriptorTable(1, srvHandle);

	// Bind the VB/IB and draw
	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCommandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	pCommandList->IASetIndexBuffer(&m_indexBufferView);
	pCommandList->DrawIndexedInstanced((UINT)m_unVertexCount, 1, 0, 0, 0);
}

void DXMesh::Render(ComPtr<ID3D12GraphicsCommandList>& pCommandList, const XMMATRIX& matWorld, const XMMATRIX& matMVP)
{
	UINT nCBVSRVDescriptorSize = mpd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);;

	//copy mvp matrix data into the constant buffer
	DirectX::XMFLOAT4X4 mvp4x4;
	XMStoreFloat4x4(&mvp4x4, XMMatrixTranspose(matMVP));

	DirectX::XMFLOAT4X4 world4x4;
	XMStoreFloat4x4(&world4x4, XMMatrixTranspose(matWorld));

	struct ObjectConstantBufferInShader
	{
		DirectX::XMFLOAT4X4 mvp4x4;
		DirectX::XMFLOAT4X4 world4x4;
		DirectX::XMFLOAT4 meshInfo; //x value is model index
		DirectX::XMFLOAT4 cameraPos;
	};

	XMFLOAT3 cameraPos3 = m_pDXCamera->GetPosition();
	XMFLOAT4 cameraPos4(cameraPos3.x, cameraPos3.y, cameraPos3.z, 1.0f);
	XMFLOAT4 meshInfo4((float)m_ModelID, m_bReceiveShadow ? 1.0f :0.0f , 0.0f ,0.0f);
	ObjectConstantBufferInShader cb{ mvp4x4, world4x4, meshInfo4 };

	memcpy(m_pConstantBufferData, &cb, sizeof(ObjectConstantBufferInShader));


	// Bind the CB
	int nStartOffset = 0;
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(m_pCBVSRVHeap->GetGPUDescriptorHandleForHeapStart());
	cbvHandle.Offset(nStartOffset + m_cbDescriptorIndex, nCBVSRVDescriptorSize);

	//cb descriptor is second parameter of the root
	int cb_root_parameter = 1;
	pCommandList->SetGraphicsRootDescriptorTable(cb_root_parameter, cbvHandle);

	// Bind the VB/IB and draw
	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCommandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	pCommandList->IASetIndexBuffer(&m_indexBufferView);
	pCommandList->DrawIndexedInstanced((UINT)m_unVertexCount, 1, 0, 0, 0);
}