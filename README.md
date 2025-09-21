**Dual-Pipeline Vulkan Renderer**

* Bryce Joseph
* [LinkedIn](https://www.linkedin.com/in/brycejoseph/), [GitHub](https://github.com/brycej217)
* Tested on: Windows 11, Intel(R) CORE(TM) Ultra 9 275HX @ 2.70GHz 32.0GB, NVIDIA GeFORCE RTX 5080 Laptop GPU 16384MB

# UNKLE Engine
UNKLE (UNtitled VuLKan Engine) Engine is a dual-pipeline real-time Vulkan rendering engine supporting both a ray-tracing pipeline and a rasterization-based pipeline. The two pipelines can be toggled between in real time without any asset reloading overhead.  
Rasterization Pipeline:  
![](images/rasterizer.png)  
Ray-Tracing Pipeline:  
![](images/raytracer.png)  
Below are the techincal details about the engine and each pipeline:

## Engine
UNKLE Engine manages several systems which come together to make the real-time renderer, namely the renderer itself, scene manager, input system, and controller. These all come together within the render loop to update our scene state and render it each frame:
```
void Engine::run()
{
	auto previousTime = high_resolution_clock::now();

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		float deltaTime = calculateDeltaTime(&previousTime);

		controller.update(input, &camera, deltaTime);
		renderer->render(camera, deltaTime);
	}
}
```
The renderer is what communicates with the Vulkan, and was the main focus of development. The renderer wraps Vulkan API objects into custom UNK objects, including the Vulkan logical device, swapchain, pipelines, buffers, command buffers, images, and acceleration structures. 
Working with the scene manager, we create the necessary resources such as vertex buffers, texture images, etc. based on the initial state of the scene, stored as any standard 3D file format such as .obj, .fbx, .gltf, etc. as allowed by the [assimp](https://github.com/assimp/assimp) asset loading library. 
On launch, the renderer will create a custom rasterization pipeline object and ray-traced pipeline object and store a reference to both of them. Since we have the necessary raw geometry/image data from the scene loading process, we can then create the specific resources for each pipeline, which I will discuss in each pipeline's dedicated sections. 
With these pipelines ready to go, we can simply swap which pipeline is active whenever we which to switch pipelines, and the respective pipeline will then render the current state of the scene with no resource creation overhead and without performance losses from managing two pipelines at once.
Instancing was achieved with custom instance data structs populated when loading meshes:
```
struct Instance
{
	uint32_t transformIndex;
	uint32_t textureIndex;
	uint32_t baseIndex;
	uint32_t baseVertex;
};
```
By having each instance keep a reference to its transform in a transform SSBO and texture in a texture SSBO, we can easily find the corresponding resource for the instance we are rendering and utilize them in our shaders. 
The base index and vertex are used for getting vertex data into our ray-tracer, thus we can work with per-vertex position, normal, and uv data in the ray-tracer. Draw commands are built on the GPU using the draw indrect suite of Vulkan API calls (namely multi-draw indirect). 
A buffer with all our draw commands is then bound when performing our rendering operations.

## Rasterization Pipeline
The rasterization pipeline utilizes a vertex shader, fragment shader, and depth buffer in order to render objects to the screen in an object first rendering system. This is a rather standard Vulkan renderer and I wish to add extra features to it in the future such as shadow mapping and deferred rendering.

## Ray-Tracing Pipeline
The ray-tracing pipeline was implemented with the VK_KHR_ray_tracing extension, which supports fundamental real-time ray-tracing capabilities in Vulkan. The main difference between the ray-tracing pipeline and the rasterization pipeline are the way geometry is represented, namely in order to 
improve performance we are required to create Vulkan acceleration structure objects (wrapped into UNK Acceleration Structure objects), which define a tree-like structure for our geometry to reside in such that we can perform early termination or rays. Outside of that, a shader binding table must be defined such that we can access 
the necessary rays when within a call.

# Further Work
Engine Systems Roadmap
* Material System (bump/normal/specular maps)
* Scene Editor  

Rasterization Roadmap
* Shadow Mapping
* Deferred Rendering

Ray-Tracer Roadmap
* Ray-Bounced Lighting
* Reflection/Refraction

### Credits
I would like to thank Alexander Overvoorde of [Vulkan Tutorial](https://vulkan-tutorial.com/) for getting me started with the basics of the Vulkan API.  
I would also like to thank Sascha Willems for his famous [Vulkan Examples](https://github.com/SaschaWillems/Vulkan/tree/master/examples) suite which helped me to understand clean object-oriented Vulkan code and for implementation details on the more less documented parts of the API like the Vulkan Ray-Tracing pipeline.
I would also like to give credit to the people on the [assimp](https://github.com/SaschaWillems/Vulkan/tree/master/examples) team for their lightweight asset loading library.