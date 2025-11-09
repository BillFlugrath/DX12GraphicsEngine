#pragma once

#include "Common.h"
#include "Structures.h"


using namespace DirectX;
using Microsoft::WRL::ComPtr;

class DXShaderUtilities;
class DXD3DUtilities;
class DXResourceUtilities;


class DXRShaderBindingTable
{
public:
	DXRShaderBindingTable();
	~DXRShaderBindingTable();


	void Create_Shader_Table(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources);
};
