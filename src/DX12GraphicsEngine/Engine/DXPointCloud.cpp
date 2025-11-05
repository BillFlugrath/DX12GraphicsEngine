#include "stdafx.h"
#include "DXPointCloud.h"
#include "DXCamera.h"

#include <stdio.h>
#include <string>
#include <cstring>
#include <iostream>
#include <fstream>

#include <assert.h>
#include <sstream>
#include <algorithm>
#include <functional> // Required for std::greater

using namespace std;
using namespace DirectX;

ComPtr<ID3D12RootSignature> DXPointCloud::m_PointCloudProcessRootSignature = nullptr;
ComPtr<ID3D12PipelineState> DXPointCloud::m_PointCloudProcessPipelineState = nullptr;

bool DXPointCloud::mDebugVizDepthBuffer = false;

// constructor
DXPointCloud::DXPointCloud() 
{
	if (mbDebugCreateBoxPointCloud)
		CreateBoxPointCloudFile(kBoxPointCloudFile.c_str(), mDebugPointCloudBoxSize);
}

// destructor
DXPointCloud::~DXPointCloud()
{
   
}

void DXPointCloud::Update(DXCamera* pCamera)
{
	if (!pCamera) return;

	if (mbUseCPUPointSort)
	{
		SortPointCloud(pCamera);

		UINT8* pMappedBuffer;
		CD3DX12_RANGE readRange(0, 0);
		m_pVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pMappedBuffer));

		int sizeOfVert = sizeof(DXGraphicsUtilities::CloudVertexPosColor);
		void* vertexData = (void*)mvCloudVertices.data();
		memcpy(pMappedBuffer, vertexData, mvCloudVertices.size() * sizeOfVert); //copy vertices into GPU VB

		m_pVertexBuffer->Unmap(0, nullptr);
	}

	m_pDXCamera = pCamera;
}

void DXPointCloud::SortPointCloud(DXCamera* pCamera)
{
	using namespace DXGraphicsUtilities;

	std::sort(mvCloudVertices.begin(), mvCloudVertices.end(), [pCamera](CloudVertexPosColor& p0, CloudVertexPosColor& p1) {
		
		XMFLOAT3 cam_pos3=pCamera->GetPosition();
		XMVECTOR vCamPos= XMLoadFloat3(&cam_pos3);
		
		XMVECTOR vecPos0 = XMLoadFloat3(&p0.Pos);
		XMVECTOR vecPos1= XMLoadFloat3(&p1.Pos);

		XMVECTOR vectorSub0 = XMVectorSubtract(vCamPos, vecPos0); // Get difference vector
		XMVECTOR length0 = XMVector3Length(vectorSub0); // Calculate length
		float distance0 = 0.0f;
		XMStoreFloat(&distance0, length0); // Store the length into the float variable

		XMVECTOR vectorSub1 = XMVectorSubtract(vCamPos, vecPos1); // Get difference vector
		XMVECTOR length1 = XMVector3Length(vectorSub1); // Calculate length
		float distance1 = 0.0f;
		XMStoreFloat(&distance1, length1); // Store the length into the float variable

		return distance0 > distance1;

		});
}

void DXPointCloud::UpdateBoundingBox()
{
	float minPosX = FLT_MAX;
	float minPosY = FLT_MAX;
	float minPosZ = FLT_MAX;
	float maxPosX = FLT_MIN;
	float maxPosY = FLT_MIN;
	float maxPosZ = FLT_MIN;

	for (auto point : mvCloudVertices)
	{
		minPosX = point.Pos.x < minPosX ? point.Pos.x : minPosX;
		minPosY = point.Pos.y < minPosY ? point.Pos.y : minPosY;
		minPosZ = point.Pos.z < minPosZ ? point.Pos.z : minPosZ;

		maxPosX = point.Pos.x > maxPosX ? point.Pos.x : maxPosX;
		maxPosY = point.Pos.y > maxPosY ? point.Pos.y : maxPosY;
		maxPosZ = point.Pos.z > maxPosZ ? point.Pos.z : maxPosZ;
	}

	mBBox.mMin.x = minPosX; mBBox.mMin.y = minPosY; mBBox.mMin.z = minPosZ;
	mBBox.mMax.x = maxPosX; mBBox.mMax.y = maxPosY; mBBox.mMax.z = maxPosZ;
}

