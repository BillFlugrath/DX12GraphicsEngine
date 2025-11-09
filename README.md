DX12GraphicsEngine is a Visual Studio project that contains the boilerplate code to render 3D assets using DirectX 12.  It's a starting point to build DX12 applications that will do complex graphics rendering in C++ and HLSL.  

It contains a few demo applications that show how to setup DX12 for rendering.  These applications include standard rendering as well as ray tracing with DXR.

The standard rendering app shows how to do multi-threaded rendering with DX12.  It also shows how to integrate a compute shader into the rendering pipeline along with graphics shaders every frame.  Basic mathematics is performed using the DirectXMath library.

The ray tracing application demonstrates how to configure DirectX 12 for rendering scenes with DXR ray tracing library.  There is an application that demonstrates DXR using a few directly bound shader resources.  More importantly, the application also shows how to use "bindless" resource managment.  Configuring the DXR pipeline is more difficult than the standard graphics pipleine.  Using bindless resources simplifies the managment of complex 3D scenes, but the code setup in C++ and HLSL is a bit tricky.  The shader binding table (SBT) includes support for multiple ray types and different materials.

There is also a point cloud renderer in one of the applications that loads .ply files.  The ply files can be generated from many devices including a modern iPhone.  It then renders each point as a quad generated with a geometry shader.

For the sample apps, 3-d models are Wavefront "obj" files that contain the vertex data (position, normals, uvs).  The texture data for the assets is either png, dds, or jpg. 

The camera is controlled via the standard WASD.  The up and down arrow keys can be used to move along camera y-axis.  Camera orientation is controlled via pressing the left or right mouse button and using the arrow keys.  The left and right arrows control camera yaw.  The up and down arrows control camera pitch.  Press "R" key to reset to starting position and orientation.

The main.cpp is the entry point to the application and has the standard WinMain function.
Microsoft Visual Studio Community 2022 is required to load the project and build the application.
