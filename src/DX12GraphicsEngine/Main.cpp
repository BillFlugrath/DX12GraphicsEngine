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

#include "stdafx.h"

#include "DX12Raytracing_2.h" 
#include "D3D12PointCloudApp_4.h"
#include "DX12Raytracing_Inline_1.h"
#include "DX12MeshShader_1.h"


_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    uint32_t appIndex = 1;

    if (appIndex == 0)
    {
        D3D12PointCloudApp_4 sample(1024, 1024, L"D3D12 Point Cloud Test 4");
        return Win32Application::Run(&sample, hInstance, nCmdShow);
    }
    else if (appIndex == 1)
    {
        DX12Raytracing_2 sample(512, 512, L"D3D12 Raytracing 2");
        return Win32Application::Run(&sample, hInstance, nCmdShow);
    }
    else if (appIndex == 2)
    {
        DX12Raytracing_Inline_1 sample(512, 512, L"D3D12 Raytracing Inline 1");
        return Win32Application::Run(&sample, hInstance, nCmdShow);
    }
    else
    {
        DX12MeshShader_1 sample(512, 512, L"D3D12 Mesh Shader 1");  //NOT DONE!!
        return Win32Application::Run(&sample, hInstance, nCmdShow); 
    }
}
