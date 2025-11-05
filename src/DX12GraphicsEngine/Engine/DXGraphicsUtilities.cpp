#include "stdafx.h"
#include "DXGraphicsUtilities.h"
#include "DXMesh.h"
#include "./DXR/Structures.h"
#include "lodepng.h"

namespace DXGraphicsUtilities
{
	std::wstring m_assetsPath;

	//initialize asset path
	void SetAssetFullPath(const std::wstring & assetsPath) { m_assetsPath = assetsPath; }

	// Helper function for resolving the full path of assets.
	std::wstring GetAssetFullPath(LPCWSTR assetName) { return m_assetsPath + assetName; }

	void CreateD3DMesh(std::shared_ptr<DXMesh> pDXMesh, D3DMesh* pD3DMesh)
	{
		(*pD3DMesh).m_pVertexBuffer = pDXMesh->GetVertexBuffer();
		(*pD3DMesh).m_vertexBufferView = pDXMesh->GetVertexBufferView();
		(*pD3DMesh).m_pIndexBuffer = pDXMesh->GetIndexBuffer();
		(*pD3DMesh).m_indexBufferView = pDXMesh->GetIndexBufferView();
		(*pD3DMesh).vertices = pDXMesh->GetVertices();
		(*pD3DMesh).indices = pDXMesh->GetIndices32Bit();
	}

