#pragma once

#include "Common.h"
#include "Structures.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class DXShaderUtilities
{
public:
	DXShaderUtilities();
	~DXShaderUtilities();

	void Init_Shader_Compiler(D3D12ShaderCompilerInfo& shaderCompiler);
	void Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, RtProgram& program);
	void Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, D3D12ShaderInfo& info, IDxcBlob** blob);
	void Destroy(D3D12ShaderCompilerInfo& shaderCompiler);

};