HRESULT DXPointCloud::LoadPointCloudFromFile( const char *          filename,
								   ComPtr<ID3D12Device>        pd3dDevice,
								   ComPtr<ID3D12DescriptorHeap> pCBVSRVHeap,
								   int cbDescriptorIndex,
								   DXGraphicsUtilities::vec3& scale,
								   bool bSwitchYZAxes)
{

    mpd3dDevice        = pd3dDevice;
	m_pCBVSRVHeap = pCBVSRVHeap;
	m_cbDescriptorIndex = cbDescriptorIndex;
	mbSwitchYZAxesOnPLYFileLoad = bSwitchYZAxes;

    std::vector< DXGraphicsUtilities::vec3 > * out_vertices = new std::vector< DXGraphicsUtilities::vec3 >();
    std::vector< DXGraphicsUtilities::vec4 > * out_colors     = new std::vector< DXGraphicsUtilities::vec4 >();

    // load data for the vertex position  and colors into separate vectors
    bool bLoaded = LoadPLY( filename, *out_vertices, *out_colors);

    assert( bLoaded && "Failed to load ply file\n" );
    assert( out_vertices->size() == out_colors->size() );

    int numberOfVertices = static_cast< int >( out_vertices->size() );
    int numberOfIndices  = numberOfVertices;

    // create an index array.  vertices are in the vector in proper order already
	std::vector<WORD> meshIndicesVector;

    int i = 0;
    for ( i = 0; i < numberOfIndices; ++i )
    {
		meshIndicesVector.push_back(i);
    }

	// Switch y and z axes save the final vertices for CPU analysis if needed
	for (i = 0; i < numberOfVertices; ++i)
	{
		if (mbSwitchYZAxesOnPLYFileLoad)
		{
			float temp = (*out_vertices)[i].y;
			(*out_vertices)[i].y = (*out_vertices)[i].z;
			(*out_vertices)[i].z = temp;
		}
	

		DXGraphicsUtilities::CloudVertexPosColor v;
		v.Pos = XMFLOAT3{ (*out_vertices)[i].x, (*out_vertices)[i].y, (*out_vertices)[i].z }; 
		v.Color = XMFLOAT4{ (*out_colors)[i].x,  (*out_colors)[i].y,  (*out_colors)[i].z, 
							 (*out_colors)[i].w };
		
		//scale position of point
		v.Pos.x *= scale.x; v.Pos.y *= scale.y; v.Pos.z *= scale.z;
		mvCloudVertices.push_back(v);
	}
	
	
	//create vertex buffer, index buffer, vertexbuffer  view, index buffer view, constant buffer view
	CreateD3DResources(pd3dDevice, pCBVSRVHeap, m_cbDescriptorIndex, *out_vertices, *out_colors, meshIndicesVector, scale);
	
	UpdateBoundingBox();

    // delete the vectors
    delete  out_vertices;
	delete  out_colors;

	return S_OK;
}

