DX12GraphicsEngine is a Visual Studio project that contains the boilerplate code to render 3D assets in D3D12.  It's a starting point to build DX12 applications that will do complex graphics rendering in C++ and HLSL.  

It contains a few demo applications that show how to setup DX12 for rendering.  These applications include standard rendering as well as ray tracing with DXR.

The standard rendering app also shows how to do multithreaded rendering with DX12.  It also demonstrates how to use a compute shader along with a graphics shader every frame.

The ray tracing applications demonstrate how to configure DX12 for rendering scenes with DXR.  There is an application that demonstrates DXR using a few directly bound shader resources.  More importantly, there is a demo application that shows how to use "bindless" resource managment.  Configuring the DXR pipeline is more difficult that the standard graphics pipleine.  Using bindless resources simplifies the managment of complex 3D scenes, but the code setup in C++ and HLSL is a bit tricky.  

For the sample apps, 3-d models are Wavefront "obj" files that contain the vertex data (position, normals, uvs).  The texture data for the assets is either png, dds, or jpg. 