	void WaitForGpu(ComPtr<ID3D12Device> &dx_device,
		ComPtr<ID3D12CommandQueue> &commandQueue)
	{
		// Create synchronization objects and wait until assets have been uploaded to the GPU.
		UINT64 fenceValue = 0;

		// Synchronization objects.
		HANDLE fenceEvent;
		ComPtr<ID3D12Fence> d3dFence;

		ThrowIfFailed(dx_device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3dFence)));
		fenceValue++;

		// Create an event handle to use for frame synchronization.
		fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// Schedule a Signal command in the queue.
		ThrowIfFailed(commandQueue->Signal(d3dFence.Get(), fenceValue));

		// Wait until the fence has been processed.
		ThrowIfFailed(d3dFence->SetEventOnCompletion(fenceValue, fenceEvent));
		WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);

	}



	//-----------------------------------------------------------------------------
	// Purpose: generate next level mipmap for an RGBA image
	//-----------------------------------------------------------------------------
	void GenMipMapRGBA(const UINT8 *pSrc, UINT8 **ppDst, int nSrcWidth, int nSrcHeight, int *pDstWidthOut, int *pDstHeightOut)
	{
		*pDstWidthOut = nSrcWidth / 2;
		if (*pDstWidthOut <= 0)
		{
			*pDstWidthOut = 1;
		}
		*pDstHeightOut = nSrcHeight / 2;
		if (*pDstHeightOut <= 0)
		{
			*pDstHeightOut = 1;
		}

		*ppDst = new UINT8[4 * (*pDstWidthOut) * (*pDstHeightOut)];
		for (int y = 0; y < *pDstHeightOut; y++)
		{
			for (int x = 0; x < *pDstWidthOut; x++)
			{
				int nSrcIndex[4];
				float r = 0.0f;
				float g = 0.0f;
				float b = 0.0f;
				float a = 0.0f;

				nSrcIndex[0] = (((y * 2) * nSrcWidth) + (x * 2)) * 4;
				nSrcIndex[1] = (((y * 2) * nSrcWidth) + (x * 2 + 1)) * 4;
				nSrcIndex[2] = ((((y * 2) + 1) * nSrcWidth) + (x * 2)) * 4;
				nSrcIndex[3] = ((((y * 2) + 1) * nSrcWidth) + (x * 2 + 1)) * 4;

				// Sum all pixels
				for (int nSample = 0; nSample < 4; nSample++)
				{
					r += pSrc[nSrcIndex[nSample]];
					g += pSrc[nSrcIndex[nSample] + 1];
					b += pSrc[nSrcIndex[nSample] + 2];
					a += pSrc[nSrcIndex[nSample] + 3];
				}

				// Average results
				r /= 4.0;
				g /= 4.0;
				b /= 4.0;
				a /= 4.0;

				// Store resulting pixels
				(*ppDst)[(y * (*pDstWidthOut) + x) * 4] = (UINT8)(r);
				(*ppDst)[(y * (*pDstWidthOut) + x) * 4 + 1] = (UINT8)(g);
				(*ppDst)[(y * (*pDstWidthOut) + x) * 4 + 2] = (UINT8)(b);
				(*ppDst)[(y * (*pDstWidthOut) + x) * 4 + 3] = (UINT8)(a);
			}
		}
	}

	//start
	bool LoadPNGTextureMap(const std::wstring& strFullPath, ComPtr< ID3D12Device >& pDevice,
		ComPtr<ID3D12CommandQueue> &commandQueue,
		ComPtr< ID3D12Resource > &pTexture,
		unsigned int& nImageWidth,unsigned int& nImageHeight)
	{
		std::vector< unsigned char > imageRGBA;
		
		std::string str(strFullPath.begin(), strFullPath.end());
		unsigned nError = lodepng::decode(imageRGBA, nImageWidth, nImageHeight, str.c_str());

		if (nError != 0)
			return false;

		// Store level 0
		std::vector< D3D12_SUBRESOURCE_DATA > mipLevelData;
		UINT8 *pBaseData = new UINT8[nImageWidth * nImageHeight * 4];
		memcpy(pBaseData, &imageRGBA[0], sizeof(UINT8) * nImageWidth * nImageHeight * 4);

		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = &pBaseData[0];
		textureData.RowPitch = nImageWidth * 4;
		textureData.SlicePitch = textureData.RowPitch * nImageHeight;
		mipLevelData.push_back(textureData);

		// Generate mipmaps for the image
		int nPrevImageIndex = 0;
		int nMipWidth = nImageWidth;
		int nMipHeight = nImageHeight;

		while (nMipWidth > 1 && nMipHeight > 1)
		{
			UINT8 *pNewImage;
			GenMipMapRGBA((UINT8*)mipLevelData[nPrevImageIndex].pData, &pNewImage, nMipWidth, nMipHeight, &nMipWidth, &nMipHeight);

			D3D12_SUBRESOURCE_DATA mipData = {};
			mipData.pData = pNewImage;
			mipData.RowPitch = nMipWidth * 4;
			mipData.SlicePitch = textureData.RowPitch * nMipHeight;
			mipLevelData.push_back(mipData);

			nPrevImageIndex++;
		}

		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = (UINT16)mipLevelData.size();
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = nImageWidth;
		textureDesc.Height = nImageHeight;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		pDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&pTexture));


		const UINT64 nUploadBufferSize = GetRequiredIntermediateSize(pTexture.Get(), 0, textureDesc.MipLevels);

		// Create the GPU upload buffer.
		ComPtr< ID3D12Resource > pTextureUploadHeap;

		pDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(nUploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&pTextureUploadHeap));

		ComPtr<ID3D12CommandAllocator> commandAllocator;
		ComPtr<ID3D12GraphicsCommandList> commandList;

		ThrowIfFailed(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
		ThrowIfFailed(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

		UpdateSubresources(commandList.Get(), pTexture.Get(), pTextureUploadHeap.Get(), 0, 0, mipLevelData.size(), &mipLevelData[0]);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		// Free mip pointers
		for (size_t nMip = 0; nMip < mipLevelData.size(); nMip++)
		{
			delete[] mipLevelData[nMip].pData;
		}
		 
		// the default heap.
		ThrowIfFailed(commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { commandList.Get()};
		commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);


		DXGraphicsUtilities::WaitForGpu(pDevice, commandQueue);
		return true;
	}


}