#pragma once

#include "../DXSampleHelper.h"
#include "./DXR/DXRVertices.h"

//#include "Win32Application.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

const int kInvalidDescriptorHandle = -1;

//File names for testing
const std::wstring kTestPNGFile = L"C:/dx12tests/DirectX-Graphics-Samples-master/Samples/Desktop/D3D12BillsTests/src/assets/textures/happy_fish.png"; //
const std::wstring kTestPNGFile_2 = L"C:/dx12tests/DirectX-Graphics-Samples-master/Samples/Desktop/D3D12BillsTests/src/assets/textures/Countdown_02.png";
const std::wstring kTestPNGFile_3 = L"C:/dx12tests/DirectX-Graphics-Samples-master/Samples/Desktop/D3D12BillsTests/src/assets/textures/hatchbrightest.png";
const std::wstring kTestPNGFile_4 = L"C:/dx12tests/DirectX-Graphics-Samples-master/Samples/Desktop/D3D12BillsTests/src/assets/textures/hatchdarkest.png";

const std::string kTestObjModelFilename = "C:/dx12tests/DirectX-Graphics-Samples-master/Samples/Desktop/D3D12BillsTests/src/assets/models/cube.objmodel";
const std::string kTestObjModelFilename2 = "C:/dx12tests/DirectX-Graphics-Samples-master/Samples/Desktop/D3D12BillsTests/src/assets/models/unitSphere.objmodel";

const std::string kTestPlyModelFilename = "C:/dx12tests/DirectX-Graphics-Samples-master/Samples/Desktop/D3D12BillsTests/src/assets/pointclouds/TestPointCloud_1.ply";
const std::string kTestPlyModelFilename_2 = "C:/dx12tests/DirectX-Graphics-Samples-master/Samples/Desktop/D3D12BillsTests/src/assets/pointclouds/TestPointCloud_2.ply";
const std::string kTestPlyModelFilename_3 = "C:/dx12tests/DirectX-Graphics-Samples-master/Samples/Desktop/D3D12BillsTests/src/assets/pointclouds/test.ply";

const std::string kiPhonePointCloudFile = "C:/dx12tests/DirectX-Graphics-Samples-master/Samples/Desktop/D3D12BillsTests/src/assets/pointclouds/test1.ply";
const std::string kBoxPointCloudFile = "C:/dx12tests/DirectX-Graphics-Samples-master/Samples/Desktop/D3D12BillsTests/src/assets/pointclouds/boxPointCloud.ply";
const std::string kzPlanePointCloudFile = "C:/dx12tests/DirectX-Graphics-Samples-master/Samples/Desktop/D3D12BillsTests/src/assets/pointclouds/zPlanePointCloud.ply";

//total number of descriptors in heap such as constant buffer descriptors, srv descriptors for textures, etc
const int kMaxNumOfCbSrvDescriptorsInHeap = 512;


struct D3DMesh;
class DXMesh;

namespace DXGraphicsUtilities
{
	void CreateD3DMesh(std::shared_ptr<DXMesh> pDXMesh, D3DMesh* pD3DMesh);

	void WaitForGpu(ComPtr<ID3D12Device> &dx_device,
		ComPtr<ID3D12CommandQueue> &commandQueue);

	bool LoadPNGTextureMap(const std::wstring& strFullPath, ComPtr< ID3D12Device >& pDevice,
		ComPtr<ID3D12CommandQueue> &commandQueue,
		ComPtr< ID3D12Resource > &pTexture,
		unsigned int& nImageWidth, unsigned int& nImageHeight);

	void GenMipMapRGBA(const UINT8 *pSrc, UINT8 **ppDst, int nSrcWidth, int nSrcHeight, int *pDstWidthOut, int *pDstHeightOut);

	//initialize asset path
	void SetAssetFullPath(const std::wstring & assetsPath);

	std::wstring GetAssetFullPath(LPCWSTR assetName);

	struct BoundingBox
	{
		DirectX::XMFLOAT3 mMin = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 mMax = { 0.0f, 0.0f, 0.0f };
	};

	struct ShaderData
	{
		XMFLOAT4X4 gProj;
		XMFLOAT4X4 gInvProj;
		XMFLOAT4X4 gView;
		XMFLOAT4X4 gInvView;
		XMFLOAT4 gQuadSize; //only uses first 2 floats
		XMFLOAT4 gCameraProperties; //{ near, far, rtt width, rtt height}
	};

}