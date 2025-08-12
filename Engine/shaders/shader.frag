#version 460
#extension GL_EXT_nonuniform_qualifier : enable

struct Light
{
    vec3 position;
	vec3 direction;
	vec3 color;
};

layout(std430, set = 0, binding = 2) readonly buffer Lights { Light lights[]; };
layout(set = 0, binding = 3) uniform sampler2D textures[];

layout(location = 0) in vec3 inViewPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) flat in uint texIndex;

layout(location = 0) out vec4 outColor;


void main() 
{
    uint i = 0;
 
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * vec3(1.0);

    vec3 norm = normalize(inNormal);
    vec3 lightDir = normalize(lights[i].position - inViewPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);

    vec3 objectColor = texture(textures[texIndex], inUV).rgb;
    
    vec3 color = (ambient + diffuse) * objectColor;

    outColor = vec4(color, 1.0);
}