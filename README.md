**Dual-Pipeline Vulkan Renderer**

* Bryce Joseph
* [LinkedIn](https://www.linkedin.com/in/brycejoseph/), [GitHub](https://github.com/brycej217)
* Tested on: Windows 11, Intel(R) CORE(TM) Ultra 9 275HX @ 2.70GHz 32.0GB, NVIDIA GeFORCE RTX 5080 Laptop GPU 16384MB

# UNKLE Engine
UNKLE (UNtitled VuLKan Engine) Engine is a dual-pipeline real-time Vulkan rendering engine supporting both a ray-tracing pipeline and a rasterization-based pipeline. The two pipelines can be toggled between in real time without any asset reloading overhead.  
Rasterization Pipeline:  
![](images/gif1.gif)  
Ray-Tracing Pipeline:  
![](images/boids.png)  
Below are the techincal details about each pipeline:

## Rasterization Pipeline
The rasterization pipeline utilizes a vertex shader, fragment shader, and depth buffer in order 

## Ray-Tracing Pipeline


# Implementation

# Performance


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

### Extra Credit
I implemented the grid-looping optimization for the uniform/coherent implementations.