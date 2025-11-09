!!MUST PUT TWO DLLs IN SAME DIR AS APP EXE!!!
The dxcompiler.dll and dxil.dll files get loaded by code at runtime. They must be in the
directory where app exe is built and put by VS. Thus, these two dll files MUST be MANUALLY COPIED
from "./RequiredDlls/" to "./bin/x64/Debug/" and "./bin/x64/Release/"
Where home dir is folder where the VS project resides.

2. Ensure DXIL libraries are present
The DirectX Shader Compiler (dxcompiler.dll) and DirectX Intermediate Language validator (dxil.dll)
must be in the same directory as the executable!!!
This is especially important for development builds where signing happens at runtime. 


I added two macros to the Preprocessor Defintions for the project properties.
STB_IMAGE_IMPLEMENTATION and TINYOBJLOADER_IMPLEMENTATION.  This was to fix missinf function defintions.
It caused double definitions, so I added the two tests below which check if it is already defined and if it is,
it undefines it.

#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif

#ifdef TINYOBJLOADER_IMPLEMENTATION
#undef TINYOBJLOADER_IMPLEMENTATION
#endif

I added _CRT_SECURE_NO_WARNINGS to the Preprocessor Defintions as well.  This removed warnings that were treated
as errors.  Specifically, warnings to upgrade to newer safer C lib functions.  sepcifically, wcstombs was crreating 
an error.

Include 3rd party dirs:
C/C++ -> General->Additional Include Dirs:
Engine\DXR\thirdparty
Engine\DXR\thirdparty\dxc

