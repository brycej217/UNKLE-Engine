#version 460
#extension GL_EXT_nonuniform_qualifier : enable

struct MVP
{
	mat4 model;
	mat4 view;
	mat4 proj;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances { uvec4 instances[]; };
layout(std430, set = 0, binding = 1) readonly buffer Transforms { MVP transforms[]; };

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outWorldNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) flat out uint outTexIndex;

void main() {

// get instance data
uint instRec = gl_InstanceIndex;
uint transformIndex = instances[instRec].x;
MVP mvp = transforms[transformIndex];
uint texIndex = instances[instRec].y;
outTexIndex = texIndex;

// transform vertex position to world and view space
vec4 worldPos = mvp.model * vec4(inPos, 1.0);
vec4 viewPos = mvp.view * worldPos;
outWorldPos = worldPos.xyz;

// transform normal to world space
mat3 normalMat = mat3(transpose(inverse(mvp.model)));
vec3 worldNormal = normalize(normalMat * inNormal);
outWorldNormal = worldNormal;

outUV = inUV;

gl_Position = mvp.proj * viewPos;
}