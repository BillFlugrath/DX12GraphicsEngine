//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include "DXSampleHelper.h"
#include "Win32Application.h"
//#include "DeviceResources.h"

class DXSample 
{
public:
    DXSample(UINT width, UINT height, std::wstring name);
    virtual ~DXSample();

    virtual void OnInit() {};
    virtual void OnUpdate() { };
    virtual void OnRender() { };
    virtual void OnSizeChanged(UINT width, UINT height, bool minimized) { };
    virtual void OnDestroy() { };

    // Samples override the event handlers to handle specific messages.
    virtual void OnKeyDown(UINT8 /*key*/) {}
    virtual void OnKeyUp(UINT8 /*key*/) {}
    virtual void OnWindowMoved(int /*x*/, int /*y*/) {}
    virtual void OnMouseMove(UINT x, UINT y, UINT8 mouse_button) {}
    virtual void OnLeftButtonDown(UINT /*x*/, UINT /*y*/) {}
    virtual void OnLeftButtonUp(UINT /*x*/, UINT /*y*/) {}
    virtual void OnDisplayChanged() {}

    // Overridable members.
    virtual void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

    // Accessors.
    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }
    RECT GetWindowsBounds() const { return m_windowBounds; }
    virtual IDXGISwapChain* GetSwapchain() { return nullptr; }
   // DX::DeviceResources* GetDeviceResources() const { return m_deviceResources.get(); }

    void UpdateForSizeChange(UINT clientWidth, UINT clientHeight);
    void SetWindowBounds(int left, int top, int right, int bottom);
    std::wstring GetAssetFullPath(LPCWSTR assetName);
    void GetHardwareAdapter(_In_ IDXGIFactory2* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter);

protected:
    void SetCustomWindowText(LPCWSTR text);

    // Viewport dimensions.
    UINT m_width;
    UINT m_height;
    float m_aspectRatio;

    // Window bounds
    RECT m_windowBounds;
    
    // Override to be able to start without Dx11on12 UI for PIX. PIX doesn't support 11 on 12. 
    bool m_enableUI;

    // D3D device resources
    UINT m_adapterIDoverride;
    //std::unique_ptr<DX::DeviceResources> m_deviceResources;

     //Stencil and Depth Buffer
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
    void CreateDepthStencilResources(ComPtr<ID3D12Device> m_device);

    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; //DXGI_FORMAT_R24G8_TYPELESS
    Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

    // Root assets path.
    std::wstring m_assetsPath;

    // Window title.
    std::wstring m_title;

    // Whether or not tearing is available for fullscreen borderless windowed mode.
    bool m_tearingSupport = true;


    // Adapter info.
    bool m_useWarpDevice;
};
