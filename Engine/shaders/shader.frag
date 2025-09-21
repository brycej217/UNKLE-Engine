#version 460
#extension GL_EXT_nonuniform_qualifier : enable

struct PointLight
{
	vec3 position; float _pad0;
	vec3 direction; float _pad1;
	vec3 color; float _pad2;

	float constant;
	float linear;
	float quadratic;
    float _pad3;
};

struct DirLight
{
	vec3 direction; float _pad1;
	vec3 color; float _pad2;
};

layout(std430, set = 0, binding = 2) readonly buffer PointLights { PointLight pointLights[]; };
layout(std430, set = 0, binding = 3) readonly buffer DirLights { DirLight dirLights[]; };
layout(set = 0, binding = 4) uniform Camera 
{
    mat4 viewInv;
    mat4 projInv;
} camera;
layout(set = 0, binding = 5) uniform sampler2D textures[];

layout( push_constant ) uniform PushConstants
{
	uint numPointLights;
    uint numDirLights;
	vec2 _pad;
} constants;

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inWorldNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) flat in uint texIndex;

layout(location = 0) out vec4 outColor;

float specularStrength = 0.5;

float getAmbient(int i)
{
    float strength = 0.1;
    return strength;
}

float getDiffuse(int i, vec3 L)
{
    vec3 N = normalize(inWorldNormal);
    
    float diff = max(dot(N, L), 0.0);
    return diff;
}

float getSpecular(int i, vec3 L)
{
    vec3 N = normalize(inWorldNormal);

    vec4 viewPos = camera.viewInv * vec4(0, 0, 0, 1);

    vec3 viewDir = normalize(viewPos.xyz - inWorldPos);
    vec3 reflectDir = reflect(-L, N);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    float specular = specularStrength * spec;
    return specular;
}

vec3 getPointLight(int i)
{
    vec3 colorDiff = texture(textures[texIndex], inUV).rgb;
    vec3 L = normalize(pointLights[i].position - inWorldPos);

    vec3 ambient = getAmbient(i) * colorDiff * pointLights[i].color.rgb;
    vec3 diffuse = getDiffuse(i, L) * colorDiff * pointLights[i].color.rgb;
    vec3 specular = getSpecular(i, L) * pointLights[i].color.rgb;

    float dist = length(pointLights[i].position - inWorldPos);
    float attenuation = 1.0 / (pointLights[i].constant + pointLights[i].linear * dist + pointLights[i].quadratic * (dist * dist));
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return (ambient + diffuse + specular);
}

vec3 getDirLight(int i)
{
    vec3 colorDiff = texture(textures[texIndex], inUV).rgb;
    vec3 L = normalize(-dirLights[i].direction);

    vec3 ambient = getAmbient(i) * colorDiff * dirLights[i].color.rgb;
    vec3 diffuse = getDiffuse(i, L) * colorDiff * dirLights[i].color.rgb;
    vec3 specular = getSpecular(i, L) * dirLights[i].color.rgb;

    return (ambient + diffuse + specular);
}

void main() 
{
    vec3 lighting = vec3(0.0);

    for (int i = 0; i < int(constants.numDirLights); i++)
    {
        lighting += getDirLight(i);
    }

    for (int i = 0; i < int(constants.numPointLights); i++)
    {
        lighting += getPointLight(i);
    }

    outColor = vec4(lighting, 1.0);
}