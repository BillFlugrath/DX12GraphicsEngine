#pragma once

#include "Common.h"
#include "Structures.h"


using namespace DirectX;
using Microsoft::WRL::ComPtr;

class DXShaderUtilities;
class DXD3DUtilities;
class DXResourceUtilities;

class DXRUtilities
{
public:
	DXRUtilities();
	~DXRUtilities();

	void Init(std::shared_ptr< DXShaderUtilities > pDXShaderUtilities,
			  std::shared_ptr< DXD3DUtilities > pDXD3DUtilities,
			  std::shared_ptr< DXResourceUtilities> pDXResourceUtilities);


	void Create_RayGen_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler, const std::wstring& filename);
	void Create_Miss_Program(D3D12Global& d3d, DXRGlobal& dxr, D3D12ShaderCompilerInfo& shaderCompiler, const std::wstring& filename);
	
	void Create_Closest_Hit_Program(D3D12Global& d3d, DXRGlobal& dxr,
		D3D12ShaderCompilerInfo& shaderCompiler, const std::wstring& filename, uint32_t numMeshObjects);

	void Create_DXR_Output_Buffer(D3D12Global& d3d, D3D12Resources& resources);

	void RenderScene(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources);

	void Destroy(DXRGlobal& dxr);

	std::shared_ptr< DXShaderUtilities > m_pDXShaderUtilities;
	std::shared_ptr< DXD3DUtilities > m_pDXD3DUtilities;
	std::shared_ptr< DXResourceUtilities> m_pDXResourceUtilities;
};
