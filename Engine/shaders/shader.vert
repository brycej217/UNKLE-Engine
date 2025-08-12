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

layout(location = 0) out vec3 outViewPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) flat out uint outTexIndex;

void main() {
uint instRec = gl_InstanceIndex;
    
uint transformIndex = instances[instRec].x;
uint texIndex = instances[instRec].y;
MVP mvp = transforms[transformIndex];

vec4 worldPos = mvp.model * vec4(inPos, 1.0);
vec4 viewPos = mvp.view * worldPos;

mat3 normalMat = mat3(transpose(inverse(mvp.model)));
vec3 viewNormal = normalize(normalMat * inNormal);

outViewPos = worldPos.xyz;
outNormal = viewNormal;
outUV = inUV;
outTexIndex = texIndex;

gl_Position = mvp.proj * viewPos;
}