bool DXPointCloud::LoadPLY(
    const char* path,
    std::vector< DXGraphicsUtilities::vec3 >& out_vertices,
	std::vector< DXGraphicsUtilities::vec4 >& out_colors
)
{
    printf("Loading Ply file %s...\n", path);

    std::vector< DXGraphicsUtilities::vec3 > temp_vertices;


    // open the file and read it into char buffer
    std::ifstream infile;
    infile.open(path, std::ios::binary);
    infile.seekg(0, std::ios::end);
    size_t fileSize = infile.tellg();
    std::vector< char > buffer; // used to store text data
    buffer.resize(fileSize);
    infile.seekg(0, std::ios::beg);
    infile.read(&buffer[0], fileSize);

    std::string strBuffer(buffer.data(), fileSize);
    std::stringstream ss(strBuffer);

    const int scanf_buffer_sz = 512; //hack since we switched from sscanf to sscanf_s
    std::string strLine;

	int numPoints = 0;
	bool bReadingData = false;

    while (std::getline(ss, strLine, '\n'))
    {
        const char* lineHeader = strLine.c_str();
        char firstWord[scanf_buffer_sz];
		char cSecondWord[64];
        sscanf_s(lineHeader, "%s", firstWord, scanf_buffer_sz);
		

		if (strcmp(firstWord, "element") == 0)
		{
			// get second word in string.  this is start of actual data
			int    firstWordLength = static_cast<int>(strlen(firstWord));
			char* it = const_cast<char*>(lineHeader) + firstWordLength;
			while (*it == ' ' || *it == '\t' || *it == '\0')
			{
				it++;
			}

			sscanf_s(it, "%s", cSecondWord,64);
			if (strcmp(cSecondWord, "vertex") == 0)
			{
				int secondWordLength = static_cast<int>(strlen(cSecondWord));

				it = const_cast<char*>(lineHeader) + firstWordLength + secondWordLength+1;
				while (*it == ' ' || *it == '\t' || *it == '\0')
				{
					it++;
				}

				sscanf_s(it, "%d", &numPoints);
			}
		}
		
		if (strcmp(firstWord, "end_header") == 0)
		{
			bReadingData = true;
			continue; //go to next line ie back to top of while loop
		}

		if (bReadingData)
		{
			DXGraphicsUtilities::vec3 pos;
			DXGraphicsUtilities::vec4 color;
			sscanf_s(lineHeader, "%f %f %f %f %f %f %f\n", &pos.x, &pos.y, &pos.z, &color.x, &color.y, &color.z, &color.w);
			out_vertices.push_back(pos);
			out_colors.push_back(color);
		}
      
    }

    return true;
}

bool DXPointCloud::CreateD3DResources(ComPtr<ID3D12Device>        pDevice,
	ComPtr<ID3D12DescriptorHeap> pCBVSRVHeap,
	int cbDescriptorIndex,
	const std::vector< DXGraphicsUtilities::vec3 >& vertices,
	const std::vector< DXGraphicsUtilities::vec4 >& colors,
	const std::vector<WORD>& meshIndices,
	DXGraphicsUtilities::vec3& scale)
{
	m_cbDescriptorIndex = cbDescriptorIndex;
	m_pCBVSRVHeap = pCBVSRVHeap;

	int numVerts = (int)vertices.size();
	int numIndices = static_cast<int>(meshIndices.size());
	int sizeOfVert = sizeof(DXGraphicsUtilities::CloudVertexPosColor);
	void* vertexData = (void*)vertices.data();
	void* indexData = (void*)meshIndices.data(); //data is WORD ie 16 bit int

	assert(colors.size() == vertices.size());

	// create a vertex array
	DXGraphicsUtilities::CloudVertexPosColor* verts = new DXGraphicsUtilities::CloudVertexPosColor[numVerts];

	// create an array of vertices.  we will submit this to D3D to create a D3D vertex buffer resource
	for (int i = 0; i < numVerts; ++i)
	{
		DXGraphicsUtilities::CloudVertexPosColor v =
		{ XMFLOAT3(vertices[i].x,  vertices[i].y, vertices[i].z),
			XMFLOAT4(colors[i].x/255.0f,colors[i].y / 255.0f, colors[i].z / 255.0f, colors[i].w / 255.0f)};

		verts[i] = v; //store vertex in heap array
	}

	// Create and populate the vertex buffer
	{
		pDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(numVerts * sizeOfVert),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_pVertexBuffer));

		UINT8* pMappedBuffer;
		CD3DX12_RANGE readRange(0, 0);
		m_pVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pMappedBuffer));
		memcpy(pMappedBuffer, verts, numVerts * sizeOfVert); //copy vertices into GPU VB
		m_pVertexBuffer->Unmap(0, nullptr);

		m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeOfVert;
		m_vertexBufferView.SizeInBytes = numVerts * sizeOfVert;
	}

	// Create and populate the index buffer
	{
		pDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint16_t) * numIndices),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_pIndexBuffer));

		UINT8* pMappedBuffer;
		CD3DX12_RANGE readRange(0, 0);
		m_pIndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pMappedBuffer));
		memcpy(pMappedBuffer, indexData, sizeof(uint16_t) * numIndices);  //copy indices into GPU IB
		m_pIndexBuffer->Unmap(0, nullptr);

		m_indexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
		m_indexBufferView.SizeInBytes = sizeof(uint16_t) * numIndices;
	}


	// Create a constant buffer to hold the global shader data 
	{
		pDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64), //min size is PointSpriteShaderData
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_pConstantBuffer));

		// Keep as persistently mapped buffer
		UINT8* pBuffer;
		CD3DX12_RANGE readRange(0, 0);
		m_pConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pBuffer));
		m_pConstantBufferData = pBuffer;


		//get a handle in the srv-cbv-uav descriptor heap
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(m_pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbDescriptorIndex, pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		
		//create a buffer view attached to the descriptor
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_pConstantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(PointSpriteShaderData) + 255) & ~255; // Pad to 256 bytes

		pDevice->CreateConstantBufferView(&cbvDesc, cbvHandle);
	}

	m_unVertexCount = numVerts;

	delete verts;

	CreateProcessingRootSignature(pDevice);

	CreateProcessingPipelineState(pDevice);

	return true;
}

