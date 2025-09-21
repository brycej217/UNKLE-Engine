#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

struct Vertex
{
	vec3 pos; float _pad0;
	vec3 normal; float _pad1;
	vec2 uv; vec2 _pad2;
};

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

layout(std430, set = 0, binding = 0) readonly buffer Instances { uvec4 instances[]; };
layout(std430, set = 0, binding = 2) readonly buffer PointLights { PointLight pointLights[]; };
layout(std430, set = 0, binding = 3) readonly buffer DirLights { DirLight dirLights[]; };
layout(set = 0, binding = 4) uniform accelerationStructureEXT topLevelAS;
layout(std430, set = 0, binding = 7) readonly buffer Vertices { Vertex vertices[]; };
layout(std430, set = 0, binding = 8) readonly buffer Indices { uint indices[]; };
layout(set = 0, binding = 9) uniform sampler2D textures[];

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadEXT bool isShadowed;
hitAttributeEXT vec2 attribs;

vec3 getPos(uvec3 idx, vec3 bc)
{
	// calculate position based on vertex attributes and bc
	vec3 p0 = vertices[idx.x].pos;
	vec3 p1 = vertices[idx.y].pos;
	vec3 p2 = vertices[idx.z].pos;
	vec3 hitPos = p0 * bc.x + p1 * bc.y + p2 * bc.z;

	return hitPos;
}

vec3 getNormal(uvec3 idx, vec3 bc)
{
	// calculate normal based on vertex attributes and bc
	vec3 n0 = vertices[idx.x].normal;
	vec3 n1 = vertices[idx.y].normal;
	vec3 n2 = vertices[idx.z].normal;
	vec3 hitNormal = normalize(n0 * bc.x + n1 * bc.y + n2 * bc.z);

	return hitNormal;
}

vec3 getUV(uvec3 idx, vec3 bc)
{
	// calculate uv based on vertex attributes and bc
	vec2 uv0 = vertices[idx.x].uv;
	vec2 uv1 = vertices[idx.y].uv;
	vec2 uv2 = vertices[idx.z].uv;
	vec2 uv = uv0 * bc.x + uv1 * bc.y + uv2 * bc.z;

	return uv;
}

void main() 
{
	// determine instance that was hit and retrieve data
	uvec4 instRec = instances[gl_InstanceCustomIndexEXT];
	uint texIndex = instRec.y;
	uint baseIndex = instRec.z;
	uint baseVertex = instRec.w;

	// calculate barycentric coordinates
	const vec3 bc = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

	// determine triangle that was hit based on instance records
	uint triFirst = baseIndex + 3u * gl_PrimitiveID;
	uvec3 idx = uvec3(indices[triFirst + 0], indices[triFirst+1], indices[triFirst+2]) + uvec3(baseVertex);

	// get vertex attributes
	vec3 pos = getPos(idx, bc);
	vec3 normal = getNormal(idx, bc);
	vec3 uv = getUV(idx, bc);

	// convert position and normal to world space
	vec3 position = (gl_ObjectToWorldEXT * vec4(hitPos,1.0)).xyz;
    normal = normalize(transpose(mat3(gl_WorldToObjectEXT)) * hitNormal);
	
	// calculate lighting
	vec3 lightPos = pointLights[0].position;
    vec3 lightDir = normalize(lightPos - position);
	float lightDist = length(lightPos - position);

	// shadow ray
	const uint rayFlags =
    gl_RayFlagsSkipClosestHitShaderEXT |
    gl_RayFlagsTerminateOnFirstHitEXT;  

	isShadowed = true;
	traceRayEXT(topLevelAS, rayFlags, 0xFF, 0, 1, 1, position + normal * 0.001, 0.001, lightDir, lightDist - 0.01, 1);

	float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * vec3(1.0);

	float diff = isShadowed ? 0.0 : max(dot(normal, lightDir), 0.0);
	vec3 diffuse = diff * vec3(1.0);

	vec3 albedo = texture(nonuniformEXT(textures[texIndex]), uv).bgr;
	
	hitValue = (ambient + diffuse) * albedo;
}