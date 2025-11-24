
#pragma once

#include "DXSample.h"
#include "./Engine/DXGraphicsUtilities.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

class DXTexturedQuad;
class DXTexture;
class DXModel;
class DXCamera;
class DXDescriptorHeap;
class DXPointCloudComputeShader_3;

class DXRUtilities;
class DXRManager;
struct D3DMesh;
struct D3DModel;
struct D3DSceneModels;
struct D3DSceneTextures;
class DXMeshShader;

//------------------------------- NOT DONE!!-------------------------------------------

class DX12MeshShader_1 : public DXSample
{
public:
	DX12MeshShader_1(UINT width, UINT height, std::wstring name);
	~DX12MeshShader_1();

	static DX12MeshShader_1* Get() { return s_app; }

protected:
	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnSizeChanged(UINT width, UINT height, bool minimized) {};
	virtual void OnDestroy();
	virtual void OnKeyDown(UINT8 key);
	virtual IDXGISwapChain* GetSwapchain() { return m_swapChain.Get(); }

private:
	static const UINT FrameCount = 2;
	static const int kNumContexts = 2;

	static const float LetterboxColor[4];
	static const float ClearColor[4];

	struct PostVertex
	{
		XMFLOAT4 position;
		XMFLOAT2 uv;
	};


	// Pipeline objects.
	CD3DX12_VIEWPORT m_sceneViewport;
	CD3DX12_VIEWPORT m_postViewport;
	CD3DX12_RECT m_sceneScissorRect;
	CD3DX12_RECT m_postScissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;