void DXPointCloud::RenderPointCloud(ComPtr<ID3D12GraphicsCommandList> & pCommandList, const XMMATRIX &matMVP)
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
	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	pCommandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	pCommandList->IASetIndexBuffer(&m_indexBufferView);
	pCommandList->DrawIndexedInstanced((UINT)m_unVertexCount, 1, 0, 0, 0);
}

void DXPointCloud::RenderPointSpriteCloud(ComPtr<ID3D12GraphicsCommandList>& pCommandList,
	const XMMATRIX& matWVP, const XMMATRIX& matVP, const XMMATRIX& matView)
{
	UpdateShaderData(matWVP, matVP, matView);

	UINT nCBVSRVDescriptorSize = mpd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);;

	uint8_t* pCBData = m_pConstantBufferData;

	// Bind the CB
	int nStartOffset = 0;
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(m_pCBVSRVHeap->GetGPUDescriptorHandleForHeapStart());
	cbvHandle.Offset(nStartOffset + m_cbDescriptorIndex, nCBVSRVDescriptorSize);

	//cb descriptor is second parameter of the root.  This has nothing to do with the base register used to
	//reference the buffer in the shader.  For example, the buffer in shader uses b0.
	int cb_root_parameter = 1;
	pCommandList->SetGraphicsRootDescriptorTable(cb_root_parameter, cbvHandle);

	// Bind the texture
	//CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(m_pCBVSRVHeap->GetGPUDescriptorHandleForHeapStart());
//	srvHandle.Offset(m_unTextureIndex, nCBVSRVDescriptorSize);
//	pCommandList->SetGraphicsRootDescriptorTable(1, srvHandle);

	// Bind the VB and draw
	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	pCommandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	pCommandList->DrawInstanced((UINT)m_unVertexCount, 1, 0, 0);

	//Bind the IB and draw
	//pCommandList->IASetIndexBuffer(&m_indexBufferView);
	//pCommandList->DrawIndexedInstanced((UINT)m_unVertexCount, 1, 0, 0, 0);
	
}

void DXPointCloud::UpdateShaderData(const XMMATRIX& matWVP, const XMMATRIX& matVP, const XMMATRIX& view)
{
	XMStoreFloat4x4(&mShaderData.g_WVPMatrix, XMMatrixTranspose(matWVP));
	XMStoreFloat4x4(&mShaderData.gViewProj, XMMatrixTranspose(matVP));
	XMStoreFloat4x4(&mShaderData.gView, XMMatrixTranspose(view));

	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMStoreFloat4x4(&mShaderData.gInvView, XMMatrixTranspose(invView));

	//get world space transform of camera
	DirectX::XMFLOAT4X4 invView4x4;
	XMStoreFloat4x4(&invView4x4, XMMatrixTranspose(invView));

	//camera world space position
	XMFLOAT4 camPos4 = { invView4x4(0,3), invView4x4(1,3), invView4x4(2,3), 1.0f };
	mShaderData.gEyePosW = camPos4;

	//quad size
	XMFLOAT2 quad_size = mQuadSize;
	XMFLOAT4 quadSize4 = { quad_size.x, quad_size.y, 0.0f, 0.0f };
	mShaderData.gQuadSize = quadSize4;

	memcpy(m_pConstantBufferData, &mShaderData, sizeof(mShaderData));
}

