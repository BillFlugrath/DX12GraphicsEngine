DX12GraphicsEngine is a Visual Studio project that contains the boilerplate code to render 3D assets in D3D12.  It's a starting point to build DX12 applications that will do complex graphics rendering in C++ and HLSL.  

It contains a few demo applications that show how to setup DX12 for rendering.  These applications include standard rendering as well as ray tracing with DXR.

The standard rendering app shows how to do multi-threaded rendering with DX12.  It also shows how to integrate a compute shader into the rendering pipeline along with graphics shaders every frame.

The ray tracing applications demonstrate how to configure DX12 for rendering scenes with DXR.  There is an application that demonstrates DXR using a few directly bound shader resources.  More importantly, there is a demo application that shows how to use "bindless" resource managment.  Configuring the DXR pipeline is more difficult that the standard graphics pipleine.  Using bindless resources simplifies the managment of complex 3D scenes, but the code setup in C++ and HLSL is a bit tricky.  

There is also a point cloud renderer in one of the applications that loads .ply files.  The ply files can be generated from many devices including a modern iPhone.  It then renders each point as a quad generated with a geometry shader.

For the sample apps, 3-d models are Wavefront "obj" files that contain the vertex data (position, normals, uvs).  The texture data for the assets is either png, dds, or jpg. 

Thea main.cpp is the entry point to the application and has the standard WinMain function.