	//the swap chain textures,(an array of two rtts). We render a textured quad into one of these rtts for final display.
	//these rtts represent the actual swap chain bufffers.  Thus, we only write into these, we do not read from them.
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];

	//All 3d objects, etc are rendered first into  mhTexturedQuadRtv (ie the RTT owned by the textured quad).
	// We then do a full screen quad draw into the m_intermediateRenderTarget.  This is the final rtt texture
	// before writing out to swap chain (ie back buffer).  This texture is used for a final full screen quad draw
	// as the pixel shader input to the quad draw. Thus, the final pixel shader
	//simply draws a quad with m_intermediateRenderTarget as the input texture and  one of the rtts in m_renderTargets as the final output
	//for display.
	ComPtr<ID3D12Resource> m_intermediateRenderTarget;

  // the mhIntermediateRttCpuSrv and mhIntermediateRttGpuSrv are used for creating and setting the 
	//intermediate rtt as a SRV to read in a pixel shader.  These handles are in a srv-cbv descriptor heap.
	int m_IntermediateRTTDescriptorIndex = -1; //index into descriptor_heap_srv_ (NOT the RTT descriptor heap!)
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhIntermediateRttCpuSrv;  //used to create SRV
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhIntermediateRttGpuSrv;  //use to bind to graphics root for pixel shader read

	//The mhIntermediateRtv is the third handle (ie index = 2) into m_rtvHeap.  The first two handles
	// are handles to the swap chain rtts.
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhIntermediateRtv; //handle used for clear and set rtt before draw call

	ComPtr<ID3D12CommandAllocator> m_postCommandAllocators[FrameCount];
	ComPtr<ID3D12CommandAllocator> m_sceneCommandAllocators[FrameCount];
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12RootSignature> m_postRootSignature;

	//the m_rtvHeap descriptor heap stores two handles for each swap chain buffer, and one handle for the m_intermediateRenderTarget
	//Thus, the descriptor heap holds three handles.  The types of Descriptor Heaps that can be set
	//on a command list ( via commandlist->SetDescriptorHeaps() ) are only:
	// D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV and D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER !!!

	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12PipelineState> m_postPipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_postCommandList;
	ComPtr<ID3D12GraphicsCommandList> m_sceneCommandList;
	UINT m_rtvDescriptorSize;
	UINT m_cbvSrvDescriptorSize;

	// App resources.
	ComPtr<ID3D12Resource> m_postVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_postVertexBufferView;

	//index of the current back buffer.  The swap chain has two buffers, so valid index is 0 or 1.
	UINT m_frameIndex;

	//--Begin Synchronization objects.
	HANDLE m_fenceEvent; //handle to an event.  the event gets signaled  when the fence reaches a certain value.  Thus, the CPU thread waits on this handle.

	//GPU fence added to the gpu command list. Fence will set a UINT64 value when it has completed on the GPU.  We can read this value on
	//the CPU via fence->GetCompletedValue().  Fence can also signal an event when its done via fence->SetEventOnCompletion(event handle);  If we want CPU thread to
	//wait for fence to finish, we can WaitForSingleObjectEx(m_fenceEvent, ...);
	ComPtr<ID3D12Fence> m_fence;

	UINT64 m_fenceValue;

	//fence value incremented each frame.  After increment, we store the value in the next frame we will render.  For example if current frame
	//with index =1 has fence value of 7 ( m_fenceValues[1]=7), then before next render loop, we do m_fenceValues[0]=8.
	UINT64 m_fenceValues[FrameCount];

	//--End Synchronization objects.

	// Track the state of the window.
	// If it's minimized the app may decide not to render frames.
	bool m_windowVisible;
	bool m_windowedMode;

	//Initialize the resources
	void LoadPipeline();
	void LoadAssets();
	void CreatePostRootSignature();
	void CreatePostPipelineState();
	void CreateRTViewsForSwapChain();
	void CreateIntermediateRttResources();
	void LoadMainSceneModelsAndTextures(const CD3DX12_VIEWPORT& quad_viewport, const CD3DX12_RECT& quad_scissor);
	void WaitForGpu();
	void MoveToNextFrame();
	void UpdateSceneViewportAndScissor();
	void UpdatePostViewportAndScissor();
	void CreateQuadVertsAndDepthStencil(ComPtr<ID3D12GraphicsCommandList> commandList);
	void UpdateTitle();

	void ProcessImageWithComputeShader();

	DXCamera *m_DXCamera;
	DXModel* m_pDXPointCloudModel;
	std::vector<DXModel*> m_DXModelScene;//models to render

	// Create a DXTexturedQuad and store the handles of the RTT in the m_pTexturedQuadRTT 
	DXTexturedQuad *m_pTexturedQuadRTT; //drawing to rtt and reading from texture in a shader

	//Cache the handle to the RTT in the m_pTexturedQuadRTT object.
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhTexturedQuadRtv; //handle used for clear and set rtt

	//descriptor heaps to store descriptors for obj models and textures associated with rtts used by DXTexturedQuad class.
	std::shared_ptr<DXDescriptorHeap> descriptor_heap_srv_;
	std::shared_ptr<DXDescriptorHeap> descriptor_heap_rtv_;

	struct ThreadParameter
	{
		int threadIndex;
	};

	ThreadParameter m_threadParameters[kNumContexts];

	// Synchronization objects.
	HANDLE m_workerBeginRenderFrame[kNumContexts]; //an array of events.  main thread calls SetEvent on all of these at begining of frame to start all worker threads
	HANDLE m_workerFinishShadowPass[kNumContexts]; //NOT USED
	HANDLE m_workerFinishedRenderFrame[kNumContexts]; //main thread waits for all of these events to be signaled before finishing frame render (ie a join)
	HANDLE m_threadHandles[kNumContexts]; //handles to worker threads


	// Singleton object so that worker threads can share members.
	static DX12MeshShader_1* s_app;

	void InitThreads();
	void WorkerThread(int threadIndex);
	void LoadContexts();
	void RenderScene();
	void RenderPostScene();

	void InitializeComputeShader();
	std::unique_ptr< DXPointCloudComputeShader_3> m_PointCloudComputeShader_3;

	//Width and height of texture in the DXTexturedQuad object.  Typically, this would be the size of frame buffer.
	int mTexturedQuadRTTWidth;
	int mTexturedQuadRTTHeight;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mhTextureInputPostProcessGpuSrv;

	//Depth texture shader SRV
	void CreateDepthTextureSrv();
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthMapCpuSrv;  //used to create SRV
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthMapGpuSrv;  //use to bind to graphics root
	int m_DepthTextureSRVDescriptorIndex = -1;

	//Constant buffer resource and CBV to access the constant buffer.
	void CreateConstantBuffer();
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhConstantBufferCpuSrv;  //used to create SRV
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhConstantBufferGpuSrv;  //use to bind to graphics root

	ComPtr< ID3D12Resource > m_pConstantBuffer;
	UINT8* m_pConstantBufferData = nullptr;
	int m_cbDescriptorIndex = -1;

	void UpdateShaderData(); //call once per frame
	
	DXGraphicsUtilities::ShaderData mShaderData;

	//Temp signature and pipeline state
	ComPtr<ID3D12RootSignature> m_TempRootSignature;
	ComPtr<ID3D12PipelineState> m_TempPipelineState;

	//Texture dimensions for the output texture of the compute shader.  This is set via a UAV for the texture.
	UINT mUavCsTextureWidth = 32;
	UINT mUavCsTextureHeight= 32;

	//Debug members
	void DebugTests();
	bool mDebugEnableDebugTests = false;  //Calls a debug function used for temporary testing only.
	bool mDebugRender3dModels = true; //render 3d models with rasterizer
	bool mDebugRenderPointCloud = true; //render a point cloud

	bool mDebugVizDepthBuffer = false; //show depth buffer of 3d scene previously rendered
	bool mDebugUseZPlanePointCloud = false;
	bool mDebugUseiPhonePointCloud = true; //use scaniverse ply file otherwise use box point cloud

	bool mDebugEnableComputeShader = true;


	//---------------- DXR ----------------------------
	
	// Create raytracing device and command list from regular D3D graphics versions
	void CreateRaytracingInterfaces(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	//load scene files
	void LoadModelsAndTextures();

	void InitRayTracing();
	void CreateRayTracingScene();
	void CreateSRVsForRayTracing();

	std::vector< std::shared_ptr<DXMesh> > m_vMeshObjects; //RT scene, not rasterized scene

	std::shared_ptr<D3DSceneModels> m_pD3DSceneModels;  //RT scene, not rasterized scene
	std::shared_ptr<D3DSceneTextures> m_pD3DSceneTextures2D;

	std::vector< std::shared_ptr<DXTexture> > m_vTextureObjects;
	std::shared_ptr<DXTexture> m_pDDSCubeMap_0;
	CD3DX12_GPU_DESCRIPTOR_HANDLE m_CubemapGPUHandle;  //parameter index = 3
	CD3DX12_GPU_DESCRIPTOR_HANDLE m_BVHGPUHandle;  //parameter index = 2

	// DirectX Raytracing (DXR) attributes
	ID3D12Device5* m_dxrDevice;
	ID3D12GraphicsCommandList4* m_dxrCommandList;
	ID3D12CommandQueue* m_CmdQueue;

	std::shared_ptr< DXRManager > m_pDXRManager;

	// ------------------------------ DX Mesh Shader ---------------------------------------
	std::shared_ptr< DXMeshShader > m_pDXMeshShader;

};