void DXPointCloud::CreateBoxPointCloudFile(const char* filename, float box_size)
{
	vector< DXGraphicsUtilities::CloudVertexPosColor> vCloudVertices;

	//spacing between points in box shaped cloud
	float res = mDebugBoxPointCloudResolution;

	//front of cube
	XMFLOAT4 front_color = { 255,255,0,255 };

	for (float y = -box_size / 2.0f; y <= box_size / 2.0f; y+=res)
	{
		for (float x = -box_size / 2.0f; x <= box_size / 2.0f; x+=res)
		{
			DXGraphicsUtilities::CloudVertexPosColor v;
			v.Pos = XMFLOAT3(x, y, -box_size/2.0f);
			v.Color = front_color;
			vCloudVertices.push_back(v);
		}
	}

	if (!mbDebugFrontFaceWriteOnly)
	{
		//back of cube
		XMFLOAT4 back_color = { 255,0,0,255 };

		for (float y = -box_size / 2.0f; y <= box_size / 2.0f; y += res)
		{
			for (float x = -box_size / 2.0f; x <= box_size / 2.0f; x += res)
			{
				DXGraphicsUtilities::CloudVertexPosColor v;
				v.Pos = XMFLOAT3(x, y, box_size / 2.0f);
				v.Color = back_color;
				vCloudVertices.push_back(v);
			}
		}

		//right of cube
		XMFLOAT4 right_color = { 0,255,0,255 };

		for (float y = -box_size / 2.0f; y <= box_size / 2.0f; y += res)
		{
			for (float z = -box_size / 2.0f; z <= box_size / 2.0f; z += res)
			{
				DXGraphicsUtilities::CloudVertexPosColor v;
				v.Pos = XMFLOAT3(box_size / 2.0f, y, z);
				v.Color = right_color;
				vCloudVertices.push_back(v);
			}
		}

		//left of cube
		XMFLOAT4 left_color = { 0,0,255,255 };

		for (float y = -box_size / 2.0f; y <= box_size / 2.0f; y += res)
		{
			for (float z = -box_size / 2.0f; z <= box_size / 2.0f; z += res)
			{
				DXGraphicsUtilities::CloudVertexPosColor v;
				v.Pos = XMFLOAT3(-box_size / 2.0f, y, z);
				v.Color = left_color;
				vCloudVertices.push_back(v);
			}
		}


		//top of cube
		XMFLOAT4 top_color = { 255,0,255,255 };

		for (float z = -box_size / 2.0f; z <= box_size / 2.0f; z += res)
		{
			for (float x = -box_size / 2.0f; x <= box_size / 2.0f; x += res)
			{
				DXGraphicsUtilities::CloudVertexPosColor v;
				v.Pos = XMFLOAT3(x, box_size / 2.0f, z);
				v.Color = top_color;
				vCloudVertices.push_back(v);
			}
		}

		//bottom of cube
		XMFLOAT4 bottom_color = { 0,255,255,255 };

		for (float z = -box_size / 2.0f; z <= box_size / 2.0f; z += res)
		{
			for (float x = -box_size / 2.0f; x <= box_size / 2.0f; x += res)
			{
				DXGraphicsUtilities::CloudVertexPosColor v;
				v.Pos = XMFLOAT3(x, -box_size / 2.0f, z);
				v.Color = bottom_color;
				vCloudVertices.push_back(v);
			}
		}
	} //if (!mbDebugFrontFaceWriteOnly)
	
	//write to file
	std::ofstream fout(filename);

	//file header 
	fout << "ply" << endl;
	fout << "format ascii 1.0" << endl;
	fout << "comment VCGLIB generated" << endl;
	fout << "element vertex " << vCloudVertices.size() << endl;
	fout << "property float x" << endl;
	fout << "property float y" << endl;
	fout << "property float z" << endl;
	fout << "property uchar red" << endl;
	fout << "property uchar green" << endl;
	fout << "property uchar blue" << endl;
	fout << "property uchar alpha" << endl;
	fout << "element face 0" << endl;
	fout << "property list uchar int vertex_indices" << endl;
	fout << "end_header" << endl;

	//write point data
	for (int i = 0; i < vCloudVertices.size(); ++i)
	{
		fout << vCloudVertices[i].Pos.x << " " << vCloudVertices[i].Pos.y << " " << vCloudVertices[i].Pos.z <<
			" " << vCloudVertices[i].Color.x << " " << vCloudVertices[i].Color.y << " "
			<< vCloudVertices[i].Color.z << " " << vCloudVertices[i].Color.w << std::endl;
	}
}

