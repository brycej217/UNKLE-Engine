"C:\VulkanSDK\1.4.313.2\Bin\glslc.exe" shader.vert -o vert.spv
"C:\VulkanSDK\1.4.313.2\Bin\glslc.exe" shader.frag -o frag.spv
"C:\VulkanSDK\1.4.313.2\Bin\glslc.exe" --target-env=vulkan1.2 --target-spv=spv1.4 raygen.rgen  -o raygen.spv
"C:\VulkanSDK\1.4.313.2\Bin\glslc.exe" --target-env=vulkan1.2 --target-spv=spv1.4 miss.rmiss   -o miss.spv
"C:\VulkanSDK\1.4.313.2\Bin\glslc.exe" --target-env=vulkan1.2 --target-spv=spv1.4 hit.rchit    -o hit.spv
"C:\VulkanSDK\1.4.313.2\Bin\glslc.exe" --target-env=vulkan1.2 --target-spv=spv1.4 shadow_hit.rahit -o shadow_hit.spv
"C:\VulkanSDK\1.4.313.2\Bin\glslc.exe" --target-env=vulkan1.2 --target-spv=spv1.4 shadow_miss.rmiss -o shadow_miss.spv
pause