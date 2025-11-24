Visual Studio project requires package:
package id="Microsoft.Direct3D.D3D12" version="1.618.3" targetFramework="native"
Get the package via right click on project name and chhose Manage NuGet Packages.

Also, the project requires latest Windows SDK.  Right click the project name and chose Retarget Projects.  Select the latest SDK. 

The source code folders are arranged such that dependencies should only be created to files at a higher folder level.  For example, the application source files depend on files in "assets", "Engine" and "Include" folders.  The "Engine" files have dependencies on the "DXR" folder's files.  Files in the same folder have dependencies between them. 
This separation allows for easily reusing a folder in another project.  For example, to just use the DX Raytracing samples code, simply copy the "DXR" folder into another project.  