void DXPointCloud::CreateProcessingRootSignature(ComPtr<ID3D12Device> pDevice)
{
	if (m_PointCloudProcessRootSignature)
		return;

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

	// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}


	//The set of descriptor tables being used at a given time, among other things, are defined as part of the root arguments.
	//The layout of the root arguments, the root signature, is an application specified definition of a binding space
	//(with a limited maximum size for efficiency) that identifies how resources in shaders(SRVs, UAVs, CBVs, Samplers) map into 
	//descriptor table locations.The root signature can also hold a small number of descriptors
	//directly(bypassing the need to put them into descriptor heaps / tables).
	//Finally, the root signature can even hold inline 32 - bit values that show up in the shader as a constant buffer.
	// Create a root signature consisting of a descriptor table with a SRV and a sampler.
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
		CD3DX12_ROOT_PARAMETER1 rootParameters[3];

		// SetGraphicsRootDescriptorTable is executed on the GPU so we can use the default
		// range behavior: D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
		//t0 on param 0
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

		//CBV with 1 descriptor.  b0 in param 1
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);

		//t1 in param2
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
		rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);

		// Allow input layout and pixel shader access and deny uneccessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		// Create a sampler.
		D3D12_STATIC_SAMPLER_DESC sampler[2]; 
		sampler[0] = D3D12_STATIC_SAMPLER_DESC();
		sampler[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler[0].MipLODBias = 0;
		sampler[0].MaxAnisotropy = 0;
		sampler[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler[0].MinLOD = 0.0f;
		sampler[0].MaxLOD = D3D12_FLOAT32_MAX;
		sampler[0].ShaderRegister = 0;
		sampler[0].RegisterSpace = 0;
		sampler[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		//sampler for z texture uses point sampling
		sampler[1] = D3D12_STATIC_SAMPLER_DESC();
		sampler[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;// D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler[1].MipLODBias = 0;
		sampler[1].MaxAnisotropy = 0;
		sampler[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler[1].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler[1].MinLOD = 0.0f;
		sampler[1].MaxLOD = D3D12_FLOAT32_MAX;
		sampler[1].ShaderRegister = 1;
		sampler[1].RegisterSpace = 0;
		sampler[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 2, sampler, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_PointCloudProcessRootSignature)));
		NAME_D3D12_OBJECT(m_PointCloudProcessRootSignature);
	}
}
void DXPointCloud::CreateProcessingPipelineState(ComPtr<ID3D12Device> pDevice)
{
	if (m_PointCloudProcessPipelineState)
		return;

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> processVertexShader;
		ComPtr<ID3DBlob> processPixelShader;
		ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ThrowIfFailed(D3DCompileFromFile(L"assets/shaders/pointCloudProcessingShaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &processVertexShader, &error));

		if (mDebugVizDepthBuffer == false)
			ThrowIfFailed(D3DCompileFromFile(L"assets/shaders/pointCloudProcessingShaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &processPixelShader, &error));
		else
			ThrowIfFailed(D3DCompileFromFile(L"assets/shaders/pointCloudProcessingShaders.hlsl", nullptr, nullptr, "PSVizDepthBuffer", "ps_5_0", compileFlags, 0, &processPixelShader, &error));

		// Define the vertex input layouts.
		D3D12_INPUT_ELEMENT_DESC scaleInputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state objects (PSOs).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		//psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		psoDesc.InputLayout = { scaleInputElementDescs, _countof(scaleInputElementDescs) };
		psoDesc.pRootSignature = m_PointCloudProcessRootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(processVertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(processPixelShader.Get());

		ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PointCloudProcessPipelineState)));
		NAME_D3D12_OBJECT(m_PointCloudProcessPipelineState);
	}
}

