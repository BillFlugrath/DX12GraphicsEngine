#include "stdafx.h"

#include "DXShaderUtilities.h"

#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif

#ifdef TINYOBJLOADER_IMPLEMENTATION
#undef TINYOBJLOADER_IMPLEMENTATION
#endif

#include "Utils.h"

DXShaderUtilities::DXShaderUtilities()
{

}

DXShaderUtilities::~DXShaderUtilities()
{

}

/**
* Compile a D3D HLSL shader using dxcompiler.
*/
void DXShaderUtilities::Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, D3D12ShaderInfo& info, IDxcBlob** blob)
{
	HRESULT hr;
	UINT32 codePage(0);
	IDxcBlobEncoding* pShaderText(nullptr);

	// Load and encode the shader file
	hr = compilerInfo.library->CreateBlobFromFile(info.filename, &codePage, &pShaderText);
	Utils::Validate(hr, L"Error: failed to create blob from shader file!");

	// Create the compiler include handler
	CComPtr<IDxcIncludeHandler> dxcIncludeHandler;
	hr = compilerInfo.library->CreateIncludeHandler(&dxcIncludeHandler);
	Utils::Validate(hr, L"Error: failed to create include handler");

	// Compile the shader
	IDxcOperationResult* result;
	hr = compilerInfo.compiler->Compile(pShaderText, info.filename, L"", info.targetProfile, nullptr, 0, nullptr, 0, dxcIncludeHandler, &result);
	Utils::Validate(hr, L"Error: failed to compile shader!");

	// Verify the result
	result->GetStatus(&hr);
	if (FAILED(hr))
	{
		IDxcBlobEncoding* error;
		hr = result->GetErrorBuffer(&error);
		Utils::Validate(hr, L"Error: failed to get shader compiler error buffer!");

		// Convert error blob to a string
		vector<char> infoLog(error->GetBufferSize() + 1);
		memcpy(infoLog.data(), error->GetBufferPointer(), error->GetBufferSize());
		infoLog[error->GetBufferSize()] = 0;

		string errorMsg = "Shader Compiler Error:\n";
		errorMsg.append(infoLog.data());

		MessageBoxA(nullptr, errorMsg.c_str(), "Error!", MB_OK);
		return;
	}

	hr = result->GetResult(blob);
	Utils::Validate(hr, L"Error: failed to get shader blob result!");
}

/**
* Compile a D3D DXRT HLSL shader using dxcompiler.
*/
void DXShaderUtilities::Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, RtProgram& program)
{
	Compile_Shader(compilerInfo, program.info, &program.blob); //fill in with compiled shader file (all of the functions are in the blob)
	program.SetBytecode();  //store the blob in a DXIL library contaned in a subobject of type D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY
}

/**
* Initialize the shader compiler.
*/
void DXShaderUtilities::Init_Shader_Compiler(D3D12ShaderCompilerInfo& shaderCompiler)
{
	HRESULT hr = shaderCompiler.DxcDllHelper.Initialize();
	Utils::Validate(hr, L"Failed to initialize DxCDllSupport!");

	hr = shaderCompiler.DxcDllHelper.CreateInstance(CLSID_DxcCompiler, &shaderCompiler.compiler);
	Utils::Validate(hr, L"Failed to create DxcCompiler!");

	hr = shaderCompiler.DxcDllHelper.CreateInstance(CLSID_DxcLibrary, &shaderCompiler.library);
	Utils::Validate(hr, L"Failed to create DxcLibrary!");
}

/**
 * Release shader compiler resources.
 */
void DXShaderUtilities::Destroy(D3D12ShaderCompilerInfo& shaderCompiler)
{
	SAFE_RELEASE(shaderCompiler.compiler);
	SAFE_RELEASE(shaderCompiler.library);
	shaderCompiler.DxcDllHelper.Cleanup();
}


